#if defined(DEVICE_UI_HEADLESS_TEST) || defined(DEVICE_UI_X11_SIMULATOR)

#include "MuiTestHarness.h"
#include "HeadlessDisplayDriver.h"
#include "graphics/common/MeshtasticView.h"
#include "graphics/common/ViewController.h"
#include "graphics/common/ViewFactory.h"
#include "graphics/driver/DisplayDriverConfig.h"
#include "graphics/view/TFT/TFTView_320x240.h"
#include "meshtastic/mesh.pb.h"
#include "meshtastic/telemetry.pb.h"
#include <cstring>
#include <random>
#include <sstream>

#ifdef DEVICE_UI_HEADLESS_TEST
const char *firmware_version = "headless-test";

namespace
{
HeadlessDisplayDriver &headlessDisplayDriver()
{
    static HeadlessDisplayDriver driver;
    return driver;
}
} // namespace

MuiTestHarness::MuiTestHarness()
    : MuiTestHarness(DisplayDriverConfig(DisplayDriverConfig::device_t::NONE, 320, 240), &headlessDisplayDriver())
{
}
#endif

class MuiRecordingViewController : public ViewController
{
  public:
    bool requestPosition(uint32_t to, uint8_t ch, uint32_t requestId) override
    {
        lastPosition = {to, ch, 0, requestId, false, ""};
        return true;
    }

    void traceRoute(uint32_t to, uint8_t ch, uint8_t hopLimit, uint32_t requestId) override
    {
        lastTrace = {to, ch, hopLimit, requestId, false, ""};
    }

    void sendTextMessage(uint32_t to, uint8_t ch, uint8_t hopLimit, uint32_t msgTime, uint32_t requestId, bool usePkc,
                         const char *textmsg) override
    {
        lastText = {to, ch, hopLimit, requestId, usePkc, textmsg ? textmsg : ""};
    }

    MuiControllerCall lastPosition;
    MuiControllerCall lastText;
    MuiControllerCall lastTrace;
};

MuiTestHarness::MuiTestHarness(const DisplayDriverConfig &config, DisplayDriver *displayDriver)
    : driver(nullptr), displayDriver(displayDriver), view(nullptr),
      recordingController(std::make_unique<MuiRecordingViewController>())
{
#ifdef DEVICE_UI_HEADLESS_TEST
    driver = static_cast<HeadlessDisplayDriver *>(displayDriver);
#endif
    view = static_cast<TFTView_320x240 *>(ViewFactory::createForTesting(config, displayDriver));

    if (!displayDriver->getDisplay()) {
        view->init(nullptr);

        meshtastic_DeviceUIConfig uiConfig{};
        uiConfig.version = 1;
        view->setupUIConfig(uiConfig);
        pump();
    }

    view->setControllerForTesting(recordingController.get());
}

MuiTestHarness::~MuiTestHarness() = default;

bool MuiTestHarness::ready()
{
    return view && displayDriver && displayDriver->getDisplay() && lv_screen_active() && view->nodeListRootForTesting();
}

void MuiTestHarness::resetNodeList()
{
    view->resetNodeListForTesting();
    pump(500);
}

void MuiTestHarness::setCurrentTime(uint32_t value)
{
    view->setCurrentTimeForTesting(value);
}

void MuiTestHarness::addNodeFixture(uint32_t nodeId, const char *shortName, const char *longName, uint32_t lastHeard,
                                    uint8_t role, bool hasKey, bool unmessagable, uint8_t channel)
{
    view->addNode(nodeId, channel, shortName, longName, lastHeard, static_cast<MeshtasticView::eRole>(role), hasKey,
                  unmessagable);
}

void MuiTestHarness::addUnknownNodeFixture(uint32_t nodeId, uint8_t channel, uint32_t lastHeard, uint8_t role, bool hasKey,
                                           bool viaMqtt)
{
    static_cast<MeshtasticView *>(view)->addOrUpdateNode(nodeId, channel, lastHeard, static_cast<MeshtasticView::eRole>(role),
                                                         hasKey, viaMqtt);
}

void MuiTestHarness::updateNodeFixture(uint32_t nodeId, const char *shortName, const char *longName, uint8_t role, bool hasKey,
                                       bool unmessagable, uint8_t channel)
{
    meshtastic_User user = meshtastic_User_init_default;
    std::strncpy(user.short_name, shortName, sizeof(user.short_name) - 1);
    std::strncpy(user.long_name, longName, sizeof(user.long_name) - 1);
    user.role = static_cast<meshtastic_Config_DeviceConfig_Role>(role);
    user.public_key.size = hasKey ? 1 : 0;
    user.has_is_unmessagable = unmessagable;
    user.is_unmessagable = unmessagable;
    view->updateNode(nodeId, channel, user);
}

