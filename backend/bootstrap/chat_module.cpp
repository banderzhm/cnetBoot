/**
 * @brief chat_module：会话持久化、对话服务与路由（全部需要令牌）。
 */
module cnetnexus.bootstrap.modules;

import std;
import cnetmod.application;
import cnetmod.orm;
import cnetmod.protocol.http;
import cnetnexus.chat.controller.chat_route_context;
import cnetnexus.chat.controller.chat_routes;
import cnetnexus.chat.mapper.chat_mapper;
import cnetnexus.chat.domain.chat_record;
import cnetnexus.chat.service.chat_service;
import cnetnexus.chat.service.chat_service_factory;
import cnetnexus.framework.audit;
import cnetnexus.framework.error;
import cnetnexus.reading.service.llm_service;

namespace cnetnexus::bootstrap {
namespace {

namespace application = cnetmod::application;
namespace chat = cnetnexus::chat;
using error_code = cnetnexus::framework::error::error_code;

class chat_service_resolver_fn
{
public:
  chat_service_resolver_fn(
      application::repository_factory<chat::chat_session_record>& sessions,
      application::repository_factory<chat::chat_message_record>& messages,
      std::shared_ptr<chat::chat_service_factory> services) noexcept
      : sessions_{&sessions}, messages_{&messages}, services_{std::move(services)}
  {
  }

  [[nodiscard]] auto operator()(
      const cnetmod::http::request_context& request) const
      -> std::expected<std::unique_ptr<chat::chat_service_scope>, error_code>
  {
    auto sessions = sessions_->for_request(request);
    auto messages = messages_->for_request(request);
    if (!sessions || !messages)
      return std::unexpected{error_code::system_internal_error};
    return services_->make(std::move(*sessions), std::move(*messages));
  }

private:
  application::repository_factory<chat::chat_session_record>* sessions_;
  application::repository_factory<chat::chat_message_record>* messages_;
  std::shared_ptr<chat::chat_service_factory> services_;
};

} // namespace

auto chat_module::name() const -> std::string_view { return "chat"; }

void chat_module::configure_options(application::options_registry &options) {
  options.section<chat::chat_options>("chat").validate(
      [](const chat::chat_options &value) -> std::expected<void, std::string> {
        if (value.history_limit == 0)
          return std::unexpected{"history_limit 必须大于 0"};
        if (value.token_budget < 256)
          return std::unexpected{"token_budget 不能小于 256"};
        if (value.max_tokens <= 0)
          return std::unexpected{"max_tokens 必须大于 0"};
        return {};
      });
}

auto chat_module::register_components(
    application::registration_context &context)
    -> std::expected<void, std::string> {
  const auto options = context.options.current<chat::chat_options>("chat");
  application::add_repository<chat::chat_session_record>(context.components,
                                                         soft_deleted_source());
  application::add_repository<chat::chat_message_record>(context.components,
                                                         soft_deleted_source());
  context.components.singleton<chat::chat_service_factory>(
      [options](application::component_resolver &resolver) {
        return std::make_shared<chat::chat_service_factory>(
            resolver.shared<cnetmod::orm::mapper_registry>(business_statements),
            resolver.get<cnetnexus::reading::llm_service>(),
            resolver.get<cnetnexus::framework::audit_log_service>(), *options);
      });
  return {};
}

auto chat_module::compose(application::composition_context &context)
    -> std::expected<void, std::string> {
  chat::declare_chat_routes(
      context.routes,
      chat_service_resolver_fn{
          context.components.get<application::repository_factory<
              chat::chat_session_record>>(),
          context.components.get<application::repository_factory<
              chat::chat_message_record>>(),
          context.components.shared<chat::chat_service_factory>()});
  return {};
}

} // namespace cnetnexus::bootstrap
