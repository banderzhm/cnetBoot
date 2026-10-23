
export module cnetboot.framework.web.response;

import std;
import cnetmod.json;
import cnetmod.protocol.http;

export namespace cnetboot::framework::web {


void respond_document(cnetmod::http::request_context& context,
    const cnetmod::json::document& body, int status = cnetmod::http::status::ok);


void respond_serialization_failure(cnetmod::http::request_context& context,
    std::string_view reason);


template <class T>
void respond_json(cnetmod::http::request_context& context, const T& body,
    int status = cnetmod::http::status::ok)
{
    const auto encoded = cnetmod::json::write(body);
    if (!encoded)
    {
        respond_serialization_failure(context, encoded.error().message());
        return;
    }
    context.json(status, *encoded);
}

} // namespace cnetboot::framework::web