void MuiTestHarness::updatePositionFixture(uint32_t nodeId, int32_t latitude, int32_t longitude, int32_t altitude,
                                           uint32_t satellites, uint32_t precision)
{
    view->updatePosition(nodeId, latitude, longitude, altitude, satellites, precision);
}

void MuiTestHarness::updateTelemetryFixture(uint32_t nodeId, float temperature, float humidity, float pressure, uint16_t iaq)
{
    meshtastic_EnvironmentMetrics metrics = meshtastic_EnvironmentMetrics_init_default;
    metrics.has_temperature = true;
    metrics.temperature = temperature;
    metrics.has_relative_humidity = true;
    metrics.relative_humidity = humidity;
    metrics.has_barometric_pressure = true;
    metrics.barometric_pressure = pressure;
    metrics.has_iaq = true;
    metrics.iaq = iaq;
    view->updateEnvironmentMetrics(nodeId, metrics);
}

void MuiTestHarness::updateMetricsFixture(uint32_t nodeId, uint32_t batteryLevel, float voltage, float channelUtilization,
                                          float airUtilization)
{
    view->updateMetrics(nodeId, batteryLevel, voltage, channelUtilization, airUtilization);
}

void MuiTestHarness::updateHopsFixture(uint32_t nodeId, uint8_t hops)
{
    view->updateHopsAway(nodeId, hops);
}

void MuiTestHarness::updateLastHeardFixture(uint32_t nodeId)
{
    view->updateLastHeardForTesting(nodeId);
}

void MuiTestHarness::addActiveChatFixture(uint32_t nodeId, uint8_t channel)
{
    uint32_t messageTime = 1700000000;
    view->newMessage(nodeId, 0, channel, "benchmark", messageTime, true);
}

void MuiTestHarness::toggleResyncPresentationFixture()
{
    view->notifyResync(true);
    view->notifyResync(false);
}

std::vector<MuiNodeFixture> MuiTestHarness::makeLegacyNodeFixtures(size_t count, uint32_t seed, size_t iteration) const
{
    constexpr uint32_t now = 1700000000U;
    std::mt19937 random(seed + static_cast<uint32_t>(iteration * 0x9e3779b9U));
    std::vector<MuiNodeFixture> fixtures;
    fixtures.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        std::ostringstream shortName;
        shortName << 'N' << std::hex << ((i + random()) & 0xfffU);
        std::string shortValue = shortName.str().substr(0, 4);
        while (shortValue.size() < 4) {
            shortValue.push_back('0');
        }

        std::string longName;
        switch (i % 6) {
        case 0:
            longName.clear();
            break;
        case 1:
            longName = "Typical Node " + std::to_string(i);
            break;
        case 2:
            longName = "Málaga UTF-8 " + std::to_string(i);
            break;
        case 3:
            longName = "Satellite 🛰 " + std::to_string(i);
            break;
        case 4:
            longName = "123456789012345678901234567890123456789";
            break;
        default:
            longName = "Node " + std::to_string(i);
            break;
        }

        const bool offline = i % 4 == 0;
        fixtures.push_back({
            static_cast<uint32_t>(0xa0000000U + (iteration << 12U) + i),
            shortValue,
            longName,
            offline ? now - 7200U - static_cast<uint32_t>(i) : now - static_cast<uint32_t>(i),
            static_cast<uint8_t>(i % 5 == 0 ? MeshtasticView::unknown : i % 7),
            i % 2 == 0,
            i % 11 == 0,
            static_cast<uint8_t>(i % 8),
            i % 3 == 0,
            i % 4 == 1,
        });
    }
    return fixtures;
}

void MuiTestHarness::populateLegacyNodeFixtures(size_t count, uint32_t seed, size_t iteration)
{
    resetNodeList();
    setCurrentTime(1700000000U);

    const auto fixtures = makeLegacyNodeFixtures(count, seed, iteration);
    for (const auto &fixture : fixtures) {
        addNodeFixture(fixture.id, fixture.shortName.c_str(), fixture.longName.c_str(), fixture.lastHeard, fixture.role,
                       fixture.hasKey, fixture.unmessagable, fixture.channel);
    }
    showNodesScreen();
    pump(50);
}

