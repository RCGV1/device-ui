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
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <string_view>

#ifndef DEVICE_UI_SOURCE_REVISION
#define DEVICE_UI_SOURCE_REVISION "unknown"
#endif

#ifndef DEVICE_UI_SOURCE_DIRTY
#define DEVICE_UI_SOURCE_DIRTY 0
#endif

namespace
{
using Clock = std::chrono::steady_clock;

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

constexpr const char *comparisonScope = "Host-relative structural, CPU, and allocator comparison; not hardware timing.";

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
        harness.addActiveChatFixture(fixtures.front().id, fixtures.front().channel);
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
    bool nodeCountOk = true;
    bool duplicateUpdateOk = true;
    bool changedNameAndRoleOk = true;
    bool capPurgeOk = true;
    bool resyncPresentationOk = true;
    bool offscreenUpdateOk = true;
    bool poolBoundedOk = list->boundRowCount() == VirtualNodeList::POOL_SIZE;
    bool objectCountStableOk = true;
    bool presentationOk = true;

    const size_t iterations = options.warmup + options.trials;
    for (size_t iteration = 0; iteration < iterations; ++iteration) {
        auto fixtures = makeFixtures(options.nodes, options.seed, iteration);
        harness.setCurrentTime(1700000000U);
        NodeStore store;
        VisibleNodeIndex index;
        NodeListFilter filter;

        auto sync = [&] {
            index.rebuild(store, filter, 0);
            list->sync(store, index);
            harness.pump();
            objectCountStableOk = objectCountStableOk && harness.objectCount() == pooledObjectCount &&
                                  harness.nodeListObjectCount() == pooledNodeListObjectCount;
        };
        auto ordered = fixtures;
        std::sort(ordered.begin(), ordered.end(),
                  [](const NodeFixture &left, const NodeFixture &right) { return left.lastHeard > right.lastHeard; });
        const uint64_t insertNs = measureNs([&] {
            for (const auto &fixture : ordered) {
                store.upsertUser(fixture.id, fixture.channel, fixture.lastHeard, makeUser(fixture), false);
            }
            sync();
        });

        const uint64_t updateNs = measureNs([&] {
            for (size_t i = 0; i < fixtures.size(); ++i) {
                const auto &fixture = fixtures[i];
                NodeFixture updated = fixture;
                updated.role = static_cast<uint8_t>((fixture.role + 1) % 7);
                store.upsertUser(updated.id, updated.channel, updated.lastHeard,
                                 makeUser(updated, "Updated Node " + std::to_string(i)), false);
            }
            sync();
        });

        store = NodeStore();
        std::mt19937 orderRandom(options.seed ^ static_cast<uint32_t>(iteration + 1));
        std::shuffle(fixtures.begin(), fixtures.end(), orderRandom);
        const uint64_t reorderNs = measureNs([&] {
            for (const auto &fixture : fixtures) {
                store.upsertUser(fixture.id, fixture.channel, fixture.lastHeard, makeUser(fixture), false);
            }
            sync();
        });

        applyMixedState(store, fixtures);
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

        if (fixtures.size() > 1) {
            const auto &offscreen = fixtures.back();
            store.upsertUser(offscreen.id, offscreen.channel, offscreen.lastHeard, makeUser(offscreen, "Offscreen Updated"),
                             false);
            sync();
            const auto *offscreenRecord = store.find(offscreen.id);
            offscreenUpdateOk =
                offscreenUpdateOk && offscreenRecord && std::strcmp(offscreenRecord->user.long_name, "Offscreen Updated") == 0;
        } else {
            sync();
        }

        const auto *changedRecord = store.find(changed.id);
        duplicateUpdateOk =
            duplicateUpdateOk && changedRecord && std::strcmp(changedRecord->user.long_name, "Duplicate Final") == 0;
        changedNameAndRoleOk = changedNameAndRoleOk && changedRecord &&
                               std::strcmp(changedRecord->user.long_name, "Duplicate Final") == 0 &&
                               changedRecord->user.role == meshtastic_Config_DeviceConfig_Role_ROUTER;

        const uint64_t visibleIndexRebuildNs = measureNs([&] { sync(); });
        list->scrollTo(changed.id, LV_ANIM_OFF);
        harness.pump();
        presentationOk = presentationOk && hasPresentedNode(container, changed.id, "Duplicate Final");
        resyncPresentationOk = resyncPresentationOk && index.ids().size() == options.nodes;

        if (options.nodes == 250) {
            NodeFixture extra{0xb0000000U + static_cast<uint32_t>(iteration),
                              "CAP1",
                              "Cap Purge Probe",
                              1699999999U,
                              static_cast<uint8_t>(MeshtasticView::client),
                              true,
                              false,
                              0,
                              false,
                              false};
            const NodeId purge = store.selectPurgeCandidate(extra.id, 0, 1700000000U);
            capPurgeOk = capPurgeOk && purge != 0;
            if (purge != 0) {
                store.remove(purge);
                store.upsertUser(extra.id, extra.channel, extra.lastHeard, makeUser(extra), false);
                sync();
            }
        }
        nodeCountOk = nodeCountOk && store.size() == options.nodes && index.ids().size() == options.nodes;

        if (iteration >= options.warmup) {
            insertSamples.push_back(insertNs);
            updateSamples.push_back(updateNs);
            reorderSamples.push_back(reorderNs);
            filterSamples.push_back(visibleIndexRebuildNs);
        }
    }

