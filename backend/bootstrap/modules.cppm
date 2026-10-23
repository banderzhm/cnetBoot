
export module cnetboot.bootstrap.modules;

import std;
import cnetmod.application;
import cnetmod.orm;
import cnetboot.system.service.login_guard;

export namespace cnetboot::bootstrap {


inline constexpr std::string_view primary_data_source{"primary"};


inline constexpr std::string_view session_cache{"primary"};


inline constexpr std::string_view business_statements{"business"};


struct platform_options
{
    std::string mapper_directory{"mapper"};
};


[[nodiscard]] inline auto primary_source() -> cnetmod::application::repository_options
{
    return cnetmod::application::repository_options{
        .instance = std::string{primary_data_source}};
}


[[nodiscard]] inline auto soft_deleted_source()
    -> cnetmod::application::repository_options
{
    cnetmod::orm::logical_delete_config deleted_at;
    deleted_at.field_name = "deleted_at";
    deleted_at.mode = cnetmod::orm::logical_delete_mode::nullable_datetime;
    auto options = primary_source();
    options.policies.logical_delete_policy = std::move(deleted_at);
    return options;
}


template <class T>
[[nodiscard]] auto owned_repository(cnetmod::application::component_resolver& resolver)
    -> cnetmod::application::managed_repository<T>
{
    auto made =
        resolver.get<cnetmod::application::repository_factory<T>>().make_repository();
    if (!made)
        throw std::system_error(made.error(), "repository handle cannot be created");
    return std::move(*made);
}


class platform_module final : public cnetmod::application::application_module
{
public:
    [[nodiscard]] auto name() const -> std::string_view override;
    void configure_options(cnetmod::application::options_registry& options) override;
    [[nodiscard]] auto register_components(
        cnetmod::application::registration_context& context)
        -> std::expected<void, std::string> override;
    [[nodiscard]] auto compose(cnetmod::application::composition_context& context)
        -> std::expected<void, std::string> override;
};


struct identity_options
{

    cnetboot::system::login_protection_options login;

    std::int64_t principal_cache_seconds{30};
};


class identity_module final : public cnetmod::application::application_module
{
public:
    [[nodiscard]] auto name() const -> std::string_view override;
    void configure_options(cnetmod::application::options_registry& options) override;
    [[nodiscard]] auto register_components(
        cnetmod::application::registration_context& context)
        -> std::expected<void, std::string> override;
    [[nodiscard]] auto compose(cnetmod::application::composition_context& context)
        -> std::expected<void, std::string> override;
};


class ai_module final : public cnetmod::application::application_module
{
public:
    [[nodiscard]] auto name() const -> std::string_view override;
    void configure_options(cnetmod::application::options_registry& options) override;
    [[nodiscard]] auto register_components(
        cnetmod::application::registration_context& context)
        -> std::expected<void, std::string> override;
    [[nodiscard]] auto compose(cnetmod::application::composition_context& context)
        -> std::expected<void, std::string> override;
};


class chat_module final : public cnetmod::application::application_module
{
public:
    [[nodiscard]] auto name() const -> std::string_view override;
    void configure_options(cnetmod::application::options_registry& options) override;
    [[nodiscard]] auto register_components(
        cnetmod::application::registration_context& context)
        -> std::expected<void, std::string> override;
    [[nodiscard]] auto compose(cnetmod::application::composition_context& context)
        -> std::expected<void, std::string> override;
};

} // namespace cnetboot::bootstrap
