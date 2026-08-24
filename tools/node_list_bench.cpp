#include "NodeListBenchmark.h"

#ifdef NODE_LIST_BENCH_EXECUTABLE
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
#endif

#include "MuiTestHarness.h"
#include "graphics/common/MeshtasticView.h"
#include "graphics/view/TFT/VirtualNodeList.h"
#include "lvgl.h"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <random>
#include <sstream>
#include <string_view>
#include <sys/resource.h>
#include <utility>

#ifndef DEVICE_UI_SOURCE_REVISION
#define DEVICE_UI_SOURCE_REVISION "unknown"
#endif

#ifndef DEVICE_UI_SOURCE_DIRTY
#define DEVICE_UI_SOURCE_DIRTY 0
#endif

namespace
{
using Clock = std::chrono::steady_clock;

enum class RusageMaxRssUnit {
    Bytes,
    Kilobytes,
};

size_t rssBytesFromRusage(long maxRss, RusageMaxRssUnit unit)
{
    if (maxRss <= 0) {
        return 0;
    }
    const auto value = static_cast<size_t>(maxRss);
    return unit == RusageMaxRssUnit::Bytes ? value : value * 1024U;
}

size_t currentMaxRssBytes()
{
    struct rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
#ifdef __APPLE__
    return rssBytesFromRusage(usage.ru_maxrss, RusageMaxRssUnit::Bytes);
#else
    return rssBytesFromRusage(usage.ru_maxrss, RusageMaxRssUnit::Kilobytes);
#endif
}

using MallocFunction = void *(*)(std::size_t);

void *allocateForBenchmarkNew(std::size_t size, MallocFunction mallocFunction)
{
    void *ptr = mallocFunction(size ? size : 1);
    if (!ptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

#if defined(NODE_LIST_BENCH_EXECUTABLE)
struct HeapAccounting {
    std::atomic<size_t> count{0};
    std::atomic<size_t> bytes{0};
};
HeapAccounting &heapAccounting()
{
    static HeapAccounting accounting;
    return accounting;
}
} // namespace
void *operator new(std::size_t size)
{
    void *ptr = allocateForBenchmarkNew(size, std::malloc);
    heapAccounting().count.fetch_add(1, std::memory_order_relaxed);
    heapAccounting().bytes.fetch_add(size, std::memory_order_relaxed);
    return ptr;
}
void operator delete(void *ptr) noexcept
{
    std::free(ptr);
}
void operator delete(void *ptr, std::size_t) noexcept
{
    std::free(ptr);
}
namespace
{
#else
#endif

struct NodeFixture {
    uint32_t id;
    std::string shortName;
    std::string longName;
    uint32_t lastHeard;
    uint8_t role;
    bool hasKey;
    bool unmessagable;
    uint8_t channel;
    bool hasPosition;
    bool hasTelemetry;
};

constexpr const char *comparisonScope =
    "Host-relative structural/CPU/allocator/LVGL-heap comparison with C++ new-delimiter counters and peak RSS; "
    "not hardware timing. The X11 simulator pair is a functional parity smoke only: its cadence includes "
    "deliberate sleep frames and must never be read as a performance comparison.";
constexpr size_t allocatorUsedCountSlack = 8;
constexpr size_t measuredCycleCount = 2;

const char *implementationName(NodeListBenchmarkImplementation implementation)
{
    return implementation == NodeListBenchmarkImplementation::VirtualCandidate ? "virtual_candidate" : "legacy";
}

class BenchmarkActionSink : public NodeListActionSink
{
  public:
    void nodeClicked(NodeId) override {}
    void nodeLongPressed(NodeId) override {}
    void nodeFocused(NodeId) override {}
};

template <typename Function> uint64_t measureNs(Function &&function)
{
    const auto start = Clock::now();
    function();
    const auto finish = Clock::now();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count());
}

template <typename Function> void recordScrollFrame(NodeListScrollTelemetry &telemetry, Function &&function)
{
    const uint64_t elapsed = measureNs(std::forward<Function>(function));
    telemetry.elapsedNs += elapsed;
    telemetry.sampleCount++;
    telemetry.frameCount++;
    telemetry.worstFrameNs = std::max(telemetry.worstFrameNs, elapsed);
}

void mergeScrollTelemetry(NodeListScrollTelemetry &total, const NodeListScrollTelemetry &trial)
{
    total.cycles += trial.cycles;
    total.rowsPerCycle = trial.rowsPerCycle;
    total.sampleCount += trial.sampleCount;
    total.frameCount += trial.frameCount;
    total.elapsedNs += trial.elapsedNs;
    total.worstFrameNs = std::max(total.worstFrameNs, trial.worstFrameNs);
}

void finalizeScrollTelemetry(NodeListScrollTelemetry &telemetry)
{
    if (telemetry.elapsedNs != 0) {
        telemetry.averageFps =
            static_cast<double>(telemetry.frameCount) * 1'000'000'000.0 / static_cast<double>(telemetry.elapsedNs);
    }
}

std::vector<NodeFixture> makeFixtures(size_t count, uint32_t seed, size_t iteration)
{
    constexpr uint32_t now = 1700000000U;
    std::mt19937 random(seed + static_cast<uint32_t>(iteration * 0x9e3779b9U));
    std::vector<NodeFixture> fixtures;
    fixtures.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        std::ostringstream shortName;
        shortName << 'N' << std::hex << ((i + random()) & 0xfffU);
        std::string shortValue = shortName.str().substr(0, 4);
        while (shortValue.size() < 4) {
            shortValue.push_back('0');
        }

        std::string longName;
        switch (i % 6) {
        case 0:
            longName.clear();
            break;
        case 1:
            longName = "Typical Node " + std::to_string(i);
            break;
        case 2:
            longName = "Málaga UTF-8 " + std::to_string(i);
            break;
        case 3:
            longName = "Satellite 🛰 " + std::to_string(i);
            break;
        case 4:
            longName = "123456789012345678901234567890123456789";
            break;
        default:
            longName = "Node " + std::to_string(i);
            break;
        }

        const bool offline = i % 4 == 0;
        fixtures.push_back({
            static_cast<uint32_t>(0xa0000000U + (iteration << 12U) + i),
            shortValue,
            longName,
            offline ? now - 7200U - static_cast<uint32_t>(i) : now - static_cast<uint32_t>(i),
            static_cast<uint8_t>(i % 5 == 0 ? MeshtasticView::unknown : i % 7),
            i % 2 == 0,
            i % 11 == 0,
            static_cast<uint8_t>(i % 8),
            i % 3 == 0,
            i % 4 == 1,
        });
    }
    return fixtures;
}

void insertFixtures(MuiTestHarness &harness, const std::vector<NodeFixture> &fixtures)
{
    for (const auto &fixture : fixtures) {
        harness.addNodeFixture(fixture.id, fixture.shortName.c_str(), fixture.longName.c_str(), fixture.lastHeard, fixture.role,
                               fixture.hasKey, fixture.unmessagable, fixture.channel);
    }
    harness.pump();
}

void applyMixedState(MuiTestHarness &harness, const std::vector<NodeFixture> &fixtures)
{
    for (size_t i = 0; i < fixtures.size(); ++i) {
        const auto &fixture = fixtures[i];
        harness.updateHopsFixture(fixture.id, static_cast<uint8_t>((i % 6) + 1));
        if (fixture.hasPosition) {
            harness.updatePositionFixture(fixture.id, 374221234 + static_cast<int32_t>(i), -1220845678 + static_cast<int32_t>(i),
                                          10 + static_cast<int32_t>(i), 7, 16);
        }
        if (fixture.hasTelemetry) {
            harness.updateTelemetryFixture(fixture.id, 20.0F + static_cast<float>(i % 10), 45.0F, 1013.2F,
                                           static_cast<uint16_t>(50 + i % 100));
        }
        if (i % 5 == 0) {
            harness.updateMetricsFixture(fixture.id, static_cast<uint32_t>(40 + i % 60), 3.8F, 12.0F, 1.0F);
        }
    }
    if (!fixtures.empty()) {
        harness.setActiveChatModelFixture(fixtures.front().id);
    }
    harness.pump();
}

meshtastic_User makeUser(const NodeFixture &fixture, const std::string &longName = {})
{
    meshtastic_User user = meshtastic_User_init_default;
    std::strncpy(user.short_name, fixture.shortName.c_str(), sizeof(user.short_name) - 1);
    std::strncpy(user.long_name, longName.empty() ? fixture.longName.c_str() : longName.c_str(), sizeof(user.long_name) - 1);
    user.role = static_cast<meshtastic_Config_DeviceConfig_Role>(fixture.role);
    user.public_key.size = fixture.hasKey ? 1 : 0;
    user.has_is_unmessagable = fixture.unmessagable;
    user.is_unmessagable = fixture.unmessagable;
    return user;
}

void applyMixedState(NodeStore &store, const std::vector<NodeFixture> &fixtures)
{
    for (size_t i = 0; i < fixtures.size(); ++i) {
        const auto &fixture = fixtures[i];
        store.updateHops(fixture.id, static_cast<int8_t>((i % 6) + 1));
        if (fixture.hasPosition) {
            store.updatePosition(fixture.id, {true, 374221234 + static_cast<int32_t>(i), -1220845678 + static_cast<int32_t>(i),
                                              10 + static_cast<int32_t>(i), 7, 16});
        }
        if (fixture.hasTelemetry) {
            meshtastic_EnvironmentMetrics metrics = meshtastic_EnvironmentMetrics_init_default;
            metrics.has_temperature = true;
            metrics.temperature = 20.0F + static_cast<float>(i % 10);
            metrics.has_relative_humidity = true;
            metrics.relative_humidity = 45.0F;
            metrics.has_barometric_pressure = true;
            metrics.barometric_pressure = 1013.2F;
            metrics.has_iaq = true;
            metrics.iaq = static_cast<uint16_t>(50 + i % 100);
            store.updateEnvironmentMetrics(fixture.id, metrics);
        }
        if (i % 5 == 0) {
            meshtastic_DeviceMetrics metrics = meshtastic_DeviceMetrics_init_default;
            metrics.has_battery_level = true;
            metrics.battery_level = static_cast<uint32_t>(40 + i % 60);
            metrics.has_voltage = true;
            metrics.voltage = 3.8F;
            metrics.has_channel_utilization = true;
            metrics.channel_utilization = 12.0F;
            metrics.has_air_util_tx = true;
            metrics.air_util_tx = 1.0F;
            store.updateDeviceMetrics(fixture.id, metrics);
        }
    }
    if (!fixtures.empty()) {
        store.setActiveChat(fixtures.front().id, true);
    }
}

bool hasPresentedNode(lv_obj_t *parent, NodeId id, const char *name)
{
    const uint32_t childCount = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < childCount; ++i) {
        lv_obj_t *row = lv_obj_get_child(parent, static_cast<int32_t>(i));
        if (static_cast<NodeId>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(row))) != id ||
            lv_obj_has_flag(row, LV_OBJ_FLAG_HIDDEN)) {
            continue;
        }
        const uint32_t rowChildCount = lv_obj_get_child_count(row);
        for (uint32_t j = 0; j < rowChildCount; ++j) {
            lv_obj_t *child = lv_obj_get_child(row, static_cast<int32_t>(j));
            if (lv_obj_check_type(child, &lv_label_class) && std::strcmp(lv_label_get_text(child), name) == 0) {
                return true;
            }
        }
    }
    return false;
}

