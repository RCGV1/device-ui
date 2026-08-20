#include "MuiTestHarness.h"
#include "graphics/common/MeshtasticView.h"
#include "images.h"
#include <doctest/doctest.h>

#if defined(DEVICE_UI_HEADLESS_TEST) && !defined(DEVICE_UI_MUI_VIRTUAL_NODE_LIST)
TEST_CASE("macro-off default node list does not retain model or rebuild visible index")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.setCurrentTime(1700000000U);

    harness.addNodeFixture(0x11111111, "ONE", "One Node", 1699999900U, MeshtasticView::router, true, false, 1);
    harness.updatePositionFixture(0x11111111, 377749000, -1224194000, 42, 8, 13);
    harness.updateMetricsFixture(0x11111111, 85, 4.12f, 12.5f, 3.2f);
    harness.updateLastHeardFixture(0x11111111);

    CHECK(harness.renderedNodeCount() == 1);
    CHECK(harness.store().size() == 0);
    CHECK(harness.visibleIndex().size() == 0);
    CHECK_FALSE(harness.virtualNodeListEnabled());
}

TEST_CASE("macro-off legacy semantic helpers and actions use retained panel state without model population")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.setCurrentTime(1700000000U);
    harness.setLoRaHopLimit(7);

    const NodeId nodeId = 0x11112222U;
    harness.addNodeFixture(nodeId, "PNL1", "Panel One", 1699999900U, MeshtasticView::router, true, false, 3);
    harness.updateHopsFixture(nodeId, 2);
    harness.updatePositionFixture(nodeId, 377749000, -1224194000, 42, 8, 13);

    CHECK(harness.store().size() == 0);
    CHECK(harness.visibleIndex().size() == 0);
    CHECK(harness.nodeIsMessagable(nodeId));
    CHECK(harness.nodeChannel(nodeId) == 3);
    CHECK(harness.nodeHasKey(nodeId));
    CHECK(harness.nodeHops(nodeId) == 2);
    CHECK(harness.nodeDisplayName(nodeId) == doctest::String("Panel One"));
    CHECK(harness.nodeShortName(nodeId) == doctest::String("PNL1"));

    const NodePosition position = harness.nodePosition(nodeId);
    CHECK(position.known);
    CHECK(position.latitude == 377749000);
    CHECK(position.longitude == -1224194000);

    harness.selectNode(nodeId);
    harness.scanSignal();
    MuiControllerCall positionRequest = harness.lastPositionRequest();
    CHECK(positionRequest.to == nodeId);
    CHECK(positionRequest.channel == 3);

    harness.startTraceRoute();
    MuiControllerCall trace = harness.lastTraceRoute();
    CHECK(trace.to == nodeId);
    CHECK(trace.channel == 3);
    CHECK(trace.hopLimit == 3);

    harness.sendDirectText(nodeId, "hello");
    MuiControllerCall text = harness.lastTextMessage();
    CHECK(text.to == nodeId);
    CHECK(text.channel == 3);
    CHECK(text.hopLimit == 3);
    CHECK(text.usePkc);
    CHECK(text.text == "hello");

    harness.dispatchMapNodeCallback(nodeId);
    CHECK(harness.nodesPanelVisible());
    CHECK(harness.selectedNode() == nodeId);

    harness.dispatchChatNodeCallback(nodeId);
    CHECK(harness.nodesPanelVisible());
    CHECK(harness.selectedNode() == nodeId);

    harness.dispatchBadKeyRoutingError(nodeId);
    CHECK_FALSE(harness.nodeHasKey(nodeId));
    CHECK(harness.topMessagesNodeImageSrc() == reinterpret_cast<uintptr_t>(&img_lock_slash_image));

    harness.sendDirectText(nodeId, "retry");
    text = harness.lastTextMessage();
    CHECK(text.to == nodeId);
    CHECK(text.channel == 3);
    CHECK_FALSE(text.usePkc);
    CHECK(harness.store().size() == 0);
    CHECK(harness.visibleIndex().size() == 0);
}

