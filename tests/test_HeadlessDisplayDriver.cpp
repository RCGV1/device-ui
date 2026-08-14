#include "HeadlessDisplayDriver.h"
#include <array>
#include <cstdint>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>

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
    CHECK(beforeFinalFlush[(2 * 320 + 1) * 3] > 240);
    CHECK(beforeFinalFlush[(2 * 320 + 1) * 3 + 1] < 10);
    CHECK(beforeFinalFlush[(2 * 320 + 1) * 3 + 2] < 10);
    CHECK(beforeFinalFlush[(3 * 320 + 2) * 3] > 240);
    CHECK(beforeFinalFlush[(3 * 320 + 2) * 3 + 1] > 240);
    CHECK(beforeFinalFlush[(3 * 320 + 2) * 3 + 2] > 240);

    const std::array<uint16_t, 1> finalPixel = {0x07e0};
    const lv_area_t finalArea = {319, 239, 319, 239};
    driver.capturePartialFlushForTesting(finalArea, reinterpret_cast<const uint8_t *>(finalPixel.data()), true);

    REQUIRE(driver.frameCompleteForTesting());
    const auto ppmPath = std::filesystem::temp_directory_path() / "headless-display-driver-frame.ppm";
    REQUIRE(driver.writePpmFrameForTesting(ppmPath.string()));
    std::ifstream ppm(ppmPath, std::ios::binary);
    const std::string contents((std::istreambuf_iterator<char>(ppm)), std::istreambuf_iterator<char>());
    CHECK(contents.rfind("P6\n320 240\n255\n", 0) == 0);
    CHECK(contents.size() == std::string("P6\n320 240\n255\n").size() + 320 * 240 * 3);
    std::filesystem::remove(ppmPath);
}
#endif
