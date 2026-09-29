import std;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.reading_domain;
import cnetnexus.reading.service.reading_service;

// 断语生成范围用例：scope 取值解析、提示词范围指令与缓存指纹分区。
//
// 迁移自 xuanxue-algorithm tests/reading_scope_test.cpp，断言逐条保留。**明确删除的
// 部分**（依赖尚未迁入 CNetNexus 的 transport 层，见交付报告「删除用例清单」）：
//  - `parse_reading_scope_query` / `uses_target_projection` 的 400 文案断言（第 1、2、3 段）
//    —— 查询串解析属于 transport（task-3 范围）；
//  - `bazi_scope_section` / `zi_wei_scope_section` 的「生成范围数据」段渲染断言（第 4、5 段）
//    —— 同样属于 transport 的提示词投影。
//  这两块依赖 `xuanxue.transport.reading_scope`，CNetNexus 里目前没有对应模块；
//  其余断言（scope 集合语义、提示词指令、指纹分区）原样保留。
namespace {

using error_code = cnetnexus::framework::error::error_code;
namespace engine = cnetnexus::reading;

/// @brief 解析 scope 列表（必须通过）。
auto scope_list(std::string_view raw) -> std::optional<engine::reading_scope_selection>
{
    auto parsed = engine::parse_reading_scope_list(raw);
    if (parsed)
        return std::make_optional(*parsed);
    std::cout << std::format("reading_scope：scope=「{}」解析失败", raw) << std::endl;
    return std::nullopt;
}

} // namespace

/// @brief scope 解析、集合语义、提示词范围指令与缓存指纹分区。
auto main() -> int
{
    // 1. scope 取值解析：缺省 / 大小写不敏感 / 去重 / 顺序无关 / 未知取值与空项被拒。
    {
        const auto plain = scope_list("");
        if (!plain || plain->canonical() != "benming")
            return 1;
        const auto empty = scope_list("   ");
        if (!empty || empty->canonical() != "benming")
            return 2;
        const auto mixed_case = scope_list("DAYUN,Benming");
        if (!mixed_case || mixed_case->canonical() != "benming,dayun")
            return 3;
        const auto dedup = scope_list("liuyue,dayun,liuyue");
        if (!dedup || dedup->canonical() != "dayun,liuyue")
            return 4;
        const auto ordered = scope_list("dayun, liunian");
        if (!ordered || ordered->canonical() != "dayun,liunian")
            return 5;
        if (engine::parse_reading_scope_list("morning").has_value())
            return 6;
        if (engine::parse_reading_scope_list("benming,").has_value())
            return 7;
    }

    // 2. 只有 ba_zi 与 zi_wei 支持 scope 之外的术数，不带 scope 时仍是缺省本命
    //    （原用例第 3 段的算法维度断言改在 domain 层：非 ba_zi/zi_wei 也接受缺省 scope）。
    {
        for (const auto algorithm : {"liu_yao", "mei_hua", "da_liu_ren", "qi_men"})
        {
            const engine::reading_input input{.algorithm = algorithm,
                .report_zh = "报告",
                .question = "事业"};
            const auto request = engine::build_reading_request(input);
            if (!request || !request->messages[1].content.contains("【生成范围】benming"))
                return 22;
        }
    }

    // 3. 提示词：范围指令按固定顺序、多选时分段要求、目标年/月取自 target_*。
    {
        engine::reading_input input{
            .algorithm = "ba_zi",
            .report_zh = "【原局】甲木生于子月",
            .question = "今年事业如何"};
        const auto scopes = scope_list("liuyue,benming,liunian");
        if (!scopes)
            return 50;
        input.scopes = *scopes;
        input.target_year = 2026;
        input.target_lunar_month = 3;
        const auto request = engine::build_reading_request(input);
        if (!request)
            return 51;
        const auto& user = request->messages[1].content;
        if (!user.contains("【生成范围】benming,liunian,liuyue"))
            return 52;
        if (!user.contains("- 本命：围绕原局格局、十神/星曜落宫与旺衰给出本命分析。"))
            return 53;
        if (!user.contains("- 流年：重点分析 2026 流年的干支与原局/大运的刑冲合害、引动与应事。"))
            return 54;
        if (!user.contains("- 流月：重点分析 2026 年 3 月（农历）的干支与原局/大运/流年的作用。"))
            return 55;
        if (!user.contains("按「本命 → 大运 → 流年 → 流月」的顺序分段输出"))
            return 56;
        if (user.find("【生成范围】") > user.find("【排盘报告】") || !user.contains("【原局】甲木生于子月"))
            return 57;

        // 单选流月：只给流月指令，不出现分段要求，也不出现本命/大运指令。
        auto only_liuyue = input;
        const auto single_scopes = scope_list("liuyue");
        if (!single_scopes)
            return 58;
        only_liuyue.scopes = *single_scopes;
        const auto single = engine::build_reading_request(only_liuyue);
        if (!single)
            return 59;
        const auto& single_user = single->messages[1].content;
        if (!single_user.contains("【生成范围】liuyue") || !single_user.contains("- 流月：重点分析 2026 年 3 月") ||
            single_user.contains("顺序分段输出") || single_user.contains("- 本命：") || single_user.contains("- 大运："))
            return 60;

        // 缺省：只做本命分析（与 scope 引入前的默认行为一致）。
        const engine::reading_input defaults{
            .algorithm = "ba_zi",
            .report_zh = "【原局】甲木生于子月",
            .question = ""};
        const auto plain = engine::build_reading_request(defaults);
        if (!plain || !plain->messages[1].content.contains("【生成范围】benming") ||
            !plain->messages[1].content.contains("- 本命："))
            return 61;
    }

    // 4. 缓存指纹：scope 参与分区，同一盘面 + 不同范围不互相命中。
    {
        const auto benming = engine::reading_chart_fingerprint("ba_zi", "benming", "【原局】甲木生于子月");
        if (benming.size() != 64 ||
            benming != engine::reading_chart_fingerprint("ba_zi", "benming", "【原局】甲木生于子月"))
            return 70;
        if (benming == engine::reading_chart_fingerprint("ba_zi", "benming,dayun", "【原局】甲木生于子月"))
            return 71;
        if (benming == engine::reading_chart_fingerprint("ba_zi", "benming", "【原局】甲木生于丑月"))
            return 72;
        if (benming == engine::reading_chart_fingerprint("zi_wei", "benming", "【原局】甲木生于子月"))
            return 73;
    }

    // 5. 所问指纹：只去首尾空白（与缓存键口径一致）。
    {
        if (engine::reading_question_hash("事业") != engine::reading_question_hash("  事业\t"))
            return 74;
        if (engine::reading_question_hash("事业") == engine::reading_question_hash("财运"))
            return 75;
        if (engine::reading_engine_version.empty())
            return 76;
    }

    return 0;
}
