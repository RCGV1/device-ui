#include "doctest.h"
#include "graphics/common/MeshtasticView.h"
#include "graphics/common/NodeListRowPresentation.h"
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
    void nodeFocusBoundary(bool forward) override
    {
        boundaryCalled = true;
        boundaryForward = forward;
    }
    void nodePositionClicked(NodeId id) override { lastPositionClicked = id; }

    NodeId lastClicked = 0;
    NodeId lastLongPressed = 0;
    NodeId lastFocused = 0;
    NodeId lastPositionClicked = 0;
    bool boundaryCalled = false;
    bool boundaryForward = false;
};

namespace
{
class StandaloneListParent
{
  public:
    explicit StandaloneListParent(MuiTestHarness &harness)
    {
        lv_obj_t *productionRoot = harness.nodeListRootForTesting();
        parent = lv_obj_create(lv_obj_get_parent(productionRoot));
        lv_obj_set_size(parent, lv_obj_get_width(productionRoot), lv_obj_get_height(productionRoot));
    }

    ~StandaloneListParent()
    {
        if (parent) {
            lv_obj_delete(parent);
        }
    }

    StandaloneListParent(const StandaloneListParent &) = delete;
    StandaloneListParent &operator=(const StandaloneListParent &) = delete;

    operator lv_obj_t *() const { return parent; }

  private:
    lv_obj_t *parent = nullptr;
};

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

lv_obj_t *virtualSpacer(lv_obj_t *parent)
{
    REQUIRE(parent != nullptr);
    const uint32_t childCount = lv_obj_get_child_count(parent);
    for (uint32_t index = 0; index < childCount; ++index) {
        lv_obj_t *child = lv_obj_get_child(parent, static_cast<int32_t>(index));
        if (child && lv_obj_get_child_count(child) == 0 && lv_obj_get_width(child) == 1) {
            return child;
        }
    }
    FAIL("virtual spacer not found");
    return nullptr;
}

constexpr uint32_t kHighlightMeshRowBorder = 0xff67ea94U;

MuiRowSnapshot snapshotVirtualRow(lv_obj_t *row)
{
    return snapshotMuiRow(row);
}

} // namespace

TEST_CASE("VirtualNodeList row pool remains strictly bounded")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
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

    // The 100-node candidate required by the compatibility plan must be structurally identical too.
    for (uint32_t i = 101; i <= 250; ++i) {
        store.remove(i);
    }
    index.rebuild(store, filter, 0);
    list.sync(store, index, 0);
    harness.pump();

    const size_t objectsAt100 = harness.nodeListObjectCount();
    CHECK(objectsAt100 == objectsAt25);
}

TEST_CASE("VirtualNodeList collapsed rows keep the retained-row label geometry")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    meshtastic_User user{};
    std::strcpy(user.short_name, "NODE");
    std::strcpy(user.long_name, "Readable Candidate Node");
    store.upsertUser(0x12345678, 0, 1700000000U, user, false);
    index.rebuild(store, filter, 0);
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

TEST_CASE("VirtualNodeList bounds long node names without marquee animations")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    meshtastic_User user{};
    std::strcpy(user.short_name, "NODE");
    std::strcpy(user.long_name, "A node name that exceeds the visible row width");
    store.upsertUser(0x12345678, 0, 1700000000U, user, false);
    store.updatePosition(0x12345678, {true, 377749000, -1224194000, 50, 9, 13});
    index.rebuild(store, filter, 0);
    list.sync(store, index);
    harness.pump();

    lv_obj_t *row = boundRow(parent, 0x12345678);
    REQUIRE(row != nullptr);
    lv_obj_t *longName = lv_obj_get_child(row, 2);
    REQUIRE(longName != nullptr);
    CHECK(lv_label_get_long_mode(longName) == LV_LABEL_LONG_DOT);
    lv_obj_t *positionDetail = lv_obj_get_child(row, 8);
    REQUIRE(positionDetail != nullptr);
    CHECK(lv_label_get_long_mode(positionDetail) == LV_LABEL_LONG_DOT);
}

TEST_CASE("VirtualNodeList renders last-heard ages against the supplied current time")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    meshtastic_User user{};
    std::strcpy(user.short_name, "NODE");
    std::strcpy(user.long_name, "Recently Heard Node");
    store.upsertUser(0x12345678, 0, 1699999880U, user, false);
    index.rebuild(store, filter, 0);

    list.sync(store, index, 0, 1700000000U);
    harness.pump();
    lv_obj_t *row = boundRow(parent, 0x12345678);
    REQUIRE(row != nullptr);
    CHECK(std::string(lv_label_get_text(lv_obj_get_child(row, 5))) == "2 min");
}

