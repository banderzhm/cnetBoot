
export module cnetboot.chat.domain.chat_domain;

import std;
import cnetboot.framework.pagination;

export namespace cnetboot::chat {


inline constexpr std::size_t default_history_limit = 20;


inline constexpr std::size_t default_token_budget = 8000;


inline constexpr std::size_t max_system_prompt_length = 4000;


inline constexpr std::size_t max_message_characters = 50000;


inline constexpr std::size_t max_reference_context_characters = 50000;


inline constexpr std::size_t max_category_characters = 64;


inline constexpr std::size_t max_title_length = 128;


inline constexpr double default_temperature = 0.7;
inline constexpr double min_temperature = 0.0;
inline constexpr double max_temperature = 2.0;


inline constexpr std::string_view role_system = "system";
inline constexpr std::string_view role_user = "user";
inline constexpr std::string_view role_assistant = "assistant";


inline constexpr std::string_view reference_context_prefix = "User-provided reference context:\n";


inline constexpr std::string_view default_system_prompt =
    "You are a reliable, clear, and honest AI assistant. Answer the user's question "
    "directly, acknowledge missing information, and never fabricate facts. For high-risk "
    "medical, legal, or financial topics, recommend consulting a qualified professional.";


struct chat_session
{
    std::uint64_t id{};
    std::uint64_t owner_id{};
    std::string title;
    std::string category;
    std::string system_prompt;
    std::string model;
    double temperature{default_temperature};
    std::int64_t message_count{};
    std::int64_t last_message_at{};
    std::int64_t created_at{};
    std::int64_t updated_at{};
};


struct chat_message
{
    std::uint64_t id{};
    std::uint64_t session_id{};
    std::string role;           ///< system / user / assistant
    std::string content;
    std::string model;
    std::int64_t tokens_in{};
    std::int64_t tokens_out{};
    std::int64_t created_at{};
};


struct chat_session_query
{
    std::string category;
    cnetboot::framework::pagination::page_request page;
};


[[nodiscard]] auto count_characters(std::string_view text) noexcept -> std::size_t;


[[nodiscard]] auto is_valid_role(std::string_view role) noexcept -> bool;


[[nodiscard]] auto normalize_category(std::string_view raw) -> std::optional<std::string>;


[[nodiscard]] auto normalize_temperature(double raw) noexcept -> std::optional<double>;


[[nodiscard]] auto effective_system_prompt(const chat_session& session, std::string_view fallback) -> std::string;


struct chat_context_options
{
    std::size_t history_limit{default_history_limit};
    std::size_t token_budget{default_token_budget};
    std::string default_system_prompt{};
};


[[nodiscard]] auto estimate_tokens(std::string_view text) noexcept -> std::size_t;


struct context_message
{
    std::string role;
    std::string content;
};


struct context_trim_result
{
    std::size_t removed_messages{};
    std::size_t removed_tokens{};
    std::size_t remaining_tokens{};
    bool limit_satisfied{true};
};


struct chat_context
{
    std::vector<context_message> messages;
    context_trim_result trim;
};


[[nodiscard]] auto build_chat_context(const chat_session& session, std::span<const chat_message> history,
    std::string_view reference_context, std::string_view pending_user_content, const chat_context_options& options)
    -> chat_context;

} // namespace cnetboot::chat
