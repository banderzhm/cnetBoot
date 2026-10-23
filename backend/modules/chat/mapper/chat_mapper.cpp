module cnetboot.chat.mapper.chat_mapper;

import std;
import cnetmod.core.time;
import cnetmod.coro.task;
import cnetmod.orm;
import cnetboot.chat.domain.chat_domain;
import cnetboot.chat.domain.chat_record;
import cnetboot.framework.error;
import cnetboot.framework.pagination;

namespace cnetboot::chat {
namespace {

namespace db_api = cnetmod::database;
namespace orm_api = cnetmod::orm;
using error_code = cnetboot::framework::error::error_code;


constexpr std::string_view select_session_statement{"ChatMapper.selectSession"};
constexpr std::string_view touch_session_statement{"ChatMapper.touchSession"};
constexpr std::string_view soft_delete_session_statement{"ChatMapper.softDeleteSession"};
constexpr std::string_view soft_delete_messages_statement{"ChatMapper.softDeleteSessionMessages"};

[[nodiscard]] auto now_wall_clock() -> db_api::calendar_datetime
{
    return db_api::datetime_from_unix_seconds(cnetmod::unix_time_seconds())
        .value_or(db_api::calendar_datetime{});
}

[[nodiscard]] auto wall_clock_of(std::int64_t unix_seconds) -> db_api::calendar_datetime
{
    return db_api::datetime_from_unix_seconds(unix_seconds).value_or(db_api::calendar_datetime{});
}

[[nodiscard]] auto to_session(const chat_session_record& row) -> chat_session
{
    return chat_session{.id = row.id,
        .owner_id = row.owner_id,
        .title = row.title,
        .category = row.category,
        .system_prompt = row.system_prompt.value_or(std::string{}),
        .model = row.model,
        .temperature = std::strtod(row.temperature.c_str(), nullptr),
        .message_count = row.message_count,
        .last_message_at = db_api::unix_seconds_from_datetime(row.last_message_at).value_or(0),
        .created_at = db_api::unix_seconds_from_datetime(row.created_at).value_or(0),
        .updated_at = db_api::unix_seconds_from_datetime(row.updated_at).value_or(0)};
}

[[nodiscard]] auto to_message(const chat_message_record& row) -> chat_message
{
    return chat_message{.id = row.id,
        .session_id = row.session_id,
        .role = row.role,
        .content = row.content,
        .model = row.model,
        .tokens_in = row.tokens_in,
        .tokens_out = row.tokens_out,
        .created_at = db_api::unix_seconds_from_datetime(row.created_at).value_or(0)};
}

[[nodiscard]] auto to_message_record(const chat_message& value) -> chat_message_record
{
    return chat_message_record{.session_id = value.session_id,
        .role = value.role,
        .content = value.content,
        .model = value.model,
        .tokens_in = value.tokens_in,
        .tokens_out = value.tokens_out,
        .created_at = value.created_at == 0
            ? db_api::calendar_datetime{}
            : wall_clock_of(value.created_at)};
}

[[nodiscard]] auto to_session_record(const chat_session& value) -> chat_session_record
{
    return chat_session_record{.owner_id = value.owner_id,
        .title = value.title,
        .category = value.category,
        .system_prompt = value.system_prompt,
        .model = value.model,
        .temperature = std::format("{:.2f}", value.temperature),
        .created_by = value.owner_id,
        .updated_by = value.owner_id};
}

template <typename T>
[[nodiscard]] auto failure_of(const orm_api::model_result<T>& result) -> error_code
{
    return result.framework_error ? error_code::upstream_unavailable
                                  : error_code::system_internal_error;
}

[[nodiscard]] auto message_filter(std::uint64_t session_id)
    -> orm_api::query_wrapper<chat_message_record>
{
    orm_api::query_wrapper<chat_message_record> filter;
    filter.eq(&chat_message_record::session_id, session_id);
    return filter;
}

} // namespace

mysql_chat_mapper::mysql_chat_mapper(
    cnetmod::application::managed_repository<chat_session_record> sessions,
    cnetmod::application::managed_repository<chat_message_record> messages,
    std::shared_ptr<const cnetmod::orm::mapper_registry> registry)
    : sessions_(std::move(sessions)),
      messages_(std::move(messages)),
      registry_(std::move(registry))
{
    if (!registry_)
        throw std::invalid_argument("chat mapper registry is required");
}

auto mysql_chat_mapper::select_session(std::uint64_t id)
    -> cnetmod::task<std::expected<std::optional<chat_session>, error_code>>
{

    auto query = orm_api::query_wrapper<chat_session_record>{};
    query.eq(&chat_session_record::id, id).limit(1);
    auto found = co_await sessions_.list(query);
    if (!found.ok())
        co_return std::unexpected{failure_of(found)};
    if (found.data.empty())
        co_return std::optional<chat_session>{};
    co_return std::optional<chat_session>{to_session(found.data.front())};
}

auto mysql_chat_mapper::create_session(const chat_session& value)
    -> cnetmod::task<std::expected<chat_session, error_code>>
{
    auto record = to_session_record(value);
    auto inserted = co_await sessions_.save(record);
    if (!inserted.ok())
        co_return std::unexpected{failure_of(inserted)};

    auto created = co_await select_session(record.id);
    if (!created)
        co_return std::unexpected{created.error()};
    if (!created->has_value())
        co_return std::unexpected{error_code::system_internal_error};
    co_return **created;
}

auto mysql_chat_mapper::find_session(std::uint64_t id)
    -> cnetmod::task<std::expected<std::optional<chat_session>, error_code>>
{
    co_return co_await select_session(id);
}

auto mysql_chat_mapper::update_session(const chat_session& value)
    -> cnetmod::task<std::expected<chat_session, error_code>>
{

    auto existing = co_await select_session(value.id);
    if (!existing)
        co_return std::unexpected{existing.error()};
    if (!existing->has_value())
        co_return std::unexpected{error_code::chat_session_not_found};


    orm_api::update_wrapper<chat_session_record> update;
    update.set(&chat_session_record::title, value.title)
        .set(&chat_session_record::system_prompt,
            std::optional<std::string>{value.system_prompt})
        .set(&chat_session_record::model, value.model)
        .set(&chat_session_record::temperature, std::format("{:.2f}", value.temperature))
        .eq(&chat_session_record::id, value.id)
        .is_null(&chat_session_record::deleted_at);
    auto updated = co_await sessions_.update(update);
    if (!updated.ok())
        co_return std::unexpected{failure_of(updated)};

    auto current = co_await select_session(value.id);
    if (!current)
        co_return std::unexpected{current.error()};
    if (!current->has_value())
        co_return std::unexpected{error_code::chat_session_not_found};
    co_return **current;
}

auto mysql_chat_mapper::remove_session(std::uint64_t id)
    -> cnetmod::task<std::expected<void, error_code>>
{
    orm_api::param_context parameters;
    parameters.set("session_id", id);
    parameters.set("updated_time", now_wall_clock());
    auto removed = co_await sessions_.execute_xml(*registry_, soft_delete_session_statement,
        parameters);
    if (!removed.ok())
        co_return std::unexpected{failure_of(removed)};
    if (removed.affected_rows == 0)
        co_return std::unexpected{error_code::chat_session_not_found};


    auto erased = co_await messages_.execute_xml(*registry_, soft_delete_messages_statement,
        parameters);
    if (!erased.ok())
        co_return std::unexpected{failure_of(erased)};
    co_return {};
}

auto mysql_chat_mapper::find_session_page(const chat_session_query& value)
    -> cnetmod::task<std::expected<pagination::page_result<chat_session>, error_code>>
{
    const auto page = pagination::normalize(value.page);


    auto filter = orm_api::query_wrapper<chat_session_record>{};
    if (!value.category.empty())
        filter.eq(&chat_session_record::category, value.category);
    filter.order_by_desc(&chat_session_record::last_message_at)
        .order_by_desc(&chat_session_record::created_at)
        .order_by_desc(&chat_session_record::id);

    auto paged = co_await sessions_.page(static_cast<std::size_t>(page.page),
        static_cast<std::size_t>(page.page_size), filter);
    if (!paged.ok())
        co_return std::unexpected{failure_of(paged.records)};

    pagination::page_result<chat_session> result;
    result.total = static_cast<std::int64_t>(paged.total);
    result.page = page.page;
    result.page_size = page.page_size;
    result.items.reserve(paged.records.data.size());
    for (const auto& row : paged.records.data)
        result.items.push_back(to_session(row));
    co_return result;
}

auto mysql_chat_mapper::append_message(const chat_message& value)
    -> cnetmod::task<std::expected<chat_message, error_code>>
{
    auto record = to_message_record(value);
    std::uint64_t inserted_id = 0;
    bool session_missing = false;


    auto outcome = co_await sessions_.transaction<std::expected<void, error_code>>(
        [&](auto& unit) -> cnetmod::task<std::expected<void, error_code>>
        {

            orm_api::param_context probe;
            probe.set("session_id", value.session_id);
            auto session = co_await unit.template mapper<chat_session_record>().select_xml(
                *registry_, select_session_statement, probe);
            if (!session.ok())
                co_return std::unexpected{failure_of(session)};
            if (session.data.empty())
            {
                session_missing = true;
                co_return std::unexpected{error_code::chat_session_not_found};
            }

            auto written = co_await unit.template mapper<chat_message_record>()
                .insert(record);
            if (!written.ok())
                co_return std::unexpected{failure_of(written)};
            inserted_id = record.id;



            orm_api::param_context touch;
            touch.set("session_id", value.session_id);
            touch.set("created_time", record.created_at);
            auto touched = co_await unit.template mapper<chat_session_record>().execute_xml(
                *registry_, touch_session_statement, touch);
            if (!touched.ok())
                co_return std::unexpected{failure_of(touched)};
            if (touched.affected_rows == 0)
            {
                session_missing = true;
                co_return std::unexpected{error_code::chat_session_not_found};
            }
            co_return std::expected<void, error_code>{};
        });

    if (!outcome)
        co_return std::unexpected{session_missing ? error_code::chat_session_not_found
                                                  : error_code::system_internal_error};

    chat_message appended = value;
    appended.id = inserted_id;
    appended.created_at = db_api::unix_seconds_from_datetime(record.created_at).value_or(0);
    co_return appended;
}

auto mysql_chat_mapper::find_message_page(std::uint64_t session_id, const pagination::page_request& page)
    -> cnetmod::task<std::expected<pagination::page_result<chat_message>, error_code>>
{
    const auto normalized = pagination::normalize(page);


    auto filter = message_filter(session_id);
    filter.order_by_asc(&chat_message_record::id);
    auto paged = co_await messages_.page(static_cast<std::size_t>(normalized.page),
        static_cast<std::size_t>(normalized.page_size), filter);
    if (!paged.ok())
        co_return std::unexpected{failure_of(paged.records)};

    pagination::page_result<chat_message> result;
    result.total = static_cast<std::int64_t>(paged.total);
    result.page = normalized.page;
    result.page_size = normalized.page_size;
    result.items.reserve(paged.records.data.size());
    for (const auto& row : paged.records.data)
        result.items.push_back(to_message(row));
    co_return result;
}

auto mysql_chat_mapper::find_recent_messages(std::uint64_t session_id, std::size_t limit)
    -> cnetmod::task<std::expected<std::vector<chat_message>, error_code>>
{

    auto window = message_filter(session_id);
    if (limit == 0)
        window.order_by_asc(&chat_message_record::id);
    else
        window.order_by_desc(&chat_message_record::id).limit(static_cast<std::int64_t>(limit));
    auto rows = co_await messages_.list(window);
    if (!rows.ok())
        co_return std::unexpected{failure_of(rows)};

    std::vector<chat_message> messages;
    messages.reserve(rows.data.size());
    if (limit == 0)
    {
        for (const auto& row : rows.data)
            messages.push_back(to_message(row));
    }
    else
    {
        for (auto item = rows.data.rbegin(); item != rows.data.rend(); ++item)
            messages.push_back(to_message(*item));
    }
    co_return messages;
}

auto validate_chat_mapper_registry(const cnetmod::orm::mapper_registry& registry)
    -> std::expected<void, std::string>
{


    for (const auto statement : {select_session_statement,
             touch_session_statement,
             soft_delete_session_statement, soft_delete_messages_statement})
    {
        if (registry.find_statement(statement) == nullptr)
            return std::unexpected{std::string{"chat mapper is missing statement: "} +
                std::string{statement}};
    }
    return {};
}

} // namespace cnetboot::chat
