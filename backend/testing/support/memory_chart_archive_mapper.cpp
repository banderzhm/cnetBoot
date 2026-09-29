module cnetnexus.chart.mapper.memory_chart_case_mapper;

import std;
import cnetmod.core.time;
import cnetmod.coro;
import cnetnexus.chart.domain.archive;
import cnetnexus.chart.domain.cases;
import cnetnexus.chart.domain.naming;
import cnetnexus.chart.domain.stats;
import cnetnexus.framework.error;
import cnetnexus.framework.pagination;

namespace cnetnexus::chart {
namespace {

using error_code = cnetnexus::framework::error::error_code;

namespace chart = cnetnexus::chart;
using error_code = cnetnexus::framework::error::error_code;

/// @brief ASCII 大小写不敏感的包含判断（中文按原字节比较）。
auto contains_ci(std::string_view haystack, std::string_view needle) -> bool
{
    if (needle.empty())
        return true;
    const auto lower = [](char c)
    {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    };
    return std::ranges::search(haystack, needle, [lower](char left, char right)
               {
                   return lower(left) == lower(right);
               })
               .begin() != haystack.end();
}

/// @brief 分页切片；offset 超出总数返回空。
template <typename T>
auto slice(const std::vector<T>& values,
    const cnetnexus::framework::pagination::page_request& page)
    -> std::vector<T>
{
    const auto offset =
        static_cast<std::size_t>((page.page - 1) * page.page_size);
    std::vector<T> result;
    for (std::size_t i = offset;
        i < values.size() &&
        result.size() < static_cast<std::size_t>(page.page_size);
        ++i)
        result.push_back(values[i]);
    return result;
}

/// @brief 按 id 取标签；找不到返回 nullptr。
auto find_tag_by_id(const std::vector<chart::chart_tag>& tags, std::uint64_t id)
    -> const chart::chart_tag*
{
    const auto found = std::ranges::find_if(
        tags, [id](const chart::chart_tag& item)
        {
            return item.id == id;
        });
    return found == tags.end() ? nullptr : &*found;
}

} // namespace

/// @brief owner 分区可见性；与 ChartMapper.xml 的 archiveFilters 谓词同口径：
///  启用但两个条件都为空时表示「看不到任何行」（恒假），绝不放行全表。
auto archive_owner_visible(const chart::archive_owner_filter& filter,
    std::uint64_t owner_id) -> bool
{
    if (!filter.active)
        return true;
    if (filter.include_unowned &&
        owner_id == chart::archive_access::anonymous_owner_id)
        return true;
    return std::ranges::find(filter.owner_ids, owner_id) !=
        filter.owner_ids.end();
}
// ============================== chart_archive_index_mapper
// ==============================

auto memory_chart_archive_mapper::save(const chart::archive_snapshot& value)
    -> cnetmod::task<std::expected<void, error_code>>
{
    if (value.metadata.archive_id.empty() || value.payload.empty())
        co_return std::unexpected{error_code::request_invalid_argument};

    const std::scoped_lock lock{db_->mutex_};
    const auto found = std::ranges::find_if(
        db_->archives_, [&value](const chart::archive_snapshot& item)
        {
            return item.metadata.archive_id == value.metadata.archive_id;
        });
    if (found != db_->archives_.end())
        *found = value;
    else
        db_->archives_.push_back(value);
    co_return std::expected<void, error_code>{};
}

auto memory_chart_archive_mapper::remove(std::string_view archive_id)
    -> cnetmod::task<std::expected<void, error_code>>
{
    const std::scoped_lock lock{db_->mutex_};
    std::erase_if(db_->archives_,
        [archive_id](const chart::archive_snapshot& item)
        {
            return item.metadata.archive_id == archive_id;
        });
    co_return std::expected<void, error_code>{};
}

auto memory_chart_archive_mapper::find(std::string_view archive_id)
    -> cnetmod::task<std::expected<
        std::optional<chart::archive_index_entry>, error_code>>
{
    const std::scoped_lock lock{db_->mutex_};
    const auto found = std::ranges::find_if(db_->archives_,
        [archive_id](const chart::archive_snapshot& item)
        {
            return item.metadata.archive_id == archive_id;
        });
    if (found == db_->archives_.end())
        co_return std::optional<chart::archive_index_entry>{};
    co_return std::optional<chart::archive_index_entry>{found->metadata};
}

auto memory_chart_archive_mapper::load(std::string_view archive_id)
    -> cnetmod::task<std::expected<std::optional<chart::archive_snapshot>, error_code>>
{
    const std::scoped_lock lock{db_->mutex_};
    const auto found = std::ranges::find_if(db_->archives_,
        [archive_id](const chart::archive_snapshot& item)
        {
            return item.metadata.archive_id == archive_id;
        });
    if (found == db_->archives_.end())
        co_return std::optional<chart::archive_snapshot>{};
    co_return std::optional<chart::archive_snapshot>{*found};
}

auto memory_chart_archive_mapper::search(const chart::archive_query& query)
    -> cnetmod::task<std::expected<
        cnetnexus::framework::pagination::page_result<chart::archive_index_entry>,
        error_code>>
{
    const std::scoped_lock lock{db_->mutex_};
    const auto page = cnetnexus::framework::pagination::normalize(query.page);

    std::vector<chart::archive_index_entry> filtered;
    for (const auto& value : db_->archives_)
    {
        const auto& candidate = value.metadata;
        if (!query.algorithm.empty() && candidate.algorithm != query.algorithm)
            continue;
        if (!query.keyword.empty() && !contains_ci(candidate.title, query.keyword))
            continue;
        if (query.from != 0 && candidate.created_at < query.from)
            continue;
        if (query.to != 0 && candidate.created_at > query.to)
            continue;
        // P0-1：owner 分区过滤（与 ChartMapper.xml 的 archiveFilters 同口径）。
        if (query.owner.active &&
            !chart::archive_owner_visible(query.owner, candidate.owner_id))
            continue;
        filtered.push_back(candidate);
    }

    std::ranges::sort(filtered, [](const chart::archive_index_entry& left, const chart::archive_index_entry& right)
        {
            if (left.created_at != right.created_at)
                return left.created_at > right.created_at;
            return left.archive_id > right.archive_id;
        });

    cnetnexus::framework::pagination::page_result<chart::archive_index_entry> result;
    result.total = static_cast<std::int64_t>(filtered.size());
    result.page = page.page;
    result.page_size = page.page_size;
    result.items = slice(filtered, page);
    co_return result;
}

auto memory_chart_archive_mapper::counts_by_algorithm() -> cnetmod::task<
    std::expected<std::vector<chart::algorithm_count>, error_code>>
{
    const std::scoped_lock lock{db_->mutex_};
    std::map<std::string, std::uint64_t, std::less<>> grouped;
    for (const auto& entry : db_->archives_)
        ++grouped[entry.metadata.algorithm];

    std::vector<chart::algorithm_count> result;
    result.reserve(grouped.size());
    for (const auto& [algorithm, count] : grouped)
        result.push_back(
            chart::algorithm_count{.algorithm = algorithm, .count = count});
    co_return result;
}

auto memory_chart_archive_mapper::count_since(std::int64_t from, std::int64_t to)
    -> cnetmod::task<std::expected<std::uint64_t, error_code>>
{
    const std::scoped_lock lock{db_->mutex_};
    std::uint64_t total = 0;
    for (const auto& value : db_->archives_)
    {
        const auto& entry = value.metadata;
        if (from != 0 && entry.created_at < from)
            continue;
        if (to != 0 && entry.created_at > to)
            continue;
        ++total;
    }
    co_return total;
}


} // namespace cnetnexus::chart
