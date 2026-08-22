#include "graphics/common/NodeStore.h"

#include <doctest/doctest.h>

#include <cstdio>
#include <ctime>

namespace
{
constexpr auto unknownRole = static_cast<meshtastic_Config_DeviceConfig_Role>(15);

meshtastic_User makeUser(const char *shortName, const char *longName,
                         meshtastic_Config_DeviceConfig_Role role = meshtastic_Config_DeviceConfig_Role_CLIENT)
{
    meshtastic_User user = meshtastic_User_init_default;
    std::snprintf(user.short_name, sizeof(user.short_name), "%s", shortName);
    std::snprintf(user.long_name, sizeof(user.long_name), "%s", longName);
    user.role = role;
    return user;
}
} // namespace

TEST_CASE("node store retains typed user fields without LVGL")
{
    NodeStore store;
    auto user = makeUser("ABCD", "Alpha", meshtastic_Config_DeviceConfig_Role_ROUTER);
    user.public_key.size = 1;
    user.public_key.bytes[0] = 0x42;
    user.has_is_unmessagable = true;
    user.is_unmessagable = true;

    const auto mutation = store.upsertUser(0x12345678, 2, 100, user, true);

    CHECK(mutation.kind == NodeMutationKind::Inserted);
    CHECK(mutation.id == 0x12345678);
    REQUIRE(store.find(0x12345678) != nullptr);
    const auto &record = *store.find(0x12345678);
    CHECK(record.channel == 2);
    CHECK(record.lastHeard == 100);
    CHECK(record.hasUser);
    CHECK(record.user.role == meshtastic_Config_DeviceConfig_Role_ROUTER);
    CHECK(record.user.public_key.bytes[0] == 0x42);
    CHECK(record.hasKey);
    CHECK(record.unmessagable);
    CHECK(record.viaMqtt);
    CHECK(store.size() == 1);
    CHECK(store.records().size() == 1);
}

TEST_CASE("node store creates the current fallback identity for an unknown node")
{
    NodeStore store;

    const auto mutation = store.upsertUnknown(0x1234abcd, 4, 250, meshtastic_Config_DeviceConfig_Role_REPEATER, false, true);

    CHECK(mutation.kind == NodeMutationKind::Inserted);
    REQUIRE(store.find(0x1234abcd) != nullptr);
    const auto &record = *store.find(0x1234abcd);
    CHECK_FALSE(record.hasUser);
    CHECK(record.channel == 4);
    CHECK(record.lastHeard == 250);
    CHECK(record.user.short_name == doctest::String("abcd"));
    CHECK(record.user.long_name == doctest::String("Meshtastic abcd"));
    CHECK(record.user.role == meshtastic_Config_DeviceConfig_Role_REPEATER);
    CHECK(record.user.hw_model == meshtastic_HardwareModel_UNSET);
    CHECK_FALSE(record.hasKey);
    CHECK(record.viaMqtt);
}

TEST_CASE("node store reports unchanged and updated duplicate user mutations")
{
    NodeStore store;
    auto user = makeUser("ALPH", "Alpha");
    store.upsertUser(1, 0, 100, user, false);

    CHECK(store.upsertUser(1, 0, 100, user, false).kind == NodeMutationKind::Unchanged);

    user.role = meshtastic_Config_DeviceConfig_Role_SENSOR;
    const auto mutation = store.upsertUser(1, 3, 200, user, true);
    CHECK(mutation.kind == NodeMutationKind::Updated);
    REQUIRE(store.find(1) != nullptr);
    CHECK(store.find(1)->channel == 3);
    CHECK(store.find(1)->lastHeard == 200);
    CHECK(store.find(1)->user.role == meshtastic_Config_DeviceConfig_Role_SENSOR);
    CHECK(store.find(1)->viaMqtt);
}

