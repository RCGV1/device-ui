#include "doctest.h"
#include "graphics/common/MeshtasticView.h"
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
    void nodePositionClicked(NodeId id) override { lastPositionClicked = id; }

    NodeId lastClicked = 0;
    NodeId lastLongPressed = 0;
    NodeId lastFocused = 0;
    NodeId lastPositionClicked = 0;
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

MuiRowSnapshot snapshotVirtualRow(lv_obj_t *row)
{
    lv_obj_t *image = lv_obj_get_child(row, 0);
    lv_obj_t *longName = lv_obj_get_child(row, 2);
    lv_obj_t *shortName = lv_obj_get_child(row, 3);
    lv_obj_t *signal = lv_obj_get_child(row, 6);
    return {
        labelText(row, 2),
        labelText(row, 3),
        labelText(row, 4),
        labelText(row, 5),
        labelText(row, 6),
        labelText(row, 7),
        labelText(row, 8),
        labelText(row, 9),
        labelText(row, 10),
        lv_obj_get_y_aligned(shortName),
        lv_color_to_u32(lv_obj_get_style_bg_color(image, LV_PART_MAIN)),
        lv_color_to_u32(lv_obj_get_style_border_color(image, LV_PART_MAIN)),
        lv_color_to_u32(lv_obj_get_style_image_recolor(image, LV_PART_MAIN)),
        lv_obj_get_style_image_recolor_opa(image, LV_PART_MAIN),
        reinterpret_cast<uintptr_t>(lv_image_get_src(image)),
        lv_color_to_u32(lv_obj_get_style_bg_color(row, LV_PART_MAIN)),
        lv_color_to_u32(lv_obj_get_style_border_color(row, LV_PART_MAIN)),
        lv_obj_get_x(image),
        lv_obj_get_y(image),
        lv_obj_get_x(longName),
        lv_obj_get_y(longName),
        lv_obj_get_x(shortName),
        lv_obj_get_x(signal),
        lv_obj_get_y(signal),
    };
}

NodeId visibleLegacyNodeAt(MuiTestHarness &harness, size_t visibleIndex)
{
    lv_obj_t *root = harness.nodeListRootForTesting();
    REQUIRE(root != nullptr);

    size_t seen = 0;
    const uint32_t childCount = lv_obj_get_child_count(root);
    for (uint32_t index = 0; index < childCount; ++index) {
        lv_obj_t *row = lv_obj_get_child(root, static_cast<int32_t>(index));
        if (!row || lv_obj_has_flag(row, LV_OBJ_FLAG_HIDDEN) || lv_obj_get_child_count(row) < 11) {
            continue;
        }
        const NodeId nodeId = static_cast<NodeId>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_obj_get_child(row, 2))));
        if (!nodeId) {
            continue;
        }
        if (seen == visibleIndex) {
            return nodeId;
        }
        ++seen;
    }

    FAIL("visible legacy row not found");
    return 0;
}

NodeId visibleVirtualNodeAt(lv_obj_t *root, size_t visibleIndex)
{
    REQUIRE(root != nullptr);

    size_t seen = 0;
    const uint32_t childCount = lv_obj_get_child_count(root);
    for (uint32_t index = 0; index < childCount; ++index) {
        lv_obj_t *row = lv_obj_get_child(root, static_cast<int32_t>(index));
        if (!row || lv_obj_has_flag(row, LV_OBJ_FLAG_HIDDEN)) {
            continue;
        }
        const NodeId nodeId = static_cast<NodeId>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(row)));
        if (!nodeId) {
            continue;
        }
        if (seen == visibleIndex) {
            return nodeId;
        }
        ++seen;
    }

    FAIL("visible virtual row not found");
    return 0;
}

