module cnetnexus.chart.service.xuanxue.zi_wei_adapter;

import std;
import nlohmann.json;
import ZhouYi.GanZhi;
import ZhouYi.ZhMapper;
import ZhouYi.ZiWei;
import ZhouYi.ZiWei.Constants;
import ZhouYi.ZiWei.Controller;
import ZhouYi.ZiWei.Horoscope;
import ZhouYi.ZiWei.SanFang;
import ZhouYi.ZiWei.SiHua;
import cnetnexus.chart.domain.zi_wei;
import cnetnexus.framework.error;
import cnetnexus.chart.service.xuanxue.json_bridge;

namespace cnetnexus::chart {

namespace {

    /// @brief 把宫位神煞 optional 枚举翻成中文；无值时输出 null，不落占位符。
    ///  中文名一律取上游 ZhMap，适配层不自建对照表。
    template <typename enum_type>
    auto shen_sha_text(const std::optional<enum_type>& value) -> nlohmann::ordered_json
    {
        if (!value)
            return nullptr;
        return std::string{ZhouYi::Mapper::to_zh(*value)};
    }

    /// @brief 把运限四化条目投影成 [{star, hua}]，顺序沿用上游（固定禄→权→科→忌）。
    ///
    /// 上游把四化表扩到十四主星加左辅、右弼、文昌、文曲，且迭代顺序即禄权科忌；
    /// 中文一律取上游 ZhMap（SiHuaXing 出星名、SiHua 出化象），适配层不自建对照表。
    auto si_hua_array(const std::vector<ZhouYi::ZiWei::SiHuaEntry>& entries) -> nlohmann::ordered_json
    {
        auto array = nlohmann::ordered_json::array();
        for (const auto& entry : entries)
            array.push_back({
                {"star", std::string{ZhouYi::Mapper::to_zh(entry.xing)}},
                {"hua", std::string{ZhouYi::Mapper::to_zh(entry.hua)}},
            });
        return array;
    }

    /// @brief 在 export_to_json_full 的结果上投影十二宫页脚字段。
    ///
    /// 只新增键，不改上游既有键名与嵌套结构：十二长生 chang_sheng、博士十二神
    /// bo_shi、岁前十二神 sui_qian、将前十二神 jiang_qian 取自上游
    /// ZhouYi::ZiWei::PalaceInfo 的神煞枚举；三方四正索引 san_fang_si_zheng 取自
    /// 上游 ZhouYi::ZiWei::get_san_fang_si_zheng()，不在适配层重算宫位关系。
    auto append_palace_footer_fields(nlohmann::ordered_json& chart,
        const ZhouYi::ZiWei::ZiWeiResult& result) -> void
    {
        auto& palaces = chart["palaces"];
        if (!palaces.is_array() || palaces.size() != std::size(result.palaces))
            return;

        for (std::size_t index = 0; index < std::size(result.palaces); ++index)
        {
            const auto& source = result.palaces[index];
            auto& target = palaces[index];

            target["chang_sheng"] = shen_sha_text(source.chang_sheng);
            target["bo_shi"] = shen_sha_text(source.bo_shi);
            target["sui_qian"] = shen_sha_text(source.sui_qian);
            target["jiang_qian"] = shen_sha_text(source.jiang_qian);

            const auto san_fang = ZhouYi::ZiWei::get_san_fang_si_zheng(static_cast<int>(index));
            target["san_fang_si_zheng"] = {
                {"ben_gong_index", san_fang.ben_gong_index},
                {"dui_gong_index", san_fang.dui_gong_index},
                {"cai_bo_index", san_fang.cai_bo_index},
                {"guan_lu_index", san_fang.guan_lu_index},
            };
        }
    }

    /// @brief 干支中文串（上游 GanZhi::Mapper 的天干、地支映射拼接）。
    auto gan_zhi_zh(ZhouYi::GanZhi::TianGan gan, ZhouYi::GanZhi::DiZhi zhi) -> std::string
    {
        return std::string{ZhouYi::GanZhi::Mapper::to_zh(gan)} + std::string{ZhouYi::GanZhi::Mapper::to_zh(zhi)};
    }

