export module cnetnexus.chart.service.bazi_service;

import std;
import cnetnexus.chart.service.compute_executor;
import cnetmod.coro.task;
import cnetmod.protocol.redis;
import cnetnexus.chart.domain.bazi;
import cnetnexus.framework.error;
import cnetnexus.chart.service.algorithm_api;

export namespace cnetnexus::chart {

/** @brief 命盘缓存存活时间：盘面只由出生信息决定，10 分钟内重复请求直接命中。 */
inline constexpr std::chrono::seconds bazi_cache_ttl{std::chrono::minutes{10}};

/** @brief 排盘用例：先做请求范围校验，再查命盘缓存，未命中时委派给算法接口。
 *
 *  缓存直接使用 Application 托管 redis_service 生成的 redis_template（由组合根
 *  以 cnetnexus:bazi: 命名空间注入；未配置时为 nullopt，不缓存），命盘经
 *  cnetmod.json（Glaze）序列化。结构变化后旧值解码失败即按未命中处理并被覆盖；
 *  Redis 不可用只降级为直接排盘，不让请求失败。
 */
class bazi_service
{
public:
    /** @brief 构造排盘用例。
     *  @param algorithm 排盘算法（同步纯函数）
     *  @param compute 计算执行器：排盘在它上面执行，不占用请求事件循环
     *  @param cache 命盘缓存；std::nullopt 表示不缓存
     */
    bazi_service(cnetnexus::chart::bazi_algorithm& algorithm,
        cnetnexus::chart::compute_executor& compute,
        std::optional<cnetmod::redis::redis_template> cache = std::nullopt) noexcept
        : algorithm_{&algorithm},
          compute_{&compute},
          cache_{std::move(cache)}
    {
    }

    /** @brief 排命盘。
     *  @param input 出生信息
     *  @return 命盘；参数越界返回 REQUEST.INVALID_ARGUMENT，算法失败沿用算法接口返回的错误码
     */
    [[nodiscard]] auto pai_pan(cnetnexus::chart::bazi::birth_input input)
        -> cnetmod::task<std::expected<cnetnexus::chart::bazi::chart,
            cnetnexus::framework::error::error_code>>;

    /** @brief 取目标公历年的流年盘面（含"今年"角标，跨年即变，因此不缓存）。
     *  @param input 出生信息 + 目标公历年 + 可选大运序号
     *  @return 流年盘面；出生信息或目标年/大运序号越界返回 REQUEST.INVALID_ARGUMENT，
     *  算法失败沿用算法接口返回的错误码
     */
    [[nodiscard]] auto liu_nian(const cnetnexus::chart::bazi::liu_nian_input& input) const
        -> cnetmod::task<std::expected<cnetnexus::chart::bazi::liu_nian_result,
            cnetnexus::framework::error::error_code>>;

    /** @brief 命盘缓存键；流派进键，盲派与子平的盘面不同，不能互相命中。 */
    [[nodiscard]] static auto cache_key(const cnetnexus::chart::bazi::birth_input& input)
        -> std::string;

private:
    cnetnexus::chart::bazi_algorithm* algorithm_;
    cnetnexus::chart::compute_executor* compute_;
    std::optional<cnetmod::redis::redis_template> cache_;
};

} // namespace cnetnexus::chart
