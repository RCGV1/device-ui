#include "HeadlessDisplayDriver.h"
#include "X11MuiSimulator.h"
#include "graphics/common/NodeStore.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#ifdef DEVICE_UI_HEADLESS_TEST
TEST_CASE("headless display composes inclusive partial RGB565 flushes into a completed PPM frame")
{
    HeadlessDisplayDriver driver;
    driver.resetCaptureForTesting();

    const std::array<uint16_t, 4> firstPixels = {0xf800, 0x07e0, 0x001f, 0xffff};
    const lv_area_t firstArea = {1, 2, 2, 3};
    driver.capturePartialFlushForTesting(firstArea, reinterpret_cast<const uint8_t *>(firstPixels.data()), false);

    CHECK_FALSE(driver.frameCompleteForTesting());
    const auto &beforeFinalFlush = driver.rgbFrameForTesting();
    CHECK(beforeFinalFlush[(2 * MUI_TEST_DISPLAY_WIDTH + 1) * 3] > 240);
    CHECK(beforeFinalFlush[(2 * MUI_TEST_DISPLAY_WIDTH + 1) * 3 + 1] < 10);
    CHECK(beforeFinalFlush[(2 * MUI_TEST_DISPLAY_WIDTH + 1) * 3 + 2] < 10);
    CHECK(beforeFinalFlush[(3 * MUI_TEST_DISPLAY_WIDTH + 2) * 3] > 240);
    CHECK(beforeFinalFlush[(3 * MUI_TEST_DISPLAY_WIDTH + 2) * 3 + 1] > 240);
    CHECK(beforeFinalFlush[(3 * MUI_TEST_DISPLAY_WIDTH + 2) * 3 + 2] > 240);

    const std::array<uint16_t, 1> finalPixel = {0x07e0};
    const lv_area_t finalArea = {MUI_TEST_DISPLAY_WIDTH - 1, MUI_TEST_DISPLAY_HEIGHT - 1, MUI_TEST_DISPLAY_WIDTH - 1,
                                 MUI_TEST_DISPLAY_HEIGHT - 1};
    driver.capturePartialFlushForTesting(finalArea, reinterpret_cast<const uint8_t *>(finalPixel.data()), true);

    REQUIRE(driver.frameCompleteForTesting());
    const auto ppmPath = std::filesystem::temp_directory_path() / "headless-display-driver-frame.ppm";
    REQUIRE(driver.writePpmFrameForTesting(ppmPath.string()));
    std::ifstream ppm(ppmPath, std::ios::binary);
    const std::string contents((std::istreambuf_iterator<char>(ppm)), std::istreambuf_iterator<char>());
    const std::string header =
        "P6\n" + std::to_string(MUI_TEST_DISPLAY_WIDTH) + " " + std::to_string(MUI_TEST_DISPLAY_HEIGHT) + "\n255\n";
    CHECK(contents.rfind(header, 0) == 0);
    CHECK(contents.size() == header.size() + MUI_TEST_DISPLAY_WIDTH * MUI_TEST_DISPLAY_HEIGHT * 3);
    std::filesystem::remove(ppmPath);
}
#endif

#ifdef DEVICE_UI_X11_SIMULATOR
TEST_CASE("X11 simulator uses shared fixtures and injected pointer input scrolls the production node list")
{
    const char *display = std::getenv("DISPLAY");
    if (!display || display[0] == '\0') {
        return;
    }

    X11MuiSimulator simulator;
    REQUIRE(simulator.initialize());

    simulator.populateNodeFixtures(40, 42);

    CHECK(simulator.renderedNodeCountForTesting() == 40);
    if (const auto *first = simulator.nodeForTesting(0xa0000000U)) {
        CHECK(std::string(first->user.short_name) == "Nc66");
        CHECK(std::string(first->user.long_name).empty());
        CHECK(first->lastHeard == 1699992800U);
        CHECK(first->channel == 0);
    }

    const auto before = simulator.nodeListScrollYForTesting();
    const int16_t centerX = static_cast<int16_t>(MUI_TEST_DISPLAY_WIDTH / 2);
    const int16_t dragStartY = static_cast<int16_t>(MUI_TEST_DISPLAY_HEIGHT * 7 / 8);
    const int16_t dragEndY = static_cast<int16_t>(MUI_TEST_DISPLAY_HEIGHT / 8);
    REQUIRE(simulator.injectPointer(centerX, dragStartY, true));
    simulator.pump(50);
    REQUIRE(simulator.injectPointer(centerX, dragEndY, true));
    simulator.pump(50);
    REQUIRE(simulator.injectPointer(centerX, dragEndY, false));
    simulator.pump(100);

    CHECK(simulator.nodeListScrollYForTesting() != before);
}
#endif