    report.timing.insertNs = summarize(std::move(insertSamples));
    report.timing.updateNs = summarize(std::move(updateSamples));
    report.timing.reorderInsertNs = summarize(std::move(reorderSamples));
    report.timing.visibleIndexRebuildNs = summarize(std::move(filterSamples));
    report.lvgl.totalObjects = harness.objectCount();
    report.lvgl.nodeListObjects = harness.nodeListObjectCount();

    lv_mem_monitor_t memory{};
    lv_mem_monitor(&memory);
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
    report.correctness.candidate = {poolBoundedOk, objectCountStableOk, presentationOk};
    report.correctness.all = report.correctness.ready && report.correctness.requestedNodeCount &&
                             report.correctness.duplicateUpdate && report.correctness.changedNameAndRole &&
                             report.correctness.capPurge && report.correctness.resyncPresentationPreservedNodes &&
                             report.correctness.offscreenUpdate && report.correctness.candidate->poolBounded &&
                             report.correctness.candidate->objectCountStable && report.correctness.candidate->presentation &&
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
    bool nodeCountOk = true;
    bool duplicateUpdateOk = true;
    bool changedNameAndRoleOk = true;
    bool capPurgeOk = true;
    bool resyncPresentationOk = true;
    bool offscreenUpdateOk = true;

