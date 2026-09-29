/**
 * @brief 多轮对话用例：会话 CRUD、上下文组装、调用 LLM 并落库。
 *
 * category 只是应用自定义分组标签，不参与模型选择或提示词推导。
 * LLM 未装配（未配置模型）时发送消息返回 DEPENDENCY.UNAVAILABLE，不静默降级。
 * 只有纯计算的 assemble_context 保持同步；一切触及映射器的方法都是协程。
 */
export module cnetboot.chat.service.chat_service;

import std;
import cnetmod.coro.task;
import cnetboot.chat.domain.chat_domain;
import cnetboot.chat.mapper.chat_mapper;
import cnetboot.framework.audit;
import cnetboot.framework.error;
import cnetboot.framework.pagination;
import cnetboot.ai.domain.llm_contract;
import cnetboot.ai.service.llm_service;

export namespace cnetboot::chat {

namespace pagination = cnetboot::framework::pagination;
namespace error = cnetboot::framework::error;

/** @brief 会话标题长度上限（字节）；与 chat_session.title 列宽一致。 */
inline constexpr std::size_t max_session_title_length = 128;

/** @brief 未发消息前的默认会话标题。 */
inline constexpr std::string_view default_session_title = "新会话";

/** @brief 模型名长度上限（字节）；与 chat_session.model 列宽一致。 */
inline constexpr std::size_t max_model_length = 64;

/** @brief 新建会话请求；has_temperature 区分「请求未提供」与「显式 0」。
 *
 *  context 不出现在会话对象里，创建时被忽略（它属于单次发送消息的入参）。
 */
struct chat_session_create_request
{
    std::string category;                                          ///< 应用自定义分类；空表示不分组
    std::string title;                                              ///< 标题；空表示用默认标题
    std::string system_prompt;                                      ///< 自定义系统提示词；空表示用服务默认
    std::string model;                                              ///< 模型覆盖；空表示用服务默认
    double temperature{cnetboot::chat::default_temperature};       ///< 采样温度
    bool has_temperature{false};                                    ///< 请求体是否提供了 temperature
};

/** @brief 会话局部更新：未提供的字段保持原值（has_* 为 false）。 */
struct chat_session_patch
{
    bool has_title{false};                                          ///< 是否更新标题
    bool has_system_prompt{false};                                  ///< 是否更新系统提示词（空串表示清空回默认）
    bool has_model{false};                                          ///< 是否更新模型
    bool has_temperature{false};                                    ///< 是否更新温度
    std::string title;                                              ///< 新标题
    std::string system_prompt;                                      ///< 新系统提示词
    std::string model;                                              ///< 新模型
    double temperature{cnetboot::chat::default_temperature};       ///< 新温度
};

/** @brief 发送消息请求。 */
struct chat_message_request
{
    std::string content; ///< 用户输入
    /// 调用方参考上下文（**已由 transport 归一化为文本**）；非空时作为 system 消息注入；
    /// 上限见 domain 的 max_reference_context_characters（50000 字）。
    std::string reference_context;
};

/** @brief 一次完整回合：用户消息与助手消息（都已落库）。 */
struct chat_turn
{
    cnetboot::chat::chat_message user_message;      ///< 本轮用户消息
    cnetboot::chat::chat_message assistant_message; ///< 本轮助手消息
};

/** @brief 对话服务配置。 */
struct chat_options
{
    std::size_t history_limit{cnetboot::chat::default_history_limit}; ///< 上下文回灌的最近 N 条
    std::size_t token_budget{cnetboot::chat::default_token_budget};   ///< 上下文估算 token 预算
    std::string system_prompt;                                         ///< 服务默认系统提示词
    std::string provider;                                              ///< 服务默认供应商名；空表示由 LLM 服务取配置默认值
    std::string model;                                                 ///< 服务默认模型
    std::string reasoning_effort;                                      ///< 推理强度；空表示不发送
    int max_tokens{1024};                                              ///< 输出上限
};

/** @brief 流式增量回调：返回 false 表示下游已断开（服务会中断上游流并落库已生成部分）。 */
using chat_delta_sink = cnetboot::ai::llm_delta_sink;

/** @brief 一次上下文组装的结果：服务侧消息序列 + 历史窗口的裁剪观测。 */
struct chat_context_assembly
{
    std::vector<cnetboot::ai::llm_message> messages; ///< 已投影到 LLM 请求的消息序列
    cnetboot::chat::context_trim_result trim;             ///< 历史窗口裁剪量
};

/**
 * @brief 多轮对话用例：会话 CRUD、上下文组装、调用 LLM 并落库（全协程）。
 *
 * 历史窗口裁剪由 domain 的 build_chat_context 负责；真的裁掉历史（或裁完仍超预算）时
 * 每回合最多记一行日志（info / warn），不逐条刷屏。
 */
class chat_service
{
public:
    chat_service(mysql_chat_mapper& mapper, cnetboot::ai::llm_service& llm,
        cnetboot::framework::audit_log_service* audit) noexcept
        : mapper_{&mapper},
          llm_{&llm},
          audit_{audit}
    {
    }

