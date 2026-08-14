#pragma once

#include "graphics/driver/DisplayDriver.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

class HeadlessDisplayDriver : public DisplayDriver
{
  public:
    HeadlessDisplayDriver();
    void init(DeviceGUI *gui) override;

    void resetCaptureForTesting();
    void capturePartialFlushForTesting(const lv_area_t &area, const uint8_t *pixels, bool finalFlush);
    bool frameCompleteForTesting() const;
    const std::vector<uint8_t> &rgbFrameForTesting() const;
    bool writePpmFrameForTesting(const std::string &path) const;

  private:
    static constexpr size_t drawBufferPixels = 320 * 16;
    static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels);

    std::array<uint8_t, drawBufferPixels * LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_NATIVE)> drawBuffer{};
    std::vector<uint8_t> rgbFrame;
    bool frameComplete = false;
};
