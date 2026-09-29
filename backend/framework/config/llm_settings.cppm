/**
 * @brief LLM 供应商目录：typed options 小节 + 加载期解析（纯逻辑，零网络依赖）。
 *
 * 配置的两层结构：
 *  - services.<name>（type=openai）是**运行时实例**：cnetmod 用它注册
 *    chat_model_service，一个实例自带一个 api_key 与一个连接池（pool_size 只决定并发
 *    连接数，**不做** key 轮换）。因此「一个 key = 一个 services 实例」。
 *  - 顶层 llm 小节（llm_options，经 cnetmod options_registry 声明、解码与校验）是
 *    **供应商目录**：把若干 services 实例聚合成一个逻辑供应商的多条 key，并声明该供应商
 *    可用的模型清单。
 *
 * 职责划分：
 *  - validate_llm_options()：小节自身的结构校验（options 阶段，带配置路径失败）；
 *  - resolve_llm_directory()：把 key 引用解析到 services.<name> 的实例名并校验实例
 *    （registration 阶段；未知服务、非 openai、空 api_key、pool_size 越界都在这里拒绝）；
 *  - resolve_llm_target()：一次请求的 (provider, model) 解析（默认值回退、拒绝未知值）。
 *
 * 多 key 的轮询、失败切 key、退避与熔断**不在配置层**：Nexus 的模型适配层显式调用
 * cnetmod `add_chat_model` 并声明 routing/fallback（见
 * cnetnexus.reading.service.model.composed_llm_service）。
 */
export module cnetnexus.framework.config.llm_settings;

import std;
import cnetmod.application.configuration;
import cnetnexus.framework.error;

export namespace cnetnexus::framework::config {

/** @brief 框架注册错误码；目录解析直接复用它，避免再引入一层私有错误枚举。 */
using error_code = cnetnexus::framework::error::error_code;

/// @brief 未配置（没有可用供应商或该供应商没有任何可用 key）：DEPENDENCY.UNAVAILABLE。
inline constexpr error_code llm_unconfigured_code{error_code::upstream_unavailable};

/// @brief 选择被拒绝（未知 provider / 未知 model / 上游调用失败）：ALGORITHM.FAILED。
inline constexpr error_code llm_rejected_code{error_code::algorithm_failed};

/// @brief cnetmod openai 服务实例连接池上限（与 integration/openai.cpp 的校验一致）。
inline constexpr std::size_t llm_maximum_pool_size{1024};

/// @brief 顶层配置小节名。
inline constexpr std::string_view llm_section_name{"llm"};

/// @brief 一条 key：指向一个 services.<service>（type=openai）实例。
struct llm_key_options
{
    std::string name;    ///< key 别名（同一供应商内唯一）；缺省取 service
    std::string service; ///< services.<service> 条目名（必填）
};

/// @brief 一个逻辑供应商：一组 key + 一份模型清单。
struct llm_provider_options
{
    std::string name;                        ///< 供应商名（全局唯一）
    std::string type{"openai"};              ///< 供应商类型；当前只支持 openai
    std::vector<std::string> models;         ///< 可用模型清单（非空、无重复）
    std::vector<std::string> model_prefixes; ///< 预留键：接受但不参与匹配
    std::vector<llm_key_options> keys;       ///< 至少一条；多条 = 多 key
};

/**
 * @brief 同一供应商多 key 之间的失败切换策略（映射到 cnetmod resilient_model_options）。
 */
struct llm_resilience_options
{
    /// 每条 key 的尝试次数（含首次）；1 = 失败立即切下一条 key，不在同一 key 上重试。
    std::size_t attempts_per_key{1};
    /// 同一 key 重试之间的初始退避（毫秒）。
    std::int64_t initial_backoff_ms{100};
    /// 退避上限（毫秒）。
    std::int64_t max_backoff_ms{2000};
};

/**
 * @brief 顶层 llm 小节。providers 为空表示本部署不启用 LLM（运行期 DEPENDENCY.UNAVAILABLE）。
 */
struct llm_options
{
    std::string default_provider; ///< 未指定供应商时的默认值；有供应商时必填
    std::string default_model;    ///< 未指定模型时的默认值；有供应商时必填
    std::vector<llm_provider_options> providers;
    llm_resilience_options resilience;
};

/// @brief 小节结构校验（交给 options_registry 的 validate()）。
[[nodiscard]] auto validate_llm_options(const llm_options& options)
    -> std::expected<void, std::string>;

/// @brief 解析后的一条 key：已绑定 cnetmod 实例名。
struct llm_key
{
    std::string name;
    std::string service;
    std::string instance; ///< add_chat_model 显式组合的 chat_model_service 实例名
    bool enabled{false};  ///< services.<service>.enabled；未启用的实例不会被 cnetmod 注册
};

/// @brief 解析后的一个供应商。
struct llm_provider
{
    std::string name;
    std::string type;
    std::vector<std::string> models;
    std::vector<llm_key> keys;

    /// @brief 至少一条 key 已启用。
    [[nodiscard]] auto usable() const noexcept -> bool;
    /// @brief 已启用 key 的实例名（声明顺序）。
    [[nodiscard]] auto enabled_instances() const -> std::vector<std::string>;
};

/// @brief 解析后的完整目录（不可变快照）。
struct llm_directory
{
    std::string default_provider;
    std::string default_model;
    std::vector<llm_provider> providers;
    llm_resilience_options resilience;

    /// @brief 按名字找供应商；找不到返回 nullptr。
    [[nodiscard]] auto find_provider(std::string_view name) const noexcept
        -> const llm_provider*;
    /// @brief 至少有一个供应商拥有至少一条可用 key。
    [[nodiscard]] auto usable() const noexcept -> bool;
};

/**
 * @brief 把 llm 小节的 key 引用解析到 services.<name> 实例并校验实例配置。
 * @return 失败信息带完整配置路径；调用方在 registration 阶段直接让构建失败
 */
[[nodiscard]] auto resolve_llm_directory(const llm_options& options,
    const std::map<std::string, cnetmod::application::configured_service,
        std::less<>>& services)
    -> std::expected<llm_directory, std::string>;

/// @brief 一次请求解析出的目标。
struct llm_target
{
    std::string provider;
    std::string model;

    auto operator==(const llm_target&) const -> bool = default;
};

/**
 * @brief 解析一次请求的 (provider, model)：空值回退默认值。
 * @return 未知 provider / 未知 model -> ALGORITHM.FAILED；
 *         没有可用供应商 / 该供应商所有 key 都未启用 -> DEPENDENCY.UNAVAILABLE
 */
[[nodiscard]] auto resolve_llm_target(const llm_directory& directory,
    std::string_view provider, std::string_view model)
    -> std::expected<llm_target, error_code>;

/// @brief 供应商在组件容器里的 ai::chat_model 组件名（"llm:<provider>"）。
[[nodiscard]] auto llm_model_component(std::string_view provider) -> std::string;

/// @brief 目录里一条 key 的脱敏视图：只有别名与「是否已配置」。
struct llm_credential_view
{
    std::string name;
    bool configured{false};
};

/// @brief 目录里一个供应商的脱敏视图：**不含** base_url、密钥或任何头部信息。
struct llm_provider_view
{
    std::string name;
    std::string type;
    std::vector<std::string> models;
    std::vector<llm_credential_view> keys;
    bool is_default{false}; ///< 是否是 default_provider
};

/// @brief 把目录投影成可对外暴露的脱敏列表（HTTP 端点的数据源）。
[[nodiscard]] auto llm_provider_catalog(const llm_directory& directory)
    -> std::vector<llm_provider_view>;

} // namespace cnetnexus::framework::config
