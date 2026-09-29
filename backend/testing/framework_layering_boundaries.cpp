/**
 * @brief 分层边界用例：外部依赖只允许出现在指定的适配层目录。
 *
 * 为什么做成测试而不是文档里的 grep：写进文档的命令不会被执行，
 * 而这条约束一旦被破坏（上游类型渗进领域层、其他层直接 import 上游），
 * 编译仍然通过、运行也看不出问题，只有边界在多轮改动后被悄悄侵蚀。
 *
 * 规则（见 docs/layering.md）：
 *  1. \`import ZhouYi.*\` 只允许出现在 modules/chart/service/xuanxue/；
 *  2. \`import cnetmod.ai\` / chat_model_* 只允许出现在 modules/reading/service/model/；
 *  3. 业务模块的目录名只允许 controller / service / domain / mapper / dto 五种；
 *  4. HTTP/web 模块只允许由 controller 导入；
 *  5. 源文件路径必须与声明的 cnetnexus.<module>.<layer> 模块一致；
 *  6. domain / dto 不得反向依赖外层，非 controller 不得依赖 controller；
 *  7. DTO 文件必须归属 dto/，不得藏进 controller。
 *
 * 用例通过扫描源码树完成，需要知道仓库根目录：由 CTest 的 WORKING_DIRECTORY
 * 指向 backend/ 之后按相对路径扫描。
 */
import std;

namespace {

/// @brief 断言收集器。
class assertions
{
public:
    void check(bool condition, std::string_view what)
    {
        ++total_;
        if (!condition)
        {
            ++failed_;
            std::println("FAIL: {}", what);
        }
    }

    [[nodiscard]] auto report() const -> int
    {
        std::println("layering_boundaries: {} assertions, {} failures", total_, failed_);
        return failed_ == 0 ? 0 : 1;
    }

private:
    int total_{};
    int failed_{};
};

/// @brief 读整个文件；失败返回空。
[[nodiscard]] auto read_file(const std::filesystem::path& path) -> std::string
{
    std::ifstream stream{path, std::ios::binary};
    if (!stream)
        return {};
    return std::string{std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
}

/// @brief 收集 backend/ 下所有 .cpp / .cppm（跳过 build 目录）。
[[nodiscard]] auto collect_sources(const std::filesystem::path& root)
    -> std::vector<std::filesystem::path>
{
    std::vector<std::filesystem::path> result;
    std::error_code ignored;
    for (auto it = std::filesystem::recursive_directory_iterator(
             root, std::filesystem::directory_options::skip_permission_denied, ignored);
         it != std::filesystem::recursive_directory_iterator(); it.increment(ignored))
    {
        if (ignored)
            break;
        const auto& entry = *it;
        if (entry.is_directory())
        {
            const auto name = entry.path().filename().string();
            // testing/ 也要跳过：本用例自身含有被检查的模式字符串字面量
            // （"import ZhouYi" 等），扫到自己会误报。
            if (name == "build" || name == ".git" || name == "testing")
                it.disable_recursion_pending();
            continue;
        }
        const auto ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".cppm")
            result.push_back(entry.path());
    }
    std::ranges::sort(result);
    return result;
}

/// @brief 路径里是否包含某个片段（统一成 / 分隔）。
[[nodiscard]] auto path_contains(const std::filesystem::path& path,
    std::string_view fragment) -> bool
{
    auto text = path.generic_string();
    return text.find(fragment) != std::string::npos;
}

/// @brief 提取 C++23 具名模块声明；忽略 global module fragment 的 `module;`。
[[nodiscard]] auto declared_module(std::string_view source) -> std::string_view
{
    constexpr std::array prefixes{
        std::string_view{"export module cnetnexus."},
        std::string_view{"module cnetnexus."}};
    for (const auto prefix : prefixes)
    {
        const auto begin = source.find(prefix);
        if (begin == std::string_view::npos)
            continue;
        const auto end = source.find(';', begin);
        if (end != std::string_view::npos)
            return source.substr(begin, end - begin);
    }
    return {};
}

/// @brief 去掉声明关键字，只返回 cnetnexus.* 模块名。
[[nodiscard]] auto declared_module_name(std::string_view source) -> std::string_view
{
    auto declaration = declared_module(source);
    constexpr std::array prefixes{
        std::string_view{"export module "}, std::string_view{"module "}};
    for (const auto prefix : prefixes)
    {
        if (declaration.starts_with(prefix))
            return declaration.substr(prefix.size());
    }
    return {};
}

/// @brief 只检查真正的 import 行，避免注释和字符串字面量造成误报。
[[nodiscard]] auto imports_module_containing(
    std::string_view source, std::string_view fragment) -> bool
{
    while (!source.empty())
    {
        const auto newline = source.find('\n');
        auto line = source.substr(0, newline);
        const auto first = line.find_first_not_of(" \t\r");
        if (first != std::string_view::npos)
        {
            line.remove_prefix(first);
            if ((line.starts_with("import ") || line.starts_with("export import ")) &&
                line.find(fragment) != std::string_view::npos)
                return true;
        }
        if (newline == std::string_view::npos)
            break;
        source.remove_prefix(newline + 1);
    }
    return false;
}

} // namespace

