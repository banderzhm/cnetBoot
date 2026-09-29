import std;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.reading_domain;

namespace prompt = cnetnexus::reading;

// 提示词投影用例：报告与问题必须原样进入 user 消息，约束必须出现在 system 消息。
//
// 迁移自 xuanxue-algorithm tests/ai_prompt_test.cpp：
//  - xuanxue::service::ai::build_reading_request -> cnetnexus::reading::build_reading_request；
//  - 玄学第 8 段断言 temperature <= 0.5 且 max_tokens > 0，本仓口径同为 0.2 / 1024，
//    断言不变；
//  - 玄学第 9 段断言空问题时 user 消息包含「未提出具体问题」；CNetNexus 的占位说明是
//    「用户未提出具体问题，请做整体解读。」，改为断言该文案（语义等价，不是放宽）；
//  - algorithm / report_zh 为空必须 REQUEST.INVALID_ARGUMENT，断言不变。
auto main() -> int
{
    const prompt::reading_input input{.algorithm = "ba_zi",
        .report_zh = "四柱：庚辰 壬午 甲辰 壬申\n日主：甲",
        .question = "今年事业如何"};
    const auto request = prompt::build_reading_request(input);
    if (!request)
        return 1;
    if (request->messages.size() != 2)
        return 2;
    if (request->messages[0].role != "system" || request->messages[1].role != "user")
        return 3;
    if (!request->messages[0].content.contains("不得改写") || !request->messages[0].content.contains("不得臆造"))
        return 4;
    if (!request->messages[1].content.contains("四柱：庚辰 壬午 甲辰 壬申"))
        return 5;
    if (!request->messages[1].content.contains("今年事业如何"))
        return 6;
    if (!request->messages[1].content.contains("ba_zi"))
        return 7;
    if (request->temperature > 0.5 || request->max_tokens <= 0)
        return 8;

    // 未提问时使用占位说明，不允许空问题直接透传。
    auto no_question = input;
    no_question.question.clear();
    const auto fallback = prompt::build_reading_request(no_question);
    if (!fallback || !fallback->messages[1].content.contains("用户未提出具体问题"))
        return 9;

    // 报告为空或术数标识为空必须被拒绝。
    auto empty_report = input;
    empty_report.report_zh.clear();
    const auto empty_report_result = prompt::build_reading_request(empty_report);
    if (empty_report_result || empty_report_result.error() !=
            cnetnexus::framework::error::error_code::request_invalid_argument)
        return 10;
    auto empty_algorithm = input;
    empty_algorithm.algorithm.clear();
    const auto empty_algorithm_result = prompt::build_reading_request(empty_algorithm);
    if (empty_algorithm_result || empty_algorithm_result.error() !=
            cnetnexus::framework::error::error_code::request_invalid_argument)
        return 11;
    return 0;
}
