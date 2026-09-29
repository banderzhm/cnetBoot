module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.framework.web.error_response;

import std;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetnexus.framework.error;

namespace cnetnexus::framework::web {

auto error_http_status(cnetnexus::framework::error::error_code code) noexcept -> int
{
    const auto entry = cnetnexus::framework::error::find(code);
    return entry ? entry->http_status : 500;
}

auto error_body(cnetnexus::framework::error::error_code code) -> error_response
{
    const auto entry = cnetnexus::framework::error::find(code);
    if (!entry)
        return {"500", "SYSTEM.INTERNAL_ERROR", "错误码未登记", false};
    return {
        .code = std::to_string(entry->http_status),
        .status = std::string{entry->name},
        .message = std::string{entry->default_message},
        .retryable = entry->retryable,
    };
}

auto error_body_with_message(cnetnexus::framework::error::error_code code,
    std::string_view detail_message) -> error_response
{
    auto body = error_body(code);
    if (!detail_message.empty())
        body.message = detail_message;
    return body;
}

namespace {

    auto encode(error_response body) -> std::string
    {
        auto result = cnetmod::json::write(body);
        if (result)
            return std::move(*result);
        return R"({"code":"500","status":"SYSTEM.INTERNAL_ERROR","message":"响应序列化失败","retryable":false})";
    }

} // namespace

auto error_body_json(cnetnexus::framework::error::error_code code) -> std::string
{
    return encode(error_body(code));
}

auto error_body_json(cnetnexus::framework::error::error_code code,
    std::string_view detail_message) -> std::string
{
    return encode(error_body_with_message(code, detail_message));
}

void respond_error(cnetmod::http::request_context& context,
    cnetnexus::framework::error::error_code code)
{
    context.json(error_http_status(code), error_body_json(code));
}

void respond_error(cnetmod::http::request_context& context,
    cnetnexus::framework::error::error_code code, std::string_view detail_message)
{
    context.json(error_http_status(code), error_body_json(code, detail_message));
}

auto error_registry_body() -> error_registry_response
{
    error_registry_response result;
    const auto registry = cnetnexus::framework::error::registry();
    result.errors.reserve(registry.size());
    for (const auto& entry : registry)
    {
        result.errors.push_back({
            .status = std::string{entry.name},
            .httpStatus = entry.http_status,
            .message = std::string{entry.default_message},
            .owner = std::string{entry.owner},
            .category = std::string{entry.category},
            .retryable = entry.retryable,
            .exposable = entry.exposable,
            .deprecated = entry.deprecated,
        });
    }
    return result;
}

} // namespace cnetnexus::framework::web