TEST_CASE("VirtualNodeList refreshes last-heard labels at node-relative minute boundaries")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    meshtastic_User user{};
    std::strcpy(user.short_name, "NODE");
    std::strcpy(user.long_name, "Boundary Node");
    store.upsertUser(0x12345678, 0, 100U, user, false);
    index.rebuild(store, filter, 0);

    list.sync(store, index, 0, 159U);
    harness.pump();
    lv_obj_t *row = boundRow(parent, 0x12345678);
    REQUIRE(row != nullptr);
    REQUIRE(labelText(row, 5) == "now");
    const uint32_t bindsBeforeBoundary = list.bindGenerationForTesting();

    list.sync(store, index, 0, 160U);
    harness.pump();

    CHECK(list.bindGenerationForTesting() > bindsBeforeBoundary);
    CHECK(labelText(row, 5) == "1 min");
}

TEST_CASE("VirtualNodeList renders expanded panel-backed detail labels without adding row objects")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
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

    index.rebuild(store, filter, 0);
    list.sync(store, index, 0x12345678, 1700000000U);
    list.finishExpansionForTesting();
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

TEST_CASE("VirtualNodeList matches the retained-row battery clamp and imperial position/weather formatting")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
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

    index.rebuild(store, filter, 0);
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
    CHECK(labelText(row, 4) == "100% 4.12V");
}

TEST_CASE("VirtualNodeList renders retained role and unmessagable icons from the record")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
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

    index.rebuild(store, filter, 0);
    list.sync(store, index, 0, 1700000000U);
    harness.pump();

    lv_obj_t *routerRow = boundRow(parent, 0x11111111);
    lv_obj_t *blockedRow = boundRow(parent, 0x22222222);
    REQUIRE(routerRow != nullptr);
    REQUIRE(blockedRow != nullptr);

    CHECK(lv_image_get_src(lv_obj_get_child(routerRow, 0)) == &img_node_router_image);
    CHECK(lv_image_get_src(lv_obj_get_child(blockedRow, 0)) == &img_unmessagable_image);
}

TEST_CASE("VirtualNodeList keeps short-name label stable across identical rebinds")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.setCurrentTime(1700000000U);

    constexpr NodeId ownNode = 0xaaaa0001;
    constexpr NodeId remoteNode = 0x12345678;
    harness.setOwnNodeFixture(ownNode);
    harness.addNodeFixture(ownNode, "OWN", "Own Node", 1699999800U);
    harness.updatePositionFixture(ownNode, 377749000, -1224194000, 0, 0, 0);
    harness.addNodeFixture(remoteNode, "ABCD", "Remote Node", 1699999900U);
    harness.updatePositionFixture(remoteNode, 377750000, -1224190000, 0, 0, 0);
    harness.showNodesScreen();

    auto snapshotFor = [&](NodeId id) {
        lv_obj_t *row = boundRow(harness.nodeListRootForTesting(), id);
        REQUIRE(row != nullptr);
        return snapshotVirtualRow(row);
    };
    const MuiRowSnapshot first = snapshotFor(remoteNode);
    REQUIRE(first.shortNameFont == reinterpret_cast<uintptr_t>(&ui_font_montserrat_14));
    // Second sync with identical store/context must keep the rendered short text and offset stable.
    harness.updatePositionFixture(remoteNode, 377750000, -1224190000, 0, 0, 0);
    const MuiRowSnapshot second = snapshotFor(remoteNode);
    CHECK(second.shortName == first.shortName);
    CHECK(second.shortNameY == first.shortNameY);
    CHECK(second.position1 == first.position1);
}

TEST_CASE("VirtualNodeList invalidates short-name distance when own node changes")
{
    MuiTestHarness harness;
    harness.resetNodeList();
    harness.setCurrentTime(1700000000U);

    constexpr NodeId originalOwnNode = 0xaaaa0001;
    constexpr NodeId newOwnNode = 0x12345678;
    harness.setOwnNodeFixture(originalOwnNode);
    harness.addNodeFixture(originalOwnNode, "OWN", "Original Own", 1699999800U);
    harness.updatePositionFixture(originalOwnNode, 377749000, -1224194000, 0, 0, 0);
    harness.addNodeFixture(newOwnNode, "ABCD", "New Own", 1699999900U);
    harness.updatePositionFixture(newOwnNode, 377750000, -1224190000, 0, 0, 0);
    harness.showNodesScreen();

    lv_obj_t *row = boundRow(harness.nodeListRootForTesting(), newOwnNode);
    REQUIRE(row != nullptr);
    CHECK(std::strchr(snapshotVirtualRow(row).shortName.c_str(), '\n') != nullptr);

    harness.setOwnNodeFixture(newOwnNode);
    harness.updateLastHeardFixture(newOwnNode);
    row = boundRow(harness.nodeListRootForTesting(), newOwnNode);
    REQUIRE(row != nullptr);
    CHECK(std::strchr(snapshotVirtualRow(row).shortName.c_str(), '\n') == nullptr);
}

