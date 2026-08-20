#include "MuiTestHarness.h"
#include "graphics/common/MeshtasticView.h"
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
#endif
