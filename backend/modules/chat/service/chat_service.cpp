module cnetboot.chat.service.chat_service;

import std;
import cnetmod.core.log;
import cnetmod.coro.task;
import cnetboot.chat.domain.chat_domain;
import cnetboot.chat.mapper.chat_mapper;
import cnetboot.framework.audit;
import cnetboot.framework.error;
import cnetboot.framework.pagination;
import cnetboot.ai.domain.llm_contract;
import cnetboot.ai.service.llm_service;

namespace cnetboot::chat {
namespace {

using error_code = cnetboot::framework::error::error_code;


auto truncate_utf8(std::string_view text, std::size_t limit) -> std::string
{
    if (text.size() <= limit)
        return std::string{text};
    auto size = limit;
    while (size > 0 && (static_cast<unsigned char>(text[size]) & 0xC0) == 0x80)
        --size;
    return std::string{text.substr(0, size)};
}


auto title_from_content(std::string_view content) -> std::string
{
    const auto line_end = content.find_first_of("\r\n");
    auto line = content.substr(0, line_end == std::string_view::npos ? content.size() : line_end);
    const auto begin = line.find_first_not_of(" \t");
    if (begin == std::string_view::npos)
        return std::string{default_session_title};
    const auto end = line.find_last_not_of(" \t");
    auto title = truncate_utf8(line.substr(begin, end - begin + 1), 40);
    if (title.empty())
        return std::string{default_session_title};
    return title;
}

auto too_long(std::string_view value, std::size_t limit) noexcept -> bool
{
    return value.size() > limit;
}

} // namespace

void chat_service::set_options(chat_options options)
{
    if (options.history_limit == 0)
        options.history_limit = cnetboot::chat::default_history_limit;
    if (options.token_budget < 256)
        options.token_budget = cnetboot::chat::default_token_budget;
    if (too_long(options.system_prompt, cnetboot::chat::max_system_prompt_length))
        options.system_prompt = truncate_utf8(options.system_prompt,
            cnetboot::chat::max_system_prompt_length);
    options_ = std::move(options);
}

auto chat_service::create_session(const chat_session_create_request& request,
    const cnetboot::framework::audit_actor& actor)
    -> cnetmod::task<std::expected<cnetboot::chat::chat_session, error_code>>
{
    if (too_long(request.title, max_session_title_length) ||
        too_long(request.system_prompt, cnetboot::chat::max_system_prompt_length) ||
        too_long(request.model, max_model_length))
        co_return std::unexpected{error_code::request_invalid_argument};

    const auto category = cnetboot::chat::normalize_category(request.category);
    if (!category)
        co_return std::unexpected{error_code::category_invalid};
    if (request.has_temperature &&
        !cnetboot::chat::normalize_temperature(request.temperature))
        co_return std::unexpected{error_code::request_invalid_argument};

    cnetboot::chat::chat_session value;
    value.owner_id = actor.user_id;
    value.category = *category;
    value.title = request.title.empty() ? std::string{default_session_title} : request.title;
    value.system_prompt = request.system_prompt;
    value.model = request.model;
    value.temperature =
        request.has_temperature ? request.temperature : cnetboot::chat::default_temperature;

    const auto created = co_await mapper_->create_session(value);
    if (!created)
        co_return std::unexpected{created.error()};
    if (audit_ != nullptr)
        (void)co_await audit_->record(actor, cnetboot::framework::audit_action_create,
            "chat_session", 200, std::to_string(created->id));
    co_return *created;
}

auto chat_service::list_sessions(const cnetboot::chat::chat_session_query& query)
    -> cnetmod::task<std::expected<pagination::page_result<cnetboot::chat::chat_session>, error_code>>
{
    auto filter = query;
    filter.page = pagination::normalize(query.page);
    if (!filter.category.empty())
    {
        const auto category = cnetboot::chat::normalize_category(filter.category);
        if (!category)
            co_return std::unexpected{error_code::category_invalid};
        filter.category = *category;
    }
    co_return co_await mapper_->find_session_page(filter);
}

auto chat_service::find_session(std::uint64_t id)
    -> cnetmod::task<std::expected<cnetboot::chat::chat_session, error_code>>
{
    const auto found = co_await mapper_->find_session(id);
    if (!found)
        co_return std::unexpected{found.error()};
    if (!found->has_value())
        co_return std::unexpected{error_code::chat_session_not_found};
    co_return **found;
}

auto chat_service::update_session(std::uint64_t id, const chat_session_patch& patch,
    const cnetboot::framework::audit_actor& actor)
    -> cnetmod::task<std::expected<cnetboot::chat::chat_session, error_code>>
{
    const auto current = co_await mapper_->find_session(id);
    if (!current)
        co_return std::unexpected{current.error()};
    if (!current->has_value())
        co_return std::unexpected{error_code::chat_session_not_found};

    auto merged = current->value();
    if (patch.has_title)
    {
        if (too_long(patch.title, max_session_title_length))
            co_return std::unexpected{error_code::request_invalid_argument};
        merged.title = patch.title.empty() ? std::string{default_session_title} : patch.title;
    }
    if (patch.has_system_prompt)
    {
        if (too_long(patch.system_prompt, cnetboot::chat::max_system_prompt_length))
            co_return std::unexpected{error_code::request_invalid_argument};
        merged.system_prompt = patch.system_prompt;
    }
    if (patch.has_model)
    {
        if (too_long(patch.model, max_model_length))
            co_return std::unexpected{error_code::request_invalid_argument};
        merged.model = patch.model;
    }
    if (patch.has_temperature)
    {
        const auto temperature = cnetboot::chat::normalize_temperature(patch.temperature);
        if (!temperature)
            co_return std::unexpected{error_code::request_invalid_argument};
        merged.temperature = *temperature;
    }

    const auto updated = co_await mapper_->update_session(merged);
    if (!updated)
        co_return std::unexpected{updated.error()};
    if (audit_ != nullptr)
        (void)co_await audit_->record(actor, cnetboot::framework::audit_action_update,
            "chat_session", 200, std::to_string(id));
    co_return *updated;
}

auto chat_service::remove_session(std::uint64_t id,
    const cnetboot::framework::audit_actor& actor)
    -> cnetmod::task<std::expected<void, error_code>>
{
    const auto removed = co_await mapper_->remove_session(id);
    if (!removed)
        co_return std::unexpected{removed.error()};
    if (audit_ != nullptr)
        (void)co_await audit_->record(actor, cnetboot::framework::audit_action_delete,
            "chat_session", 200, std::to_string(id));
    co_return std::expected<void, error_code>{};
}

auto chat_service::list_messages(std::uint64_t session_id, const pagination::page_request& page)
    -> cnetmod::task<std::expected<pagination::page_result<cnetboot::chat::chat_message>, error_code>>
{
    const auto session = co_await mapper_->find_session(session_id);
    if (!session)
        co_return std::unexpected{session.error()};
    if (!session->has_value())
        co_return std::unexpected{error_code::chat_session_not_found};
    co_return co_await mapper_->find_message_page(session_id, pagination::normalize(page));
}

auto chat_service::assemble_context(const cnetboot::chat::chat_session& session,
    std::span<const cnetboot::chat::chat_message> history, std::string_view reference_context,
    std::string_view pending_user_content) const -> chat_context_assembly
{
    const cnetboot::chat::chat_context_options options{
        .history_limit = options_.history_limit,
        .token_budget = options_.token_budget,
        .default_system_prompt = options_.system_prompt,
    };


    auto built = cnetboot::chat::build_chat_context(session, history, reference_context,
        pending_user_content, options);
    chat_context_assembly assembled;
    assembled.trim = built.trim;
    assembled.messages.reserve(built.messages.size());
    for (auto& message : built.messages)
        assembled.messages.push_back(cnetboot::ai::llm_message{
            .role = std::move(message.role),
            .content = std::move(message.content)});
    return assembled;
}

auto chat_service::build_llm_request(const cnetboot::chat::chat_session& session,
    const chat_message_request& request) const -> cnetmod::task<cnetboot::ai::llm_request>
{
    cnetboot::ai::llm_request result;


    const auto history = co_await mapper_->find_recent_messages(session.id, options_.history_limit);
    if (!history)
    {

        logger::warn{"chat history load failed: session={} error={}", session.id,
            static_cast<int>(history.error())};
    }
    const std::span<const cnetboot::chat::chat_message> window{
        history ? std::span<const cnetboot::chat::chat_message>{*history}
                : std::span<const cnetboot::chat::chat_message>{}};

    auto assembled = assemble_context(session, window, request.reference_context, request.content);

    const auto& trim = assembled.trim;
    if (trim.removed_messages > 0 || !trim.limit_satisfied)
    {
        if (trim.limit_satisfied)
            logger::info{"chat context trimmed: session={} removed_messages={} removed_tokens={} remaining_tokens={}",
                session.id, trim.removed_messages, trim.removed_tokens, trim.remaining_tokens};
        else
            logger::warn{"chat context over budget after trim: session={} removed_messages={} removed_tokens={} remaining_tokens={} limit_satisfied=false",
                session.id, trim.removed_messages, trim.removed_tokens, trim.remaining_tokens};
    }
    result.messages = std::move(assembled.messages);


    result.provider = options_.provider;
    result.model = session.model.empty() ? options_.model : session.model;
    result.temperature = session.temperature;
    result.max_tokens = options_.max_tokens;
    result.reasoning_effort = options_.reasoning_effort;
    co_return result;
}

auto chat_service::refresh_title(const cnetboot::chat::chat_session& session,
    std::string_view content, const cnetboot::framework::audit_actor& actor)
    -> cnetmod::task<std::expected<cnetboot::chat::chat_session, error_code>>
{
    auto merged = session;
    merged.title = title_from_content(content);
    const auto updated = co_await mapper_->update_session(merged);
    if (!updated)
        co_return std::unexpected{updated.error()};
    if (audit_ != nullptr)
        (void)co_await audit_->record(actor, cnetboot::framework::audit_action_update,
            "chat_session", 200, std::to_string(session.id));
    co_return *updated;
}

auto chat_service::prepare_turn(std::uint64_t session_id, const chat_message_request& request,
    const cnetboot::framework::audit_actor& actor)
    -> cnetmod::task<std::expected<turn_plan, error_code>>
{
    if (llm_ == nullptr || !llm_->configured())
        co_return std::unexpected{error_code::upstream_unavailable};

    if (request.content.empty() ||
        cnetboot::chat::count_characters(request.content) >
            cnetboot::chat::max_message_characters ||
        cnetboot::chat::count_characters(request.reference_context) >
            cnetboot::chat::max_reference_context_characters)
        co_return std::unexpected{error_code::request_invalid_argument};

    const auto found = co_await mapper_->find_session(session_id);
    if (!found)
        co_return std::unexpected{found.error()};
    if (!found->has_value())
        co_return std::unexpected{error_code::chat_session_not_found};

    turn_plan plan;
    plan.session = found->value();

    if (plan.session.message_count == 0 &&
        (plan.session.title.empty() || plan.session.title == default_session_title))
    {
        const auto refreshed = co_await refresh_title(plan.session, request.content, actor);
        if (!refreshed)
            co_return std::unexpected{refreshed.error()};
        plan.session = *refreshed;
    }

    plan.request = co_await build_llm_request(plan.session, request);

    cnetboot::chat::chat_message user;
    user.session_id = session_id;
    user.role = std::string{cnetboot::chat::role_user};
    user.content = request.content;
    const auto appended = co_await mapper_->append_message(user);
    if (!appended)
        co_return std::unexpected{appended.error()};
    plan.user_message = *appended;
    if (audit_ != nullptr)
        (void)co_await audit_->record(actor, cnetboot::framework::audit_action_create,
            "chat_message", 200, std::to_string(session_id));
    co_return plan;
}

auto chat_service::finish_turn(const turn_plan& plan, std::string content, int tokens_in,
    int tokens_out) -> cnetmod::task<std::expected<chat_turn, error_code>>
{
    cnetboot::chat::chat_message assistant;
    assistant.session_id = plan.session.id;
    assistant.role = std::string{cnetboot::chat::role_assistant};
    assistant.content = std::move(content);
    assistant.model = plan.request.model;
    assistant.tokens_in = tokens_in;
    assistant.tokens_out = tokens_out;

    const auto appended = co_await mapper_->append_message(assistant);
    if (!appended)
        co_return std::unexpected{appended.error()};

    chat_turn turn;
    turn.user_message = plan.user_message;
    turn.assistant_message = *appended;
    co_return turn;
}

auto chat_service::send_message(std::uint64_t session_id, const chat_message_request& request,
    const cnetboot::framework::audit_actor& actor)
    -> cnetmod::task<std::expected<chat_turn, error_code>>
{
    auto plan = co_await prepare_turn(session_id, request, actor);
    if (!plan)
        co_return std::unexpected{plan.error()};

    const auto completion = co_await llm_->complete(plan->request);
    if (!completion)
    {

        logger::warn{"chat completion failed: session={} error={}", session_id,
            static_cast<int>(completion.error())};
        co_return std::unexpected{completion.error()};
    }
    co_return co_await finish_turn(*plan, completion->content, completion->tokens_in,
        completion->tokens_out);
}

auto chat_service::stream_message(std::uint64_t session_id, const chat_message_request& request,
    const cnetboot::framework::audit_actor& actor, const chat_delta_sink& sink)
    -> cnetmod::task<std::expected<chat_turn, error_code>>
{
    auto plan = co_await prepare_turn(session_id, request, actor);
    if (!plan)
        co_return std::unexpected{plan.error()};

    const auto completion = co_await llm_->stream(plan->request, sink);
    if (!completion)
    {
        logger::warn{"chat stream failed: session={} error={}", session_id,
            static_cast<int>(completion.error())};
        co_return std::unexpected{completion.error()};
    }
    co_return co_await finish_turn(*plan, completion->content, completion->tokens_in,
        completion->tokens_out);
}

} // namespace cnetboot::chat
