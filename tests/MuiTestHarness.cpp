#ifdef DEVICE_UI_HEADLESS_TEST

#include "MuiTestHarness.h"
#include "graphics/common/ViewFactory.h"
#include "graphics/driver/DisplayDriverConfig.h"
#include "graphics/view/TFT/TFTView_320x240.h"
#include <cstring>

const char *firmware_version = "headless-test";

namespace
{
HeadlessDisplayDriver &headlessDisplayDriver()
{
    static HeadlessDisplayDriver driver;
    return driver;
}
} // namespace

MuiTestHarness::MuiTestHarness() : driver(&headlessDisplayDriver()), view(nullptr)
{
    DisplayDriverConfig config(DisplayDriverConfig::device_t::NONE, 320, 240);
    view = static_cast<TFTView_320x240 *>(ViewFactory::createForTesting(config, driver));

    if (!driver->getDisplay()) {
        view->init(nullptr);

        meshtastic_DeviceUIConfig uiConfig{};
        uiConfig.version = 1;
        view->setupUIConfig(uiConfig);
        pump();
    }
}

bool MuiTestHarness::ready()
{
    return view && driver->getDisplay() && lv_screen_active() && view->nodeListRootForTesting();
}

void MuiTestHarness::addNodeFixture(uint32_t nodeId, const char *shortName, const char *longName)
{
    view->addNode(nodeId, 0, shortName, longName, 0, MeshtasticView::eRole::client, true, false);
}

void MuiTestHarness::updatePositionFixture(uint32_t nodeId, int32_t latitude, int32_t longitude, int32_t altitude,
                                           uint32_t satellites, uint32_t precision)
{
    view->updatePosition(nodeId, latitude, longitude, altitude, satellites, precision);
}

void MuiTestHarness::pump(uint32_t elapsedMs)
{
    lv_tick_inc(elapsedMs);
    lv_timer_handler();
}

size_t MuiTestHarness::objectCount() const
{
    return countObjects(lv_screen_active());
}

size_t MuiTestHarness::nodeListObjectCount() const
{
    return countObjects(view->nodeListRootForTesting());
}

size_t MuiTestHarness::countObjects(const lv_obj_t *root)
{
    if (!root) {
        return 0;
    }

    size_t count = 1;
    const uint32_t childCount = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < childCount; ++i) {
        count += countObjects(lv_obj_get_child(root, static_cast<int32_t>(i)));
    }
    return count;
}

#endif
