#ifdef DEVICE_UI_HEADLESS_TEST

#include "MuiTestHarness.h"
#include "graphics/common/ViewFactory.h"
#include "graphics/driver/DisplayDriverConfig.h"
#include "graphics/view/TFT/TFTView_320x240.h"
#include "meshtastic/mesh.pb.h"
#include "meshtastic/telemetry.pb.h"
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

const NodeStore &MuiTestHarness::store() const
{
    return view->nodeStoreForTesting();
}

const VisibleNodeIndex &MuiTestHarness::visibleIndex() const
{
    return view->visibleNodesForTesting();
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
