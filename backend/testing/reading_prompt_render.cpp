import std;
import cnetnexus.chat.domain.chat_domain;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.reading_domain;

namespace prompt = cnetnexus::reading;
namespace chat = cnetnexus::chat;

// 渲染一致性用例：把改造前（手写拼接）的提示词字面量固化下来，逐字节比对当前渲染结果。
// 改动提示词文案时本用例必须同步更新，否则就是「悄悄改了模型看到的输入」。
//
// 迁移自 xuanxue-algorithm tests/prompt_render_test.cpp：
//  - 命名换成 cnetnexus.reading / cnetnexus.chat 口径；
//  - 玄学那份提示词由 cnetmod 的 PromptTemplate 渲染，CNetNexus 的
//    cnetnexus.reading.domain.reading_domain 是自己拼装（模板语义等价），字面量一致；
//  - 原文件第 3 段「框架窗口策略固化」测的是玄学的 build_chat_context（走 cnetmod
//    trim_messages、preserve_system_messages 等框架语义），CNetNexus 的
//    cnetnexus.chat.domain.chat_domain 是自持裁剪策略（从最旧的非 system 消息开始丢），
//    因此这 3 组断言按 CNetNexus 的真实实现重写为等价口径：
//      * token_budget 只约束历史窗口本身，system 与本轮输入不占预算 —— 不变；
//      * 条数窗口先丢非 system 消息、历史 system 优先保留 —— 不变；
//      * 被挤出的消息条数/token 进 trim_result 观测 —— 不变。
//    唯一的口径差异是 token 估算：玄学用框架的估算，CNetNexus 用
//    chat_domain::estimate_tokens（3 字节/token 上界），期望值按后者给。
namespace {

using error_code = cnetnexus::framework::error::error_code;

/// 断言计数：用例结束时核对总数，防止某个子用例被跳过。
std::size_t checks = 0;

/// system 消息的字面量（cnetnexus.reading.domain.reading_domain 的 k_system_prompt 原文）。
constexpr std::string_view k_system_expected = R"(你是玄学排盘解读助手。用户会给你一份由确定性算法生成的中文排盘或断卦报告，你要用通俗语言解释它。

约束：
1. 不得改写、增删或"修正"报告中的四柱、干支、卦象、宫位、星曜、分值与结论，只能引用并解释。
2. 报告没有给出的信息不得臆造；确实需要额外资料时，明确说明缺少什么。
3. 不提供医疗、法律、投资等专业建议，不预测具体生死、疾病或事故。
4. 用简体中文回答，先给结论再给依据，并指出依据来自报告的哪一部分。)";

/// 盘面摘要 system 消息的字面量（chart_context_prefix + 换行 + 摘要原文）。
constexpr std::string_view k_chart_context_expected = "当前盘面摘要（由页面提供，仅作参考）：\n日主甲木，月令子水";

/// 评测输入里的报告正文（两行，用来覆盖换行不被吞掉）。
constexpr std::string_view k_report = "四柱：庚辰 壬午 甲辰 壬申\n日主：甲";

/// 默认 benming + 带提问：user 消息的字面量。
constexpr std::string_view k_user_default_benming = R"(【术数】bazi
【生成范围】benming
【生成范围指令】
- 本命：围绕原局格局、十神/星曜落宫与旺衰给出本命分析。
只输出所选范围的分析，未选范围不要展开。
【排盘报告】
四柱：庚辰 壬午 甲辰 壬申
日主：甲
【用户问题】
今年事业如何)";

/// 不带提问：占位说明必须原样接在「【用户问题】」段。
constexpr std::string_view k_user_benming_no_question = R"(【术数】bazi
【生成范围】benming
【生成范围指令】
- 本命：围绕原局格局、十神/星曜落宫与旺衰给出本命分析。
只输出所选范围的分析，未选范围不要展开。
【排盘报告】
四柱：庚辰 壬午 甲辰 壬申
日主：甲
【用户问题】
用户未提出具体问题，请做整体解读。)";

/// 仅 dayun：目标流年对指令无影响（只列大运）。
constexpr std::string_view k_user_dayun = R"(【术数】bazi
【生成范围】dayun
【生成范围指令】
- 大运：在本命基础上，重点分析当前（或指定）大运的干支、起止年龄与吉凶走向。
只输出所选范围的分析，未选范围不要展开。
【排盘报告】
四柱：庚辰 壬午 甲辰 壬申
日主：甲
【用户问题】
今年事业如何)";

