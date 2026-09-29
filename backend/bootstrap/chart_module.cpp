/**
 * @brief chart_module：六术数、存档、批量、案例与统计。
 *
 * 路由声明顺序即匹配顺序，通配路由晚于同前缀的字面量路由（例如
 * /api/v1/archive/search 早于 /api/v1/archive/:algorithm/:id）。
 * 公开与受保护在各 controller 的注册处用端点元数据声明：
 *  - catalog 与六术数路由是已发布契约，anonymous()；
 *  - reading 端点与存档读取 optional_identity()：匿名按用户 0
 * 分区，带有效令牌时绑定用户， 带无效令牌时 401（不静默降级为匿名）；
 *  - batch、cases、tags、stats、archive 检索与删除需要令牌与权限码。
 */
module cnetnexus.bootstrap.modules;

import std;
import cnetmod.application;
import cnetmod.core.log;
import cnetmod.coro.task;
import cnetmod.orm;
import cnetmod.protocol.http;
import cnetmod.protocol.redis;
import cnetnexus.chart.controller.archive_routes;
import cnetnexus.chart.controller.batch_routes;
import cnetnexus.chart.controller.bazi_routes;
import cnetnexus.chart.controller.case_routes;
import cnetnexus.chart.controller.catalog_routes;
import cnetnexus.chart.controller.da_liu_ren_routes;
import cnetnexus.chart.controller.liu_yao_routes;
import cnetnexus.chart.controller.mei_hua_routes;
import cnetnexus.chart.controller.qi_men_routes;
import cnetnexus.chart.controller.reading_routes;
import cnetnexus.chart.controller.stats_routes;
import cnetnexus.chart.controller.zi_wei_routes;
import cnetnexus.chart.mapper.mysql_chart_case_mapper;
import cnetnexus.chart.service.bazi_service;
import cnetnexus.chart.service.chart_generation_service;
import cnetnexus.chart.service.chart_reading_service;
import cnetnexus.chart.service.case_service;
import cnetnexus.chart.service.case_service_factory;
import cnetnexus.chart.service.chart_archive_service;
import cnetnexus.chart.service.compute_executor;
import cnetnexus.chart.service.da_liu_ren_service;
import cnetnexus.chart.service.liu_yao_service;
import cnetnexus.chart.service.mei_hua_service;
import cnetnexus.chart.service.qi_men_service;
import cnetnexus.chart.service.stats_service;
import cnetnexus.chart.mapper.user_count_mapper;
import cnetnexus.chart.service.zi_wei_service;
import cnetnexus.chart.service.xuanxue.bazi_adapter;
import cnetnexus.chart.service.xuanxue.da_liu_ren_adapter;
import cnetnexus.chart.service.xuanxue.liu_yao_adapter;
import cnetnexus.chart.service.xuanxue.mei_hua_adapter;
import cnetnexus.chart.service.xuanxue.qi_men_adapter;
import cnetnexus.chart.service.xuanxue.zi_wei_adapter;
import cnetnexus.framework.audit;
import cnetnexus.framework.error;
import cnetnexus.reading.service.reading_use_case;
import cnetnexus.system.domain.sys_user;

namespace cnetnexus::bootstrap {
namespace {

namespace application = cnetmod::application;
namespace chart = cnetnexus::chart;
namespace reading = cnetnexus::reading;
using error_code = cnetnexus::framework::error::error_code;

/**
 * @brief Registered-user counter for GET /api/v1/stats/overview.
 *
 * sys_user belongs to the system module, so the query lives in the
 * composition root: a business mapper contract must not import another
 * module's storage. A failed query propagates as an error so the overview
 * reports SYSTEM.INTERNAL_ERROR instead of half-filled data.
 */
class sys_user_count_mapper final : public chart::user_count_mapper {
public:
  explicit sys_user_count_mapper(
      application::managed_repository<cnetnexus::system::sys_user> users)
      : users_{std::move(users)} {}

