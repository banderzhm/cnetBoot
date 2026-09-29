/**
 * @brief 断语生成用例：把确定性报告投影成提示词并交给补全实现。
 *
 * 补全未装配（模型未配置）时返回 DEPENDENCY.UNAVAILABLE，不静默降级成本地规则。
 * 引擎版本串与两个指纹在这里统一计算：缓存键的规范化口径只此一处。
 */
export module cnetnexus.reading.service.reading_service;

import std;
import cnetmod.coro.task;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.llm_contract;
import cnetnexus.reading.domain.reading_domain;
import cnetnexus.reading.service.llm_service;

export namespace cnetnexus::reading {

namespace error = cnetnexus::framework::error;

/** @brief 解读生成策略；推理强度与输出上限直接决定断语延迟。 */
struct reading_options
{
    std::string model;                    ///< 模型覆盖；空表示由补全实现取配置默认值
    std::string reasoning_effort{"none"}; ///< 透传给 OpenAI 兼容网关的推理强度；空表示不发送
    int max_tokens{512};                  ///< 输出上限；越小越快、越长越完整
};

/** @brief 断语缓存引擎版本：盘面投影结构或提示词模板变化时必须升这个串。
 *
 *  口径：`reading-engine/<n>`，改动以下任一项就 +1（读取时版本不一致按未命中处理，
 *  旧缓存自动失效，不需要删表）：
 *  - 提示词模板或消息组装口径；
 *  - 各适配器投影进 report_zh 的盘面结构（含字段与文案）；
 *  - 缓存键的规范化口径（reading_chart_fingerprint / reading_question_hash）。
 *
 *  模型名与采样参数**不进**这个串：它们不改变「同一盘面 + 同一问题」的语义。
 *
 *  版本历史：reading-engine/2 = 盘面指纹并入生成范围（scope），且报告增加
 *  「生成范围数据」段（大运列表/当前大运、流年干支、流月干支）。
 */
inline constexpr std::string_view reading_engine_version{"reading-engine/2"};

/** @brief 盘面指纹：算法 + 生成范围 + 规范化中文报告 -> sha256 hex（缓存键的盘面维度）。
 *
 *  规范化只此一处：`algorithm + '\x1f' + scope + '\x1f' + report_zh`（算法作命名空间，
 *  \x1f 是控制字符，不会出现在报告文本或范围串里）。报告本身是排盘入参与生成范围的
 *  确定性投影（不含时间戳/请求 id/随机量），因此同指纹 ⇒ 同提示词 ⇒ 复用断语安全。
 */
[[nodiscard]] auto reading_chart_fingerprint(std::string_view algorithm, std::string_view scope,
    std::string_view report_zh) -> std::string;

/** @brief 所问指纹：问句去掉首尾空白（空格 / 制表 / 换行）后 -> sha256 hex。
 *
 *  提示词里仍使用问句**原文**，这里只影响缓存分区：用户多打了一个首尾空格时仍命中同一条缓存。
 */
[[nodiscard]] auto reading_question_hash(std::string_view question) -> std::string;

/** @brief 解读用例；补全实现由组合根注入（可以为空实现，此时返回 DEPENDENCY.UNAVAILABLE）。 */
class reading_service
{
public:
    explicit reading_service(llm_service& llm) noexcept
        : llm_{&llm}
    {
    }

    reading_service(const reading_service&) = delete;
    reading_service(reading_service&&) = delete;

    /** @brief 当前补全实现。 */
    [[nodiscard]] auto llm() const noexcept -> llm_service&
    {
        return *llm_;
    }

    /** @brief 设置生成策略（推理强度/输出上限/模型覆盖）。 */
    void set_options(reading_options options) noexcept
    {
        options_ = std::move(options);
    }

    /** @brief 当前生成策略；调用方用它取模型名写进缓存行与 SSE done 帧。 */
    [[nodiscard]] auto options() const noexcept -> const reading_options&
    {
        return options_;
    }

    /** @brief 执行一次非流式解读（一次性返回整段）。
     *  @return 模型输出文本；报告为空等输入问题返回 REQUEST.INVALID_ARGUMENT */
    [[nodiscard]] auto read(const reading_input& input) const
        -> cnetmod::task<std::expected<std::string, error::error_code>>;

    /** @brief 执行一次流式解读：提示词与非流式完全一致，只是输出逐帧交给 sink。
     *  @param sink 增量回调；返回 false 表示下游已断开
     *  @return 全文与 token 计量（流式路径网关通常拿不到 usage，计量可能为 0） */
    [[nodiscard]] auto read_stream(const reading_input& input, const llm_delta_sink& sink) const
        -> cnetmod::task<std::expected<llm_completion, error::error_code>>;

private:
    /// @brief 组装一次解读请求：入参校验 + 提示词投影 + 生成策略覆盖。
    [[nodiscard]] auto prepare_request(const reading_input& input) const
        -> std::expected<llm_request, error::error_code>;

    llm_service* llm_;
    reading_options options_{};
};

} // namespace cnetnexus::reading