TEST_CASE("VirtualNodeList short-name distance formatting clears stale pooled bytes")
{
    char shortText[32];
    std::memset(shortText, 'X', sizeof(shortText));
    shortText[sizeof(shortText) - 1] = '\0';

    NodeListRowPresentation::formatShortNameWithDistance(shortText, sizeof(shortText), "AB", 0x12345678, true, 377749000,
                                                         -1224194000, 377749500, -1224194000, true);

    CHECK(std::string(shortText).substr(0, 5) == "AB  \n");
}

TEST_CASE("VirtualNodeList position formatting accepts extreme protocol coordinates")
{
    char shortText[32];
    char positionText[48];
    char altitudeText[24];

    NodeListRowPresentation::formatShortNameWithDistance(shortText, sizeof(shortText), "AB", 0x12345678, true, 1700000000,
                                                         -1700000000, -1700000000, 1700000000, true);
    NodeListRowPresentation::formatPositionLines(0, 0, INT32_MIN, true, positionText, sizeof(positionText), altitudeText,
                                                 sizeof(altitudeText));

    CHECK(std::strchr(shortText, '\n') != nullptr);
    CHECK(std::string(altitudeText) == "0m MSL");
}

TEST_CASE("VirtualNodeList short-name formatting clears stale pooled bytes without a distance line")
{
    char shortText[32];
    std::memset(shortText, 'X', sizeof(shortText));
    shortText[sizeof(shortText) - 1] = '\0';

    NodeListRowPresentation::formatShortDisplayName(shortText, sizeof(shortText), "AB", 0x12345678);

    CHECK(shortText[0] == 'A');
    CHECK(shortText[1] == 'B');
    CHECK(shortText[2] == '\0');
    CHECK(shortText[3] == '\0');
}

TEST_CASE("VirtualNodeList expansion and stable selection")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
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

TEST_CASE("VirtualNodeList expanded row geometry is exact at list boundaries")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    for (uint32_t i = 1; i <= 5; ++i) {
        meshtastic_User user{};
        std::snprintf(user.short_name, sizeof(user.short_name), "N%03u", i);
        store.upsertUser(i, 0, 1000 + (6 - i), user, false);
    }
    index.rebuild(store, filter, 0);

    CHECK(VirtualNodeList::COLLAPSED_ROW_HEIGHT == 53);
    CHECK(VirtualNodeList::EXPANDED_ROW_HEIGHT == 83);
    CHECK(VirtualNodeList::ROW_GAP == 5);

    constexpr int32_t stride = 53 + 5;
    constexpr int32_t delta = 83 - 53;

    SUBCASE("expanded at index 0")
    {
        list.sync(store, index, 1, 1700000000U);
        list.finishExpansionForTesting();
        harness.pump();

        CHECK(list.rowHeightForTesting(1) == VirtualNodeList::EXPANDED_ROW_HEIGHT);
        CHECK(list.rowYForTesting(0) == 0);
        CHECK(list.rowYForTesting(1) == stride + delta);
        CHECK(list.rowYForTesting(5) == 5 * stride - VirtualNodeList::ROW_GAP + delta);
        CHECK(lv_obj_get_y(boundRow(parent, 1)) == 0);
        CHECK(lv_obj_get_height(boundRow(parent, 1)) == 83);
        CHECK(lv_obj_get_y(boundRow(parent, 2)) == 88);
        CHECK(lv_obj_get_height(virtualSpacer(parent)) == 315);
    }

    SUBCASE("expanded at middle index")
    {
        list.sync(store, index, 3, 1700000000U);
        list.finishExpansionForTesting();
        harness.pump();

        CHECK(list.rowHeightForTesting(3) == VirtualNodeList::EXPANDED_ROW_HEIGHT);
        CHECK(list.rowYForTesting(2) == 2 * stride);
        CHECK(list.rowYForTesting(3) == 3 * stride + delta);
        CHECK(list.rowYForTesting(5) == 5 * stride - VirtualNodeList::ROW_GAP + delta);
        CHECK(lv_obj_get_y(boundRow(parent, 3)) == 116);
        CHECK(lv_obj_get_height(boundRow(parent, 3)) == 83);
        CHECK(lv_obj_get_y(boundRow(parent, 4)) == 204);
        CHECK(lv_obj_get_height(virtualSpacer(parent)) == 315);
    }

    SUBCASE("expanded at final index")
    {
        list.sync(store, index, 5, 1700000000U);
        list.finishExpansionForTesting();
        harness.pump();

        CHECK(list.rowHeightForTesting(5) == VirtualNodeList::EXPANDED_ROW_HEIGHT);
        CHECK(list.rowYForTesting(4) == 4 * stride);
        CHECK(list.rowYForTesting(5) == 5 * stride - VirtualNodeList::ROW_GAP + delta);
        CHECK(lv_obj_get_y(boundRow(parent, 5)) == 232);
        CHECK(lv_obj_get_height(boundRow(parent, 5)) == 83);
        CHECK(lv_obj_get_height(virtualSpacer(parent)) == 315);
    }
}

