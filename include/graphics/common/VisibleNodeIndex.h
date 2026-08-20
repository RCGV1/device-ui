#pragma once

#include "graphics/common/NodeStore.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct NodeListFilter {
    bool unknown = false;
    bool offline = false;
    bool publicKey = false;
    uint8_t channel = 0; // 0 = all channels, 1..8 = channel + 1
    int hops = 0;        // 0 = all, 1..14 = dropdown index
    bool position = false;
    bool viaMqtt = false;
    std::string name;
    uint32_t curTime = 0;
    uint32_t secsUntilOffline = 900; // default 15 minutes
};

enum class NodeListFilterPolicy { LegacyCompatible };

class VisibleNodeIndex
{
  public:
    void rebuild(const NodeStore &store, const NodeListFilter &filter, NodeId ownNode, NodeListFilterPolicy policy);

    const std::vector<NodeId> &ids() const { return visibleIds; }
    size_t size() const { return visibleIds.size(); }
    bool empty() const { return visibleIds.empty(); }

    std::optional<size_t> indexOf(NodeId id) const;
    bool contains(NodeId id) const;

    static bool isVisible(const NodeRecord &node, const NodeListFilter &filter, NodeId ownNode, NodeListFilterPolicy policy);

  private:
    std::vector<NodeId> visibleIds;
};