NodeListDurationSummary summarize(std::vector<uint64_t> values)
{
    NodeListDurationSummary summary;
    summary.trials = std::move(values);
    if (summary.trials.empty()) {
        return summary;
    }

    auto sorted = summary.trials;
    std::sort(sorted.begin(), sorted.end());
    summary.median = sorted[sorted.size() / 2];
    const size_t p95Index = static_cast<size_t>(std::ceil(static_cast<double>(sorted.size()) * 0.95)) - 1;
    summary.p95 = sorted[p95Index];
    summary.maximum = sorted.back();
    return summary;
}

NodeListAllocatorSnapshot captureAllocatorSnapshot()
{
    lv_mem_monitor_t memory{};
    lv_mem_monitor(&memory);
    return {memory.free_size, memory.free_biggest_size, memory.used_cnt, memory.frag_pct};
}

int64_t allocatorDelta(size_t after, size_t before)
{
    return after >= before ? static_cast<int64_t>(after - before) : -static_cast<int64_t>(before - after);
}

NodeListAllocatorDelta allocatorDelta(const NodeListAllocatorSnapshot &before, const NodeListAllocatorSnapshot &after)
{
    return {allocatorDelta(after.freeSize, before.freeSize), allocatorDelta(after.biggestFreeBlock, before.biggestFreeBlock),
            allocatorDelta(after.usedCount, before.usedCount),
            static_cast<int64_t>(after.fragmentationPercent) - static_cast<int64_t>(before.fragmentationPercent)};
}

void observeAllocatorSnapshot(NodeListAllocatorSnapshot &peak, const NodeListAllocatorSnapshot &snapshot)
{
    peak.freeSize = std::min(peak.freeSize, snapshot.freeSize);
    peak.biggestFreeBlock = std::min(peak.biggestFreeBlock, snapshot.biggestFreeBlock);
    peak.usedCount = std::max(peak.usedCount, snapshot.usedCount);
    peak.fragmentationPercent = std::max(peak.fragmentationPercent, snapshot.fragmentationPercent);
}

bool retainedAllocatorBlockCountIsBounded(const NodeListAllocatorSnapshot &before, const NodeListAllocatorSnapshot &after)
{
    return after.usedCount <= before.usedCount + allocatorUsedCountSlack;
}

void observeOperationPeak(NodeListCandidateAllocatorTelemetry::Operation &operation, const NodeListAllocatorSnapshot &snapshot)
{
    operation.snapshots.push_back(snapshot);
    observeAllocatorSnapshot(operation.peak, snapshot);
}

template <typename Function>
uint64_t recordOperation(NodeListCandidateAllocatorTelemetry::Trial &trial, MuiTestHarness &harness, const char *name,
                         Function &&function)
{
    NodeListCandidateAllocatorTelemetry::Operation operation;
    operation.name = name;
    operation.before = captureAllocatorSnapshot();
    operation.peak = operation.before;
    operation.objectsBefore = harness.objectCount();
    operation.nodeListObjectsBefore = harness.nodeListObjectCount();
    const uint64_t elapsed = measureNs([&] { function(operation); });
    operation.after = captureAllocatorSnapshot();
    operation.delta = allocatorDelta(operation.before, operation.after);
    observeOperationPeak(operation, operation.after);
    operation.objectsAfter = harness.objectCount();
    operation.nodeListObjectsAfter = harness.nodeListObjectCount();
    operation.objectCountStable =
        operation.objectsBefore == operation.objectsAfter && operation.nodeListObjectsBefore == operation.nodeListObjectsAfter;
    operation.allocatorUsedCountBounded = retainedAllocatorBlockCountIsBounded(operation.before, operation.after);
    trial.operations.push_back(std::move(operation));
    return elapsed;
}

bool trialOperationsAreStable(const NodeListCandidateAllocatorTelemetry::Trial &trial)
{
    return std::all_of(trial.operations.begin(), trial.operations.end(),
                       [](const auto &operation) { return operation.objectCountStable && operation.allocatorUsedCountBounded; });
}

void captureTerminalBefore(NodeListCandidateAllocatorTelemetry::Trial &trial, const MuiTestHarness &harness, size_t retainedNodes,
                           size_t nodeStoreSize, size_t nodeCount)
{
    trial.objectsBefore = harness.objectCount();
    trial.nodeListObjectsBefore = harness.nodeListObjectCount();
    trial.retainedNodesBefore = retainedNodes;
    trial.nodeStoreSizeBefore = nodeStoreSize;
    trial.nodeCountBefore = nodeCount;
}

