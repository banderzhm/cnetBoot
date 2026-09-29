module;

#include <cnetmod/json_codec.hpp>

export module cnetnexus.chart.controller.archive_access;

/**
 * @brief HTTP 适配层：把请求作用域里的身份投影成 domain 的 archive_access。
 *
 * current_user 决定本人 owner_id；data_permission_scope 只提供全量管理员标志。
 * partition_ids 是部门 ID，不得转成用户 owner_id。
 *
 * 公开 GET 使用可选鉴权：有有效身份则可读本人和公共分区，否则只读公共分区。
 */

import std;
import cnetmod.protocol.http;
import cnetnexus.chart.domain.archive;

export namespace cnetnexus::chart {

/**
 * @brief 从请求上下文投影归属上下文。
 *
 * 未认证返回默认值（owner_id = 0：只可见匿名分区）。派生规则见 domain::archive_access：
 *  - 全量数据范围（unrestricted）→ can_access_all()；
 *  - 其余情况只读本人及匿名公共分区；删除权限单独判定。
 */
[[nodiscard]] auto archive_access_from(
    const cnetmod::http::request_context& context)
    -> cnetnexus::chart::archive_access;

} // namespace cnetnexus::chart
