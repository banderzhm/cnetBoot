/**
 * @brief GET /api/v1/llm/providers：多供应商目录的**脱敏**投影。
 *
 * 只回供应商名、类型、模型清单、每条 key 的别名与「是否已配置」；绝不回显 base_url、
 * 密钥、实例名或请求头。未配置 LLM 时返回 configured=false 的空目录，而不是 5xx——
 * 端点本身是「配置可见性」端点，不是补全调用。
 *
 * 与 /api/v1/capabilities 一样属于已发布契约，免令牌：在注册处用 endpoint 元数据
 * anonymous() 声明，认证/授权中间件据此放行，不另维护白名单。
 */
export module cnetnexus.reading.controller.llm_routes;

import std;
import cnetmod.protocol.http;
import cnetnexus.framework.web.route_handler;
import cnetnexus.framework.config.llm_settings;

export namespace cnetnexus::reading {

/** @brief 声明 LLM 目录端点；directory 为不可变快照，与应用同寿命。 */
void declare_llm_routes(cnetmod::http::router& routes,
    std::shared_ptr<const cnetnexus::framework::config::llm_directory> directory);

} // namespace cnetnexus::reading
