#include "doctest.h"
#include "graphics/common/VisibleNodeIndex.h"

#include <algorithm>

namespace
{

meshtastic_User makeUser(const char *id, const char *longName, const char *shortName,
                         meshtastic_Config_DeviceConfig_Role role = meshtastic_Config_DeviceConfig_Role_CLIENT,
                         bool withKey = true)
{
    meshtastic_User u{};
    snprintf(u.id, sizeof(u.id), "%s", id);
    snprintf(u.long_name, sizeof(u.long_name), "%s", longName);
    snprintf(u.short_name, sizeof(u.short_name), "%s", shortName);
    u.role = role;
    if (withKey) {
        u.public_key.size = 32;
        memset(u.public_key.bytes, 0x42, 32);
    }
    return u;
}

NodeStore fixtureStore()
{
    NodeStore store;
    // Node 1: Alpha (recent, channel 0, hops 0, with key, with position)
    store.upsertUser(0x1111, 0, 1000, makeUser("!00001111", "Alpha Node", "ALPH"), false);
    NodePosition pos1{true, 377749000, -1224194000, 10, 8, 1};
    store.updatePosition(0x1111, pos1);
    store.updateHops(0x1111, 0);

    // Node 2: Bravo (older, channel 1, hops 2, unknown/fallback, no key)
    store.upsertUnknown(0x2222, 1, 500, 0, false, false);
    store.updateHops(0x2222, 2);

    // Node 3: Charlie (offline / lastHeard=0, channel 0, hops 1, with key)
    store.upsertUser(0x3333, 0, 0, makeUser("!00003333", "Charlie Station", "CHAR"), false);
    store.updateHops(0x3333, 1);

    // Node 4: Delta (medium age, channel 0, hops -1 / direct, with key)
    store.upsertUser(0x4444, 0, 800, makeUser("!00004444", "Delta Gateway", "DELT"), false);

    // Node 5: Own Node (always included regardless of filters)
    store.upsertUser(0x9999, 0, 1000, makeUser("!00009999", "My Self Node", "SELF"), false);

    return store;
}

} // namespace