    /// @brief 单组运限流曜：12 条 {gongIndex, stars}。
    ///
    /// gongIndex 原样取上游 HoroscopeStarData::gong_index（寅起宫序，与 palaces[] 同一空间）；
    /// stars 是上游给的星名数组，该宫没有流曜时就是空数组——原样输出，不代填、不补占位符。
    auto star_group(const std::array<ZhouYi::ZiWei::HoroscopeStarData, 12>& group) -> nlohmann::ordered_json
    {
        auto array = nlohmann::ordered_json::array();
        for (const auto& entry : group)
            array.push_back({
                {"gongIndex", entry.gong_index},
                {"stars", entry.stars},
            });
        return array;
    }

    /// @brief 四化化象的中文单字：禄 / 权 / 科 / 忌。
    ///
    /// 冻结契约给 hua 的取值是单个汉字，上游 ZhMap<SiHua> 给的是「化禄」…「化忌」；这里只删掉
    /// 四象共用的前缀「化」，仍以上游映射为唯一来源，不在适配层另立第二份对照表。
    auto hua_text(ZhouYi::ZiWei::SiHua hua) -> std::string_view
    {
        constexpr std::string_view prefix = "化";
        const auto text = ZhouYi::Mapper::to_zh(hua);
        return text.starts_with(prefix) ? text.substr(prefix.size()) : text;
    }

    /// @brief 本盘十二宫的「星名 → 宫位下标」查找表；值在 palaces[] 空间（0 = 寅宫）。
    ///
    /// 覆盖主星、辅星、煞星、杂曜四组，与上游 SiHuaSystem 判定落宫时的星曜集合一致；同盘同
    /// 星名唯一，落宫没有歧义，万一重复也保留先出现的宫。键指向 chart.palaces[] 内部的字符串，
    /// 生命周期与 chart 相同。
    auto star_palace_index(const ZhouYi::ZiWei::ZiWeiResult& chart) -> std::unordered_map<std::string_view, int>
    {
        std::unordered_map<std::string_view, int> index;
        index.reserve(64);
        for (int palace = 0; palace < 12; ++palace)
        {
            const auto& info = chart.palaces[static_cast<std::size_t>(palace)];
            const auto add_group = [&index, palace](const auto& stars)
            {
                for (const auto& star : stars)
                    index.try_emplace(star.name, palace);
            };
            add_group(info.zhu_xing);
            add_group(info.fu_xing);
            add_group(info.sha_xing);
            add_group(info.za_yao);
        }
        return index;
    }

    /// @brief 十二宫宫名（中文），下标与 palaces[] 一致（0 = 寅宫）。
    ///
    /// 宫名随命宫位置转动，逐宫取 PalaceInfo::gong_data.gong_wei 再翻中文，不按枚举下标硬编码；
    /// 返回值指向上游 ZhMap<GongWei> 的静态字面量，不依赖 chart 生命周期。
    auto palace_names(const ZhouYi::ZiWei::ZiWeiResult& chart) -> std::array<std::string_view, 12>
    {
        std::array<std::string_view, 12> names{};
        for (std::size_t index = 0; index < names.size(); ++index)
            names[index] = ZhouYi::Mapper::to_zh(chart.palaces[index].gong_data.gong_wei);
        return names;
    }