void MuiTestHarness::scanNodeFilters()
{
    view->updateNodesFilteredForTesting(true);
    const size_t chunks = (view->nodeCountForTesting() + 9) / 10;
    for (size_t i = 1; i < chunks; ++i) {
        view->updateNodesFilteredForTesting(false);
    }
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

size_t MuiTestHarness::renderedNodeCount() const
{
    return view->nodeCountForTesting();
}

const char *MuiTestHarness::nodeLongName(uint32_t nodeId) const
{
    return view->nodeLongNameForTesting(nodeId);
}

uint8_t MuiTestHarness::nodeRole(uint32_t nodeId) const
{
    return view->nodeRoleForTesting(nodeId);
}

const NodeRecord *MuiTestHarness::node(uint32_t nodeId) const
{
    return view->nodeRecordForTesting(nodeId);
}

bool MuiTestHarness::nodeIsMessagable(uint32_t nodeId) const
{
    return view->nodeIsMessagableForTesting(nodeId);
}

uint8_t MuiTestHarness::nodeChannel(uint32_t nodeId) const
{
    return view->nodeChannelForTesting(nodeId);
}

bool MuiTestHarness::nodeHasKey(uint32_t nodeId) const
{
    return view->nodeHasKeyForTesting(nodeId);
}

int8_t MuiTestHarness::nodeHops(uint32_t nodeId) const
{
    return view->nodeHopsForTesting(nodeId);
}

const char *MuiTestHarness::nodeDisplayName(uint32_t nodeId) const
{
    return view->nodeDisplayNameForTesting(nodeId);
}

const char *MuiTestHarness::nodeShortName(uint32_t nodeId) const
{
    return view->nodeShortNameForTesting(nodeId);
}

NodePosition MuiTestHarness::nodePosition(uint32_t nodeId) const
{
    return view->nodePositionForTesting(nodeId);
}

uint32_t MuiTestHarness::nodePurgeCandidate(uint32_t incoming) const
{
    return view->nodePurgeCandidateForTesting(incoming);
}

void MuiTestHarness::corruptLegacyNodePanel(uint32_t nodeId)
{
    view->corruptLegacyNodePanelForTesting(nodeId);
}

void MuiTestHarness::removeLegacyNodePanel(uint32_t nodeId)
{
    view->removeLegacyNodePanelForTesting(nodeId);
}

void MuiTestHarness::setLoRaHopLimit(uint8_t hopLimit)
{
    view->setLoRaHopLimitForTesting(hopLimit);
}

void MuiTestHarness::selectNode(uint32_t nodeId)
{
    view->selectNodeForTesting(nodeId);
}

void MuiTestHarness::scanSignal(uint32_t scanNo)
{
    view->scanSignalForTesting(scanNo);
}

void MuiTestHarness::showTraceRoute()
{
    view->showTraceRouteForTesting();
}

void MuiTestHarness::startTraceRoute()
{
    view->startTraceRouteForTesting();
}

void MuiTestHarness::dispatchTraceRouteNodeCallback(uint32_t nodeId)
{
    view->dispatchTraceRouteNodeCallbackForTesting(nodeId);
}

bool MuiTestHarness::nodesPanelVisible() const
{
    return view->nodesPanelVisibleForTesting();
}

bool MuiTestHarness::traceRoutePanelVisible() const
{
    return view->traceRoutePanelVisibleForTesting();
}

void MuiTestHarness::sendDirectText(uint32_t nodeId, const char *msg)
{
    char buf[240]{};
    std::strncpy(buf, msg, sizeof(buf) - 1);
    view->sendDirectTextForTesting(nodeId, buf);
}

uint8_t MuiTestHarness::nodeHopLimit(uint32_t nodeId, int8_t unknownHops) const
{
    return view->nodeHopLimitForTesting(nodeId, unknownHops);
}

uintptr_t MuiTestHarness::traceRouteNodeCallbackPayload(uint32_t nodeId) const
{
    return view->traceRouteNodeCallbackPayloadForTesting(nodeId);
}

MuiControllerCall MuiTestHarness::lastPositionRequest() const
{
    return recordingController->lastPosition;
}

MuiControllerCall MuiTestHarness::lastTextMessage() const
{
    return recordingController->lastText;
}

MuiControllerCall MuiTestHarness::lastTraceRoute() const
{
    return recordingController->lastTrace;
}

const NodeStore &MuiTestHarness::store() const
{
    return view->nodeStoreForTesting();
}

const VisibleNodeIndex &MuiTestHarness::visibleIndex() const
{
    return view->visibleNodesForTesting();
}

lv_obj_t *MuiTestHarness::nodeListRootForTesting() const
{
    return view->nodeListRootForTesting();
}

void MuiTestHarness::showNodesScreen()
{
    view->showNodesScreenForTesting();
}

HeadlessDisplayDriver *MuiTestHarness::displayDriverForTesting() const
{
    return driver;
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