TEST_CASE("node store updates position telemetry and radio fields on an existing node")
{
    NodeStore store;
    store.upsertUnknown(7, 0, 10, meshtastic_Config_DeviceConfig_Role_CLIENT, false, false);

    NodePosition position{true, 123456789, -987654321, 1234, 9, 22};
    meshtastic_DeviceMetrics device = meshtastic_DeviceMetrics_init_default;
    device.has_battery_level = true;
    device.battery_level = 78;
    meshtastic_EnvironmentMetrics environment = meshtastic_EnvironmentMetrics_init_default;
    environment.has_temperature = true;
    environment.temperature = 22.5f;
    meshtastic_AirQualityMetrics air = meshtastic_AirQualityMetrics_init_default;
    air.has_pm25_standard = true;
    air.pm25_standard = 17;

    CHECK(store.updatePosition(7, position).kind == NodeMutationKind::Updated);
    CHECK(store.updatePosition(7, position).kind == NodeMutationKind::Unchanged);
    CHECK(store.updateDeviceMetrics(7, device).kind == NodeMutationKind::Updated);
    CHECK(store.updateEnvironmentMetrics(7, environment).kind == NodeMutationKind::Updated);
    CHECK(store.updateAirQualityMetrics(7, air).kind == NodeMutationKind::Updated);
    CHECK(store.updateSignal(7, -87, 6.25f).kind == NodeMutationKind::Updated);
    CHECK(store.updateHops(7, 3).kind == NodeMutationKind::Updated);
    CHECK(store.updateLastHeard(7, 999).kind == NodeMutationKind::Updated);
    CHECK(store.setActiveChat(7, true).kind == NodeMutationKind::Updated);

    REQUIRE(store.find(7) != nullptr);
    const auto &record = *store.find(7);
    CHECK(record.position.latitude == 123456789);
    CHECK(record.position.longitude == -987654321);
    CHECK(record.position.altitude == 1234);
    CHECK(record.position.satellites == 9);
    CHECK(record.position.precision == 22);
    CHECK(record.hasDeviceMetrics);
    CHECK(record.deviceMetrics.battery_level == 78);
    CHECK(record.hasEnvironmentMetrics);
    CHECK(record.environmentMetrics.temperature == doctest::Approx(22.5f));
    CHECK(record.hasAirQualityMetrics);
    CHECK(record.airQualityMetrics.pm25_standard == 17);
    CHECK(record.rssi == -87);
    CHECK(record.snr == doctest::Approx(6.25f));
    CHECK(record.hopsAway == 3);
    CHECK(record.lastHeard == 999);
    CHECK(record.hasActiveChat);
}

TEST_CASE("node store preserves legacy-visible position and battery fields across incomplete updates")
{
    NodeStore store;
    store.upsertUnknown(7, 0, 10, meshtastic_Config_DeviceConfig_Role_CLIENT, false, false);

    const NodePosition validPosition{true, 123456789, -987654321, 1234, 9, 22};
    CHECK(store.updatePosition(7, validPosition).kind == NodeMutationKind::Updated);
    CHECK(store.updatePosition(7, {true, 0, 0, 0, 0, 0}).kind == NodeMutationKind::Unchanged);
    CHECK(store.find(7)->position.latitude == validPosition.latitude);
    CHECK(store.find(7)->position.longitude == validPosition.longitude);

    meshtastic_DeviceMetrics metrics = meshtastic_DeviceMetrics_init_default;
    metrics.has_battery_level = true;
    metrics.battery_level = 78;
    metrics.has_voltage = true;
    metrics.voltage = 4.12f;
    metrics.has_channel_utilization = true;
    metrics.channel_utilization = 12.5f;
    metrics.has_air_util_tx = true;
    metrics.air_util_tx = 3.2f;
    CHECK(store.updateDeviceMetrics(7, metrics).kind == NodeMutationKind::Updated);

    meshtastic_DeviceMetrics zeroBattery = meshtastic_DeviceMetrics_init_default;
    zeroBattery.has_channel_utilization = true;
    zeroBattery.channel_utilization = 0.0f;
    zeroBattery.has_air_util_tx = true;
    zeroBattery.air_util_tx = 0.0f;
    CHECK(store.updateDeviceMetrics(7, zeroBattery).kind == NodeMutationKind::Updated);
    REQUIRE(store.find(7) != nullptr);
    CHECK(store.find(7)->deviceMetrics.battery_level == 78);
    CHECK(store.find(7)->deviceMetrics.voltage == doctest::Approx(4.12f));
    CHECK(store.find(7)->deviceMetrics.channel_utilization == doctest::Approx(0.0f));
    CHECK(store.find(7)->deviceMetrics.air_util_tx == doctest::Approx(0.0f));
}

