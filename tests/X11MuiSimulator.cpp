#ifdef DEVICE_UI_X11_SIMULATOR

#include "X11MuiSimulator.h"
#include "graphics/driver/DisplayDriverFactory.h"
#include "graphics/driver/X11Driver.h"
#include "graphics/view/TFT/TFTView_320x240.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
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

namespace
{
bool fullyVisibleCenter(lv_obj_t *object, int16_t &x, int16_t &y)
{
    if (!object || lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) {
        return false;
    }

    lv_area_t coords{};
    lv_obj_get_coords(object, &coords);
    if (coords.x1 < 0 || coords.y1 < 0 || coords.x2 >= 320 || coords.y2 >= 240) {
        return false;
    }
    x = static_cast<int16_t>((coords.x1 + coords.x2) / 2);
    y = static_cast<int16_t>((coords.y1 + coords.y2) / 2);
    return true;
}

bool objectAndAncestorsVisible(lv_obj_t *object)
{
    for (auto *current = object; current; current = lv_obj_get_parent(current)) {
        if (lv_obj_has_flag(current, LV_OBJ_FLAG_HIDDEN)) {
            return false;
        }
    }
    return true;
}

bool acceptNodeButton(lv_obj_t *button, uint32_t selectedNode, uintptr_t focusedObject, int16_t &x, int16_t &y, uintptr_t &target)
{
    if (!button || !objectAndAncestorsVisible(button) || !lv_obj_check_type(button, &lv_button_class) ||
        reinterpret_cast<uintptr_t>(button) == focusedObject || !fullyVisibleCenter(button, x, y)) {
        return false;
    }

    const auto data = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(button)));
    if (data != 0 && data == selectedNode) {
        return false;
    }

    target = reinterpret_cast<uintptr_t>(button);
    return true;
}

bool findNodeListButton(lv_obj_t *object, uint32_t selectedNode, uintptr_t focusedObject, int16_t &x, int16_t &y,
                        uintptr_t &target)
{
    if (!object || !objectAndAncestorsVisible(object)) {
        return false;
    }

    if (acceptNodeButton(object, selectedNode, focusedObject, x, y, target)) {
        return true;
    }

    const uint32_t childCount = lv_obj_get_child_count(object);
    if (childCount > 1) {
        auto *rowButton = lv_obj_get_child(object, 1);
        if (acceptNodeButton(rowButton, selectedNode, focusedObject, x, y, target)) {
            return true;
        }
    }

    for (uint32_t index = 0; index < childCount; ++index) {
        if (findNodeListButton(lv_obj_get_child(object, index), selectedNode, focusedObject, x, y, target)) {
            return true;
        }
    }
    return false;
}
} // namespace

X11MuiSimulator::X11MuiSimulator()
    : config(DisplayDriverConfig::device_t::X11, 320, 240), driver(nullptr), pointerInput(nullptr), keyboardInput(nullptr),
      encoderInput(nullptr), fixturePointerInput(nullptr), originalPointerRead(nullptr), originalKeyboardRead(nullptr),
      originalEncoderRead(nullptr)
{
}

