module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.service.chart_reading_service;

import std;
import cnetmod.coro.task;
import cnetnexus.chart.domain.bazi;
import cnetnexus.chart.domain.da_liu_ren;
import cnetnexus.chart.domain.liu_yao;
import cnetnexus.chart.domain.mei_hua;
import cnetnexus.chart.domain.qi_men;
import cnetnexus.chart.domain.zi_wei;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.reading_domain;

namespace cnetnexus::chart {

using error_code = cnetnexus::framework::error::error_code;
using cnetnexus::reading::reading_scope;

auto reading_scope_request::uses_target_projection() const noexcept -> bool {
  return scopes.contains(reading_scope::dayun) ||
         scopes.contains(reading_scope::liunian) ||
         scopes.contains(reading_scope::liuyue);
}

chart_reading_service::chart_reading_service(
    bazi_service &bazi, liu_yao_service &liu_yao,
    mei_hua_service &mei_hua, zi_wei_service &zi_wei,
    da_liu_ren_service &da_liu_ren, qi_men_service &qi_men) noexcept
    : bazi_(&bazi), liu_yao_(&liu_yao), mei_hua_(&mei_hua),
      zi_wei_(&zi_wei), da_liu_ren_(&da_liu_ren), qi_men_(&qi_men) {}

auto chart_reading_service::prepare(bazi::birth_input input,
    const reading_scope_request &scope, std::string question) const
    -> cnetmod::task<std::expected<chart_reading_material, error_code>> {
  const auto chart = co_await bazi_->pai_pan(input);
  if (!chart)
    co_return std::unexpected(chart.error());
  std::string period_tables;
  std::string scope_summary;
  if (scope.uses_target_projection() && scope.target_year) {
    bazi::liu_nian_input target{};
    target.birth = input;
    target.target_year = *scope.target_year;
    target.target_lunar_month = scope.target_lunar_month;
    const auto projection = co_await bazi_->liu_nian(target);
    if (!projection)
      co_return std::unexpected(projection.error());
    period_tables = projection->period_zh;
    scope_summary = bazi_scope_section(chart->chart_projection,
        &projection->chart_projection, scope);
  } else {
    scope_summary = bazi_scope_section(chart->chart_projection, nullptr, scope);
  }
  co_return chart_reading_material{"ba_zi",
      chart->chart_zh + period_tables + chart->analysis_zh + scope_summary,
      std::move(question)};
}

auto chart_reading_service::prepare(liu_yao::cast_input input) const
    -> cnetmod::task<std::expected<chart_reading_material, error_code>> {
  const auto result = co_await liu_yao_->cast(input);
  if (!result)
    co_return std::unexpected(result.error());
  co_return chart_reading_material{"liu_yao", result->analysis_zh,
      std::move(input.question)};
}

auto chart_reading_service::prepare(mei_hua::cast_input input) const
    -> cnetmod::task<std::expected<chart_reading_material, error_code>> {
  const auto result = co_await mei_hua_->cast(input);
  if (!result)
    co_return std::unexpected(result.error());
  co_return chart_reading_material{"mei_hua", result->analysis_zh,
      std::move(input.question)};
}

auto chart_reading_service::prepare(zi_wei::chart_input input,
    const reading_scope_request &scope, std::string question) const
    -> cnetmod::task<std::expected<chart_reading_material, error_code>> {
  if (scope.scopes.contains(reading_scope::liuyue)) {
    input.target_year = *scope.target_year;
    input.target_lunar_month = *scope.target_lunar_month;
    input.target_lunar_day = scope.target_lunar_day.value_or(1);
    input.target_hour = scope.target_hour.value_or("子");
  } else if (scope.uses_target_projection() && scope.target_year) {
    input.target_year = scope.target_year;
  }
  const auto chart = co_await zi_wei_->pai_pan(input);
  if (!chart)
    co_return std::unexpected(chart.error());
  const auto scope_data = zi_wei_scope_section(
      chart->chart_projection,
      chart->horoscope_projection ? std::addressof(*chart->horoscope_projection)
                                  : nullptr,
      scope);
  co_return chart_reading_material{"zi_wei", chart->report_zh + scope_data,
      std::move(question)};
}

auto chart_reading_service::prepare(da_liu_ren::cast_input input) const
    -> cnetmod::task<std::expected<chart_reading_material, error_code>> {
  const auto result = co_await da_liu_ren_->cast(input);
  if (!result)
    co_return std::unexpected(result.error());
  co_return chart_reading_material{"da_liu_ren", result->analysis_zh,
      std::move(input.question)};
}

auto chart_reading_service::prepare(qi_men::chart_input input) const
    -> cnetmod::task<std::expected<chart_reading_material, error_code>> {
  const auto result = co_await qi_men_->pai_pan(input);
  if (!result)
    co_return std::unexpected(result.error());
  co_return chart_reading_material{"qi_men", result->analysis_zh,
      std::move(input.question)};
}

} // namespace cnetnexus::chart
