module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chart.controller.reading_routes;

import std;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetnexus.chart.controller.reading_scope;
import cnetnexus.chart.service.chart_reading_service;
import cnetnexus.reading.service.reading_use_case;
import cnetnexus.framework.web.route_handler;

/**
 * @brief 六条 /reading 路由的模块接口：依赖集合、共用工具与逐术数处理器声明。
 */
export namespace cnetnexus::chart::detail {

/** @brief 六条 reading 路由的依赖集合。 */
struct reading_route_context {
  cnetnexus::chart::chart_reading_service *charts{};
  cnetnexus::reading::reading_use_case *reading{};
};

/** @brief 把一次解读交给共用响应器；缓存分区用户取当前登录者，匿名记 0。 */
[[nodiscard]] auto
emit_reading(cnetmod::http::request_context &context,
             const reading_route_context &services, std::string_view algorithm,
             std::string_view report_zh, std::string_view question, bool stream,
             const cnetnexus::chart::reading_scope_request &scope)
    -> cnetmod::task<void>;

/** @brief 解析并校验 ?scope=（失败时直接写出统一错误体并返回 false）。 */
[[nodiscard]] auto accept_scope(cnetmod::http::request_context &context,
                                std::string_view algorithm,
                                std::string_view query_text,
                                cnetnexus::chart::reading_scope_request &scope)
    -> bool;

/** @brief 取问题原文。 */
[[nodiscard]] auto question_of(std::string_view query_text) -> std::string;

/** @brief 解析出生日期时间入参；任一处非法返回 false，由调用方回 400。 */
[[nodiscard]] auto parse_calendar_input(std::string_view query_text, int &year,
                                        int &month, int &day, int &hour,
                                        int &minute, bool &is_male,
                                        bool with_minute) -> bool;

/** @brief 解析 calendar=solar|lunar。 */
[[nodiscard]] auto parse_calendar_kind(std::string_view query_text, bool &lunar)
    -> bool;

[[nodiscard]] auto handle_bazi_reading(cnetmod::http::request_context &context,
                                       const reading_route_context &services)
    -> cnetmod::task<void>;
[[nodiscard]] auto
handle_liu_yao_reading(cnetmod::http::request_context &context,
                       const reading_route_context &services)
    -> cnetmod::task<void>;
[[nodiscard]] auto
handle_mei_hua_reading(cnetmod::http::request_context &context,
                       const reading_route_context &services)
    -> cnetmod::task<void>;
[[nodiscard]] auto
handle_zi_wei_reading(cnetmod::http::request_context &context,
                      const reading_route_context &services)
    -> cnetmod::task<void>;
[[nodiscard]] auto
handle_da_liu_ren_reading(cnetmod::http::request_context &context,
                          const reading_route_context &services)
    -> cnetmod::task<void>;
[[nodiscard]] auto
handle_qi_men_reading(cnetmod::http::request_context &context,
                      const reading_route_context &services)
    -> cnetmod::task<void>;

/** @brief 逐术数注册六条 reading 路由。 */
void declare_reading_handlers(cnetmod::http::router &routes,
                              const reading_route_context &services);

} // namespace cnetnexus::chart::detail

export namespace cnetnexus::chart {

/** @brief 注册大模型解读路由（六条，全部公开已发布契约，不鉴权）。
 *
 *  done 帧里的 algorithm 一律是规范值 ba_zi。缓存分区用户取可选 Bearer
 * 的登录者， 匿名记 0（公开端点不因缺失令牌 401）。模型未装配时统一返回
 * DEPENDENCY.UNAVAILABLE。
 */
void declare_reading_routes(cnetmod::http::router &routes,
                            cnetnexus::chart::chart_reading_service &charts,
                            cnetnexus::reading::reading_use_case &reading);

} // namespace cnetnexus::chart