bool X11MuiSimulator::initialize(Implementation implementation)
{
    if (ready()) {
        return true;
    }

    driver = &X11Driver::create(320, 240);
    harness = std::make_unique<MuiTestHarness>(config, driver);
    if (implementation == Implementation::VirtualCandidate) {
        harness->enableVirtualNodeListFixture();
#ifdef DEVICE_UI_MUI_VIRTUAL_NODE_LIST
    } else {
        harness->enableVirtualNodeModelFixture();
#endif
    }
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

bool X11MuiSimulator::runNodeListHardwareBenchmark(uint32_t timeoutMs, bool tdeckConstrained, std::string &report)
{
#if defined(DEVICE_UI_MUI_NODE_LIST_HW_BENCH)
    if (!ready() || !ensurePointerInputForTesting()) {
        return false;
    }

    auto *view = harness->viewForTesting();
    if (!view) {
        return false;
    }

    view->startNodeListHardwareBenchmark();
    constexpr uint32_t defaultStepMs = 16;
    constexpr uint32_t tdeckUiPeriodMs = 40;
    constexpr uint32_t tdeckFrameTransferSleepMs = 31;
    const uint32_t stepMs = tdeckConstrained ? tdeckUiPeriodMs : defaultStepMs;
    for (uint32_t elapsed = 0; elapsed < timeoutMs && !view->nodeListHardwareBenchmarkComplete(); elapsed += stepMs) {
        lv_tick_inc(stepMs);
        if (tdeckConstrained) {
            view->advanceNodeListHardwareBenchmark();
        } else {
            view->task_handler();
        }
        if (tdeckConstrained) {
            lv_timer_handler();
            if (driver && driver->getDisplay()) {
                lv_refr_now(driver->getDisplay());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(tdeckFrameTransferSleepMs));
        } else {
            for (uint8_t refresh = 0; refresh < 4; ++refresh) {
                lv_tick_inc(4);
                lv_timer_handler();
                if (driver && driver->getDisplay()) {
                    lv_refr_now(driver->getDisplay());
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    report = view->nodeListHardwareBenchmarkReport();
    return view->nodeListHardwareBenchmarkComplete() && report.rfind("MUI_NODE_LIST_HW_BENCH {", 0) == 0 &&
           report.find("\"e\":\"none\"") != std::string::npos;
#else
    (void)timeoutMs;
    report.clear();
    return false;
#endif
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

bool X11MuiSimulator::disableNodeListScrollMomentumForTesting()
{
    auto *root = harness ? harness->nodeListRootForTesting() : nullptr;
    if (!root) {
        return false;
    }
    const bool wasEnabled = lv_obj_has_flag(root, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    return wasEnabled;
}

void X11MuiSimulator::restoreNodeListScrollMomentumForTesting(bool enabled)
{
    auto *root = harness ? harness->nodeListRootForTesting() : nullptr;
    if (root && enabled) {
        lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    }
}

void X11MuiSimulator::stopNodeListScrollForTesting()
{
    auto *root = harness ? harness->nodeListRootForTesting() : nullptr;
    if (root) {
        lv_obj_stop_scroll_anim(root);
    }
}

bool X11MuiSimulator::virtualNodeListEnabledForTesting() const
{
    return harness && harness->virtualNodeListEnabled();
}

uint32_t X11MuiSimulator::selectedNodeForTesting() const
{
    return harness ? harness->selectedNode() : 0;
}

uintptr_t X11MuiSimulator::focusedObjectForTesting() const
{
    lv_group_t *group = lv_group_get_default();
    return group ? reinterpret_cast<uintptr_t>(lv_group_get_focused(group)) : 0;
}

bool X11MuiSimulator::focusedObjectCenterForTesting(int16_t &x, int16_t &y) const
{
    lv_group_t *group = lv_group_get_default();
    lv_obj_t *focused = group ? lv_group_get_focused(group) : nullptr;
    if (!focused) {
        return false;
    }
    lv_area_t coords{};
    lv_obj_get_coords(focused, &coords);
    x = static_cast<int16_t>((coords.x1 + coords.x2) / 2);
    y = static_cast<int16_t>((coords.y1 + coords.y2) / 2);
    return true;
}

bool X11MuiSimulator::nodeListClickTargetCenterForTesting(int16_t &x, int16_t &y) const
{
    uintptr_t target = 0;
    return nodeListClickTargetForTesting(x, y, target);
}

bool X11MuiSimulator::nodeListClickTargetForTesting(int16_t &x, int16_t &y, uintptr_t &target) const
{
    auto *root = harness ? harness->nodeListRootForTesting() : nullptr;
    return findNodeListButton(root, selectedNodeForTesting(), focusedObjectForTesting(), x, y, target);
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

void X11MuiSimulator::fallbackPointerReadCallback(lv_indev_t *, lv_indev_data_t *data)
{
    data->point.x = 0;
    data->point.y = 0;
    data->state = LV_INDEV_STATE_RELEASED;
}

bool X11MuiSimulator::ensurePointerInputForTesting()
{
    for (lv_indev_t *input = lv_indev_get_next(nullptr); input; input = lv_indev_get_next(input)) {
        if (lv_indev_get_type(input) == LV_INDEV_TYPE_POINTER) {
            return true;
        }
    }

    if (!fixturePointerInput) {
        fixturePointerInput = lv_indev_create();
        lv_indev_set_type(fixturePointerInput, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(fixturePointerInput, fallbackPointerReadCallback);
    }
    return fixturePointerInput != nullptr;
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