TEST_CASE("macro-off scanner and traceroute selected targets render retained panel role and name")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.setCurrentTime(1700000000U);

    const NodeId router = 0x11113333U;
    harness.addNodeFixture(router, "RTR1", "Router One", 1699999900U, MeshtasticView::router, true, false, 1);
    harness.selectNode(router);

    harness.showSignalScanner();
    CHECK(harness.signalScannerTargetLabel() == doctest::String("RTR1"));
    CHECK(harness.signalScannerTargetImageSrc() == reinterpret_cast<uintptr_t>(&img_node_router_image));

    harness.showTraceRoute();
    CHECK(harness.traceRouteTargetLabel() == doctest::String("Router One"));
    CHECK(harness.traceRouteTargetImageSrc() == reinterpret_cast<uintptr_t>(&img_node_router_image));

    const NodeId blocked = 0x11114444U;
    harness.addNodeFixture(blocked, "BLK1", "Blocked One", 1699999800U, MeshtasticView::client, false, true, 1);
    harness.selectNode(blocked);

    harness.showSignalScanner();
    CHECK(harness.signalScannerTargetLabel() == doctest::String("BLK1"));
    CHECK(harness.signalScannerTargetImageSrc() == reinterpret_cast<uintptr_t>(&img_unmessagable_image));

    harness.showTraceRoute();
    CHECK(harness.traceRouteTargetLabel() == doctest::String("Blocked One"));
    CHECK(harness.traceRouteTargetImageSrc() == reinterpret_cast<uintptr_t>(&img_unmessagable_image));

    CHECK(harness.store().size() == 0);
    CHECK(harness.visibleIndex().size() == 0);
}

TEST_CASE("macro-off traceroute route results render retained panel node and reverse action")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.setCurrentTime(1700000000U);

    const NodeId router = 0x11115555U;
    harness.addNodeFixture(router, "RTR2", "Router Two", 1699999900U, MeshtasticView::router, true, false, 1);
    harness.showTraceRoute();
    harness.dispatchTraceRouteResult(router);

    CHECK(harness.firstTraceRouteTowardsLabel() == doctest::String("RTR2"));
    CHECK(harness.firstTraceRouteTowardsImageSrc() == reinterpret_cast<uintptr_t>(&img_node_router_image));

    harness.clickFirstTraceRouteTowardsButton();
    CHECK(harness.nodesPanelVisible());
    CHECK(harness.selectedNode() == router);
    CHECK(harness.store().size() == 0);
    CHECK(harness.visibleIndex().size() == 0);
}

TEST_CASE("macro-off purge keeps the retained legacy equal-timestamp row order")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.setCurrentTime(1000);

    harness.addNodeFixture(0x30000000U, "N300", "Node 300", 900, MeshtasticView::client, true, false, 0);
    harness.addNodeFixture(0x10000000U, "N100", "Node 100", 900, MeshtasticView::client, true, false, 0);
    harness.addNodeFixture(0x20000000U, "N200", "Node 200", 900, MeshtasticView::client, true, false, 0);

    CHECK(harness.nodePurgeCandidate(0x40000000U) == 0x20000000U);

    harness.updateLastHeardFixture(0x30000000U);
    CHECK(harness.nodePurgeCandidate(0x40000000U) == 0x20000000U);

    harness.addNodeFixture(0x50000000U, "N500", "Node 500", 2000, MeshtasticView::client, true, false, 0);
    CHECK(harness.nodePurgeCandidate(0x40000000U) == 0x20000000U);
}

TEST_CASE("macro-off purge prefers retained stale unknown rows before oldest named rows")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.setCurrentTime(1000);

    harness.addNodeFixture(0x10000000U, "OLD", "Old Named", 100, MeshtasticView::client, true, false, 0);
    harness.addUnknownNodeFixture(0x20000000U, 0, 800, static_cast<uint8_t>(MeshtasticView::unknown), false, false);
    harness.addNodeFixture(0x30000000U, "NEW", "New Named", 950, MeshtasticView::client, true, false, 0);
    harness.addNodeFixture(0x40000000U, "NWR", "Newer Named", 960, MeshtasticView::client, true, false, 0);
    harness.addNodeFixture(0x50000000U, "NWS", "Newest Named", 970, MeshtasticView::client, true, false, 0);

    CHECK(harness.nodePurgeCandidate(0x60000000U) == 0x20000000U);
}

#endif
