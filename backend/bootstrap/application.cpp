/**
 * @brief run() 的实现：配置 -> 模块 -> build() -> run()。
 *
 * 所有业务对象都是组件容器里的组件，由 host 持有并与 host 同寿命；这里不再有手工排序
 * 的装配结构体，也不再有「先建中间件、后填依赖」的两段式初始化。构建失败时
 * build_error::describe() 给出阶段、组件链与配置路径。
 */
module cnetnexus.bootstrap.application;

import std;
import cnetmod.application;
import cnetmod.core.log;
import cnetnexus.bootstrap.modules;

namespace cnetnexus::bootstrap {

auto run(std::string configuration_file) -> int
{
    auto host = cnetmod::application::application_builder{"cnetnexus"}
                    .configuration_file(std::move(configuration_file))
                    .enable_auto_configuration()
                    .add_module<platform_module>()
                    .add_module<identity_module>()
                    .add_module<reading_module>()
                    .add_module<chart_module>()
                    .add_module<chat_module>()
                    .build();
    if (!host)
    {
        logger::critical{"cnetnexus: application build failed: {}",
            host.error().describe()};
        return 1;
    }

    const auto served = host->run();
    if (!served)
    {
        logger::critical{"cnetnexus: application run failed: {}",
            served.error().message()};
        return 1;
    }
    return 0;
}

} // namespace cnetnexus::bootstrap
