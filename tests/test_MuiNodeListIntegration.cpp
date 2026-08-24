#include "MuiTestHarness.h"
#include "graphics/common/MeshtasticView.h"
#include "graphics/view/TFT/TFTView_320x240.h"
#include "graphics/view/TFT/VirtualNodeList.h"
#include "images.h"
#include <array>
#include <cstring>
#include <doctest/doctest.h>
#include <string>

#ifdef DEVICE_UI_HEADLESS_TEST
namespace
{
int defaultGroupFocusCallbacks = 0;
int defaultGroupEdgeCallbacks = 0;

void defaultGroupFocusCallback(lv_group_t *)
{
    defaultGroupFocusCallbacks++;
}

void defaultGroupEdgeCallback(lv_group_t *, bool)
{
    defaultGroupEdgeCallbacks++;
}
} // namespace

TEST_CASE("320x240 view initializes on a deterministic headless display")
{
    MuiTestHarness harness;

    CHECK(harness.ready());
    CHECK(harness.objectCount() > 0);
    CHECK(harness.nodeListObjectCount() > 0);
}

TEST_CASE("deterministic headless display remains valid across harness instances")
{
    {
        MuiTestHarness firstHarness;
        CHECK(firstHarness.ready());
    }

    MuiTestHarness secondHarness;
    CHECK(secondHarness.ready());
    CHECK(secondHarness.objectCount() > 0);
}

TEST_CASE("test harness can activate the populated nodes screen")
{
    MuiTestHarness harness;
    REQUIRE(harness.ready());
    REQUIRE(lv_obj_has_flag(harness.nodeListRootForTesting(), LV_OBJ_FLAG_HIDDEN));

    harness.showNodesScreen();

    CHECK_FALSE(lv_obj_has_flag(harness.nodeListRootForTesting(), LV_OBJ_FLAG_HIDDEN));
}

TEST_CASE("view updates model fields before rendering a current row")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeModelFixture();

    harness.addNodeFixture(0x12345678, "ALPH", "Alpha Node", 1000, 0, true, false, 1);
    const auto *node = harness.node(0x12345678);
    REQUIRE(node != nullptr);
    CHECK(node->id == 0x12345678);
    CHECK(node->channel == 1);
    CHECK(node->lastHeard == 1000);
    CHECK(std::string(node->user.short_name) == "ALPH");
    CHECK(std::string(node->user.long_name) == "Alpha Node");
    CHECK(node->hasKey == true);

    // Position update
    harness.updatePositionFixture(0x12345678, 377749000, -1224194000, 50, 9, 2);
    node = harness.node(0x12345678);
    REQUIRE(node != nullptr);
    CHECK(node->position.known == true);
    CHECK(node->position.latitude == 377749000);
    CHECK(node->position.longitude == -1224194000);

    // Metrics & Telemetry update
    harness.updateMetricsFixture(0x12345678, 85, 4.12f, 12.5f, 3.2f);
    harness.updateTelemetryFixture(0x12345678, 22.5f, 45.0f, 1013.25f, 42);
    node = harness.node(0x12345678);
    REQUIRE(node != nullptr);
    CHECK(node->hasDeviceMetrics == true);
    CHECK(node->deviceMetrics.battery_level == 85);
    CHECK(node->hasEnvironmentMetrics == true);
    CHECK(node->environmentMetrics.temperature == doctest::Approx(22.5f));
    CHECK(node->environmentMetrics.iaq == 42);

    // Hops update
    harness.updateHopsFixture(0x12345678, 3);
    node = harness.node(0x12345678);
    REQUIRE(node != nullptr);
    CHECK(node->hopsAway == 3);
}

TEST_CASE("node model tracks removals and store purge integrity")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeModelFixture();

    harness.addNodeFixture(0x0001, "N1", "Node 1", 100);
    harness.addNodeFixture(0x0002, "N2", "Node 2", 200);
    CHECK(harness.store().size() == 2);
    CHECK(harness.node(0x0001) != nullptr);
    CHECK(harness.node(0x0002) != nullptr);

    harness.resetNodeList();
    CHECK(harness.store().size() == 0);
    CHECK(harness.node(0x0001) == nullptr);
}

TEST_CASE("unknown ingress keeps model identity and MQTT provenance")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeModelFixture();

    harness.addUnknownNodeFixture(0x1234abcd, 3, 1000, static_cast<uint8_t>(MeshtasticView::unknown), false, true);

    const auto *node = harness.node(0x1234abcd);
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasUser);
    CHECK(node->viaMqtt);
    CHECK(node->channel == 3);
    CHECK(std::string(node->user.short_name) == "abcd");
    CHECK(std::string(node->user.long_name) == "Meshtastic abcd");
}

TEST_CASE("last-heard updates keep the model in sync with the node row")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeModelFixture();
    harness.addNodeFixture(0x12345678, "ALPH", "Alpha Node", 100);
    harness.setCurrentTime(900);

    harness.updateLastHeardFixture(0x12345678);

    const auto *node = harness.node(0x12345678);
    REQUIRE(node != nullptr);
    CHECK(node->lastHeard == 900);
}

TEST_CASE("virtual NodeInfo refreshes retain established channel and last-heard model fields")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();

    constexpr NodeId nodeId = 0x12345678;
    harness.addOrUpdateNodeFixture(nodeId, "ONE", "First NodeInfo", 100, MeshtasticView::client, true, 3);
    harness.addOrUpdateNodeFixture(nodeId, "TWO", "Refreshed NodeInfo", 200, MeshtasticView::router, true, 6);

    const NodeRecord *node = harness.node(nodeId);
    REQUIRE(node != nullptr);
    CHECK(node->channel == 3);
    CHECK(node->lastHeard == 100);
    CHECK(std::string(node->user.long_name) == "Refreshed NodeInfo");
}

TEST_CASE("default node list keeps retained row panels as the production path")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.populateLegacyNodeFixtures(25);

    CHECK_FALSE(harness.virtualNodeListEnabled());
    CHECK(harness.legacyRetainedNodeCount() == 25);
    CHECK(harness.renderedNodeCount() == 25);
    CHECK(harness.store().size() == 0);
    CHECK(harness.visibleIndex().size() == 0);
}

TEST_CASE("retained node updates pad a one-character short-name cache without distance")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    constexpr NodeId nodeId = 0x12345678;
    harness.addNodeFixture(nodeId, "OLD", "Retained Node", 100);
    harness.updateNodeFixture(nodeId, "A", "Updated Node", static_cast<uint8_t>(MeshtasticView::client), true);

    const auto cachedShortName = harness.nodeShortNameCache(nodeId);
    CHECK(cachedShortName == std::array<char, 4>{'A', ' ', ' ', ' '});
}

TEST_CASE("visible node index resyncs after retained model mutations")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeModelFixture();
    harness.setCurrentTime(1700000000U);

    SUBCASE("insertion")
    {
        harness.addNodeFixture(0x11111111, "ONE", "One Node", 100);
        REQUIRE(harness.visibleIndex().size() == 1);
        CHECK(harness.visibleIndex().ids()[0] == 0x11111111);
    }

    SUBCASE("user update")
    {
        harness.addNodeFixture(0x11111111, "ONE", "One Node", 100);
        harness.updateNodeFixture(0x11111111, "TWO", "Two Node", static_cast<uint8_t>(MeshtasticView::router), true);
        REQUIRE(harness.visibleIndex().size() == 1);
        const NodeRecord *node = harness.node(harness.visibleIndex().ids()[0]);
        REQUIRE(node != nullptr);
        CHECK(std::string(node->user.long_name) == "Two Node");
    }

    SUBCASE("filter change")
    {
        harness.addNodeFixture(0x11111111, "OLD", "Old Node", 100);
        harness.addNodeFixture(0x22222222, "NEW", "New Node", 1699999990U);
        harness.setOfflineFilterFixture(true);
        REQUIRE(harness.visibleIndex().size() == 1);
        CHECK(harness.visibleIndex().ids()[0] == 0x22222222);
    }

    SUBCASE("active chat change")
    {
        harness.addNodeFixture(0x11111111, "ONE", "One Node", 100);
        harness.addActiveChatFixture(0x11111111);
        REQUIRE(harness.visibleIndex().size() == 1);
        const NodeRecord *node = harness.node(harness.visibleIndex().ids()[0]);
        REQUIRE(node != nullptr);
        CHECK(node->hasActiveChat);
    }

    SUBCASE("last-heard reorder")
    {
        harness.addNodeFixture(0x11111111, "ONE", "One Node", 100);
        harness.addNodeFixture(0x22222222, "TWO", "Two Node", 200);
        harness.setCurrentTime(300);
        harness.updateLastHeardFixture(0x11111111);
        REQUIRE(harness.visibleIndex().size() == 2);
        CHECK(harness.visibleIndex().ids()[0] == 0x11111111);
        CHECK(harness.visibleIndex().ids()[1] == 0x22222222);
    }

    SUBCASE("presentation resync")
    {
        harness.addNodeFixture(0x11111111, "ONE", "One Node", 100);
        harness.toggleResyncPresentationFixture();
        REQUIRE(harness.visibleIndex().size() == 1);
        CHECK(harness.visibleIndex().ids()[0] == 0x11111111);
    }

    SUBCASE("purge")
    {
        harness.addUntilPurgeFixture(251);
        CHECK(harness.store().size() == 250);
        CHECK(harness.visibleIndex().size() == 250);
        CHECK_FALSE(harness.visibleIndex().contains(0xb0000000U));
    }
}