MuiRowSnapshot syncVirtualSnapshotFromLegacy(MuiTestHarness &legacy, NodeId nodeId, NodeId expanded = 0, NodeId ownNode = 0)
{
    legacy.enableVirtualNodeModelFixture();
    lv_obj_t *virtualRoot = lv_obj_create(lv_screen_active());
    lv_obj_set_size(virtualRoot, 320, 240);
    lv_obj_set_style_layout(virtualRoot, LV_LAYOUT_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);

    MuiRowSnapshot snapshot;
    {
        DummyActionSink sink;
        VirtualNodeList list(virtualRoot, sink);
        VisibleNodeIndex index;
        NodeListFilter filter;
        index.rebuild(legacy.store(), filter, ownNode, NodeListFilterPolicy::LegacyCompatible);

        NodeListRenderContext context;
        context.ownNode = ownNode;
        const NodePosition ownPosition = legacy.nodePosition(ownNode);
        context.hasOwnPosition = ownPosition.known;
        context.ownLatitude = ownPosition.latitude;
        context.ownLongitude = ownPosition.longitude;
        context.metricUnits = true;

        list.sync(legacy.store(), index, expanded, 0, context);
        legacy.pump();

        lv_obj_t *row = boundRow(virtualRoot, nodeId);
        REQUIRE(row != nullptr);
        snapshot = snapshotVirtualRow(row);
    }
    lv_obj_delete(virtualRoot);
    return snapshot;
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

TEST_CASE("VirtualNodeList matches legacy battery clamp and imperial position/weather formatting")
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
    user.public_key.size = 32;
    store.upsertUser(0x12345678, 0, 1699999880U, user, false);
    store.updatePosition(0x12345678, {true, 377749000, -1224194000, 50, 9, 13});

    meshtastic_DeviceMetrics device{};
    device.battery_level = 150;
    device.voltage = 4.12f;
    store.updateDeviceMetrics(0x12345678, device);

    meshtastic_EnvironmentMetrics environment{};
    environment.temperature = 22.5f;
    environment.relative_humidity = 45.0f;
    environment.barometric_pressure = 1013.25f;
    store.updateEnvironmentMetrics(0x12345678, environment);

    index.rebuild(store, filter, 0, NodeListFilterPolicy::LegacyCompatible);
    NodeListRenderContext context;
    context.metricUnits = false;
    list.sync(store, index, 0x12345678, 1700000000U, context);
    harness.pump();

    lv_obj_t *row = boundRow(parent, 0x12345678);
    REQUIRE(row != nullptr);
    CHECK(labelText(row, 4) == "100% 4.12V");
    CHECK(labelText(row, 8) == "164ft MSL");
    CHECK(labelText(row, 9) == "72.5°F 45% 29.9inHg");

    device.battery_level = 0;
    device.voltage = 0.0f;
    store.updateDeviceMetrics(0x12345678, device);
    list.sync(store, index, 0x12345678, 1700000000U, context);
    harness.pump();
    CHECK(labelText(row, 4).empty());
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

TEST_CASE("VirtualNodeList matches legacy event-driven row text and distance presentation")
{
    MuiTestHarness legacy;
    legacy.resetNodeList();
    legacy.enableVirtualNodeModelFixture();
    legacy.setCurrentTime(1700000000U);

    constexpr NodeId ownNode = 0xaaaa0001;
    constexpr NodeId remoteNode = 0x12345678;
    legacy.setOwnNodeFixture(ownNode);
    legacy.addNodeFixture(ownNode, "SELF", "Own Node", 1699999900U, MeshtasticView::client, true, false, 0);
    legacy.updatePositionFixture(ownNode, 377749000, -1224194000, 50, 9, 13);

    legacy.addNodeFixture(remoteNode, "ABCD", "Remote Weather", 1699999880U, MeshtasticView::router, false, false, 2);
    legacy.updatePositionFixture(remoteNode, 377750000, -1224190000, 44, 7, 12);
    legacy.updateMetricsFixture(remoteNode, 85, 4.12f, 12.5f, 3.2f);
    legacy.updateTelemetryFixture(remoteNode, 22.5f, 45.0f, 1013.25f, 42);
    legacy.updateHopsFixture(remoteNode, 3);
    legacy.updateSignalFixture(remoteNode, -70, 5.5f);
    legacy.pump();

    const MuiRowSnapshot legacyRow = legacy.legacyRowSnapshot(remoteNode);
    const MuiRowSnapshot virtualRow = syncVirtualSnapshotFromLegacy(legacy, remoteNode, remoteNode, ownNode);

    CHECK(virtualRow.longName == legacyRow.longName);
    CHECK(virtualRow.shortName == legacyRow.shortName);
    CHECK(virtualRow.shortNameY == legacyRow.shortNameY);
    CHECK(virtualRow.battery == legacyRow.battery);
    CHECK(virtualRow.lastHeard == legacyRow.lastHeard);
    CHECK(virtualRow.signal == legacyRow.signal);
    CHECK(virtualRow.position1 == legacyRow.position1);
    CHECK(virtualRow.position2 == legacyRow.position2);
    CHECK(virtualRow.telemetry1 == legacyRow.telemetry1);
    CHECK(virtualRow.telemetry2 == legacyRow.telemetry2);
}

TEST_CASE("VirtualNodeList matches the same legacy row visual contract at the same viewport position")
{
    MuiTestHarness legacy;
    legacy.resetNodeList();
    legacy.enableVirtualNodeModelFixture();
    legacy.setCurrentTime(1700000000U);

    constexpr NodeId target = 0x10000001U;
    legacy.addNodeFixture(0x10000003U, "LOW3", "Lower Three", 1699999700U, MeshtasticView::client, true, false, 0);
    legacy.addNodeFixture(target, "TOP1", "Top Visual Node", 1699999900U, MeshtasticView::router, false, false, 0);
    legacy.addNodeFixture(0x10000002U, "LOW2", "Lower Two", 1699999800U, MeshtasticView::client, true, false, 0);
    legacy.updateHopsFixture(target, 2);
    legacy.pump();

    lv_obj_t *virtualRoot = lv_obj_create(lv_screen_active());
    lv_obj_set_size(virtualRoot, 320, 240);
    lv_obj_set_style_layout(virtualRoot, LV_LAYOUT_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);

    MuiRowSnapshot legacyRow;
    MuiRowSnapshot virtualRow;
    {
        DummyActionSink sink;
        VirtualNodeList list(virtualRoot, sink);
        VisibleNodeIndex index;
        NodeListFilter filter;
        index.rebuild(legacy.store(), filter, 0, NodeListFilterPolicy::LegacyCompatible);
        list.sync(legacy.store(), index, 0, 1700000000U);
        legacy.pump();

        CHECK(visibleLegacyNodeAt(legacy, 0) == target);
        CHECK(visibleVirtualNodeAt(virtualRoot, 0) == target);

        lv_obj_t *row = boundRow(virtualRoot, target);
        REQUIRE(row != nullptr);
        legacyRow = legacy.legacyRowSnapshot(target);
        virtualRow = snapshotVirtualRow(row);
    }
    lv_obj_delete(virtualRoot);

    CHECK(virtualRow.longName == legacyRow.longName);
    CHECK(virtualRow.shortName == legacyRow.shortName);
    CHECK(virtualRow.signal == legacyRow.signal);
    CHECK(virtualRow.imageSrc == legacyRow.imageSrc);
    CHECK(virtualRow.rowBg == legacyRow.rowBg);
    CHECK(virtualRow.rowBorder == legacyRow.rowBorder);
    CHECK(virtualRow.imageBg == legacyRow.imageBg);
    CHECK(virtualRow.imageBorder == legacyRow.imageBorder);
    CHECK(virtualRow.imageRecolor == legacyRow.imageRecolor);
    CHECK(virtualRow.imageRecolorOpa == legacyRow.imageRecolorOpa);
    CHECK(virtualRow.imageX == legacyRow.imageX);
    CHECK(virtualRow.imageY == legacyRow.imageY);
    CHECK(virtualRow.longNameX == legacyRow.longNameX);
    CHECK(virtualRow.longNameY == legacyRow.longNameY);
    CHECK(virtualRow.shortNameX == legacyRow.shortNameX);
    CHECK(virtualRow.shortNameY == legacyRow.shortNameY);
    CHECK(virtualRow.signalX == legacyRow.signalX);
    CHECK(virtualRow.signalY == legacyRow.signalY);
}

TEST_CASE("VirtualNodeList matches legacy signal label event order")
{
    {
        MuiTestHarness signalThenHops;
        signalThenHops.resetNodeList();
        signalThenHops.enableVirtualNodeModelFixture();
        signalThenHops.setCurrentTime(1700000000U);
        signalThenHops.addNodeFixture(0x11111111, "SIG1", "Signal Then Hops", 1699999900U);
        signalThenHops.updateSignalFixture(0x11111111, -80, 4.2f);
        signalThenHops.updateHopsFixture(0x11111111, 2);

        CHECK(syncVirtualSnapshotFromLegacy(signalThenHops, 0x11111111).signal ==
              signalThenHops.legacyRowSnapshot(0x11111111).signal);
    }

    {
        MuiTestHarness hopsThenSignal;
        hopsThenSignal.resetNodeList();
        hopsThenSignal.enableVirtualNodeModelFixture();
        hopsThenSignal.setCurrentTime(1700000000U);
        hopsThenSignal.addNodeFixture(0x22222222, "SIG2", "Hops Then Signal", 1699999900U);
        hopsThenSignal.updateHopsFixture(0x22222222, 4);
        hopsThenSignal.updateSignalFixture(0x22222222, -71, 5.5f);

        CHECK(syncVirtualSnapshotFromLegacy(hopsThenSignal, 0x22222222).signal ==
              hopsThenSignal.legacyRowSnapshot(0x22222222).signal);
        CHECK(hopsThenSignal.nodeHops(0x22222222) == 0);
    }
}

TEST_CASE("VirtualNodeList matches legacy icon style and short-name fallback after node events")
{
    MuiTestHarness legacy;
    legacy.resetNodeList();
    legacy.enableVirtualNodeModelFixture();
    legacy.setCurrentTime(1700000000U);

    constexpr NodeId noKeyRouter = 0x11112222;
    constexpr NodeId blocked = 0x22223333;
    constexpr NodeId blankShort = 0x1234abcd;
    constexpr NodeId blockedNoKey = 0x33334444;
    constexpr NodeId darkNormal = 0x00000001;
    constexpr NodeId brightNormal = 0x00fefefe;
    constexpr NodeId nonPrintableShort = 0x4444beef;

    legacy.addNodeFixture(noKeyRouter, "RTR1", "Router Without Key", 1699999900U, MeshtasticView::router, false, false, 0);
    legacy.addNodeFixture(blocked, "BLKD", "Blocked Node", 1699999890U, MeshtasticView::client, true, true, 0);
    legacy.addNodeFixture(blankShort, "", "Blank Short", 1699999880U, MeshtasticView::client, true, false, 0);
    legacy.addNodeFixture(blockedNoKey, "BNK0", "Blocked Node Without Key", 1699999870U, MeshtasticView::client, false, true, 0);
    legacy.addNodeFixture(darkNormal, "DRK1", "Dark Normal Node", 1699999860U, MeshtasticView::client, true, false, 0);
    legacy.addNodeFixture(brightNormal, "BRT1", "Bright Normal Node", 1699999850U, MeshtasticView::client, true, false, 0);
    legacy.addNodeFixture(nonPrintableShort, "\x01\x02\x03\x04", "Non Printable Short", 1699999840U, MeshtasticView::client, true,
                          false, 0);
    legacy.pump();

    const MuiRowSnapshot legacyRouter = legacy.legacyRowSnapshot(noKeyRouter);
    const MuiRowSnapshot virtualRouter = syncVirtualSnapshotFromLegacy(legacy, noKeyRouter);
    CHECK(virtualRouter.imageBg == legacyRouter.imageBg);
    CHECK(virtualRouter.imageBorder == legacyRouter.imageBorder);
    CHECK(virtualRouter.imageRecolor == legacyRouter.imageRecolor);
    CHECK(virtualRouter.imageRecolorOpa == legacyRouter.imageRecolorOpa);

    const MuiRowSnapshot legacyBlocked = legacy.legacyRowSnapshot(blocked);
    const MuiRowSnapshot virtualBlocked = syncVirtualSnapshotFromLegacy(legacy, blocked);
    CHECK(virtualBlocked.imageBg == legacyBlocked.imageBg);
    CHECK(virtualBlocked.imageBorder == legacyBlocked.imageBorder);
    CHECK(virtualBlocked.imageRecolor == legacyBlocked.imageRecolor);
    CHECK(virtualBlocked.imageRecolorOpa == legacyBlocked.imageRecolorOpa);

    const MuiRowSnapshot legacyBlockedNoKey = legacy.legacyRowSnapshot(blockedNoKey);
    const MuiRowSnapshot virtualBlockedNoKey = syncVirtualSnapshotFromLegacy(legacy, blockedNoKey);
    CHECK(virtualBlockedNoKey.imageBg == legacyBlockedNoKey.imageBg);
    CHECK(virtualBlockedNoKey.imageBorder == legacyBlockedNoKey.imageBorder);
    CHECK(virtualBlockedNoKey.imageRecolor == legacyBlockedNoKey.imageRecolor);
    CHECK(virtualBlockedNoKey.imageRecolorOpa == legacyBlockedNoKey.imageRecolorOpa);

    const MuiRowSnapshot legacyDarkNormal = legacy.legacyRowSnapshot(darkNormal);
    const MuiRowSnapshot virtualDarkNormal = syncVirtualSnapshotFromLegacy(legacy, darkNormal);
    CHECK(virtualDarkNormal.imageBg == legacyDarkNormal.imageBg);
    CHECK(virtualDarkNormal.imageBorder == legacyDarkNormal.imageBorder);
    CHECK(virtualDarkNormal.imageRecolor == legacyDarkNormal.imageRecolor);
    CHECK(virtualDarkNormal.imageRecolorOpa == legacyDarkNormal.imageRecolorOpa);

    const MuiRowSnapshot legacyBrightNormal = legacy.legacyRowSnapshot(brightNormal);
    const MuiRowSnapshot virtualBrightNormal = syncVirtualSnapshotFromLegacy(legacy, brightNormal);
    CHECK(virtualBrightNormal.imageBg == legacyBrightNormal.imageBg);
    CHECK(virtualBrightNormal.imageBorder == legacyBrightNormal.imageBorder);
    CHECK(virtualBrightNormal.imageRecolor == legacyBrightNormal.imageRecolor);
    CHECK(virtualBrightNormal.imageRecolorOpa == legacyBrightNormal.imageRecolorOpa);

    CHECK(syncVirtualSnapshotFromLegacy(legacy, blankShort).shortName == legacy.legacyRowSnapshot(blankShort).shortName);
    CHECK(syncVirtualSnapshotFromLegacy(legacy, nonPrintableShort).shortName ==
          legacy.legacyRowSnapshot(nonPrintableShort).shortName);
}

TEST_CASE("VirtualNodeList resets pooled image recolor when a row is rebound after unmessagable")
{
    MuiTestHarness legacy;
    legacy.resetNodeList();
    legacy.enableVirtualNodeModelFixture();

    lv_obj_t *virtualRoot = lv_obj_create(lv_screen_active());
    lv_obj_set_size(virtualRoot, 320, 240);
    lv_obj_set_style_layout(virtualRoot, LV_LAYOUT_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);

    DummyActionSink sink;
    MuiRowSnapshot reusedDarkNormal;
    MuiRowSnapshot freshDarkNormal;
    MuiRowSnapshot reusedBrightRouter;
    MuiRowSnapshot freshBrightRouter;
    {
        VirtualNodeList list(virtualRoot, sink);
        VisibleNodeIndex index;
        NodeListFilter filter;
        lv_obj_t *pooledRow = nullptr;

        const auto syncCurrentLegacy = [&](NodeId id) {
            index.rebuild(legacy.store(), filter, 0, NodeListFilterPolicy::LegacyCompatible);
            list.sync(legacy.store(), index);
            legacy.pump();
            lv_obj_t *row = boundRow(virtualRoot, id);
            REQUIRE(row != nullptr);
            return row;
        };

        constexpr NodeId blockedFirst = 0x5555a001;
        legacy.addNodeFixture(blockedFirst, "BLK1", "Blocked Prime", 1699999900U, MeshtasticView::client, true, true, 0);
        pooledRow = syncCurrentLegacy(blockedFirst);
        CHECK(snapshotVirtualRow(pooledRow).imageRecolor == 0xffff5555U);

        constexpr NodeId darkNormal = 0x00000001;
        legacy.resetNodeList();
        legacy.enableVirtualNodeModelFixture();
        legacy.addNodeFixture(darkNormal, "DRK1", "Fresh Dark Normal", 1699999900U, MeshtasticView::client, true, false, 0);
        freshDarkNormal = legacy.legacyRowSnapshot(darkNormal);
        lv_obj_t *darkRow = syncCurrentLegacy(darkNormal);
        CHECK(darkRow == pooledRow);
        reusedDarkNormal = snapshotVirtualRow(darkRow);

        constexpr NodeId blockedSecond = 0x5555a002;
        legacy.resetNodeList();
        legacy.enableVirtualNodeModelFixture();
        legacy.addNodeFixture(blockedSecond, "BLK2", "Blocked Prime Again", 1699999900U, MeshtasticView::client, true, true, 0);
        lv_obj_t *blockedRow = syncCurrentLegacy(blockedSecond);
        CHECK(blockedRow == pooledRow);
        CHECK(snapshotVirtualRow(blockedRow).imageRecolor == 0xffff5555U);

        constexpr NodeId brightRouter = 0x00fefefe;
        legacy.resetNodeList();
        legacy.enableVirtualNodeModelFixture();
        legacy.addNodeFixture(brightRouter, "BRT1", "Fresh Bright Router", 1699999900U, MeshtasticView::router, true, false, 0);
        freshBrightRouter = legacy.legacyRowSnapshot(brightRouter);
        lv_obj_t *brightRow = syncCurrentLegacy(brightRouter);
        CHECK(brightRow == pooledRow);
        reusedBrightRouter = snapshotVirtualRow(brightRow);
    }
    lv_obj_delete(virtualRoot);

    CHECK(reusedDarkNormal.imageBg == freshDarkNormal.imageBg);
    CHECK(reusedDarkNormal.imageBorder == freshDarkNormal.imageBorder);
    CHECK(reusedDarkNormal.imageRecolor == freshDarkNormal.imageRecolor);
    CHECK(reusedDarkNormal.imageRecolorOpa == freshDarkNormal.imageRecolorOpa);

    CHECK(reusedBrightRouter.imageBg == freshBrightRouter.imageBg);
    CHECK(reusedBrightRouter.imageBorder == freshBrightRouter.imageBorder);
    CHECK(reusedBrightRouter.imageRecolor == freshBrightRouter.imageRecolor);
    CHECK(reusedBrightRouter.imageRecolorOpa == freshBrightRouter.imageRecolorOpa);
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
