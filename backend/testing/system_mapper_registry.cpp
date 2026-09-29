import std;
import cnetmod.orm;

auto main(int argc, char** argv) -> int
{
    if (argc != 2)
        return 2;

    cnetmod::orm::mapper_registry registry;
    const auto loaded = registry.load_directory(argv[1]);
    if (!loaded)
        return 3;

    constexpr std::array required{
        "SystemUserMapper.countPage",
        "SystemUserMapper.findPage",
        "SystemRoleMapper.findByUserId",
        "SystemRoleMapper.findUserIds",
        "SystemMenuMapper.findByRoleIds",
        "SystemMenuMapper.findPermissionCodes",
        "SystemDepartmentMapper.findVisible",
        "SystemDepartmentMapper.findSubtree",
    };
    for (const auto statement : required)
    {
        if (registry.find_statement(statement) == nullptr)
            return 4;
    }

    cnetmod::orm::param_context parameters;
    parameters.set("userName", std::string{"alice"});
    parameters.set("offset", std::uint64_t{20});
    parameters.set("limit", std::uint64_t{20});

    const auto* statement =
        registry.find_statement("SystemUserMapper.findPage");
    const auto* fragments = registry.get_fragments("SystemUserMapper");
    if (statement == nullptr || fragments == nullptr)
        return 5;
    cnetmod::orm::dynamic_sql_processor processor{
        cnetmod::orm::format_options{}};
    const auto sql = processor.process(*statement, parameters, *fragments);
    if (!sql.prepared_sql.contains("LIMIT"))
        return 63;
    if (!sql.prepared_sql.contains("OFFSET"))
        return 64;
    if (sql.params.size() != 3)
        return static_cast<int>(70 + sql.params.size());

    const auto* visibility = registry.find_statement(
        "SystemDepartmentMapper.findVisible");
    const auto* department_fragments = registry.get_fragments(
        "SystemDepartmentMapper");
    if (!visibility || !department_fragments)
        return 80;
    cnetmod::orm::param_context department_scope;
    cnetmod::orm::param_context root;
    root.set("value", std::int64_t{7});
    department_scope.add_collection("departmentIds", {root});
    const auto custom = processor.process(
        *visibility, department_scope, *department_fragments);
    if (!custom.prepared_sql.contains("d.dept_id IN") ||
        custom.params.size() != 1)
        return 81;

    const auto* subtree = registry.find_statement(
        "SystemDepartmentMapper.findSubtree");
    department_scope.set("departmentId", std::int64_t{7});
    const auto tree = processor.process(
        *subtree, department_scope, *department_fragments);
    if (!tree.prepared_sql.contains("WITH RECURSIVE") ||
        !tree.prepared_sql.contains("child.parent_id = parent.dept_id") ||
        tree.params.size() != 1)
        return 82;
    return 0;
}