#ifdef DEVICE_UI_MUI_VIRTUAL_NODE_LIST
namespace
{
NodeId renderedVirtualNodeAt(MuiTestHarness &harness, size_t visibleIndex)
{
    lv_obj_t *root = harness.nodeListRootForTesting();
    REQUIRE(root != nullptr);

    size_t seen = 0;
    const uint32_t childCount = lv_obj_get_child_count(root);
    for (uint32_t index = 0; index < childCount; ++index) {
        lv_obj_t *child = lv_obj_get_child(root, static_cast<int32_t>(index));
        if (!child || lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN) || lv_obj_get_child_count(child) < 11) {
            continue;
        }
        if (seen == visibleIndex) {
            return static_cast<NodeId>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(child)));
        }
        seen++;
    }

    FAIL("visible virtual row not found");
    return 0;
}

lv_obj_t *virtualRowForNode(MuiTestHarness &harness, NodeId id)
{
    lv_obj_t *root = harness.nodeListRootForTesting();
    if (!root) {
        return nullptr;
    }

    const uint32_t childCount = lv_obj_get_child_count(root);
    for (uint32_t index = 0; index < childCount; ++index) {
        lv_obj_t *row = lv_obj_get_child(root, static_cast<int32_t>(index));
        if (row && !lv_obj_has_flag(row, LV_OBJ_FLAG_HIDDEN) &&
            static_cast<NodeId>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(row))) == id) {
            return row;
        }
    }
    return nullptr;
}

// Row accent colors as read back through lv_color_to_u32.
constexpr uint32_t kMissingKeyImageBorderRed = 0xffff5555U;
constexpr uint32_t kHighlightMeshRowBorder = 0xff67ea94U;

MuiRowSnapshot virtualRowSnapshot(MuiTestHarness &harness, NodeId id)
{
    lv_obj_t *root = harness.nodeListRootForTesting();
    if (!root) {
        return {};
    }

    const uint32_t childCount = lv_obj_get_child_count(root);
    for (uint32_t index = 0; index < childCount; ++index) {
        lv_obj_t *row = lv_obj_get_child(root, static_cast<int32_t>(index));
        if (!row || lv_obj_has_flag(row, LV_OBJ_FLAG_HIDDEN) || lv_obj_get_child_count(row) < 11 ||
            static_cast<NodeId>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(row))) != id) {
            continue;
        }
        return snapshotMuiRow(row);
    }
    return {};
}

lv_obj_t *virtualPositionLabel(MuiTestHarness &harness, NodeId id)
{
    lv_obj_t *root = harness.nodeListRootForTesting();
    if (!root) {
        return nullptr;
    }

    const uint32_t childCount = lv_obj_get_child_count(root);
    for (uint32_t index = 0; index < childCount; ++index) {
        lv_obj_t *row = lv_obj_get_child(root, static_cast<int32_t>(index));
        if (row && !lv_obj_has_flag(row, LV_OBJ_FLAG_HIDDEN) && lv_obj_get_child_count(row) >= 11 &&
            static_cast<NodeId>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(row))) == id) {
            return lv_obj_get_child(row, 7);
        }
    }
    return nullptr;
}
} // namespace

TEST_CASE("gated virtual node list reuses the bounded retained-list viewport")
{
    for (size_t nodeCount : {25U, 100U, 250U}) {
        CAPTURE(nodeCount);
        MuiTestHarness harness;
        harness.resetNodeList();
        harness.enableVirtualNodeListFixture();
        harness.populateLegacyNodeFixtures(nodeCount);
        harness.showNodesScreen();

        CHECK(harness.virtualNodeListEnabled());
        CHECK(harness.legacyNodeListRootForTesting() == harness.nodeListRootForTesting());
        CHECK(harness.legacyRetainedNodeCount() == 0);
        CHECK(harness.renderedNodeCount() == nodeCount);
        CHECK(harness.visibleIndex().size() == nodeCount);
        CHECK(harness.nodeListObjectCount() <= 100);
    }
}

TEST_CASE("gated virtual node list restores the retained-list viewport when reset")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.populateLegacyNodeFixtures(30);
    harness.showNodesScreen();

    lv_obj_t *root = harness.nodeListRootForTesting();
    REQUIRE(root != nullptr);
    lv_obj_scroll_by(root, 0, 100, LV_ANIM_OFF);
    harness.pump();
    REQUIRE(lv_obj_get_scroll_y(root) != 0);

    harness.resetNodeList();
    root = harness.legacyNodeListRootForTesting();
    REQUIRE(root != nullptr);
    CHECK(lv_obj_get_scroll_y(root) == 0);
    CHECK(lv_obj_get_style_layout(root, LV_PART_MAIN) == LV_LAYOUT_FLEX);
    REQUIRE(lv_obj_get_child_count(root) > 0);
    CHECK_FALSE(lv_obj_has_flag(lv_obj_get_child(root, 0), LV_OBJ_FLAG_HIDDEN));
}

TEST_CASE("gated virtual node list does not rebind its pool for a pixel scroll inside the same logical window")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.populateLegacyNodeFixtures(25);
    harness.showNodesScreen();

    lv_obj_t *root = harness.nodeListRootForTesting();
    REQUIRE(root != nullptr);
    const uint32_t bindsBeforePixelScroll = harness.virtualNodeListBindGeneration();

    lv_obj_scroll_by(root, 0, 1, LV_ANIM_OFF);
    harness.pump();

    CHECK(harness.virtualNodeListBindGeneration() == bindsBeforePixelScroll);
}

TEST_CASE("gated virtual node list consumes filter work after the first virtual sync")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    harness.addNodeFixture(0x11111111, "OLD", "Old Node", 100U);
    harness.addNodeFixture(0x22222222, "NEW", "New Node", 1699999990U);
    harness.setOfflineFilterFixture(true);

    REQUIRE(harness.visibleIndex().size() == 1);
    REQUIRE(renderedVirtualNodeAt(harness, 0) == 0x22222222);
    REQUIRE(virtualRowSnapshot(harness, 0x22222222).longName == "New Node");
    const uint32_t bindsAfterFilterSync = harness.virtualNodeListBindGeneration();

    harness.viewForTesting()->task_handler();
    harness.viewForTesting()->task_handler();

    CHECK(harness.visibleIndex().size() == 1);
    CHECK(renderedVirtualNodeAt(harness, 0) == 0x22222222);
    CHECK(virtualRowSnapshot(harness, 0x22222222).longName == "New Node");
    CHECK(harness.virtualNodeListBindGeneration() == bindsAfterFilterSync);
}

