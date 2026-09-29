/**
 * @brief 对话域契约：会话与消息实体、分页检索条件，以及「系统提示词 + 最近 N 条 + 盘面摘要」的上下文组装规则。
 *
 * 这里只有纯数据与纯函数，不依赖任何持久化或 HTTP 类型：上下文窗口使用本域自己的
 * context_message / context_trim_result 观测契约，不让供应商或框架协议类型穿透领域边界。
 */
export module cnetnexus.chat.domain.chat_domain;

import std;
import cnetnexus.framework.pagination;

export namespace cnetnexus::chat {

/** @brief 回灌给模型的历史消息条数上限（默认 20 条，可由配置覆盖）。 */
inline constexpr std::size_t default_history_limit = 20;

/** @brief 上下文估算 token 预算；超出时从最旧的历史消息开始丢弃。 */
inline constexpr std::size_t default_token_budget = 8000;

/** @brief 每会话自定义系统提示词的长度上限（字节）。 */
inline constexpr std::size_t max_system_prompt_length = 4000;

/** @brief 单条消息内容上限：**50000 字**（用户口径的「5 万字」）。
 *
 *  口径：按 Unicode 字符数计（中文 1 个汉字 = 1 字），**不按 UTF-8 字节数**。
 *  内容与盘面摘要共用同一套计数与上限；常量只在本文件定义一处，service 与 transport 都引用这里。
 */
inline constexpr std::size_t max_message_characters = 50000;

/** @brief 盘面摘要（chartContext）上限：同样 50000 字，按归一化之后的字符数校验。 */
inline constexpr std::size_t max_chart_context_characters = 50000;

/** @brief 会话标题长度上限（字节）。 */
inline constexpr std::size_t max_title_length = 128;

/** @brief 采样温度默认值与合法区间（与 OpenAI 兼容网关口径一致）。 */
inline constexpr double default_temperature = 0.7;
inline constexpr double min_temperature = 0.0;
inline constexpr double max_temperature = 2.0;

/** @brief 消息角色常量；与 cnetmod openai message.role 同口径。 */
inline constexpr std::string_view role_system = "system";
inline constexpr std::string_view role_user = "user";
inline constexpr std::string_view role_assistant = "assistant";

/** @brief 注入盘面摘要时使用的提示词前缀（前端把当前盘面摘要按纯文本传上来）。 */
inline constexpr std::string_view chart_context_prefix = "当前盘面摘要（由页面提供，仅作参考）：";

/** @brief 未配置自定义提示词时使用的兜底系统提示词。 */
inline constexpr std::string_view default_system_prompt =
    "你是玄学排盘助手中的对话助手。回答时只做术语解释、盘面现象说明与追问引导，"
    "不承诺预测结果、不做医疗或投资建议；用户提供的盘面摘要仅供参考，不要臆造盘中"
    "不存在的干支、卦象或星曜。回答使用简体中文，直接给结论，避免复述问题。";

/** @brief 会话（chat_session 表）。字段名与前端契约的会话对象对齐。 */
struct chat_session
{
    std::uint64_t id{};                      ///< 主键
    std::uint64_t owner_id{};                ///< 创建者；0 表示系统
    std::string title;                       ///< 标题；空串由服务层按首条消息生成
    std::string algorithm;                   ///< 术数标识（规范值）；空表示未分组，不参与任何计算
    std::string system_prompt;               ///< 本会话自定义系统提示词；空表示用服务默认
    std::string model;                       ///< 本会话模型覆盖；空表示用服务默认
    double temperature{default_temperature}; ///< 本会话采样温度覆盖
    std::int64_t message_count{};            ///< 未删除消息条数（由 mapper 在追加消息时维护）
    std::int64_t last_message_at{};          ///< 最后一条消息时间（UTC 秒）；0 表示还没有消息
    std::int64_t created_at{};               ///< 创建时间（UTC 秒）
    std::int64_t updated_at{};               ///< 更新时间（UTC 秒）
};

/** @brief 一条对话消息（chat_message 表）。 */
struct chat_message
{
    std::uint64_t id{};         ///< 主键
    std::uint64_t session_id{}; ///< 所属会话
    std::string role;           ///< system / user / assistant
    std::string content;        ///< 文本内容
    std::string model;          ///< assistant 消息记录实际模型；其余为空
    std::int64_t tokens_in{};   ///< 输入 token（assistant 消息，未知为 0）
    std::int64_t tokens_out{};  ///< 输出 token（assistant 消息，未知为 0）
    std::int64_t created_at{};  ///< 创建时间（UTC 秒）
};

/** @brief 会话列表过滤条件；algorithm 为空表示不过滤（术数名只做分组与检索）。 */
struct chat_session_query
{
    std::string algorithm;                        ///< 术数标识（规范值）
    cnetnexus::framework::pagination::page_request page; ///< 分页参数
};

/** @brief 按 Unicode 码点统计字符数（UTF-8 解码；中文 1 字 = 1，不按字节算）。
 *  只统计非续接字节（0b10xxxxxx 之外的字节），非法字节序列也按 1 个字计，不抛异常。
 */
[[nodiscard]] auto count_characters(std::string_view text) noexcept -> std::size_t;

/** @brief 角色是否合法（system / user / assistant）。 */
[[nodiscard]] auto is_valid_role(std::string_view role) noexcept -> bool;

/** @brief 请求里的术数标识归一；空串合法（表示不分组），非空必须是六个已登记术数之一。
 *  @return 合法返回规范值；非法返回 std::nullopt
 */
[[nodiscard]] auto normalize_algorithm(std::string_view raw) -> std::optional<std::string>;

/** @brief 归一化采样温度：超出 [0, 2] 返回 std::nullopt。 */
[[nodiscard]] auto normalize_temperature(double raw) noexcept -> std::optional<double>;

/** @brief 生效的系统提示词：会话自定义优先，其次服务默认，都没有时用内置兜底。 */
[[nodiscard]] auto effective_system_prompt(const chat_session& session, std::string_view fallback) -> std::string;

/** @brief 上下文组装参数。 */
struct chat_context_options
{
    std::size_t history_limit{default_history_limit}; ///< 最近 N 条历史消息
    std::size_t token_budget{default_token_budget};   ///< 估算 token 预算
    std::string default_system_prompt{};              ///< 会话未自定义时使用的系统提示词
};

/** @brief 估算文本 token 数。
 *
 *  没有引入 tokenizer 依赖，按「3 字节 ≈ 1 token」保守估算：UTF-8 下中文 1 字 3 字节，
 *  英文约 4 字节 1 token，取两者较大者，保证不会低估而超模型上下文。
 */
[[nodiscard]] auto estimate_tokens(std::string_view text) noexcept -> std::size_t;

/** @brief 供应商无关的上下文消息，由对话域自己拥有。 */
struct context_message
{
    std::string role;
    std::string content;
};

/** @brief 历史窗口裁剪的可观测结果。 */
struct context_trim_result
{
    std::size_t removed_messages{};
    std::size_t removed_tokens{};
    std::size_t remaining_tokens{};
    bool limit_satisfied{true};
};

/** @brief 一次上下文组装的结果：消息序列 + 裁剪观测。 */
struct chat_context
{
    std::vector<context_message> messages; ///< 顺序：system → 盘面摘要 → 历史窗口 → 本轮输入
    context_trim_result trim;              ///< 历史窗口裁剪量
};

/** @brief 组装多轮上下文。
 *
 *  顺序固定为：system（会话自定义或默认）→ 盘面摘要（chart_context 非空时注入为一条
 *  system 消息）→ 历史窗口 → 本轮用户输入。
 *
 *  历史消息使用本域稳定策略裁剪：从最旧的非 system 消息开始移除，保留最后一条历史，
 *  直到条数与 token 都在窗口内。该规则不依赖任意模型供应商协议类型。
 *
 *  @note token_budget 只约束历史窗口本身；system / 盘面摘要 / 本轮输入是业务语义，
 *        不参与裁剪、也不从预算里预先扣除。历史里 role=system/developer 的消息优先保留。
 *  @param session 会话（提供系统提示词）
 *  @param history 该会话已有的历史消息，按时间正序
 *  @param chart_context 前端带来的当前盘面摘要；空表示不注入
 *  @param pending_user_content 本轮用户输入（尚未落库）；空表示不追加
 *  @param options 条数与预算配置
 *  @return 消息序列与本次历史窗口的裁剪观测
 */
[[nodiscard]] auto build_chat_context(const chat_session& session, std::span<const chat_message> history,
    std::string_view chart_context, std::string_view pending_user_content, const chat_context_options& options)
    -> chat_context;

} // namespace cnetnexus::chat
