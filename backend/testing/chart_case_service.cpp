import std;
import cnetmod.coro;
import cnetmod.io;
import cnetnexus.chart.domain.archive;
import cnetnexus.chart.domain.cases;
import cnetnexus.chart.mapper.memory_chart_case_mapper;
import cnetnexus.chart.service.case_service;
import cnetnexus.chart.service.chart_archive_service;
import cnetnexus.framework.audit;
import cnetnexus.framework.error;

namespace {

using error_code = cnetnexus::framework::error::error_code;

/// @brief 记录审计行的测试替身（玄学用 audit_service，CNetNexus 用 audit_log_service 抽象）。
class recording_audit_log final : public cnetnexus::framework::audit_log_service
{
public:
    int calls{0};
    std::vector<std::string> actions;

    auto record(const cnetnexus::framework::audit_actor&, std::string_view action,
        std::string_view, int, std::string) -> cnetmod::task<std::expected<void, error_code>> override
    {
        ++calls;
        actions.emplace_back(action);
        co_return std::expected<void, error_code>{};
    }
};

/// @brief 测试自持事件循环上跑完一个协程并取回结果。
template <typename T> auto drive(cnetmod::io_context& io, cnetmod::task<T> work) -> T
{
    std::optional<T> result;
    auto runner = [&io, &result, work = std::move(work)]() mutable -> cnetmod::task<void>
    {
        result.emplace(co_await std::move(work));
        io.stop();
    };
    cnetmod::spawn(io, runner());
    io.run();
    io.restart();
    return std::move(*result);
}

/// @brief 同上；被测协程返回 void 时用它。
inline auto drive(cnetmod::io_context& io, cnetmod::task<void> work) -> void
{
    auto runner = [&io, work = std::move(work)]() mutable -> cnetmod::task<void>
    {
        co_await std::move(work);
        io.stop();
    };
    cnetmod::spawn(io, runner());
    io.run();
    io.restart();
}

} // namespace

