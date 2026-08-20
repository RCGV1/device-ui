#ifdef DEVICE_UI_X11_SIMULATOR

#include "X11MuiSimulator.h"
#include "graphics/common/ViewFactory.h"
#include "graphics/driver/DisplayDriverFactory.h"
#include "graphics/driver/X11Driver.h"
#include "graphics/view/TFT/TFTView_320x240.h"
#include "meshtastic/mesh.pb.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>
#include <thread>

const char *firmware_version = "x11-simulator";

DisplayDriverFactory::DisplayDriverFactory() {}

DisplayDriver *DisplayDriverFactory::create(uint16_t width, uint16_t height)
{
    return &X11Driver::create(width, height);
}

DisplayDriver *DisplayDriverFactory::create(const DisplayDriverConfig &config)
{
    return &X11Driver::create(config.width(), config.height());
}

X11MuiSimulator::X11MuiSimulator()
    : config(DisplayDriverConfig::device_t::X11, 320, 240), driver(nullptr), view(nullptr), pointerInput(nullptr),
      keyboardInput(nullptr), encoderInput(nullptr)
{
}

bool X11MuiSimulator::initialize()
{
    if (ready()) {
        return true;
    }

    driver = &X11Driver::create(320, 240);
    view = static_cast<TFTView_320x240 *>(ViewFactory::createForTesting(config, driver));
    view->init(nullptr);

    meshtastic_DeviceUIConfig uiConfig{};
    uiConfig.version = 1;
    view->setupUIConfig(uiConfig);
    scanInputs();
    pump(50);
    return ready();
}

bool X11MuiSimulator::ready() const
{
    return view && driver && driver->getDisplay() && lv_screen_active() && view->nodeListRootForTesting();
}

void X11MuiSimulator::populateLegacyNodeFixtures(size_t count, uint32_t seed)
{
    view->resetNodeListForTesting();
    view->setCurrentTimeForTesting(1700000000U);

    std::mt19937 random(seed);
    for (size_t index = 0; index < count; ++index) {
        char shortName[5] = {};
        char longName[32] = {};
        std::snprintf(shortName, sizeof(shortName), "N%03x", static_cast<unsigned>((index + random()) & 0xfffU));
        std::snprintf(longName, sizeof(longName), "Simulator Node %zu", index);
        view->addNode(static_cast<uint32_t>(0xa1000000U + index), static_cast<uint8_t>(index % 8U), shortName, longName,
                      1700000000U - static_cast<uint32_t>(index * 17U), static_cast<MeshtasticView::eRole>(index % 7U),
                      index % 2U == 0, false);
    }
    view->showNodesScreenForTesting();
    pump(50);
}

void X11MuiSimulator::pumpUntilClosed()
{
    while (ready()) {
        pump(10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void X11MuiSimulator::pump(uint32_t elapsedMs)
{
    lv_tick_inc(elapsedMs);
    lv_timer_handler();
}

void X11MuiSimulator::scanInputs()
{
    pointerInput = nullptr;
    keyboardInput = nullptr;
    encoderInput = nullptr;
    for (lv_indev_t *input = lv_indev_get_next(nullptr); input; input = lv_indev_get_next(input)) {
        switch (lv_indev_get_type(input)) {
        case LV_INDEV_TYPE_POINTER:
            pointerInput = input;
            break;
        case LV_INDEV_TYPE_KEYPAD:
            keyboardInput = input;
            break;
        case LV_INDEV_TYPE_ENCODER:
            encoderInput = input;
            break;
        default:
            break;
        }
    }
}

#endif
