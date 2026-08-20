#pragma once

#include "MuiTestHarness.h"
#include "graphics/driver/DisplayDriverConfig.h"
#include "lvgl.h"
#include <cstddef>
#include <cstdint>
#include <memory>

struct NodeRecord;
class X11Driver;

class X11MuiSimulator
{
  public:
    enum class Implementation {
        Legacy,
        VirtualCandidate,
    };

    X11MuiSimulator();

    bool initialize(Implementation implementation = Implementation::Legacy);
    bool ready() const;
    void populateLegacyNodeFixtures(size_t count = 100, uint32_t seed = 42);
    void pumpUntilClosed();
    void pump(uint32_t elapsedMs = 10);
    bool injectPointer(int16_t x, int16_t y, bool pressed);
    bool injectKeyboard(uint32_t key);
    bool injectEncoder(int16_t diff);
    size_t renderedNodeCountForTesting() const;
    const NodeRecord *nodeForTesting(uint32_t nodeId) const;
    int32_t nodeListScrollYForTesting() const;
    bool virtualNodeListEnabledForTesting() const;
    uint32_t selectedNodeForTesting() const;

    lv_indev_t *pointerInputForTesting() const { return pointerInput; }
    lv_indev_t *keyboardInputForTesting() const { return keyboardInput; }
    lv_indev_t *encoderInputForTesting() const { return encoderInput; }

  private:
    struct PointerState {
        lv_point_t point{};
        bool pressed = false;
        bool pending = false;
    };

    struct KeyState {
        uint32_t key = 0;
        bool pending = false;
    };

    struct EncoderState {
        int16_t diff = 0;
        bool pending = false;
    };

    static void pointerReadCallback(lv_indev_t *indev, lv_indev_data_t *data);
    static void keyboardReadCallback(lv_indev_t *indev, lv_indev_data_t *data);
    static void encoderReadCallback(lv_indev_t *indev, lv_indev_data_t *data);

    void scanInputs();

    DisplayDriverConfig config;
    X11Driver *driver;
    std::unique_ptr<MuiTestHarness> harness;
    lv_indev_t *pointerInput;
    lv_indev_t *keyboardInput;
    lv_indev_t *encoderInput;
    lv_indev_read_cb_t originalPointerRead;
    lv_indev_read_cb_t originalKeyboardRead;
    lv_indev_read_cb_t originalEncoderRead;
    PointerState pointerState;
    KeyState keyState;
    EncoderState encoderState;
};
