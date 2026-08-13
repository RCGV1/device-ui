#include "graphics/common/NodeStore.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

namespace
{
constexpr uint32_t purgeFreshnessSeconds = 120;

bool sameUser(const meshtastic_User &left, const meshtastic_User &right)
{
    return std::strcmp(left.id, right.id) == 0 && std::strcmp(left.long_name, right.long_name) == 0 &&
           std::strcmp(left.short_name, right.short_name) == 0 &&
           std::memcmp(left.macaddr, right.macaddr, sizeof(left.macaddr)) == 0 && left.hw_model == right.hw_model &&
           left.is_licensed == right.is_licensed && left.role == right.role && left.public_key.size == right.public_key.size &&
           std::memcmp(left.public_key.bytes, right.public_key.bytes, left.public_key.size) == 0 &&
           left.has_is_unmessagable == right.has_is_unmessagable && left.is_unmessagable == right.is_unmessagable;
}

bool samePosition(const NodePosition &left, const NodePosition &right)
{
    return left.known == right.known && left.latitude == right.latitude && left.longitude == right.longitude &&
           left.altitude == right.altitude && left.satellites == right.satellites && left.precision == right.precision;
}

template <typename T> bool sameMessage(const T &left, const T &right)
{
    return std::memcmp(&left, &right, sizeof(T)) == 0;
}

NodeMutation unchanged(NodeId id)
{
    return {NodeMutationKind::Unchanged, id, NodeFieldNone};
}

NodeMutation updated(NodeId id, uint32_t fields)
{
    return {fields == NodeFieldNone ? NodeMutationKind::Unchanged : NodeMutationKind::Updated, id, fields};
}
} // namespace

const NodeRecord *NodeStore::find(NodeId id) const
{
    const auto it = nodes.find(id);
    return it == nodes.end() ? nullptr : &it->second;
}

NodeMutation NodeStore::upsertUser(NodeId id, uint8_t channel, uint32_t lastHeard, const meshtastic_User &user, bool viaMqtt)
{
    auto [it, inserted] = nodes.try_emplace(id);
    auto &record = it->second;
    uint32_t changed = NodeFieldNone;

    if (inserted) {
        record.id = id;
        changed = NodeFieldUser | NodeFieldChannel | NodeFieldFlags | NodeFieldLastHeard;
    }
    if (!record.hasUser || !sameUser(record.user, user)) {
        record.hasUser = true;
        record.user = user;
        changed |= NodeFieldUser;
    }
    if (record.channel != channel) {
        record.channel = channel;
        changed |= NodeFieldChannel;
    }
    const bool hasKey = user.public_key.size != 0;
    const bool unmessagable = user.has_is_unmessagable && user.is_unmessagable;
    if (record.hasKey != hasKey || record.unmessagable != unmessagable || record.viaMqtt != viaMqtt) {
        record.hasKey = hasKey;
        record.unmessagable = unmessagable;
        record.viaMqtt = viaMqtt;
        changed |= NodeFieldFlags;
    }
    if (record.lastHeard != lastHeard) {
        record.lastHeard = lastHeard;
        changed |= NodeFieldLastHeard;
    }

    return inserted ? NodeMutation{NodeMutationKind::Inserted, id, changed} : updated(id, changed);
}

NodeMutation NodeStore::upsertUnknown(NodeId id, uint8_t channel, uint32_t lastHeard, uint8_t role, bool hasKey, bool viaMqtt)
{
    meshtastic_User fallback = meshtastic_User_init_default;
    std::snprintf(fallback.short_name, sizeof(fallback.short_name), "%04x", id & 0xffff);
    std::snprintf(fallback.long_name, sizeof(fallback.long_name), "Meshtastic %s", fallback.short_name);
    fallback.role = static_cast<meshtastic_Config_DeviceConfig_Role>(role);
    fallback.hw_model = meshtastic_HardwareModel_UNSET;

    auto [it, inserted] = nodes.try_emplace(id);
    auto &record = it->second;
    uint32_t changed = NodeFieldNone;

    if (inserted) {
        record.id = id;
        changed = NodeFieldUser | NodeFieldChannel | NodeFieldFlags | NodeFieldLastHeard;
    }
    if (record.hasUser || !sameUser(record.user, fallback)) {
        record.hasUser = false;
        record.user = fallback;
        changed |= NodeFieldUser;
    }
    if (record.channel != channel) {
        record.channel = channel;
        changed |= NodeFieldChannel;
    }
    if (record.hasKey != hasKey || record.unmessagable || record.viaMqtt != viaMqtt) {
        record.hasKey = hasKey;
        record.unmessagable = false;
        record.viaMqtt = viaMqtt;
        changed |= NodeFieldFlags;
    }
    if (record.lastHeard != lastHeard) {
        record.lastHeard = lastHeard;
        changed |= NodeFieldLastHeard;
    }

    return inserted ? NodeMutation{NodeMutationKind::Inserted, id, changed} : updated(id, changed);
}

