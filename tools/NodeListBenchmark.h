#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct NodeListBenchmarkOptions {
    size_t nodes;
    size_t trials;
    uint32_t seed;
    size_t warmup;
};

struct NodeListDurationSummary {
    std::vector<uint64_t> trials;
    uint64_t median = 0;
    uint64_t p95 = 0;
    uint64_t maximum = 0;
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
        NodeListDurationSummary filterNs;
    } timing;
    struct {
        bool ready = false;
        bool requestedNodeCount = false;
        bool duplicateUpdate = false;
        bool changedNameAndRole = false;
        bool capPurge = false;
        bool resyncPresentationPreservedNodes = false;
        bool offscreenUpdate = false;
        bool all = false;
    } correctness;
};

NodeListBenchmarkReport runNodeListBenchmark(const NodeListBenchmarkOptions &options);
bool writeNodeListBenchmarkJson(const NodeListBenchmarkReport &report, const std::string &path, std::string &error);
bool parseNodeListBenchmarkCommandLine(int argc, const char *const *argv, NodeListBenchmarkOptions &options,
                                       std::string &jsonPath);