// 案例 / 标签 / 存档索引用例：内存实现下的完整语义（案例 CRUD、标签、检索、
// report_text 回退到存档快照、归档与删除）。
//
// 迁移自 xuanxue-algorithm tests/chart_case_service_test.cpp。**明确删除的部分**
// （依赖尚未迁入 CNetNexus 的 stats_service / 用户仓储）：
//  - 原第 7 段「统计：用户数、案例数、存档分布与今日计数」整段（overview() 用户数、
//    today_charts/today_logins）—— CNetNexus 的 stats_service 存在但依赖 system 域的
//    用户/登录统计仓储，且本用例没有对应替身；该能力的覆盖留给 system 域用例。
//  其余断言（标签冲突、术数别名归一、标签装配、过滤、编辑语义、report_text、
//  存档写读删、删除后再查报错）逐条保留。
auto main() -> int
{
    cnetnexus::chart::chart_database chart_db;
    cnetnexus::chart::memory_chart_case_mapper cases{chart_db};
    cnetnexus::chart::memory_chart_archive_mapper index{chart_db};

    auto io = cnetmod::make_io_context();
    if (!io)
        return 1;
    recording_audit_log audit;
    cnetnexus::chart::chart_archive_service archives{index};
    cnetnexus::chart::case_service service{cases, archives, audit};

    cnetnexus::framework::audit_actor actor;
    actor.user_id = 1;
    actor.username = "admin";

    // 1. 标签：新建、重复冲突、列表。
    const auto tag = drive(*io, service.create_tag("事业", "#ff0000", 1, actor));
    if (!tag || tag->id != 1 || tag->name != "事业")
        return 1;
    const auto conflict = drive(*io, service.create_tag("事业", "", 1, actor));
    if (conflict || conflict.error() != error_code::tag_conflict)
        return 2;
    if (const auto tags = drive(*io, service.list_tags()); !tags || tags->size() != 1)
        return 3;

    // 2. 新建案例：术数别名归一 + 标签装配。
    cnetnexus::chart::case_write_request request;
    request.value.title = "张三-事业";
    request.value.algorithm = "bazi";
    request.value.client_name = "张三";
    request.value.payload = R"({"analysisZh":"日主丁火"})";
    request.tag_ids = {1};
    request.has_tag_ids = true;
    request.has_payload = true;
    const auto created = drive(*io, service.create(request, actor));
    if (!created || created->id != 1 || created->algorithm != "ba_zi")
        return 4;
    if (created->tags.size() != 1 || created->tag_ids.size() != 1)
        return 5;

    // 3. 列表过滤：关键字、术数、标签。
    cnetnexus::chart::case_query filter;
    filter.keyword = "张三";
    if (const auto page = drive(*io, service.list(filter)); !page || page->total != 1)
        return 6;
    filter = {};
    filter.algorithm = "ba_zi";
    if (const auto page = drive(*io, service.list(filter)); !page || page->total != 1)
        return 7;
    filter = {};
    filter.tag_id = 99;
    if (const auto page = drive(*io, service.list(filter)); !page || page->total != 0)
        return 8;

    // 4. 编辑：has_payload=false 时保留原 payload，has_reading=true 时覆盖 reading。
    cnetnexus::chart::case_write_request edit;
    edit.value.id = 1;
    edit.value.title = "张三-事业（改）";
    edit.value.algorithm = "ba_zi";
    edit.has_reading = true;
    edit.value.reading = "改过的断语";
    const auto updated = drive(*io, service.update(edit, actor));
    if (!updated || updated->title != "张三-事业（改）")
        return 9;
    if (!updated->payload.has_value() || *updated->payload != R"({"analysisZh":"日主丁火"})")
        return 10;
    if (!updated->reading.has_value() || *updated->reading != "改过的断语")
        return 11;

    // 5. report_text：优先读 payload 里的 analysisZh。
    const auto report = drive(*io, service.report_text(*updated));
    if (!report || !report->has_value() || **report != "日主丁火")
        return 12;

    // 6. 存档：写入→检索（规范化）→删除→再删报错；report_text 回退到存档快照。
    const auto archive_id =
        drive(*io, archives.archive("bazi",
            R"({"title":"x","snapshot":"{\"analysisZh\":\"存档报告\"}"})", "张三", 1));
    if (!archive_id)
        return 13;
    cnetnexus::chart::archive_query search;
    search.algorithm = "bazi";
    const auto found = drive(*io, archives.search(search));
    if (!found || found->total != 1 || found->items.front().algorithm != "ba_zi")
        return 14;
    search.keyword = "张三";
    if (const auto by_keyword = drive(*io, archives.search(search)); !by_keyword || by_keyword->total != 1)
        return 15;
    search = {};
    search.keyword = "不存在";
    if (const auto miss = drive(*io, archives.search(search)); !miss || miss->total != 0)
        return 16;

    auto linked = *created;
    linked.payload.reset();
    linked.archive_id = *archive_id;
    const auto archived_report = drive(*io, service.report_text(linked));
    if (!archived_report || !archived_report->has_value() || **archived_report != "存档报告")
        return 17;

    if (const auto removed = drive(*io, archives.remove("ba_zi", *archive_id)); !removed)
        return 18;
    const auto missing = drive(*io, archives.remove("ba_zi", *archive_id));
    if (missing || missing.error() != error_code::archive_not_found)
        return 19;

    // 7. 审计：每次写操作都必须落一行（玄学用 audit_service，CNetNexus 用审计契约）。
    if (audit.calls == 0)
        return 20;

    // 8. 删除案例后查不到；标签删除后再删报错。
    if (const auto removed = drive(*io, service.remove(1, actor)); !removed)
        return 21;
    const auto gone = drive(*io, service.find(1));
    if (gone || gone.error() != error_code::case_not_found)
        return 22;
    if (const auto removed = drive(*io, service.remove_tag(1, actor)); !removed)
        return 23;
    const auto gone_tag = drive(*io, service.remove_tag(1, actor));
    if (gone_tag || gone_tag.error() != error_code::tag_not_found)
        return 24;
    return 0;
}