TEST_CASE("gated virtual node list renders mutation resyncs")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    SUBCASE("user update rebinds visible row text")
    {
        harness.addNodeFixture(0x11111111, "ONE", "One Node", 1699999900U);
        REQUIRE(virtualRowSnapshot(harness, 0x11111111).longName == "One Node");

        harness.updateNodeFixture(0x11111111, "TWO", "Two Node", static_cast<uint8_t>(MeshtasticView::router), true);

        CHECK(virtualRowSnapshot(harness, 0x11111111).longName == "Two Node");
        CHECK(virtualRowSnapshot(harness, 0x11111111).shortName == "TWO");
    }

    SUBCASE("filter change rebinds the visible virtual pool")
    {
        harness.addNodeFixture(0x11111111, "OLD", "Old Node", 100U);
        harness.addNodeFixture(0x22222222, "NEW", "New Node", 1699999990U);

        harness.setOfflineFilterFixture(true);

        CHECK(virtualRowSnapshot(harness, 0x11111111).longName.empty());
        CHECK(virtualRowSnapshot(harness, 0x22222222).longName == "New Node");
        CHECK(renderedVirtualNodeAt(harness, 0) == 0x22222222);
    }

    SUBCASE("active-chat mutation is reflected in the model used by virtual purge")
    {
        harness.addNodeFixture(0x11111111, "KEEP", "Active Chat Node", 1U);
        harness.addNodeFixture(0x22222222, "DROP", "Purge Candidate", 2U);
        const uint32_t bindsBeforeActiveChat = harness.virtualNodeListBindGeneration();

        harness.addActiveChatFixture(0x11111111);
        const uint32_t bindsAfterActiveChat = harness.virtualNodeListBindGeneration();

        CHECK(bindsAfterActiveChat > bindsBeforeActiveChat);
        harness.addUntilPurgeFixture(249);

        CHECK(harness.node(0x11111111) != nullptr);
        CHECK(harness.node(0x22222222) == nullptr);
        CHECK(harness.store().size() == 250);
        CHECK(harness.visibleIndex().size() == 250);
        CHECK(renderedVirtualNodeAt(harness, 0) == 0xb00000f8U);
    }

    SUBCASE("last-heard reorder rebinds first rendered row")
    {
        harness.addNodeFixture(0x11111111, "ONE", "One Node", 100U);
        harness.addNodeFixture(0x22222222, "TWO", "Two Node", 200U);
        REQUIRE(renderedVirtualNodeAt(harness, 0) == 0x22222222);

        harness.setCurrentTime(300U);
        harness.updateLastHeardFixture(0x11111111);

        CHECK(renderedVirtualNodeAt(harness, 0) == 0x11111111);
        CHECK(virtualRowSnapshot(harness, 0x11111111).lastHeard == "now");
    }

    SUBCASE("presentation resync preserves virtual host and re-renders rows")
    {
        harness.addNodeFixture(0x11111111, "ONE", "One Node", 1699999900U);
        lv_obj_t *host = harness.nodeListRootForTesting();
        const uint32_t bindsBeforeResync = harness.virtualNodeListBindGeneration();

        harness.toggleResyncPresentationFixture();

        CHECK(harness.virtualNodeListBindGeneration() > bindsBeforeResync);
        CHECK(harness.nodeListRootForTesting() == host);
        CHECK(virtualRowSnapshot(harness, 0x11111111).longName == "One Node");
        CHECK(renderedVirtualNodeAt(harness, 0) == 0x11111111);
    }

    SUBCASE("purge removes the oldest model node and rebinds visible rows")
    {
        harness.addUntilPurgeFixture(251);

        CHECK(harness.store().size() == 250);
        CHECK(harness.visibleIndex().size() == 250);
        CHECK(harness.node(0xb0000000U) == nullptr);
        CHECK(virtualRowSnapshot(harness, 0xb0000000U).longName.empty());
        CHECK(renderedVirtualNodeAt(harness, 0) == 0xb00000faU);
    }
}

TEST_CASE("gated virtual node list avoids redundant mutation presentation work")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    for (uint32_t i = 1; i <= 20; ++i) {
        char shortName[8]{};
        char longName[32]{};
        std::snprintf(shortName, sizeof(shortName), "N%02u", i);
        std::snprintf(longName, sizeof(longName), "Node %02u", i);
        harness.addNodeFixture(i, shortName, longName, 1000U + i);
    }
    harness.showNodesScreen();

    SUBCASE("unchanged visible user mutation does not rebind")
    {
        const auto *record = harness.node(20);
        REQUIRE(record != nullptr);
        const uint32_t bindsBefore = harness.virtualNodeListBindGeneration();

        harness.viewForTesting()->updateNode(20, record->channel, record->user);

        CHECK(harness.virtualNodeListBindGeneration() == bindsBefore);
        CHECK(virtualRowSnapshot(harness, 20).longName == "Node 20");
    }

    SUBCASE("offscreen content mutation does not rebind")
    {
        const uint32_t bindsBefore = harness.virtualNodeListBindGeneration();

        harness.updateSignalFixture(1, -87, 6.2f);

        CHECK(harness.virtualNodeListBindGeneration() == bindsBefore);
        CHECK(virtualRowSnapshot(harness, 1).signal.empty());
    }

    SUBCASE("visible signal mutation refreshes only that row")
    {
        const uint32_t bindsBefore = harness.virtualNodeListBindGeneration();

        harness.updateSignalFixture(20, -88, 7.5f);

        CHECK(harness.virtualNodeListBindGeneration() == bindsBefore + 1);
        CHECK(virtualRowSnapshot(harness, 20).signal == "rssi: -88 snr: 7.5");
    }

    SUBCASE("power metrics without node model state do not rebind virtual rows")
    {
        meshtastic_PowerMetrics metrics = meshtastic_PowerMetrics_init_default;
        metrics.has_ch1_voltage = true;
        metrics.ch1_voltage = 4.2f;
        metrics.has_ch1_current = true;
        metrics.ch1_current = 12.5f;
        const uint32_t bindsBefore = harness.virtualNodeListBindGeneration();

        harness.viewForTesting()->updatePowerMetrics(20, metrics);

        CHECK(harness.virtualNodeListBindGeneration() == bindsBefore);
    }

    SUBCASE("visible signal mutation during expansion preserves interpolated geometry")
    {
        harness.dispatchVirtualNodeEvent(20, LV_EVENT_CLICKED);
        harness.pump(100);
        lv_obj_t *row = virtualRowForNode(harness, 20);
        REQUIRE(row != nullptr);
        const int32_t heightBefore = lv_obj_get_height(row);
        const int32_t yBefore = lv_obj_get_y(row);
        REQUIRE(heightBefore > VirtualNodeList::COLLAPSED_ROW_HEIGHT);
        REQUIRE(heightBefore < VirtualNodeList::EXPANDED_ROW_HEIGHT);

        harness.updateSignalFixture(20, -88, 7.5f);

        CHECK(lv_obj_get_height(row) == heightBefore);
        CHECK(lv_obj_get_y(row) == yBefore);
        CHECK(virtualRowSnapshot(harness, 20).signal == "rssi: -88 snr: 7.5");
    }

    SUBCASE("public insertion path performs at most one visible pool bind")
    {
        meshtastic_User user = meshtastic_User_init_default;
        std::strncpy(user.short_name, "NEW", sizeof(user.short_name) - 1);
        std::strncpy(user.long_name, "Newest Node", sizeof(user.long_name) - 1);
        user.role = meshtastic_Config_DeviceConfig_Role_CLIENT;
        user.public_key.size = 1;
        const uint32_t bindsBefore = harness.virtualNodeListBindGeneration();

        harness.viewForTesting()->addOrUpdateNode(0x101, 0, 1700000000U, user);

        CHECK(harness.virtualNodeListBindGeneration() <= bindsBefore + VirtualNodeList::POOL_SIZE);
        CHECK(renderedVirtualNodeAt(harness, 0) == 0x101);
    }

    SUBCASE("last-heard order mutation still refreshes the reordered window")
    {
        const uint32_t bindsBefore = harness.virtualNodeListBindGeneration();

        harness.updateLastHeardFixture(1);

        CHECK(harness.virtualNodeListBindGeneration() > bindsBefore);
        CHECK(renderedVirtualNodeAt(harness, 0) == 1);
        CHECK(virtualRowSnapshot(harness, 1).lastHeard == "now");
    }
}