TEST_CASE("VirtualNodeList expansion animation geometry stays linear")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    for (uint32_t i = 1; i <= 5; ++i) {
        meshtastic_User user{};
        std::snprintf(user.short_name, sizeof(user.short_name), "N%03u", i);
        store.upsertUser(i, 0, 1000 + (6 - i), user, false);
    }
    index.rebuild(store, filter, 0);

    constexpr int32_t stride = 53 + 5;
    constexpr int32_t delta = 83 - 53;

    list.sync(store, index, 2, 1700000000U);
    list.setExpansionProgressForTesting(0);
    CHECK(list.rowHeightForTesting(2) == VirtualNodeList::COLLAPSED_ROW_HEIGHT);
    CHECK(list.rowYForTesting(3) == 3 * stride);

    list.setExpansionProgressForTesting(50);
    CHECK(list.rowHeightForTesting(2) == VirtualNodeList::COLLAPSED_ROW_HEIGHT + delta / 2);
    CHECK(list.rowYForTesting(3) == 3 * stride + delta / 2);

    list.setExpansionProgressForTesting(100);
    CHECK(list.rowHeightForTesting(2) == VirtualNodeList::EXPANDED_ROW_HEIGHT);
    CHECK(list.rowYForTesting(3) == 3 * stride + delta);
    list.finishExpansionForTesting();
    harness.pump();
    CHECK(lv_obj_get_height(boundRow(parent, 2)) == 83);
    CHECK(lv_obj_get_y(boundRow(parent, 3)) == 146);
    CHECK(lv_obj_get_height(virtualSpacer(parent)) == 315);

    list.setExpanded(4);
    list.setExpansionProgressForTesting(50);
    CHECK(list.rowHeightForTesting(2) == VirtualNodeList::EXPANDED_ROW_HEIGHT - delta / 2);
    CHECK(list.rowHeightForTesting(4) == VirtualNodeList::COLLAPSED_ROW_HEIGHT + delta / 2);
    CHECK(list.rowYForTesting(5) == 5 * stride - VirtualNodeList::ROW_GAP + delta);
}

TEST_CASE("VirtualNodeList applies expanded sync requests after the active animation finishes")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    for (uint32_t i = 1; i <= 5; ++i) {
        meshtastic_User user{};
        std::snprintf(user.short_name, sizeof(user.short_name), "N%03u", i);
        store.upsertUser(i, 0, 1000 + (6 - i), user, false);
    }
    index.rebuild(store, filter, 0);

    SUBCASE("collapse request is replayed")
    {
        list.sync(store, index, 2, 1700000000U);
        list.setExpansionProgressForTesting(50);
        list.sync(store, index, 0, 1700000000U);
        harness.pump();
        CHECK(list.getExpanded() == 2);

        list.finishExpansionForTesting();
        harness.pump();
        CHECK(list.getExpanded() == 0);
        CHECK(list.expansionAnimating());
        CHECK(lv_obj_get_height(boundRow(parent, 2)) == 83);

        list.setExpansionProgressForTesting(100);
        list.finishExpansionForTesting();
        harness.pump();
        CHECK_FALSE(list.expansionAnimating());
        CHECK(lv_obj_get_height(boundRow(parent, 2)) == 53);
        CHECK(lv_obj_get_height(virtualSpacer(parent)) == 285);
    }

    SUBCASE("reselection request is replayed after reorder")
    {
        list.sync(store, index, 2, 1700000000U);
        list.setExpansionProgressForTesting(50);

        for (uint32_t i = 1; i <= 5; ++i) {
            store.updateLastHeard(i, 2000 + i);
        }
        index.rebuild(store, filter, 0);
        list.sync(store, index, 4, 1700000000U);
        harness.pump();

        CHECK(list.getExpanded() == 2);
        CHECK(lv_obj_get_y(boundRow(parent, 2)) == 174);
        CHECK(list.rowHeightForTesting(2) == 68);
        CHECK(lv_obj_get_height(virtualSpacer(parent)) == 300);

        list.finishExpansionForTesting();
        harness.pump();
        CHECK(list.getExpanded() == 4);
        CHECK(list.expansionAnimating());

        list.setExpansionProgressForTesting(100);
        list.finishExpansionForTesting();
        harness.pump();
        CHECK(lv_obj_get_y(boundRow(parent, 4)) == 58);
        CHECK(lv_obj_get_height(boundRow(parent, 4)) == 83);
        CHECK(lv_obj_get_height(virtualSpacer(parent)) == 315);
    }

    SUBCASE("removed expanded node drops its cached geometry")
    {
        list.sync(store, index, 2, 1700000000U);
        list.setExpansionProgressForTesting(50);

        store.remove(2);
        index.rebuild(store, filter, 0);
        list.sync(store, index, 4, 1700000000U);
        harness.pump();

        CHECK(boundRow(parent, 2) == nullptr);
        CHECK(lv_obj_get_y(boundRow(parent, 3)) == 58);
        CHECK(lv_obj_get_height(virtualSpacer(parent)) == 227);

        list.finishExpansionForTesting();
        harness.pump();
        CHECK(list.getExpanded() == 4);
        list.setExpansionProgressForTesting(100);
        list.finishExpansionForTesting();
        harness.pump();
        CHECK(lv_obj_get_height(boundRow(parent, 4)) == 83);
        CHECK(lv_obj_get_height(virtualSpacer(parent)) == 257);
    }

    SUBCASE("latest request for the current expanding row clears stale pending")
    {
        list.sync(store, index, 2, 1700000000U);
        list.setExpansionProgressForTesting(50);
        list.sync(store, index, 0, 1700000000U);
        list.sync(store, index, 4, 1700000000U);
        list.sync(store, index, 2, 1700000000U);
        harness.pump();

        CHECK(list.getExpanded() == 2);

        list.finishExpansionForTesting();
        harness.pump();
        CHECK(list.getExpanded() == 2);
        CHECK_FALSE(list.expansionAnimating());
        CHECK(lv_obj_get_height(boundRow(parent, 2)) == 83);
        CHECK(lv_obj_get_height(virtualSpacer(parent)) == 315);
    }
}

