module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.service.chart_generation_service;

import std;
import cnetmod.core.time;
import cnetmod.coro.task;
import cnetmod.json;
import cnetnexus.chart.dto.chart;
import cnetnexus.chart.dto.chart_projection;
import cnetnexus.chart.domain.bazi;
import cnetnexus.chart.domain.da_liu_ren;
import cnetnexus.chart.domain.liu_yao;
import cnetnexus.chart.domain.mei_hua;
import cnetnexus.chart.domain.naming;
import cnetnexus.chart.domain.qi_men;
import cnetnexus.chart.domain.zi_wei;
import cnetnexus.chart.service.bazi_service;
import cnetnexus.chart.service.chart_archive_service;
import cnetnexus.chart.service.da_liu_ren_service;
import cnetnexus.chart.service.liu_yao_service;
import cnetnexus.chart.service.mei_hua_service;
import cnetnexus.chart.service.qi_men_service;
import cnetnexus.chart.service.zi_wei_service;
import cnetnexus.framework.error;

namespace cnetnexus::chart {

namespace error = cnetnexus::framework::error;

namespace {

struct generation_dependencies {
  bazi_service *bazi{};
  liu_yao_service *liu_yao{};
  mei_hua_service *mei_hua{};
  zi_wei_service *zi_wei{};
  da_liu_ren_service *da_liu_ren{};
  qi_men_service *qi_men{};
  chart_archive_service *archive{};
};

using batch_item = cnetnexus::chart::dto::chart_batch_item_request;

// 条目上限 max_batch_items 是模块接口导出的具名常量，
// 理由与依据写在那里：批量循环同步且无挂起点，超限请求必须在进入循环前拒绝。

[[nodiscard]] auto
string_or_empty(const std::optional<cnetmod::json::raw_value> &raw)
    -> std::string {
  return dto::scalar_of<std::string>(raw).value_or("");
}

[[nodiscard]] auto lunar_of(const batch_item &item) -> std::optional<bool> {
  if (!item.calendar)
    return false;
  const auto calendar = dto::scalar_of<std::string>(item.calendar);
  if (!calendar)
    return std::nullopt;
  if (calendar->empty() || *calendar == "solar")
    return false;
  if (*calendar == "lunar")
    return true;
  return std::nullopt;
}

[[nodiscard]] auto is_male_of(const batch_item &item) -> std::optional<bool> {
  if (!item.gender)
    return true;
  const auto gender = dto::scalar_of<std::string>(item.gender);
  if (!gender)
    return std::nullopt;
  if (gender->empty() || *gender == "male")
    return true;
  if (*gender == "female")
    return false;
  return std::nullopt;
}

[[nodiscard]] auto changing_of(const batch_item &item)
    -> std::optional<std::vector<int>> {
  if (!item.changing || item.changing->is_null())
    return std::vector<int>{};
  if (item.changing->is_array()) {
    auto values =
        cnetmod::json::from_document<std::vector<int>>(*item.changing);
    if (!values)
      return std::nullopt;
    return std::move(*values);
  }
  const auto text = cnetmod::json::from_document<std::string>(*item.changing);
  if (!text)
    return std::nullopt;
  if (text->empty())
    return std::vector<int>{};
  std::vector<int> values;
  for (const auto part : std::views::split(*text, ',')) {
    const std::string_view token{part.begin(), part.end()};
    int value{};
    const auto [end, ec] =
        std::from_chars(token.data(), token.data() + token.size(), value);
    if (ec != std::errc{} || end != token.data() + token.size())
      return std::nullopt;
    values.push_back(value);
  }
  return values;
}

/// @brief 把 provider 投影失败（JSON 解析/序列化）翻成统一错误码。
template <typename T>
[[nodiscard]] auto as_chart(std::expected<T, std::error_code> projected)
    -> std::expected<T, error::error_code> {
  if (!projected)
    return std::unexpected(error::error_code::system_internal_error);
  return std::move(*projected);
}

using chart_result = std::expected<cnetmod::json::value, error::error_code>;

struct batch_input {
  int year{};
  int month{};
  int day{};
  int hour{};
  int minute{};
  bool lunar{};
  std::string kind;
  std::string question;
};

[[nodiscard]] auto parse_batch_input(const batch_item &item)
    -> std::expected<batch_input, error::error_code> {
  const auto year = dto::scalar_of<int>(item.year);
  const auto month = dto::scalar_of<int>(item.month);
  const auto day = dto::scalar_of<int>(item.day);
  const auto hour = dto::scalar_of<int>(item.hour);
  const auto minute = item.minute
                          ? dto::scalar_of<int>(item.minute)
                          : std::optional<int>{0};
  const auto lunar = lunar_of(item);
  if (!year || !month || !day || !hour || !minute || !lunar)
    return std::unexpected(error::error_code::request_invalid_argument);
  return batch_input{*year,
                     *month,
                     *day,
                     *hour,
                     *minute,
                     *lunar,
                     string_or_empty(item.kind),
                     string_or_empty(item.question)};
}

[[nodiscard]] auto produce_bazi(const generation_dependencies &services,
                                const batch_item &item,
                                const batch_input &values)
    -> cnetmod::task<chart_result> {
  const auto is_male = is_male_of(item);
  if (!is_male)
    co_return std::unexpected(error::error_code::request_invalid_argument);
  cnetnexus::chart::bazi::birth_input input{};
  input.calendar = values.lunar ? cnetnexus::chart::bazi::calendar_kind::lunar
                                : cnetnexus::chart::bazi::calendar_kind::solar;
  input.year = values.year;
  input.month = values.month;
  input.day = values.day;
  input.hour = values.hour;
  input.minute = values.minute;
  input.is_male = *is_male;
  const auto result = co_await services.bazi->pai_pan(input);
  if (!result)
    co_return std::unexpected(result.error());
  co_return as_chart(cnetnexus::chart::projection::chart_response(*result));
}

[[nodiscard]] auto produce_liu_yao(const generation_dependencies &services,
                                   const batch_item &item,
                                   const batch_input &values)
    -> cnetmod::task<chart_result> {
  const auto code = dto::scalar_of<std::string>(item.code);
  const auto changing = changing_of(item);
  if (!code || code->empty() || !changing)
    co_return std::unexpected(error::error_code::request_invalid_argument);
  cnetnexus::chart::liu_yao::cast_input input{};
  input.year = values.year;
  input.month = values.month;
  input.day = values.day;
  input.hour = values.hour;
  input.minute = values.minute;
  input.hexagram_code = *code;
  input.changing_lines = *changing;
  input.kind = values.kind;
  input.question = values.question;
  const auto result = co_await services.liu_yao->cast(input);
  if (!result)
    co_return std::unexpected(result.error());
  co_return as_chart(cnetnexus::chart::projection::chart_response(*result));
}

[[nodiscard]] auto produce_mei_hua(const generation_dependencies &services,
                                   const batch_item &,
                                   const batch_input &values)
    -> cnetmod::task<chart_result> {
  cnetnexus::chart::mei_hua::cast_input input{};
  input.calendar = values.lunar
                       ? cnetnexus::chart::mei_hua::calendar_kind::lunar
                       : cnetnexus::chart::mei_hua::calendar_kind::solar;
  input.year = values.year;
  input.month = values.month;
  input.day = values.day;
  input.hour = values.hour;
  input.minute = values.minute;
  input.kind = values.kind;
  input.question = values.question;
  const auto result = co_await services.mei_hua->cast(input);
  if (!result)
    co_return std::unexpected(result.error());
  co_return as_chart(cnetnexus::chart::projection::chart_response(*result));
}

[[nodiscard]] auto produce_zi_wei(const generation_dependencies &services,
                                  const batch_item &item,
                                  const batch_input &values)
    -> cnetmod::task<chart_result> {
  const auto is_male = is_male_of(item);
  if (values.minute != 0 || !is_male)
    co_return std::unexpected(error::error_code::request_invalid_argument);
  cnetnexus::chart::zi_wei::chart_input input{};
  input.calendar = values.lunar
                       ? cnetnexus::chart::zi_wei::calendar_kind::lunar
                       : cnetnexus::chart::zi_wei::calendar_kind::solar;
  input.year = values.year;
  input.month = values.month;
  input.day = values.day;
  input.hour = values.hour;
  input.is_male = *is_male;
  const auto result = co_await services.zi_wei->pai_pan(input);
  if (!result)
    co_return std::unexpected(result.error());
  co_return as_chart(cnetnexus::chart::projection::chart_response(*result));
}

[[nodiscard]] auto produce_da_liu_ren(const generation_dependencies &services,
                                      const batch_item &item,
                                      const batch_input &values)
    -> cnetmod::task<chart_result> {
  cnetnexus::chart::da_liu_ren::cast_input input{};
  input.year = values.year;
  input.month = values.month;
  input.day = values.day;
  input.hour = values.hour;
  input.minute = values.minute;
  input.kind = values.kind;
  input.question = values.question;
  if (item.gender) {
    const auto gender = dto::scalar_of<std::string>(item.gender);
    if (!gender)
      co_return std::unexpected(error::error_code::request_invalid_argument);
    input.gender = *gender;
  }
  const auto result = co_await services.da_liu_ren->cast(input);
  if (!result)
    co_return std::unexpected(result.error());
  co_return as_chart(cnetnexus::chart::projection::chart_response(*result));
}

[[nodiscard]] auto produce_qi_men(const generation_dependencies &services,
                                  const batch_item &, const batch_input &values)
    -> cnetmod::task<chart_result> {
  cnetnexus::chart::qi_men::chart_input input{};
  input.calendar = values.lunar
                       ? cnetnexus::chart::qi_men::calendar_kind::lunar
                       : cnetnexus::chart::qi_men::calendar_kind::solar;
  input.year = values.year;
  input.month = values.month;
  input.day = values.day;
  input.hour = values.hour;
  input.minute = values.minute;
  input.kind = values.kind;
  input.question = values.question;
  const auto result = co_await services.qi_men->pai_pan(input);
  if (!result)
    co_return std::unexpected(result.error());
  co_return as_chart(cnetnexus::chart::projection::chart_response(*result));
}

using batch_producer = cnetmod::task<chart_result> (*)(const generation_dependencies &,
                                                       const batch_item &,
                                                       const batch_input &);

[[nodiscard]] auto producer_for(std::string_view algorithm) noexcept
    -> batch_producer {
  struct entry {
    std::string_view algorithm;
    batch_producer produce;
  };
  constexpr std::array routes{
      entry{"ba_zi", produce_bazi},
      entry{"liu_yao", produce_liu_yao},
      entry{"mei_hua", produce_mei_hua},
      entry{"zi_wei", produce_zi_wei},
      entry{"da_liu_ren", produce_da_liu_ren},
      entry{"qi_men", produce_qi_men},
  };
  const auto found =
      std::ranges::find_if(routes, [algorithm](const entry &value) {
        return value.algorithm == algorithm;
      });
  return found == routes.end() ? nullptr : found->produce;
}

[[nodiscard]] auto produce_chart(batch_producer produce,
                                 const generation_dependencies &services,
                                 const batch_item &item)
    -> cnetmod::task<chart_result> {
  auto input = parse_batch_input(item);
  if (!input)
    co_return std::unexpected(input.error());
  co_return co_await produce(services, item, *input);
}

[[nodiscard]] auto batch_error(error::error_code code)
    -> cnetnexus::chart::dto::batch_error_response {
  const auto entry = error::find(code).value_or(
      *error::find(error::error_code::system_internal_error));
  return {.code = std::to_string(entry.http_status),
          .status = std::string{entry.name},
          .message = std::string{entry.default_message},
          .retryable = entry.retryable};
}

} // namespace

chart_generation_service::chart_generation_service(bazi_service &bazi,
                             liu_yao_service &liu_yao,
                             mei_hua_service &mei_hua, zi_wei_service &zi_wei,
                             da_liu_ren_service &da_liu_ren,
                             qi_men_service &qi_men,
                             chart_archive_service &archive) noexcept
    : bazi_(&bazi), liu_yao_(&liu_yao), mei_hua_(&mei_hua), zi_wei_(&zi_wei),
      da_liu_ren_(&da_liu_ren), qi_men_(&qi_men), archive_(&archive) {}

auto chart_generation_service::execute(const dto::chart_batch_request &request,
                            std::uint64_t owner_id) const
    -> cnetmod::task<std::expected<dto::chart_batch_response,
                                  error::error_code>> {
  if (!request.algorithm || !request.items)
    co_return std::unexpected(error::error_code::request_invalid_argument);
  if (!batch_within_limit(request.items->size()))
    co_return std::unexpected(error::error_code::request_invalid_argument);
  const auto algorithm = canonical_algorithm(*request.algorithm);
  const auto producer = producer_for(algorithm);
  if (producer == nullptr)
    co_return std::unexpected(error::error_code::algorithm_not_supported);

  dto::chart_batch_response response;
  const generation_dependencies services{bazi_, liu_yao_, mei_hua_, zi_wei_,
                                          da_liu_ren_, qi_men_, archive_};
  response.items.reserve(request.items->size());
  const auto save_archive = request.save_archive.value_or(false);
  for (std::size_t index = 0; index < request.items->size(); ++index) {
    const auto &item = (*request.items)[index];
    dto::chart_batch_item_response entry{
        .index = index,
        .title = string_or_empty(item.title),
        .algorithm = algorithm,
    };
    std::optional<error::error_code> failure;
    auto chart = co_await produce_chart(producer, services, item);
    if (!chart)
      failure = chart.error();

    if (chart && save_archive) {
      dto::chart_batch_archive_document document{
          .algorithm = algorithm,
          .savedAt = cnetmod::unix_time_seconds(),
          .title = entry.title,
          .snapshot = *chart,
      };
      const auto payload = cnetmod::json::to_document(document);
      const auto encoded =
          payload ? cnetmod::json::write_document(*payload)
                  : std::expected<std::string, std::error_code>{
                        std::unexpected(cnetmod::json::make_error_code(
                            cnetmod::json::errc::parse_failed))};
      if (!encoded)
        failure = error::error_code::system_internal_error;
      else {
        const auto saved =
            co_await archive_->archive(algorithm, *encoded, entry.title, owner_id);
        if (saved)
          entry.archive_id = *saved;
        else
          failure = saved.error();
      }
    }

    entry.ok = chart.has_value() && !failure.has_value();
    if (entry.ok) {
      entry.chart = std::move(*chart);
      ++response.succeeded;
    } else {
      entry.error = batch_error(
          failure.value_or(error::error_code::system_internal_error));
      ++response.failed;
    }
    response.items.push_back(std::move(entry));
  }

  co_return response;
}

auto chart_generation_service::archive_bazi(bazi::birth_input input,
                                             std::uint64_t owner_id) const
    -> cnetmod::task<std::expected<dto::archive_created_response,
                                  error::error_code>> {
  const auto chart = co_await bazi_->pai_pan(input);
  if (!chart)
    co_return std::unexpected(chart.error());
  auto snapshot = projection::chart_response(*chart);
  if (!snapshot)
    co_return std::unexpected(error::error_code::system_internal_error);
  dto::archive_snapshot_document document{
      .algorithm = "ba_zi",
      .savedAt = cnetmod::unix_time_seconds(),
      .snapshot = std::move(*snapshot),
  };
  const auto payload = cnetmod::json::to_document(document);
  if (!payload)
    co_return std::unexpected(error::error_code::system_internal_error);
  const auto encoded = cnetmod::json::write_document(*payload);
  if (!encoded)
    co_return std::unexpected(error::error_code::system_internal_error);
  auto id = co_await archive_->archive("ba_zi", *encoded, {}, owner_id);
  if (!id)
    co_return std::unexpected(id.error());
  co_return dto::archive_created_response{.id = std::move(*id),
                                          .algorithm = "ba_zi"};
}

auto batch_limit_violation(std::size_t items) -> std::optional<std::string> {
  if (batch_within_limit(items))
    return std::nullopt;
  return std::format("批量排盘单次最多 {} 条，收到 {} 条", max_batch_items,
                     items);
}

} // namespace cnetnexus::chart
