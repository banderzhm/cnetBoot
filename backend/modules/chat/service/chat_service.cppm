
export module cnetboot.chat.service.chat_service;

import std;
import cnetmod.coro.task;
import cnetboot.chat.domain.chat_domain;
import cnetboot.chat.mapper.chat_mapper;
import cnetboot.framework.audit;
import cnetboot.framework.error;
import cnetboot.framework.pagination;
import cnetboot.ai.domain.llm_contract;
import cnetboot.ai.service.llm_service;

export namespace cnetboot::chat {

namespace pagination = cnetboot::framework::pagination;
namespace error = cnetboot::framework::error;


inline constexpr std::size_t max_session_title_length = 128;


inline constexpr std::string_view default_session_title = "New conversation";


inline constexpr std::size_t max_model_length = 64;


struct chat_session_create_request
{
    std::string category;
    std::string title;
    std::string system_prompt;
    std::string model;
    double temperature{cnetboot::chat::default_temperature};
    bool has_temperature{false};
};


struct chat_session_patch
{
    bool has_title{false};
    bool has_system_prompt{false};
    bool has_model{false};
    bool has_temperature{false};
    std::string title;
    std::string system_prompt;
    std::string model;
    double temperature{cnetboot::chat::default_temperature};
};


struct chat_message_request
{
    std::string content;


    std::string reference_context;
};


struct chat_turn
{
    cnetboot::chat::chat_message user_message;
    cnetboot::chat::chat_message assistant_message;
};


struct chat_options
{
    std::size_t history_limit{cnetboot::chat::default_history_limit};
    std::size_t token_budget{cnetboot::chat::default_token_budget};
    std::string system_prompt;
    std::string provider;
    std::string model;
    std::string reasoning_effort;
    int max_tokens{1024};
};


using chat_delta_sink = cnetboot::ai::llm_delta_sink;


struct chat_context_assembly
{
    std::vector<cnetboot::ai::llm_message> messages;
    cnetboot::chat::context_trim_result trim;
};


class chat_service
{
public:
    chat_service(mysql_chat_mapper& mapper, cnetboot::ai::llm_service& llm,
        cnetboot::framework::audit_log_service* audit) noexcept
        : mapper_{&mapper},
          llm_{&llm},
          audit_{audit}
    {
    }

    chat_service(const chat_service&) = delete;
    chat_service(chat_service&&) = delete;


    void set_options(chat_options options);


    [[nodiscard]] auto options() const noexcept -> const chat_options&
    {
        return options_;
    }


    [[nodiscard]] auto create_session(const chat_session_create_request& request,
        const cnetboot::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<cnetboot::chat::chat_session, error::error_code>>;


    [[nodiscard]] auto list_sessions(const cnetboot::chat::chat_session_query& query)
        -> cnetmod::task<std::expected<pagination::page_result<cnetboot::chat::chat_session>,
            error::error_code>>;


    [[nodiscard]] auto find_session(std::uint64_t id)
        -> cnetmod::task<std::expected<cnetboot::chat::chat_session, error::error_code>>;


    [[nodiscard]] auto update_session(std::uint64_t id, const chat_session_patch& patch,
        const cnetboot::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<cnetboot::chat::chat_session, error::error_code>>;


    [[nodiscard]] auto remove_session(std::uint64_t id,
        const cnetboot::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<void, error::error_code>>;


    [[nodiscard]] auto list_messages(std::uint64_t session_id, const pagination::page_request& page)
        -> cnetmod::task<std::expected<pagination::page_result<cnetboot::chat::chat_message>,
            error::error_code>>;


    [[nodiscard]] auto send_message(std::uint64_t session_id, const chat_message_request& request,
        const cnetboot::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<chat_turn, error::error_code>>;


    [[nodiscard]] auto stream_message(std::uint64_t session_id, const chat_message_request& request,
        const cnetboot::framework::audit_actor& actor, const chat_delta_sink& sink)
        -> cnetmod::task<std::expected<chat_turn, error::error_code>>;


    [[nodiscard]] auto assemble_context(const cnetboot::chat::chat_session& session,
        std::span<const cnetboot::chat::chat_message> history, std::string_view reference_context,
        std::string_view pending_user_content) const -> chat_context_assembly;

private:

    struct turn_plan
    {
        cnetboot::chat::chat_session session;
        cnetboot::chat::chat_message user_message;
        cnetboot::ai::llm_request request;
    };


    [[nodiscard]] auto prepare_turn(std::uint64_t session_id, const chat_message_request& request,
        const cnetboot::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<turn_plan, error::error_code>>;


    [[nodiscard]] auto finish_turn(const turn_plan& plan, std::string content, int tokens_in,
        int tokens_out) -> cnetmod::task<std::expected<chat_turn, error::error_code>>;


    [[nodiscard]] auto build_llm_request(const cnetboot::chat::chat_session& session,
        const chat_message_request& request) const -> cnetmod::task<cnetboot::ai::llm_request>;


    [[nodiscard]] auto refresh_title(const cnetboot::chat::chat_session& session,
        std::string_view content, const cnetboot::framework::audit_actor& actor)
        -> cnetmod::task<std::expected<cnetboot::chat::chat_session, error::error_code>>;

    mysql_chat_mapper* mapper_;
    cnetboot::ai::llm_service* llm_;
    cnetboot::framework::audit_log_service* audit_{nullptr};
    chat_options options_{};
};

} // namespace cnetboot::chat
