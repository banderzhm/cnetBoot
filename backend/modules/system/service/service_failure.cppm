
export module cnetboot.system.service.failure;

import std;
import cnetmod.core.log;
import cnetmod.orm;
import cnetboot.framework.error;

export namespace cnetboot::system {

using error_code = cnetboot::framework::error::error_code;


[[nodiscard]] inline auto storage_failure(std::string_view operation,
    std::string_view detail) -> error_code
{
    logger::error{"system storage failure: operation={} detail={}", operation, detail};
    return error_code::upstream_unavailable;
}


template <typename T>
[[nodiscard]] auto storage_failure(std::string_view operation,
    const cnetmod::orm::model_result<T>& result) -> error_code
{
    if (!result.error_msg.empty())
        return storage_failure(operation, result.error_msg);
    if (result.framework_error)
        return storage_failure(operation, result.framework_error.message());
    return storage_failure(operation, "database operation failed");
}


[[nodiscard]] inline auto integrity_failure(std::string_view operation,
    std::string_view detail) -> error_code
{
    logger::error{"system data integrity failure: operation={} detail={}", operation,
        detail};
    return error_code::system_internal_error;
}

} // namespace cnetboot::system
