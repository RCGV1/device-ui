#include "graphics/common/NodeStore.h"
#include "graphics/common/MeshtasticView.h"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace
{
constexpr uint32_t purgeFreshnessSeconds = 120;

uint32_t effectiveLastHeard(const NodeRecord &record, uint32_t now)
{
    return record.lastHeard > now ? now : record.lastHeard;
}

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

bool samePublicKey(const meshtastic_User &left, const meshtastic_User &right)
{
    return left.public_key.size == right.public_key.size &&
           std::memcmp(left.public_key.bytes, right.public_key.bytes, left.public_key.size) == 0;
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

void NodeStore::touchRecency(NodeRecord &record)
{
    record.recencyPromoted = true;
    record.recencyOrder = nextRecencyOrder++;
}

NodeMutation NodeStore::upsertUser(NodeId id, uint8_t channel, uint32_t lastHeard, const meshtastic_User &user, bool viaMqtt)
{
    auto [it, inserted] = nodes.try_emplace(id);
    auto &record = it->second;
    uint32_t changed = NodeFieldNone;
    const bool keyChanged = record.hasUser && !samePublicKey(record.user, user);

    if (inserted) {
        record.id = id;
        record.recencyOrder = nextRecencyOrder++;
        changed = NodeFieldUser | NodeFieldFlags | NodeFieldLastHeard;
        if (channel < c_max_channels) {
            changed |= NodeFieldChannel;
        }
    }
    if (!record.hasUser || !sameUser(record.user, user)) {
        record.hasUser = true;
        record.user = user;
        changed |= NodeFieldUser;
    }
    if (channel < c_max_channels) {
        if (record.channel != channel) {
            record.channel = channel;
            changed |= NodeFieldChannel;
        }
    } else if (inserted) {
        // New record with invalid channel keeps default 0 without dirtying.
        record.channel = 0;
    }
    const bool hasKey = user.public_key.size != 0;
    const bool unmessagable = user.has_is_unmessagable && user.is_unmessagable;
    const bool keyPresenceChanged = record.hasKey != hasKey;
    if (keyPresenceChanged || record.unmessagable != unmessagable || record.viaMqtt != viaMqtt || keyChanged) {
        record.hasKey = hasKey;
        if (keyPresenceChanged || keyChanged) {
            record.hasBadKey = false;
        }
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
        record.recencyOrder = nextRecencyOrder++;
        changed = NodeFieldUser | NodeFieldFlags | NodeFieldLastHeard;
        if (channel < c_max_channels) {
            changed |= NodeFieldChannel;
        }
    }
    if (record.hasUser || !sameUser(record.user, fallback)) {
        record.hasUser = false;
        record.user = fallback;
        changed |= NodeFieldUser;
    }
    if (inserted && channel < c_max_channels && record.channel != channel) {
        record.channel = channel;
        changed |= NodeFieldChannel;
    } else if (inserted && channel >= c_max_channels) {
        record.channel = 0;
    }
    if (record.hasKey != hasKey || record.unmessagable || record.viaMqtt != viaMqtt) {
        record.hasKey = hasKey;
        record.hasBadKey = false;
        record.unmessagable = false;
        record.viaMqtt = viaMqtt;
        changed |= NodeFieldFlags;
    }
    if (inserted && record.lastHeard != lastHeard) {
        record.lastHeard = lastHeard;
        changed |= NodeFieldLastHeard;
    }

    return inserted ? NodeMutation{NodeMutationKind::Inserted, id, changed} : updated(id, changed);
}

NodeMutation NodeStore::updatePosition(NodeId id, const NodePosition &position)
{
    auto it = nodes.find(id);
    if (it == nodes.end() || !position.hasCoordinates() || samePosition(it->second.position, position))
        return unchanged(id);
    it->second.position = position;
    return updated(id, NodeFieldPosition);
}

NodeMutation NodeStore::updateDeviceMetrics(NodeId id, const meshtastic_DeviceMetrics &metrics)
{
    auto it = nodes.find(id);
    if (it == nodes.end())
        return unchanged(id);
    auto retainedMetrics = metrics;
    if (it->second.hasDeviceMetrics && metrics.battery_level == 0 && metrics.voltage == 0.0f) {
        retainedMetrics.battery_level = it->second.deviceMetrics.battery_level;
        retainedMetrics.voltage = it->second.deviceMetrics.voltage;
    }
    if (it->second.hasDeviceMetrics && sameMessage(it->second.deviceMetrics, retainedMetrics))
        return unchanged(id);
    it->second.hasDeviceMetrics = true;
    it->second.deviceMetrics = retainedMetrics;
    return updated(id, NodeFieldDeviceMetrics);
}

NodeMutation NodeStore::updateEnvironmentMetrics(NodeId id, const meshtastic_EnvironmentMetrics &metrics)
{
    auto it = nodes.find(id);
    if (it == nodes.end())
        return unchanged(id);
    auto retainedMetrics = metrics;
    if (it->second.hasEnvironmentMetrics && (metrics.iaq == 0 || metrics.iaq >= 1000) && it->second.environmentMetrics.iaq > 0 &&
        it->second.environmentMetrics.iaq < 1000) {
        retainedMetrics.iaq = it->second.environmentMetrics.iaq;
        retainedMetrics.voltage = it->second.environmentMetrics.voltage;
        retainedMetrics.current = it->second.environmentMetrics.current;
    }
    if (it->second.hasEnvironmentMetrics && sameMessage(it->second.environmentMetrics, retainedMetrics))
        return unchanged(id);
    it->second.hasEnvironmentMetrics = true;
    it->second.environmentMetrics = retainedMetrics;
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
    if (it == nodes.end())
        return unchanged(id);
    const bool changed = it->second.rssi != rssi || it->second.snr != snr || it->second.hopsAway != 0 ||
                         it->second.signalDisplay != NodeSignalDisplayKind::Rssi;
    it->second.rssi = rssi;
    it->second.snr = snr;
    it->second.hopsAway = 0;
    it->second.signalDisplay = NodeSignalDisplayKind::Rssi;
    if (!changed)
        return unchanged(id);
    return updated(id, NodeFieldSignal | NodeFieldHops);
}

NodeMutation NodeStore::updateHops(NodeId id, int8_t hopsAway)
{
    auto it = nodes.find(id);
    if (it == nodes.end())
        return unchanged(id);
    const bool changed = it->second.hopsAway != hopsAway || it->second.signalDisplay != NodeSignalDisplayKind::Hops;
    it->second.hopsAway = hopsAway;
    it->second.signalDisplay = NodeSignalDisplayKind::Hops;
    if (!changed)
        return unchanged(id);
    return updated(id, NodeFieldHops);
}

NodeMutation NodeStore::updateLastHeard(NodeId id, uint32_t now)
{
    auto it = nodes.find(id);
    if (it == nodes.end())
        return unchanged(id);
    if (it->second.lastHeard == now) {
        touchRecency(it->second);
        return updated(id, NodeFieldLastHeard);
    }
    it->second.lastHeard = now;
    touchRecency(it->second);
    return updated(id, NodeFieldLastHeard);
}

NodeMutation NodeStore::markBadKey(NodeId id)
{
    auto it = nodes.find(id);
    if (it == nodes.end() || it->second.hasBadKey)
        return unchanged(id);
    it->second.hasBadKey = true;
    return updated(id, NodeFieldFlags);
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

    const auto older = [now](const NodeRecord *left, const NodeRecord *right) {
        const uint32_t leftHeard = effectiveLastHeard(*left, now);
        const uint32_t rightHeard = effectiveLastHeard(*right, now);
        if (leftHeard != rightHeard)
            return leftHeard < rightHeard;
        if (left->recencyPromoted != right->recencyPromoted)
            return !left->recencyPromoted;
        if (left->recencyOrder != right->recencyOrder)
            return left->recencyPromoted ? left->recencyOrder < right->recencyOrder : left->recencyOrder > right->recencyOrder;
        return left->id < right->id;
    };

    const auto removable = [incoming, ownNode](const NodeRecord &record) {
        return record.id != incoming && record.id != ownNode && !record.hasActiveChat;
    };
    const auto staleUnknown = [now](const NodeRecord &record) {
        return !record.hasUser && now >= record.lastHeard && now - record.lastHeard >= purgeFreshnessSeconds;
    };

    const NodeRecord *oldestRemovable = nullptr;
    const NodeRecord *oldestStaleUnknown = nullptr;
    for (const auto &[id, record] : nodes) {
        if (removable(record)) {
            if (!oldestRemovable || older(&record, oldestRemovable)) {
                oldestRemovable = &record;
            }
            if (staleUnknown(record) && (!oldestStaleUnknown || older(&record, oldestStaleUnknown))) {
                oldestStaleUnknown = &record;
            }
        }
    }

    if (oldestStaleUnknown) {
        const size_t preferredPopulation = (nodes.size() * 4 + 4) / 5;
        size_t staleUnknownRank = 1;
        for (const auto &[id, record] : nodes) {
            if (older(&record, oldestStaleUnknown)) {
                ++staleUnknownRank;
            }
        }
        if (staleUnknownRank <= preferredPopulation) {
            return oldestStaleUnknown->id;
        }
    }

    return oldestRemovable ? oldestRemovable->id : 0;
}