    chat_service(const chat_service&) = delete;
    chat_service(chat_service&&) = delete;

    /** @brief 设置对话配置；history_limit 至少 1，token_budget 至少 256。 */
    void set_options(chat_options options);

    /** @brief 当前配置。 */
    [[nodiscard]] auto options() const noexcept -> const chat_options&
    {
        return options_;
    }

    /** @brief 新建会话。 */
    [[nodiscard]] auto create_session(const chat_session_create_request& request,
        const cnetboot::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<cnetboot::chat::chat_session, error::error_code>>;

    /** @brief 分页检索会话。 */
    [[nodiscard]] auto list_sessions(const cnetboot::chat::chat_session_query& query)
        -> cnetmod::task<std::expected<pagination::page_result<cnetboot::chat::chat_session>,
            error::error_code>>;

    /** @brief 按 ID 查询会话；不存在返回 CHAT.SESSION_NOT_FOUND。 */
    [[nodiscard]] auto find_session(std::uint64_t id)
        -> cnetmod::task<std::expected<cnetboot::chat::chat_session, error::error_code>>;

    /** @brief 局部更新会话；未提供的字段保持原值。 */
    [[nodiscard]] auto update_session(std::uint64_t id, const chat_session_patch& patch,
        const cnetboot::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<cnetboot::chat::chat_session, error::error_code>>;

    /** @brief 逻辑删除会话；不存在返回 CHAT.SESSION_NOT_FOUND。 */
    [[nodiscard]] auto remove_session(std::uint64_t id,
        const cnetboot::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<void, error::error_code>>;

    /** @brief 会话消息分页（按时间正序）；会话不存在返回 CHAT.SESSION_NOT_FOUND。 */
    [[nodiscard]] auto list_messages(std::uint64_t session_id, const pagination::page_request& page)
        -> cnetmod::task<std::expected<pagination::page_result<cnetboot::chat::chat_message>,
            error::error_code>>;

    /** @brief 非流式发送：一次补全，用户消息与助手消息都落库。
     *  @return 本轮两条消息；LLM 失败返回 ALGORITHM.FAILED（用户消息已落库，可重发） */
    [[nodiscard]] auto send_message(std::uint64_t session_id, const chat_message_request& request,
        const cnetboot::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<chat_turn, error::error_code>>;

    /** @brief 流式发送：增量依次交给 sink，成功后落库助手消息。
     *  @param sink 增量回调；返回 false 表示下游断开（仍会把已生成内容落库） */
    [[nodiscard]] auto stream_message(std::uint64_t session_id, const chat_message_request& request,
        const cnetboot::framework::audit_actor& actor, const chat_delta_sink& sink)
        -> cnetmod::task<std::expected<chat_turn, error::error_code>>;

    /** @brief 组装多轮上下文（纯逻辑，供用例直接校验顺序、条数与预算截断）。 */
    [[nodiscard]] auto assemble_context(const cnetboot::chat::chat_session& session,
        std::span<const cnetboot::chat::chat_message> history, std::string_view reference_context,
        std::string_view pending_user_content) const -> chat_context_assembly;

private:
    /** @brief 一次发送的准备工作：会话、已落库的用户消息、组装好的 LLM 请求。 */
    struct turn_plan
    {
        cnetboot::chat::chat_session session;      ///< 会话（可能已刷新首条标题）
        cnetboot::chat::chat_message user_message; ///< 已落库的用户消息
        cnetboot::ai::llm_request request;    ///< 组装好的请求
    };

    /// @brief 校验入参、刷新首条标题、装配上下文并落库用户消息。
    [[nodiscard]] auto prepare_turn(std::uint64_t session_id, const chat_message_request& request,
        const cnetboot::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<turn_plan, error::error_code>>;

    /// @brief 落库助手消息并组装返回值。
    [[nodiscard]] auto finish_turn(const turn_plan& plan, std::string content, int tokens_in,
        int tokens_out) -> cnetmod::task<std::expected<chat_turn, error::error_code>>;

    /// @brief 按会话、历史与本轮输入组装 LLM 请求。
    [[nodiscard]] auto build_llm_request(const cnetboot::chat::chat_session& session,
        const chat_message_request& request) const -> cnetmod::task<cnetboot::ai::llm_request>;

    /// @brief 首条消息时把默认标题替换成消息摘要（UTF-8 安全截断）。
    [[nodiscard]] auto refresh_title(const cnetboot::chat::chat_session& session,
        std::string_view content, const cnetboot::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<cnetboot::chat::chat_session, error::error_code>>;

    mysql_chat_mapper* mapper_;
    cnetboot::ai::llm_service* llm_;
    cnetboot::framework::audit_log_service* audit_{nullptr};
    chat_options options_{};
};

} // namespace cnetboot::chat
