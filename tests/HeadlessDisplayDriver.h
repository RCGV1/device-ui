#pragma once

#include "graphics/driver/DisplayDriver.h"
#include <array>

class HeadlessDisplayDriver : public DisplayDriver
{
  public:
    HeadlessDisplayDriver();
    void init(DeviceGUI *gui) override;

  private:
    static constexpr size_t drawBufferPixels = 320 * 16;
    static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels);

    std::array<lv_color_t, drawBufferPixels> drawBuffer{};
};
