module cnetboot.chat.service.chat_service_factory;

import std;
import cnetmod.application;
import cnetmod.orm;
import cnetboot.chat.domain.chat_domain;
import cnetboot.chat.domain.chat_record;
import cnetboot.chat.mapper.chat_mapper;
import cnetboot.chat.service.chat_service;
import cnetboot.framework.audit;
import cnetboot.framework.error;
import cnetboot.ai.service.llm_service;

namespace cnetboot::chat {

chat_service_scope::chat_service_scope(
    cnetmod::application::managed_repository<chat_session_record> sessions,
    cnetmod::application::managed_repository<chat_message_record> messages,
    std::shared_ptr<const cnetmod::orm::mapper_registry> registry,
    cnetboot::ai::llm_service& llm,
    cnetboot::framework::audit_log_service& audit,
    const chat_options& options)
    : mapper{std::move(sessions), std::move(messages), std::move(registry)},
      service{mapper, llm, &audit}
{
    service.set_options(options);
}

chat_service_factory::chat_service_factory(
    std::shared_ptr<const cnetmod::orm::mapper_registry> registry,
    cnetboot::ai::llm_service& llm,
    cnetboot::framework::audit_log_service& audit,
    chat_options options) noexcept
    : registry_{std::move(registry)}, llm_{&llm}, audit_{&audit},
      options_{std::move(options)}
{
}

auto chat_service_factory::make(
    cnetmod::application::managed_repository<chat_session_record> sessions,
    cnetmod::application::managed_repository<chat_message_record> messages) const
    -> std::unique_ptr<chat_service_scope>
{
    return std::make_unique<chat_service_scope>(std::move(sessions),
        std::move(messages), registry_, *llm_, *audit_, options_);
}

} // namespace cnetboot::chat
