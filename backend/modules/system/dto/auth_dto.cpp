module cnetboot.system.dto.auth;

import cnetmod.json;

namespace cnetboot::system {

auto to_document(const token_response& value) -> cnetmod::json::document
{
    return cnetmod::json::object({
        {"accessToken", value.access_token},
        {"tokenType", value.token_type},
        {"expiresIn", value.expires_in},
    });
}

auto revoked_document() -> cnetmod::json::document
{
    return cnetmod::json::object({{"revoked", true}});
}

} // namespace cnetboot::system
