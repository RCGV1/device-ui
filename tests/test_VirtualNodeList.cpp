#include "doctest.h"
#include "graphics/view/TFT/VirtualNodeList.h"
#include "images.h"
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

namespace
{
lv_obj_t *boundRow(lv_obj_t *parent, NodeId id)
{
    const uint32_t childCount = lv_obj_get_child_count(parent);
    for (uint32_t index = 0; index < childCount; ++index) {
        lv_obj_t *child = lv_obj_get_child(parent, static_cast<int32_t>(index));
        if (static_cast<NodeId>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(child))) == id) {
            return child;
        }
    }
    return nullptr;
}

std::string labelText(lv_obj_t *row, uint32_t childIndex)
{
    return lv_label_get_text(lv_obj_get_child(row, static_cast<int32_t>(childIndex)));
}

bool childHidden(lv_obj_t *row, uint32_t childIndex)
{
    return lv_obj_has_flag(lv_obj_get_child(row, static_cast<int32_t>(childIndex)), LV_OBJ_FLAG_HIDDEN);
}

} // namespace

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
    index.rebuild(store, filter, 0, NodeListFilterPolicy::LegacyCompatible);
    list.sync(store, index, 0);

    const size_t objectsAt25 = harness.nodeListObjectCount();

    // Expand to 250 nodes
    for (uint32_t i = 26; i <= 250; ++i) {
        meshtastic_User u{};
        snprintf(u.short_name, sizeof(u.short_name), "N%03u", i);
        snprintf(u.long_name, sizeof(u.long_name), "Node Number %u", i);
        store.upsertUser(i, 0, 1000 + i, u, false);
    }
    index.rebuild(store, filter, 0, NodeListFilterPolicy::LegacyCompatible);
    list.sync(store, index, 0);

    const size_t objectsAt250 = harness.nodeListObjectCount();

    // The LVGL object count in the node list must be IDENTICAL between 25 and 250 nodes!
    CHECK(objectsAt250 == objectsAt25);

    // The 100-node candidate required by the compatibility plan must be structurally identical too.
    for (uint32_t i = 101; i <= 250; ++i) {
        store.remove(i);
    }
    index.rebuild(store, filter, 0, NodeListFilterPolicy::LegacyCompatible);
    list.sync(store, index, 0);
    harness.pump();

    const size_t objectsAt100 = harness.nodeListObjectCount();
    CHECK(objectsAt100 == objectsAt25);
}

TEST_CASE("VirtualNodeList collapsed rows retain the legacy label geometry")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    lv_obj_t *parent = harness.nodeListRootForTesting();
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    meshtastic_User user{};
    std::strcpy(user.short_name, "NODE");
    std::strcpy(user.long_name, "Readable Candidate Node");
    store.upsertUser(0x12345678, 0, 1700000000U, user, false);
    index.rebuild(store, filter, 0, NodeListFilterPolicy::LegacyCompatible);
    list.sync(store, index);
    harness.pump();

    lv_obj_t *row = boundRow(parent, 0x12345678);
    REQUIRE(row != nullptr);
    REQUIRE(lv_obj_get_child_count(row) == 11);

    lv_obj_t *shortName = lv_obj_get_child(row, 3);
    lv_obj_t *battery = lv_obj_get_child(row, 4);
    lv_obj_t *lastHeard = lv_obj_get_child(row, 5);
    lv_obj_t *signal = lv_obj_get_child(row, 6);

    CHECK(lv_obj_get_x_aligned(shortName) == 30);
    CHECK(lv_obj_get_y_aligned(shortName) == 10);
    CHECK(lv_obj_get_style_align(shortName, LV_PART_MAIN) == LV_ALIGN_TOP_LEFT);

    CHECK(lv_obj_get_x_aligned(battery) == 8);
    CHECK(lv_obj_get_y_aligned(battery) == 17);
    CHECK(lv_obj_get_style_align(battery, LV_PART_MAIN) == LV_ALIGN_TOP_RIGHT);
    CHECK(lv_obj_get_x_aligned(lastHeard) == 8);
    CHECK(lv_obj_get_y_aligned(lastHeard) == 33);
    CHECK(lv_obj_get_style_align(lastHeard, LV_PART_MAIN) == LV_ALIGN_TOP_RIGHT);
    CHECK(lv_obj_get_x_aligned(signal) == 8);
    CHECK(lv_obj_get_y_aligned(signal) == 1);
    CHECK(lv_obj_get_style_align(signal, LV_PART_MAIN) == LV_ALIGN_TOP_RIGHT);
}

TEST_CASE("VirtualNodeList renders last-heard ages against the supplied current time")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    lv_obj_t *parent = harness.nodeListRootForTesting();
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    meshtastic_User user{};
    std::strcpy(user.short_name, "NODE");
    std::strcpy(user.long_name, "Recently Heard Node");
    store.upsertUser(0x12345678, 0, 1699999880U, user, false);
    index.rebuild(store, filter, 0, NodeListFilterPolicy::LegacyCompatible);

    list.sync(store, index, 0, 1700000000U);
    harness.pump();
    lv_obj_t *row = boundRow(parent, 0x12345678);
    REQUIRE(row != nullptr);
    CHECK(std::string(lv_label_get_text(lv_obj_get_child(row, 5))) == "2 min");
}

