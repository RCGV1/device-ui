#ifdef DEVICE_UI_X11_SIMULATOR

#include "X11MuiSimulator.h"
#include "graphics/driver/DisplayDriverFactory.h"
#include "graphics/driver/X11Driver.h"
#include <chrono>
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
    : config(DisplayDriverConfig::device_t::X11, 320, 240), driver(nullptr), pointerInput(nullptr), keyboardInput(nullptr),
      encoderInput(nullptr), originalPointerRead(nullptr), originalKeyboardRead(nullptr), originalEncoderRead(nullptr)
{
}

bool X11MuiSimulator::initialize()
{
    if (ready()) {
        return true;
    }

    driver = &X11Driver::create(320, 240);
    harness = std::make_unique<MuiTestHarness>(config, driver);
    scanInputs();
    pump(50);
    return ready();
}

bool X11MuiSimulator::ready() const
{
    return harness && harness->ready() && driver && driver->getDisplay() && lv_screen_active();
}

void X11MuiSimulator::populateLegacyNodeFixtures(size_t count, uint32_t seed)
{
    harness->populateLegacyNodeFixtures(count, seed);
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

bool X11MuiSimulator::injectPointer(int16_t x, int16_t y, bool pressed)
{
    if (!pointerInput) {
        return false;
    }
    pointerState.point.x = x;
    pointerState.point.y = y;
    pointerState.pressed = pressed;
    pointerState.pending = true;
    lv_indev_read(pointerInput);
    return true;
}

bool X11MuiSimulator::injectKeyboard(uint32_t key)
{
    if (!keyboardInput) {
        return false;
    }
    keyState.key = key;
    keyState.pending = true;
    lv_indev_read(keyboardInput);
    return true;
}

bool X11MuiSimulator::injectEncoder(int16_t diff)
{
    if (!encoderInput) {
        return false;
    }
    encoderState.diff = diff;
    encoderState.pending = true;
    lv_indev_read(encoderInput);
    return true;
}

size_t X11MuiSimulator::renderedNodeCountForTesting() const
{
    return harness ? harness->renderedNodeCount() : 0;
}

const NodeRecord *X11MuiSimulator::nodeForTesting(uint32_t nodeId) const
{
    return harness ? harness->node(nodeId) : nullptr;
}

int32_t X11MuiSimulator::nodeListScrollYForTesting() const
{
    auto *root = harness ? harness->nodeListRootForTesting() : nullptr;
    return root ? lv_obj_get_scroll_y(root) : 0;
}

void X11MuiSimulator::pointerReadCallback(lv_indev_t *indev, lv_indev_data_t *data)
{
    auto *simulator = static_cast<X11MuiSimulator *>(lv_indev_get_user_data(indev));
    if (!simulator || !simulator->pointerState.pending) {
        if (simulator && simulator->originalPointerRead) {
            simulator->originalPointerRead(indev, data);
        }
        return;
    }

    data->point = simulator->pointerState.point;
    data->state = simulator->pointerState.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    simulator->pointerState.pending = false;
}

void X11MuiSimulator::keyboardReadCallback(lv_indev_t *indev, lv_indev_data_t *data)
{
    auto *simulator = static_cast<X11MuiSimulator *>(lv_indev_get_user_data(indev));
    if (!simulator || !simulator->keyState.pending) {
        if (simulator && simulator->originalKeyboardRead) {
            simulator->originalKeyboardRead(indev, data);
        }
        return;
    }

    data->key = simulator->keyState.key;
    data->state = LV_INDEV_STATE_PRESSED;
    simulator->keyState.pending = false;
}

void X11MuiSimulator::encoderReadCallback(lv_indev_t *indev, lv_indev_data_t *data)
{
    auto *simulator = static_cast<X11MuiSimulator *>(lv_indev_get_user_data(indev));
    if (!simulator || !simulator->encoderState.pending) {
        if (simulator && simulator->originalEncoderRead) {
            simulator->originalEncoderRead(indev, data);
        }
        return;
    }

    data->enc_diff = simulator->encoderState.diff;
    data->state = LV_INDEV_STATE_RELEASED;
    simulator->encoderState.pending = false;
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
            originalPointerRead = lv_indev_get_read_cb(input);
            lv_indev_set_user_data(input, this);
            lv_indev_set_read_cb(input, pointerReadCallback);
            break;
        case LV_INDEV_TYPE_KEYPAD:
            keyboardInput = input;
            originalKeyboardRead = lv_indev_get_read_cb(input);
            lv_indev_set_user_data(input, this);
            lv_indev_set_read_cb(input, keyboardReadCallback);
            break;
        case LV_INDEV_TYPE_ENCODER:
            encoderInput = input;
            originalEncoderRead = lv_indev_get_read_cb(input);
            lv_indev_set_user_data(input, this);
            lv_indev_set_read_cb(input, encoderReadCallback);
            break;
        default:
            break;
        }
    }
}

#endif