TEST_CASE("gated virtual node list click expands and collapses by NodeId")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    harness.addNodeFixture(0x11111111, "ONE", "One Node", 1699999900U, MeshtasticView::router, true, false, 1);
    harness.updatePositionFixture(0x11111111, 377749000, -1224194000, 42, 8, 13);
    harness.showNodesScreen();

    CHECK(harness.selectedNode() == 0U);

    harness.dispatchVirtualNodeEvent(0x11111111, LV_EVENT_CLICKED);
    CHECK(harness.selectedNode() == 0x11111111);
    CHECK(virtualRowSnapshot(harness, 0x11111111).position1 == "37.77490 -122.41940");

    harness.dispatchVirtualNodeEvent(0x11111111, LV_EVENT_CLICKED);
    CHECK(harness.selectedNode() == 0x11111111);
    harness.pump(250);

    harness.dispatchVirtualNodeEvent(0x11111111, LV_EVENT_CLICKED);
    CHECK(harness.selectedNode() == 0U);
    harness.pump(250);
    // Extended detail labels stay populated and visible once data arrives; the
    // collapsed 53 px row clips them back down to a sliver.
    const MuiRowSnapshot collapsed = virtualRowSnapshot(harness, 0x11111111);
    CHECK(collapsed.position1 == "37.77490 -122.41940");
    CHECK_FALSE(collapsed.position1Hidden);
}

TEST_CASE("gated virtual node list ordinary click does not rebuild the visible index")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    for (uint32_t i = 1; i <= 30; ++i) {
        char shortName[8]{};
        char longName[32]{};
        std::snprintf(shortName, sizeof(shortName), "N%02u", i);
        std::snprintf(longName, sizeof(longName), "Selectable Node %02u", i);
        harness.addNodeFixture(i, shortName, longName, 1000U + i);
    }
    harness.showNodesScreen();

    const uint32_t generationBeforeClick = harness.visibleIndex().generation();
    const uint32_t bindsBeforeClick = harness.virtualNodeListBindGeneration();

    harness.dispatchVirtualNodeEvent(30, LV_EVENT_CLICKED);

    CHECK(harness.selectedNode() == 30U);
    CHECK(harness.visibleIndex().generation() == generationBeforeClick);
    CHECK(harness.virtualNodeListBindGeneration() > bindsBeforeClick);

    harness.pump(250);
    CHECK(virtualRowSnapshot(harness, 30).longName == "Selectable Node 30");
}

TEST_CASE("gated virtual node list long press opens only retained-permitted direct chats")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);
    harness.setLoRaHopLimit(7);

    harness.addNodeFixture(0x11111111, "ONE", "One Node", 1699999900U, MeshtasticView::router, true, false, 1);
    harness.addNodeFixture(0x22222222, "TWO", "Two Node", 1699999890U, MeshtasticView::client, false, true, 2);
    harness.updateHopsFixture(0x11111111, 2);
    harness.showNodesScreen();

    harness.dispatchVirtualNodeEvent(0x22222222, LV_EVENT_LONG_PRESSED);
    CHECK_FALSE(harness.messagesPanelVisible());
    CHECK(harness.lastTextMessage().to == 0U);

    harness.dispatchVirtualNodeEvent(0x11111111, LV_EVENT_LONG_PRESSED);
    REQUIRE(harness.messagesPanelVisible());
    harness.sendActiveText("hello");

    const MuiControllerCall text = harness.lastTextMessage();
    CHECK(text.to == 0x11111111);
    CHECK(text.channel == 1);
    CHECK(text.hopLimit == 3);
    CHECK(text.usePkc);
    CHECK(text.text == "hello");
}

TEST_CASE("gated virtual node list scan and traceroute route by selected NodeId")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);
    harness.setLoRaHopLimit(7);

    harness.addNodeFixture(0x11111111, "ONE", "One Node", 1699999900U, MeshtasticView::router, true, false, 1);
    harness.updateHopsFixture(0x11111111, 2);
    harness.showNodesScreen();
    harness.dispatchVirtualNodeEvent(0x11111111, LV_EVENT_CLICKED);

    harness.scanSignal();
    MuiControllerCall position = harness.lastPositionRequest();
    CHECK(position.to == 0x11111111);
    CHECK(position.channel == 1);

    harness.startTraceRoute();
    MuiControllerCall trace = harness.lastTraceRoute();
    CHECK(trace.to == 0x11111111);
    CHECK(trace.channel == 1);
    CHECK(trace.hopLimit == 3);
}

TEST_CASE("gated virtual trace-route result callbacks reopen nodes by model NodeId")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    harness.addNodeFixture(0x11111111, "ONE", "One Node", 1699999900U, MeshtasticView::router, true, false, 1);
    harness.showTraceRoute();
    REQUIRE(harness.traceRoutePanelVisible());

    harness.dispatchTraceRouteNodeCallback(0x11111111);

    CHECK(harness.nodesPanelVisible());
    CHECK(harness.selectedNode() == 0x11111111);
}

TEST_CASE("gated virtual map and chat result callbacks reopen nodes by model NodeId")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    harness.addNodeFixture(0x11111111, "ONE", "One Node", 1699999900U, MeshtasticView::router, true, false, 1);

    harness.dispatchMapNodeCallback(0x11111111);
    CHECK(harness.nodesPanelVisible());
    CHECK(harness.selectedNode() == 0x11111111);

    harness.showTraceRoute();
    REQUIRE(harness.traceRoutePanelVisible());
    harness.dispatchChatNodeCallback(0x11111111);
    CHECK(harness.nodesPanelVisible());
    CHECK(harness.selectedNode() == 0x11111111);
}

TEST_CASE("gated virtual bad-key routing updates model presentation and message reopen state")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    constexpr NodeId nodeId = 0x11111111;
    harness.addNodeFixture(nodeId, "ONE", "One Node", 1699999900U, MeshtasticView::router, true, false, 1);
    harness.updateHopsFixture(nodeId, 2);
    harness.sendDirectText(nodeId, "hello");

    harness.dispatchBadKeyRoutingError(nodeId);

    const NodeRecord *node = harness.node(nodeId);
    REQUIRE(node != nullptr);
    CHECK(node->hasKey);
    CHECK(node->hasBadKey);
    CHECK_FALSE(harness.nodeHasKey(nodeId));

    const MuiRowSnapshot badKeyRow = virtualRowSnapshot(harness, nodeId);
    CHECK(badKeyRow.imageSrc == reinterpret_cast<uintptr_t>(&img_node_router_image));

    harness.sendDirectText(nodeId, "retry");
    CHECK(harness.messagesPanelVisible());
    CHECK(harness.topMessagesNodeImageSrc() == reinterpret_cast<uintptr_t>(&img_lock_slash_image));
    CHECK_FALSE(harness.lastTextMessage().usePkc);
}

TEST_CASE("gated virtual bad-key routing repaints the visible row border immediately")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    constexpr NodeId nodeId = 0x11111111;
    harness.addNodeFixture(nodeId, "ONE", "One Node", 1699999900U, MeshtasticView::router, true, false, 1);
    harness.updateHopsFixture(nodeId, 2);
    harness.sendDirectText(nodeId, "hello");

    const MuiRowSnapshot before = virtualRowSnapshot(harness, nodeId);
    CHECK(before.imageBorder != kMissingKeyImageBorderRed);

    harness.dispatchBadKeyRoutingError(nodeId);

    const MuiRowSnapshot after = virtualRowSnapshot(harness, nodeId);
    CHECK(after.imageBorder == kMissingKeyImageBorderRed);
}

TEST_CASE("gated virtual node list keeps home battery widgets live from own-node metrics")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    constexpr NodeId ownNode = 0x1a2b3c4d;
    harness.setOwnNodeFixture(ownNode);
    harness.addNodeFixture(ownNode, "OWN", "Own Node", 1699999900U);
    harness.pump();

    CHECK(std::string(harness.homeBatteryPercentageText()) != "64%");
    CHECK(harness.homeBatteryImageSrc() != reinterpret_cast<uintptr_t>(&img_battery_mid_image));

    harness.updateMetricsFixture(ownNode, 64, 3.99F, 12.0F, 1.0F);

    CHECK(std::string(harness.homeBatteryPercentageText()) == "64%");
    CHECK(harness.homeBatteryImageSrc() == reinterpret_cast<uintptr_t>(&img_battery_mid_image));
}