    /// @brief 投影一组四化：固定 禄→权→科→忌，每条给出化星与落宫。
    ///
    /// 索引空间：targetPalaceIndex 与 targetPalaceName 都取 palaces[] 下标（0 = 寅宫），与
    /// chart.palaces[]、宫位 opposite_index、horoscope.gongIndex 同一空间，**不是**「子 = 0」的
    /// 地支序；化星不在本盘十二宫内时两个字段一并给 null，不用 -1 或编号占位。
    ///
    /// self：化星落回本宫（宫干自化，与上游 ZiHuaInfo 同一口径）时为 true。只有本命十二宫这
    /// 一组的条目带着本宫下标 from_palace；生年/大限/流年三组的条目挂在干支上、本身不含本宫，
    /// 故恒为 false。
    ///
    /// @param entries 四化条目：本命与生年取上游 get_si_hua_entries(TianGan)，运限取上游已算好
    ///  的 HoroscopeResult::si_hua_entries，适配层不重写四化表
    /// @param star_palace 星名 → 落宫下标
    /// @param names 十二宫宫名，下标与 palaces[] 一致
    /// @param from_palace 本宫下标；负值表示该组没有本宫，self 恒为 false
    auto fly_star_items(const std::vector<ZhouYi::ZiWei::SiHuaEntry>& entries,
        const std::unordered_map<std::string_view, int>& star_palace,
        const std::array<std::string_view, 12>& names, int from_palace) -> nlohmann::ordered_json
    {
        // 冻结契约把 items 顺序固定成禄→权→科→忌，这里按 SiHua 枚举值（禄 0、权 1、科 2、忌 3）
        // 稳定排序，使顺序不依赖上游返回次序；同一化象下多星的相对次序不变。
        auto ordered = entries;
        std::ranges::stable_sort(ordered, {}, &ZhouYi::ZiWei::SiHuaEntry::hua);

        auto items = nlohmann::ordered_json::array();
        for (const auto& entry : ordered)
        {
            const std::string star{ZhouYi::Mapper::to_zh(entry.xing)};
            const auto found = star_palace.find(star);
            const int target = found == star_palace.end() ? -1 : found->second;
            const auto target_index = target >= 0 ? nlohmann::ordered_json(target) : nlohmann::ordered_json(nullptr);
            const auto target_name = target >= 0
                ? nlohmann::ordered_json(std::string{names[static_cast<std::size_t>(target)]})
                : nlohmann::ordered_json(nullptr);

            auto item = nlohmann::ordered_json::object();
            item["hua"] = std::string{hua_text(entry.hua)};
            item["star"] = star;
            item["targetPalaceIndex"] = target_index;
            item["targetPalaceName"] = target_name;
            item["self"] = target >= 0 && target == from_palace;
            items.push_back(std::move(item));
        }
        return items;
    }

    /// @brief 飞星四化投影（chart.flyStars）：宫干 → 化星 → 落宫，供前端画飞星四化连线。
    ///
    /// 结构（键名一律 camelCase，只新增键、不动上游既有键与嵌套结构）：
    ///  - palaces：12 项，顺序与 chart.palaces[] 完全一致，每宫
    ///    {palaceIndex, palaceName, gongGan, items}；
    ///  - year：生年四化 {gan, items}，天干取上游生年天干（ZiWeiResult::year_pillar.gan）；
    ///  - daXian / liuNian：请求带 target_year（即调用方持有 horoscope）时才出现，
    ///    {ganZhi, items} 的干支与上游 get_horoscope 的大限、流年同源。
    ///
    /// 索引空间：palaceIndex 与 items[].targetPalaceIndex 都是 palaces[] 下标（0 = 寅宫），与
    /// chart.palaces[]、opposite_index 同一空间，**不是**「子 = 0」的地支序；化星不在十二宫内
    /// 给 null。四化表不重写：本命十二宫与生年取上游 get_si_hua_entries(TianGan)，运限两组取
    /// horoscope 已算好的 si_hua_entries（同一份表的产物）。
    ///
    /// @param chart 上游排盘结果（本命十二宫与生年天干）
    /// @param horoscope 请求带 target_year 时的运限结果；nullptr 表示未请求运限，此时不投影
    ///  daXian/liuNian 两个键，前端按缺键降级（不画线、不显示开关）
    auto fly_stars_json(const ZhouYi::ZiWei::ZiWeiResult& chart, const ZhouYi::ZiWei::HoroscopeResult* horoscope)
        -> nlohmann::ordered_json
    {
        const auto star_palace = star_palace_index(chart);
        const auto names = palace_names(chart);

        auto palaces = nlohmann::ordered_json::array();
        for (int index = 0; index < 12; ++index)
        {
            const auto& palace = chart.palaces[static_cast<std::size_t>(index)];
            palaces.push_back({
                {"palaceIndex", index},
                {"palaceName", std::string{names[static_cast<std::size_t>(index)]}},
                {"gongGan", std::string{ZhouYi::GanZhi::Mapper::to_zh(palace.gong_data.tian_gan)}},
                {"items",
                    fly_star_items(ZhouYi::ZiWei::get_si_hua_entries(palace.gong_data.tian_gan), star_palace, names, index)},
            });
        }

        nlohmann::ordered_json payload;
        payload["palaces"] = std::move(palaces);
        payload["year"] = {
            {"gan", std::string{ZhouYi::GanZhi::Mapper::to_zh(chart.year_pillar.gan)}},
            {"items", fly_star_items(ZhouYi::ZiWei::get_si_hua_entries(chart.year_pillar.gan), star_palace, names, -1)},
        };
        if (horoscope != nullptr)
        {
            payload["daXian"] = {
                {"ganZhi", gan_zhi_zh(horoscope->da_xian.tian_gan, horoscope->da_xian.di_zhi)},
                {"items", fly_star_items(horoscope->da_xian.si_hua_entries, star_palace, names, -1)},
            };
            payload["liuNian"] = {
                {"ganZhi", gan_zhi_zh(horoscope->liu_nian.tian_gan, horoscope->liu_nian.di_zhi)},
                {"items", fly_star_items(horoscope->liu_nian.si_hua_entries, star_palace, names, -1)},
            };
        }
        return payload;
    }

