/**
 * @brief 多供应商 / 多模型 / 多 key 的离线验收用例（不连网、不需要运行中的 cnetmod 服务）。
 *
 * 覆盖五层：
 *  1. 小节结构校验 validate_llm_options：默认值可达、重复名/模型/key、退避参数；
 *  2. key 解析 resolve_llm_directory：未知服务、非 openai、空 api_key、pool_size 越界、
 *     未启用实例；
 *  3. 目标解析 resolve_llm_target：默认值回退、未知 provider/model 拒绝、未配置；
 *  4. composed_llm_service：协议翻译（模型名、推理强度）、按供应商路由、错误码映射、
 *     流式增量转发与客户端断开（用脚本化 framework_ai::chat_model 替身，不连网）；
 *  5. 脱敏目录 llm_catalog_json：只暴露供应商/模型/key 是否配置，绝不回显端点或密钥。
 *
 * 另有一段端到端配置用例：真读 YAML + ${ENV} 展开 + 顶层 llm 小节解码与解析
 * （临时文件与环境变量，仍然不联网）。
 *
 * 多 key 的轮询与失败切 key 由 cnetBoot 显式调用 cnetmod add_chat_model 开启；这里验证
 * 策略输入，cnetmod 测试负责验证“未显式声明时拒绝多实例组合”。
 */
// setenv/_putenv_s 只经由 <cstdlib> 提供（import std 不带 C 头）。
#include <cstdlib>

import std;
import cnetmod.ai;
import cnetmod.protocol.openai;
import cnetmod.application.configuration;
import cnetmod.application.options;
import cnetmod.coro.task;
import cnetmod.json;
import cnetboot.framework.config.llm_settings;
import cnetboot.framework.error;
import cnetboot.ai.domain.llm_contract;
import cnetboot.ai.service.llm_catalog;
import cnetboot.ai.service.llm_service;
import cnetboot.ai.service.model.composed_llm_service;

namespace {

using error_code = cnetboot::framework::error::error_code;
namespace config = cnetboot::framework::config;
namespace app_ai = cnetboot::ai;
namespace framework_ai = cnetmod::ai;

using service_table =
    std::map<std::string, cnetmod::application::configured_service, std::less<>>;

/// @brief 记录失败的检查点名；全部通过才返回 0。
class checklist
{
public:
    void check(bool condition, std::string_view name)
    {
        ++total_;
        if (!condition)
            failures_.emplace_back(name);
    }

    [[nodiscard]] auto report() const -> int
    {
        for (const auto& name : failures_)
            std::println("  FAILED: {}", name);
        std::println("llm_provider_selection: {} checkpoints, {} failures", total_,
            failures_.size());
        return failures_.empty() ? 0 : 1;
    }

private:
    int total_{};
    std::vector<std::string> failures_;
};

[[nodiscard]] auto openai_service(std::string instance, std::string api_key = "sk-test",
    bool enabled = true) -> cnetmod::application::configured_service
{
    cnetmod::application::configured_service service;
    service.name = "openai";
    service.instance = std::move(instance);
    service.enabled = enabled;
    service.properties = cnetmod::json::object();
    service.properties["base_url"] = std::string{"https://api.example.test/v1"};
    service.properties["api_key"] = std::move(api_key);
    return service;
}

/// @brief 基准服务表：openai 两条 key；deepseek 一条未启用的 key。
[[nodiscard]] auto services_fixture() -> service_table
{
    service_table services;
    services.emplace("openai-primary", openai_service("alpha"));
    services.emplace("openai-secondary", openai_service("beta"));
    services.emplace("deepseek-primary", openai_service("deepseek", "sk-test", false));
    return services;
}

/// @brief 基准小节：openai 两条 key 两个模型；deepseek 一条 key 一个模型。
[[nodiscard]] auto options_fixture() -> config::llm_options
{
    config::llm_options options;
    options.default_provider = "openai";
    options.default_model = "gpt-4o-mini";
    options.providers.push_back(config::llm_provider_options{
        .name = "openai",
        .models = {"gpt-4o-mini", "gpt-4o"},
        .keys = {{.name = "primary", .service = "openai-primary"},
            {.name = "secondary", .service = "openai-secondary"}}});
    options.providers.push_back(config::llm_provider_options{
        .name = "deepseek",
        .models = {"deepseek-chat"},
        .keys = {{.service = "deepseek-primary"}}});
    return options;
}

[[nodiscard]] auto rejected_with(const std::expected<void, std::string>& result,
    std::string_view fragment) -> bool
{
    return !result && result.error().contains(fragment);
}

/**
 * @brief 脚本化的组合模型替身：记录收到的请求，按脚本成功、失败或流式下发。
 */
class scripted_model final : public framework_ai::chat_model
{
public:
    std::optional<std::string> failure;
    std::vector<std::string> deltas{"第一段", "第二段"};
    std::vector<framework_ai::chat_request> requests;
    std::size_t delivered{};