void captureTerminalAfter(NodeListCandidateAllocatorTelemetry::Trial &trial, const MuiTestHarness &harness, size_t retainedNodes,
                          size_t nodeStoreSize, size_t nodeCount)
{
    trial.objectsAfter = harness.objectCount();
    trial.nodeListObjectsAfter = harness.nodeListObjectCount();
    trial.retainedNodesAfter = retainedNodes;
    trial.nodeStoreSizeAfter = nodeStoreSize;
    trial.nodeCountAfter = nodeCount;
    trial.objectCountStable =
        trial.objectsBefore == trial.objectsAfter && trial.nodeListObjectsBefore == trial.nodeListObjectsAfter;
    trial.retainedNodesStable = trial.retainedNodesBefore == trial.retainedNodesAfter;
    trial.nodeStoreSizeStable = trial.nodeStoreSizeBefore == trial.nodeStoreSizeAfter;
    trial.nodeCountStable = trial.nodeCountBefore == trial.nodeCountAfter;
    trial.allocatorUsedCountBounded = retainedAllocatorBlockCountIsBounded(trial.before, trial.after);
    trial.terminalStable = trial.objectCountStable && trial.retainedNodesStable && trial.nodeStoreSizeStable &&
                           trial.nodeCountStable && trial.allocatorUsedCountBounded;
}

bool trialTerminalIsStable(const NodeListCandidateAllocatorTelemetry::Trial &trial)
{
    return trial.terminalStable;
}

std::string escapeJson(std::string_view value)
{
    std::ostringstream output;
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (character < 0x20) {
                constexpr char hex[] = "0123456789abcdef";
                output << "\\u00" << hex[character >> 4U] << hex[character & 0x0fU];
            } else {
                output << character;
            }
            break;
        }
    }
    return output.str();
}

void writeDuration(std::ostream &output, const NodeListDurationSummary &summary, size_t indent)
{
    const std::string padding(indent, ' ');
    output << "{\n" << padding << "  \"trials\": [";
    for (size_t i = 0; i < summary.trials.size(); ++i) {
        output << (i == 0 ? "" : ", ") << summary.trials[i];
    }
    output << "],\n" << padding << "  \"raw_ns\": [";
    for (size_t i = 0; i < summary.trials.size(); ++i) {
        output << (i == 0 ? "" : ", ") << summary.trials[i];
    }
    output << "],\n"
           << padding << "  \"median\": " << summary.median << ",\n"
           << padding << "  \"p95\": " << summary.p95 << ",\n"
           << padding << "  \"maximum\": " << summary.maximum << "\n"
           << padding << '}';
}

void writeOptionalDuration(std::ostream &output, const std::optional<NodeListDurationSummary> &summary, size_t indent)
{
    if (summary.has_value()) {
        writeDuration(output, summary.value(), indent);
    } else {
        output << "null";
    }
}

void writeAllocatorSnapshot(std::ostream &output, const NodeListAllocatorSnapshot &snapshot)
{
    output << "{\"free_size\": " << snapshot.freeSize << ", \"biggest_free_block\": " << snapshot.biggestFreeBlock
           << ", \"used_count\": " << snapshot.usedCount
           << ", \"fragmentation_percent\": " << static_cast<unsigned>(snapshot.fragmentationPercent) << '}';
}

void writeAllocatorDelta(std::ostream &output, const NodeListAllocatorDelta &delta)
{
    output << "{\"free_size\": " << delta.freeSize << ", \"biggest_free_block\": " << delta.biggestFreeBlock
           << ", \"used_count\": " << delta.usedCount << ", \"fragmentation_percent\": " << delta.fragmentationPercent << '}';
}

bool parseUnsigned(std::string_view value, uint64_t &parsed)
{
    if (value.empty()) {
        return false;
    }
    const char *begin = value.data();
    const char *end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    return result.ec == std::errc{} && result.ptr == end;
}

#ifdef NODE_LIST_BENCH_EXECUTABLE
void printUsage(std::ostream &output)
{
    output << "usage: node_list_bench --nodes 1|25|100|250 --trials N --seed N "
              "[--implementation legacy|virtual_candidate] --json PATH\n";
}
#endif
} // namespace

bool parseNodeListBenchmarkCommandLine(int argc, const char *const *argv, NodeListBenchmarkOptions &options,
                                       std::string &jsonPath)
{
    options = {0, 0, 0, 1, NodeListBenchmarkImplementation::Legacy};
    jsonPath.clear();
    bool sawNodes = false;
    bool sawTrials = false;
    bool sawSeed = false;
    bool sawJson = false;
    bool sawImplementation = false;

    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) {
            return false;
        }
        const std::string_view flag(argv[i]);
        const std::string_view value(argv[i + 1]);
        uint64_t parsed = 0;
        if (flag == "--nodes" && !sawNodes && parseUnsigned(value, parsed) &&
            (parsed == 1 || parsed == 25 || parsed == 100 || parsed == 250)) {
            options.nodes = static_cast<size_t>(parsed);
            sawNodes = true;
        } else if (flag == "--trials" && !sawTrials && parseUnsigned(value, parsed) && parsed > 0 &&
                   parsed <= std::numeric_limits<size_t>::max() - options.warmup) {
            options.trials = static_cast<size_t>(parsed);
            sawTrials = true;
        } else if (flag == "--seed" && !sawSeed && parseUnsigned(value, parsed) &&
                   parsed <= std::numeric_limits<uint32_t>::max()) {
            options.seed = static_cast<uint32_t>(parsed);
            sawSeed = true;
        } else if (flag == "--implementation" && !sawImplementation && (value == "legacy" || value == "virtual_candidate")) {
            options.implementation = value == "virtual_candidate" ? NodeListBenchmarkImplementation::VirtualCandidate
                                                                  : NodeListBenchmarkImplementation::Legacy;
            sawImplementation = true;
        } else if (flag == "--json" && !sawJson && !value.empty()) {
            jsonPath = std::string(value);
            sawJson = true;
        } else {
            return false;
        }
    }

    return sawNodes && sawTrials && sawSeed && sawJson;
}

#ifdef UNIT_TEST
void *nodeListBenchmarkAllocateForNewForTesting(size_t size, NodeListBenchmarkMallocForTesting mallocFunction)
{
    return allocateForBenchmarkNew(size, mallocFunction);
}

size_t nodeListBenchmarkRssBytesForTesting(long maxRss, NodeListBenchmarkRssUnit unit)
{
    return rssBytesFromRusage(maxRss,
                              unit == NodeListBenchmarkRssUnit::Bytes ? RusageMaxRssUnit::Bytes : RusageMaxRssUnit::Kilobytes);
}
#endif

