module cnetboot.chat.domain.chat_domain;

import std;

namespace cnetboot::chat {

auto count_characters(std::string_view text) noexcept -> std::size_t
{
    std::size_t total = 0;
    for (const auto raw : text)
    {
        // UTF-8 续接字节（10xxxxxx）不计：一个码点算一个字，中文与英文都是 1。
        if ((static_cast<unsigned char>(raw) & 0xC0) != 0x80)
            ++total;
    }
    return total;
}

auto is_valid_role(std::string_view role) noexcept -> bool
{
    return role == role_system || role == role_user || role == role_assistant;
}

auto normalize_category(std::string_view raw) -> std::optional<std::string>
{
    const auto first = raw.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
        return std::string{};
    const auto last = raw.find_last_not_of(" \t\r\n");
    const auto normalized = raw.substr(first, last - first + 1);
    if (count_characters(normalized) > max_category_characters)
        return std::nullopt;
    return std::string{normalized};
}

auto normalize_temperature(double raw) noexcept -> std::optional<double>
{
    if (raw < min_temperature || raw > max_temperature)
        return std::nullopt;
    return raw;
}

auto effective_system_prompt(const chat_session& session, std::string_view fallback) -> std::string
{
    if (!session.system_prompt.empty())
        return session.system_prompt;
    if (!fallback.empty())
        return std::string{fallback};
    return std::string{default_system_prompt};
}

auto estimate_tokens(std::string_view text) noexcept -> std::size_t
{
    if (text.empty())
        return 0;
    // 3 字节/token 的上界估计：宁可多算，也不要在上下文预算上低估。
    return text.size() / 3 + 1;
}

auto build_chat_context(const chat_session& session, std::span<const chat_message> history,
    std::string_view reference_context, std::string_view pending_user_content, const chat_context_options& options)
    -> chat_context
{
    std::vector<context_message> window;
    window.reserve(history.size());
    for (const auto& message : history)
        window.push_back(context_message{.role = message.role, .content = message.content});

    context_trim_result trim;
    const auto total_tokens = [&window]
    {
        std::size_t total = 0;
        for (const auto& message : window)
            total += estimate_tokens(message.content);
        return total;
    };
    const auto over_limit = [&]
    {
        return (options.history_limit > 0 && window.size() > options.history_limit) ||
            (options.token_budget > 0 && total_tokens() > options.token_budget);
    };
    while (window.size() > 1 && over_limit())
    {
        const auto removable = std::ranges::find_if(window.begin(), window.end() - 1,
            [](const context_message& message)
            {
                return message.role != role_system && message.role != "developer";
            });
        if (removable == window.end() - 1)
            break;
        trim.removed_tokens += estimate_tokens(removable->content);
        ++trim.removed_messages;
        window.erase(removable);
    }
    trim.remaining_tokens = total_tokens();
    trim.limit_satisfied = !over_limit();

    std::vector<context_message> messages;
    messages.reserve(window.size() + 3);

    if (auto prompt = effective_system_prompt(session, options.default_system_prompt); !prompt.empty())
        messages.push_back(context_message{.role = std::string{role_system}, .content = std::move(prompt)});

    if (!reference_context.empty())
    {
        std::string content{reference_context_prefix};
        content += '\n';
        content.append(reference_context);
        messages.push_back(context_message{.role = std::string{role_system}, .content = std::move(content)});
    }

    messages.insert(messages.end(), std::make_move_iterator(window.begin()),
        std::make_move_iterator(window.end()));

    if (!pending_user_content.empty())
        messages.push_back(
            context_message{.role = std::string{role_user}, .content = std::string{pending_user_content}});

    return chat_context{.messages = std::move(messages), .trim = trim};
}

} // namespace cnetboot::chat
