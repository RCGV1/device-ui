#include "MuiTestHarness.h"
#include "graphics/common/MeshtasticView.h"
#include "graphics/view/TFT/VirtualNodeList.h"
#include "images.h"
#include <doctest/doctest.h>

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

TEST_CASE("default node list keeps retained legacy rows as the production path")
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

        lv_obj_t *image = lv_obj_get_child(row, 0);
        lv_obj_t *shortName = lv_obj_get_child(row, 3);
        return {
            lv_label_get_text(lv_obj_get_child(row, 2)),
            lv_label_get_text(shortName),
            lv_label_get_text(lv_obj_get_child(row, 4)),
            lv_label_get_text(lv_obj_get_child(row, 5)),
            lv_label_get_text(lv_obj_get_child(row, 6)),
            lv_label_get_text(lv_obj_get_child(row, 7)),
            lv_label_get_text(lv_obj_get_child(row, 8)),
            lv_label_get_text(lv_obj_get_child(row, 9)),
            lv_label_get_text(lv_obj_get_child(row, 10)),
            lv_obj_get_y_aligned(shortName),
            lv_color_to_u32(lv_obj_get_style_bg_color(image, LV_PART_MAIN)),
            lv_color_to_u32(lv_obj_get_style_border_color(image, LV_PART_MAIN)),
            lv_color_to_u32(lv_obj_get_style_image_recolor(image, LV_PART_MAIN)),
            lv_obj_get_style_image_recolor_opa(image, LV_PART_MAIN),
            reinterpret_cast<uintptr_t>(lv_image_get_src(image)),
        };
    }
    return {};
}
} // namespace

TEST_CASE("gated virtual node list uses a dedicated bounded host")
{
    for (size_t nodeCount : {25U, 100U, 250U}) {
        CAPTURE(nodeCount);
        MuiTestHarness harness;
        harness.resetNodeList();
        harness.enableVirtualNodeListFixture();
        harness.populateLegacyNodeFixtures(nodeCount);

        CHECK(harness.virtualNodeListEnabled());
        CHECK(harness.legacyNodeListRootForTesting() != harness.nodeListRootForTesting());
        CHECK(lv_obj_get_parent(harness.nodeListRootForTesting()) == lv_obj_get_parent(harness.legacyNodeListRootForTesting()));
        CHECK(harness.legacyRetainedNodeCount() == 0);
        CHECK(harness.renderedNodeCount() == nodeCount);
        CHECK(harness.visibleIndex().size() == nodeCount);
        CHECK(harness.nodeListObjectCount() <= 90);
    }
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
    CHECK(harness.selectedNode() == 0U);
    CHECK(virtualRowSnapshot(harness, 0x11111111).position1.empty());
}

TEST_CASE("gated virtual node list long press opens only legacy-permitted direct chats")
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

TEST_CASE("gated virtual node list preserves legacy same-second insertion and update recency order")
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
    CHECK(harness.nodePurgeCandidate(0x50000000) == 0x30000000);

    harness.updateLastHeardFixture(0x20000000);
    CHECK(renderedVirtualNodeAt(harness, 0) == 0x20000000);
    CHECK(harness.nodePurgeCandidate(0x50000000) == 0x30000000);

    harness.addNodeFixture(0x40000000, "FUT", "Future Clamped", 5000U);
    CHECK(renderedVirtualNodeAt(harness, 0) == 0x20000000);
    REQUIRE(harness.node(0x40000000) != nullptr);
    CHECK(harness.node(0x40000000)->lastHeard == 1000U);
    CHECK(harness.nodePurgeCandidate(0x50000000) == 0x30000000);
}

TEST_CASE("gated virtual node list keeps selection stable across recycling, filters, and purge")
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
    CHECK(harness.selectedNode() == 25U);

    harness.setCurrentTime(2000U);
    harness.updateLastHeardFixture(1);
    CHECK(harness.selectedNode() == 25U);
    CHECK(harness.node(25) != nullptr);

    harness.setPositionFilterFixture(true);
    CHECK(harness.selectedNode() == 25U);
    CHECK(harness.node(25) != nullptr);

    harness.setPositionFilterFixture(false);
    CHECK(harness.selectedNode() == 25U);
    CHECK(virtualRowSnapshot(harness, 25).longName == "Selectable Node");

    harness.resetNodeList();
    harness.enableVirtualNodeListFixture();
    harness.addNodeFixture(1, "N01", "Stale Selected Node", 1U);
    harness.showNodesScreen();
    harness.dispatchVirtualNodeEvent(1, LV_EVENT_CLICKED);
    CHECK(harness.selectedNode() == 1U);
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
    CHECK(harness.selectedNode() == lastRendered);
    REQUIRE(harness.virtualNavigationGroup() != nullptr);
    CHECK(harness.keyboardInputGroup() == harness.virtualNavigationGroup());
    CHECK(harness.encoderInputGroup() == harness.virtualNavigationGroup());
    harness.focusNextInVirtualGroup();
    CHECK(harness.selectedNode() == nextLogical);
    CHECK(virtualRowSnapshot(harness, nextLogical).longName == "Focusable Node");

    harness.scrollVirtualNodeIntoView(20);
    const NodeId firstRendered = renderedVirtualNodeAt(harness, 0);
    auto firstIndex = harness.visibleIndex().indexOf(firstRendered);
    REQUIRE(firstIndex.has_value());
    REQUIRE(firstIndex.value() > 0);
    const NodeId previousLogical = harness.visibleIndex().ids()[firstIndex.value() - 1];

    REQUIRE(harness.focusRenderedVirtualNode(firstRendered));
    CHECK(harness.selectedNode() == firstRendered);
    CHECK(harness.keyboardInputGroup() == harness.virtualNavigationGroup());
    CHECK(harness.encoderInputGroup() == harness.virtualNavigationGroup());
    harness.focusPreviousInVirtualGroup();
    CHECK(harness.selectedNode() == previousLogical);
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
    const NodeId nextLogical = harness.visibleIndex().ids()[lastIndex.value() + 1];
    REQUIRE(harness.focusRenderedVirtualNode(lastRendered));
    defaultGroupFocusCallbacks = 0;
    defaultGroupEdgeCallbacks = 0;

    harness.focusNextInVirtualGroup();
    CHECK(harness.selectedNode() == nextLogical);
    CHECK(defaultGroupFocusCallbacks == 0);
    CHECK(defaultGroupEdgeCallbacks == 0);
    CHECK(lv_group_get_focused(defaultGroup) == unrelated);

    harness.scrollVirtualNodeIntoView(20);
    const NodeId firstRendered = renderedVirtualNodeAt(harness, 0);
    auto firstIndex = harness.visibleIndex().indexOf(firstRendered);
    REQUIRE(firstIndex.has_value());
    REQUIRE(firstIndex.value() > 0);
    const NodeId previousLogical = harness.visibleIndex().ids()[firstIndex.value() - 1];
    REQUIRE(harness.focusRenderedVirtualNode(firstRendered));
    defaultGroupFocusCallbacks = 0;
    defaultGroupEdgeCallbacks = 0;

    harness.focusPreviousInVirtualGroup();
    CHECK(harness.selectedNode() == previousLogical);
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

TEST_CASE("default legacy node list selection is unchanged by virtual selection handling")
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

TEST_CASE("active direct chats protect purge candidates in legacy and virtual node lists")
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
#endif
