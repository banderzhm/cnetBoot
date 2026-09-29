/**
 * @brief Typed public error responses and registry projections.
 */
export module cnetnexus.framework.web.error_response;

import std;
import cnetmod.protocol.http;
import cnetnexus.framework.error;

export namespace cnetnexus::framework::web {

struct error_response
{
    std::string code;
    std::string status;
    std::string message;
    bool retryable{};
};

struct error_registry_entry_response
{
    std::string status;
    int httpStatus{};
    std::string message;
    std::string owner;
    std::string category;
    bool retryable{};
    bool exposable{};
    bool deprecated{};
};

struct error_registry_response
{
    std::vector<error_registry_entry_response> errors;
};

[[nodiscard]] auto error_body(cnetnexus::framework::error::error_code code)
    -> error_response;
[[nodiscard]] auto error_body_with_message(cnetnexus::framework::error::error_code code,
    std::string_view detail_message) -> error_response;
[[nodiscard]] auto error_body_json(cnetnexus::framework::error::error_code code)
    -> std::string;
[[nodiscard]] auto error_body_json(cnetnexus::framework::error::error_code code,
    std::string_view detail_message) -> std::string;
[[nodiscard]] auto error_http_status(cnetnexus::framework::error::error_code code) noexcept
    -> int;
void respond_error(cnetmod::http::request_context& context,
    cnetnexus::framework::error::error_code code);
void respond_error(cnetmod::http::request_context& context,
    cnetnexus::framework::error::error_code code, std::string_view detail_message);
[[nodiscard]] auto error_registry_body() -> error_registry_response;

} // namespace cnetnexus::framework::web
