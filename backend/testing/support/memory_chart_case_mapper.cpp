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


// ============================== chart_case_mapper
// ==============================

auto memory_chart_case_mapper::create(const chart::chart_case& value)
    -> cnetmod::task<std::expected<chart::chart_case, error_code>>
{
    if (value.title.empty())
        co_return std::unexpected{error_code::request_invalid_argument};

    const std::scoped_lock lock{db_->mutex_};
    for (const auto tag_id : value.tag_ids)
        if (std::ranges::none_of(db_->tags_,
                [tag_id](const auto& tag) { return tag.id == tag_id; }))
            co_return std::unexpected{error_code::tag_not_found};
    auto created = value;
    created.id = db_->next_case_id_++;
    const auto now = cnetmod::unix_time_seconds();
    created.created_at = now;
    created.updated_at = now;
    db_->cases_.push_back(created);
    if (!value.tag_ids.empty())
        db_->case_tags_[created.id] = value.tag_ids;
    co_return created;
}

auto memory_chart_case_mapper::find_by_id(std::uint64_t id)
    -> cnetmod::task<
        std::expected<std::optional<chart::chart_case>, error_code>>
{
    const std::scoped_lock lock{db_->mutex_};
    const auto found =
        std::ranges::find_if(db_->cases_, [id](const chart::chart_case& item)
            {
                return item.id == id;
            });
    if (found == db_->cases_.end())
        co_return std::optional<chart::chart_case>{};

    auto value = *found;
    const auto linked = db_->case_tags_.find(id);
    if (linked != db_->case_tags_.end())
    {
        value.tag_ids = linked->second;
        for (const auto tag_id : linked->second)
        {
            if (const auto* tag = find_tag_by_id(db_->tags_, tag_id); tag != nullptr)
                value.tags.push_back(*tag);
        }
    }
    co_return std::optional<chart::chart_case>{std::move(value)};
}

auto memory_chart_case_mapper::update(const chart::chart_case& value, bool replace_tags)
    -> cnetmod::task<std::expected<chart::chart_case, error_code>>
{
    const std::scoped_lock lock{db_->mutex_};
    const auto found = std::ranges::find_if(
        db_->cases_,
        [id = value.id](const chart::chart_case& item)
        {
            return item.id == id;
        });
    if (found == db_->cases_.end())
        co_return std::unexpected{error_code::case_not_found};

    auto merged = value;
    merged.owner_id = found->owner_id;
    merged.created_at = found->created_at;
    merged.updated_at = cnetmod::unix_time_seconds();
    if (replace_tags)
    {
        for (const auto tag_id : value.tag_ids)
            if (std::ranges::none_of(db_->tags_,
                    [tag_id](const auto& tag) { return tag.id == tag_id; }))
                co_return std::unexpected{error_code::tag_not_found};
        db_->case_tags_[found->id] = value.tag_ids;
    }
    else
        merged.tag_ids = found->tag_ids;
    merged.tags.clear();
    *found = merged;
    co_return merged;
}

auto memory_chart_case_mapper::remove(std::uint64_t id)
    -> cnetmod::task<std::expected<void, error_code>>
{
    const std::scoped_lock lock{db_->mutex_};
    const auto found =
        std::ranges::find_if(db_->cases_, [id](const chart::chart_case& item)
            {
                return item.id == id;
            });
    if (found == db_->cases_.end())
        co_return std::unexpected{error_code::case_not_found};
    db_->cases_.erase(found);
    db_->case_tags_.erase(id);
    co_return std::expected<void, error_code>{};
}

