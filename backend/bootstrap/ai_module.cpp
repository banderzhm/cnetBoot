/**
 * @brief ai_module：显式注册多供应商模型、补全服务与模型目录路由。
 *
 * llm 小节在 options 阶段解码与结构校验；registration 阶段把 key 引用解析到
 * services.<name> 实例（缺服务、空 api_key、pool_size
 * 越界都在这里让构建失败）， 再为每个可用供应商注册一个 cnetmod
 * 组合模型。未配置 LLM 时补全实现仍然存在， 只是 configured()=false，用例返回
 * DEPENDENCY.UNAVAILABLE。
 */
module cnetboot.bootstrap.modules;

import std;
import cnetmod.application;
import cnetmod.core.log;
import cnetboot.framework.config.llm_settings;
import cnetboot.ai.controller.llm_routes;
import cnetboot.ai.service.llm_service;
import cnetboot.ai.service.model.composed_llm_service;

namespace cnetboot::bootstrap {
namespace {

namespace application = cnetmod::application;
namespace config = cnetboot::framework::config;
namespace ai = cnetboot::ai;

} // namespace

auto ai_module::name() const -> std::string_view { return "ai"; }

void ai_module::configure_options(application::options_registry &options) {
  options.section<config::llm_options>(std::string{config::llm_section_name})
      .validate(config::validate_llm_options);
}

auto ai_module::register_components(
    application::registration_context &context)
    -> std::expected<void, std::string> {
  const auto options =
      context.options.current<config::llm_options>(config::llm_section_name);
  auto directory =
      config::resolve_llm_directory(*options, context.configuration.services);
  if (!directory)
    return std::unexpected{std::move(directory.error())};

  ai::add_llm_models(context.components, *directory);
  auto snapshot =
      std::make_shared<config::llm_directory>(std::move(*directory));
  context.components.instance(snapshot);
  context.components.singleton<ai::llm_service>(
      [snapshot](application::component_resolver &resolver)
          -> std::shared_ptr<ai::llm_service> {
        auto service = ai::make_llm_service(resolver, *snapshot);
        logger::info{"llm configured={} providers={} default={}",
                     service->configured(), snapshot->providers.size(),
                     snapshot->default_provider};
        return service;
      });

  return {};
}

auto ai_module::compose(application::composition_context &context)
    -> std::expected<void, std::string> {
  ai::declare_llm_routes(
      context.routes, context.components.shared<config::llm_directory>());
  return {};
}

} // namespace cnetboot::bootstrap
