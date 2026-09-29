/**
 * @brief 对话聚合（会话 + 其消息）的 MySQL 映射器与 XML 语句校验。
 *
 * SQL 分工：
 * - 读取与选择性更新（按主键查会话、会话分页、消息分页、计数、可写列更新）走
 *   cnetmod.orm 的 query_wrapper / update_wrapper；
 * - XML mapper（modules/chat/mapper/ChatMapper.xml）只保留构建器表达不了的语句：
 *   会话插入（created_by / updated_by 取 owner_id、两列时间同值）、message_count 列自增、
 *   会话与消息的批量逻辑删除、追加前的会话存在性探针；
 * - 消息追加在一个仓储事务里落库并维护会话计数，避免「有消息但计数为 0」的中间态。
 *
 * 写入与读取的会话存在性口径不同：追加要求会话存在（返回 CHAT.SESSION_NOT_FOUND），
 * 读取与清理是幂等的（没有记录 = 空页 / 0 条 / 删除成功）。
 */
export module cnetboot.chat.mapper.chat_mapper;

import std;
import cnetmod.application;
import cnetmod.coro.task;
import cnetmod.orm;
import cnetboot.chat.domain.chat_domain;
import cnetboot.chat.domain.chat_record;
import cnetboot.framework.error;
import cnetboot.framework.pagination;

export namespace cnetboot::chat {

namespace pagination = cnetboot::framework::pagination;
namespace error = cnetboot::framework::error;

/** @brief 会话与消息的 MySQL 映射器（chat_session / chat_message 两张表）。
 *
 *  会话与消息是同一聚合（消息只属于一个会话），因此合并为一个映射器；实现必须保证：
 *  - 读取一律排除 deleted_at 非空的行；
 *  - append_message 在**同一事务内**维护会话的 message_count / last_message_at /
 *    updated_at，避免会话列表出现「有消息但计数为 0」的中间态；
 *  - 消息按主键升序（= 时间序）返回，分页稳定；
 *  - session.title 为空时由服务层补默认标题，映射器只负责存取。
 */
class mysql_chat_mapper
{
public:
    mysql_chat_mapper(cnetmod::application::managed_repository<chat_session_record> sessions,
        cnetmod::application::managed_repository<chat_message_record> messages,
        std::shared_ptr<const cnetmod::orm::mapper_registry> registry);

    /** @brief 新建会话；id 与时间戳由映射器补齐。 */
    [[nodiscard]] auto create_session(const chat_session& value)
        -> cnetmod::task<std::expected<chat_session, error::error_code>>;

    /** @brief 按主键查询；不存在返回 std::nullopt。 */
    [[nodiscard]] auto find_session(std::uint64_t id)
        -> cnetmod::task<std::expected<std::optional<chat_session>, error::error_code>>;

    /** @brief 覆盖更新（title / system_prompt / model / temperature）；不存在返回
     *  CHAT.SESSION_NOT_FOUND。 */
    [[nodiscard]] auto update_session(const chat_session& value)
        -> cnetmod::task<std::expected<chat_session, error::error_code>>;

    /** @brief 逻辑删除会话及其全部消息；不存在返回 CHAT.SESSION_NOT_FOUND。 */
    [[nodiscard]] auto remove_session(std::uint64_t id)
        -> cnetmod::task<std::expected<void, error::error_code>>;

    /** @brief 分页检索会话，按 last_message_at、created_at、id 倒序。 */
    [[nodiscard]] auto find_session_page(const chat_session_query& value)
        -> cnetmod::task<std::expected<pagination::page_result<chat_session>, error::error_code>>;

    /** @brief 追加一条消息，并在同一事务内同步会话计数与最后消息时间。 */
    [[nodiscard]] auto append_message(const chat_message& value)
        -> cnetmod::task<std::expected<chat_message, error::error_code>>;

    /** @brief 会话消息分页（按主键升序，最早的一页在前）。 */
    [[nodiscard]] auto find_message_page(std::uint64_t session_id, const pagination::page_request& page)
        -> cnetmod::task<std::expected<pagination::page_result<chat_message>, error::error_code>>;

    /** @brief 最近 limit 条消息，返回按时间正序（供上下文组装）。
     *  @param limit 0 表示不限 */
    [[nodiscard]] auto find_recent_messages(std::uint64_t session_id, std::size_t limit)
        -> cnetmod::task<std::expected<std::vector<chat_message>, error::error_code>>;

private:
    /// @brief 按 ID 读一行会话（不走分页；query_wrapper + 逻辑删除守卫）。
    [[nodiscard]] auto select_session(std::uint64_t id)
        -> cnetmod::task<std::expected<std::optional<chat_session>, error::error_code>>;

    cnetmod::application::managed_repository<chat_session_record> sessions_;
    cnetmod::application::managed_repository<chat_message_record> messages_;
    std::shared_ptr<const cnetmod::orm::mapper_registry> registry_;
};

/** @brief 启动期校验对话 XML 的必需语句（装载由统一的 mapper registry 服务负责）。 */
[[nodiscard]] auto validate_chat_mapper_registry(const cnetmod::orm::mapper_registry& registry)
    -> std::expected<void, std::string>;

} // namespace cnetboot::chat
