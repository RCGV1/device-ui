#pragma once

#include "meshtastic/mesh.pb.h"
#include "meshtastic/telemetry.pb.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

using NodeId = uint32_t;

struct NodePosition {
    bool known = false;
    int32_t latitude = 0;
    int32_t longitude = 0;
    int32_t altitude = 0;
    uint32_t satellites = 0;
    uint32_t precision = 0;
};

struct NodeRecord {
    NodeId id = 0;
    uint8_t channel = 0;
    bool hasUser = false;
    meshtastic_User user{};
    bool hasKey = false;
    bool unmessagable = false;
    bool viaMqtt = false;
    uint32_t lastHeard = 0;
    int32_t rssi = 0;
    float snr = 0;
    int8_t hopsAway = -1;
    NodePosition position{};
    bool hasDeviceMetrics = false;
    meshtastic_DeviceMetrics deviceMetrics{};
    bool hasEnvironmentMetrics = false;
    meshtastic_EnvironmentMetrics environmentMetrics{};
    bool hasAirQualityMetrics = false;
    meshtastic_AirQualityMetrics airQualityMetrics{};
    bool hasActiveChat = false;
};

enum class NodeMutationKind { Inserted, Updated, Removed, Unchanged };

enum NodeChangedField : uint32_t {
    NodeFieldNone = 0,
    NodeFieldUser = 1U << 0,
    NodeFieldChannel = 1U << 1,
    NodeFieldFlags = 1U << 2,
    NodeFieldLastHeard = 1U << 3,
    NodeFieldPosition = 1U << 4,
    NodeFieldDeviceMetrics = 1U << 5,
    NodeFieldEnvironmentMetrics = 1U << 6,
    NodeFieldAirQualityMetrics = 1U << 7,
    NodeFieldSignal = 1U << 8,
    NodeFieldHops = 1U << 9,
    NodeFieldActiveChat = 1U << 10,
};

struct NodeMutation {
    NodeMutationKind kind = NodeMutationKind::Unchanged;
    NodeId id = 0;
    uint32_t changedFields = NodeFieldNone;
};

class NodeStore
{
  public:
    using Records = std::unordered_map<NodeId, NodeRecord>;

    const NodeRecord *find(NodeId id) const;
    const Records &records() const { return nodes; }
    size_t size() const { return nodes.size(); }

    NodeMutation upsertUser(NodeId id, uint8_t channel, uint32_t lastHeard, const meshtastic_User &user, bool viaMqtt);
    NodeMutation upsertUnknown(NodeId id, uint8_t channel, uint32_t lastHeard, uint8_t role, bool hasKey, bool viaMqtt);
    NodeMutation updatePosition(NodeId id, const NodePosition &position);
    NodeMutation updateDeviceMetrics(NodeId id, const meshtastic_DeviceMetrics &metrics);
    NodeMutation updateEnvironmentMetrics(NodeId id, const meshtastic_EnvironmentMetrics &metrics);
    NodeMutation updateAirQualityMetrics(NodeId id, const meshtastic_AirQualityMetrics &metrics);
    NodeMutation updateSignal(NodeId id, int32_t rssi, float snr);
    NodeMutation updateHops(NodeId id, int8_t hopsAway);
    NodeMutation updateLastHeard(NodeId id, uint32_t now);
    NodeMutation setActiveChat(NodeId id, bool active);
    NodeMutation remove(NodeId id);

    NodeId selectPurgeCandidate(NodeId incoming, NodeId ownNode) const;
    NodeId selectPurgeCandidate(NodeId incoming, NodeId ownNode, uint32_t now) const;

  private:
    Records nodes;
};
