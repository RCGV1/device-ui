#include "MuiTestHarness.h"
#include "graphics/common/MeshtasticView.h"
#include <doctest/doctest.h>

#ifdef DEVICE_UI_HEADLESS_TEST
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
}

TEST_CASE("visible node index resyncs after retained model mutations")
{
    MuiTestHarness harness;
    harness.resetNodeList();
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
TEST_CASE("gated virtual node list uses a dedicated bounded host")
{
    for (size_t nodeCount : {25U, 100U, 250U}) {
        CAPTURE(nodeCount);
        MuiTestHarness harness;
        harness.resetNodeList();
        harness.enableVirtualNodeListFixture();
        harness.populateLegacyNodeFixtures(nodeCount);

        CHECK(harness.virtualNodeListEnabled());
        CHECK(harness.legacyRetainedNodeCount() == 0);
        CHECK(harness.renderedNodeCount() == nodeCount);
        CHECK(harness.visibleIndex().size() == nodeCount);
        CHECK(harness.nodeListObjectCount() <= 90);
    }
}
#endif
#endif
