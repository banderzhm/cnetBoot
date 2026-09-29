module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chart.controller.archive_access;

import std;
import cnetmod.protocol.http;
import cnetnexus.chart.domain.archive;
import cnetnexus.framework.security.authorization;
import cnetnexus.framework.security.current_user;

namespace cnetnexus::chart {

auto archive_access_from(const cnetmod::http::request_context& context)
    -> cnetnexus::chart::archive_access
{
    // enforce 一旦为真，service 层就会按下面的分区判定：不会出现「忘记传参 = 放行」。
    cnetnexus::chart::archive_access access;
    access.enforce = true;

    const auto* principal =
        context.scope().find<cnetnexus::security::current_user>();
    if (principal != nullptr && principal->id > 0)
    {
        access.authenticated = true;
        access.owner_id = static_cast<std::uint64_t>(principal->id);
    }

    if (access.authenticated)
        access.unrestricted =
            cnetnexus::security::has_unrestricted_data_access(context);
    return access;
}

} // namespace cnetnexus::chart