TEST_CASE("node store accepts positions on the equator and prime meridian")
{
    NodeStore store;
    store.upsertUnknown(7, 0, 10, meshtastic_Config_DeviceConfig_Role_CLIENT, false, false);

    CHECK(store.updatePosition(7, {true, 0, -987654321, 1234, 9, 22}).kind == NodeMutationKind::Updated);
    CHECK(store.find(7)->position.latitude == 0);
    CHECK(store.find(7)->position.longitude == -987654321);

    CHECK(store.updatePosition(7, {true, 123456789, 0, 1234, 9, 22}).kind == NodeMutationKind::Updated);
    CHECK(store.find(7)->position.latitude == 123456789);
    CHECK(store.find(7)->position.longitude == 0);

    CHECK(store.updatePosition(7, {true, 0, 0, 1234, 9, 22}).kind == NodeMutationKind::Unchanged);
}

TEST_CASE("node store treats RSSI after hops as direct and records bad PKI keys")
{
    NodeStore store;
    store.upsertUser(7, 0, 10, makeUser("NODE", "Node"), false);

    store.updateHops(7, 4);
    auto signal = store.updateSignal(7, -87, 6.25f);
    REQUIRE(store.find(7) != nullptr);
    CHECK((signal.changedFields & NodeFieldHops) != 0U);
    CHECK(store.find(7)->hopsAway == 0);
    CHECK(store.find(7)->signalDisplay == NodeSignalDisplayKind::Rssi);

    CHECK(store.markBadKey(7).kind == NodeMutationKind::Updated);
    CHECK(store.find(7)->hasBadKey);
    CHECK(store.markBadKey(7).kind == NodeMutationKind::Unchanged);
}

TEST_CASE("node store clears a bad-key marker when a replacement public key arrives")
{
    NodeStore store;
    auto user = makeUser("NODE", "Node");
    user.public_key.size = 32;
    user.public_key.bytes[0] = 0x11;
    store.upsertUser(7, 0, 10, user, false);
    store.markBadKey(7);

    user.public_key.bytes[0] = 0x22;
    const NodeMutation mutation = store.upsertUser(7, 0, 10, user, false);

    CHECK(mutation.kind == NodeMutationKind::Updated);
    CHECK((mutation.changedFields & NodeFieldFlags) != 0U);
    CHECK_FALSE(store.find(7)->hasBadKey);
}

TEST_CASE("node store retains a bad-key marker when same-key flags change")
{
    NodeStore store;
    auto user = makeUser("NODE", "Node");
    user.public_key.size = 32;
    user.public_key.bytes[0] = 0x11;
    store.upsertUser(7, 0, 10, user, false);
    store.markBadKey(7);

    user.has_is_unmessagable = true;
    user.is_unmessagable = true;
    store.upsertUser(7, 0, 10, user, false);
    REQUIRE(store.find(7) != nullptr);
    CHECK(store.find(7)->hasBadKey);

    store.upsertUser(7, 0, 10, user, true);
    CHECK(store.find(7)->hasBadKey);
}

TEST_CASE("node store field updates do not create incomplete nodes")
{
    NodeStore store;
    const NodePosition position{true, 1, 2, 3, 4, 5};

    CHECK(store.updatePosition(42, position).kind == NodeMutationKind::Unchanged);
    CHECK(store.updateSignal(42, -90, 1.0f).kind == NodeMutationKind::Unchanged);
    CHECK(store.setActiveChat(42, true).kind == NodeMutationKind::Unchanged);
    CHECK(store.find(42) == nullptr);
    CHECK(store.size() == 0);
}

TEST_CASE("node store removes records with explicit mutation results")
{
    NodeStore store;
    store.upsertUnknown(9, 0, 0, meshtastic_Config_DeviceConfig_Role_CLIENT, false, false);

    CHECK(store.remove(9).kind == NodeMutationKind::Removed);
    CHECK(store.find(9) == nullptr);
    CHECK(store.remove(9).kind == NodeMutationKind::Unchanged);
}