  [[nodiscard]] auto count()
      -> cnetmod::task<std::expected<std::uint64_t, error_code>> override {
    auto result = co_await users_.count();
    if (!result.ok() || result.data.empty())
      co_return std::unexpected{error_code::system_internal_error};
    co_return static_cast<std::uint64_t>(result.data.front());
  }

private:
  application::managed_repository<cnetnexus::system::sys_user> users_;
};

/**
 * @brief 排盘缓存模板；calculation-cache
 * 是可选依赖，缺失时退化为不缓存并记日志。
 */
[[nodiscard]] auto
calculation_cache_template(application::component_resolver &resolver)
    -> std::optional<cnetmod::redis::redis_template> {
  auto *cache = resolver.find<application::redis_service>(calculation_cache);
  if (cache == nullptr) {
    logger::warn{
        "bazi calculation cache '{}' is not configured; caching disabled",
        calculation_cache};
    return std::nullopt;
  }
  return cache->make_template(
      cnetmod::redis::template_options{.ns = {.prefix = "cnetnexus:bazi:"}});
}

/**
 * @brief 生产计算执行器：排盘交给 Application 的 CPU
 * 池，完成后回到请求所在的事件循环。
 */
class runtime_compute_executor final : public chart::compute_executor {
public:
  explicit runtime_compute_executor(
      application::application_runtime &runtime) noexcept
      : runtime_{&runtime} {}

  [[nodiscard]] auto execute(std::function<void()> work)
      -> cnetmod::task<void> override {
    co_await runtime_->offload(std::move(work));
  }

private:
  application::application_runtime *runtime_;
};

/** Resolves one data-permission-bound case service for a request. */
class case_service_resolver_fn
{
public:
  case_service_resolver_fn(
      application::repository_factory<chart::chart_case_storage_record>& cases,
      application::repository_factory<chart::chart_tag_storage_record>& tags,
      std::shared_ptr<chart::case_service_factory> services) noexcept
      : cases_{&cases}, tags_{&tags}, services_{std::move(services)}
  {
  }

