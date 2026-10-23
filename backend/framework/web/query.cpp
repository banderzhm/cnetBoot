module cnetboot.framework.web.query;

import std;
import cnetmod.protocol.http;

namespace cnetboot::framework::web::query {


auto value(std::string_view query, std::string_view key) -> std::optional<std::string>
{
    const auto parameters = cnetmod::http::parse_query_params(query);
    const auto found = parameters.find(std::string{key});
    if (found == parameters.end())
        return std::nullopt;
    return cnetmod::http::url_decode(found->second);
}


auto integer(std::string_view query, std::string_view key) -> std::optional<int>
{
    const auto text = value(query, key);
    if (!text || text->empty())
        return std::nullopt;
    int result = 0;
    const auto parsed = std::from_chars(text->data(), text->data() + text->size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text->data() + text->size())
        return std::nullopt;
    return result;
}


auto integer_list(std::string_view query, std::string_view key) -> std::optional<std::vector<int>>
{
    const auto text = value(query, key);
    std::vector<int> result;
    if (!text || text->empty())
        return result;
    const std::string_view list{*text};
    std::size_t position = 0;
    while (position <= list.size())
    {
        const auto separator = list.find(',', position);
        const auto item = list.substr(position,
            separator == std::string_view::npos ? std::string_view::npos : separator - position);
        if (item.empty())
            return std::nullopt;
        int parsed_value = 0;
        const auto parsed = std::from_chars(item.data(), item.data() + item.size(), parsed_value);
        if (parsed.ec != std::errc{} || parsed.ptr != item.data() + item.size())
            return std::nullopt;
        result.push_back(parsed_value);
        if (separator == std::string_view::npos)
            break;
        position = separator + 1;
    }
    return result;
}

auto path_id(std::string_view raw) -> std::optional<std::uint64_t>
{
    if (raw.empty())
        return std::nullopt;
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(raw.data(), raw.data() + raw.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != raw.data() + raw.size() || result == 0)
        return std::nullopt;
    return result;
}

} // namespace cnetboot::framework::web::query
