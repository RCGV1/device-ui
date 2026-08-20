#include "MuiTestHarness.h"
#include "graphics/common/MeshtasticView.h"
#include <doctest/doctest.h>

#ifdef DEVICE_UI_HEADLESS_TEST
TEST_CASE("node semantic helpers read model fields instead of retained legacy panel state")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    harness.addNodeFixture(0x1234abcd, "MODL", "Model Node", 1000, MeshtasticView::router, true, false, 3);
    harness.updateHopsFixture(0x1234abcd, 2);
    harness.updatePositionFixture(0x1234abcd, 377749000, -1224194000, 42, 8, 13);
    harness.corruptLegacyNodePanel(0x1234abcd);

    CHECK(harness.nodeIsMessagable(0x1234abcd));
    CHECK(harness.nodeChannel(0x1234abcd) == 3);
    CHECK(harness.nodeHasKey(0x1234abcd));
    CHECK(harness.nodeHops(0x1234abcd) == 2);
    CHECK(harness.nodeDisplayName(0x1234abcd) == doctest::String("Model Node"));
    CHECK(harness.nodeShortName(0x1234abcd) == doctest::String("MODL"));

    const NodePosition position = harness.nodePosition(0x1234abcd);
    CHECK(position.known);
    CHECK(position.latitude == 377749000);
    CHECK(position.longitude == -1224194000);
    CHECK(position.altitude == 42);
    CHECK(position.satellites == 8);
    CHECK(position.precision == 13);
}

TEST_CASE("node semantic helpers handle missing nodes with safe defaults")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    CHECK_FALSE(harness.nodeIsMessagable(0x40404040));
    CHECK(harness.nodeChannel(0x40404040) == 0);
    CHECK_FALSE(harness.nodeHasKey(0x40404040));
    CHECK(harness.nodeHops(0x40404040) == -1);
    CHECK(harness.nodeDisplayName(0x40404040) == nullptr);
    CHECK(harness.nodeShortName(0x40404040) == nullptr);
    CHECK_FALSE(harness.nodePosition(0x40404040).known);
}

TEST_CASE("active direct chats protect model purge candidate selection")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.setCurrentTime(1000);

    harness.addUnknownNodeFixture(0x10101010, 0, 100, static_cast<uint8_t>(MeshtasticView::unknown), false, false);
    harness.addUnknownNodeFixture(0x20202020, 0, 200, static_cast<uint8_t>(MeshtasticView::unknown), false, false);
    harness.addUnknownNodeFixture(0x30303030, 0, 300, static_cast<uint8_t>(MeshtasticView::unknown), false, false);
    harness.addActiveChatFixture(0x10101010);

    CHECK(harness.nodePurgeCandidate(0x30303030) == 0x20202020);
}
#endif
