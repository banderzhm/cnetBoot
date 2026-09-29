/**
 * @brief 成功响应的唯一出口：把结构化文档作为 JSON 响应体下发。
 *
 * 全仓只有两种响应形态：成功时是业务文档本身，失败时是 error_response 的
 * {code,status,message,retryable}。不再有 {code,message,data} 之类的包装层。
 */
export module cnetnexus.framework.web.response;

import std;
import cnetmod.json;
import cnetmod.protocol.http;

export namespace cnetnexus::framework::web {

/**
 * @brief 下发一份 JSON 文档；序列化失败时改发 SYSTEM.INTERNAL_ERROR。
 */
void respond_document(cnetmod::http::request_context& context,
    const cnetmod::json::document& body, int status = cnetmod::http::status::ok);

/// @brief typed response 的非模板错误出口，避免在模块接口里复制日志与错误契约。
void respond_serialization_failure(cnetmod::http::request_context& context,
    std::string_view reason);

/**
 * @brief 下发强类型 DTO；业务响应优先使用此入口，避免控制器手工拼 JSON 字段。
 */
template <class T>
void respond_json(cnetmod::http::request_context& context, const T& body,
    int status = cnetmod::http::status::ok)
{
    const auto encoded = cnetmod::json::write(body);
    if (!encoded)
    {
        respond_serialization_failure(context, encoded.error().message());
        return;
    }
    context.json(status, *encoded);
}

} // namespace cnetnexus::framework::web
