export module cnetboot.chat.service.chat_service_factory;

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

export namespace cnetboot::chat {

/** One request-bound chat service and its mapper implementation. */
struct chat_service_scope
{
    chat_service_scope(
        cnetmod::application::managed_repository<chat_session_record> sessions,
        cnetmod::application::managed_repository<chat_message_record> messages,
        std::shared_ptr<const cnetmod::orm::mapper_registry> registry,
        cnetboot::ai::llm_service& llm,
        cnetboot::framework::audit_log_service& audit,
        const chat_options& options);

    mysql_chat_mapper mapper;
    chat_service service;
};

/** Provides request-scoped chat services without exposing Mapper to controllers. */
class chat_service_factory
{
public:
    chat_service_factory(
        std::shared_ptr<const cnetmod::orm::mapper_registry> registry,
        cnetboot::ai::llm_service& llm,
        cnetboot::framework::audit_log_service& audit,
        chat_options options) noexcept;

    [[nodiscard]] auto make(
        cnetmod::application::managed_repository<chat_session_record> sessions,
        cnetmod::application::managed_repository<chat_message_record> messages) const
        -> std::unique_ptr<chat_service_scope>;

private:
    std::shared_ptr<const cnetmod::orm::mapper_registry> registry_;
    cnetboot::ai::llm_service* llm_;
    cnetboot::framework::audit_log_service* audit_;
    chat_options options_;
};

} // namespace cnetboot::chat
