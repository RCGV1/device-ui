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
#endif