    /// @brief 目标农历月/日/时的解析结果；三项成套，缺省时沿用正月初一子时。
    struct target_moment
    {
        bool provided{false};                                  ///< 三项是否齐备；只有齐备时才投影流月/流日/流时与对应流曜
        int lunar_month{1};                                    ///< 目标农历月 1—12
        int lunar_day{1};                                      ///< 目标农历日 1—30
        ZhouYi::GanZhi::DiZhi hour{ZhouYi::GanZhi::DiZhi::Zi}; ///< 目标时辰地支
    };

    /// @brief 把上游 get_horoscope 的结果投影成 camelCase JSON（真实 nlohmann 对象）。
    ///
    /// 索引空间：palaceTags、daXian/xiaoXian/liuNian/liuYue/liuRi/liuShi 的 gongIndex，以及
    /// transitStars 每组 12 项的 gongIndex 都是 ZiWeiResult::palaces[] 的数组下标（0 = 寅宫），
    /// **不是** DiZhi 枚举值；与既有 chart_json 的宫位下标同一空间。中文标签一律取上游 ZhMap
    /// （ZhouYi::Mapper::to_zh / ZhouYi::GanZhi::Mapper::to_zh），适配层不自建对照表。
    ///
    /// 目标农历月/日/时（target_moment）：三项齐备时按调用方给的真实农历月/日/时调用上游，
    /// 并投影 liuYue/liuRi/liuShi 与 transitStars 的对应 scope；缺省时固定传必然合法的正月初一
    /// 子时：既不借用出生日期伪装成用户输入，也不让它影响已投影的大限／小限／流年／十二宫角标
    /// （这些只取决于 target_year 与 current_age），同时不投影那三个键与 scope，避免发出
    /// 用占位日期算出来的无意义数据。
    ///
    /// 已投影（上游修复后照实投影，不补算、不伪造）：大限干支取该宫真实宫干支、五组运限四化
    /// 明细（固定禄权科忌顺序）、liuNian.ganZhi（公共 tyme 干支年换算）、五组运限流曜星名
    /// （上游 HoroscopeResult 的 da_xian_stars / liu_nian_stars / liu_yue_stars / liu_ri_stars /
    /// liu_shi_stars）。
    ///
    /// 运限结果由调用方（pai_pan）算一次后传入：chart.flyStars 的 daXian/liuNian 四化与这里的
    /// 大限／流年同源，两处共用同一个上游结果，不重复调用 get_horoscope。
    auto horoscope_payload(int target_year, int current_age, const target_moment& moment,
        const ZhouYi::ZiWei::HoroscopeResult& horoscope) -> nlohmann::ordered_json
    {
        nlohmann::ordered_json payload;
        payload["targetYear"] = target_year;
        payload["currentAge"] = current_age;

        payload["daXian"] = {
            {"gongIndex", horoscope.da_xian.gong_index},
            {"startAge", horoscope.da_xian.start_age},
            {"endAge", horoscope.da_xian.end_age},
            {"tianGan", std::string{ZhouYi::GanZhi::Mapper::to_zh(horoscope.da_xian.tian_gan)}},
            {"diZhi", std::string{ZhouYi::GanZhi::Mapper::to_zh(horoscope.da_xian.di_zhi)}},
            {"siHua", si_hua_array(horoscope.da_xian.si_hua_entries)},
        };
        payload["xiaoXian"] = {
            {"age", horoscope.xiao_xian.age},
            {"gongIndex", horoscope.xiao_xian.gong_index},
        };
        // 流年干支由上游公共 tyme 干支年模块换算（映射失败即抛异常，不静默降级），与生年干支
        // 同源，因此可以投影。
        payload["liuNian"] = {
            {"year", horoscope.liu_nian.year},
            {"ganZhi", gan_zhi_zh(horoscope.liu_nian.tian_gan, horoscope.liu_nian.di_zhi)},
            {"gongIndex", horoscope.liu_nian.gong_index},
            {"siHua", si_hua_array(horoscope.liu_nian.si_hua_entries)},
        };

        // 流月／流日／流时只在请求给了成套的农历月/日/时才投影：没请求就整键不出现，不把上游拿
        // “正月初一子时”算出来的占位结果冒充用户输入。干支与四化口径与流年一致。
        if (moment.provided)
        {
            payload["liuYue"] = {
                {"lunarMonthIndex", horoscope.liu_yue.month},
                {"ganZhi", gan_zhi_zh(horoscope.liu_yue.tian_gan, horoscope.liu_yue.di_zhi)},
                {"gongIndex", horoscope.liu_yue.gong_index},
                {"siHua", si_hua_array(horoscope.liu_yue.si_hua_entries)},
            };
            payload["liuRi"] = {
                {"lunarDay", horoscope.liu_ri.day},
                {"ganZhi", gan_zhi_zh(horoscope.liu_ri.tian_gan, horoscope.liu_ri.di_zhi)},
                {"gongIndex", horoscope.liu_ri.gong_index},
                {"siHua", si_hua_array(horoscope.liu_ri.si_hua_entries)},
            };
            payload["liuShi"] = {
                {"hourBranch", std::string{ZhouYi::GanZhi::Mapper::to_zh(horoscope.liu_shi.shi_chen)}},
                {"ganZhi", gan_zhi_zh(horoscope.liu_shi.tian_gan, horoscope.liu_shi.di_zhi)},
                {"gongIndex", horoscope.liu_shi.gong_index},
                {"siHua", si_hua_array(horoscope.liu_shi.si_hua_entries)},
            };
        }

        // 目标流年口径的岁前／将前：取上游 PalaceHoroscopeTag 的枚举再翻中文，不另算口诀。
        auto palace_tags = nlohmann::ordered_json::array();
        for (const auto& tag : horoscope.palace_tags)
        {
            palace_tags.push_back({
                {"gongIndex", tag.gong_index},
                {"isDaXian", tag.is_da_xian},
                {"isXiaoXian", tag.is_xiao_xian},
                {"isLiuNian", tag.is_liu_nian},
                {"suiQian", std::string{ZhouYi::Mapper::to_zh(tag.sui_qian)}},
                {"jiangQian", std::string{ZhouYi::Mapper::to_zh(tag.jiang_qian)}},
            });
        }
        payload["palaceTags"] = std::move(palace_tags);

        // 五组运限流曜：daXian/liuNian 恒有（只取决于大限干支与流年干支），liuYue/liuRi/liuShi
        // 与上面同名键同步出现——不越权补算，也不给未请求的 scope 造数据。
        auto transit_stars = nlohmann::ordered_json::object();
        transit_stars["daXian"] = star_group(horoscope.da_xian_stars);
        transit_stars["liuNian"] = star_group(horoscope.liu_nian_stars);
        if (moment.provided)
        {
            transit_stars["liuYue"] = star_group(horoscope.liu_yue_stars);
            transit_stars["liuRi"] = star_group(horoscope.liu_ri_stars);
            transit_stars["liuShi"] = star_group(horoscope.liu_shi_stars);
        }
        payload["transitStars"] = std::move(transit_stars);
        return payload;
    }

} // namespace

auto zi_wei_adapter::pai_pan(const cnetnexus::chart::zi_wei::chart_input& input)
    -> std::expected<cnetnexus::chart::zi_wei::chart_result, cnetnexus::framework::error::error_code>
{
    // 目标农历月/日/时是成套参数：service 已按契约判过组合与范围，这里在边界再自检一次，
    // 直接调用本适配器的调用方（如测试）也不会因为“只给一半”拿到伪装成默认值的结果。
    // 时辰地支汉字→地支取上游既有映射 ZhouYi::GanZhi::Mapper::from_zh_zhi，本仓库不维护
    // 第二份对照表；非地支汉字（含空串）与上游一样按 REQUEST.INVALID_ARGUMENT 拒绝。
    const bool has_target_month = input.target_lunar_month.has_value();
    const bool has_target_day = input.target_lunar_day.has_value();
    const bool has_target_hour = input.target_hour.has_value();
    if (has_target_month != has_target_day || has_target_month != has_target_hour)
        return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    if (has_target_hour && !input.target_year)
        return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};

