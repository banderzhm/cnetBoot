import std;
import cnetmod.json;
import cnetmod.orm;
import cnetboot.system.dto.authorization;
import cnetboot.system.domain.sys_role;
import cnetboot.system.domain.sys_user;

auto main() -> int
{
    cnetboot::system::authorization_dto original{
        .user_id = 42,
        .roles = {{.id = 3, .name = "Editor", .key = "editor"}},
        .permissions = {"system:user:list"},
        .menus = {{.id = 1,
            .name = "System",
            .children = {{.id = 2, .parent_id = 1, .name = "Users"}}}},
    };
    const auto encoded = cnetmod::json::write(original);
    if (!encoded)
        return 1;
    const auto decoded =
        cnetmod::json::parse<cnetboot::system::authorization_dto>(*encoded);
    if (!decoded || decoded->user_id != 42 || decoded->roles.size() != 1 ||
        decoded->roles.front().key != "editor" ||
        decoded->permissions != original.permissions ||
        decoded->menus.size() != 1 ||
        decoded->menus.front().children.size() != 1 ||
        decoded->menus.front().children.front().name != "Users")
        return 2;

    const cnetboot::system::sys_role new_role{};
    if (new_role.data_scope !=
        static_cast<int>(cnetboot::system::role_data_scope::self))
        return 3;

    cnetmod::orm::data_permission_interceptor<cnetboot::system::sys_user>
        filter{cnetmod::orm::data_permission_scope{
            .include_owner = true,
            .owner_id = 42,
            .partition_ids = {7, 9},
        }};
    auto guarded = filter.apply(cnetmod::orm::sql_operation::query,
        {.sql = "SELECT u.* FROM sys_user u WHERE u.deleted = 0"});
    if (!guarded || !guarded->sql.contains("u.dept_id IN") ||
        !guarded->sql.contains("u.user_id =") ||
        guarded->parameters.size() != 3)
        return 4;
    return 0;
}
