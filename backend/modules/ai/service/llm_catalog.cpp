/**
 * @brief llm_catalog 的实现：目录快照 -> 脱敏视图 -> JSON。
 */
module cnetnexus.reading.service.llm_catalog;

import std;
import cnetnexus.framework.config.llm_settings;

namespace cnetnexus::reading {
namespace {

    /// @brief JSON 字符串转义（只处理必须转义的字符；配置值是供应商/模型名）。
    [[nodiscard]] auto escape_json(std::string_view text) -> std::string
    {
        std::string result;
        result.reserve(text.size() + 2);
        for (const auto character : text)
        {
            switch (character)
            {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                result.push_back(character);
                break;
            }
        }
        return result;
    }

    void append_string_array(std::string& out, const std::vector<std::string>& values)
    {
        out.push_back('[');
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            if (index != 0)
                out.push_back(',');
            out.push_back('"');
            out += escape_json(values[index]);
            out.push_back('"');
        }
        out.push_back(']');
    }

} // namespace

auto llm_catalog(const cnetnexus::framework::config::llm_directory& directory)
    -> llm_catalog_snapshot
{
    return llm_catalog_snapshot{
        .configured = directory.usable(),
        .default_provider = directory.default_provider,
        .providers = cnetnexus::framework::config::llm_provider_catalog(directory),
    };
}

auto llm_catalog_json(const cnetnexus::framework::config::llm_directory& directory)
    -> std::string
{
    const auto snapshot = llm_catalog(directory);
    std::string out;
    out.reserve(256);
    out += "{\"configured\":";
    out += snapshot.configured ? "true" : "false";
    out += ",\"default_provider\":\"";
    out += escape_json(snapshot.default_provider);
    out += "\",\"providers\":[";
    for (std::size_t index = 0; index < snapshot.providers.size(); ++index)
    {
        const auto& provider = snapshot.providers[index];
        if (index != 0)
            out.push_back(',');
        out += "{\"name\":\"";
        out += escape_json(provider.name);
        out += "\",\"type\":\"";
        out += escape_json(provider.type);
        out += "\",\"default\":";
        out += provider.is_default ? "true" : "false";
        out += ",\"models\":";
        append_string_array(out, provider.models);
        out += ",\"keys\":[";
        for (std::size_t key_index = 0; key_index < provider.keys.size(); ++key_index)
        {
            if (key_index != 0)
                out.push_back(',');
            out += "{\"name\":\"";
            out += escape_json(provider.keys[key_index].name);
            out += "\",\"configured\":";
            out += provider.keys[key_index].configured ? "true" : "false";
            out += "}";
        }
        out += "]}";
    }
    out += "]}";
    return out;
}

} // namespace cnetnexus::reading