    auto invoke(framework_ai::chat_request request, const framework_ai::run_config&)
        -> cnetmod::task<std::expected<framework_ai::chat_response, std::string>> override
    {
        requests.push_back(std::move(request));
        if (failure)
            co_return std::unexpected{*failure};
        co_return response(join(deltas.size()));
    }

    auto stream(framework_ai::chat_request request, stream_handler handler,
        const framework_ai::run_config&)
        -> cnetmod::task<std::expected<framework_ai::chat_response, std::string>> override
    {
        requests.push_back(std::move(request));
        if (failure)
            co_return std::unexpected{*failure};
        delivered = 0;
        for (const auto& delta : deltas)
        {
            framework_ai::chat_chunk chunk;
            chunk.delta_content = delta;
            ++delivered;
            if (!co_await handler(chunk))
                break;
        }
        co_return response(join(delivered));
    }

private:
    [[nodiscard]] auto join(std::size_t count) const -> std::string
    {
        std::string text;
        for (std::size_t index = 0; index < count && index < deltas.size(); ++index)
            text += deltas[index];
        return text;
    }

    [[nodiscard]] static auto response(std::string content) -> framework_ai::chat_response
    {
        framework_ai::chat_response result;
        cnetmod::openai::choice first;
        first.msg.content = std::move(content);
        result.choices.push_back(std::move(first));
        result.token_usage.prompt_tokens = 11;
        result.token_usage.completion_tokens = 7;
        return result;
    }
};

[[nodiscard]] auto request_for(std::string provider, std::string model)
    -> app_ai::llm_request
{
    return app_ai::llm_request{
        .messages = {{.role = "system", .content = "规则"},
            {.role = "user", .content = "问题"}},
        .provider = std::move(provider),
        .model = std::move(model),
        .reasoning_effort = "low",
    };
}

void check_options_validation(checklist& expect)
{
    expect.check(config::validate_llm_options(options_fixture()).has_value(),
        "fixture options are valid");
    expect.check(config::validate_llm_options(config::llm_options{}).has_value(),
        "empty section disables LLM without error");

    auto defaults_without_providers = config::llm_options{};
    defaults_without_providers.default_provider = "openai";
    expect.check(rejected_with(config::validate_llm_options(defaults_without_providers),
                     "providers 为空"),
        "defaults without providers are rejected");

    auto missing_default = options_fixture();
    missing_default.default_model.clear();
    expect.check(rejected_with(config::validate_llm_options(missing_default),
                     "default_model"),
        "missing default_model is rejected");

    auto unreachable_provider = options_fixture();
    unreachable_provider.default_provider = "claude";
    expect.check(rejected_with(config::validate_llm_options(unreachable_provider),
                     "default_provider=claude"),
        "unknown default_provider is rejected");

    auto unreachable_model = options_fixture();
    unreachable_model.default_model = "deepseek-chat";
    expect.check(rejected_with(config::validate_llm_options(unreachable_model),
                     "default_model=deepseek-chat"),
        "default_model outside the default provider is rejected");

    auto duplicate_provider = options_fixture();
    duplicate_provider.providers[1].name = "openai";
    duplicate_provider.providers[1].keys = {{.service = "other"}};
    expect.check(rejected_with(config::validate_llm_options(duplicate_provider), "重复"),
        "duplicate provider names are rejected");

    auto duplicate_model = options_fixture();
    duplicate_model.providers[0].models.push_back("gpt-4o");
    expect.check(rejected_with(config::validate_llm_options(duplicate_model), "重复"),
        "duplicate models are rejected");

    auto duplicate_alias = options_fixture();
    duplicate_alias.providers[0].keys[1].name = "primary";
    expect.check(rejected_with(config::validate_llm_options(duplicate_alias), "重复"),
        "duplicate key aliases are rejected");

    auto shared_instance = options_fixture();
    shared_instance.providers[1].keys[0].service = "openai-primary";
    expect.check(rejected_with(config::validate_llm_options(shared_instance),
                     "services.openai-primary"),
        "a services instance referenced twice is rejected");

    auto no_keys = options_fixture();
    no_keys.providers[1].keys.clear();
    expect.check(rejected_with(config::validate_llm_options(no_keys), "keys"),
        "providers without keys are rejected");

    auto wrong_type = options_fixture();
    wrong_type.providers[1].type = "anthropic";
    expect.check(rejected_with(config::validate_llm_options(wrong_type), "openai"),
        "unsupported provider type is rejected");

    auto zero_attempts = options_fixture();
    zero_attempts.resilience.attempts_per_key = 0;
    expect.check(rejected_with(config::validate_llm_options(zero_attempts),
                     "attempts_per_key"),
        "zero attempts per key is rejected");

    auto inverted_backoff = options_fixture();
    inverted_backoff.resilience.max_backoff_ms = 10;
    expect.check(rejected_with(config::validate_llm_options(inverted_backoff), "退避"),
        "backoff ceiling below the initial backoff is rejected");
}

void check_directory_resolution(checklist& expect)
{
    const auto services = services_fixture();
    auto resolved = config::resolve_llm_directory(options_fixture(), services);
    expect.check(resolved.has_value(), "fixture resolves");
    if (!resolved)
        return;
    const auto* openai = resolved->find_provider("openai");
    const auto* deepseek = resolved->find_provider("deepseek");
    expect.check(openai != nullptr && openai->keys.size() == 2 &&
            openai->keys[0].instance == "alpha" && openai->keys[1].instance == "beta",
        "keys resolve to cnetmod instance names in declaration order");
    expect.check(openai != nullptr &&
            openai->enabled_instances() == std::vector<std::string>{"alpha", "beta"},
        "enabled instances feed add_chat_model explicitly");
    expect.check(deepseek != nullptr && !deepseek->usable() &&
            deepseek->keys[0].name == "deepseek-primary",
        "disabled instance keeps the provider listed but unusable; alias defaults to service");
    expect.check(resolved->usable(), "directory with one usable provider is usable");

    auto missing = services;
    missing.erase("openai-secondary");
    auto unknown = config::resolve_llm_directory(options_fixture(), missing);
    expect.check(!unknown && unknown.error().contains("llm.providers[0].keys[1]") &&
            unknown.error().contains("services.openai-secondary"),
        "unknown service reference is rejected with its path");

    auto wrong_type = services;
    wrong_type.at("openai-primary").name = "redis";
    auto rejected_type = config::resolve_llm_directory(options_fixture(), wrong_type);
    expect.check(!rejected_type && rejected_type.error().contains("type 必须是 openai"),
        "non-openai service reference is rejected");

    auto empty_key = services;
    empty_key.at("openai-primary") = openai_service("alpha", "   ");
    auto rejected_key = config::resolve_llm_directory(options_fixture(), empty_key);
    expect.check(!rejected_key && rejected_key.error().contains("api_key 不能为空"),
        "blank api_key is rejected at load time");

    auto disabled_without_key = services;
    disabled_without_key.at("deepseek-primary") =
        openai_service("deepseek", "", false);
    auto tolerated =
        config::resolve_llm_directory(options_fixture(), disabled_without_key);
    expect.check(tolerated.has_value() &&
            !tolerated->find_provider("deepseek")->usable(),
        "a disabled instance may leave its api_key empty");

    auto oversized_pool = services;
    oversized_pool.at("openai-primary").properties["pool_size"] = std::int64_t{4096};
    auto rejected_pool =
        config::resolve_llm_directory(options_fixture(), oversized_pool);
    expect.check(!rejected_pool && rejected_pool.error().contains("pool_size"),
        "pool_size above the cnetmod limit is rejected");
}

void check_target_resolution(checklist& expect)
{
    const auto directory =
        config::resolve_llm_directory(options_fixture(), services_fixture());
    if (!directory)
    {
        expect.check(false, "target fixture resolves");
        return;
    }
    const auto defaults = config::resolve_llm_target(*directory, "", "");
    expect.check(defaults &&
            *defaults == config::llm_target{.provider = "openai", .model = "gpt-4o-mini"},
        "empty request falls back to the configured defaults");

    const auto explicit_model = config::resolve_llm_target(*directory, "", "gpt-4o");
    expect.check(explicit_model && explicit_model->model == "gpt-4o",
        "explicit model on the default provider is honored");

    const auto unknown_provider = config::resolve_llm_target(*directory, "claude", "");
    expect.check(!unknown_provider &&
            unknown_provider.error() == config::llm_rejected_code,
        "unknown provider is ALGORITHM.FAILED");

    const auto unknown_model =
        config::resolve_llm_target(*directory, "openai", "gpt-5-unknown");
    expect.check(!unknown_model && unknown_model.error() == config::llm_rejected_code,
        "unknown model is ALGORITHM.FAILED");

    const auto disabled =
        config::resolve_llm_target(*directory, "deepseek", "deepseek-chat");
    expect.check(!disabled && disabled.error() == config::llm_unconfigured_code,
        "provider without enabled keys is DEPENDENCY.UNAVAILABLE");

    const auto empty = config::resolve_llm_target(config::llm_directory{}, "", "");
    expect.check(!empty && empty.error() == config::llm_unconfigured_code,
        "empty directory is DEPENDENCY.UNAVAILABLE");

    expect.check(config::llm_model_component("openai") == "llm:openai",
        "provider component naming is stable");
}

void check_composed_service(checklist& expect)
{
    auto directory = config::resolve_llm_directory(options_fixture(), services_fixture());
    if (!directory)
    {
        expect.check(false, "service fixture resolves");
        return;
    }
    auto openai = std::make_shared<scripted_model>();
    app_ai::composed_llm_service service{*directory, {{"openai", openai}}};
    expect.check(service.configured(), "service with a composed model is configured");

    auto completed = cnetmod::sync_wait(service.complete(request_for("", "")));
    expect.check(completed && completed->content == "第一段第二段" &&
            completed->tokens_in == 11 && completed->tokens_out == 7,
        "completion content and usage are translated back");
    expect.check(openai->requests.size() == 1 &&
            openai->requests[0].model == "gpt-4o-mini" &&
            openai->requests[0].messages.size() == 2 &&
            openai->requests[0].messages[0].role == "system",
        "request is translated with the resolved model and message roles");
    expect.check(!openai->requests.empty() &&
            openai->requests[0].to_json().contains("reasoning_effort"),
        "reasoning_effort is forwarded as an extra field");

    auto unknown = cnetmod::sync_wait(service.complete(request_for("claude", "")));
    expect.check(!unknown && unknown.error() == error_code::ai_request_failed &&
            openai->requests.size() == 1,
        "unknown provider is rejected before any upstream call");

    auto unconfigured =
        cnetmod::sync_wait(service.complete(request_for("deepseek", "deepseek-chat")));
    expect.check(!unconfigured && unconfigured.error() == error_code::upstream_unavailable,
        "provider without a composed model is DEPENDENCY.UNAVAILABLE");

    openai->failure = "all keys exhausted";
    auto failed = cnetmod::sync_wait(service.complete(request_for("", "")));
    expect.check(!failed && failed.error() == error_code::ai_request_failed,
        "upstream failure after the composed fallbacks is ALGORITHM.FAILED");
    openai->failure.reset();

    std::vector<std::string> received;
    const app_ai::llm_delta_sink collect =
        [&received](std::string_view delta) -> cnetmod::task<bool>
    {
        received.emplace_back(delta);
        co_return true;
    };
    auto streamed = cnetmod::sync_wait(service.stream(request_for("", ""), collect));
    expect.check(streamed && streamed->content == "第一段第二段" &&
            received == std::vector<std::string>{"第一段", "第二段"},
        "stream forwards every delta in order");

    std::vector<std::string> partial;
    const app_ai::llm_delta_sink disconnect =
        [&partial](std::string_view delta) -> cnetmod::task<bool>
    {
        partial.emplace_back(delta);
        co_return false;
    };
    auto truncated = cnetmod::sync_wait(service.stream(request_for("", ""), disconnect));
    expect.check(truncated && truncated->content == "第一段" && partial.size() == 1 &&
            openai->delivered == 1,
        "client disconnect stops the upstream stream and returns the partial text");

    app_ai::composed_llm_service empty{config::llm_directory{}, {}};
    expect.check(!empty.configured(), "empty directory is not configured");
}

void check_catalog(checklist& expect)
{
    const auto directory =
        config::resolve_llm_directory(options_fixture(), services_fixture());
    if (!directory)
    {
        expect.check(false, "catalog fixture resolves");
        return;
    }
    const auto snapshot = app_ai::llm_catalog(*directory);
    expect.check(snapshot.configured && snapshot.default_provider == "openai" &&
            snapshot.providers.size() == 2 && snapshot.providers[0].is_default &&
            !snapshot.providers[1].keys[0].configured,
        "catalog snapshot reflects defaults and key availability");

    const auto document = app_ai::llm_catalog_document(*directory);
    const auto json = cnetmod::json::write_document(document).value_or("");
    expect.check(json.contains("\"name\":\"openai\"") &&
            json.contains("\"models\":[\"gpt-4o-mini\",\"gpt-4o\"]"),
        "catalog json lists providers and models");
    expect.check(!json.contains("sk-test") && !json.contains("api.example.test") &&
            !json.contains("alpha") && !json.contains("openai-primary"),
        "catalog json never exposes keys, endpoints, instances or service names");

    const auto empty = cnetmod::json::write_document(
        app_ai::llm_catalog_document(config::llm_directory{})).value_or("");
    expect.check(empty.contains("\"configured\":false") &&
            empty.contains("\"providers\":[]"),
        "unconfigured catalog is an empty, non-error document");
}

void set_variable(const char* name, const char* value)
{
#if defined(_WIN32)
    (void)_putenv_s(name, value);
#else
    (void)setenv(name, value, 1);
#endif
}

/// @brief 真读 YAML：顶层 llm 小节 -> llm_options -> 校验 -> 解析到服务实例。
[[nodiscard]] auto load_directory(const std::filesystem::path& path)
    -> std::expected<config::llm_directory, std::string>
{
    auto configuration = cnetmod::application::load_configuration(path);
    if (!configuration)
        return std::unexpected{configuration.error().describe()};
    cnetmod::application::options_registry registry;
    registry.section<config::llm_options>(std::string{config::llm_section_name}, true)
        .validate(config::validate_llm_options);
    if (auto bound = registry.bind(*configuration); !bound)
        return std::unexpected{bound.error().describe()};
    return config::resolve_llm_directory(
        *registry.current<config::llm_options>(config::llm_section_name),
        configuration->services);
}

void check_end_to_end_configuration(checklist& expect)
{
    const auto directory = std::filesystem::temp_directory_path();
    const auto path = directory / "cnetboot_llm_settings_test.yaml";
    set_variable("CNETBOOT_TEST_LLM_KEY", "sk-test-not-a-real-key");
    {
        std::ofstream stream{path, std::ios::binary | std::ios::trunc};
        stream << "application:\n"
                  "  name: cnetboot\n"
                  "llm:\n"
                  "  default_provider: openai\n"
                  "  default_model: gpt-4o-mini\n"
                  "  providers:\n"
                  "    - name: openai\n"
                  "      type: openai\n"
                  "      models: [gpt-4o-mini, gpt-4o]\n"
                  "      keys:\n"
                  "        - name: primary\n"
                  "          service: openai-primary\n"
                  "        - name: secondary\n"
                  "          service: openai-secondary\n"
                  "services:\n"
                  "  openai-primary:\n"
                  "    type: openai\n"
                  "    instance: alpha\n"
                  "    enabled: true\n"
                  "    base_url: https://api.example.test/v1\n"
                  "    api_key: ${CNETBOOT_TEST_LLM_KEY}\n"
                  "    pool_size: 4\n"
                  "  openai-secondary:\n"
                  "    type: openai\n"
                  "    instance: beta\n"
                  "    enabled: true\n"
                  "    base_url: https://api2.example.test/v1\n"
                  "    api_key: ${CNETBOOT_TEST_LLM_KEY}\n"
                  "    pool_size: 2\n";
    }
    auto loaded = load_directory(path);
    expect.check(loaded.has_value(), loaded
            ? std::string{"yaml llm section decodes and resolves"}
            : std::format("yaml llm section decodes and resolves: {}", loaded.error()));
    if (loaded)
    {
        const auto* openai = loaded->find_provider("openai");
        expect.check(openai != nullptr && openai->keys.size() == 2 &&
                openai->keys[0].instance == "alpha" &&
                openai->keys[1].instance == "beta" && loaded->usable() &&
                loaded->resilience.attempts_per_key == 1,
            "yaml keys resolve to instances and resilience takes its defaults");
    }

    // 环境变量未注入：必须硬失败（不能把空 key 带进运行期）。
    const auto other = directory / "cnetboot_llm_settings_missing_env.yaml";
    {
        std::ofstream stream{other, std::ios::binary | std::ios::trunc};
        stream << "application:\n"
                  "  name: cnetboot\n"
                  "llm:\n"
                  "  default_provider: openai\n"
                  "  default_model: gpt-4o-mini\n"
                  "  providers:\n"
                  "    - name: openai\n"
                  "      models: [gpt-4o-mini]\n"
                  "      keys:\n"
                  "        - service: openai-primary\n"
                  "services:\n"
                  "  openai-primary:\n"
                  "    type: openai\n"
                  "    instance: alpha\n"
                  "    enabled: true\n"
                  "    base_url: https://api.example.test/v1\n"
                  "    api_key: ${CNETBOOT_TEST_LLM_KEY_ABSENT:-}\n";
    }
    expect.check(!load_directory(other).has_value(),
        "an absent api_key environment variable fails at load time");

    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(other, ignored);
}

} // namespace

auto main() -> int
{
    checklist expect;
    check_options_validation(expect);
    check_directory_resolution(expect);
    check_target_resolution(expect);
    check_composed_service(expect);
    check_catalog(expect);
    check_end_to_end_configuration(expect);
    return expect.report();
}