TEST_CASE("VirtualNodeList scrolling expanded rows does not repeat expanded index scans")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    for (uint32_t i = 1; i <= 80; ++i) {
        meshtastic_User user{};
        std::snprintf(user.short_name, sizeof(user.short_name), "N%03u", i);
        store.upsertUser(i, 0, 1000 + (81 - i), user, false);
    }
    index.rebuild(store, filter, 0);
    list.sync(store, index, 40, 1700000000U);
    list.setExpansionProgressForTesting(50);

    const uint32_t scansAfterSync = list.expandedIndexScanCountForTesting();
    CHECK(list.rowYForTesting(70) == 70 * 58 + 15);
    CHECK(list.rowYForTesting(80) == 80 * 58 - 5 + 15);
    CHECK(list.expandedIndexScanCountForTesting() == scansAfterSync);

    list.scrollTo(70, LV_ANIM_OFF);
    harness.pump();
    list.refreshVisibleRows(true, false);

    CHECK(list.expandedIndexScanCountForTesting() == scansAfterSync);
}

TEST_CASE("VirtualNodeList sync skips redundant visible-row rebinding")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    for (uint32_t i = 1; i <= 30; ++i) {
        meshtastic_User user{};
        std::snprintf(user.short_name, sizeof(user.short_name), "N%03u", i);
        store.upsertUser(i, 0, 1000 + i, user, false);
    }
    index.rebuild(store, filter, 0);

    list.sync(store, index, 0, 1700000000U);
    const uint32_t bindsAfterInitialSync = list.bindGenerationForTesting();

    list.sync(store, index, 0, 1700000000U);

    CHECK(list.bindGenerationForTesting() == bindsAfterInitialSync);
}

TEST_CASE("VirtualNodeList keeps the visible anchor stable when a node arrives above it")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    for (uint32_t i = 1; i <= 30; ++i) {
        meshtastic_User user{};
        std::snprintf(user.short_name, sizeof(user.short_name), "N%03u", i);
        store.upsertUser(i, 0, 1000 + i, user, false);
    }
    index.rebuild(store, filter, 0);
    list.sync(store, index, 0, 1700000000U);

    constexpr NodeId anchorId = 20;
    const size_t oldAnchorIndex = index.indexOf(anchorId).value();
    const int32_t oldAnchorY = list.rowYForTesting(oldAnchorIndex);
    lv_obj_scroll_to_y(parent, oldAnchorY + 7, LV_ANIM_OFF);
    harness.pump();
    const int32_t anchorOffset = lv_obj_get_scroll_y(parent) - oldAnchorY;

    meshtastic_User incoming{};
    std::strcpy(incoming.short_name, "NEW");
    store.upsertUser(99, 0, 5000, incoming, false);
    index.rebuild(store, filter, 0);
    list.sync(store, index, 0, 1700000000U);
    harness.pump();

    const int32_t expectedScrollY = list.rowYForTesting(index.indexOf(anchorId).value()) + anchorOffset;
    CHECK(lv_obj_get_scroll_y(parent) == expectedScrollY);
}

