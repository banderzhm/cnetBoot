export module cnetnexus.system.dto.auth;

import std;
import cnetmod.json;

export namespace cnetnexus::system {

struct login_request
{
    std::string username;
    std::string password;
};

struct token_response
{
    std::string access_token;
    std::string token_type{"Bearer"};
    std::int64_t expires_in{};
};

[[nodiscard]] auto to_document(const token_response& value)
    -> cnetmod::json::document;
[[nodiscard]] auto revoked_document() -> cnetmod::json::document;

} // namespace cnetnexus::system
