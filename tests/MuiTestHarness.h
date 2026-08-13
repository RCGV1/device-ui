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
    void resetNodeList();
    void setCurrentTime(uint32_t value);
    void addNodeFixture(uint32_t nodeId, const char *shortName = "TEST", const char *longName = "Test Node",
                        uint32_t lastHeard = 0, uint8_t role = 0, bool hasKey = true, bool unmessagable = false,
                        uint8_t channel = 0);
    void updateNodeFixture(uint32_t nodeId, const char *shortName, const char *longName, uint8_t role, bool hasKey,
                           bool unmessagable = false, uint8_t channel = 0);
    void updatePositionFixture(uint32_t nodeId, int32_t latitude, int32_t longitude, int32_t altitude = 0,
                               uint32_t satellites = 0, uint32_t precision = 0);
    void updateTelemetryFixture(uint32_t nodeId, float temperature, float humidity, float pressure, uint16_t iaq);
    void updateMetricsFixture(uint32_t nodeId, uint32_t batteryLevel, float voltage, float channelUtilization,
                              float airUtilization);
    void updateHopsFixture(uint32_t nodeId, uint8_t hops);
    void addActiveChatFixture(uint32_t nodeId, uint8_t channel = 0);
    void toggleResyncPresentationFixture();
    void scanNodeFilters();
    void pump(uint32_t elapsedMs = 10);
    size_t objectCount() const;
    size_t nodeListObjectCount() const;
    size_t renderedNodeCount() const;
    const char *nodeLongName(uint32_t nodeId) const;
    uint8_t nodeRole(uint32_t nodeId) const;

  private:
    static size_t countObjects(const lv_obj_t *root);

    HeadlessDisplayDriver *driver;
    TFTView_320x240 *view;
};