TEST_CASE("gated virtual direct messages resolve chat titles from the node model")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    constexpr NodeId nodeId = 0x33333333;
    harness.addNodeFixture(nodeId, "THR", "Third Node", 1699999900U, MeshtasticView::router, true, false, 1);

    harness.sendDirectText(nodeId, "hello");

    CHECK(harness.messagesPanelVisible());
    CHECK(std::string(harness.chatButtonLabel()) == "THR: Third Node");
}

TEST_CASE("gated virtual name filtering matches rendered short names including id fallback")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    // Blank short names render as the %04x id fallback on both implementations.
    constexpr NodeId fallbackNode = 0x12345678;
    harness.addNodeFixture(fallbackNode, "", "", 1699999900U);
    harness.addNodeFixture(0x44444444, "NRM", "Normal Node", 1699999800U);
    harness.showNodesScreen();

    harness.setNodeNameFilter("5678");
    CHECK(harness.visibleIndex().contains(fallbackNode));
    CHECK_FALSE(harness.visibleIndex().contains(0x44444444));

    harness.setNodeNameFilter("NOMATCH");
    CHECK_FALSE(harness.visibleIndex().contains(fallbackNode));
}

TEST_CASE("gated virtual highlight matches the rendered short-name label like the retained row")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    constexpr NodeId fallbackNode = 0x12345678;
    harness.addNodeFixture(fallbackNode, "", "", 1699999900U);
    harness.showNodesScreen();

    harness.setNodeHighlightName("5678");
    CHECK(virtualRowSnapshot(harness, fallbackNode).rowBorder == kHighlightMeshRowBorder);
}

TEST_CASE("gated virtual name filtering matches the rendered distance line like the retained label search")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    constexpr NodeId ownNode = 0x1a2b3c4d;
    harness.setOwnNodeFixture(ownNode);
    harness.addNodeFixture(ownNode, "OWN", "Own Node", 1699999900U);
    harness.updatePositionFixture(ownNode, 377749000, -1224194000, 42, 8, 13);
    constexpr NodeId remoteNode = 0x22222222;
    harness.addNodeFixture(remoteNode, "REM", "Remote Node", 1699999800U);
    // Far enough away that the rendered distance line reads "km".
    harness.updatePositionFixture(remoteNode, 378776000, -1223950000, 42, 8, 13);
    harness.showNodesScreen();

    harness.setNodeNameFilter("km");
    CHECK(harness.visibleIndex().contains(remoteNode));
}

TEST_CASE("gated virtual node model keeps own-node settings widgets updated on rename")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    constexpr NodeId ownNode = 0x1a2b3c4d;
    harness.setOwnNodeFixture(ownNode);
    harness.addNodeFixture(ownNode, "OWN", "Own Node", 1699999900U);
    harness.pump();

    harness.updateNodeFixture(ownNode, "NEWS", "Renamed Own Node", MeshtasticView::client, true, false, 0);

    CHECK(std::string(harness.settingsUserLabelText()) == "User name: NEWS");
}

TEST_CASE("gated virtual node list keeps own node pinned first across recency promotions")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    constexpr NodeId ownNode = 0x1a2b3c4d;
    constexpr NodeId remoteNode = 0x22222222;
    harness.setOwnNodeFixture(ownNode);
    harness.addNodeFixture(ownNode, "OWN", "Own Node", 1699999900U);
    harness.addNodeFixture(remoteNode, "REM", "Remote Node", 1699999800U);
    harness.showNodesScreen();

    // A remote heard afterwards is promoted above every other remote...
    harness.updateLastHeardFixture(remoteNode);
    REQUIRE(harness.visibleIndex().ids().size() == 2);
    CHECK(harness.visibleIndex().ids()[0] == ownNode);
    CHECK(harness.visibleIndex().ids()[1] == remoteNode);

    // ...while the own node stays pinned to the first position.
    harness.runLastHeardTickFixture();
    CHECK(harness.visibleIndex().ids()[0] == ownNode);
    CHECK(harness.visibleIndex().ids()[1] == remoteNode);
}

TEST_CASE("gated virtual node list updates own-node last-heard model while keeping presentation pinned")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    constexpr NodeId ownNode = 0x1a2b3c4d;
    constexpr NodeId remoteNode = 0x22222222;
    harness.setOwnNodeFixture(ownNode);
    harness.addNodeFixture(ownNode, "OWN", "Own Node", 1699999900U);
    harness.addNodeFixture(remoteNode, "REM", "Remote Node", 1699999800U);
    harness.showNodesScreen();

    harness.setCurrentTime(1700000100U);
    harness.updateLastHeardFixture(ownNode);

    REQUIRE(harness.node(ownNode) != nullptr);
    CHECK(harness.node(ownNode)->lastHeard == 1700000100U);
    REQUIRE(harness.visibleIndex().ids().size() == 2);
    CHECK(harness.visibleIndex().ids()[0] == ownNode);
    CHECK(harness.visibleIndex().ids()[1] == remoteNode);
    CHECK(virtualRowSnapshot(harness, ownNode).lastHeard == "now");
}

TEST_CASE("gated virtual chat titles never fabricate a distance line for positionless peers")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    constexpr NodeId ownNode = 0x1a2b3c4d;
    constexpr NodeId peerNode = 0x33333333;
    harness.setOwnNodeFixture(ownNode);
    harness.addNodeFixture(ownNode, "OWN", "Own Node", 1699999900U);
    // Own device has a GPS fix; the peer does not.
    harness.updatePositionFixture(ownNode, 377749000, -1224194000, 42, 8, 13);

    harness.addNodeFixture(peerNode, "THR", "Third Node", 1699999800U);
    harness.sendDirectText(peerNode, "hello");

    CHECK(harness.messagesPanelVisible());
    CHECK(std::string(harness.chatButtonLabel()) == "THR: Third Node");
}

TEST_CASE("gated virtual node list routes expanded position clicks to map by NodeId")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    harness.addNodeFixture(0x11111111, "ONE", "One Node", 1699999900U, MeshtasticView::router, true, false, 1);
    harness.updatePositionFixture(0x11111111, 377749000, -1224194000, 42, 8, 13);
    harness.showNodesScreen();
    harness.dispatchVirtualNodeEvent(0x11111111, LV_EVENT_CLICKED);
    REQUIRE(virtualRowSnapshot(harness, 0x11111111).position1 == "37.77490 -122.41940");

    harness.dispatchVirtualNodePositionEvent(0x11111111);
    CHECK(harness.mapPanelVisible());
}

TEST_CASE("gated virtual node list keeps own-node coordinates non-clickable like the retained UI")
{
    constexpr NodeId ownNode = 0x11111111;
    constexpr NodeId remoteNode = 0x22222222;
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setOwnNodeFixture(ownNode);
    harness.addNodeFixture(ownNode, "OWN", "Own Node", 1699999900U);
    harness.addNodeFixture(remoteNode, "REM", "Remote Node", 1699999890U);
    harness.updatePositionFixture(ownNode, 377749000, -1224194000, 42, 8, 13);
    harness.updatePositionFixture(remoteNode, 377750000, -1224190000, 42, 8, 13);
    harness.showNodesScreen();

    harness.dispatchVirtualNodeEvent(ownNode, LV_EVENT_CLICKED);
    harness.pump(250);
    lv_obj_t *ownPosition = virtualPositionLabel(harness, ownNode);
    REQUIRE(ownPosition != nullptr);
    CHECK_FALSE(lv_obj_has_flag(ownPosition, LV_OBJ_FLAG_CLICKABLE));

    harness.dispatchVirtualNodeEvent(remoteNode, LV_EVENT_CLICKED);
    harness.pump(250);
    lv_obj_t *remotePosition = virtualPositionLabel(harness, remoteNode);
    REQUIRE(remotePosition != nullptr);
    CHECK(lv_obj_has_flag(remotePosition, LV_OBJ_FLAG_CLICKABLE));
}

TEST_CASE("node list reset clears own-node identity for subsequent harness fixtures")
{
    constexpr NodeId previousOwnNode = 0x11111111;
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.setOwnNodeFixture(previousOwnNode);

    harness.resetNodeList();
    harness.enableVirtualNodeModelFixture();
    harness.addNodeFixture(previousOwnNode, "REM", "Remote After Reset", 1699999900U);
    harness.updateHopsFixture(previousOwnNode, 2);

    CHECK(harness.legacyRowSnapshot(previousOwnNode).signal == "hops: 2");
}

