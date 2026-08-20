#pragma once

#include "graphics/driver/DisplayDriverConfig.h"
#include <cstddef>
#include <cstdint>

class TFTView_320x240;
class X11Driver;
struct _lv_indev_t;
typedef struct _lv_indev_t lv_indev_t;

class X11MuiSimulator
{
  public:
    X11MuiSimulator();

    bool initialize();
    bool ready() const;
    void populateLegacyNodeFixtures(size_t count = 100, uint32_t seed = 42);
    void pumpUntilClosed();
    void pump(uint32_t elapsedMs = 10);

    lv_indev_t *pointerInputForTesting() const { return pointerInput; }
    lv_indev_t *keyboardInputForTesting() const { return keyboardInput; }
    lv_indev_t *encoderInputForTesting() const { return encoderInput; }

  private:
    void scanInputs();

    DisplayDriverConfig config;
    X11Driver *driver;
    TFTView_320x240 *view;
    lv_indev_t *pointerInput;
    lv_indev_t *keyboardInput;
    lv_indev_t *encoderInput;
};