    const size_t iterations = options.warmup + options.trials;
    for (size_t iteration = 0; iteration < iterations; ++iteration) {
        auto fixtures = makeFixtures(options.nodes, options.seed, iteration);
        harness.resetNodeList();
        harness.setCurrentTime(1700000000U);

        auto ordered = fixtures;
        std::sort(ordered.begin(), ordered.end(),
                  [](const NodeFixture &left, const NodeFixture &right) { return left.lastHeard > right.lastHeard; });
        const uint64_t insertNs = measureNs([&] { insertFixtures(harness, ordered); });

        const uint64_t updateNs = measureNs([&] {
            for (size_t i = 0; i < fixtures.size(); ++i) {
                const auto &fixture = fixtures[i];
                const std::string longName = "Updated Node " + std::to_string(i);
                harness.updateNodeFixture(fixture.id, fixture.shortName.c_str(), longName.c_str(),
                                          static_cast<uint8_t>((fixture.role + 1) % 7), fixture.hasKey, fixture.unmessagable,
                                          fixture.channel);
            }
            harness.pump();
        });

        harness.resetNodeList();
        harness.setCurrentTime(1700000000U);
        std::mt19937 orderRandom(options.seed ^ static_cast<uint32_t>(iteration + 1));
        std::shuffle(fixtures.begin(), fixtures.end(), orderRandom);
        const uint64_t reorderNs = measureNs([&] { insertFixtures(harness, fixtures); });

        applyMixedState(harness, fixtures);
        const auto &changed = fixtures.front();
        harness.updateNodeFixture(changed.id, "DUP1", "Duplicate First", static_cast<uint8_t>(MeshtasticView::client), true,
                                  false, changed.channel);
        harness.updateNodeFixture(changed.id, "DUP2", "Duplicate Final", static_cast<uint8_t>(MeshtasticView::router), true,
                                  false, changed.channel);

        if (fixtures.size() > 1) {
            const auto &offscreen = fixtures.back();
            harness.updateNodeFixture(offscreen.id, "OFFS", "Offscreen Updated", offscreen.role, offscreen.hasKey,
                                      offscreen.unmessagable, offscreen.channel);
            harness.pump();
            const char *offscreenName = harness.nodeLongName(offscreen.id);
            offscreenUpdateOk = offscreenUpdateOk && offscreenName && std::strcmp(offscreenName, "Offscreen Updated") == 0;
        } else {
            harness.pump();
        }

        const char *changedName = harness.nodeLongName(changed.id);
        duplicateUpdateOk = duplicateUpdateOk && changedName && std::strcmp(changedName, "Duplicate Final") == 0;
        changedNameAndRoleOk = changedNameAndRoleOk && changedName && std::strcmp(changedName, "Duplicate Final") == 0 &&
                               harness.nodeRole(changed.id) == static_cast<uint8_t>(MeshtasticView::router);

        const uint64_t filterNs = measureNs([&] { harness.scanNodeFilters(); });
        harness.toggleResyncPresentationFixture();
        resyncPresentationOk = resyncPresentationOk && harness.renderedNodeCount() == options.nodes;

        if (options.nodes == 250) {
            const uint32_t extraId = 0xb0000000U + static_cast<uint32_t>(iteration);
            harness.addNodeFixture(extraId, "CAP1", "Cap Purge Probe", 1699999999U, static_cast<uint8_t>(MeshtasticView::client),
                                   true, false, 0);
            harness.pump();
            capPurgeOk = capPurgeOk && harness.renderedNodeCount() == 250;
        }
        nodeCountOk = nodeCountOk && harness.renderedNodeCount() == options.nodes;

        if (iteration >= options.warmup) {
            insertSamples.push_back(insertNs);
            updateSamples.push_back(updateNs);
            reorderSamples.push_back(reorderNs);
            filterSamples.push_back(filterNs);
        }
    }

    report.timing.insertNs = summarize(std::move(insertSamples));
    report.timing.updateNs = summarize(std::move(updateSamples));
    report.timing.reorderInsertNs = summarize(std::move(reorderSamples));
    report.timing.filterNs = summarize(std::move(filterSamples));
    report.lvgl.totalObjects = harness.objectCount();
    report.lvgl.nodeListObjects = harness.nodeListObjectCount();

    lv_mem_monitor_t memory{};
    lv_mem_monitor(&memory);
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
                             report.correctness.offscreenUpdate && report.memory.integrityOk;
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
           << "  \"memory\": {\"total_size\": " << report.memory.totalSize << ", \"free_count\": " << report.memory.freeCount
           << ", \"free_size\": " << report.memory.freeSize << ", \"biggest_free_block\": " << report.memory.biggestFreeBlock
           << ", \"used_count\": " << report.memory.usedCount << ", \"max_used\": " << report.memory.maxUsed
           << ", \"used_percent\": " << static_cast<unsigned>(report.memory.usedPercent)
           << ", \"fragmentation_percent\": " << static_cast<unsigned>(report.memory.fragmentationPercent)
           << ", \"integrity_ok\": " << (report.memory.integrityOk ? "true" : "false") << "},\n"
           << "  \"timing\": {\n    \"insert_ns\": ";
    writeDuration(output, report.timing.insertNs, 4);
    output << ",\n    \"update_ns\": ";
    writeDuration(output, report.timing.updateNs, 4);
    output << ",\n    \"reorder_insert_ns\": ";
    writeDuration(output, report.timing.reorderInsertNs, 4);
    output << ",\n    \"filter_ns\": ";
    writeDuration(output, report.timing.filterNs, 4);
    output << ",\n    \"visible_index_rebuild_ns\": ";
    writeDuration(output, report.timing.visibleIndexRebuildNs, 4);
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
