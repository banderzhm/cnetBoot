
module cnetboot.bootstrap.application;

import std;
import cnetmod.application;
import cnetmod.core.log;
import cnetboot.bootstrap.modules;

namespace cnetboot::bootstrap {

auto run(std::string configuration_file) -> int
{
    auto host = cnetmod::application::application_builder{"cnetboot"}
                    .configuration_file(std::move(configuration_file))
                    .enable_auto_configuration()
                    .add_module<platform_module>()
                    .add_module<identity_module>()
                    .add_module<ai_module>()
                    .add_module<chat_module>()
                    .build();
    if (!host)
    {
        logger::critical{"cnetboot: application build failed: {}",
            host.error().describe()};
        return 1;
    }

    const auto served = host->run();
    if (!served)
    {
        logger::critical{"cnetboot: application run failed: {}",
            served.error().message()};
        return 1;
    }
    return 0;
}

} // namespace cnetboot::bootstrap
