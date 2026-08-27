#pragma once

#include <array>

#include "graphics/common/NodeStore.h"
#include "graphics/common/VisibleNodeIndex.h"
#include "graphics/driver/DisplayDriverConfig.h"
#include "lvgl.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class DisplayDriver;
class HeadlessDisplayDriver;
class MuiRecordingViewController;
class TFTView_320x240;

struct MuiNodeFixture {
    uint32_t id;
    std::string shortName;
    std::string longName;
    uint32_t lastHeard;
    uint8_t role;
    bool hasKey;
    bool unmessagable;
    uint8_t channel;
    bool hasPosition;
    bool hasTelemetry;
};

struct MuiControllerCall {
    uint32_t to = 0;
    uint8_t channel = 0;
    uint8_t hopLimit = 0;
    uint32_t requestId = 0;
    bool usePkc = false;
    std::string text;
};

struct MuiRowSnapshot {
    std::string longName;
    std::string shortName;
    std::string battery;
    std::string lastHeard;
    std::string signal;
    std::string position1;
    std::string position2;
    std::string telemetry1;
    std::string telemetry2;
    int32_t shortNameY = 0;
    uint32_t imageBg = 0;
    uint32_t imageBorder = 0;
    uint32_t imageRecolor = 0;
    uint8_t imageRecolorOpa = 0;
    uintptr_t imageSrc = 0;
    uint32_t rowBg = 0;
    uint32_t rowBorder = 0;
    int32_t imageX = 0;
    int32_t imageY = 0;
    int32_t longNameX = 0;
    int32_t longNameY = 0;
    int32_t shortNameX = 0;
    int32_t signalX = 0;
    int32_t signalY = 0;
    int32_t rowWidth = 0;
    int32_t signalWidth = 0;
    int32_t signalLongMode = 0;
    bool position1Hidden = false;
    bool position2Hidden = false;
    bool telemetry1Hidden = false;
    bool telemetry2Hidden = false;
    uintptr_t shortNameFont = 0;
};

// Capture a MuiRowSnapshot from a rendered 11-child node row panel.
MuiRowSnapshot snapshotMuiRow(lv_obj_t *row);

class MuiTestHarness
{
  public:
    MuiTestHarness();
    MuiTestHarness(const DisplayDriverConfig &config, DisplayDriver *displayDriver);
    ~MuiTestHarness();