int main()
{
    assertions expect;

    // CTest 把 WORKING_DIRECTORY 设为 backend/，因此仓库根就是当前目录。
    const std::filesystem::path root{"."};
    expect.check(std::filesystem::exists(root / "modules"),
        "run from the backend root (modules/ must exist)");

    const auto sources = collect_sources(root);
    expect.check(sources.size() > 100,
        "the source tree was actually scanned (more than 100 files)");

    // ---- 1. ZhouYi 上游只允许出现在 chart 的适配层 ----
    int zhouyi_total = 0;
    std::vector<std::string> zhouyi_violations;
    for (const auto& path : sources)
    {
        const auto text = read_file(path);
        if (text.find("import ZhouYi") == std::string::npos)
            continue;
        ++zhouyi_total;
        if (!path_contains(path, "modules/chart/service/xuanxue/"))
            zhouyi_violations.push_back(path.generic_string());
    }
    expect.check(zhouyi_total > 0,
        "the ZhouYi adapter layer is present (otherwise the rule proves nothing)");
    for (const auto& v : zhouyi_violations)
        std::println("  ZhouYi import outside the adapter layer: {}", v);
    expect.check(zhouyi_violations.empty(),
        "import ZhouYi appears ONLY in modules/chart/service/xuanxue/");

    // ---- 2. 模型调用上游只允许出现在 reading 的适配层 ----
    std::vector<std::string> model_violations;
    int model_total = 0;
    for (const auto& path : sources)
    {
        const auto text = read_file(path);
        const bool uses_upstream = text.find("import cnetmod.ai") != std::string::npos ||
            text.find("import cnetmod.application.chat_model_template") != std::string::npos ||
            text.find("import cnetmod.application.chat_model_service") != std::string::npos ||
            text.find("import cnetmod.application.chat_model_composition") != std::string::npos;
        if (!uses_upstream)
            continue;
        ++model_total;
        if (!path_contains(path, "modules/reading/service/model/"))
            model_violations.push_back(path.generic_string());
    }
    expect.check(model_total > 0,
        "the chat-model adapter layer is present");
    for (const auto& v : model_violations)
        std::println("  model upstream import outside the adapter layer: {}", v);
    expect.check(model_violations.empty(),
        "cnetmod.ai / chat_model_* imports appear ONLY in modules/reading/service/model/");

    // ---- 3. 业务模块目录名只允许约定的几种 ----
    const std::vector<std::string> allowed{"controller", "service", "domain", "mapper",
        "dto"};
    std::vector<std::string> bad_dirs;
    std::error_code ignored;
    for (const auto& module : std::filesystem::directory_iterator(root / "modules", ignored))
    {
        if (!module.is_directory())
            continue;
        for (const auto& layer : std::filesystem::directory_iterator(module.path(), ignored))
        {
            if (!layer.is_directory())
                continue;
            const auto name = layer.path().filename().string();
            if (std::ranges::find(allowed, name) == allowed.end())
                bad_dirs.push_back(layer.path().generic_string());
        }
    }
    for (const auto& v : bad_dirs)
        std::println("  unexpected layer directory: {}", v);
    expect.check(bad_dirs.empty(),
        "every modules/<m>/* directory is one of controller/service/domain/mapper/dto");

    // ---- 4. HTTP 适配层不得反向渗入业务内层 ----
    std::vector<std::string> web_violations;
    for (const auto& path : sources)
    {
        if (!path_contains(path, "modules/") ||
            path_contains(path, "/controller/"))
            continue;
        const auto text = read_file(path);
        if (imports_module_containing(text, "cnetnexus.framework.web") ||
            imports_module_containing(text, "cnetmod.protocol.http"))
            web_violations.push_back(path.generic_string());
    }
    for (const auto& v : web_violations)
        std::println("  framework.web import outside controller: {}", v);
    expect.check(web_violations.empty(),
        "HTTP/web imports appear ONLY in business controller layers");

    // ---- 5. 物理目录与 C++ 具名模块必须严格一致 ----
    std::vector<std::string> module_ownership_violations;
    for (const auto& path : sources)
    {
        const auto relative = path.lexically_relative(root).generic_string();
        if (!relative.starts_with("modules/"))
            continue;

        std::vector<std::string_view> parts;
        std::string_view remaining{relative};
        while (!remaining.empty())
        {
            const auto slash = remaining.find('/');
            parts.push_back(remaining.substr(0, slash));
            if (slash == std::string_view::npos)
                break;
            remaining.remove_prefix(slash + 1);
        }
        if (parts.size() < 4)
        {
            module_ownership_violations.push_back(relative + " (invalid path)");
            continue;
        }

        const auto text = read_file(path);
        const auto actual = declared_module_name(text);
        const auto expected = std::format(
            "cnetnexus.{}.{}.", parts[1], parts[2]);
        if (!actual.starts_with(expected))
            module_ownership_violations.push_back(std::format(
                "{} (expected {}*, got {})", relative, expected,
                actual.empty() ? "<none>" : actual));
    }
    for (const auto& v : module_ownership_violations)
        std::println("  module ownership violation: {}", v);
    expect.check(module_ownership_violations.empty(),
        "every business source declares cnetnexus.<module>.<physical-layer>.*");

    // ---- 6. 业务内层依赖只能指向同层或更内层 ----
    std::vector<std::string> dependency_violations;
    for (const auto& path : sources)
    {
        if (!path_contains(path, "modules/"))
            continue;
        const bool controller = path_contains(path, "/controller/");
        const bool domain = path_contains(path, "/domain/");
        const bool dto = path_contains(path, "/dto/");
        const auto text = read_file(path);

        if (!controller && imports_module_containing(text, ".controller."))
            dependency_violations.push_back(
                path.generic_string() + " (imports controller)");
        if (domain && (imports_module_containing(text, ".service.") ||
                          imports_module_containing(text, ".mapper.") ||
                          imports_module_containing(text, ".dto.")))
            dependency_violations.push_back(
                path.generic_string() + " (domain imports an outer layer)");
        if (dto && (imports_module_containing(text, ".service.") ||
                       imports_module_containing(text, ".mapper.")))
            dependency_violations.push_back(
                path.generic_string() + " (dto imports service/mapper)");
        if (path_contains(path, "/mapper/") &&
            imports_module_containing(text, ".dto."))
            dependency_violations.push_back(
                path.generic_string() + " (mapper imports dto)");
        if (!domain && (text.find("CNETMOD_MODEL(") != std::string::npos ||
                           text.find("CNETMOD_PROJECTION(") != std::string::npos))
            dependency_violations.push_back(
                path.generic_string() + " (ORM model/projection outside domain)");
        if (controller && imports_module_containing(text, ".mapper."))
            dependency_violations.push_back(
                path.generic_string() + " (controller imports mapper)");
        if (controller && imports_module_containing(text, "cnetmod.orm"))
            dependency_violations.push_back(
                path.generic_string() + " (controller imports ORM)");
        if (controller &&
            (text.find("managed_repository<") != std::string::npos ||
                text.find("repository_factory<") != std::string::npos))
            dependency_violations.push_back(
                path.generic_string() + " (controller owns repository access)");
        if (path_contains(path, "/mapper/") &&
            imports_module_containing(text, ".service."))
            dependency_violations.push_back(
                path.generic_string() + " (mapper imports service)");
    }
    for (const auto& v : dependency_violations)
        std::println("  dependency direction violation: {}", v);
    expect.check(dependency_violations.empty(),
        "domain/dto/mapper dependencies point inward and controller is never imported inward");

    // ---- 7. DTO 是 controller 的同级层，不得藏进 controller ----
    std::vector<std::string> dto_violations;
    for (const auto& path : sources)
    {
        if (!path_contains(path, "modules/"))
            continue;
        const auto text = read_file(path);
        const auto module = declared_module(text);
        const auto filename = path.filename().string();
        const bool dto_named_file = filename.find("dto") != std::string::npos;
        const bool dto_module = module.find(".dto.") != std::string_view::npos;

        if (path_contains(path, "/controller/") &&
            (dto_named_file || dto_module))
            dto_violations.push_back(path.generic_string());
        if (path_contains(path, "/dto/") &&
            (!dto_module || text.find(".controller.") != std::string::npos))
            dto_violations.push_back(path.generic_string());
    }
    std::ranges::sort(dto_violations);
    dto_violations.erase(
        std::ranges::unique(dto_violations).begin(), dto_violations.end());
    for (const auto& v : dto_violations)
        std::println("  DTO ownership violation: {}", v);
    expect.check(dto_violations.empty(),
        "DTO modules live under modules/<m>/dto and never under controller");

    // ---- 8. Service 必须按业务能力命名，禁止只描述技术形态 ----
    constexpr std::array forbidden_service_names{
        std::string_view{"batch_service"},
        std::string_view{"common_service"},
        std::string_view{"data_service"},
        std::string_view{"base_service"},
        std::string_view{"manager_service"},
        std::string_view{"helper_service"},
    };
    std::vector<std::string> service_name_violations;
    for (const auto& path : sources)
    {
        if (!path_contains(path, "/service/"))
            continue;
        const auto stem = path.stem().string();
        const auto text = read_file(path);
        for (const auto forbidden : forbidden_service_names)
        {
            if (stem == forbidden ||
                text.find(std::format("class {}", forbidden)) != std::string::npos ||
                text.find(std::format("struct {}", forbidden)) != std::string::npos)
                service_name_violations.push_back(path.generic_string());
        }
    }
    std::ranges::sort(service_name_violations);
    service_name_violations.erase(
        std::ranges::unique(service_name_violations).begin(),
        service_name_violations.end());
    for (const auto& v : service_name_violations)
        std::println("  non-business service name: {}", v);
    expect.check(service_name_violations.empty(),
        "services use concrete business capability names, not transport/mechanism names");

    // ---- 9. 旧的 client 分层不应再出现 ----
    expect.check(!std::filesystem::exists(root / "modules/chart/client"),
        "modules/chart/client no longer exists");
    expect.check(!std::filesystem::exists(root / "modules/reading/client"),
        "modules/reading/client no longer exists");

    return expect.report();
}