NodeMutation NodeStore::updatePosition(NodeId id, const NodePosition &position)
{
    auto it = nodes.find(id);
    if (it == nodes.end() || samePosition(it->second.position, position))
        return unchanged(id);
    it->second.position = position;
    return updated(id, NodeFieldPosition);
}

NodeMutation NodeStore::updateDeviceMetrics(NodeId id, const meshtastic_DeviceMetrics &metrics)
{
    auto it = nodes.find(id);
    if (it == nodes.end())
        return unchanged(id);
    if (it->second.hasDeviceMetrics && sameMessage(it->second.deviceMetrics, metrics))
        return unchanged(id);
    it->second.hasDeviceMetrics = true;
    it->second.deviceMetrics = metrics;
    return updated(id, NodeFieldDeviceMetrics);
}

NodeMutation NodeStore::updateEnvironmentMetrics(NodeId id, const meshtastic_EnvironmentMetrics &metrics)
{
    auto it = nodes.find(id);
    if (it == nodes.end())
        return unchanged(id);
    if (it->second.hasEnvironmentMetrics && sameMessage(it->second.environmentMetrics, metrics))
        return unchanged(id);
    it->second.hasEnvironmentMetrics = true;
    it->second.environmentMetrics = metrics;
    return updated(id, NodeFieldEnvironmentMetrics);
}

NodeMutation NodeStore::updateAirQualityMetrics(NodeId id, const meshtastic_AirQualityMetrics &metrics)
{
    auto it = nodes.find(id);
    if (it == nodes.end())
        return unchanged(id);
    if (it->second.hasAirQualityMetrics && sameMessage(it->second.airQualityMetrics, metrics))
        return unchanged(id);
    it->second.hasAirQualityMetrics = true;
    it->second.airQualityMetrics = metrics;
    return updated(id, NodeFieldAirQualityMetrics);
}

NodeMutation NodeStore::updateSignal(NodeId id, int32_t rssi, float snr)
{
    auto it = nodes.find(id);
    if (it == nodes.end() || (it->second.rssi == rssi && it->second.snr == snr))
        return unchanged(id);
    it->second.rssi = rssi;
    it->second.snr = snr;
    return updated(id, NodeFieldSignal);
}

NodeMutation NodeStore::updateHops(NodeId id, int8_t hopsAway)
{
    auto it = nodes.find(id);
    if (it == nodes.end() || it->second.hopsAway == hopsAway)
        return unchanged(id);
    it->second.hopsAway = hopsAway;
    return updated(id, NodeFieldHops);
}

NodeMutation NodeStore::updateLastHeard(NodeId id, uint32_t now)
{
    auto it = nodes.find(id);
    if (it == nodes.end() || it->second.lastHeard == now)
        return unchanged(id);
    it->second.lastHeard = now;
    return updated(id, NodeFieldLastHeard);
}

NodeMutation NodeStore::setActiveChat(NodeId id, bool active)
{
    auto it = nodes.find(id);
    if (it == nodes.end() || it->second.hasActiveChat == active)
        return unchanged(id);
    it->second.hasActiveChat = active;
    return updated(id, NodeFieldActiveChat);
}

NodeMutation NodeStore::remove(NodeId id)
{
    if (nodes.erase(id) == 0)
        return unchanged(id);
    return {NodeMutationKind::Removed, id, NodeFieldNone};
}

NodeId NodeStore::selectPurgeCandidate(NodeId incoming, NodeId ownNode) const
{
    return selectPurgeCandidate(incoming, ownNode, static_cast<uint32_t>(std::time(nullptr)));
}

NodeId NodeStore::selectPurgeCandidate(NodeId incoming, NodeId ownNode, uint32_t now) const
{
    if (nodes.size() <= 1)
        return 0;

    std::vector<const NodeRecord *> ordered;
    ordered.reserve(nodes.size());
    for (const auto &[id, record] : nodes)
        ordered.push_back(&record);
    std::sort(ordered.begin(), ordered.end(), [](const NodeRecord *left, const NodeRecord *right) {
        return left->lastHeard == right->lastHeard ? left->id < right->id : left->lastHeard < right->lastHeard;
    });

    const auto removable = [incoming, ownNode](const NodeRecord &record) {
        return record.id != incoming && record.id != ownNode && !record.hasActiveChat;
    };
    const auto staleUnknown = [now](const NodeRecord &record) {
        return !record.hasUser && now >= record.lastHeard && now - record.lastHeard >= purgeFreshnessSeconds;
    };

    const size_t preferredPopulation = (ordered.size() * 4 + 4) / 5;
    for (size_t i = 0; i < preferredPopulation; ++i) {
        if (removable(*ordered[i]) && staleUnknown(*ordered[i]))
            return ordered[i]->id;
    }
    for (const auto *record : ordered) {
        if (removable(*record))
            return record->id;
    }
    return 0;
}
