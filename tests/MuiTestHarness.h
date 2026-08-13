#pragma once

#include "HeadlessDisplayDriver.h"
#include <cstddef>
#include <cstdint>

class TFTView_320x240;

class MuiTestHarness
{
  public:
    MuiTestHarness();

    bool ready();
    void addNodeFixture(uint32_t nodeId, const char *shortName = "TEST", const char *longName = "Test Node");
    void updatePositionFixture(uint32_t nodeId, int32_t latitude, int32_t longitude, int32_t altitude = 0,
                               uint32_t satellites = 0, uint32_t precision = 0);
    void pump(uint32_t elapsedMs = 10);
    size_t objectCount() const;
    size_t nodeListObjectCount() const;

  private:
    static size_t countObjects(const lv_obj_t *root);

    HeadlessDisplayDriver *driver;
    TFTView_320x240 *view;
};
