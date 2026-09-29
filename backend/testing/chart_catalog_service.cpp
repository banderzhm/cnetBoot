import std;
import cnetnexus.chart.domain.catalog;
import cnetnexus.chart.service.catalog_service;

// 已发布契约用例：/api/v1/capabilities 的载荷必须逐字稳定。
// 迁移自 xuanxue-algorithm tests/catalog_service_test.cpp：断言逻辑不变，
// 只把命名换成 CNetNexus 口径（xuanxue::service -> cnetnexus::chart，
// xuanxue::domain -> cnetnexus::chart）。
auto main() -> int
{
    const auto capabilities = cnetnexus::chart::capabilities();
    if (capabilities.algorithms != std::vector<std::string>{"ba_zi", "liu_yao", "mei_hua", "da_liu_ren", "qi_men", "zi_wei"})
        return 2;

    if (cnetnexus::chart::capability_catalog().size() != 6)
        return 3;
    if (cnetnexus::chart::capability_catalog().front().key != "ba_zi")
        return 4;
    return 0;
}
