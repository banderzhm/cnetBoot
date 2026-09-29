export module cnetboot.framework.web.sse_stream;

import std;
import cnetmod.coro.task;
import cnetmod.protocol.http;
import cnetboot.framework.error;
import cnetboot.framework.web.query;

export namespace cnetboot::framework::web {

/** @brief delta 事件的 data 载荷：{"text":"增量"}。 */
[[nodiscard]] auto sse_delta_payload(std::string_view text) -> std::string;

/** @brief error 事件的 data 载荷：与 HTTP 失败响应同一套统一错误体
 * {code,status,message,retryable}。
 */
[[nodiscard]] auto sse_error_payload(cnetboot::framework::error::error_code code) -> std::string;

/** @brief SSE 事件流写出器：承载通用 AI 流式响应的具名事件契约。
 *
 *  **分工：线上帧字节全部由 cnetmod 框架产出，本类只承载业务契约。**
 *  - 帧编码（`event:` / `data:` 行与空行分隔符）：request_context::sse_send →
 *    http::sse::data（third_party/cnetmod/src/protocol/http/extension/sse.cpp）；
 *  - 响应头（Content-Type / Cache-Control / Connection: keep-alive / X-Accel-Buffering /
 *    X-Streamed，并去掉 Content-Length）：sse_send 与 sse_begin 内部的 http::sse::prepare；
 *  - 终止帧 `data: {"done":true}`：request_context::sse_done → http::sse::done；
 *  - 「是否已开始」：request_context::sse_started()（router.cppm:82）。
 *  本模块**不得**出现任何帧文本字面量；新增帧类型只能经 write_event → sse_send。
 *
 *  本类保留的业务语义（前端已按此实现，不得改动）：
 *  - 具名事件 delta / done / error，data 是 JSON 载荷（delta 与 error 见上面的共用函数；
 *    done 的载荷由各调用方按自己的契约构造，chat 是 messageId/content/tokens*，
 *    不同业务可以定义自己的 done DTO）；
 *  - finish() 写框架的无名终止帧收尾；连接按响应头的 keep-alive 保持（不发心跳帧，与现状一致）；
 *  - 惰性开始：首帧写入延迟到第一次写出时才发生（sse_send 内部会先写 200 + SSE 响应头），
 *    调用方据此可以在「首帧之前失败」时退回去写普通 HTTP 错误响应；
 *    一旦写过任何一帧（started() == true）就不能再回退，只能补 error 帧收尾。
 *
 *  started() 不再由本类镜像：直接转发框架状态 request_context::sse_started()，
 *  判据是「已尝试写出 SSE 响应头/帧」（框架在写响应头之前就把状态置为 committing，
 *  写失败停在 failed，两者都算 started），比「已成功写出」更保守。
 *  与旧本地镜像唯一的差异路径是 begin() 写响应头失败：旧实现把 started_ 留成 false，
 *  框架停在 failed（sse_started() == true）。调用方应据此避免在已提交后改写普通响应。
 */
class sse_writer
{
public:
    explicit sse_writer(cnetmod::http::sse_stream& stream) noexcept;

    /** @brief 是否已尝试写出 SSE 响应头/帧（转发框架 request_context::sse_started()）；
     *  用于失败时决定回退普通 JSON 还是补 error 帧。 */
    [[nodiscard]] auto started() const noexcept -> bool;

    /** @brief 显式开始事件流（200 + SSE 响应头）。
     *  开始后失败必须写 error 帧；尚未开始时仍可返回普通 HTTP 错误。
     */
    auto begin() -> cnetmod::task<bool>;

    /** @brief 写出一帧 event: delta。返回 false 表示下游断开。 */
    auto delta(std::string_view text) -> cnetmod::task<bool>;

    /** @brief 写出一帧 event: done；payload 由调用方按自身契约构造。 */
    auto done(std::string_view payload) -> cnetmod::task<bool>;

    /** @brief 写出一帧 event: error（统一错误体）。 */
    auto error(cnetboot::framework::error::error_code code) -> cnetmod::task<bool>;

    /** @brief 写出无语义终止帧 data: {"done":true}，结束本次事件流。 */
    auto finish() -> cnetmod::task<bool>;

private:
    /** @brief 具名事件的唯一出口：交给框架编码并写线（sse_send → http::sse::data）。
     *  三个具名事件共用它；开始判据由框架在 sse_begin 内部置位，本类不再维护镜像状态。
     */
    auto write_event(std::string_view payload, std::string_view event) -> cnetmod::task<bool>;

    cnetmod::http::sse_stream* stream_; ///< 由 request_context::with_sse 管理生命周期
};

/** @brief 把写出器绑成增量回调（按引用捕获；started() 判据落在同一个请求上下文的框架状态上）。 */
[[nodiscard]] auto sse_delta_callback(sse_writer& writer) -> std::function<cnetmod::task<bool>(std::string_view)>;

/** @brief 解析 ?stream= 开关（断语与 chat 的查询参数口径统一在这里）。
 *
 *  缺省 / 空 / false / 0 表示一次性响应；true / 1 表示 SSE。其余取值按
 *  REQUEST.INVALID_ARGUMENT 处理（严格，不静默当成 false）。
 */
[[nodiscard]] auto sse_requested(std::string_view query) -> std::expected<bool, cnetboot::framework::error::error_code>;

} // namespace cnetboot::framework::web
