#define DOCTEST_CONFIG_IMPLEMENT
#include "X11MuiSimulator.h"
#include <charconv>
#include <cstdlib>
#include <doctest/doctest.h>
#include <iostream>
#include <limits>
#include <string_view>

namespace
{
struct Options {
    X11MuiSimulator::Implementation implementation = X11MuiSimulator::Implementation::Legacy;
    size_t nodes = 100;
    uint32_t seed = 42;
    uint32_t runForMs = 0;
};

constexpr std::string_view usage =
    "usage: mui_node_list_simulator --implementation legacy|virtual_candidate [--nodes N] [--seed N] [--run-for-ms N]";

bool displayAvailable()
{
    const char *display = std::getenv("DISPLAY");
    return display && display[0] != '\0';
}

bool parseUnsigned(std::string_view value, uint64_t &parsed)
{
    auto begin = value.data();
    auto end = value.data() + value.size();
    auto result = std::from_chars(begin, end, parsed);
    return result.ec == std::errc{} && result.ptr == end;
}

bool parseOptions(int argc, char **argv, Options &options)
{
    bool sawImplementation = false;
    for (int index = 1; index < argc; index += 2) {
        if (index + 1 >= argc) {
            return false;
        }

        const std::string_view flag(argv[index]);
        const std::string_view value(argv[index + 1]);
        uint64_t parsed = 0;
        if (flag == "--implementation" && !sawImplementation) {
            sawImplementation = true;
            if (value == "legacy") {
                options.implementation = X11MuiSimulator::Implementation::Legacy;
            } else if (value == "virtual_candidate") {
#ifdef DEVICE_UI_MUI_VIRTUAL_NODE_LIST
                options.implementation = X11MuiSimulator::Implementation::VirtualCandidate;
#else
                std::cerr << "virtual_candidate requires ENABLE_MUI_VIRTUAL_NODE_LIST\n";
                return false;
#endif
            } else {
                return false;
            }
        } else if (flag == "--nodes" && parseUnsigned(value, parsed) && parsed > 0 &&
                   parsed <= static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
            options.nodes = static_cast<size_t>(parsed);
        } else if (flag == "--seed" && parseUnsigned(value, parsed) &&
                   parsed <= static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
            options.seed = static_cast<uint32_t>(parsed);
        } else if (flag == "--run-for-ms" && parseUnsigned(value, parsed) &&
                   parsed <= static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
            options.runForMs = static_cast<uint32_t>(parsed);
        } else {
            return false;
        }
    }
    return sawImplementation;
}
} // namespace

int main(int argc, char **argv)
{
    Options options;
    if (!parseOptions(argc, argv, options)) {
        std::cerr << usage << '\n';
        return 2;
    }

    if (!displayAvailable()) {
        std::cerr << "DISPLAY is required to launch the X11 simulator\n";
        return 1;
    }

    X11MuiSimulator simulator;
    if (!simulator.initialize(options.implementation)) {
        std::cerr << "failed to initialize X11 MUI simulator\n";
        return 1;
    }
    simulator.populateLegacyNodeFixtures(options.nodes, options.seed);
    if (options.runForMs > 0) {
        for (uint32_t elapsed = 0; elapsed < options.runForMs; elapsed += 10) {
            simulator.pump(10);
        }
    } else {
        simulator.pumpUntilClosed();
    }
    return 0;
}