TEST_CASE("VirtualNodeList does not rebind unchanged visible rows for offscreen node arrival")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    for (uint32_t i = 1; i <= 30; ++i) {
        meshtastic_User user{};
        std::snprintf(user.short_name, sizeof(user.short_name), "N%03u", i);
        store.upsertUser(i, 0, 1000 + i, user, false);
    }
    index.rebuild(store, filter, 0);
    list.sync(store, index, 0, 1700000000U);
    list.scrollTo(20, LV_ANIM_OFF);
    harness.pump();
    const uint32_t bindsBeforeArrival = list.bindGenerationForTesting();

    meshtastic_User incoming{};
    std::strcpy(incoming.short_name, "NEW");
    store.upsertUser(99, 0, 5000, incoming, false);
    index.rebuild(store, filter, 0);
    list.sync(store, index, 0, 1700000000U);
    harness.pump();

    CHECK(list.bindGenerationForTesting() == bindsBeforeArrival);
}

TEST_CASE("VirtualNodeList rebinds only the newly visible row when the render window advances by one row")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    for (uint32_t i = 1; i <= 30; ++i) {
        meshtastic_User user{};
        std::snprintf(user.short_name, sizeof(user.short_name), "N%03u", i);
        store.upsertUser(i, 0, 1000 + i, user, false);
    }
    index.rebuild(store, filter, 0);

    list.sync(store, index, 0, 1700000000U);
    harness.pump();
    const uint32_t bindsAfterInitialSync = list.bindGenerationForTesting();
    const uint32_t panelMovesAfterInitialSync = list.panelOrderMoveCountForTesting();
    const uint32_t groupMovesAfterInitialSync = list.groupOrderMoveCountForTesting();
    REQUIRE(bindsAfterInitialSync == VirtualNodeList::POOL_SIZE);

    lv_obj_scroll_to_y(parent, list.rowYForTesting(2), LV_ANIM_OFF);
    harness.pump();

    CHECK(list.bindGenerationForTesting() == bindsAfterInitialSync + 1);
    CHECK(list.panelOrderMoveCountForTesting() == panelMovesAfterInitialSync + VirtualNodeList::POOL_SIZE + 1);
    CHECK(list.groupOrderMoveCountForTesting() == groupMovesAfterInitialSync + VirtualNodeList::POOL_SIZE);
}

TEST_CASE("VirtualNodeList fills a tall node-list viewport with overscan")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    lv_obj_set_size(parent, 400, 422);
    lv_obj_set_style_pad_all(parent, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_update_layout(parent);
    VirtualNodeList list(parent, sink);

    CHECK(list.boundRowCount() == VirtualNodeList::POOL_SIZE + 2);
}

TEST_CASE("VirtualNodeList leaves a tall list when its group has stale foreign focus")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    lv_obj_set_size(parent, 400, 422);
    lv_obj_set_style_pad_all(parent, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_update_layout(parent);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    for (NodeId id = 1; id <= 9; ++id) {
        meshtastic_User user{};
        std::snprintf(user.short_name, sizeof(user.short_name), "N%03u", id);
        store.upsertUser(id, 0, 1000 + id, user, false);
    }
    index.rebuild(store, filter, 0);
    list.sync(store, index, 0, 1700000000U);
    harness.pump();
    REQUIRE(list.boundRowCount() == VirtualNodeList::MAX_POOL_SIZE);

    list.focus(1);
    lv_obj_t *staleFocus = lv_button_create(parent);
    lv_group_add_obj(list.navigationGroup(), staleFocus);
    lv_group_focus_obj(staleFocus);

    sink.boundaryCalled = false;
    lv_group_focus_next(list.navigationGroup());

    CHECK(sink.boundaryCalled);
    CHECK(sink.boundaryForward);
}

TEST_CASE("VirtualNodeList defers row ordering until fast multi-row scrolling ends")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    for (uint32_t i = 1; i <= 30; ++i) {
        meshtastic_User user{};
        std::snprintf(user.short_name, sizeof(user.short_name), "N%03u", i);
        store.upsertUser(i, 0, 1000 + i, user, false);
    }
    index.rebuild(store, filter, 0);
    list.sync(store, index, 0, 1700000000U);
    harness.pump();

    const uint32_t bindsBeforeForward = list.bindGenerationForTesting();
    const uint32_t panelMovesBeforeForward = list.panelOrderMoveCountForTesting();
    const uint32_t groupMovesBeforeForward = list.groupOrderMoveCountForTesting();
    lv_obj_scroll_to_y(parent, list.rowYForTesting(4), LV_ANIM_OFF);
    harness.pump();

    CHECK(list.bindGenerationForTesting() == bindsBeforeForward + 3);
    CHECK(list.panelOrderMoveCountForTesting() == panelMovesBeforeForward + VirtualNodeList::POOL_SIZE + 1);
    CHECK(list.groupOrderMoveCountForTesting() == groupMovesBeforeForward + VirtualNodeList::POOL_SIZE);
    CHECK(boundRow(parent, 27) != nullptr);

    const uint32_t bindsBeforeBackward = list.bindGenerationForTesting();
    const uint32_t panelMovesBeforeBackward = list.panelOrderMoveCountForTesting();
    const uint32_t groupMovesBeforeBackward = list.groupOrderMoveCountForTesting();
    lv_obj_scroll_to_y(parent, list.rowYForTesting(1), LV_ANIM_OFF);
    harness.pump();

    CHECK(list.bindGenerationForTesting() == bindsBeforeBackward + 3);
    CHECK(list.panelOrderMoveCountForTesting() == panelMovesBeforeBackward + VirtualNodeList::POOL_SIZE + 1);
    CHECK(list.groupOrderMoveCountForTesting() == groupMovesBeforeBackward + VirtualNodeList::POOL_SIZE);
    CHECK(boundRow(parent, 30) != nullptr);
}

