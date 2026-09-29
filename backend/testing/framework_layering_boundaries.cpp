import std;
import cnetmod.core.log;

namespace {

struct expectations
{
    int failures{};

    void require(bool value, std::string_view message)
    {
        if (value)
            return;
        ++failures;
        logger::error{"FAILED: {}", message};
    }
};

[[nodiscard]] auto normalized(const std::filesystem::path& path) -> std::string
{
    auto value = path.generic_string();
    std::ranges::transform(value, value.begin(), [](unsigned char ch)
    {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

[[nodiscard]] auto source_file(const std::filesystem::path& path) -> bool
{
    const auto extension = path.extension().string();
    return extension == ".cpp" || extension == ".cppm" || extension == ".xml" ||
        extension == ".sql" || extension == ".yaml" || extension == ".md";
}

[[nodiscard]] auto read_text(const std::filesystem::path& path) -> std::string
{
    std::ifstream input{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

} // namespace

auto main() -> int
{
    expectations expect;
    const auto root = std::filesystem::current_path();
    const auto modules = root / "modules";
    const auto repository_instructions = root.parent_path() / "AGENTS.md";

    expect.require(std::filesystem::exists(repository_instructions),
        "repository AGENTS.md must exist");
    if (std::filesystem::exists(repository_instructions))
    {
        const auto instructions = read_text(repository_instructions);
        expect.require(instructions.contains("3rdparty/cnetmod/AGENTS.md") &&
                instructions.contains("完整读取最新"),
            "AI instructions must require reading the latest cnetmod AGENTS.md in full");
        expect.require(instructions.contains("wsl_clang_3rdparty_build.sh") &&
                instructions.contains("windows_3rdparty_build.ps1"),
            "build instructions must require prebuilding cnetmod before the backend");
    }

    const auto repository_root = root.parent_path();
    expect.require(std::filesystem::exists(repository_root / "3rdparty/cnetmod/AGENTS.md"),
        "cnetmod must be available as the repository-local 3rdparty submodule");
    for (const auto script : {"windows_3rdparty_build.ps1", "windows_build.ps1",
             "wsl_clang_3rdparty_build.sh", "wsl_clang_build.sh"})
        expect.require(std::filesystem::exists(repository_root / "script" / script),
            std::format("required build script is missing: {}", script));

    const std::set<std::string> allowed_modules{"ai", "chat", "system"};
    for (const auto& entry : std::filesystem::directory_iterator{modules})
        if (entry.is_directory())
            expect.require(allowed_modules.contains(entry.path().filename().string()),
                std::format("unexpected business module: {}", entry.path().string()));

    const std::array forbidden_business{
        std::string{"zhou"} + "yi",
        std::string{"xuan"} + "xue",
        std::string{"ba"} + "zi",
        std::string{"liu_"} + "yao",
        std::string{"qi_"} + "men",
        std::string{"zi_"} + "wei",
        std::string{"mei_"} + "hua",
        std::string{"da_liu_"} + "ren",
        std::string{"chart"} + "context",
        std::string{"cnet"} + "nexus"};

    for (const auto& entry : std::filesystem::recursive_directory_iterator{root})
    {
        if (!entry.is_regular_file() || !source_file(entry.path()))
            continue;
        const auto path = normalized(std::filesystem::relative(entry.path(), root));
        if (path.starts_with("testing/") || path.starts_with("build/"))
            continue;
        auto text = read_text(entry.path());
        std::ranges::transform(text, text.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
        for (const auto& term : forbidden_business)
            expect.require(text.find(term) == std::string::npos,
                std::format("vertical-business term '{}' leaked into {}", term, path));

        if (path.find("/controller/") != std::string::npos)
            expect.require(path.find("_dto.") == std::string::npos,
                std::format("DTO must live in the sibling dto directory: {}", path));
        if (path.find("/mapper/") != std::string::npos)
            expect.require(path.find("_record.cppm") == std::string::npos,
                std::format("database entities must live in domain: {}", path));
        if (path.find("/service/") != std::string::npos)
        {
            const auto name = entry.path().filename().string();
            expect.require(name != "batch_service.cpp" && name != "batch_service.cppm" &&
                    name != "common_service.cpp" && name != "common_service.cppm",
                std::format("service must be named after a concrete capability: {}", path));
        }
    }

    expect.require(std::filesystem::exists(modules / "system" / "dto"),
        "system DTOs must live beside controller/service/mapper");
    expect.require(std::filesystem::exists(modules / "chat" / "dto"),
        "chat DTOs must live beside controller/service/mapper");
    expect.require(std::filesystem::exists(modules / "ai" / "service" / "model"),
        "AI provider integration must stay behind the ai service/model boundary");

    return expect.failures == 0 ? 0 : 1;
}