  [[nodiscard]] auto operator()(
      const cnetmod::http::request_context& request) const
      -> std::expected<std::unique_ptr<chart::case_service_scope>, error_code>
  {
    auto cases = cases_->for_request(request);
    auto tags = tags_->for_request(request);
    if (!cases || !tags)
      return std::unexpected{error_code::system_internal_error};
    return services_->make(std::move(*cases), std::move(*tags));
  }

private:
  application::repository_factory<chart::chart_case_storage_record>* cases_;
  application::repository_factory<chart::chart_tag_storage_record>* tags_;
  std::shared_ptr<chart::case_service_factory> services_;
};

/// @brief 注册一个无依赖的算法适配器（ZhouYiLab）。
template <class Adapter>
void add_adapter(application::component_collection &components) {
  components.singleton<Adapter>([](application::component_resolver &) {
    return std::make_shared<Adapter>();
  });
}

/// @brief 注册一个只依赖自身算法适配器的术数服务。
template <class Service, class Adapter>
void add_algorithm_service(application::component_collection &components) {
  components.singleton<Service>([](application::component_resolver &resolver) {
    return std::make_shared<Service>(resolver.get<Adapter>(),
                                     resolver.get<chart::compute_executor>());
  });
}

} // namespace

auto chart_module::name() const -> std::string_view { return "chart"; }

void chart_module::configure_options(application::options_registry &options) {
  (void)options;
}

auto chart_module::register_components(
    application::registration_context &context)
    -> std::expected<void, std::string> {
  auto &components = context.components;
  components.singleton<chart::compute_executor>(
      [](application::component_resolver &resolver) {
        return std::make_shared<runtime_compute_executor>(
            resolver.get<application::application_runtime>());
      });
  add_adapter<chart::bazi_adapter>(components);
  add_adapter<chart::liu_yao_adapter>(components);
  add_adapter<chart::mei_hua_adapter>(components);
  add_adapter<chart::zi_wei_adapter>(components);
  add_adapter<chart::da_liu_ren_adapter>(components);
  add_adapter<chart::qi_men_adapter>(components);

  application::add_repository<chart::chart_case_storage_record>(
      components, soft_deleted_source());
  application::add_repository<chart::chart_tag_storage_record>(
      components, primary_source());
  application::add_repository<chart::chart_archive_storage_record>(
      components, primary_source());

  components.singleton<chart::mysql_chart_mappers>(
      [](application::component_resolver &resolver) {
        return std::make_shared<chart::mysql_chart_mappers>(
            owned_repository<chart::chart_case_storage_record>(resolver),
            owned_repository<chart::chart_tag_storage_record>(resolver),
            owned_repository<chart::chart_archive_storage_record>(resolver),
            resolver.get<cnetmod::orm::mapper_registry>(business_statements));
      });
  components.singleton<chart::user_count_mapper>(
      [](application::component_resolver &resolver) {
        return std::make_shared<sys_user_count_mapper>(
            owned_repository<cnetnexus::system::sys_user>(resolver));
      });

  components.singleton<chart::bazi_service>(
      [](application::component_resolver &resolver) {
        return std::make_shared<chart::bazi_service>(
            resolver.get<chart::bazi_adapter>(),
            resolver.get<chart::compute_executor>(),
            calculation_cache_template(resolver));
      });
  add_algorithm_service<chart::liu_yao_service, chart::liu_yao_adapter>(
      components);
  add_algorithm_service<chart::mei_hua_service, chart::mei_hua_adapter>(
      components);
  add_algorithm_service<chart::zi_wei_service, chart::zi_wei_adapter>(
      components);
  add_algorithm_service<chart::da_liu_ren_service, chart::da_liu_ren_adapter>(
      components);
  add_algorithm_service<chart::qi_men_service, chart::qi_men_adapter>(
      components);

  components.singleton<chart::chart_archive_service>(
      [](application::component_resolver &resolver) {
        return std::make_shared<chart::chart_archive_service>(
            resolver.get<chart::mysql_chart_mappers>().archives);
      });
  components.singleton<chart::chart_generation_service>(
      [](application::component_resolver &resolver) {
        return std::make_shared<chart::chart_generation_service>(
            resolver.get<chart::bazi_service>(),
            resolver.get<chart::liu_yao_service>(),
            resolver.get<chart::mei_hua_service>(),
            resolver.get<chart::zi_wei_service>(),
            resolver.get<chart::da_liu_ren_service>(),
            resolver.get<chart::qi_men_service>(),
            resolver.get<chart::chart_archive_service>());
      });
  components.singleton<chart::chart_reading_service>(
      [](application::component_resolver &resolver) {
        return std::make_shared<chart::chart_reading_service>(
            resolver.get<chart::bazi_service>(),
            resolver.get<chart::liu_yao_service>(),
            resolver.get<chart::mei_hua_service>(),
            resolver.get<chart::zi_wei_service>(),
            resolver.get<chart::da_liu_ren_service>(),
            resolver.get<chart::qi_men_service>());
      });
  components.singleton<chart::case_service_factory>(
      [](application::component_resolver &resolver) {
        return std::make_shared<chart::case_service_factory>(
            resolver.get<cnetmod::orm::mapper_registry>(business_statements),
            resolver.get<chart::chart_archive_service>(),
            resolver.get<cnetnexus::framework::audit_log_service>());
      });
  components.singleton<chart::stats_service>(
      [](application::component_resolver &resolver) {
        auto &mappers = resolver.get<chart::mysql_chart_mappers>();
        return std::make_shared<chart::stats_service>(
            resolver.get<chart::user_count_mapper>(), mappers.cases,
            mappers.archives);
      });
  return {};
}

auto chart_module::compose(application::composition_context &context)
    -> std::expected<void, std::string> {
  const auto &components = context.components;
  auto &routes = context.routes;
  auto &bazi = components.get<chart::bazi_service>();
  auto &liu_yao = components.get<chart::liu_yao_service>();
  auto &mei_hua = components.get<chart::mei_hua_service>();
  auto &zi_wei = components.get<chart::zi_wei_service>();
  auto &da_liu_ren = components.get<chart::da_liu_ren_service>();
  auto &qi_men = components.get<chart::qi_men_service>();
  auto &archive = components.get<chart::chart_archive_service>();
  auto &reading = components.get<reading::reading_use_case>();

  chart::declare_catalog_routes(routes);
  chart::declare_bazi_routes(routes, bazi);
  chart::declare_liu_yao_routes(routes, liu_yao);
  chart::declare_mei_hua_routes(routes, mei_hua);
  chart::declare_zi_wei_routes(routes, zi_wei);
  chart::declare_da_liu_ren_routes(routes, da_liu_ren);
  chart::declare_qi_men_routes(routes, qi_men);
  chart::declare_archive_routes(
      routes, components.get<chart::chart_generation_service>(), archive);
  chart::declare_batch_routes(
      routes, components.get<chart::chart_generation_service>());
  chart::declare_reading_routes(
      routes, components.get<chart::chart_reading_service>(), reading);
  chart::declare_case_routes(
      routes,
      case_service_resolver_fn{
          components.get<application::repository_factory<
              chart::chart_case_storage_record>>(),
          components.get<application::repository_factory<
              chart::chart_tag_storage_record>>(),
          components.shared<chart::case_service_factory>()},
      reading);
  chart::declare_stats_routes(routes, components.get<chart::stats_service>());
  return {};
}

} // namespace cnetnexus::bootstrap