namespace
{
NodeListBenchmarkReport runVirtualCandidateBenchmark(const NodeListBenchmarkOptions &options, NodeListBenchmarkReport report)
{
    MuiTestHarness harness;
    report.correctness.ready = harness.ready();
    if (!report.correctness.ready) {
        return report;
    }

    harness.resetNodeList();
    lv_obj_t *container = lv_obj_create(harness.nodeListRootForTesting());
    lv_obj_set_size(container, 320, 240);
    BenchmarkActionSink sink;
    auto list = std::make_unique<VirtualNodeList>(container, sink);
    const size_t pooledObjectCount = harness.objectCount();
    const size_t pooledNodeListObjectCount = harness.nodeListObjectCount();

    std::vector<uint64_t> insertSamples;
    std::vector<uint64_t> updateSamples;
    std::vector<uint64_t> reorderSamples;
    std::vector<uint64_t> filterSamples;
    std::vector<uint64_t> virtualRefreshSamples;
    NodeListScrollTelemetry scrollTelemetry;
    bool nodeCountOk = true;
    bool duplicateUpdateOk = true;
    bool changedNameAndRoleOk = true;
    bool capPurgeOk = true;
    bool resyncPresentationOk = true;
    bool offscreenUpdateOk = true;
    bool poolBoundedOk = list->boundRowCount() == VirtualNodeList::POOL_SIZE;
    bool objectCountStableOk = true;
    bool allocatorChurnBoundedOk = true;
    bool presentationOk = true;
    NodeListCandidateAllocatorTelemetry allocatorTelemetry;
    allocatorTelemetry.meaning =
        "LVGL allocator snapshots captured after the virtual row pool is created. Each trial records its before and after "
        "snapshots plus every post-sync snapshot. Peak reports the worst observed values in that trial: minimum free_size "
        "and biggest_free_block, maximum used_count and fragmentation_percent. allocator_churn_bounded is true only "
        "when every non-warmup trial and operation remains within the small used-block slack for its sync/rebind sequence. "
        "Post-sync snapshots remain diagnostic because active label-scroll animations are transient allocations.";

    const size_t iterations = options.warmup + options.trials;
    for (size_t iteration = 0; iteration < iterations; ++iteration) {
        NodeListCandidateAllocatorTelemetry::Trial allocatorTrial;
        allocatorTrial.iteration = iteration;
        allocatorTrial.warmup = iteration < options.warmup;
        auto fixtures = makeFixtures(options.nodes, options.seed, iteration);
        harness.setCurrentTime(1700000000U);
        NodeStore store;
        VisibleNodeIndex index;
        NodeListFilter filter;

        auto sync = [&](NodeListCandidateAllocatorTelemetry::Operation *operation = nullptr) {
            index.rebuild(store, filter, 0, NodeListFilterPolicy::LegacyCompatible);
            list->sync(store, index, 0, 1700000000U);
            harness.pump();
            objectCountStableOk = objectCountStableOk && harness.objectCount() == pooledObjectCount &&
                                  harness.nodeListObjectCount() == pooledNodeListObjectCount;
            const auto snapshot = captureAllocatorSnapshot();
            allocatorTrial.syncSnapshots.push_back(snapshot);
            observeAllocatorSnapshot(allocatorTrial.peak, snapshot);
            if (operation) {
                observeOperationPeak(*operation, snapshot);
            }
        };
        auto ordered = fixtures;
        std::sort(ordered.begin(), ordered.end(),
                  [](const NodeFixture &left, const NodeFixture &right) { return left.lastHeard > right.lastHeard; });
        for (const auto &fixture : ordered) {
            store.upsertUser(fixture.id, fixture.channel, fixture.lastHeard, makeUser(fixture), false);
        }
        sync();
        allocatorTrial.syncSnapshots.clear();
        allocatorTrial.before = captureAllocatorSnapshot();
        allocatorTrial.peak = allocatorTrial.before;
        captureTerminalBefore(allocatorTrial, harness, index.ids().size(), store.size(), index.ids().size());

        uint64_t insertNs = 0;
        uint64_t updateNs = 0;
        uint64_t reorderNs = 0;
        uint64_t filterNs = 0;
        uint64_t virtualRefreshNs = 0;
        NodeListScrollTelemetry trialScrollTelemetry;
        trialScrollTelemetry.rowsPerCycle = fixtures.size();
        for (size_t cycle = 0; cycle < measuredCycleCount; ++cycle) {
            allocatorTrial.cycleCount++;
            auto orderedCycle = fixtures;
            std::sort(orderedCycle.begin(), orderedCycle.end(),
                      [](const NodeFixture &left, const NodeFixture &right) { return left.lastHeard > right.lastHeard; });
            insertNs += recordOperation(allocatorTrial, harness, "insert", [&](auto &operation) {
                for (const auto &fixture : orderedCycle) {
                    store.upsertUser(fixture.id, fixture.channel, fixture.lastHeard, makeUser(fixture), false);
                }
                sync(&operation);
            });

            updateNs += recordOperation(allocatorTrial, harness, "update", [&](auto &operation) {
                for (size_t i = 0; i < fixtures.size(); ++i) {
                    const auto &fixture = fixtures[i];
                    NodeFixture updated = fixture;
                    updated.role = static_cast<uint8_t>((fixture.role + 1) % 7);
                    store.upsertUser(updated.id, updated.channel, updated.lastHeard,
                                     makeUser(updated, "Updated Node " + std::to_string(cycle) + "-" + std::to_string(i)), false);
                }
                const auto &changed = fixtures.front();
                NodeFixture duplicate = changed;
                duplicate.shortName = "DUP1";
                duplicate.longName = "Duplicate First";
                duplicate.role = static_cast<uint8_t>(MeshtasticView::client);
                duplicate.hasKey = true;
                duplicate.unmessagable = false;
                store.upsertUser(duplicate.id, duplicate.channel, duplicate.lastHeard, makeUser(duplicate), false);
                duplicate.shortName = "DUP2";
                duplicate.longName = "Duplicate Final";
                duplicate.role = static_cast<uint8_t>(MeshtasticView::router);
                store.upsertUser(duplicate.id, duplicate.channel, duplicate.lastHeard, makeUser(duplicate), false);
                sync(&operation);
            });

            applyMixedState(store, fixtures);
            const auto &changed = fixtures.front();
            if (fixtures.size() > 1) {
                const auto &offscreen = fixtures.back();
                recordOperation(allocatorTrial, harness, "rebind", [&](auto &operation) {
                    store.upsertUser(offscreen.id, offscreen.channel, offscreen.lastHeard,
                                     makeUser(offscreen, "Offscreen Updated"), false);
                    sync(&operation);
                });
                const auto *offscreenRecord = store.find(offscreen.id);
                offscreenUpdateOk = offscreenUpdateOk && offscreenRecord &&
                                    std::strcmp(offscreenRecord->user.long_name, "Offscreen Updated") == 0;
            } else {
                recordOperation(allocatorTrial, harness, "rebind", [&](auto &operation) { sync(&operation); });
            }

            filterNs += recordOperation(allocatorTrial, harness, "filter", [&](auto &operation) {
                filter.position = true;
                sync(&operation);
                filter.position = false;
                sync(&operation);
            });

            std::mt19937 orderRandom(options.seed ^ static_cast<uint32_t>((iteration + 1) * (cycle + 1)));
            std::shuffle(fixtures.begin(), fixtures.end(), orderRandom);
            reorderNs += recordOperation(allocatorTrial, harness, "reorder", [&](auto &operation) {
                for (size_t i = 0; i < fixtures.size(); ++i) {
                    const auto &fixture = fixtures[i];
                    const uint32_t lastHeard = 1700000000U - static_cast<uint32_t>(i + cycle * fixtures.size());
                    if (fixture.id == changed.id) {
                        NodeFixture updated = fixture;
                        updated.role = static_cast<uint8_t>(MeshtasticView::router);
                        store.upsertUser(updated.id, updated.channel, lastHeard, makeUser(updated, "Duplicate Final"), false);
                    } else {
                        store.upsertUser(fixture.id, fixture.channel, lastHeard, makeUser(fixture), false);
                    }
                }
                sync(&operation);
            });

            const auto *changedRecord = store.find(changed.id);
            duplicateUpdateOk =
                duplicateUpdateOk && changedRecord && std::strcmp(changedRecord->user.long_name, "Duplicate Final") == 0;
            changedNameAndRoleOk = changedNameAndRoleOk && changedRecord &&
                                   std::strcmp(changedRecord->user.long_name, "Duplicate Final") == 0 &&
                                   changedRecord->user.role == meshtastic_Config_DeviceConfig_Role_ROUTER;

            recordOperation(allocatorTrial, harness, "scroll", [&](auto &) {
                list->scrollTo(changed.id, LV_ANIM_OFF);
                harness.pump();
            });
            trialScrollTelemetry.cycles++;
            for (size_t sample = 0; sample < fixtures.size(); ++sample) {
                const size_t fixtureIndex = cycle % 2 == 0 ? sample : fixtures.size() - 1 - sample;
                recordScrollFrame(trialScrollTelemetry, [&] {
                    list->scrollTo(fixtures[fixtureIndex].id, LV_ANIM_OFF);
                    harness.pump();
                });
            }
            list->scrollTo(changed.id, LV_ANIM_OFF);
            harness.pump();
            virtualRefreshNs +=
                recordOperation(allocatorTrial, harness, "presentation_resync", [&](auto &operation) { sync(&operation); });
            presentationOk = presentationOk && hasPresentedNode(container, changed.id, "Duplicate Final");
            resyncPresentationOk = resyncPresentationOk && index.ids().size() == options.nodes;

            NodeFixture extra{0xb0000000U + static_cast<uint32_t>(iteration * measuredCycleCount + cycle),
                              "CAP1",
                              "Cap Purge Probe",
                              1699999999U,
                              static_cast<uint8_t>(MeshtasticView::client),
                              true,
                              false,
                              0,
                              false,
                              false};
            recordOperation(allocatorTrial, harness, "purge", [&](auto &operation) {
                const NodeId purge = store.selectPurgeCandidate(extra.id, 0, 1700000000U);
                capPurgeOk = capPurgeOk && purge != 0;
                if (purge != 0) {
                    store.remove(purge);
                    store.upsertUser(extra.id, extra.channel, extra.lastHeard, makeUser(extra), false);
                    auto purgedFixture =
                        std::find_if(fixtures.begin(), fixtures.end(), [&](const auto &fixture) { return fixture.id == purge; });
                    if (purgedFixture != fixtures.end()) {
                        *purgedFixture = extra;
                    }
                    sync(&operation);
                }
            });
        }
        nodeCountOk = nodeCountOk && store.size() == options.nodes && index.ids().size() == options.nodes;
        allocatorTrial.after = captureAllocatorSnapshot();
        allocatorTrial.delta = allocatorDelta(allocatorTrial.before, allocatorTrial.after);
        observeAllocatorSnapshot(allocatorTrial.peak, allocatorTrial.after);
        captureTerminalAfter(allocatorTrial, harness, index.ids().size(), store.size(), index.ids().size());
        if (!allocatorTrial.warmup) {
            allocatorChurnBoundedOk =
                allocatorChurnBoundedOk && trialTerminalIsStable(allocatorTrial) && trialOperationsAreStable(allocatorTrial);
        }
        allocatorTelemetry.trials.push_back(std::move(allocatorTrial));

        if (iteration >= options.warmup) {
            insertSamples.push_back(insertNs);
            updateSamples.push_back(updateNs);
            reorderSamples.push_back(reorderNs);
            filterSamples.push_back(filterNs);
            virtualRefreshSamples.push_back(virtualRefreshNs);
            mergeScrollTelemetry(scrollTelemetry, trialScrollTelemetry);
        }
    }

    report.timing.insertNs = summarize(std::move(insertSamples));
    report.timing.updateNs = summarize(std::move(updateSamples));
    report.timing.reorderInsertNs = summarize(std::move(reorderSamples));
    report.timing.filterNs = summarize(std::move(filterSamples));
    report.timing.virtualRefreshNs = summarize(std::move(virtualRefreshSamples));
    finalizeScrollTelemetry(scrollTelemetry);
    report.scrollTelemetry = scrollTelemetry;
    report.allocatorTelemetry = std::move(allocatorTelemetry);
    report.lvgl.totalObjects = harness.objectCount();
    report.lvgl.nodeListObjects = harness.nodeListObjectCount();

    lv_mem_monitor_t memory{};
    lv_mem_monitor(&memory);
    report.heap.maxRssBytes = currentMaxRssBytes();
#ifdef NODE_LIST_BENCH_EXECUTABLE
    report.heap.newCount = heapAccounting().count.load();
    report.heap.newBytes = heapAccounting().bytes.load();
#endif
    report.memory.totalSize = memory.total_size;
    report.memory.freeCount = memory.free_cnt;
    report.memory.freeSize = memory.free_size;
    report.memory.biggestFreeBlock = memory.free_biggest_size;
    report.memory.usedCount = memory.used_cnt;
    report.memory.maxUsed = memory.max_used;
    report.memory.usedPercent = memory.used_pct;
    report.memory.fragmentationPercent = memory.frag_pct;
    report.memory.integrityOk = lv_mem_test() == LV_RESULT_OK;

    report.correctness.requestedNodeCount = nodeCountOk;
    report.correctness.duplicateUpdate = duplicateUpdateOk;
    report.correctness.changedNameAndRole = changedNameAndRoleOk;
    report.correctness.capPurge = capPurgeOk;
    report.correctness.resyncPresentationPreservedNodes = resyncPresentationOk;
    report.correctness.offscreenUpdate = offscreenUpdateOk;
    report.correctness.candidate = {poolBoundedOk, objectCountStableOk, allocatorChurnBoundedOk, presentationOk};
    report.correctness.all = report.correctness.ready && report.correctness.requestedNodeCount &&
                             report.correctness.duplicateUpdate && report.correctness.changedNameAndRole &&
                             report.correctness.capPurge && report.correctness.resyncPresentationPreservedNodes &&
                             report.correctness.offscreenUpdate && report.correctness.candidate->poolBounded &&
                             report.correctness.candidate->objectCountStable &&
                             report.correctness.candidate->allocatorChurnBounded && report.correctness.candidate->presentation &&
                             report.memory.integrityOk;
    list.reset();
    lv_obj_delete(container);
    return report;
}
} // namespace

