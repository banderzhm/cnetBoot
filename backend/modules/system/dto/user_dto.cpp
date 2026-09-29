module cnetboot.system.dto.user;

import cnetmod.json;
import cnetboot.framework.pagination;

namespace cnetboot::system {

auto to_document(const user_dto& value) -> cnetmod::json::document
{
    return cnetmod::json::object({{"id", value.id},
        {"departmentId", value.department_id},
        {"departmentName", value.department_name},
        {"username", value.username}, {"nickname", value.nickname},
        {"email", value.email}, {"enabled", value.enabled}});
}

auto to_document(
    const cnetboot::framework::pagination::page_result<user_dto>& value)
    -> cnetmod::json::document
{
    auto rows = cnetmod::json::array();
    rows.get_array().reserve(value.items.size());
    for (const auto& row : value.items)
        rows.get_array().push_back(to_document(row));
    return cnetmod::json::object({{"rows", std::move(rows)},
        {"total", value.total}, {"page", value.page},
        {"pageSize", value.page_size}});
}

} // namespace cnetboot::system
