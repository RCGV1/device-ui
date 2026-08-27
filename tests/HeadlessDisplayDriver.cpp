#ifdef DEVICE_UI_HEADLESS_TEST

#include "HeadlessDisplayDriver.h"
#include <algorithm>
#include <cstring>
#include <fstream>

namespace
{
uint8_t scaleRgb565(uint16_t value, uint16_t maximum)
{
    return static_cast<uint8_t>((value * 255U + maximum / 2U) / maximum);
}

void convertPixelToRgb(lv_color_format_t format, const uint8_t *source, uint8_t *destination)
{
    switch (format) {
    case LV_COLOR_FORMAT_RGB565:
    case LV_COLOR_FORMAT_RGB565_SWAPPED: {
        uint16_t packed = 0;
        std::memcpy(&packed, source, sizeof(packed));
        if (format == LV_COLOR_FORMAT_RGB565_SWAPPED) {
            packed = lv_color_swap_16(packed);
        }
        destination[0] = scaleRgb565(static_cast<uint16_t>((packed >> 11U) & 0x1fU), 31U);
        destination[1] = scaleRgb565(static_cast<uint16_t>((packed >> 5U) & 0x3fU), 63U);
        destination[2] = scaleRgb565(static_cast<uint16_t>(packed & 0x1fU), 31U);
        return;
    }
    case LV_COLOR_FORMAT_RGB888:
        destination[0] = source[2];
        destination[1] = source[1];
        destination[2] = source[0];
        return;
    case LV_COLOR_FORMAT_XRGB8888:
    case LV_COLOR_FORMAT_ARGB8888:
    case LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED:
        destination[0] = source[2];
        destination[1] = source[1];
        destination[2] = source[0];
        return;
    default:
        destination[0] = 0;
        destination[1] = 0;
        destination[2] = 0;
        return;
    }
}
} // namespace

HeadlessDisplayDriver::HeadlessDisplayDriver()
    : DisplayDriver(MUI_TEST_DISPLAY_WIDTH, MUI_TEST_DISPLAY_HEIGHT), rgbFrame(screenWidth * screenHeight * 3, 0)
{
}

void HeadlessDisplayDriver::init(DeviceGUI *gui)
{
    DisplayDriver::init(gui);
    display = lv_display_create(screenWidth, screenHeight);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_NATIVE);
    lv_display_set_buffers(display, drawBuffer.data(), nullptr, sizeof(drawBuffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(display, this);
    lv_display_set_flush_cb(display, flush);
}

void HeadlessDisplayDriver::resetCaptureForTesting()
{
    std::fill(rgbFrame.begin(), rgbFrame.end(), 0);
    frameComplete = false;
}

void HeadlessDisplayDriver::capturePartialFlushForTesting(const lv_area_t &area, const uint8_t *pixels, bool finalFlush)
{
    frameComplete = false;
    if (!pixels || area.x2 < area.x1 || area.y2 < area.y1) {
        frameComplete = finalFlush;
        return;
    }

    const int32_t sourceWidth = area.x2 - area.x1 + 1;
    const int32_t sourceHeight = area.y2 - area.y1 + 1;
    const int32_t clippedX1 = std::max<int32_t>(area.x1, 0);
    const int32_t clippedY1 = std::max<int32_t>(area.y1, 0);
    const int32_t clippedX2 = std::min<int32_t>(area.x2, screenWidth - 1);
    const int32_t clippedY2 = std::min<int32_t>(area.y2, screenHeight - 1);
    if (clippedX1 <= clippedX2 && clippedY1 <= clippedY2) {
        const lv_color_format_t format = display ? lv_display_get_color_format(display) : LV_COLOR_FORMAT_NATIVE;
        const size_t pixelBytes = lv_color_format_get_size(format);
        if (pixelBytes > 0) {
            for (int32_t y = clippedY1; y <= clippedY2; ++y) {
                const size_t sourceRow = static_cast<size_t>(y - area.y1) * sourceWidth;
                for (int32_t x = clippedX1; x <= clippedX2; ++x) {
                    const size_t sourceIndex = (sourceRow + static_cast<size_t>(x - area.x1)) * pixelBytes;
                    const size_t destinationIndex = (static_cast<size_t>(y) * screenWidth + x) * 3;
                    convertPixelToRgb(format, pixels + sourceIndex, rgbFrame.data() + destinationIndex);
                }
            }
        }
    }
    (void)sourceHeight;
    frameComplete = finalFlush;
}

bool HeadlessDisplayDriver::frameCompleteForTesting() const
{
    return frameComplete;
}

const std::vector<uint8_t> &HeadlessDisplayDriver::rgbFrameForTesting() const
{
    return rgbFrame;
}

bool HeadlessDisplayDriver::writePpmFrameForTesting(const std::string &path) const
{
    if (!frameComplete) {
        return false;
    }
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return false;
    }
    output << "P6\n" << screenWidth << ' ' << screenHeight << "\n255\n";
    output.write(reinterpret_cast<const char *>(rgbFrame.data()), static_cast<std::streamsize>(rgbFrame.size()));
    return output.good();
}

void HeadlessDisplayDriver::flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    auto *driver = static_cast<HeadlessDisplayDriver *>(lv_display_get_user_data(display));
    if (driver && area) {
        driver->capturePartialFlushForTesting(*area, pixels, lv_display_flush_is_last(display));
    }
    lv_display_flush_ready(display);
}

#endif