TEST_CASE("gated virtual node list preserves retained same-second insertion and update recency order")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1000U);

    harness.addNodeFixture(0x30000000, "THR", "Third Inserted", 1000U);
    harness.addNodeFixture(0x10000000, "FIR", "First Inserted", 1000U);
    harness.addNodeFixture(0x20000000, "SEC", "Second Inserted", 1000U);
    CHECK(renderedVirtualNodeAt(harness, 0) == 0x30000000);
    CHECK(renderedVirtualNodeAt(harness, 1) == 0x10000000);
    CHECK(renderedVirtualNodeAt(harness, 2) == 0x20000000);
    CHECK(harness.nodePurgeCandidate(0x50000000) == 0x20000000);

    harness.updateLastHeardFixture(0x10000000);
    CHECK(renderedVirtualNodeAt(harness, 0) == 0x10000000);
    CHECK(harness.nodePurgeCandidate(0x50000000) == 0x20000000);

    harness.addNodeFixture(0x40000000, "FUT", "Future Clamped", 5000U);
    CHECK(renderedVirtualNodeAt(harness, 0) == 0x10000000);
    REQUIRE(harness.node(0x40000000) != nullptr);
    CHECK(harness.node(0x40000000)->lastHeard == 1000U);
    CHECK(harness.nodePurgeCandidate(0x50000000) == 0x40000000);
}

TEST_CASE("gated virtual purge selection matches retained equal-timestamp protection")
{
    NodeId retainedLegacyCandidate = 0;
    {
        MuiTestHarness harness;
        harness.resetNodeList();
        harness.enableVirtualNodeModelFixture();
        harness.setCurrentTime(1000U);
        harness.addUnknownNodeFixture(0x30000000U, 0, 900U, static_cast<uint8_t>(MeshtasticView::unknown), false, false);
        harness.addUnknownNodeFixture(0x10000000U, 0, 900U, static_cast<uint8_t>(MeshtasticView::unknown), false, false);
        harness.addUnknownNodeFixture(0x20000000U, 0, 900U, static_cast<uint8_t>(MeshtasticView::unknown), false, false);
        harness.addActiveChatFixture(0x20000000U);
        retainedLegacyCandidate = harness.nodePurgeCandidate(0x40000000U);
        CHECK(retainedLegacyCandidate == 0x10000000U);
    }

    MuiTestHarness virtualHarness;
    virtualHarness.resetNodeList();
    virtualHarness.enableVirtualNodeListFixture();
    virtualHarness.setCurrentTime(1000U);
    virtualHarness.addUnknownNodeFixture(0x30000000U, 0, 900U, static_cast<uint8_t>(MeshtasticView::unknown), false, false);
    virtualHarness.addUnknownNodeFixture(0x10000000U, 0, 900U, static_cast<uint8_t>(MeshtasticView::unknown), false, false);
    virtualHarness.addUnknownNodeFixture(0x20000000U, 0, 900U, static_cast<uint8_t>(MeshtasticView::unknown), false, false);
    virtualHarness.addActiveChatFixture(0x20000000U);

    CHECK(virtualHarness.nodePurgeCandidate(0x40000000U) == retainedLegacyCandidate);
}

TEST_CASE("gated virtual node list keeps selection separate from focus across recycling, filters, and purge")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    for (uint32_t i = 1; i <= 30; ++i) {
        char shortName[8]{};
        std::snprintf(shortName, sizeof(shortName), "N%02u", i);
        harness.addNodeFixture(i, shortName, "Selectable Node", 1000U + i);
    }
    harness.showNodesScreen();

    harness.scrollVirtualNodeIntoView(25);
    REQUIRE(harness.focusRenderedVirtualNode(25));
    CHECK(harness.selectedNode() == 0U);

    harness.setCurrentTime(2000U);
    harness.updateLastHeardFixture(1);
    CHECK(harness.selectedNode() == 0U);
    CHECK(harness.node(25) != nullptr);

    harness.setPositionFilterFixture(true);
    CHECK(harness.selectedNode() == 0U);
    CHECK(harness.node(25) != nullptr);

    harness.setPositionFilterFixture(false);
    CHECK(harness.selectedNode() == 0U);
    CHECK(virtualRowSnapshot(harness, 25).longName == "Selectable Node");

    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.addNodeFixture(1, "N01", "Stale Selected Node", 1U);
    harness.showNodesScreen();
    harness.dispatchVirtualNodeEvent(1, LV_EVENT_CLICKED);
    CHECK(harness.selectedNode() == 1U);
    harness.pump(250);
    harness.addUntilPurgeFixture(250);
    CHECK(harness.node(1) == nullptr);
    CHECK(harness.selectedNode() == 0U);
}

TEST_CASE("gated virtual node list group focus traverses past recycled pool boundaries")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.configureInputDevicesFixture(true, true, false);
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);

    for (uint32_t i = 1; i <= 30; ++i) {
        char shortName[8]{};
        std::snprintf(shortName, sizeof(shortName), "N%02u", i);
        harness.addNodeFixture(i, shortName, "Focusable Node", 1000U + i);
    }
    harness.showNodesScreen();

    const NodeId lastRendered = renderedVirtualNodeAt(harness, VirtualNodeList::POOL_SIZE - 1);
    auto lastIndex = harness.visibleIndex().indexOf(lastRendered);
    REQUIRE(lastIndex.has_value());
    REQUIRE(lastIndex.value() + 1 < harness.visibleIndex().ids().size());
    const NodeId nextLogical = harness.visibleIndex().ids()[lastIndex.value() + 1];

    REQUIRE(harness.focusRenderedVirtualNode(lastRendered));
    CHECK(harness.selectedNode() == 0U);
    REQUIRE(harness.virtualNavigationGroup() != nullptr);
    CHECK(harness.keyboardInputGroup() == harness.virtualNavigationGroup());
    CHECK(harness.encoderInputGroup() == harness.virtualNavigationGroup());

    harness.focusNextInVirtualGroup();
    CHECK(harness.selectedNode() == 0U);
    CHECK(virtualRowSnapshot(harness, nextLogical).longName == "Focusable Node");

    harness.scrollVirtualNodeIntoView(20);
    const NodeId firstRendered = renderedVirtualNodeAt(harness, 0);
    auto firstIndex = harness.visibleIndex().indexOf(firstRendered);
    REQUIRE(firstIndex.has_value());
    REQUIRE(firstIndex.value() > 0);
    const NodeId previousLogical = harness.visibleIndex().ids()[firstIndex.value() - 1];

    REQUIRE(harness.focusRenderedVirtualNode(firstRendered));
    CHECK(harness.selectedNode() == 0U);
    CHECK(harness.keyboardInputGroup() == harness.virtualNavigationGroup());
    CHECK(harness.encoderInputGroup() == harness.virtualNavigationGroup());
    harness.focusPreviousInVirtualGroup();
    CHECK(harness.selectedNode() == 0U);
    CHECK(virtualRowSnapshot(harness, previousLogical).longName == "Focusable Node");

    harness.configureInputDevicesFixture(false, false, false);
}

TEST_CASE("gated virtual node screen assigns configured input devices to the private group and restores default")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    lv_group_t *defaultGroup = lv_group_create();
    REQUIRE(defaultGroup != nullptr);
    lv_group_set_default(defaultGroup);

    harness.configureInputDevicesFixture(true, true, true);
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);
    for (uint32_t i = 1; i <= 10; ++i) {
        char shortName[8]{};
        std::snprintf(shortName, sizeof(shortName), "N%02u", i);
        harness.addNodeFixture(i, shortName, "Input Group Node", 1000U + i);
    }

    harness.showNodesScreen();
    REQUIRE(harness.virtualNavigationGroup() != nullptr);
    CHECK(harness.virtualNavigationGroup() != defaultGroup);
    CHECK(harness.keyboardInputGroup() == harness.virtualNavigationGroup());
    CHECK(harness.encoderInputGroup() == harness.virtualNavigationGroup());
    CHECK(harness.pointerInputGroup() == nullptr);

    harness.showTraceRoute();
    CHECK(harness.keyboardInputGroup() == defaultGroup);
    CHECK(harness.encoderInputGroup() == defaultGroup);
    CHECK(harness.pointerInputGroup() == nullptr);

    harness.configureInputDevicesFixture(false, false, false);
    lv_group_set_default(nullptr);
    lv_group_delete(defaultGroup);
}

