export module cnetnexus.system.dto.user;

import std;
import cnetmod.json;
import cnetnexus.framework.pagination;

export namespace cnetnexus::system {

/**
 * @brief Public system-user representation returned by administration APIs.
 */
struct user_dto
{
    std::int64_t id{};
    std::int64_t department_id{};
    std::string department_name;
    std::string username;
    std::string nickname;
    std::string email;
    bool enabled{};
};

[[nodiscard]] auto to_document(const user_dto& value)
    -> cnetmod::json::document;
[[nodiscard]] auto to_document(
    const cnetnexus::framework::pagination::page_result<user_dto>& value)
    -> cnetmod::json::document;

} // namespace cnetnexus::system
