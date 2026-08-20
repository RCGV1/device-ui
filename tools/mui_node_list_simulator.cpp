#define DOCTEST_CONFIG_IMPLEMENT
#include "X11MuiSimulator.h"
#include <cstdlib>
#include <doctest/doctest.h>
#include <iostream>
#include <string_view>

namespace
{
constexpr std::string_view usage = "usage: mui_node_list_simulator --implementation legacy";

bool displayAvailable()
{
    const char *display = std::getenv("DISPLAY");
    return display && display[0] != '\0';
}
} // namespace

int main(int argc, char **argv)
{
    if (argc != 3 || std::string_view(argv[1]) != "--implementation") {
        std::cerr << usage << '\n';
        return 2;
    }

    const std::string_view implementation(argv[2]);
    if (implementation == "virtual_candidate") {
        std::cerr << "virtual_candidate is not integrated into the X11 simulator\n";
        return 2;
    }
    if (implementation != "legacy") {
        std::cerr << usage << '\n';
        return 2;
    }
    if (!displayAvailable()) {
        std::cerr << "DISPLAY is required to launch the X11 simulator\n";
        return 1;
    }

    X11MuiSimulator simulator;
    if (!simulator.initialize()) {
        std::cerr << "failed to initialize legacy X11 MUI simulator\n";
        return 1;
    }
    simulator.populateLegacyNodeFixtures();
    simulator.pumpUntilClosed();
    return 0;
}
