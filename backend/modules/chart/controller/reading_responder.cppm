module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chart.controller.reading_responder;

import std;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetnexus.chart.controller.reading_scope;
import cnetnexus.chart.service.chart_reading_service;
import cnetnexus.reading.service.reading_use_case;

export namespace cnetnexus::chart {

/** @brief 一次断语响应的输入；六个 /reading 与 /cases/:id/reading 共用。 */
struct reading_response_request {
  std::uint64_t user_id{0}; ///< 缓存分区：登录用户 id；0 = 匿名
  std::string algorithm;    ///< 规范术数标识（done 帧与缓存键都用它）
  std::string report_zh;    ///< 确定性中文报告（盘面指纹的输入）
  std::string question;     ///< 用户问题
  bool stream{false};       ///< true = text/event-stream；false = 一次性 JSON

  /** @brief 生成范围（默认仅本命）；canonical 串进缓存键并回显在 done 帧 /
   * 一次性 JSON。 */
  cnetnexus::chart::reading_scope_request scope{};
};

/**
 * @brief 断语响应的完整流程（六条 reading 与案例断语共用，不再各写一套）。
 *
 *  1. 先查断语缓存（键 = 用户 + 算法 + 盘面指纹 + 所问指纹，engine_version
 * 不一致按未命中）： 命中完全不调模型 —— stream=true 只下发一帧 event:
 * done（cached:true）后收尾， stream=false 直接返回一次性 JSON 并追加 "cached":
 * true。
 *  2. 未命中：流式逐帧 delta / 一次性返回，成功后 UPSERT 回填缓存。
 *  3. 失败语义：首帧之前失败仍按普通 HTTP 错误返回；已经下发过增量后失败补
 * error 帧收尾。
 *  4. 缓存读写故障不阻断断语：查缓存失败按未命中继续。
 *
 *  @param on_content
 * 命中或生成成功后的回调（案例断语用它把正文写回案例）；可为空
 */
[[nodiscard]] auto
respond_reading(cnetmod::http::request_context &context,
                cnetnexus::reading::reading_use_case &reading,
                const reading_response_request &request,
                const std::function<void(std::string_view)> &on_content = {})
    -> cnetmod::task<void>;

/** @brief 把模型输出交给调用方（案例断语回写正文用）。 */
[[nodiscard]] auto reading_answer_text(std::string_view content) -> std::string;

} // namespace cnetnexus::chart
