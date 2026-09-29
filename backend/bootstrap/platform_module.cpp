/**
 * @brief platform_module：跨业务的基础能力（入口限流、请求体上限、XML
 * 语句集、审计）。
 */
module cnetnexus.bootstrap.modules;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetmod.orm;
import cnetmod.protocol.http;
import cnetmod.protocol.http.middleware.body_limit;
import cnetnexus.chart.mapper.chart_mapper_registry;
import cnetnexus.chat.mapper.chat_mapper;
import cnetnexus.framework.audit;
import cnetnexus.framework.audit.mysql;
import cnetnexus.framework.audit.record;
import cnetnexus.framework.error;
import cnetnexus.framework.web.error_response;
import cnetnexus.framework.web.rate_limit;
import cnetnexus.reading.mapper.reading_cache_mapper;

namespace cnetnexus::bootstrap {
namespace {

/// 请求体上限。cnetmod 提供但默认不装，唯一兜底是 32MB 的解析器常量；batch
/// 每条约 1.13ms 实算，32MB 可塞进 ~55 万条 ≈ 10 分钟同步
/// CPU，而请求超时是协作式的、对同步 循环无效，所以必须在入口拦下来。1MB
/// 覆盖现有全部契约（最大的是批量排盘）。
constexpr std::size_t request_body_limit{1024 * 1024};

/**
 * @brief Loads and validates the chart / chat / reading XML statement set.
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
  if (auto loaded = cnetnexus::chart::load_chart_mappers(*registry, directory);
      !loaded)
    throw std::runtime_error{loaded.error()};
  if (auto valid = cnetnexus::chat::validate_chat_mapper_registry(*registry);
      !valid)
    throw std::runtime_error{valid.error()};
  if (auto valid =
          cnetnexus::reading::validate_reading_cache_mapper_registry(*registry);
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
      .section<cnetnexus::framework::web::edge_options>(
          std::string{cnetnexus::framework::web::edge_section_name})
      .validate(cnetnexus::framework::web::validate_edge_options);
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
  cnetmod::application::add_repository<cnetnexus::framework::audit_log_record>(
      context.components, primary_source());
  context.components.singleton<cnetnexus::framework::audit_log_service>(
      [](cnetmod::application::component_resolver &resolver) {
        return std::make_shared<cnetnexus::framework::mysql_audit_log_service>(
            owned_repository<cnetnexus::framework::audit_log_record>(resolver));
      });
  return {};
}

auto platform_module::compose(
    cnetmod::application::composition_context &context)
    -> std::expected<void, std::string> {
  const auto edge = context.components
                        .get<cnetmod::application::options_monitor<
                            cnetnexus::framework::web::edge_options>>(
                            cnetnexus::framework::web::edge_section_name)
                        .current();
  // 未匹配路由的 404 / 405 也走统一错误格式（405 的 Allow 头由框架设置）。
  context.routes.on_unmatched(
      [](cnetmod::http::request_context &request,
         const cnetmod::http::unmatched_request &unmatched)
          -> cnetmod::task<void> {
        cnetnexus::framework::web::respond_error(
            request,
            unmatched.status == cnetmod::http::status::method_not_allowed
                ? cnetnexus::framework::error::error_code::
                      request_method_not_allowed
                : cnetnexus::framework::error::error_code::request_not_found);
        co_return;
      });
  // 限流最先执行：被拒绝的请求不读请求体、不做认证，也不触碰任何下游资源。
  context.middleware.push_back(
      cnetnexus::framework::web::rate_limit_middleware(*edge));
  context.middleware.push_back(cnetmod::body_limit(request_body_limit));
  return {};
}

} // namespace cnetnexus::bootstrap
