import std;
import cnetboot.framework.error;
import cnetboot.framework.web.error_response;




// transport errors -> cnetboot::framework::web::error_*).
auto main() -> int
{
    if (const auto valid = cnetboot::framework::error::validate(); !valid)
        return 1;

    const auto entries = cnetboot::framework::error::registry();
    if (entries.empty())
        return 2;
    if (!cnetboot::framework::error::find("SYSTEM.INTERNAL_ERROR").has_value())
        return 3;
    if (cnetboot::framework::error::find("NOT.REGISTERED").has_value())
        return 4;
    if (cnetboot::framework::web::error_http_status(
            cnetboot::framework::error::error_code::upstream_unavailable) != 503)
        return 5;
    if (cnetboot::framework::web::error_http_status(
            cnetboot::framework::error::error_code::category_invalid) != 400)
        return 6;


    const auto body = cnetboot::framework::web::error_body(
        cnetboot::framework::error::error_code::category_invalid);
    if (body.code != "400" || body.status != "CHAT.CATEGORY_INVALID" || body.retryable)
        return 7;
    if (body.message.empty())
        return 8;

    const auto registry = cnetboot::framework::web::error_registry_body();
    if (registry.errors.size() != entries.size())
        return 9;
    const auto unavailable = std::ranges::find_if(registry.errors,
        [](const auto& item)
        {
            return item.status == "DEPENDENCY.UNAVAILABLE";
        });
    if (unavailable == registry.errors.end() || unavailable->httpStatus != 503)
        return 10;
    return 0;
}
