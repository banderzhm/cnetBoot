export module cnetnexus.chat.service.chat_service_factory;

import std;
import cnetmod.application;
import cnetmod.orm;
import cnetnexus.chat.domain.chat_domain;
import cnetnexus.chat.domain.chat_record;
import cnetnexus.chat.mapper.chat_mapper;
import cnetnexus.chat.service.chat_service;
import cnetnexus.framework.audit;
import cnetnexus.framework.error;
import cnetnexus.reading.service.llm_service;

export namespace cnetnexus::chat {

/** One request-bound chat service and its mapper implementation. */
struct chat_service_scope
{
    chat_service_scope(
        cnetmod::application::managed_repository<chat_session_record> sessions,
        cnetmod::application::managed_repository<chat_message_record> messages,
        std::shared_ptr<const cnetmod::orm::mapper_registry> registry,
        cnetnexus::reading::llm_service& llm,
        cnetnexus::framework::audit_log_service& audit,
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
        cnetnexus::reading::llm_service& llm,
        cnetnexus::framework::audit_log_service& audit,
        chat_options options) noexcept;

    [[nodiscard]] auto make(
        cnetmod::application::managed_repository<chat_session_record> sessions,
        cnetmod::application::managed_repository<chat_message_record> messages) const
        -> std::unique_ptr<chat_service_scope>;

private:
    std::shared_ptr<const cnetmod::orm::mapper_registry> registry_;
    cnetnexus::reading::llm_service* llm_;
    cnetnexus::framework::audit_log_service* audit_;
    chat_options options_;
};

} // namespace cnetnexus::chat