TEST_CASE("VirtualNodeList removes both scroll callbacks when destroyed")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    const uint32_t eventsBefore = lv_obj_get_event_count(parent);
    {
        VirtualNodeList list(parent, sink);
        CHECK(lv_obj_get_event_count(parent) == eventsBefore + 2);
    }
    CHECK(lv_obj_get_event_count(parent) == eventsBefore);
}

TEST_CASE("VirtualNodeList click retains the pressed node across pool reuse")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    for (uint32_t i = 1; i <= 30; ++i) {
        meshtastic_User user{};
        std::snprintf(user.short_name, sizeof(user.short_name), "N%03u", i);
        store.upsertUser(i, 0, 1000 + i, user, false);
    }
    index.rebuild(store, filter, 0);
    list.sync(store, index);

    lv_obj_t *pressedRow = boundRow(parent, 30);
    REQUIRE(pressedRow != nullptr);
    lv_obj_t *pressedButton = lv_obj_get_child(pressedRow, 1);
    REQUIRE(pressedButton != nullptr);
    lv_obj_send_event(pressedButton, LV_EVENT_PRESSED, nullptr);

    list.scrollTo(1, LV_ANIM_OFF);
    harness.pump();
    REQUIRE(static_cast<NodeId>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(pressedButton))) != 30);

    lv_obj_send_event(pressedButton, LV_EVENT_CLICKED, nullptr);
    CHECK(sink.lastClicked == 30);
}

TEST_CASE("VirtualNodeList clears retained press identity when LVGL cancels the press")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    for (uint32_t i = 1; i <= 30; ++i) {
        meshtastic_User user{};
        std::snprintf(user.short_name, sizeof(user.short_name), "N%03u", i);
        store.upsertUser(i, 0, 1000 + i, user, false);
    }
    index.rebuild(store, filter, 0);
    list.sync(store, index);

    lv_obj_t *pressedRow = boundRow(parent, 30);
    REQUIRE(pressedRow != nullptr);
    lv_obj_t *pressedButton = lv_obj_get_child(pressedRow, 1);
    REQUIRE(pressedButton != nullptr);

    SUBCASE("press lost")
    {
        lv_obj_send_event(pressedButton, LV_EVENT_PRESSED, nullptr);
        lv_obj_send_event(pressedButton, LV_EVENT_PRESS_LOST, nullptr);
        list.scrollTo(1, LV_ANIM_OFF);
        harness.pump();

        lv_obj_send_event(pressedButton, LV_EVENT_CLICKED, nullptr);
        CHECK(sink.lastClicked != 30);
    }

    SUBCASE("cancel")
    {
        lv_obj_send_event(pressedButton, LV_EVENT_PRESSED, nullptr);
        lv_obj_send_event(pressedButton, LV_EVENT_CANCEL, nullptr);
        list.scrollTo(1, LV_ANIM_OFF);
        harness.pump();

        lv_obj_send_event(pressedButton, LV_EVENT_CLICKED, nullptr);
        CHECK(sink.lastClicked != 30);
    }
}

TEST_CASE("VirtualNodeList group edge navigation uses the currently focused recycled row")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    for (uint32_t i = 1; i <= 30; ++i) {
        meshtastic_User user{};
        std::snprintf(user.short_name, sizeof(user.short_name), "N%03u", i);
        store.upsertUser(i, 0, 1000 + i, user, false);
    }
    index.rebuild(store, filter, 0);
    list.sync(store, index);

    list.focus(24);
    REQUIRE(sink.lastFocused == 24);

    lv_obj_t *focused = lv_group_get_focused(list.navigationGroup());
    REQUIRE(focused != nullptr);
    list.scrollTo(20, LV_ANIM_OFF);
    harness.pump();

    const NodeId recycledFocusedId = static_cast<NodeId>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(focused)));
    REQUIRE(recycledFocusedId != 0);
    REQUIRE(recycledFocusedId != 24);
    auto recycledIndex = index.indexOf(recycledFocusedId);
    REQUIRE(recycledIndex.has_value());
    REQUIRE(recycledIndex.value() + 1 < index.ids().size());
    const NodeId expectedNext = index.ids()[recycledIndex.value() + 1];

    lv_group_focus_next(list.navigationGroup());
    harness.pump();

    CHECK(sink.lastFocused == expectedNext);
}