TEST_CASE("gated virtual node list handles keypad and encoder events at logical boundaries")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    lv_group_t *defaultGroup = lv_group_create();
    REQUIRE(defaultGroup != nullptr);
    lv_group_set_default(defaultGroup);
    harness.configureInputDevicesFixture(true, true, false);
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(1700000000U);
    harness.addNodeFixture(1, "ONE", "First Node", 1001U);
    harness.addNodeFixture(2, "TWO", "Last Node", 1002U);
    harness.showNodesScreen();
    harness.pump(6000);
    CHECK(lv_group_get_obj_count(harness.virtualNavigationGroup()) == 2U);

    REQUIRE(harness.focusRenderedVirtualNode(2));
    harness.dispatchKeyboardKey(LV_KEY_PREV);
    CHECK(harness.keyboardInputGroup() == lv_group_get_default());
    CHECK(harness.encoderInputGroup() == lv_group_get_default());

    harness.showNodesScreen();
    harness.pump(6000);
    REQUIRE(harness.focusRenderedVirtualNode(1));
    harness.dispatchEncoderDelta(1);
    CHECK(harness.keyboardInputGroup() == lv_group_get_default());
    CHECK(harness.encoderInputGroup() == lv_group_get_default());

    harness.showNodesScreen();
    harness.pump(6000);
    REQUIRE(harness.focusRenderedVirtualNode(1));
    harness.dispatchKeyboardKey(LV_KEY_ENTER);
    CHECK(harness.selectedNode() == 1U);

    harness.showNodesScreen();
    harness.pump(6000);
    REQUIRE(harness.focusRenderedVirtualNode(2));
    harness.dispatchEncoderKey(LV_KEY_ENTER, true);
    CHECK(harness.messagesPanelVisible());

    harness.configureInputDevicesFixture(false, false, false);
    lv_group_set_default(nullptr);
    lv_group_delete(defaultGroup);
}

TEST_CASE("gated virtual empty node list returns touchless input to global navigation")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    lv_group_t *defaultGroup = lv_group_create();
    REQUIRE(defaultGroup != nullptr);
    lv_group_set_default(defaultGroup);
    harness.configureInputDevicesFixture(true, true, false);
    harness.enableVirtualNodeListFixture();

    harness.showNodesScreen();

    CHECK(harness.keyboardInputGroup() == defaultGroup);
    CHECK(harness.encoderInputGroup() == defaultGroup);

    harness.configureInputDevicesFixture(false, false, false);
    lv_group_set_default(nullptr);
    lv_group_delete(defaultGroup);
}

TEST_CASE("gated virtual node screen adopts its private input group when populated live")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    lv_group_t *defaultGroup = lv_group_create();
    REQUIRE(defaultGroup != nullptr);
    lv_group_set_default(defaultGroup);
    harness.configureInputDevicesFixture(true, true, false);
    harness.enableVirtualNodeListFixture();

    harness.showNodesScreen();
    REQUIRE(harness.keyboardInputGroup() == defaultGroup);
    REQUIRE(harness.encoderInputGroup() == defaultGroup);

    harness.addNodeFixture(1, "ONE", "Live Node", 100);
    REQUIRE(harness.virtualNavigationGroup() != nullptr);
    CHECK(harness.keyboardInputGroup() == harness.virtualNavigationGroup());
    CHECK(harness.encoderInputGroup() == harness.virtualNavigationGroup());

    harness.setNodeNameFilter("missing");
    CHECK(harness.keyboardInputGroup() == defaultGroup);
    CHECK(harness.encoderInputGroup() == defaultGroup);

    harness.configureInputDevicesFixture(false, false, false);
    lv_group_set_default(nullptr);
    lv_group_delete(defaultGroup);
}

TEST_CASE("gated virtual map hides markers created while a name filter is active")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setNodeNameFilter("include");
    constexpr NodeId nodeId = 0x12345678;
    harness.addNodeFixture(nodeId, "MISS", "Excluded Node", 100);
    harness.showMapScreen();

    harness.updatePositionFixture(nodeId, 377749000, -1224194000, 0, 0, 0);

    CHECK_FALSE(harness.visibleIndex().contains(nodeId));
    CHECK(harness.mapMarkerFiltered(nodeId));
}

TEST_CASE("gated virtual map applies active filters when it is created after markers")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setNodeNameFilter("include");
    constexpr NodeId nodeId = 0x87654321;
    harness.addNodeFixture(nodeId, "MISS", "Excluded Node", 100);
    harness.updatePositionFixture(nodeId, 377749000, -1224194000, 0, 0, 0);

    harness.showMapScreen();

    CHECK_FALSE(harness.visibleIndex().contains(nodeId));
    CHECK(harness.mapMarkerFiltered(nodeId));
}

TEST_CASE("gated virtual map skips filter work for recency-only reorders")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(2000);

    constexpr uint32_t nodeCount = 32;
    for (uint32_t id = 1; id <= nodeCount; ++id) {
        harness.addNodeFixture(id, "NODE", "Mapped Node", 1000 + id);
        harness.updatePositionFixture(id, 377749000 + static_cast<int32_t>(id), -1224194000 + static_cast<int32_t>(id));
    }
    harness.showMapScreen();
    REQUIRE(harness.visibleIndex().size() == nodeCount);

    harness.resetMapFilterCounters();
    harness.updateLastHeardFixture(1);

    REQUIRE(harness.visibleIndex().ids()[0] == 1);
    CHECK(harness.mapFilterUpdateCount() == 0);
    CHECK(harness.visibleNodeContainsCallCount() == 0);
}

TEST_CASE("gated virtual node list leaves boundary handback on the default group during a normal sync")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    lv_group_t *defaultGroup = lv_group_create();
    REQUIRE(defaultGroup != nullptr);
    lv_group_set_default(defaultGroup);
    harness.configureInputDevicesFixture(true, true, false);
    harness.enableVirtualNodeListFixture();
    harness.setCurrentTime(2000);
    harness.addNodeFixture(1, "ONE", "First Node", 1001U);
    harness.addNodeFixture(2, "TWO", "Last Node", 1002U);
    harness.showNodesScreen();
    REQUIRE(harness.focusRenderedVirtualNode(2));

    harness.dispatchKeyboardKey(LV_KEY_PREV);
    REQUIRE(harness.keyboardInputGroup() == defaultGroup);
    REQUIRE(harness.encoderInputGroup() == defaultGroup);

    harness.updateLastHeardFixture(1);
    CHECK(harness.keyboardInputGroup() == defaultGroup);
    CHECK(harness.encoderInputGroup() == defaultGroup);

    harness.configureInputDevicesFixture(false, false, false);
    lv_group_set_default(nullptr);
    lv_group_delete(defaultGroup);
}

