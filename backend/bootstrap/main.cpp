#include <cnetmod/config.hpp>

import std;
import cnetmod.core;
import cnetboot.bootstrap.application;

auto main(int argc, char** argv) -> int
{
    cnetmod::net_init network;
    const auto configuration = argc > 1 ? std::string{argv[1]}
                                        : std::string{"application.yaml"};
    return cnetboot::bootstrap::run(configuration);
}
