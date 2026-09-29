module cnetnexus.framework.web.query;

import std;
import cnetmod.protocol.http;

namespace cnetnexus::framework::web::query {

/** @brief 从 application/x-www-form-urlencoded 形式的 query string 中取一个键的值。
 *
 *  键值切分与百分号解码都用框架（cnetmod.protocol.http 的 parse_query_params /
 *  url_decode）；本模块只补框架没提供的一件事：**区分「键缺失」与「键存在但为空」**，
 *  因为参数校验要把这两种情况报成不同的 400 文案。
 *
 *  @param query 原始 query string（不含 '?'）
 *  @param key 键名
 *  @return 命中时返回百分号解码后的值；缺失返回 std::nullopt
 */
auto value(std::string_view query, std::string_view key) -> std::optional<std::string>
{
    const auto parameters = cnetmod::http::parse_query_params(query);
    const auto found = parameters.find(std::string{key});
    if (found == parameters.end())
        return std::nullopt;
    return cnetmod::http::url_decode(found->second);
}

/** @brief 解析十进制整数参数；缺失、空值或含非数字返回 std::nullopt。 */
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

/** @brief 解析逗号分隔的整数列表；缺失返回空列表，出现非法元素返回 std::nullopt。 */
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

} // namespace cnetnexus::framework::web::query
