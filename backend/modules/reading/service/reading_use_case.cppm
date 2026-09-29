/**
 * @brief Reading application use case: cache, keyed single-flight, generation
 * and persistence.
 */
export module cnetnexus.reading.service.reading_use_case;

import std;
import cnetmod.coro;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.llm_contract;
import cnetnexus.reading.domain.reading_domain;
import cnetnexus.reading.service.reading_cache_service;
import cnetnexus.reading.service.reading_service;

export namespace cnetnexus::reading {

struct reading_use_case_request {
  std::uint64_t user_id{};
  reading_input input;
};

struct reading_use_case_result {
  std::string content;
  std::string model;
  int tokens_in{};
  int tokens_out{};
  bool cached{};
};

/**
 * @brief Coordinates cache lookup, model invocation and cache persistence.
 *
 * Equal user/chart/question identities share a bounded coroutine gate. A
 * waiter rechecks the cache after acquiring it, so only the leader invokes the
 * model while unrelated identities remain concurrent.
 */
class reading_use_case {
public:
  reading_use_case(reading_service &reading,
                   reading_cache_service &cache) noexcept;

  [[nodiscard]] auto execute(const reading_use_case_request &request)
      -> cnetmod::task<std::expected<reading_use_case_result,
                                     cnetnexus::framework::error::error_code>>;

  [[nodiscard]] auto execute_stream(const reading_use_case_request &request,
                                    const llm_delta_sink &sink)
      -> cnetmod::task<std::expected<reading_use_case_result,
                                     cnetnexus::framework::error::error_code>>;

private:
  [[nodiscard]] auto
  cache_request(const reading_use_case_request &request) const
      -> reading_cache_request;
  [[nodiscard]] auto gate_key(const reading_cache_request &request) const
      -> std::string;
  [[nodiscard]] auto lookup(const reading_cache_request &request)
      -> cnetmod::task<std::optional<reading_cache_hit>>;
  [[nodiscard]] auto cached_result(const reading_cache_hit &hit) const
      -> reading_use_case_result;

  reading_service *reading_;
  reading_cache_service *cache_;
  cnetmod::striped_async_mutex<std::string> gates_;
};

} // namespace cnetnexus::reading
