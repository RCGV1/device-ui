#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class NodeListBenchmarkImplementation {
    Legacy,
    VirtualCandidate,
};

struct NodeListBenchmarkOptions {
    size_t nodes;
    size_t trials;
    uint32_t seed;
    size_t warmup;
    NodeListBenchmarkImplementation implementation = NodeListBenchmarkImplementation::Legacy;
};

struct NodeListDurationSummary {
    std::vector<uint64_t> trials;
    uint64_t median = 0;
    uint64_t p95 = 0;
    uint64_t maximum = 0;
};

struct NodeListScrollTelemetry {
    size_t cycles = 0;
    size_t rowsPerCycle = 0;
    size_t sampleCount = 0;
    size_t frameCount = 0;
    uint64_t elapsedNs = 0;
    double averageFps = 0;
    uint64_t worstFrameNs = 0;
};

struct NodeListHeapInstrumentation {
    size_t newCount = 0;
    size_t newBytes = 0;
    size_t maxRssBytes = 0;
};

#ifdef UNIT_TEST
enum class NodeListBenchmarkRssUnit {
    Bytes,
    Kilobytes,
};
using NodeListBenchmarkMallocForTesting = void *(*)(size_t);
void *nodeListBenchmarkAllocateForNewForTesting(size_t size, NodeListBenchmarkMallocForTesting mallocFunction);
size_t nodeListBenchmarkRssBytesForTesting(long maxRss, NodeListBenchmarkRssUnit unit);
#endif

struct NodeListAllocatorSnapshot {
    size_t freeSize = 0;
    size_t biggestFreeBlock = 0;
    size_t usedCount = 0;
    uint8_t fragmentationPercent = 0;
};

struct NodeListAllocatorDelta {
    int64_t freeSize = 0;
    int64_t biggestFreeBlock = 0;
    int64_t usedCount = 0;
    int64_t fragmentationPercent = 0;
};

struct NodeListCandidateAllocatorTelemetry {
    struct Operation {
        std::string name;
        NodeListAllocatorSnapshot before;
        NodeListAllocatorSnapshot after;
        NodeListAllocatorDelta delta;
        NodeListAllocatorSnapshot peak;
        std::vector<NodeListAllocatorSnapshot> snapshots;
        size_t objectsBefore = 0;
        size_t objectsAfter = 0;
        size_t nodeListObjectsBefore = 0;
        size_t nodeListObjectsAfter = 0;
        bool objectCountStable = false;
        bool allocatorUsedCountBounded = false;
    };

    struct Trial {
        size_t iteration = 0;
        bool warmup = false;
        size_t cycleCount = 0;
        NodeListAllocatorSnapshot before;
        std::vector<NodeListAllocatorSnapshot> syncSnapshots;
        NodeListAllocatorSnapshot after;
        NodeListAllocatorDelta delta;
        NodeListAllocatorSnapshot peak;
        std::vector<Operation> operations;
        size_t objectsBefore = 0;
        size_t objectsAfter = 0;
        size_t nodeListObjectsBefore = 0;
        size_t nodeListObjectsAfter = 0;
        size_t retainedNodesBefore = 0;
        size_t retainedNodesAfter = 0;
        size_t nodeStoreSizeBefore = 0;
        size_t nodeStoreSizeAfter = 0;
        size_t nodeCountBefore = 0;
        size_t nodeCountAfter = 0;
        bool objectCountStable = false;
        bool retainedNodesStable = false;
        bool nodeStoreSizeStable = false;
        bool nodeCountStable = false;
        bool allocatorUsedCountBounded = false;
        bool terminalStable = false;
    };

    std::string meaning;
    std::vector<Trial> trials;
};

struct NodeListBenchmarkReport {
    struct UnsupportedFixture {
        std::string name;
        std::string reason;
    };
    struct {
        std::string revision;
        bool dirty = false;
    } source;
    struct {
        std::string name;
        std::string comparisonScope;
    } implementation;
    struct {
        std::string version;
        size_t totalObjects = 0;
        size_t nodeListObjects = 0;
    } lvgl;
    struct {
        std::string implementation;
        size_t configuredBytes = 0;
        size_t ramSizeBytes = 0;
    } allocator;
    struct {
        size_t totalSize = 0;
        size_t freeCount = 0;
        size_t freeSize = 0;
        size_t biggestFreeBlock = 0;
        size_t usedCount = 0;
        size_t maxUsed = 0;
        uint8_t usedPercent = 0;
        uint8_t fragmentationPercent = 0;
        bool integrityOk = false;
    } memory;
    std::optional<NodeListCandidateAllocatorTelemetry> allocatorTelemetry;
    NodeListHeapInstrumentation heap;
    struct {
        size_t nodes = 0;
        size_t trials = 0;
        uint32_t seed = 0;
        size_t warmup = 0;
    } scenario;
    struct {
        std::vector<std::string> supported;
        std::vector<UnsupportedFixture> unsupported;
    } fixtures;
    struct {
        NodeListDurationSummary insertNs;
        NodeListDurationSummary updateNs;
        NodeListDurationSummary reorderInsertNs;
        std::optional<NodeListDurationSummary> filterNs;
        std::optional<NodeListDurationSummary> virtualRefreshNs;
    } timing;
    NodeListScrollTelemetry scrollTelemetry;
    struct {
        bool ready = false;
        bool requestedNodeCount = false;
        bool duplicateUpdate = false;
        bool changedNameAndRole = false;
        bool capPurge = false;
        bool resyncPresentationPreservedNodes = false;
        bool offscreenUpdate = false;
        struct Candidate {
            bool poolBounded = false;
            bool objectCountStable = false;
            bool allocatorChurnBounded = false;
            bool presentation = false;
        };
        std::optional<Candidate> candidate;
        bool all = false;
    } correctness;
};

NodeListBenchmarkReport runNodeListBenchmark(const NodeListBenchmarkOptions &options);
bool writeNodeListBenchmarkJson(const NodeListBenchmarkReport &report, const std::string &path, std::string &error);
bool parseNodeListBenchmarkCommandLine(int argc, const char *const *argv, NodeListBenchmarkOptions &options,
                                       std::string &jsonPath);
