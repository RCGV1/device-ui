#ifdef DEVICE_UI_HEADLESS_TEST

#include "HeadlessDisplayDriver.h"

HeadlessDisplayDriver::HeadlessDisplayDriver() : DisplayDriver(320, 240) {}

void HeadlessDisplayDriver::init(DeviceGUI *gui)
{
    DisplayDriver::init(gui);
    display = lv_display_create(screenWidth, screenHeight);
    lv_display_set_buffers(display, drawBuffer.data(), nullptr, sizeof(drawBuffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
}

void HeadlessDisplayDriver::flush(lv_display_t *display, const lv_area_t *, uint8_t *)
{
    lv_display_flush_ready(display);
}

#endif
