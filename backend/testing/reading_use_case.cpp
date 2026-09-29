import std;
import cnetmod.coro;
import cnetmod.io;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.llm_contract;
import cnetnexus.reading.mapper.reading_cache_mapper;
import cnetnexus.reading.service.llm_service;
import cnetnexus.reading.service.reading_cache_service;
import cnetnexus.reading.service.reading_service;
import cnetnexus.reading.service.reading_use_case;

namespace {

using error_code = cnetnexus::framework::error::error_code;

class memory_cache_mapper final
    : public cnetnexus::reading::reading_cache_mapper {
public:
  auto find(const cnetnexus::reading::reading_cache_key &) -> cnetmod::task<
      std::expected<std::optional<cnetnexus::reading::reading_cache_row>,
                    error_code>> override {
    co_return row;
  }

  auto upsert(const cnetnexus::reading::reading_cache_key &,
              const cnetnexus::reading::reading_cache_row &value)
      -> cnetmod::task<std::expected<void, error_code>> override {
    row = value;
    co_return {};
  }

  auto record_hit(const cnetnexus::reading::reading_cache_key &)
      -> cnetmod::task<std::expected<void, error_code>> override {
    ++hits;
    co_return {};
  }

  std::optional<cnetnexus::reading::reading_cache_row> row;
  int hits{};
};

class delayed_llm final : public cnetnexus::reading::llm_service {
public:
  explicit delayed_llm(cnetmod::io_context &io) noexcept : io_{&io} {}

  auto complete(const cnetnexus::reading::llm_request &) -> cnetmod::task<
      std::expected<cnetnexus::reading::llm_completion, error_code>> override {
    ++calls;
    co_await cnetmod::async_sleep(*io_, std::chrono::milliseconds{2});
    co_return cnetnexus::reading::llm_completion{.content = "single-flight"};
  }

  cnetmod::io_context *io_;
  int calls{};
};

} // namespace

auto main() -> int {
  auto io = cnetmod::make_io_context();
  if (!io)
    return 1;
  int result = 1;

  auto run = [&]() -> cnetmod::task<void> {
    memory_cache_mapper mapper;
    cnetnexus::reading::reading_cache_service cache{mapper};
    delayed_llm llm{*io};
    cnetnexus::reading::reading_service generator{llm};
    cnetnexus::reading::reading_use_case use_case{generator, cache};
    const cnetnexus::reading::reading_use_case_request request{
        .user_id = 42,
        .input = {.algorithm = "ba_zi",
                  .report_zh = "四柱：庚辰 壬午 甲辰 壬申",
                  .question = "事业如何"}};

    auto [first, second] = co_await cnetmod::when_all(
        use_case.execute(request), use_case.execute(request));
    if (!first || !second || first->content != "single-flight" ||
        second->content != "single-flight" || llm.calls != 1 ||
        first->cached == second->cached || mapper.hits != 1) {
      result = 2;
      io->stop();
      co_return;
    }
    result = 0;
    io->stop();
  };
  cnetmod::spawn(*io, run());
  io->run();
  return result;
}
