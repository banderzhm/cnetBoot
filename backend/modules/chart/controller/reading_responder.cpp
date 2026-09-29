module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.reading_responder;

import std;
import cnetnexus.framework.web.response;
import cnetmod.coro.task;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetnexus.chart.controller.reading_scope;
import cnetnexus.chart.service.chart_reading_service;
import cnetnexus.chart.controller.route_support;
import cnetnexus.framework.error;
import cnetnexus.framework.web.error_response;
import cnetnexus.framework.web.sse_stream;
import cnetnexus.reading.domain.reading_domain;
import cnetnexus.reading.service.reading_use_case;

namespace cnetnexus::chart {

namespace error = cnetnexus::framework::error;
namespace reading = cnetnexus::reading;
namespace web = cnetnexus::framework::web;

namespace dto {

/// @brief 一次性 JSON 响应体：{algorithm,scope,question,answer}，命中再追加
/// cached:true。
struct reading_response_dto {
  std::string algorithm;
  std::string scope;
  std::string question;
  std::string answer;
  std::optional<bool> cached;
};

/// @brief done 帧载荷：补判语与模型计量。
struct reading_done_dto {
  std::string algorithm;
  std::string scope;
  std::string content;
  int tokensIn{};
  int tokensOut{};
  bool cached{};
  std::string model;
};

} // namespace dto

namespace {

/// @brief 一次性 JSON 响应体：scope 是规范化生成范围串，命中时追加
/// cached:true。
void respond_json(cnetmod::http::request_context &context,
                  std::string_view algorithm, std::string_view scope,
                  std::string_view question, std::string_view answer,
                  bool cached) {
  web::respond_json(context,
                    dto::reading_response_dto{
                        .algorithm = std::string{algorithm},
                        .scope = std::string{scope},
                        .question = std::string{question},
                        .answer = std::string{answer},
                        .cached = cached ? std::optional<bool>{true}
                                         : std::nullopt,
                    });
}

[[nodiscard]] auto done_payload(std::string_view algorithm,
                                std::string_view scope,
                                std::string_view content, int tokens_in,
                                int tokens_out, bool cached,
                                std::string_view model) -> std::string {
  auto encoded = cnetmod::json::write(
      dto::reading_done_dto{.algorithm = std::string{algorithm},
                            .scope = std::string{scope},
                            .content = std::string{content},
                            .tokensIn = tokens_in,
                            .tokensOut = tokens_out,
                            .cached = cached,
                            .model = std::string{model}});
  return encoded ? std::move(*encoded) : std::string{"{}"};
}

} // namespace

auto reading_answer_text(std::string_view content) -> std::string {
  return std::string{content};
}

auto respond_reading(cnetmod::http::request_context &context,
                     reading::reading_use_case &use_case,
                     const reading_response_request &request,
                     const std::function<void(std::string_view)> &on_content)
    -> cnetmod::task<void> {
  // 生成范围：canonical 串同时进提示词、缓存键与响应回显，三处口径只此一处。
  const auto scope = request.scope.scopes.canonical();

  reading::reading_input input{};
  input.algorithm = request.algorithm;
  input.report_zh = request.report_zh;
  input.question = request.question;
  input.scopes = request.scope.scopes;
  input.target_year = request.scope.target_year;
  input.target_lunar_month = request.scope.target_lunar_month;

  const reading::reading_use_case_request use_case_request{
      .user_id = request.user_id, .input = std::move(input)};

  if (!request.stream) {
    const auto result = co_await use_case.execute(use_case_request);
    if (!result) {
      web::respond_error(context, result.error());
      co_return;
    }
    if (on_content)
      on_content(result->content);
    respond_json(context, request.algorithm, scope, request.question,
                 result->content, result->cached);
    co_return;
  }

  co_await context.with_sse(
      [&](cnetmod::http::request_context &stream_context,
          cnetmod::http::sse_stream &stream) -> cnetmod::task<void> {
        web::sse_writer writer{stream};
        const auto result = co_await use_case.execute_stream(
            use_case_request, web::sse_delta_callback(writer));
        if (!result) {
          // Before the first frame, ordinary HTTP errors remain possible.
          if (!writer.started()) {
            web::respond_error(stream_context, result.error());
            co_return;
          }
          (void)co_await writer.error(result.error());
          (void)co_await writer.finish();
          co_return;
        }
        if (on_content)
          on_content(result->content);
        if (co_await writer.done(done_payload(
                request.algorithm, scope, result->content, result->tokens_in,
                result->tokens_out, result->cached, result->model)))
          (void)co_await writer.finish();
      });
  co_return;
}

} // namespace cnetnexus::chart
