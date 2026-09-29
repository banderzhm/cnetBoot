module cnetnexus.reading.domain.reading_domain;

import std;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.llm_contract;

namespace cnetnexus::reading {
namespace {

using error_code = cnetnexus::framework::error::error_code;

/// 系统提示把确定性报告与大模型输出严格分开：模型只能解释，不能改写排盘结论。
constexpr std::string_view k_system_prompt{R"(你是玄学排盘解读助手。用户会给你一份由确定性算法生成的中文排盘或断卦报告，你要用通俗语言解释它。

约束：
1. 不得改写、增删或"修正"报告中的四柱、干支、卦象、宫位、星曜、分值与结论，只能引用并解释。
2. 报告没有给出的信息不得臆造；确实需要额外资料时，明确说明缺少什么。
3. 不提供医疗、法律、投资等专业建议，不预测具体生死、疾病或事故。
4. 用简体中文回答，先给结论再给依据，并指出依据来自报告的哪一部分。)"};

/// 生成范围的规范顺序与规范名；canonical() 与解析都只认这一份表。
constexpr std::array<std::pair<reading_scope, std::string_view>, 4> k_scope_names{{
    {reading_scope::benming, "benming"},
    {reading_scope::dayun, "dayun"},
    {reading_scope::liunian, "liunian"},
    {reading_scope::liuyue, "liuyue"},
}};

/// @brief ASCII 小写；scope 取值只可能是 ASCII，不引入 locale 相关的转换。
auto ascii_lower(std::string_view value) -> std::string
{
    std::string result{value};
    for (auto& character : result)
        if (character >= 'A' && character <= 'Z')
            character = static_cast<char>(character - 'A' + 'a');
    return result;
}

/// @brief 去掉首尾 ASCII 空白。
auto trimmed(std::string_view value) -> std::string_view
{
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos)
        return {};
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

/// @brief 把范围名翻成枚举；未登记返回 nullopt。
auto scope_of_name(std::string_view name) -> std::optional<reading_scope>
{
    for (const auto& item : k_scope_names)
        if (item.second == name)
            return item.first;
    return std::nullopt;
}

auto scope_instruction(const reading_input& input, reading_scope scope) -> std::string
{
    switch (scope)
    {
    case reading_scope::benming:
        return "- 本命：围绕原局格局、十神/星曜落宫与旺衰给出本命分析。\n";
    case reading_scope::dayun:
        return "- 大运：在本命基础上，重点分析当前（或指定）大运的干支、起止年龄与吉凶走向。\n";
    case reading_scope::liunian:
        return "- 流年：重点分析 " +
            (input.target_year ? std::to_string(*input.target_year) : std::string{"目标"}) +
            " 流年的干支与原局/大运的刑冲合害、引动与应事。\n";
    case reading_scope::liuyue:
        return "- 流月：重点分析 " +
            (input.target_year ? std::to_string(*input.target_year) : std::string{"目标"}) + " 年 " +
            (input.target_lunar_month ? std::to_string(*input.target_lunar_month)
                                      : std::string{"目标"}) +
            " 月（农历）的干支与原局/大运/流年的作用。\n";
    }
    return {};
}

} // namespace

auto reading_scope_selection::contains(reading_scope scope) const noexcept -> bool
{
    return std::ranges::find(scopes, scope) != scopes.end();
}

auto reading_scope_selection::canonical() const -> std::string
{
    // 按 k_scope_names 的固定顺序输出：与构造顺序、重复次数无关。
    std::string result;
    for (const auto& item : k_scope_names)
    {
        if (!contains(item.first))
            continue;
        if (!result.empty())
            result.push_back(',');
        result.append(item.second);
    }
    return result;
}

auto reading_scope_selection::multiple() const noexcept -> bool
{
    return scopes.size() > 1;
}

auto parse_reading_scope_list(std::string_view raw)
    -> std::expected<reading_scope_selection, std::string>
{
    // 缺失或全空白 = 未提供 = 默认仅本命。
    if (trimmed(raw).empty())
        return reading_scope_selection{};

    std::array<bool, k_scope_names.size()> selected{};
    std::size_t position = 0;
    while (position <= raw.size())
    {
        const auto separator = raw.find(',', position);
        const auto token = trimmed(raw.substr(position,
            separator == std::string_view::npos ? std::string_view::npos : separator - position));
        if (token.empty())
            return std::unexpected{std::string{}}; // 空项（如 "benming,"）同样是未知取值
        const auto scope = scope_of_name(ascii_lower(token));
        if (!scope)
            return std::unexpected{std::string{token}}; // 原文回给调用方拼 400 文案
        for (std::size_t index = 0; index < k_scope_names.size(); ++index)
            if (k_scope_names[index].first == *scope)
                selected[index] = true;
        if (separator == std::string_view::npos)
            break;
        position = separator + 1;
    }

    reading_scope_selection selection;
    selection.scopes.clear();
    for (std::size_t index = 0; index < k_scope_names.size(); ++index)
        if (selected[index])
            selection.scopes.push_back(k_scope_names[index].first);
    return selection;
}

auto build_reading_request(const reading_input& input)
    -> std::expected<llm_request, error_code>
{
    if (input.algorithm.empty() || input.report_zh.empty())
        return std::unexpected{error_code::request_invalid_argument};

    llm_request request{};
    request.messages.push_back({.role = "system", .content = std::string{k_system_prompt}});

    std::string user;
    user.reserve(input.algorithm.size() + input.report_zh.size() + input.question.size() + 512);
    user.append("【术数】").append(input.algorithm);
    user.append("\n【生成范围】").append(input.scopes.canonical());
    user.append("\n【生成范围指令】\n");
    for (const auto& item : k_scope_names)
        if (input.scopes.contains(item.first))
            user.append(scope_instruction(input, item.first));
    user.append("只输出所选范围的分析，未选范围不要展开。");
    if (input.scopes.multiple())
        user.append("多选时按「本命 → 大运 → 流年 → 流月」的顺序分段输出，每段以对应的范围名起头（例如「【大运】」），"
                    "便于用户逐段对应。");
    user.append("\n【排盘报告】\n").append(input.report_zh);
    user.append("\n【用户问题】\n");
    user.append(input.question.empty() ? "用户未提出具体问题，请做整体解读。" : input.question);
    request.messages.push_back({.role = "user", .content = std::move(user)});
    request.temperature = 0.2;
    request.max_tokens = 1024;
    return request;
}

} // namespace cnetnexus::reading