    target_moment moment;
    if (has_target_hour)
    {
        const auto target_hour = ZhouYi::GanZhi::Mapper::from_zh_zhi(*input.target_hour);
        if (!target_hour)
            return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
        moment = target_moment{.provided = true,
            .lunar_month = *input.target_lunar_month,
            .lunar_day = *input.target_lunar_day,
            .hour = *target_hour};
    }

    // 上游紫微链路不用 std::expected：非法日期由 tyme 抛
    // std::invalid_argument，统一在此边界翻译。
    try
    {
        const auto chart = input.calendar == cnetnexus::chart::zi_wei::calendar_kind::lunar
            ? ZhouYi::ZiWei::pai_pan_lunar(input.year, input.month, input.day, input.hour, input.is_male)
            : ZhouYi::ZiWei::pai_pan_solar(input.year, input.month, input.day, input.hour, input.is_male);

        cnetnexus::chart::zi_wei::chart_result result{};
        result.input = input;
        // 两路输出同源：JSON 给接口调用方，中文文本给大模型提示词投影。
        // 上游 export_to_json_full 不含十二宫页脚字段，按契约在此补投影；
        // 解析成真实 JSON 对象后再加键，不做字符串拼接。
        auto chart_json = nlohmann::ordered_json::parse(ZhouYi::ZiWei::export_to_json_full(chart));
        append_palace_footer_fields(chart_json, chart);
        // 飞星四化同样在真实 JSON 对象上加键，且加在既有键之后：既有键名、含义与位置一个不动。
        // daXian/liuNian 两组与根键 horoscope 同源，所以请求带 target_year 时先把上游运限算
        // 一次，flyStars 与 horoscope 两处共用，不重复调用 get_horoscope。
        if (input.target_year)
        {
            // 虚岁：调用方显式给值优先；否则 target_year − 出生年 + 1（虚岁起算）。
            // 推导结果小于 1 时由 get_horoscope 抛 invalid_argument，此处统一翻译成 400。
            const int current_age = input.current_age.value_or(*input.target_year - input.year + 1);
            const auto horoscope =
                chart.get_horoscope(*input.target_year, moment.lunar_month, moment.lunar_day, moment.hour, current_age);
            chart_json["flyStars"] = fly_stars_json(chart, &horoscope);
            auto horoscope_projection = parse_provider_object(
                horoscope_payload(*input.target_year, current_age, moment, horoscope).dump());
            if (!horoscope_projection)
                return std::unexpected{
                    cnetnexus::framework::error::error_code::algorithm_failed};
            result.horoscope_projection = std::move(*horoscope_projection);
        }
        else
        {
            // 没请求运限：flyStars 只投影本命十二宫与生年四化，daXian/liuNian 整键不出现，
            // 前端按缺键降级。
            chart_json["flyStars"] = fly_stars_json(chart, nullptr);
        }
        auto chart_projection = parse_provider_object(chart_json.dump());
        if (!chart_projection)
            return std::unexpected{
                cnetnexus::framework::error::error_code::algorithm_failed};
        result.chart_projection = std::move(*chart_projection);
        result.report_zh = chart.to_string();
        if (result.report_zh.empty())
            return std::unexpected{cnetnexus::framework::error::error_code::algorithm_failed};
        return result;
    }
    catch (const std::invalid_argument&)
    {
        return std::unexpected{cnetnexus::framework::error::error_code::request_invalid_argument};
    }
    catch (const std::exception&)
    {
        return std::unexpected{cnetnexus::framework::error::error_code::algorithm_failed};
    }
}

} // namespace cnetnexus::chart
