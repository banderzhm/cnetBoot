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
        extension == ".sql" || extension == ".yaml" || extension == ".yml" ||
        extension == ".md" || extension == ".ps1" || extension == ".sh";
}

[[nodiscard]] auto read_text(const std::filesystem::path& path) -> std::string
{
    std::ifstream input{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] auto is_cjk_lead(std::string_view text, std::size_t offset) noexcept -> bool
{
    const auto first = static_cast<unsigned char>(text[offset]);
    if (first < 0xE3 || first > 0xE9 || offset + 2 >= text.size())
        return false;
    const auto second = static_cast<unsigned char>(text[offset + 1]);
    const auto third = static_cast<unsigned char>(text[offset + 2]);
    if ((second & 0xC0U) != 0x80U || (third & 0xC0U) != 0x80U)
        return false;
    const auto codepoint = ((first & 0x0FU) << 12U) |
        ((second & 0x3FU) << 6U) | (third & 0x3FU);
    return codepoint >= 0x3400U && codepoint <= 0x9FFFU;
}

[[nodiscard]] auto contains_cjk(std::string_view text) noexcept -> bool
{
    for (std::size_t index = 0; index < text.size(); ++index)
        if (is_cjk_lead(text, index))
            return true;
    return false;
}

} // namespace

auto main() -> int
{
    expectations expect;
    const auto root = std::filesystem::current_path();
    const auto modules = root / "modules";
    const auto repository_instructions = root.parent_path() / "AGENTS.md";
    const auto build_definition = root / "CMakeLists.txt";

    expect.require(std::filesystem::exists(repository_instructions),
        "repository AGENTS.md must exist");
    if (std::filesystem::exists(repository_instructions))
    {
        const auto instructions = read_text(repository_instructions);
        expect.require(instructions.contains("3rdparty/cnetmod/AGENTS.md") &&
                instructions.contains("must read the latest"),
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
    expect.require(std::filesystem::exists(repository_root / ".github/workflows/build.yml"),
        "GitHub push and pull requests must execute the build and architecture gate");
    expect.require(std::filesystem::exists(build_definition),
        "backend CMakeLists.txt must exist");
    if (std::filesystem::exists(build_definition))
    {
        const auto cmake = read_text(build_definition);
        expect.require(cmake.contains("add_custom_target(cnetboot_architecture_gate ALL"),
            "the architecture gate must execute as part of every default build");
    }

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

    auto entries = std::filesystem::recursive_directory_iterator{repository_root};
    const auto end = std::filesystem::recursive_directory_iterator{};
    for (; entries != end; ++entries)
    {
        const auto path = normalized(std::filesystem::relative(entries->path(), repository_root));
        if (entries->is_directory() &&
            (path == ".git" || path == "3rdparty" || path.starts_with("backend/build") ||
                path.starts_with(".tmp-")))
        {
            entries.disable_recursion_pending();
            continue;
        }
        const auto& entry = *entries;
        if (!entry.is_regular_file() || !source_file(entry.path()))
            continue;
        auto text = read_text(entry.path());
        expect.require(!contains_cjk(text),
            std::format("repository-owned text must be English-only: {}", path));
        std::ranges::transform(text, text.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
        for (const auto& term : forbidden_business)
            expect.require(text.find(term) == std::string::npos,
                std::format("vertical-business term '{}' leaked into {}", term, path));

        if (path.find("backend/modules/") == std::string::npos)
            continue;
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