TEST_CASE("node store purge candidate prefers a stale unknown from the oldest population")
{
    constexpr uint32_t now = 100000;
    constexpr NodeId incoming = 0x99;
    constexpr NodeId ownNode = 0x01;
    constexpr NodeId oldestKnown = 0x10;
    constexpr NodeId oldestEligibleUnknown = 0x20;
    NodeStore store;

    store.upsertUser(oldestKnown, 0, now - 10000, makeUser("KNWN", "Known"), false);
    store.upsertUnknown(ownNode, 0, now - 9000, meshtastic_Config_DeviceConfig_Role_CLIENT, false, false);
    store.upsertUnknown(0x30, 0, now - 8000, static_cast<uint8_t>(unknownRole), false, false);
    store.setActiveChat(0x30, true);
    store.upsertUnknown(incoming, 0, now - 7000, static_cast<uint8_t>(unknownRole), false, false);
    store.upsertUnknown(oldestEligibleUnknown, 0, now - 5000, static_cast<uint8_t>(unknownRole), false, false);
    store.upsertUnknown(0x40, 0, now - 30, static_cast<uint8_t>(unknownRole), false, false);

    CHECK(store.selectPurgeCandidate(incoming, ownNode, now) == oldestEligibleUnknown);
}

TEST_CASE("node store purge candidate falls back to the oldest removable node")
{
    constexpr uint32_t now = 100000;
    NodeStore store;
    store.upsertUser(0x10, 0, now - 10000, makeUser("OLD", "Old known"), false);
    store.upsertUnknown(0x20, 0, now - 30, static_cast<uint8_t>(unknownRole), false, false);

    CHECK(store.selectPurgeCandidate(0x99, 0x01, now) == 0x10);
}

TEST_CASE("node store purge candidate keeps retained legacy same-timestamp ordering and protection")
{
    constexpr uint32_t now = 1000;
    NodeStore store;

    store.upsertUnknown(0x30000000, 0, 900, static_cast<uint8_t>(unknownRole), false, false);
    store.upsertUnknown(0x10000000, 0, 900, static_cast<uint8_t>(unknownRole), false, false);
    store.upsertUnknown(0x20000000, 0, 900, static_cast<uint8_t>(unknownRole), false, false);

    CHECK(store.selectPurgeCandidate(0x40000000, 0, now) == 0x20000000);

    store.setActiveChat(0x20000000, true);
    CHECK(store.selectPurgeCandidate(0x40000000, 0, now) == 0x10000000);
}

TEST_CASE("node store purge candidate limits stale-unknown preference to the oldest population")
{
    constexpr uint32_t now = 1000;
    NodeStore store;

    store.upsertUser(0x10000000, 0, 100, makeUser("OLD", "Old Named"), false);
    store.upsertUser(0x20000000, 0, 200, makeUser("MID", "Middle Named"), false);
    store.upsertUser(0x30000000, 0, 300, makeUser("NEW", "New Named"), false);
    store.upsertUser(0x40000000, 0, 400, makeUser("NWR", "Newer Named"), false);
    store.upsertUnknown(0x50000000, 0, 800, static_cast<uint8_t>(unknownRole), false, false);

    CHECK(store.selectPurgeCandidate(0x60000000, 0, now) == 0x10000000);
}

TEST_CASE("node store purge candidate returns zero when every node is protected")
{
    constexpr uint32_t now = 100000;
    constexpr NodeId incoming = 0x99;
    constexpr NodeId ownNode = 0x01;
    NodeStore store;
    store.upsertUnknown(ownNode, 0, now - 5000, static_cast<uint8_t>(unknownRole), false, false);
    store.upsertUnknown(incoming, 0, now - 4000, static_cast<uint8_t>(unknownRole), false, false);
    store.upsertUnknown(0x30, 0, now - 3000, static_cast<uint8_t>(unknownRole), false, false);
    store.setActiveChat(0x30, true);

    CHECK(store.selectPurgeCandidate(incoming, ownNode, now) == 0);
}

TEST_CASE("node store never purges its sole record")
{
    NodeStore store;
    store.upsertUnknown(7, 0, 1, static_cast<uint8_t>(unknownRole), false, false);

    CHECK(store.selectPurgeCandidate(99, 1, 1000) == 0);
}