NodeListBenchmarkReport runNodeListBenchmark(const NodeListBenchmarkOptions &options)
{
    NodeListBenchmarkReport report;
    report.source.revision = DEVICE_UI_SOURCE_REVISION;
    report.source.dirty = DEVICE_UI_SOURCE_DIRTY != 0;
    report.implementation.name = implementationName(options.implementation);
    report.implementation.comparisonScope = comparisonScope;
    report.lvgl.version =
        std::to_string(LVGL_VERSION_MAJOR) + "." + std::to_string(LVGL_VERSION_MINOR) + "." + std::to_string(LVGL_VERSION_PATCH);
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    report.allocator.implementation = "LV_STDLIB_BUILTIN";
#elif LV_USE_STDLIB_MALLOC == LV_STDLIB_CLIB
    report.allocator.implementation = "LV_STDLIB_CLIB";
#elif LV_USE_STDLIB_MALLOC == LV_STDLIB_MICROPYTHON
    report.allocator.implementation = "LV_STDLIB_MICROPYTHON";
#elif LV_USE_STDLIB_MALLOC == LV_STDLIB_RTTHREAD
    report.allocator.implementation = "LV_STDLIB_RTTHREAD";
#else
    report.allocator.implementation = "LV_STDLIB_CUSTOM";
#endif
#ifdef LV_MEM_SIZE
    report.allocator.configuredBytes = static_cast<size_t>(LV_MEM_SIZE);
#endif
#ifdef RAM_SIZE
    report.allocator.ramSizeBytes = static_cast<size_t>(RAM_SIZE) * 1024U;
#endif
    report.scenario = {options.nodes, options.trials, options.seed, options.warmup};
    report.fixtures.supported = {"known/unknown",
                                 "online/offline",
                                 "LoRa hops",
                                 "key/no-key",
                                 "position/no-position",
                                 "telemetry/no-telemetry",
                                 "active chat",
                                 "UTF-8/emoji",
                                 "empty/typical/max names",
                                 "duplicate updates",
                                 "changed name/role",
                                 "cap purge",
                                 "resync presentation retention",
                                 "off-screen update"};
    report.fixtures.unsupported.push_back(
        {"MQTT", "The baseline TFT node row has no representable MQTT state; its MQTT filter path is compiled out as TODO."});

    if (options.nodes == 0 || options.trials == 0 || options.trials > std::numeric_limits<size_t>::max() - options.warmup) {
        return report;
    }

    if (options.implementation == NodeListBenchmarkImplementation::VirtualCandidate) {
        return runVirtualCandidateBenchmark(options, std::move(report));
    }

    MuiTestHarness harness;
    report.correctness.ready = harness.ready();
    if (!report.correctness.ready) {
        return report;
    }

    std::vector<uint64_t> insertSamples;
    std::vector<uint64_t> updateSamples;
    std::vector<uint64_t> reorderSamples;
    std::vector<uint64_t> filterSamples;
    NodeListScrollTelemetry scrollTelemetry;
    bool nodeCountOk = true;
    bool duplicateUpdateOk = true;
    bool changedNameAndRoleOk = true;
    bool capPurgeOk = true;
    bool resyncPresentationOk = true;
    bool offscreenUpdateOk = true;
    bool allocatorChurnBoundedOk = true;

    const size_t iterations = options.warmup + options.trials;
    NodeListCandidateAllocatorTelemetry allocatorTelemetry;
    allocatorTelemetry.meaning =
        "LVGL allocator and object snapshots captured around each benchmark operation after the legacy list has reached a "
        "steady post-population state. Each operation records before, after, delta, peak, total object counts, node-list object "
        "counts, and booleans derived only from the recorded values. Reorder and purge use causal update/remove/add paths "
        "instead "
        "of reset/rebuild shortcuts so recurring retained-object or allocator growth fails the recorded stability checks.";
    for (size_t iteration = 0; iteration < iterations; ++iteration) {
        NodeListCandidateAllocatorTelemetry::Trial allocatorTrial;
        allocatorTrial.iteration = iteration;
        allocatorTrial.warmup = iteration < options.warmup;
        auto fixtures = makeFixtures(options.nodes, options.seed, iteration);
        harness.resetNodeList();
        harness.enableVirtualNodeModelFixture();
        harness.setCurrentTime(1700000000U);

        auto ordered = fixtures;
        std::sort(ordered.begin(), ordered.end(),
                  [](const NodeFixture &left, const NodeFixture &right) { return left.lastHeard > right.lastHeard; });
        insertFixtures(harness, ordered);
        harness.showNodesScreen();
        harness.pump(50);
        applyMixedState(harness, fixtures);
        harness.pump(500);
        allocatorTrial.before = captureAllocatorSnapshot();
        allocatorTrial.peak = allocatorTrial.before;
        captureTerminalBefore(allocatorTrial, harness, harness.legacyRetainedNodeCount(), harness.store().size(),
                              harness.renderedNodeCount());

        uint64_t insertNs = 0;
        uint64_t updateNs = 0;
        uint64_t reorderNs = 0;
        uint64_t filterNs = 0;
        NodeListScrollTelemetry trialScrollTelemetry;
        trialScrollTelemetry.rowsPerCycle = fixtures.size();
        for (size_t cycle = 0; cycle < measuredCycleCount; ++cycle) {
            allocatorTrial.cycleCount++;
            auto orderedCycle = fixtures;
            std::sort(orderedCycle.begin(), orderedCycle.end(),
                      [](const NodeFixture &left, const NodeFixture &right) { return left.lastHeard > right.lastHeard; });
            insertNs += recordOperation(allocatorTrial, harness, "insert", [&](auto &) {
                for (const auto &fixture : orderedCycle) {
                    harness.updateNodeFixture(fixture.id, fixture.shortName.c_str(), fixture.longName.c_str(), fixture.role,
                                              fixture.hasKey, fixture.unmessagable, fixture.channel);
                }
                harness.pump();
            });

            updateNs += recordOperation(allocatorTrial, harness, "update", [&](auto &) {
                for (size_t i = 0; i < fixtures.size(); ++i) {
                    auto &fixture = fixtures[i];
                    const std::string longName = "Updated Node " + std::to_string(cycle) + "-" + std::to_string(i);
                    fixture.longName = longName;
                    fixture.role = static_cast<uint8_t>((fixture.role + 1) % 7);
                    harness.updateNodeFixture(fixture.id, fixture.shortName.c_str(), fixture.longName.c_str(), fixture.role,
                                              fixture.hasKey, fixture.unmessagable, fixture.channel);
                }
                auto &changed = fixtures.front();
                changed.shortName = "DUP2";
                changed.longName = "Duplicate Final";
                changed.role = static_cast<uint8_t>(MeshtasticView::router);
                changed.hasKey = true;
                changed.unmessagable = false;
                harness.updateNodeFixture(changed.id, "DUP1", "Duplicate First", static_cast<uint8_t>(MeshtasticView::client),
                                          true, false, changed.channel);
                harness.updateNodeFixture(changed.id, changed.shortName.c_str(), changed.longName.c_str(), changed.role,
                                          changed.hasKey, changed.unmessagable, changed.channel);
                harness.pump();
            });

            const auto &changed = fixtures.front();
            if (fixtures.size() > 1) {
                auto &offscreen = fixtures.back();
                recordOperation(allocatorTrial, harness, "rebind", [&](auto &) {
                    harness.updateNodeFixture(offscreen.id, "OFFS", "Offscreen Updated", offscreen.role, offscreen.hasKey,
                                              offscreen.unmessagable, offscreen.channel);
                    harness.pump();
                });
                offscreen.shortName = "OFFS";
                offscreen.longName = "Offscreen Updated";
                const char *offscreenName = harness.nodeLongName(offscreen.id);
                offscreenUpdateOk = offscreenUpdateOk && offscreenName && std::strcmp(offscreenName, "Offscreen Updated") == 0;
                const auto offscreenSnapshot = harness.legacyRowSnapshot(offscreen.id);
                offscreenUpdateOk = offscreenUpdateOk && offscreenSnapshot.longName == "Offscreen Updated";
            } else {
                recordOperation(allocatorTrial, harness, "rebind", [&](auto &) { harness.pump(); });
            }

            const char *changedName = harness.nodeLongName(changed.id);
            duplicateUpdateOk = duplicateUpdateOk && changedName && std::strcmp(changedName, "Duplicate Final") == 0;
            changedNameAndRoleOk = changedNameAndRoleOk && changedName && std::strcmp(changedName, "Duplicate Final") == 0 &&
                                   harness.nodeRole(changed.id) == static_cast<uint8_t>(MeshtasticView::router);

            filterNs += recordOperation(allocatorTrial, harness, "filter", [&](auto &) { harness.scanNodeFilters(); });

            std::mt19937 orderRandom(options.seed ^ static_cast<uint32_t>((iteration + 1) * (cycle + 1)));
            std::shuffle(fixtures.begin(), fixtures.end(), orderRandom);
            reorderNs += recordOperation(allocatorTrial, harness, "reorder", [&](auto &) {
                for (const auto &fixture : fixtures) {
                    harness.updateLastHeardFixture(fixture.id);
                    harness.pump();
                }
            });

            recordOperation(allocatorTrial, harness, "scroll", [&](auto &) {
                lv_obj_scroll_to_y(harness.legacyNodeListRootForTesting(), 120, LV_ANIM_OFF);
                harness.pump();
            });
            trialScrollTelemetry.cycles++;
            for (size_t sample = 0; sample < fixtures.size(); ++sample) {
                const size_t fixtureIndex = cycle % 2 == 0 ? sample : fixtures.size() - 1 - sample;
                recordScrollFrame(trialScrollTelemetry, [&] {
                    lv_obj_scroll_to_y(harness.legacyNodeListRootForTesting(), static_cast<int32_t>(fixtureIndex * 48U),
                                       LV_ANIM_OFF);
                    harness.pump();
                });
            }

            recordOperation(allocatorTrial, harness, "presentation_resync",
                            [&](auto &) { harness.toggleResyncPresentationFixture(); });
            resyncPresentationOk = resyncPresentationOk && harness.legacyRetainedNodeCount() == options.nodes;

            NodeFixture extra{0xb0000000U + static_cast<uint32_t>(iteration * measuredCycleCount + cycle),
                              "CAP1",
                              "Cap Purge Probe",
                              1699999999U,
                              static_cast<uint8_t>(MeshtasticView::client),
                              true,
                              false,
                              0,
                              false,
                              false};
            recordOperation(allocatorTrial, harness, "purge", [&](auto &) {
                const uint32_t purgeId = harness.nodePurgeCandidate(extra.id);
                capPurgeOk = capPurgeOk && purgeId != 0;
                if (purgeId != 0) {
                    auto purgedFixture = std::find_if(fixtures.begin(), fixtures.end(),
                                                      [&](const auto &fixture) { return fixture.id == purgeId; });
                    if (purgedFixture != fixtures.end()) {
                        extra.hasPosition = purgedFixture->hasPosition;
                        extra.hasTelemetry = purgedFixture->hasTelemetry;
                    }
                    if (options.nodes < 250) {
                        harness.purgeLegacyNode(extra.id);
                    }
                    harness.addNodeFixture(extra.id, extra.shortName.c_str(), extra.longName.c_str(), extra.lastHeard, extra.role,
                                           extra.hasKey, extra.unmessagable, extra.channel);
                    harness.updateHopsFixture(extra.id, 1);
                    if (extra.hasPosition) {
                        harness.updatePositionFixture(extra.id, 374221234, -1220845678, 10, 7, 16);
                    }
                    if (extra.hasTelemetry) {
                        harness.updateTelemetryFixture(extra.id, 20.0F, 45.0F, 1013.2F, 50);
                    }
                    harness.pump();
                    if (purgedFixture != fixtures.end()) {
                        *purgedFixture = extra;
                    }
                    capPurgeOk =
                        capPurgeOk && harness.nodeLongName(extra.id) != nullptr && harness.nodeLongName(purgeId) == nullptr;
                }
            });
        }
        nodeCountOk = nodeCountOk && harness.legacyRetainedNodeCount() == options.nodes &&
                      harness.store().size() == options.nodes && harness.renderedNodeCount() == options.nodes;
        allocatorTrial.after = captureAllocatorSnapshot();
        allocatorTrial.delta = allocatorDelta(allocatorTrial.before, allocatorTrial.after);
        observeAllocatorSnapshot(allocatorTrial.peak, allocatorTrial.after);
        captureTerminalAfter(allocatorTrial, harness, harness.legacyRetainedNodeCount(), harness.store().size(),
                             harness.renderedNodeCount());
        if (!allocatorTrial.warmup) {
            allocatorChurnBoundedOk =
                allocatorChurnBoundedOk && trialTerminalIsStable(allocatorTrial) && trialOperationsAreStable(allocatorTrial);
        }
        allocatorTelemetry.trials.push_back(std::move(allocatorTrial));

        if (iteration >= options.warmup) {
            insertSamples.push_back(insertNs);
            updateSamples.push_back(updateNs);
            reorderSamples.push_back(reorderNs);
            filterSamples.push_back(filterNs);
            mergeScrollTelemetry(scrollTelemetry, trialScrollTelemetry);
        }
    }

    report.timing.insertNs = summarize(std::move(insertSamples));
    report.timing.updateNs = summarize(std::move(updateSamples));
    report.timing.reorderInsertNs = summarize(std::move(reorderSamples));
    report.timing.filterNs = summarize(std::move(filterSamples));
    finalizeScrollTelemetry(scrollTelemetry);
    report.scrollTelemetry = scrollTelemetry;
    report.allocatorTelemetry = std::move(allocatorTelemetry);
    report.lvgl.totalObjects = harness.objectCount();
    report.lvgl.nodeListObjects = harness.nodeListObjectCount();

    lv_mem_monitor_t memory{};
    lv_mem_monitor(&memory);
    report.heap.maxRssBytes = currentMaxRssBytes();
#ifdef NODE_LIST_BENCH_EXECUTABLE
    report.heap.newCount = heapAccounting().count.load();
    report.heap.newBytes = heapAccounting().bytes.load();
#endif
    report.memory.totalSize = memory.total_size;
    report.memory.freeCount = memory.free_cnt;
    report.memory.freeSize = memory.free_size;
    report.memory.biggestFreeBlock = memory.free_biggest_size;
    report.memory.usedCount = memory.used_cnt;
    report.memory.maxUsed = memory.max_used;
    report.memory.usedPercent = memory.used_pct;
    report.memory.fragmentationPercent = memory.frag_pct;
    report.memory.integrityOk = lv_mem_test() == LV_RESULT_OK;

    report.correctness.requestedNodeCount = nodeCountOk;
    report.correctness.duplicateUpdate = duplicateUpdateOk;
    report.correctness.changedNameAndRole = changedNameAndRoleOk;
    report.correctness.capPurge = capPurgeOk;
    report.correctness.resyncPresentationPreservedNodes = resyncPresentationOk;
    report.correctness.offscreenUpdate = offscreenUpdateOk;
    report.correctness.all = report.correctness.ready && report.correctness.requestedNodeCount &&
                             report.correctness.duplicateUpdate && report.correctness.changedNameAndRole &&
                             report.correctness.capPurge && report.correctness.resyncPresentationPreservedNodes &&
                             report.correctness.offscreenUpdate && allocatorChurnBoundedOk && report.memory.integrityOk;
    return report;
}

