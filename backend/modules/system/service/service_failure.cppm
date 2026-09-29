/**
 * @brief system 服务层的失败转换：细节只进日志，对外只给注册错误码。
 *
 * 数据层（mapper / repository）返回的文本可能包含 SQL、表名或驱动信息，
 * 不能进入响应体；服务层在这里把它记录下来并换成稳定的错误码。
 */
export module cnetboot.system.service.failure;

import std;
import cnetmod.core.log;
import cnetmod.orm;
import cnetboot.framework.error;

export namespace cnetboot::system {

using error_code = cnetboot::framework::error::error_code;

/// @brief 存储访问失败：DEPENDENCY.UNAVAILABLE。
[[nodiscard]] inline auto storage_failure(std::string_view operation,
    std::string_view detail) -> error_code
{
    logger::error{"system storage failure: operation={} detail={}", operation, detail};
    return error_code::upstream_unavailable;
}

/// @brief 仓储结果失败：DEPENDENCY.UNAVAILABLE。
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

/// @brief 库内数据违反不变式（配置错误的角色、坏掉的树）：SYSTEM.INTERNAL_ERROR。
[[nodiscard]] inline auto integrity_failure(std::string_view operation,
    std::string_view detail) -> error_code
{
    logger::error{"system data integrity failure: operation={} detail={}", operation,
        detail};
    return error_code::system_internal_error;
}

} // namespace cnetboot::system