/// 仅 liunian：目标年份替换进指令。
constexpr std::string_view k_user_liunian = R"(【术数】bazi
【生成范围】liunian
【生成范围指令】
- 流年：重点分析 2025 流年的干支与原局/大运的刑冲合害、引动与应事。
只输出所选范围的分析，未选范围不要展开。
【排盘报告】
四柱：庚辰 壬午 甲辰 壬申
日主：甲
【用户问题】
今年事业如何)";

/// 仅 liuyue：年份与农历月两个变量都要落到指令里。
constexpr std::string_view k_user_liuyue = R"(【术数】bazi
【生成范围】liuyue
【生成范围指令】
- 流月：重点分析 2025 年 6 月（农历）的干支与原局/大运/流年的作用。
只输出所选范围的分析，未选范围不要展开。
【排盘报告】
四柱：庚辰 壬午 甲辰 壬申
日主：甲
【用户问题】
今年事业如何)";

/// liunian 缺目标年份时的兜底：文案仍是「目标」，不落 0。
constexpr std::string_view k_user_liunian_without_target = R"(【术数】bazi
【生成范围】liunian
【生成范围指令】
- 流年：重点分析 目标 流年的干支与原局/大运的刑冲合害、引动与应事。
只输出所选范围的分析，未选范围不要展开。
【排盘报告】
四柱：庚辰 壬午 甲辰 壬申
日主：甲
【用户问题】
今年事业如何)";

/// 多选 benming + dayun：追加分段输出要求。
constexpr std::string_view k_user_benming_dayun = R"(【术数】bazi
【生成范围】benming,dayun
【生成范围指令】
- 本命：围绕原局格局、十神/星曜落宫与旺衰给出本命分析。
- 大运：在本命基础上，重点分析当前（或指定）大运的干支、起止年龄与吉凶走向。
只输出所选范围的分析，未选范围不要展开。多选时按「本命 → 大运 → 流年 → 流月」的顺序分段输出，每段以对应的范围名起头（例如「【大运】」），便于用户逐段对应。
【排盘报告】
四柱：庚辰 壬午 甲辰 壬申
日主：甲
【用户问题】
今年事业如何)";

/// 四个范围全选：指令与分段要求的完整顺序。
constexpr std::string_view k_user_all_scopes = R"(【术数】bazi
【生成范围】benming,dayun,liunian,liuyue
【生成范围指令】
- 本命：围绕原局格局、十神/星曜落宫与旺衰给出本命分析。
- 大运：在本命基础上，重点分析当前（或指定）大运的干支、起止年龄与吉凶走向。
- 流年：重点分析 2025 流年的干支与原局/大运的刑冲合害、引动与应事。
- 流月：重点分析 2025 年 6 月（农历）的干支与原局/大运/流年的作用。
只输出所选范围的分析，未选范围不要展开。多选时按「本命 → 大运 → 流年 → 流月」的顺序分段输出，每段以对应的范围名起头（例如「【大运】」），便于用户逐段对应。
【排盘报告】
四柱：庚辰 壬午 甲辰 壬申
日主：甲
【用户问题】
今年事业如何)";

/// 报告与提问里带花括号：只替换变量，正文原样透传。
constexpr std::string_view k_user_with_braces = R"(【术数】bazi
【生成范围】benming
【生成范围指令】
- 本命：围绕原局格局、十神/星曜落宫与旺衰给出本命分析。
只输出所选范围的分析，未选范围不要展开。
【排盘报告】
四柱：{甲} 壬午
日主：{report_zh}
【用户问题】
第 {question} 段)";

/// 渲染期望：user 消息的完整字面量。
struct user_expectation
{
    std::string_view name;    ///< 用例名（失败时便于定位）
    std::string_view content; ///< user 消息原文
};

/// @brief 逐字节比对一次渲染结果与字面量。
/// @return 0 表示一致；非 0 是本次比对中首个失败的断言序号
auto expect_render(const prompt::reading_input& input, const user_expectation& expected) -> int
{
    const auto request = prompt::build_reading_request(input);
    ++checks;
    if (!request)
        return 1;
    ++checks;
    if (request->messages.size() != 2)
        return 2;
    ++checks;
    if (request->messages[0].role != "system" || request->messages[1].role != "user")
        return 3;
    ++checks;
    if (request->messages[0].content != k_system_expected)
        return 4;
    ++checks;
    if (request->messages[1].content != expected.content)
        return 5;
    ++checks;
    // 采样参数（解读类任务低温、输出上限 1024）。
    if (request->temperature != 0.2 || request->max_tokens != 1024)
        return 6;
    return 0;
}