auto memory_chart_case_mapper::list(const chart::case_query& query)
    -> cnetmod::task<
        std::expected<cnetnexus::framework::pagination::page_result<chart::chart_case>,
            error_code>>
{
    const std::scoped_lock lock{db_->mutex_};
    const auto page = cnetnexus::framework::pagination::normalize(query.page);

    std::vector<chart::chart_case> filtered;
    for (const auto& candidate : db_->cases_)
    {
        if (!query.algorithm.empty() && candidate.algorithm != query.algorithm)
            continue;
        if (query.tag_id != 0)
        {
            const auto linked = db_->case_tags_.find(candidate.id);
            if (linked == db_->case_tags_.end() ||
                std::ranges::find(linked->second, query.tag_id) ==
                    linked->second.end())
                continue;
        }
        if (!query.keyword.empty() &&
            !contains_ci(candidate.title, query.keyword) &&
            !contains_ci(candidate.client_name, query.keyword) &&
            !contains_ci(candidate.question, query.keyword))
            continue;
        filtered.push_back(candidate);
    }

    std::ranges::sort(filtered, [](const chart::chart_case& left, const chart::chart_case& right)
        {
            if (left.created_at != right.created_at)
                return left.created_at > right.created_at;
            return left.id > right.id;
        });

    cnetnexus::framework::pagination::page_result<chart::chart_case> result;
    result.total = static_cast<std::int64_t>(filtered.size());
    result.page = page.page;
    result.page_size = page.page_size;
    result.items = slice(filtered, page);

    // 列表项装配标签，前端直接渲染 tag chips。
    for (auto& item : result.items)
    {
        const auto linked = db_->case_tags_.find(item.id);
        if (linked == db_->case_tags_.end())
            continue;
        item.tag_ids = linked->second;
        for (const auto tag_id : linked->second)
        {
            if (const auto* tag = find_tag_by_id(db_->tags_, tag_id); tag != nullptr)
                item.tags.push_back(*tag);
        }
    }
    co_return result;
}

auto memory_chart_case_mapper::count()
    -> cnetmod::task<std::expected<std::uint64_t, error_code>>
{
    const std::scoped_lock lock{db_->mutex_};
    co_return static_cast<std::uint64_t>(db_->cases_.size());
}

auto memory_chart_case_mapper::set_tags(
    std::uint64_t case_id, std::span<const std::uint64_t> tag_ids)
    -> cnetmod::task<std::expected<void, error_code>>
{
    const std::scoped_lock lock{db_->mutex_};
    const auto found = std::ranges::find_if(
        db_->cases_,
        [case_id](const chart::chart_case& item)
        {
            return item.id == case_id;
        });
    if (found == db_->cases_.end())
        co_return std::unexpected{error_code::case_not_found};

    std::vector<std::uint64_t> unique;
    for (const auto tag_id : tag_ids)
    {
        if (find_tag_by_id(db_->tags_, tag_id) == nullptr)
            co_return std::unexpected{error_code::tag_not_found};
        if (std::ranges::find(unique, tag_id) == unique.end())
            unique.push_back(tag_id);
    }
    db_->case_tags_[case_id] = std::move(unique);
    co_return std::expected<void, error_code>{};
}

auto memory_chart_case_mapper::list_tags()
    -> cnetmod::task<std::expected<std::vector<chart::chart_tag>, error_code>>
{
    const std::scoped_lock lock{db_->mutex_};
    auto tags = db_->tags_;
    std::ranges::sort(
        tags, [](const chart::chart_tag& left, const chart::chart_tag& right)
        {
            return left.id < right.id;
        });
    co_return tags;
}

auto memory_chart_case_mapper::find_tag(std::uint64_t id) -> cnetmod::task<
    std::expected<std::optional<chart::chart_tag>, error_code>>
{
    const std::scoped_lock lock{db_->mutex_};
    if (const auto* tag = find_tag_by_id(db_->tags_, id); tag != nullptr)
        co_return std::optional<chart::chart_tag>{*tag};
    co_return std::optional<chart::chart_tag>{};
}

auto memory_chart_case_mapper::create_tag(const chart::chart_tag& value)
    -> cnetmod::task<std::expected<chart::chart_tag, error_code>>
{
    if (value.name.empty())
        co_return std::unexpected{error_code::request_invalid_argument};

    const std::scoped_lock lock{db_->mutex_};
    const auto duplicate =
        std::ranges::any_of(db_->tags_, [&value](const chart::chart_tag& item)
            {
                return item.name == value.name && item.owner_id == value.owner_id;
            });
    if (duplicate)
        co_return std::unexpected{error_code::tag_conflict};

    auto created = value;
    created.id = db_->next_tag_id_++;
    created.created_at = cnetmod::unix_time_seconds();
    db_->tags_.push_back(created);
    co_return created;
}

auto memory_chart_case_mapper::remove_tag(std::uint64_t id)
    -> cnetmod::task<std::expected<void, error_code>>
{
    const std::scoped_lock lock{db_->mutex_};
    const auto found = std::ranges::find_if(
        db_->tags_, [id](const chart::chart_tag& item)
        {
            return item.id == id;
        });
    if (found == db_->tags_.end())
        co_return std::unexpected{error_code::tag_not_found};
    db_->tags_.erase(found);
    for (auto& [case_id, tags] : db_->case_tags_)
        std::erase(tags, id);
    co_return std::expected<void, error_code>{};
}


} // namespace cnetnexus::chart