bool writeNodeListBenchmarkJson(const NodeListBenchmarkReport &report, const std::string &path, std::string &error)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "cannot open JSON output path: " + path;
        return false;
    }

    output << "{\n"
           << "  \"schema_version\": 2,\n"
           << "  \"source\": {\"revision\": \"" << escapeJson(report.source.revision)
           << "\", \"dirty\": " << (report.source.dirty ? "true" : "false") << "},\n"
           << "  \"implementation\": {\"name\": \"" << escapeJson(report.implementation.name) << "\", \"comparison_scope\": \""
           << escapeJson(report.implementation.comparisonScope) << "\"},\n"
           << "  \"lvgl\": {\"version\": \"" << escapeJson(report.lvgl.version)
           << "\", \"total_objects\": " << report.lvgl.totalObjects << ", \"node_list_objects\": " << report.lvgl.nodeListObjects
           << "},\n"
           << "  \"allocator\": {\"implementation\": \"" << escapeJson(report.allocator.implementation)
           << "\", \"configured_bytes\": " << report.allocator.configuredBytes
           << ", \"ram_size_bytes\": " << report.allocator.ramSizeBytes << "},\n"
           << "  \"scenario\": {\"nodes\": " << report.scenario.nodes << ", \"trials\": " << report.scenario.trials
           << ", \"seed\": " << report.scenario.seed << ", \"warmup\": " << report.scenario.warmup << "},\n"
           << "  \"fixtures\": {\n    \"supported\": [";
    for (size_t i = 0; i < report.fixtures.supported.size(); ++i) {
        output << (i == 0 ? "" : ", ") << '"' << escapeJson(report.fixtures.supported[i]) << '"';
    }
    output << "],\n    \"unsupported\": [";
    for (size_t i = 0; i < report.fixtures.unsupported.size(); ++i) {
        const auto &fixture = report.fixtures.unsupported[i];
        output << (i == 0 ? "" : ", ") << "{\"name\": \"" << escapeJson(fixture.name) << "\", \"reason\": \""
               << escapeJson(fixture.reason) << "\"}";
    }
    output << "]\n  },\n"
           << "  \"heap\": {\"new_count\": " << report.heap.newCount << ", \"new_bytes\": " << report.heap.newBytes
           << ", \"max_rss_bytes\": " << report.heap.maxRssBytes << "},\n"
           << "  \"memory\": {\"total_size\": " << report.memory.totalSize << ", \"free_count\": " << report.memory.freeCount
           << ", \"free_size\": " << report.memory.freeSize << ", \"biggest_free_block\": " << report.memory.biggestFreeBlock
           << ", \"used_count\": " << report.memory.usedCount << ", \"max_used\": " << report.memory.maxUsed
           << ", \"used_percent\": " << static_cast<unsigned>(report.memory.usedPercent)
           << ", \"fragmentation_percent\": " << static_cast<unsigned>(report.memory.fragmentationPercent)
           << ", \"integrity_ok\": " << (report.memory.integrityOk ? "true" : "false") << "},\n"
           << "  \"scroll_telemetry\": {\"cycles\": " << report.scrollTelemetry.cycles
           << ", \"rows_per_cycle\": " << report.scrollTelemetry.rowsPerCycle
           << ", \"sample_count\": " << report.scrollTelemetry.sampleCount
           << ", \"frame_count\": " << report.scrollTelemetry.frameCount
           << ", \"elapsed_ns\": " << report.scrollTelemetry.elapsedNs << ", \"average_fps\": " << std::setprecision(12)
           << report.scrollTelemetry.averageFps << ", \"worst_frame_ns\": " << report.scrollTelemetry.worstFrameNs << "},\n"
           << "  \"allocator_telemetry\": ";
    if (report.allocatorTelemetry.has_value()) {
        const auto &telemetry = report.allocatorTelemetry.value();
        output << "{\"meaning\": \"" << escapeJson(telemetry.meaning) << "\", \"trials\": [\n";
        for (size_t i = 0; i < telemetry.trials.size(); ++i) {
            const auto &trial = telemetry.trials[i];
            output << "    {\"iteration\": " << trial.iteration << ", \"warmup\": " << (trial.warmup ? "true" : "false")
                   << ", \"cycle_count\": " << trial.cycleCount << ", \"before\": ";
            writeAllocatorSnapshot(output, trial.before);
            output << ", \"sync_snapshots\": [";
            for (size_t syncIndex = 0; syncIndex < trial.syncSnapshots.size(); ++syncIndex) {
                output << (syncIndex == 0 ? "" : ", ");
                writeAllocatorSnapshot(output, trial.syncSnapshots[syncIndex]);
            }
            output << "], \"operations\": [";
            for (size_t operationIndex = 0; operationIndex < trial.operations.size(); ++operationIndex) {
                const auto &operation = trial.operations[operationIndex];
                output << (operationIndex == 0 ? "" : ", ") << "{\"name\": \"" << escapeJson(operation.name)
                       << "\", \"before\": ";
                writeAllocatorSnapshot(output, operation.before);
                output << ", \"after\": ";
                writeAllocatorSnapshot(output, operation.after);
                output << ", \"delta\": ";
                writeAllocatorDelta(output, operation.delta);
                output << ", \"peak\": ";
                writeAllocatorSnapshot(output, operation.peak);
                output << ", \"snapshots\": [";
                for (size_t snapshotIndex = 0; snapshotIndex < operation.snapshots.size(); ++snapshotIndex) {
                    output << (snapshotIndex == 0 ? "" : ", ");
                    writeAllocatorSnapshot(output, operation.snapshots[snapshotIndex]);
                }
                output << ']';
                output << ", \"objects_before\": " << operation.objectsBefore << ", \"objects_after\": " << operation.objectsAfter
                       << ", \"node_list_objects_before\": " << operation.nodeListObjectsBefore
                       << ", \"node_list_objects_after\": " << operation.nodeListObjectsAfter
                       << ", \"object_count_stable\": " << (operation.objectCountStable ? "true" : "false")
                       << ", \"allocator_used_count_bounded\": " << (operation.allocatorUsedCountBounded ? "true" : "false")
                       << '}';
            }
            output << "], \"after\": ";
            writeAllocatorSnapshot(output, trial.after);
            output << ", \"delta\": ";
            writeAllocatorDelta(output, trial.delta);
            output << ", \"peak\": ";
            writeAllocatorSnapshot(output, trial.peak);
            output << ", \"terminal\": {\"objects_before\": " << trial.objectsBefore
                   << ", \"objects_after\": " << trial.objectsAfter
                   << ", \"node_list_objects_before\": " << trial.nodeListObjectsBefore
                   << ", \"node_list_objects_after\": " << trial.nodeListObjectsAfter
                   << ", \"retained_nodes_before\": " << trial.retainedNodesBefore
                   << ", \"retained_nodes_after\": " << trial.retainedNodesAfter
                   << ", \"node_store_size_before\": " << trial.nodeStoreSizeBefore
                   << ", \"node_store_size_after\": " << trial.nodeStoreSizeAfter
                   << ", \"node_count_before\": " << trial.nodeCountBefore << ", \"node_count_after\": " << trial.nodeCountAfter
                   << ", \"object_count_stable\": " << (trial.objectCountStable ? "true" : "false")
                   << ", \"retained_nodes_stable\": " << (trial.retainedNodesStable ? "true" : "false")
                   << ", \"node_store_size_stable\": " << (trial.nodeStoreSizeStable ? "true" : "false")
                   << ", \"node_count_stable\": " << (trial.nodeCountStable ? "true" : "false")
                   << ", \"allocator_used_count_bounded\": " << (trial.allocatorUsedCountBounded ? "true" : "false")
                   << ", \"all\": " << (trial.terminalStable ? "true" : "false") << '}';
            output << '}' << (i + 1 == telemetry.trials.size() ? "\n" : ",\n");
        }
        output << "  ]}";
    } else {
        output << "null";
    }
    output << ",\n"
           << "  \"timing\": {\n    \"insert_ns\": ";
    writeDuration(output, report.timing.insertNs, 4);
    output << ",\n    \"update_ns\": ";
    writeDuration(output, report.timing.updateNs, 4);
    output << ",\n    \"reorder_insert_ns\": ";
    writeDuration(output, report.timing.reorderInsertNs, 4);
    output << ",\n    \"filter_ns\": ";
    writeOptionalDuration(output, report.timing.filterNs, 4);
    output << ",\n    \"virtual_refresh_ns\": ";
    writeOptionalDuration(output, report.timing.virtualRefreshNs, 4);
    output << "\n  },\n"
           << "  \"correctness\": {\"ready\": " << (report.correctness.ready ? "true" : "false")
           << ", \"requested_node_count\": " << (report.correctness.requestedNodeCount ? "true" : "false")
           << ", \"duplicate_update\": " << (report.correctness.duplicateUpdate ? "true" : "false")
           << ", \"changed_name_and_role\": " << (report.correctness.changedNameAndRole ? "true" : "false")
           << ", \"cap_purge\": " << (report.correctness.capPurge ? "true" : "false")
           << ", \"resync_presentation_preserved_nodes\": "
           << (report.correctness.resyncPresentationPreservedNodes ? "true" : "false")
           << ", \"offscreen_update\": " << (report.correctness.offscreenUpdate ? "true" : "false") << ", \"candidate\": ";
    if (report.correctness.candidate.has_value()) {
        const auto &candidate = report.correctness.candidate.value();
        output << "{\"pool_bounded\": " << (candidate.poolBounded ? "true" : "false")
               << ", \"object_count_stable\": " << (candidate.objectCountStable ? "true" : "false")
               << ", \"allocator_churn_bounded\": " << (candidate.allocatorChurnBounded ? "true" : "false")
               << ", \"presentation\": " << (candidate.presentation ? "true" : "false") << '}';
    } else {
        output << "null";
    }
    output << ", \"all\": " << (report.correctness.all ? "true" : "false") << "}\n}\n";

    if (!output) {
        error = "failed while writing JSON output path: " + path;
        return false;
    }
    return true;
}

#ifdef NODE_LIST_BENCH_EXECUTABLE
int main(int argc, char **argv)
{
    NodeListBenchmarkOptions options{};
    std::string jsonPath;
    if (!parseNodeListBenchmarkCommandLine(argc, argv, options, jsonPath)) {
        printUsage(std::cerr);
        return 2;
    }

    const auto report = runNodeListBenchmark(options);
    std::string error;
    if (!writeNodeListBenchmarkJson(report, jsonPath, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    if (!report.correctness.all) {
        std::cerr << "benchmark correctness checks failed\n";
        return 1;
    }
    return 0;
}
#endif