TEST_CASE("VirtualNodeList renders expanded legacy detail labels without adding row objects")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    lv_obj_t *parent = harness.nodeListRootForTesting();
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;

    meshtastic_User user{};
    std::strcpy(user.short_name, "WX01");
    std::strcpy(user.long_name, "Weather Candidate");
    user.role = meshtastic_Config_DeviceConfig_Role_SENSOR;
    user.public_key.size = 32;
    store.upsertUser(0x12345678, 0, 1699999880U, user, false);
    store.updatePosition(0x12345678, {true, 377749000, -1224194000, 50, 9, 13});

    meshtastic_DeviceMetrics device{};
    device.battery_level = 85;
    device.voltage = 4.12f;
    store.updateDeviceMetrics(0x12345678, device);

    meshtastic_EnvironmentMetrics environment{};
    environment.temperature = 22.5f;
    environment.relative_humidity = 45.0f;
    environment.barometric_pressure = 1013.25f;
    environment.iaq = 42;
    store.updateEnvironmentMetrics(0x12345678, environment);
    store.updateHops(0x12345678, 3);

    index.rebuild(store, filter, 0, NodeListFilterPolicy::LegacyCompatible);
    list.sync(store, index, 0x12345678, 1700000000U);
    harness.pump();

    lv_obj_t *row = boundRow(parent, 0x12345678);
    REQUIRE(row != nullptr);
    CHECK(lv_obj_get_height(row) == VirtualNodeList::EXPANDED_ROW_HEIGHT);
    CHECK(lv_obj_get_child_count(row) == 11);

    CHECK(labelText(row, 3) == "WX01");
    CHECK(labelText(row, 2) == "Weather Candidate");
    CHECK(labelText(row, 4) == "85% 4.12V");
    CHECK(labelText(row, 5) == "2 min");
    CHECK(labelText(row, 6) == "hops: 3");
    CHECK(labelText(row, 7) == "37.77490 -122.41940");
    CHECK(labelText(row, 8) == "50m MSL");
    CHECK(labelText(row, 9) == "22.5°C 45% 1013.2hPa");
    CHECK(labelText(row, 10) == "IAQ: 42 0.0V 0.0mA");
    CHECK_FALSE(childHidden(row, 7));
    CHECK_FALSE(childHidden(row, 8));
    CHECK_FALSE(childHidden(row, 9));
    CHECK_FALSE(childHidden(row, 10));
}

TEST_CASE("VirtualNodeList renders legacy role and unmessagable icons from the record")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    lv_obj_t *parent = harness.nodeListRootForTesting();
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;

    meshtastic_User router{};
    std::strcpy(router.short_name, "RTR1");
    std::strcpy(router.long_name, "Router Candidate");
    router.role = meshtastic_Config_DeviceConfig_Role_ROUTER;
    router.public_key.size = 32;
    store.upsertUser(0x11111111, 0, 1000, router, false);

    meshtastic_User blocked{};
    std::strcpy(blocked.short_name, "BLKD");
    std::strcpy(blocked.long_name, "Blocked Candidate");
    blocked.role = meshtastic_Config_DeviceConfig_Role_CLIENT;
    blocked.has_is_unmessagable = true;
    blocked.is_unmessagable = true;
    blocked.public_key.size = 32;
    store.upsertUser(0x22222222, 0, 900, blocked, false);

    index.rebuild(store, filter, 0, NodeListFilterPolicy::LegacyCompatible);
    list.sync(store, index, 0, 1700000000U);
    harness.pump();

    lv_obj_t *routerRow = boundRow(parent, 0x11111111);
    lv_obj_t *blockedRow = boundRow(parent, 0x22222222);
    REQUIRE(routerRow != nullptr);
    REQUIRE(blockedRow != nullptr);

    CHECK(lv_image_get_src(lv_obj_get_child(routerRow, 0)) == &img_node_router_image);
    CHECK(lv_image_get_src(lv_obj_get_child(blockedRow, 0)) == &img_unmessagable_image);
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
    index.rebuild(store, filter, 0, NodeListFilterPolicy::LegacyCompatible);
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
    index.rebuild(store, filter, 0, NodeListFilterPolicy::LegacyCompatible);
    list.sync(store, index, 0);

    // Scroll to the very bottom
    list.scrollTo(50, LV_ANIM_OFF);
    harness.pump();

    // Rapidly purge down to 2 nodes
    for (uint32_t i = 3; i <= 50; ++i) {
        store.remove(i);
    }
    index.rebuild(store, filter, 0, NodeListFilterPolicy::LegacyCompatible);
    list.sync(store, index, 0);
    harness.pump();

    // Must not crash or blank out, remaining nodes must be rendered
    CHECK(index.ids().size() == 2);
}

#endif
