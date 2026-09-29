/**
 * @brief platform_module：跨业务的基础能力（入口限流、请求体上限、XML
 * 语句集、审计）。
 */
module cnetboot.bootstrap.modules;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetmod.orm;
import cnetmod.protocol.http;
import cnetmod.protocol.http.middleware.body_limit;
import cnetboot.chat.mapper.chat_mapper;
import cnetboot.framework.audit;
import cnetboot.framework.audit.mysql;
import cnetboot.framework.audit.record;
import cnetboot.framework.error;
import cnetboot.framework.web.error_response;
import cnetboot.framework.web.rate_limit;

namespace cnetboot::bootstrap {
namespace {

/// 请求体上限。cnetmod 提供但默认不安装，基础模板在入口统一限制。
constexpr std::size_t request_body_limit{1024 * 1024};

/**
 * @brief Loads and validates the chat XML statement set.
 *
 * load_directory() loads every XML file in the directory, and CMake copies
 * ChartMapper.xml, ChatMapper.xml and ReadingCacheMapper.xml into the same
 * `mapper/` folder, so all three validation contracts apply to one registry
 * and the three mapper families observe exactly the same statement set.
 */
[[nodiscard]] auto validate_platform_options(const platform_options &options)
    -> std::expected<void, std::string> {
  if (options.mapper_directory.find_first_not_of(" \t\r\n") ==
      std::string::npos)
    return std::unexpected{"mapper_directory 不能为空"};
  return {};
}

[[nodiscard]] auto load_business_statements(std::string directory)
    -> std::shared_ptr<cnetmod::orm::mapper_registry> {
  auto registry = std::make_shared<cnetmod::orm::mapper_registry>();
  if (auto loaded = registry->load_directory(directory); !loaded)
    throw std::runtime_error{loaded.error()};
  if (auto valid = cnetboot::chat::validate_chat_mapper_registry(*registry);
      !valid)
    throw std::runtime_error{valid.error()};
  return registry;
}

} // namespace

auto platform_module::name() const -> std::string_view { return "platform"; }

void platform_module::configure_options(
    cnetmod::application::options_registry &options) {
  options.section<platform_options>("platform")
      .validate(validate_platform_options);
  options
      .section<cnetboot::framework::web::edge_options>(
          std::string{cnetboot::framework::web::edge_section_name})
      .validate(cnetboot::framework::web::validate_edge_options);
}

auto platform_module::register_components(
    cnetmod::application::registration_context &context)
    -> std::expected<void, std::string> {
  const auto options = context.options.current<platform_options>("platform");
  context.components.singleton<cnetmod::orm::mapper_registry>(
      std::string{business_statements},
      [directory = options->mapper_directory](
          cnetmod::application::component_resolver &) {
        return load_business_statements(directory);
      });
  cnetmod::application::add_repository<cnetboot::framework::audit_log_record>(
      context.components, primary_source());
  context.components.singleton<cnetboot::framework::audit_log_service>(
      [](cnetmod::application::component_resolver &resolver) {
        return std::make_shared<cnetboot::framework::mysql_audit_log_service>(
            owned_repository<cnetboot::framework::audit_log_record>(resolver));
      });
  return {};
}

auto platform_module::compose(
    cnetmod::application::composition_context &context)
    -> std::expected<void, std::string> {
  const auto edge = context.components
                        .get<cnetmod::application::options_monitor<
                            cnetboot::framework::web::edge_options>>(
                            cnetboot::framework::web::edge_section_name)
                        .current();
  // 未匹配路由的 404 / 405 也走统一错误格式（405 的 Allow 头由框架设置）。
  context.routes.on_unmatched(
      [](cnetmod::http::request_context &request,
         const cnetmod::http::unmatched_request &unmatched)
          -> cnetmod::task<void> {
        cnetboot::framework::web::respond_error(
            request,
            unmatched.status == cnetmod::http::status::method_not_allowed
                ? cnetboot::framework::error::error_code::
                      request_method_not_allowed
                : cnetboot::framework::error::error_code::request_not_found);
        co_return;
      });
  // 限流最先执行：被拒绝的请求不读请求体、不做认证，也不触碰任何下游资源。
  context.middleware.push_back(
      cnetboot::framework::web::rate_limit_middleware(*edge));
  context.middleware.push_back(cnetmod::body_limit(request_body_limit));
  return {};
}

} // namespace cnetboot::bootstrap