TEST_CASE("gated virtual node list navigation does not take over the shared default group")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.setCurrentTime(1700000000U);

    lv_group_t *defaultGroup = lv_group_create();
    REQUIRE(defaultGroup != nullptr);
    lv_group_set_default(defaultGroup);
    lv_group_set_wrap(defaultGroup, true);
    lv_group_set_focus_cb(defaultGroup, defaultGroupFocusCallback);
    lv_group_set_edge_cb(defaultGroup, defaultGroupEdgeCallback);

    lv_obj_t *unrelated = lv_button_create(lv_screen_active());
    lv_group_add_obj(defaultGroup, unrelated);
    defaultGroupFocusCallbacks = 0;
    defaultGroupEdgeCallbacks = 0;

    harness.enableVirtualNodeListFixture();
    for (uint32_t i = 1; i <= 30; ++i) {
        char shortName[8]{};
        std::snprintf(shortName, sizeof(shortName), "N%02u", i);
        harness.addNodeFixture(i, shortName, "Focusable Node", 1000U + i);
    }
    harness.showNodesScreen();

    CHECK(lv_group_get_default() == defaultGroup);
    CHECK(lv_group_get_wrap(defaultGroup));
    CHECK(lv_group_get_focus_cb(defaultGroup) == defaultGroupFocusCallback);
    CHECK(lv_group_get_edge_cb(defaultGroup) == defaultGroupEdgeCallback);
    REQUIRE(harness.virtualNavigationGroup() != nullptr);
    CHECK(harness.virtualNavigationGroup() != defaultGroup);

    const NodeId lastRendered = renderedVirtualNodeAt(harness, VirtualNodeList::POOL_SIZE - 1);
    auto lastIndex = harness.visibleIndex().indexOf(lastRendered);
    REQUIRE(lastIndex.has_value());
    REQUIRE(harness.focusRenderedVirtualNode(lastRendered));
    defaultGroupFocusCallbacks = 0;
    defaultGroupEdgeCallbacks = 0;

    harness.focusNextInVirtualGroup();
    CHECK(harness.selectedNode() == 0U);
    CHECK(defaultGroupFocusCallbacks == 0);
    CHECK(defaultGroupEdgeCallbacks == 0);
    CHECK(lv_group_get_focused(defaultGroup) == unrelated);

    harness.scrollVirtualNodeIntoView(20);
    const NodeId firstRendered = renderedVirtualNodeAt(harness, 0);
    auto firstIndex = harness.visibleIndex().indexOf(firstRendered);
    REQUIRE(firstIndex.has_value());
    REQUIRE(firstIndex.value() > 0);
    REQUIRE(harness.focusRenderedVirtualNode(firstRendered));
    defaultGroupFocusCallbacks = 0;
    defaultGroupEdgeCallbacks = 0;

    harness.focusPreviousInVirtualGroup();
    CHECK(harness.selectedNode() == 0U);
    CHECK(defaultGroupFocusCallbacks == 0);
    CHECK(defaultGroupEdgeCallbacks == 0);
    CHECK(lv_group_get_focused(defaultGroup) == unrelated);

    harness.showTraceRoute();
    CHECK(lv_group_get_default() == defaultGroup);
    CHECK(lv_group_get_wrap(defaultGroup));
    CHECK(lv_group_get_focus_cb(defaultGroup) == defaultGroupFocusCallback);
    CHECK(lv_group_get_edge_cb(defaultGroup) == defaultGroupEdgeCallback);

    lv_group_set_default(nullptr);
    lv_obj_delete(unrelated);
    lv_group_delete(defaultGroup);
}

TEST_CASE("default node list selection is unchanged by virtual selection handling")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.setCurrentTime(1700000000U);

    harness.addNodeFixture(0x11111111, "ONE", "One Node", 1699999900U, MeshtasticView::router, true, false, 1);
    harness.showNodesScreen();
    harness.selectNode(0x11111111);
    REQUIRE_FALSE(harness.virtualNodeListEnabled());
    CHECK(harness.selectedNode() == 0x11111111);

    harness.setPositionFilterFixture(true);
    CHECK(harness.selectedNode() == 0x11111111);
}

TEST_CASE("active direct chats protect purge candidates in retained and virtual node lists")
{
    for (bool virtualMode : {false, true}) {
        CAPTURE(virtualMode);
        MuiTestHarness harness;
        harness.resetNodeList();
        if (virtualMode) {
            harness.enableVirtualNodeListFixture();
        } else {
            harness.enableVirtualNodeModelFixture();
        }
        harness.setCurrentTime(1000U);

        harness.addNodeFixture(0x10101010, "KEEP", "Active Chat Node", 1U);
        harness.addNodeFixture(0x20202020, "DROP", "Purge Candidate", 2U);
        harness.addActiveChatFixture(0x10101010);
        harness.addUntilPurgeFixture(249);

        CHECK(harness.node(0x10101010) != nullptr);
        CHECK(harness.node(0x20202020) == nullptr);
        CHECK(harness.store().size() == 250);
    }
}

TEST_CASE("cap enforcement terminates when every retained node is protected")
{
    for (bool virtualMode : {false, true}) {
        CAPTURE(virtualMode);
        MuiTestHarness harness;
        harness.resetNodeList();
        harness.setCurrentTime(1000U);
        if (virtualMode) {
            harness.enableVirtualNodeListFixture();
        } else {
            harness.enableVirtualNodeModelFixture();
        }

        for (uint32_t i = 0; i < 250; ++i) {
            const uint32_t id = 0xc0000000U + i;
            harness.addNodeFixture(id, "KEEP", "Protected Node", 1U + i);
            harness.addActiveChatFixture(id);
        }
        harness.addNodeFixture(0xd0000000U, "NEW", "Incoming Node", 2000U);

        CHECK(harness.renderedNodeCount() == 250);
        CHECK(harness.store().size() == 250);
        CHECK(harness.node(0xd0000000U) == nullptr);
    }
}

#endif

#if defined(DEVICE_UI_MUI_NODE_LIST_HW_BENCH)
TEST_CASE("hardware node list benchmark seeds fixtures and reports pointer gesture samples")
{
    MuiTestHarness harness;
    REQUIRE(harness.ready());

#if defined(DEVICE_UI_MUI_VIRTUAL_NODE_LIST)
    constexpr const char *expectedMode = "\"mode\":\"virtual\"";
#else
    constexpr const char *expectedMode = "\"mode\":\"legacy\"";
#endif

    harness.viewForTesting()->startNodeListHardwareBenchmark();
    harness.viewForTesting()->advanceNodeListHardwareBenchmark();
    harness.addNodeFixture(0xd0000000U, "LIVE", "Live Node", 1700000000U, MeshtasticView::client, true, false, 0);
    CHECK(harness.node(0xd0000000U) == nullptr);
    for (size_t attempts = 0; attempts < 64; ++attempts) {
        harness.viewForTesting()->advanceNodeListHardwareBenchmark();
        harness.pump(16);
    }

    const std::string progress = harness.viewForTesting()->nodeListHardwareBenchmarkReport();
    CHECK(progress.rfind("MUI_NODE_LIST_HW_BENCH_PROGRESS {", 0) == 0);
    CHECK(progress.find(expectedMode) != std::string::npos);
    CHECK(progress.find("\"gesture_injected\":true") != std::string::npos);
    CHECK(progress.find("\"gesture\":\"lvgl_pointer_drag_up\"") != std::string::npos);
    CHECK(progress.find("\"sample_count\":") != std::string::npos);
    CHECK(progress.find("\"movement_count\":") != std::string::npos);
    CHECK(progress.find("\"avg_gesture_us\":") != std::string::npos);
    CHECK(progress.find("\"complete\":false") != std::string::npos);

    for (size_t attempts = 0; attempts < 1280 && !harness.viewForTesting()->nodeListHardwareBenchmarkComplete(); ++attempts) {
        harness.viewForTesting()->advanceNodeListHardwareBenchmark();
        harness.pump(16);
    }

    REQUIRE(harness.viewForTesting()->nodeListHardwareBenchmarkComplete());

    const std::string report = harness.viewForTesting()->nodeListHardwareBenchmarkReport();
    CHECK(report.rfind("MUI_NODE_LIST_HW_BENCH {", 0) == 0);
    CHECK(report.find(expectedMode) != std::string::npos);
    CHECK(report.find("\"gesture_injected\":true") != std::string::npos);
    CHECK(report.find("\"physical_finger\":false") != std::string::npos);
    CHECK(report.find("\"fixtures\":250") != std::string::npos);
    CHECK(report.find("\"sample_count\":40") != std::string::npos);
    CHECK(report.find("\"movement_count\":") != std::string::npos);
    CHECK(report.find("\"avg_gesture_us\":") != std::string::npos);
    CHECK(report.find("\"p95_gesture_us\":") != std::string::npos);
    CHECK(report.find("\"worst_gesture_us\":") != std::string::npos);
    CHECK(report.find("\"memory_before\":{\"lvgl_free\":") != std::string::npos);
    CHECK(report.find("\"memory_after\":{\"lvgl_free\":") != std::string::npos);
    CHECK(report.find("\"error\":\"none\"") != std::string::npos);
    CHECK(report.find("\"reported\":true") != std::string::npos);

    const size_t firstRecord = report.find("MUI_NODE_LIST_HW_BENCH");
    REQUIRE(firstRecord != std::string::npos);
    CHECK(report.find("MUI_NODE_LIST_HW_BENCH", firstRecord + 1) == std::string::npos);
}
#endif
#endif
