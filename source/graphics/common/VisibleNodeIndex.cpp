#include "graphics/common/VisibleNodeIndex.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace
{

bool containsCaseInsensitive(const char *haystack, const std::string &needle)
{
    if (!haystack || needle.empty()) {
        return false;
    }
    auto it = std::search(haystack, haystack + std::strlen(haystack), needle.begin(), needle.end(), [](char ch1, char ch2) {
        return std::tolower(static_cast<unsigned char>(ch1)) == std::tolower(static_cast<unsigned char>(ch2));
    });
    return it != (haystack + std::strlen(haystack));
}

} // namespace

bool VisibleNodeIndex::isVisible(const NodeRecord &node, const NodeListFilter &filter, NodeId ownNode)
{
    // Own node is never hidden by filters in MUI
    if (node.id == ownNode && ownNode != 0) {
        return true;
    }

    // Filter Unknown: hide if no user data or unknown
    if (filter.unknown) {
        if (!node.hasUser) {
            return false;
        }
    }

    // Filter Offline: hide if lastHeard is 0 or older than secsUntilOffline
    if (filter.offline) {
        if (node.lastHeard == 0) {
            return false;
        }
        if (filter.curTime > 0 && filter.curTime >= node.lastHeard) {
            if ((filter.curTime - node.lastHeard) > filter.secsUntilOffline) {
                return false;
            }
        }
    }

    // Filter Public Key: hide if no public key
    if (filter.publicKey) {
        if (!node.hasKey) {
            return false;
        }
    }

    // Filter Channel: dropdown index 0 = all, >0 = channel index (selected - 1)
    if (filter.channel != 0) {
        uint8_t targetChannel = filter.channel - 1;
        if (node.channel != targetChannel) {
            return false;
        }
    }

    // Filter Hops: dropdown index 0 = all
    if (filter.hops != 0) {
        int selected = filter.hops - 7;
        if (node.hopsAway < 0) {
            return false;
        }
        if (selected <= 0) {
            if (node.hopsAway > -selected) {
                return false;
            }
        } else {
            if (node.hopsAway < selected) {
                return false;
            }
        }
    }

    // Filter MQTT
    if (filter.viaMqtt) {
        if (!node.viaMqtt) {
            return false;
        }
    }

    // Filter Position: require known coordinates
    if (filter.position) {
        if (!node.position.known || (node.position.latitude == 0 && node.position.longitude == 0)) {
            return false;
        }
    }

    // Filter Name
    if (!filter.name.empty()) {
        char hexIdBuf[16];
        std::snprintf(hexIdBuf, sizeof(hexIdBuf), "%04x", static_cast<unsigned int>(node.id & 0xffff));
        char fullHexBuf[16];
        std::snprintf(fullHexBuf, sizeof(fullHexBuf), "%08x", static_cast<unsigned int>(node.id));

        auto matchesAny = [&](const std::string &query) {
            return containsCaseInsensitive(node.user.long_name, query) || containsCaseInsensitive(node.user.short_name, query) ||
                   containsCaseInsensitive(node.user.id, query) || containsCaseInsensitive(hexIdBuf, query) ||
                   containsCaseInsensitive(fullHexBuf, query);
        };

        if (filter.name[0] != '!') {
            if (!matchesAny(filter.name)) {
                return false;
            }
        } else {
            std::string negated = filter.name.substr(1);
            if (!negated.empty() && matchesAny(negated)) {
                return false;
            }
        }
    }

    return true;
}

void VisibleNodeIndex::rebuild(const NodeStore &store, const NodeListFilter &filter, NodeId ownNode)
{
    visibleIds.clear();
    visibleIds.reserve(store.size());

    for (const auto &pair : store.records()) {
        const auto &record = pair.second;
        if (isVisible(record, filter, ownNode)) {
            visibleIds.push_back(record.id);
        }
    }

    // Sort by lastHeard descending, tie-breaker: NodeId ascending
    std::sort(visibleIds.begin(), visibleIds.end(), [&store](NodeId a, NodeId b) {
        const auto *recA = store.find(a);
        const auto *recB = store.find(b);
        uint32_t lhA = recA ? recA->lastHeard : 0;
        uint32_t lhB = recB ? recB->lastHeard : 0;
        if (lhA != lhB) {
            return lhA > lhB;
        }
        return a < b;
    });
}

std::optional<size_t> VisibleNodeIndex::indexOf(NodeId id) const
{
    for (size_t i = 0; i < visibleIds.size(); ++i) {
        if (visibleIds[i] == id) {
            return i;
        }
    }
    return std::nullopt;
}

bool VisibleNodeIndex::contains(NodeId id) const
{
    return indexOf(id).has_value();
}
