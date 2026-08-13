#include "doctest.h"
#include "graphics/view/TFT/VirtualNodeList.h"
#include "tests/MuiTestHarness.h"

#ifdef DEVICE_UI_HEADLESS_TEST

class DummyActionSink : public NodeListActionSink
{
  public:
    void nodeClicked(NodeId id) override { lastClicked = id; }
    void nodeLongPressed(NodeId id) override { lastLongPressed = id; }
    void nodeFocused(NodeId id) override { lastFocused = id; }

    NodeId lastClicked = 0;
    NodeId lastLongPressed = 0;
    NodeId lastFocused = 0;
};

TEST_CASE("VirtualNodeList row pool remains strictly bounded")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    lv_obj_t *parent = harness.nodeListRootForTesting();
    REQUIRE(parent != nullptr);

    VirtualNodeList list(parent, sink);
    CHECK(list.boundRowCount() > 0);
    const size_t poolSize = list.boundRowCount();
    CHECK(poolSize <= 8); // Bounded to 6-8 reusable rows for 320x240 screen

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;

    // Load 25 nodes
    for (uint32_t i = 1; i <= 25; ++i) {
        meshtastic_User u{};
        snprintf(u.short_name, sizeof(u.short_name), "N%03u", i);
        snprintf(u.long_name, sizeof(u.long_name), "Node Number %u", i);
        store.upsertUser(i, 0, 1000 + i, u, false);
    }
    index.rebuild(store, filter, 0);
    list.sync(store, index, 0);

    const size_t objectsAt25 = harness.nodeListObjectCount();

    // Expand to 250 nodes
    for (uint32_t i = 26; i <= 250; ++i) {
        meshtastic_User u{};
        snprintf(u.short_name, sizeof(u.short_name), "N%03u", i);
        snprintf(u.long_name, sizeof(u.long_name), "Node Number %u", i);
        store.upsertUser(i, 0, 1000 + i, u, false);
    }
    index.rebuild(store, filter, 0);
    list.sync(store, index, 0);

    const size_t objectsAt250 = harness.nodeListObjectCount();

    // The LVGL object count in the node list must be IDENTICAL between 25 and 250 nodes!
    CHECK(objectsAt250 == objectsAt25);
}

TEST_CASE("VirtualNodeList expansion and stable selection")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    lv_obj_t *parent = harness.nodeListRootForTesting();
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;

    for (uint32_t i = 1; i <= 30; ++i) {
        meshtastic_User u{};
        snprintf(u.short_name, sizeof(u.short_name), "N%03u", i);
        store.upsertUser(i, 0, 1000 + i, u, false);
    }
    index.rebuild(store, filter, 0);
    list.sync(store, index, 0);

    // Expand node 5
    list.setExpanded(5);
    CHECK(list.getExpanded() == 5);

    // Scroll to node 20
    list.scrollTo(20, LV_ANIM_OFF);
    harness.pump();

    // Node 5 should still be recorded as expanded even when scrolled off-screen
    CHECK(list.getExpanded() == 5);
}

TEST_CASE("VirtualNodeList handles rapid deletion while scrolled down and overscroll gracefully")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    lv_obj_t *parent = harness.nodeListRootForTesting();
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;

    for (uint32_t i = 1; i <= 50; ++i) {
        meshtastic_User u{};
        snprintf(u.short_name, sizeof(u.short_name), "N%03u", i);
        store.upsertUser(i, 0, 1000 + i, u, false);
    }
    index.rebuild(store, filter, 0);
    list.sync(store, index, 0);

    // Scroll to the very bottom
    list.scrollTo(50, LV_ANIM_OFF);
    harness.pump();

    // Rapidly purge down to 2 nodes
    for (uint32_t i = 3; i <= 50; ++i) {
        store.remove(i);
    }
    index.rebuild(store, filter, 0);
    list.sync(store, index, 0);
    harness.pump();

    // Must not crash or blank out, remaining nodes must be rendered
    CHECK(index.ids().size() == 2);
}

#endif
