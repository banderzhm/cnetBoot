module;

#include <cnetmod/json_codec.hpp>

/**
 * @brief Shared HTTP-layer helpers for the chart controllers.
 *
 * 这里只有协议适配：查询参数解析、审计上下文组装、JSON 写出与 raw 字段校验。
 * 任何业务规则仍留在 service / domain。
 */
export module cnetnexus.chart.controller.route_support;

import std;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetnexus.chart.domain.cases;
import cnetnexus.framework.audit;
import cnetnexus.framework.security.current_user;
import cnetnexus.framework.pagination;
import cnetnexus.framework.web.query;

export namespace cnetnexus::chart {

/** @brief 从 query string 组装分页参数（page / page_size，规范化后返回）。 */
[[nodiscard]] auto page_from(std::string_view query_text)
    -> cnetnexus::framework::pagination::page_request;

/**
 * @brief 解析可选整数参数。
 *
 * 缺省返回 true 且不写 output；参数出现但非整数或越界返回 false
 * （「提供了非法值」绝不静默当成「没提供」）。
 */
[[nodiscard]] auto optional_int_param(std::string_view query_text,
    std::string_view key, int minimum, int maximum, std::optional<int>& output)
    -> bool;

/** @brief 由请求上下文组装审计上下文；未认证时 user_id 为 0。 */
[[nodiscard]] auto audit_actor_from(
    const cnetmod::http::request_context& context,
    const cnetnexus::security::current_user* principal)
    -> cnetnexus::framework::audit_actor;


/** @brief 读取请求作用域里的当前登录用户；未认证返回 nullptr。 */
[[nodiscard]] auto current_principal(
    cnetmod::http::request_context& context) noexcept
    -> const cnetnexus::security::current_user*;

/** @brief 把 raw JSON 值解析成 T；类型不符返回 std::nullopt。 */
template <typename T>
[[nodiscard]] auto scalar_of(const std::optional<cnetmod::json::raw_value>& raw)
    -> std::optional<T>
{
    if (!raw || raw->is_null())
        return std::nullopt;
    auto parsed = cnetmod::json::from_document<T>(*raw);
    if (!parsed)
        return std::nullopt;
    return std::move(*parsed);
}

/** @brief 可空文本：null 视为「显式清空」，返回内层空 optional；类型不符返回最外层 nullopt。 */
[[nodiscard]] auto nullable_text(const cnetmod::json::raw_value& raw)
    -> std::optional<std::optional<std::string>>;

/** @brief 可空 ID 列表：null 视为空列表；类型不符返回 nullopt。 */
[[nodiscard]] auto id_list(const cnetmod::json::raw_value& raw)
    -> std::optional<std::vector<std::uint64_t>>;

} // namespace cnetnexus::chart
