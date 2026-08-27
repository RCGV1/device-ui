#include "NodeListVideo.h"
#define DOCTEST_CONFIG_IMPLEMENT
#include "HeadlessDisplayDriver.h"
#include "MuiTestHarness.h"
#include "meshtastic/mesh.pb.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <doctest/doctest.h>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr uint32_t captureTime = 1700000000U;
constexpr auto framePeriod = std::chrono::milliseconds(1000 / 30);

struct NodeFixture {
    uint32_t id;
    std::string shortName;
    std::string longName;
    uint32_t lastHeard;
    uint8_t role;
    bool hasKey;
    uint8_t channel;
};

std::vector<NodeFixture> makeFixtures(size_t count, uint32_t seed)
{
    std::mt19937 random(seed);
    std::vector<NodeFixture> fixtures;
    fixtures.reserve(count);
    for (size_t index = 0; index < count; ++index) {
        char shortName[5] = {};
        std::snprintf(shortName, sizeof(shortName), "N%03x", static_cast<unsigned>((index + random()) & 0xfffU));
        fixtures.push_back({static_cast<uint32_t>(0xa1000000U + index), shortName, "Capture Node " + std::to_string(index),
                            captureTime - static_cast<uint32_t>(index * 17U), static_cast<uint8_t>(index % 7U), index % 2U == 0,
                            static_cast<uint8_t>(index % 8U)});
    }
    return fixtures;
}

bool writeCaptureFrame(MuiTestHarness &harness, const std::filesystem::path &directory, size_t frame)
{
    auto *driver = harness.displayDriverForTesting();
    driver->resetCaptureForTesting();
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(driver->getDisplay());
    harness.pump(static_cast<uint32_t>(framePeriod.count()));
    if (!driver->frameCompleteForTesting()) {
        return false;
    }
    char filename[32] = {};
    std::snprintf(filename, sizeof(filename), "frame_%04zu.ppm", frame);
    return driver->writePpmFrameForTesting((directory / filename).string());
}

void populateProductionList(MuiTestHarness &harness, const std::vector<NodeFixture> &fixtures)
{
    harness.resetNodeList();
    harness.setCurrentTime(captureTime);
    for (const auto &fixture : fixtures) {
        harness.addNodeFixture(fixture.id, fixture.shortName.c_str(), fixture.longName.c_str(), fixture.lastHeard, fixture.role,
                               fixture.hasKey, false, fixture.channel);
    }
    harness.showNodesScreen();
}

bool captureVideo(const NodeListVideoOptions &options)
{
    std::error_code error;
    const std::filesystem::path directory(options.outputDirectory);
    std::filesystem::create_directories(directory, error);
    if (error) {
        std::cerr << "cannot create output directory: " << error.message() << '\n';
        return false;
    }

    MuiTestHarness harness;
    if (!harness.ready()) {
        std::cerr << "headless MUI view did not initialize\n";
        return false;
    }
    const auto fixtures = makeFixtures(options.nodes, options.seed);
    populateProductionList(harness, fixtures);

    auto deadline = std::chrono::steady_clock::now();
    for (size_t frame = 0; frame < options.outputFrames; ++frame) {
        const size_t fixtureIndex = (frame * fixtures.size()) / options.outputFrames;
        if (!fixtures.empty()) {
            harness.scrollVirtualNodeIntoView(fixtures[fixtureIndex].id);
        }
        if (!writeCaptureFrame(harness, directory, frame)) {
            std::cerr << "LVGL did not complete frame " << frame << '\n';
            return false;
        }
        deadline += framePeriod;
        std::this_thread::sleep_until(deadline);
    }
    return true;
}
} // namespace

int main(int argc, char **argv)
{
    NodeListVideoOptions options{};
    std::string error;
    if (!parseNodeListVideoCommandLine(argc, argv, options, error)) {
        std::cerr << "usage: node_list_video --implementation production --nodes 1..250 --seed N "
                     "--output-frames N --output-dir PATH\n"
                  << error << '\n';
        return 2;
    }
    return captureVideo(options) ? 0 : 1;
}
