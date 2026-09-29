module cnetnexus.chart.domain.archive;

import std;
import cnetnexus.framework.error;

namespace cnetnexus::chart {

auto archive_access::can_access_all() const noexcept -> bool
{
    return enforce && authenticated && unrestricted;
}

auto archive_access::scoped() const noexcept -> bool
{
    return enforce;
}

auto archive_access::may_read(std::uint64_t record_owner_id) const noexcept
    -> bool
{
    if (!enforce || can_access_all())
        return true;
    return record_owner_id == anonymous_owner_id ||
        (authenticated && record_owner_id == owner_id);
}

auto archive_access::may_delete(std::uint64_t record_owner_id) const noexcept
    -> bool
{
    if (!enforce || can_access_all())
        return true;
    return authenticated && owner_id != anonymous_owner_id &&
        record_owner_id == owner_id;
}

auto archive_access::denial() const noexcept
    -> cnetnexus::framework::error::error_code
{
    return authenticated
        ? cnetnexus::framework::error::error_code::auth_forbidden
        : cnetnexus::framework::error::error_code::auth_token_missing;
}

auto archive_access::filter() const -> cnetnexus::chart::archive_owner_filter
{
    cnetnexus::chart::archive_owner_filter filter;
    if (!enforce || can_access_all())
        return filter;

    filter.active = true;
    // 与 may_read() 同口径：公共分区对所有请求可读。
    filter.include_unowned = true;
    if (authenticated && owner_id != anonymous_owner_id)
        filter.owner_ids.push_back(owner_id);
    return filter;
}

} // namespace cnetnexus::chart
