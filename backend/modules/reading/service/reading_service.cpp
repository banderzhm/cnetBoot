module cnetnexus.reading.service.reading_service;

import std;
import cnetmod.core.log;
import cnetmod.coro.task;
import cnetmod.utils.sha256;
import cnetnexus.framework.error;
import cnetnexus.reading.domain.llm_contract;
import cnetnexus.reading.domain.reading_domain;
import cnetnexus.reading.service.llm_service;

namespace cnetnexus::reading {
namespace {

using error_code = cnetnexus::framework::error::error_code;

/// @brief 去掉首尾 ASCII 空白；全空白返回空 view。
[[nodiscard]] auto trimmed(std::string_view value) -> std::string_view
{
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos)
        return {};
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

} // namespace

auto reading_chart_fingerprint(std::string_view algorithm, std::string_view scope,
    std::string_view report_zh) -> std::string
{
    std::string canonical;
    canonical.reserve(algorithm.size() + scope.size() + report_zh.size() + 2);
    canonical.append(algorithm);
    canonical.push_back('\x1f');
    canonical.append(scope);
    canonical.push_back('\x1f');
    canonical.append(report_zh);
    return cnetmod::utils::sha256_hex(canonical);
}

auto reading_question_hash(std::string_view question) -> std::string
{
    return cnetmod::utils::sha256_hex(trimmed(question));
}

auto reading_service::prepare_request(const reading_input& input) const
    -> std::expected<llm_request, error_code>
{
    auto request = build_reading_request(input);
    if (!request)
        return std::unexpected{request.error()};

    // 未装配可用的补全实现：返回 DEPENDENCY.UNAVAILABLE，不静默降级成本地规则。
    if (!llm_->configured())
        return std::unexpected{error_code::upstream_unavailable};

    // 覆盖提示词里的默认值：这两个参数决定断语延迟。
    request->max_tokens = options_.max_tokens;
    request->reasoning_effort = options_.reasoning_effort;
    // 模型覆盖：空表示仍由补全实现取配置默认值；提示词内容与它无关。
    request->model = options_.model;
    return request;
}

auto reading_service::read(const reading_input& input) const
    -> cnetmod::task<std::expected<std::string, error_code>>
{
    auto request = prepare_request(input);
    if (!request)
        co_return std::unexpected{request.error()};

    auto completion = co_await llm_->complete(*request);
    if (!completion)
    {
        // 失败原文已在补全实现里留日志（不含提示词正文与密钥）；这里只补规模信息与错误码。
        logger::warn{"reading failed: algorithm={} report_chars={} question_chars={} error={}",
            input.algorithm, input.report_zh.size(), input.question.size(),
            static_cast<int>(completion.error())};
        co_return std::unexpected{completion.error()};
    }
    co_return std::move(completion->content);
}

auto reading_service::read_stream(const reading_input& input, const llm_delta_sink& sink) const
    -> cnetmod::task<std::expected<llm_completion, error_code>>
{
    auto request = prepare_request(input);
    if (!request)
        co_return std::unexpected{request.error()};

    // 与非流式同一份 request：提示词逐字节一致，只是输出改成逐帧。
    auto completion = co_await llm_->stream(*request, sink);
    if (!completion)
    {
        logger::warn{"reading stream failed: algorithm={} report_chars={} question_chars={} error={}",
            input.algorithm, input.report_zh.size(), input.question.size(),
            static_cast<int>(completion.error())};
        co_return std::unexpected{completion.error()};
    }
    co_return std::move(*completion);
}

} // namespace cnetnexus::reading
