/**
 * @brief reading_module：llm 配置小节、多供应商补全实现、解读服务与 LLM
 * 目录路由。
 *
 * llm 小节在 options 阶段解码与结构校验；registration 阶段把 key 引用解析到
 * services.<name> 实例（缺服务、空 api_key、pool_size
 * 越界都在这里让构建失败）， 再为每个可用供应商注册一个 cnetmod
 * 组合模型。未配置 LLM 时补全实现仍然存在， 只是 configured()=false，用例返回
 * DEPENDENCY.UNAVAILABLE。
 */
module cnetnexus.bootstrap.modules;

import std;
import cnetmod.application;
import cnetmod.core.log;
import cnetmod.orm;
import cnetnexus.framework.config.llm_settings;
import cnetnexus.reading.controller.llm_routes;
import cnetnexus.reading.mapper.reading_cache_mapper;
import cnetnexus.reading.domain.reading_cache_record;
import cnetnexus.reading.service.llm_service;
import cnetnexus.reading.service.model.composed_llm_service;
import cnetnexus.reading.service.reading_cache_service;
import cnetnexus.reading.service.reading_service;
import cnetnexus.reading.service.reading_use_case;

namespace cnetnexus::bootstrap {
namespace {

namespace application = cnetmod::application;
namespace config = cnetnexus::framework::config;
namespace reading = cnetnexus::reading;

} // namespace

auto reading_module::name() const -> std::string_view { return "reading"; }

void reading_module::configure_options(application::options_registry &options) {
  options.section<config::llm_options>(std::string{config::llm_section_name})
      .validate(config::validate_llm_options);
}

auto reading_module::register_components(
    application::registration_context &context)
    -> std::expected<void, std::string> {
  const auto options =
      context.options.current<config::llm_options>(config::llm_section_name);
  auto directory =
      config::resolve_llm_directory(*options, context.configuration.services);
  if (!directory)
    return std::unexpected{std::move(directory.error())};

  reading::add_llm_models(context.components, *directory);
  auto snapshot =
      std::make_shared<config::llm_directory>(std::move(*directory));
  context.components.instance(snapshot);
  context.components.singleton<reading::llm_service>(
      [snapshot](application::component_resolver &resolver)
          -> std::shared_ptr<reading::llm_service> {
        auto service = reading::make_llm_service(resolver, *snapshot);
        logger::info{"llm configured={} providers={} default={}",
                     service->configured(), snapshot->providers.size(),
                     snapshot->default_provider};
        return service;
      });

  application::add_repository<reading::reading_cache_record>(context.components,
                                                             primary_source());
  context.components.singleton<reading::mysql_reading_cache_mapper>(
      [](application::component_resolver &resolver) {
        return std::make_shared<reading::mysql_reading_cache_mapper>(
            owned_repository<reading::reading_cache_record>(resolver),
            resolver.shared<cnetmod::orm::mapper_registry>(
                business_statements));
      });
  context.components.singleton<reading::reading_cache_service>(
      [](application::component_resolver &resolver) {
        return std::make_shared<reading::reading_cache_service>(
            resolver.get<reading::mysql_reading_cache_mapper>());
      });
  context.components.singleton<reading::reading_service>(
      [](application::component_resolver &resolver) {
        return std::make_shared<reading::reading_service>(
            resolver.get<reading::llm_service>());
      });
  context.components.singleton<reading::reading_use_case>(
      [](application::component_resolver &resolver) {
        return std::make_shared<reading::reading_use_case>(
            resolver.get<reading::reading_service>(),
            resolver.get<reading::reading_cache_service>());
      });
  return {};
}

auto reading_module::compose(application::composition_context &context)
    -> std::expected<void, std::string> {
  reading::declare_llm_routes(
      context.routes, context.components.shared<config::llm_directory>());
  return {};
}

} // namespace cnetnexus::bootstrap
