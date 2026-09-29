module cnetnexus.reading.service.reading_use_case;

import std;
import cnetmod.core.log;
import cnetmod.coro;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.llm_contract;
import cnetnexus.reading.domain.reading_domain;
import cnetnexus.reading.service.reading_cache_service;
import cnetnexus.reading.service.reading_service;

namespace cnetnexus::reading {

namespace error = cnetnexus::framework::error;

reading_use_case::reading_use_case(reading_service &reading,
                                   reading_cache_service &cache) noexcept
    : reading_{&reading}, cache_{&cache} {}

auto reading_use_case::cache_request(
    const reading_use_case_request &request) const -> reading_cache_request {
  return reading_cache_request{.user_id = request.user_id,
                               .algorithm = request.input.algorithm,
                               .scope = request.input.scopes.canonical(),
                               .report_zh = request.input.report_zh,
                               .question = request.input.question};
}

auto reading_use_case::gate_key(const reading_cache_request &request) const
    -> std::string {
  return std::format("{}:{}:{}:{}", request.user_id, request.algorithm,
                     reading_chart_fingerprint(request.algorithm, request.scope,
                                               request.report_zh),
                     reading_question_hash(request.question));
}

auto reading_use_case::lookup(const reading_cache_request &request)
    -> cnetmod::task<std::optional<reading_cache_hit>> {
  const auto found = co_await cache_->lookup(request);
  if (!found) {
    logger::warn{"reading cache lookup failed; falling back to the model: "
                 "algorithm={} user={} error={}",
                 request.algorithm, request.user_id,
                 static_cast<int>(found.error())};
    co_return std::nullopt;
  }
  co_return *found;
}

auto reading_use_case::cached_result(const reading_cache_hit &hit) const
    -> reading_use_case_result {
  return reading_use_case_result{.content = hit.content,
                                 .model = hit.model,
                                 .tokens_in = hit.tokens_in,
                                 .tokens_out = hit.tokens_out,
                                 .cached = true};
}

auto reading_use_case::execute(const reading_use_case_request &request)
    -> cnetmod::task<
        std::expected<reading_use_case_result, error::error_code>> {
  const auto key = cache_request(request);
  if (const auto hit = co_await lookup(key))
    co_return cached_result(*hit);

  auto gate = co_await gates_.lock(gate_key(key));
  if (const auto hit = co_await lookup(key))
    co_return cached_result(*hit);

  const auto answer = co_await reading_->read(request.input);
  if (!answer)
    co_return std::unexpected{answer.error()};

  reading_cache_hit stored{.content = *answer,
                           .model = reading_->options().model};
  (void)co_await cache_->save(key, stored);
  co_return reading_use_case_result{.content = std::move(stored.content),
                                    .model = std::move(stored.model)};
}

auto reading_use_case::execute_stream(const reading_use_case_request &request,
                                      const llm_delta_sink &sink)
    -> cnetmod::task<
        std::expected<reading_use_case_result, error::error_code>> {
  const auto key = cache_request(request);
  if (const auto hit = co_await lookup(key))
    co_return cached_result(*hit);

  auto gate = co_await gates_.lock(gate_key(key));
  if (const auto hit = co_await lookup(key))
    co_return cached_result(*hit);

  const auto answer = co_await reading_->read_stream(request.input, sink);
  if (!answer)
    co_return std::unexpected{answer.error()};

  reading_cache_hit stored{.content = answer->content,
                           .model = reading_->options().model,
                           .tokens_in = answer->tokens_in,
                           .tokens_out = answer->tokens_out};
  (void)co_await cache_->save(key, stored);
  co_return reading_use_case_result{.content = std::move(stored.content),
                                    .model = std::move(stored.model),
                                    .tokens_in = stored.tokens_in,
                                    .tokens_out = stored.tokens_out};
}

} // namespace cnetnexus::reading