TEST_CASE("VirtualNodeList hands both logical ends back to its action sink")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    VirtualNodeList list(parent, sink);
    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    meshtastic_User user{};
    std::strcpy(user.short_name, "ONE");
    store.upsertUser(1, 0, 1000, user, false);
    index.rebuild(store, filter, 0);
    list.sync(store, index);

    list.focus(1);
    lv_group_focus_prev(list.navigationGroup());
    harness.pump();
    CHECK(sink.boundaryCalled);
    CHECK_FALSE(sink.boundaryForward);

    sink.boundaryCalled = false;
    list.focus(1);
    lv_group_focus_next(list.navigationGroup());
    harness.pump();
    CHECK(sink.boundaryCalled);
    CHECK(sink.boundaryForward);
}

TEST_CASE("VirtualNodeList clears IAQ label styles when a pooled row becomes non-IAQ")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    VirtualNodeList list(parent, sink);
    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    meshtastic_User user{};
    std::strcpy(user.short_name, "AIR");
    store.upsertUser(1, 0, 1000, user, false);
    meshtastic_EnvironmentMetrics air{};
    air.iaq = 42;
    store.updateEnvironmentMetrics(1, air);
    index.rebuild(store, filter, 0);

    NodeListRenderContext context;
    context.highlightIaq = true;
    list.sync(store, index, 1, 1700000000U, context);
    list.finishExpansionForTesting();
    lv_obj_t *row = boundRow(parent, 1);
    REQUIRE(row != nullptr);
    lv_obj_t *iaqLabel = lv_obj_get_child(row, 10);
    REQUIRE(iaqLabel != nullptr);
    CHECK(lv_obj_get_style_bg_opa(iaqLabel, LV_PART_MAIN) == LV_OPA_COVER);

    context.highlightIaq = false;
    list.sync(store, index, 1, 1700000000U, context, true);
    harness.pump();

    lv_obj_t *normalLabel = lv_label_create(row);
    CHECK(lv_obj_get_style_bg_opa(iaqLabel, LV_PART_MAIN) == LV_OPA_TRANSP);
    CHECK(lv_color_to_u32(lv_obj_get_style_bg_color(iaqLabel, LV_PART_MAIN)) ==
          lv_color_to_u32(lv_obj_get_style_bg_color(normalLabel, LV_PART_MAIN)));
    CHECK(lv_color_to_u32(lv_obj_get_style_text_color(iaqLabel, LV_PART_MAIN)) ==
          lv_color_to_u32(lv_obj_get_style_text_color(normalLabel, LV_PART_MAIN)));
    lv_obj_delete(normalLabel);
}

TEST_CASE("VirtualNodeList rebinds visible highlights when only render context changes")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    VirtualNodeList list(parent, sink);
    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    meshtastic_User user{};
    std::strcpy(user.short_name, "MATCH");
    std::strcpy(user.long_name, "Context Match");
    store.upsertUser(1, 0, 1000, user, false);
    index.rebuild(store, filter, 0);

    list.sync(store, index, 0, 1700000000U);
    lv_obj_t *row = boundRow(parent, 1);
    REQUIRE(row != nullptr);
    const uint32_t bindsBefore = list.bindGenerationForTesting();

    NodeListRenderContext context;
    std::strcpy(context.highlightName, "Match");
    list.sync(store, index, 0, 1700000000U, context);

    CHECK(list.bindGenerationForTesting() > bindsBefore);
    CHECK(lv_color_to_u32(lv_obj_get_style_border_color(row, LV_PART_MAIN)) == kHighlightMeshRowBorder);
}

TEST_CASE("VirtualNodeList unit-test finish removes the live LVGL expansion animation")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
    REQUIRE(parent != nullptr);
    VirtualNodeList list(parent, sink);

    NodeStore store;
    VisibleNodeIndex index;
    NodeListFilter filter;
    for (uint32_t i = 1; i <= 5; ++i) {
        meshtastic_User user{};
        std::snprintf(user.short_name, sizeof(user.short_name), "N%03u", i);
        store.upsertUser(i, 0, 1000 + i, user, false);
    }
    index.rebuild(store, filter, 0);

    list.sync(store, index, 2, 1700000000U);
    REQUIRE(list.expansionAnimationRegisteredForTesting());

    list.finishExpansionForTesting();

    CHECK_FALSE(list.expansionAnimationRegisteredForTesting());
    CHECK_FALSE(list.expansionAnimating());
    CHECK(list.rowHeightForTesting(2) == VirtualNodeList::EXPANDED_ROW_HEIGHT);
}

TEST_CASE("VirtualNodeList handles rapid deletion while scrolled down and overscroll gracefully")
{
    MuiTestHarness harness;
    harness.resetNodeList();

    DummyActionSink sink;
    StandaloneListParent parent(harness);
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