/// @brief 造一个最小输入；报告正文固定为 k_report，问题按用例给。
auto make_input(std::string_view question) -> prompt::reading_input
{
    return prompt::reading_input{
        .algorithm = "bazi",
        .report_zh = std::string{k_report},
        .question = std::string{question}};
}

/// @brief 盘面上下文有/无：注入的 system 消息逐字节比对字面量。
auto expect_chart_context() -> int
{
    const chat::chat_session session{};
    const chat::chat_context_options options{
        .history_limit = 20,
        .token_budget = 8000,
        .default_system_prompt = "服务默认提示词"};
    const std::span<const chat::chat_message> no_history{};

    // 有盘面上下文：system（服务默认）→ system（摘要）→ user（本轮输入）。
    const auto with_chart = chat::build_chat_context(session, no_history, "日主甲木，月令子水", "在吗", options);
    ++checks;
    if (with_chart.messages.size() != 3)
        return 1;
    ++checks;
    if (with_chart.messages[0].role != "system" || with_chart.messages[0].content != "服务默认提示词")
        return 2;
    ++checks;
    if (with_chart.messages[1].role != "system" || with_chart.messages[1].content != k_chart_context_expected)
        return 3;
    ++checks;
    if (with_chart.messages[2].role != "user" || with_chart.messages[2].content != "在吗")
        return 4;
    // 没有历史可裁：trim_result 必须是「零裁剪、预算满足」。
    ++checks;
    if (with_chart.trim.removed_messages != 0 || with_chart.trim.removed_tokens != 0 ||
        !with_chart.trim.limit_satisfied)
        return 11;

    // 无盘面上下文：不注入摘要消息，只剩 system + user。
    const auto without_chart = chat::build_chat_context(session, no_history, {}, "在吗", options);
    ++checks;
    if (without_chart.messages.size() != 2)
        return 5;
    ++checks;
    if (without_chart.messages[1].role != "user" || without_chart.messages[1].content != "在吗")
        return 6;

    // 窗口策略固化 1：token_budget 只约束历史窗口本身，system 与本轮输入不占预算。
    //  「服务默认提示词」18 字节 → estimate_tokens = 18/3+1 = 7，「在吗」3 字节 → 2；
    //  历史 3 条各 7 字节 → 3 token/条，合计 9 ≤ 10，因此一条都不裁。
    {
        std::vector<chat::chat_message> history;
        for (int index = 0; index < 3; ++index)
            history.push_back(chat::chat_message{.id = static_cast<std::uint64_t>(index + 1),
                .session_id = 1,
                .role = index % 2 == 0 ? "user" : "assistant",
                .content = "xxxxxxx"});
        const chat::chat_context_options narrow{
            .history_limit = 20,
            .token_budget = 10,
            .default_system_prompt = "服务默认提示词"};
        const auto kept = chat::build_chat_context(session, history, {}, "在吗", narrow);
        ++checks;
        if (kept.messages.size() != 5) // system + 3 条历史 + 本轮输入
            return 7;
        ++checks;
        if (kept.messages[1].content != "xxxxxxx" || kept.messages[4].content != "在吗")
            return 8;
        // 9 token ≤ 预算 10：一条都不裁，观测为满足。
        ++checks;
        if (kept.trim.removed_messages != 0 || !kept.trim.limit_satisfied)
            return 12;
    }

    // 窗口策略固化 2：条数窗口先丢 user/assistant，历史里的 system 消息优先保留。
    {
        std::vector<chat::chat_message> history;
        history.push_back(chat::chat_message{.id = 1, .session_id = 1, .role = "system", .content = "s1s1s1"});
        history.push_back(chat::chat_message{.id = 2, .session_id = 1, .role = "user", .content = "u1"});
        history.push_back(chat::chat_message{.id = 3, .session_id = 1, .role = "user", .content = "u2"});
        const chat::chat_context_options windowed{
            .history_limit = 2,
            .token_budget = 8000,
            .default_system_prompt = "服务默认提示词"};
        const auto kept = chat::build_chat_context(session, history, {}, "在吗", windowed);
        ++checks;
        if (kept.messages.size() != 4) // system + 历史 s1/u2 + 本轮输入
            return 9;
        ++checks;
        if (kept.messages[1].content != "s1s1s1" || kept.messages[2].content != "u2" ||
            kept.messages[3].content != "在吗")
            return 10;
        // 裁剪观测量：正好丢掉被挤出的 u1（1 条 / 1 token），系统消息与尾部保留。
        ++checks;
        if (kept.trim.removed_messages != 1 || kept.trim.removed_tokens != 1 || !kept.trim.limit_satisfied)
            return 13;
        ++checks;
        if (kept.trim.remaining_tokens != 4) // s1s1s1(6 字节→3) + u2(2 字节→1)
            return 14;
    }
    return 0;
}

