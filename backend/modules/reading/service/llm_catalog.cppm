/**
 * @brief LLM 供应商目录的读取入口：给 HTTP 端点用的**脱敏**投影。
 *
 * 只暴露供应商名、模型清单、每条 key 的别名与「是否已配置」，绝不回显 base_url、
 * 密钥、实例名或任何请求头。未配置 LLM（llm.providers 为空）时返回空目录
 * （configured=false），不会抛异常、不会降级成别的语义。
 */
export module cnetnexus.reading.service.llm_catalog;

import std;
import cnetnexus.framework.config.llm_settings;

export namespace cnetnexus::reading {

/** @brief 脱敏的供应商目录视图。 */
struct llm_catalog_snapshot
{
    bool configured{false}; ///< 是否至少有一个供应商拥有可用 key
    std::string default_provider; ///< 默认供应商名；未配置 LLM 时为空
    std::vector<cnetnexus::framework::config::llm_provider_view> providers;
};

/** @brief 把已解析的目录投影成脱敏快照。 */
[[nodiscard]] auto llm_catalog(
    const cnetnexus::framework::config::llm_directory& directory)
    -> llm_catalog_snapshot;

/**
 * @brief 把脱敏目录编码成 JSON（HTTP 响应体）。
 *
 * 形如：{"configured":true,"default_provider":"openai",
 *        "providers":[{"name":"openai","type":"openai","default":true,
 *        "models":["gpt-4o-mini"],"keys":[{"name":"primary","configured":true}]}]}
 */
[[nodiscard]] auto llm_catalog_json(
    const cnetnexus::framework::config::llm_directory& directory) -> std::string;

} // namespace cnetnexus::reading
