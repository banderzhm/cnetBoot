module cnetnexus.framework.web.response;

import std;
import cnetmod.core.log;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetnexus.framework.error;
import cnetnexus.framework.web.error_response;

namespace cnetnexus::framework::web {

void respond_serialization_failure(cnetmod::http::request_context& context,
    std::string_view reason)
{
    logger::error{"response serialization failed: path={} error={}",
        context.path(), reason};
    respond_error(context,
        cnetnexus::framework::error::error_code::system_internal_error);
}

void respond_document(cnetmod::http::request_context& context,
    const cnetmod::json::document& body, int status)
{
    auto encoded = cnetmod::json::write_document(body);
    if (!encoded)
    {
        respond_serialization_failure(context, encoded.error().message());
        return;
    }
    context.json(status, *encoded);
}

} // namespace cnetnexus::framework::web