/// 本用例的断言总数；渲染矩阵或断言变了必须同步这里
/// （含 2 条窗口策略固化断言与 4 条 trim_result 观测断言）。
constexpr std::size_t expected_checks = 70;

} // namespace

/// @brief 渲染一致性：默认 benming、四个范围各一、带/不带提问、盘面上下文有/无。
auto main() -> int
{
    // 默认范围（不传 scope）＝ 仅本命，带提问。
    if (expect_render(make_input("今年事业如何"), {"默认benming+带提问", k_user_default_benming}) != 0)
        return 1;

    // 不带提问：走占位说明。
    if (expect_render(make_input(""), {"不带提问", k_user_benming_no_question}) != 0)
        return 2;

    // 仅大运。
    {
        auto input = make_input("今年事业如何");
        input.scopes = prompt::reading_scope_selection{.scopes = {prompt::reading_scope::dayun}};
        input.target_year = 2025;
        if (expect_render(input, {"仅dayun", k_user_dayun}) != 0)
            return 3;
    }

    // 仅流年：目标年份替换进指令。
    {
        auto input = make_input("今年事业如何");
        input.scopes = prompt::reading_scope_selection{.scopes = {prompt::reading_scope::liunian}};
        input.target_year = 2025;
        if (expect_render(input, {"仅liunian", k_user_liunian}) != 0)
            return 4;
    }

    // 仅流月：年份 + 农历月两个变量。
    {
        auto input = make_input("今年事业如何");
        input.scopes = prompt::reading_scope_selection{.scopes = {prompt::reading_scope::liuyue}};
        input.target_year = 2025;
        input.target_lunar_month = 6;
        if (expect_render(input, {"仅liuyue", k_user_liuyue}) != 0)
            return 5;
    }

    // 流年缺目标年份：兜底文案是「目标」。
    {
        auto input = make_input("今年事业如何");
        input.scopes = prompt::reading_scope_selection{.scopes = {prompt::reading_scope::liunian}};
        if (expect_render(input, {"liunian缺目标年份", k_user_liunian_without_target}) != 0)
            return 6;
    }

    // 多选：追加分段输出要求。
    {
        auto input = make_input("今年事业如何");
        input.scopes =
            prompt::reading_scope_selection{.scopes = {prompt::reading_scope::benming, prompt::reading_scope::dayun}};
        if (expect_render(input, {"多选benming+dayun", k_user_benming_dayun}) != 0)
            return 7;
    }

    // 四个范围全选：指令顺序固定 本命 → 大运 → 流年 → 流月。
    {
        auto input = make_input("今年事业如何");
        input.scopes = prompt::reading_scope_selection{.scopes = {prompt::reading_scope::liuyue,
                                                           prompt::reading_scope::liunian, prompt::reading_scope::dayun, prompt::reading_scope::benming}};
        input.target_year = 2025;
        input.target_lunar_month = 6;
        if (expect_render(input, {"四范围全选", k_user_all_scopes}) != 0)
            return 8;
    }

    // 正文里的花括号原样透传：变量值不参与模板解析。
    {
        auto input = prompt::reading_input{.algorithm = "bazi",
            .report_zh = "四柱：{甲} 壬午\n日主：{report_zh}",
            .question = "第 {question} 段"};
        if (expect_render(input, {"正文含花括号", k_user_with_braces}) != 0)
            return 9;
    }

    // 盘面上下文「无」：报告为空必须被拒，不能渲染出空盘面提示词。
    {
        auto input = make_input("今年事业如何");
        input.report_zh.clear();
        const auto rejected = prompt::build_reading_request(input);
        ++checks;
        if (rejected || rejected.error() != error_code::request_invalid_argument)
            return 10;
    }

    // 术数标识为空同样拒绝。
    {
        auto input = make_input("今年事业如何");
        input.algorithm.clear();
        const auto rejected = prompt::build_reading_request(input);
        ++checks;
        if (rejected || rejected.error() != error_code::request_invalid_argument)
            return 11;
    }

    // 对话侧盘面上下文有/无（属于 Service 装配，这里固化前缀字面量防止被顺手改掉）。
    if (expect_chart_context() != 0)
        return 12;

    std::cout << std::format("reading_prompt_render: 实际断言 {} 条，期望 {} 条", checks, expected_checks)
              << std::endl;
    if (checks != expected_checks)
        return 13;
    return 0;
}