TEST_CASE("visible index applies current filters and newest-first ordering")
{
    NodeStore store = fixtureStore();
    VisibleNodeIndex index;
    NodeListFilter filter;
    filter.curTime = 1000;
    filter.secsUntilOffline = 300; // nodes older than 700 are offline
    NodeId ownNode = 0x9999;

    SUBCASE("no filter: sorted newest first, then NodeId ascending")
    {
        index.rebuild(store, filter, ownNode);
        // Order expected:
        // 0x1111 (1000) & 0x9999 (1000) -> 0x1111, 0x9999
        // 0x4444 (800)
        // 0x2222 (500)
        // 0x3333 (0)
        const auto &ids = index.ids();
        REQUIRE(ids.size() == 5);
        CHECK(ids[0] == 0x1111);
        CHECK(ids[1] == 0x9999);
        CHECK(ids[2] == 0x4444);
        CHECK(ids[3] == 0x2222);
        CHECK(ids[4] == 0x3333);
    }

    SUBCASE("filter unknown: excludes 0x2222 (unknown)")
    {
        filter.unknown = true;
        index.rebuild(store, filter, ownNode);
        CHECK(index.ids() == std::vector<NodeId>{0x1111, 0x9999, 0x4444, 0x3333});
    }

    SUBCASE("filter offline: excludes 0x2222 (500 < 700) and 0x3333 (0)")
    {
        filter.offline = true;
        index.rebuild(store, filter, ownNode);
        CHECK(index.ids() == std::vector<NodeId>{0x1111, 0x9999, 0x4444});
    }

    SUBCASE("filter public key: excludes 0x2222 (hasKey=false)")
    {
        filter.publicKey = true;
        index.rebuild(store, filter, ownNode);
        CHECK(index.ids() == std::vector<NodeId>{0x1111, 0x9999, 0x4444, 0x3333});
    }

    SUBCASE("filter channel: channel 1 (dropdown selected=2 -> channel=1) selects only 0x2222 plus ownNode")
    {
        filter.channel = 2; // channel index 1
        index.rebuild(store, filter, ownNode);
        CHECK(index.ids() == std::vector<NodeId>{0x9999, 0x2222});
    }

    SUBCASE("filter position: requires known coordinates, selects 0x1111 plus ownNode")
    {
        filter.position = true;
        index.rebuild(store, filter, ownNode);
        CHECK(index.ids() == std::vector<NodeId>{0x1111, 0x9999});
    }

    SUBCASE("filter mqtt: selects only nodes via mqtt plus ownNode")
    {
        store.upsertUser(0x5555, 0, 950, makeUser("!00005555", "MQTT Node", "MQTT"), true);
        filter.viaMqtt = true;
        index.rebuild(store, filter, ownNode);
        CHECK(index.ids() == std::vector<NodeId>{0x9999, 0x5555});
    }

    SUBCASE("filter hops: hops <= 0 (dropdown 7) selects direct nodes (0x1111)")
    {
        filter.hops = 7; // 7 - 7 = 0 -> hopsAway <= 0
        index.rebuild(store, filter, ownNode);
        CHECK(index.contains(0x1111));
        CHECK_FALSE(index.contains(0x2222)); // hops 2
        CHECK_FALSE(index.contains(0x3333)); // hops 1
    }

    SUBCASE("filter hops: hops >= 2 (dropdown 9) selects 0x2222")
    {
        filter.hops = 9; // 9 - 7 = 2 -> hopsAway >= 2
        index.rebuild(store, filter, ownNode);
        CHECK(index.contains(0x2222));
        CHECK_FALSE(index.contains(0x1111)); // hops 0
        CHECK_FALSE(index.contains(0x3333)); // hops 1
    }

    SUBCASE("filter name: case-insensitive search matching 'alp'")
    {
        filter.name = "alp";
        index.rebuild(store, filter, ownNode);
        CHECK(index.ids() == std::vector<NodeId>{0x1111, 0x9999}); // 0x1111 matches, ownNode always kept
    }

    SUBCASE("filter name: search matching hex node ID of unknown node '2222'")
    {
        filter.name = "2222";
        index.rebuild(store, filter, ownNode);
        CHECK(index.contains(0x2222));
        CHECK_FALSE(index.contains(0x1111));
    }

    SUBCASE("filter name negation: '!' excludes matches")
    {
        filter.name = "!alpha";
        index.rebuild(store, filter, ownNode);
        // Excludes 0x1111, keeps others
        CHECK(std::find(index.ids().begin(), index.ids().end(), 0x1111) == index.ids().end());
        CHECK(index.contains(0x4444));
        CHECK(index.contains(0x9999));
    }
}

TEST_CASE("visible index preserves stable selection lookup after last-heard reorder")
{
    NodeStore store;
    store.upsertUser(0x1111, 0, 500, makeUser("!00001111", "Alpha", "A"), false);
    store.upsertUser(0x2222, 0, 300, makeUser("!00002222", "Bravo", "B"), false);
    store.upsertUser(0x3333, 0, 100, makeUser("!00003333", "Charlie", "C"), false);

    VisibleNodeIndex index;
    NodeListFilter filter;
    index.rebuild(store, filter, 0);

    REQUIRE(index.ids().size() == 3);
    CHECK(index.indexOf(0x2222) == std::optional<size_t>(1));

    // Update Bravo to be newest
    store.updateLastHeard(0x2222, 900);
    index.rebuild(store, filter, 0);

    CHECK(index.indexOf(0x2222) == std::optional<size_t>(0));
    CHECK(index.indexOf(0x1111) == std::optional<size_t>(1));
    CHECK(index.indexOf(0x3333) == std::optional<size_t>(2));
    CHECK(index.indexOf(0x9999) == std::nullopt);
}
