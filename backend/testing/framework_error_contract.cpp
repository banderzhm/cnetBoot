import std;
import cnetmod.application.yaml_configuration;
import cnetmod.json;
import cnetboot.framework.error;

namespace {

namespace json = cnetmod::json;


auto member(const json::document& source, std::string_view key) -> const json::document*
{
    return json::find(source, key);
}


auto matches(const json::document& entry,
    const cnetboot::framework::error::error_entry& registered) -> bool
{
    const auto* code = member(entry, "code");
    const auto* status = member(entry, "httpStatus");
    const auto* message = member(entry, "defaultMessage");
    const auto* owner = member(entry, "owner");
    const auto* category = member(entry, "category");
    const auto* retryable = member(entry, "retryable");
    const auto* exposable = member(entry, "exposable");
    const auto* deprecated = member(entry, "deprecated");
    if (code == nullptr || status == nullptr || message == nullptr || owner == nullptr ||
        category == nullptr || retryable == nullptr || exposable == nullptr || deprecated == nullptr)
        return false;
    return code->get<std::string>() == registered.name &&
        status->as<int>() == registered.http_status &&
        message->get<std::string>() == registered.default_message &&
        owner->get<std::string>() == registered.owner &&
        category->get<std::string>() == registered.category &&
        retryable->get<bool>() == registered.retryable &&
        exposable->get<bool>() == registered.exposable &&
        deprecated->get<bool>() == registered.deprecated;
}

} // namespace



//




auto main() -> int
{
    const auto document = cnetmod::application::load_yaml_configuration_document("contracts/error_codes.yaml");
    if (!document)
        return 1;
    const auto* found = cnetmod::json::find(*document, "errors");
    if (found == nullptr || !found->is_array())
        return 2;

    const auto registered = cnetboot::framework::error::registry();
    if (found->get_array().size() != registered.size())
        return 3;


    for (const auto& entry : found->get_array())
    {
        const auto* code = cnetmod::json::find(entry, "code");
        if (code == nullptr)
            return 4;
        const auto name = code->get<std::string>();
        const auto match = cnetboot::framework::error::find(name);
        if (!match || !matches(entry, *match))
            return 4;
    }


    for (const auto& entry : registered)
    {
        const auto present = std::ranges::any_of(found->get_array(),
            [&entry](const cnetmod::json::document& candidate)
            {
                const auto* code = cnetmod::json::find(candidate, "code");
                return code != nullptr && code->get<std::string>() == entry.name;
            });
        if (!present)
            return 5;
    }


    if (const auto valid = cnetboot::framework::error::validate(); !valid)
        return 6;
    return 0;
}
