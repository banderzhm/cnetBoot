/**
 * @brief 断语生成的范围口径与提示词投影。
 *
 * 这里只有纯数据与纯函数：把确定性排盘报告投影成 LLM 对话请求，不依赖任何持久化类型。
 * 生成范围的规范串（canonical）同时是缓存键的一部分与响应回显的取值，三处共用一个实现。
 */
export module cnetnexus.reading.domain.reading_domain;

import std;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.llm_contract;

export namespace cnetnexus::reading {

/** @brief 断语生成范围；可多选，输出顺序固定为 本命 -> 大运 -> 流年 -> 流月。 */
enum class reading_scope
{
    benming, ///< 本命：原局格局、十神/星曜落宫与旺衰
    dayun,   ///< 大运：大运干支、起止年龄与吉凶走向
    liunian, ///< 流年：目标流年的干支与原局/大运的刑冲合害、引动与应事
    liuyue,  ///< 流月：目标年目标月的干支与原局/大运/流年的作用
};

/** @brief 规范化后的生成范围集合：去重、顺序无关，canonical() 固定按枚举顺序输出。 */
struct reading_scope_selection
{
    std::vector<reading_scope> scopes{reading_scope::benming}; ///< 命中的范围；缺省仅本命，不得为空

    /** @brief 该范围是否被选中。 */
    [[nodiscard]] auto contains(reading_scope scope) const noexcept -> bool;

    /** @brief 规范化逗号串（固定顺序，与构造顺序无关）。 */
    [[nodiscard]] auto canonical() const -> std::string;

    /** @brief 是否多选；多选时提示词要求按范围分段输出。 */
    [[nodiscard]] auto multiple() const noexcept -> bool;
};

/** @brief 解析 ?scope= 的原始取值：逗号分隔、大小写不敏感、重复取值去重、顺序无关。
 *  @param raw query 里的原始取值；缺失或全空白表示缺省（仅本命）
 *  @return 规范化集合；出现未知取值或空项时返回该取值原文（空项为空串），
 *          由调用方翻成带具体文案的 400
 */
[[nodiscard]] auto parse_reading_scope_list(std::string_view raw)
    -> std::expected<reading_scope_selection, std::string>;

/** @brief 解读输入：确定性算法产出的中文报告 + 用户问题。 */
struct reading_input
{
    std::string algorithm; ///< 术数标识，例如 ba_zi / liu_yao / mei_hua / zi_wei
    std::string report_zh; ///< 上游生成的中文排盘或断卦报告，不得为空
    std::string question;  ///< 用户问题，可为空

    /** @brief 生成范围；缺省仅本命。提示词按它注入范围指令与顺序要求。 */
    reading_scope_selection scopes{};

    /** @brief 流年/流月目标公历年；scope 含 dayun 时可选，含 liunian/liuyue 时必填。 */
    std::optional<int> target_year;

    /** @brief 流月目标月（1—12，正月起算）；scope 含 liuyue 时必填。 */
    std::optional<int> target_lunar_month;
};

/** @brief 把中文报告投影成 LLM 对话请求。
 *  @param input 解读输入
 *  @return 对话请求；algorithm 或 report_zh 为空返回 REQUEST.INVALID_ARGUMENT
 */
[[nodiscard]] auto build_reading_request(const reading_input& input)
    -> std::expected<llm_request, cnetnexus::framework::error::error_code>;

} // namespace cnetnexus::reading