    bool ready();
    void resetNodeList();
    void setCurrentTime(uint32_t value);
    void setOwnNodeFixture(uint32_t nodeId);
    void addNodeFixture(uint32_t nodeId, const char *shortName = "TEST", const char *longName = "Test Node",
                        uint32_t lastHeard = 0, uint8_t role = 0, bool hasKey = true, bool unmessagable = false,
                        uint8_t channel = 0);
    void addUnknownNodeFixture(uint32_t nodeId, uint8_t channel, uint32_t lastHeard, uint8_t role, bool hasKey, bool viaMqtt);
    void addOrUpdateNodeFixture(uint32_t nodeId, const char *shortName, const char *longName, uint32_t lastHeard, uint8_t role,
                                bool hasKey, uint8_t channel);
    void updateNodeFixture(uint32_t nodeId, const char *shortName, const char *longName, uint8_t role, bool hasKey,
                           bool unmessagable = false, uint8_t channel = 0);
    void updatePositionFixture(uint32_t nodeId, int32_t latitude, int32_t longitude, int32_t altitude = 0,
                               uint32_t satellites = 0, uint32_t precision = 0);
    void updateTelemetryFixture(uint32_t nodeId, float temperature, float humidity, float pressure, uint16_t iaq);
    void updateMetricsFixture(uint32_t nodeId, uint32_t batteryLevel, float voltage, float channelUtilization,
                              float airUtilization);
    void updateSignalFixture(uint32_t nodeId, int32_t rssi, float snr);
    void updateHopsFixture(uint32_t nodeId, uint8_t hops);
    void updateLastHeardFixture(uint32_t nodeId);
    void addActiveChatFixture(uint32_t nodeId, uint8_t channel = 0);
    void setActiveChatModelFixture(uint32_t nodeId, bool active = true);
    void toggleResyncPresentationFixture();
    void configureInputDevicesFixture(bool keyboard, bool encoder, bool pointer);
    void setOfflineFilterFixture(bool enabled);
    void setPositionFilterFixture(bool enabled);
    void addUntilPurgeFixture(size_t count);
    std::vector<MuiNodeFixture> makeNodeFixtures(size_t count = 100, uint32_t seed = 42, size_t iteration = 0) const;
    void populateNodeFixtures(size_t count = 100, uint32_t seed = 42, size_t iteration = 0);
    void scanNodeFilters();
    void pump(uint32_t elapsedMs = 10);
    size_t objectCount() const;
    size_t nodeListObjectCount() const;
    size_t renderedNodeCount() const;
    uint16_t nodesOnlineCount() const;
    uint32_t virtualNodeListBindGeneration() const;
    const char *nodeLongName(uint32_t nodeId) const;
    uint8_t nodeRole(uint32_t nodeId) const;
    const NodeRecord *node(uint32_t nodeId) const;
    bool nodeIsMessagable(uint32_t nodeId) const;
    uint8_t nodeChannel(uint32_t nodeId) const;
    bool nodeHasKey(uint32_t nodeId) const;
    int8_t nodeHops(uint32_t nodeId) const;
    const char *nodeDisplayName(uint32_t nodeId) const;
    const char *nodeShortName(uint32_t nodeId) const;
    std::array<char, 4> nodeShortNameCache(uint32_t nodeId) const;
    NodePosition nodePosition(uint32_t nodeId) const;
    uint32_t nodePurgeCandidate(uint32_t incoming) const;
    void setLoRaHopLimit(uint8_t hopLimit);
    void selectNode(uint32_t nodeId);
    void scanSignal(uint32_t scanNo = 0);
    void showTraceRoute();
    void showSignalScanner();
    void startTraceRoute();
    void dispatchTraceRouteResult(uint32_t nodeId);
    void dispatchTraceRouteNodeCallback(uint32_t nodeId);
    void dispatchMapNodeCallback(uint32_t nodeId);
    void dispatchChatNodeCallback(uint32_t nodeId);
    void dispatchBadKeyRoutingError(uint32_t nodeId);
    bool nodesPanelVisible() const;
    bool traceRoutePanelVisible() const;
    void sendDirectText(uint32_t nodeId, const char *msg);
    uint8_t nodeHopLimit(uint32_t nodeId, int8_t unknownHops) const;
    uintptr_t traceRouteNodeCallbackPayload(uint32_t nodeId) const;
    MuiControllerCall lastPositionRequest() const;
    MuiControllerCall lastTextMessage() const;
    MuiControllerCall lastTraceRoute() const;
    uint32_t selectedNode() const;
    bool messagesPanelVisible() const;
    bool mapPanelVisible() const;
    void showMapScreen();
    bool mapMarkerFiltered(uint32_t nodeId) const;
    void resetMapFilterCounters();
    uint32_t mapFilterUpdateCount() const;
    uint32_t visibleNodeContainsCallCount() const;
    uintptr_t topMessagesNodeImageSrc() const;
    const char *homeBatteryPercentageText() const;
    uintptr_t homeBatteryImageSrc() const;
    void setNodeNameFilter(const char *text);
    void setNodeHighlightName(const char *text);
    const char *chatButtonLabel() const;
    const char *settingsUserLabelText() const;
    void runLastHeardTickFixture();
    const char *firstTraceRouteTowardsLabel() const;
    uintptr_t firstTraceRouteTowardsImageSrc() const;
    void clickFirstTraceRouteTowardsButton();
    const char *signalScannerTargetLabel() const;
    uintptr_t signalScannerTargetImageSrc() const;
    const char *traceRouteTargetLabel() const;
    uintptr_t traceRouteTargetImageSrc() const;
    void dispatchVirtualNodeEvent(uint32_t nodeId, lv_event_code_t eventCode);
    void dispatchVirtualNodePositionEvent(uint32_t nodeId);
    bool focusRenderedVirtualNode(uint32_t nodeId);
    void scrollVirtualNodeIntoView(uint32_t nodeId);
    void focusNextInGroup();
    void focusPreviousInGroup();
    void focusNextInVirtualGroup();
    void focusPreviousInVirtualGroup();
    void dispatchKeyboardKey(uint32_t key, bool longPress = false);
    void dispatchEncoderDelta(int16_t diff);
    void dispatchEncoderKey(uint32_t key, bool longPress = false);
    lv_group_t *virtualNavigationGroup() const;
    lv_group_t *keyboardInputGroup() const;
    lv_group_t *encoderInputGroup() const;
    lv_group_t *pointerInputGroup() const;
    void injectPacketFrom(uint32_t from, uint32_t to = 0xffffffff, uint8_t channel = 0);
    void sendActiveText(const char *msg);
    const NodeStore &store() const;
    const VisibleNodeIndex &visibleIndex() const;
    lv_obj_t *nodeListRootForTesting() const;
    TFTView_320x240 *viewForTesting() const { return view; }
    void showNodesScreen();
    HeadlessDisplayDriver *displayDriverForTesting() const;

  private:
    static size_t countObjects(const lv_obj_t *root);

    HeadlessDisplayDriver *driver;
    DisplayDriver *displayDriver;
    TFTView_320x240 *view;
    std::unique_ptr<MuiRecordingViewController> recordingController;
};
