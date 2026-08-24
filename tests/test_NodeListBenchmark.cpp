#include "NodeListBenchmark.h"
#include "libs/thorvg/rapidjson/document.h"
#include <algorithm>
#include <array>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <new>
#include <string>

#ifdef DEVICE_UI_HEADLESS_TEST
namespace
{
constexpr std::array<const char *, 8> operationSequence = {
    "insert", "update", "rebind", "filter", "reorder", "scroll", "presentation_resync", "purge"};
constexpr uint64_t allocatorUsedCountSlack = 8;

int64_t expectedDelta(uint64_t beforeValue, uint64_t afterValue)
{
    return afterValue >= beforeValue ? static_cast<int64_t>(afterValue - beforeValue)
                                     : -static_cast<int64_t>(beforeValue - afterValue);
}

bool isAllocatorSnapshot(const rapidjson::Value &value)
{
    if (!value.IsObject()) {
        return false;
    }
    for (const char *field : {"free_size", "biggest_free_block", "used_count", "fragmentation_percent"}) {
        if (!value.HasMember(field) || !value[field].IsUint64()) {
            return false;
        }
    }
    return true;
}

bool isAllocatorDeltaFor(const rapidjson::Value &delta, const rapidjson::Value &before, const rapidjson::Value &after)
{
    if (!delta.IsObject()) {
        return false;
    }
    for (const char *field : {"free_size", "biggest_free_block", "used_count", "fragmentation_percent"}) {
        if (!delta.HasMember(field) || !delta[field].IsInt64()) {
            return false;
        }
    }
    return delta["free_size"].GetInt64() == expectedDelta(before["free_size"].GetUint64(), after["free_size"].GetUint64()) &&
           delta["biggest_free_block"].GetInt64() ==
               expectedDelta(before["biggest_free_block"].GetUint64(), after["biggest_free_block"].GetUint64()) &&
           delta["used_count"].GetInt64() == expectedDelta(before["used_count"].GetUint64(), after["used_count"].GetUint64()) &&
           delta["fragmentation_percent"].GetInt64() ==
               expectedDelta(before["fragmentation_percent"].GetUint64(), after["fragmentation_percent"].GetUint64());
}

bool peakMatchesSnapshots(const rapidjson::Value &peak, const rapidjson::Value &before, const rapidjson::Value &after,
                          const rapidjson::Value &snapshots)
{
    if (!isAllocatorSnapshot(peak) || !snapshots.IsArray()) {
        return false;
    }
    uint64_t minimumFreeSize = before["free_size"].GetUint64();
    uint64_t minimumBiggestBlock = before["biggest_free_block"].GetUint64();
    uint64_t maximumUsedCount = before["used_count"].GetUint64();
    uint64_t maximumFragmentation = before["fragmentation_percent"].GetUint64();
    for (const auto &snapshot : snapshots.GetArray()) {
        if (!isAllocatorSnapshot(snapshot)) {
            return false;
        }
        minimumFreeSize = std::min(minimumFreeSize, snapshot["free_size"].GetUint64());
        minimumBiggestBlock = std::min(minimumBiggestBlock, snapshot["biggest_free_block"].GetUint64());
        maximumUsedCount = std::max(maximumUsedCount, snapshot["used_count"].GetUint64());
        maximumFragmentation = std::max(maximumFragmentation, snapshot["fragmentation_percent"].GetUint64());
    }
    minimumFreeSize = std::min(minimumFreeSize, after["free_size"].GetUint64());
    minimumBiggestBlock = std::min(minimumBiggestBlock, after["biggest_free_block"].GetUint64());
    maximumUsedCount = std::max(maximumUsedCount, after["used_count"].GetUint64());
    maximumFragmentation = std::max(maximumFragmentation, after["fragmentation_percent"].GetUint64());
    return peak["free_size"].GetUint64() == minimumFreeSize && peak["biggest_free_block"].GetUint64() == minimumBiggestBlock &&
           peak["used_count"].GetUint64() == maximumUsedCount &&
           peak["fragmentation_percent"].GetUint64() == maximumFragmentation;
}

bool validOperationTelemetry(const rapidjson::Value &operation, const char *expectedName)
{
    if (!operation.IsObject() || !operation.HasMember("name") || !operation["name"].IsString() ||
        std::string(operation["name"].GetString()) != expectedName || !operation.HasMember("before") ||
        !operation.HasMember("after") || !operation.HasMember("delta") || !operation.HasMember("peak") ||
        !operation.HasMember("snapshots") || !isAllocatorSnapshot(operation["before"]) ||
        !isAllocatorSnapshot(operation["after"]) ||
        !isAllocatorDeltaFor(operation["delta"], operation["before"], operation["after"]) ||
        !peakMatchesSnapshots(operation["peak"], operation["before"], operation["after"], operation["snapshots"])) {
        return false;
    }
    for (const char *field : {"objects_before", "objects_after", "node_list_objects_before", "node_list_objects_after"}) {
        if (!operation.HasMember(field) || !operation[field].IsUint64()) {
            return false;
        }
    }
    if (!operation.HasMember("object_count_stable") || !operation["object_count_stable"].IsBool() ||
        !operation.HasMember("allocator_used_count_bounded") || !operation["allocator_used_count_bounded"].IsBool()) {
        return false;
    }
    const bool expectedObjectStable =
        operation["objects_before"].GetUint64() == operation["objects_after"].GetUint64() &&
        operation["node_list_objects_before"].GetUint64() == operation["node_list_objects_after"].GetUint64();
    const bool expectedAllocatorBounded =
        operation["after"]["used_count"].GetUint64() <= operation["before"]["used_count"].GetUint64() + allocatorUsedCountSlack;
    return operation["object_count_stable"].GetBool() == expectedObjectStable &&
           operation["allocator_used_count_bounded"].GetBool() == expectedAllocatorBounded && expectedObjectStable &&
           expectedAllocatorBounded;
}

bool validTerminalTelemetry(const rapidjson::Value &terminal, const rapidjson::Value &trialBefore,
                            const rapidjson::Value &trialAfter)
{
    if (!terminal.IsObject()) {
        return false;
    }
    for (const char *field :
         {"objects_before", "objects_after", "node_list_objects_before", "node_list_objects_after", "retained_nodes_before",
          "retained_nodes_after", "node_store_size_before", "node_store_size_after", "node_count_before", "node_count_after"}) {
        if (!terminal.HasMember(field) || !terminal[field].IsUint64()) {
            return false;
        }
    }
    for (const char *field : {"object_count_stable", "retained_nodes_stable", "node_store_size_stable", "node_count_stable",
                              "allocator_used_count_bounded", "all"}) {
        if (!terminal.HasMember(field) || !terminal[field].IsBool()) {
            return false;
        }
    }
    const bool objectStable = terminal["objects_before"].GetUint64() == terminal["objects_after"].GetUint64() &&
                              terminal["node_list_objects_before"].GetUint64() == terminal["node_list_objects_after"].GetUint64();
    const bool retainedStable = terminal["retained_nodes_before"].GetUint64() == terminal["retained_nodes_after"].GetUint64();
    const bool storeStable = terminal["node_store_size_before"].GetUint64() == terminal["node_store_size_after"].GetUint64();
    const bool nodeCountStable = terminal["node_count_before"].GetUint64() == terminal["node_count_after"].GetUint64();
    const bool allocatorBounded =
        trialAfter["used_count"].GetUint64() <= trialBefore["used_count"].GetUint64() + allocatorUsedCountSlack;
    return terminal["object_count_stable"].GetBool() == objectStable &&
           terminal["retained_nodes_stable"].GetBool() == retainedStable &&
           terminal["node_store_size_stable"].GetBool() == storeStable &&
           terminal["node_count_stable"].GetBool() == nodeCountStable &&
           terminal["allocator_used_count_bounded"].GetBool() == allocatorBounded &&
           terminal["all"].GetBool() == (objectStable && retainedStable && storeStable && nodeCountStable && allocatorBounded) &&
           terminal["all"].GetBool();
}

bool validTask7BenchmarkSchema(const rapidjson::Document &document, NodeListBenchmarkImplementation expectedImplementation,
                               size_t expectedNodes)
{
    if (!document.IsObject() || !document.HasMember("implementation") || !document["implementation"].IsObject() ||
        !document["implementation"].HasMember("name") || !document["implementation"]["name"].IsString() ||
        !document.HasMember("scenario") || !document["scenario"].IsObject() || !document["scenario"].HasMember("nodes") ||
        !document["scenario"]["nodes"].IsUint64() || document["scenario"]["nodes"].GetUint64() != expectedNodes ||
        !document.HasMember("allocator_telemetry") || !document["allocator_telemetry"].IsObject() ||
        !document["allocator_telemetry"].HasMember("trials") || !document["allocator_telemetry"]["trials"].IsArray() ||
        !document.HasMember("correctness") || !document["correctness"].IsObject()) {
        return false;
    }
    const std::string implementationName = document["implementation"]["name"].GetString();
    const bool candidate = expectedImplementation == NodeListBenchmarkImplementation::VirtualCandidate;
    if (implementationName != (candidate ? "virtual_candidate" : "legacy")) {
        return false;
    }
    const auto &trials = document["allocator_telemetry"]["trials"];
    if (trials.Size() != 2) {
        return false;
    }
    bool allNonWarmupCyclesStable = true;
    for (rapidjson::SizeType trialIndex = 0; trialIndex < trials.Size(); ++trialIndex) {
        const auto &trial = trials[trialIndex];
        if (!trial.IsObject() || !trial.HasMember("iteration") || !trial["iteration"].IsUint64() ||
            trial["iteration"].GetUint64() != trialIndex || !trial.HasMember("warmup") || !trial["warmup"].IsBool() ||
            trial["warmup"].GetBool() != (trialIndex == 0) || !trial.HasMember("before") || !trial.HasMember("after") ||
            !trial.HasMember("delta") || !trial.HasMember("peak") || !trial.HasMember("sync_snapshots") ||
            !trial.HasMember("operations") || !trial.HasMember("terminal") || !trial.HasMember("cycle_count") ||
            !trial["cycle_count"].IsUint64() || trial["cycle_count"].GetUint64() != 2 || !isAllocatorSnapshot(trial["before"]) ||
            !isAllocatorSnapshot(trial["after"]) || !isAllocatorDeltaFor(trial["delta"], trial["before"], trial["after"]) ||
            !peakMatchesSnapshots(trial["peak"], trial["before"], trial["after"], trial["sync_snapshots"]) ||
            !validTerminalTelemetry(trial["terminal"], trial["before"], trial["after"]) || !trial["operations"].IsArray() ||
            trial["operations"].Size() != operationSequence.size() * trial["cycle_count"].GetUint64()) {
            return false;
        }
        for (rapidjson::SizeType operationIndex = 0; operationIndex < trial["operations"].Size(); ++operationIndex) {
            const auto &operation = trial["operations"][operationIndex];
            if (!validOperationTelemetry(operation, operationSequence[operationIndex % operationSequence.size()])) {
                return false;
            }
            if (!trial["warmup"].GetBool()) {
                allNonWarmupCyclesStable = allNonWarmupCyclesStable && operation["object_count_stable"].GetBool() &&
                                           operation["allocator_used_count_bounded"].GetBool();
            }
        }
        if (!trial["warmup"].GetBool()) {
            allNonWarmupCyclesStable = allNonWarmupCyclesStable && trial["terminal"]["all"].GetBool();
        }
    }
    const auto &correctness = document["correctness"];
    for (const char *field : {"ready", "requested_node_count", "duplicate_update", "changed_name_and_role", "cap_purge",
                              "resync_presentation_preserved_nodes", "offscreen_update", "all"}) {
        if (!correctness.HasMember(field) || !correctness[field].IsBool()) {
            return false;
        }
    }
    bool expectedAll = correctness["ready"].GetBool() && correctness["requested_node_count"].GetBool() &&
                       correctness["duplicate_update"].GetBool() && correctness["changed_name_and_role"].GetBool() &&
                       correctness["cap_purge"].GetBool() && correctness["resync_presentation_preserved_nodes"].GetBool() &&
                       correctness["offscreen_update"].GetBool();
    if (candidate) {
        if (!correctness.HasMember("candidate") || !correctness["candidate"].IsObject()) {
            return false;
        }
        const auto &candidateChecks = correctness["candidate"];
        for (const char *field : {"pool_bounded", "object_count_stable", "allocator_churn_bounded", "presentation"}) {
            if (!candidateChecks.HasMember(field) || !candidateChecks[field].IsBool()) {
                return false;
            }
        }
        if (candidateChecks["allocator_churn_bounded"].GetBool() != allNonWarmupCyclesStable) {
            return false;
        }
        expectedAll = expectedAll && candidateChecks["pool_bounded"].GetBool() &&
                      candidateChecks["object_count_stable"].GetBool() && candidateChecks["allocator_churn_bounded"].GetBool() &&
                      candidateChecks["presentation"].GetBool();
    } else if (!correctness.HasMember("candidate") || !correctness["candidate"].IsNull()) {
        return false;
    }
    return correctness["all"].GetBool() == (expectedAll && allNonWarmupCyclesStable);
}

rapidjson::Document task7BenchmarkJson(NodeListBenchmarkImplementation implementation, size_t nodes)
{
    const auto report = runNodeListBenchmark({nodes, 1, 42, 1, implementation});
    const auto outputPath =
        std::filesystem::temp_directory_path() / ("device-ui-node-list-task7-schema-" + std::to_string(nodes) + "-" +
                                                  std::to_string(static_cast<int>(implementation)) + ".json");
    std::string error;
    REQUIRE(writeNodeListBenchmarkJson(report, outputPath.string(), error));

    std::ifstream input(outputPath, std::ios::binary);
    const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    rapidjson::Document document;
    document.Parse(json.c_str());
    REQUIRE_FALSE(document.HasParseError());

    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);
    CHECK_FALSE(removeError);
    return document;
}

void *nullBenchmarkMalloc(size_t)
{
    return nullptr;
}
} // namespace

TEST_CASE("baseline benchmark emits list diagnostics")
{
    const auto report = runNodeListBenchmark({25, 1, 42, 0});

    CHECK(report.lvgl.totalObjects > 0);
    CHECK(report.lvgl.nodeListObjects > 0);
    CHECK(report.implementation.name == "legacy");
    CHECK(report.implementation.comparisonScope.find("not hardware timing") != std::string::npos);
    CHECK_FALSE(report.lvgl.version.empty());
    CHECK_FALSE(report.source.revision.empty());
    CHECK(report.allocator.configuredBytes > 0);
    CHECK(report.memory.freeSize > 0);
    CHECK(report.memory.integrityOk);
    REQUIRE(report.timing.insertNs.trials.size() == 1);
    CHECK(report.timing.insertNs.median > 0);
    CHECK(report.timing.updateNs.median > 0);
    CHECK(report.timing.reorderInsertNs.median > 0);
    REQUIRE(report.timing.filterNs.has_value());
    CHECK(report.timing.filterNs->median > 0);
    CHECK_FALSE(report.timing.virtualRefreshNs.has_value());
    REQUIRE(report.fixtures.unsupported.size() == 1);
    CHECK(report.fixtures.unsupported.front().name == "MQTT");
    CHECK(report.correctness.ready);
    CHECK(report.correctness.resyncPresentationPreservedNodes);
    CHECK_FALSE(report.correctness.candidate.has_value());
    CHECK(report.correctness.all);
}

TEST_CASE("benchmark reset keeps final object and live memory baselines stable across trials")
{
    const auto oneTrial = runNodeListBenchmark({25, 1, 42, 0});
    const auto twoTrials = runNodeListBenchmark({25, 2, 42, 0});

    REQUIRE(oneTrial.correctness.all);
    REQUIRE(twoTrials.correctness.all);
    CHECK(oneTrial.lvgl.totalObjects == twoTrials.lvgl.totalObjects);
    CHECK(oneTrial.lvgl.nodeListObjects == twoTrials.lvgl.nodeListObjects);
    const auto difference = [](size_t left, size_t right) { return left > right ? left - right : right - left; };
    CHECK(difference(oneTrial.memory.freeSize, twoTrials.memory.freeSize) <= 512);
    CHECK(difference(oneTrial.memory.usedCount, twoTrials.memory.usedCount) <= 4);
    CHECK(oneTrial.memory.usedPercent == twoTrials.memory.usedPercent);
    CHECK(oneTrial.memory.fragmentationPercent == twoTrials.memory.fragmentationPercent);
}

TEST_CASE("benchmark CLI rejects a trial count that overflows its warm-up")
{
    const std::string tooManyTrials = std::to_string(std::numeric_limits<size_t>::max());
    const char *arguments[] = {"node_list_bench",
                               "--nodes",
                               "25",
                               "--trials",
                               tooManyTrials.c_str(),
                               "--seed",
                               "42",
                               "--json",
                               "/tmp/node-list-overflow.json"};
    NodeListBenchmarkOptions options{};
    std::string jsonPath;

    CHECK_FALSE(parseNodeListBenchmarkCommandLine(9, arguments, options, jsonPath));
}

TEST_CASE("benchmark CLI selects the virtual candidate explicitly")
{
    const char *arguments[] = {"node_list_bench",
                               "--nodes",
                               "25",
                               "--trials",
                               "1",
                               "--seed",
                               "42",
                               "--implementation",
                               "virtual_candidate",
                               "--json",
                               "/tmp/node-list-virtual-candidate.json"};
    NodeListBenchmarkOptions options{};
    std::string jsonPath;

    REQUIRE(parseNodeListBenchmarkCommandLine(11, arguments, options, jsonPath));
    CHECK(options.implementation == NodeListBenchmarkImplementation::VirtualCandidate);
    CHECK(jsonPath == "/tmp/node-list-virtual-candidate.json");
}

TEST_CASE("benchmark RSS helper preserves byte-valued ru_maxrss and expands kilobyte-valued ru_maxrss")
{
    CHECK(nodeListBenchmarkRssBytesForTesting(4096, NodeListBenchmarkRssUnit::Bytes) == 4096);
    CHECK(nodeListBenchmarkRssBytesForTesting(4096, NodeListBenchmarkRssUnit::Kilobytes) == 4096U * 1024U);
}

TEST_CASE("benchmark replacement operator new throws bad_alloc when malloc fails")
{
    CHECK_THROWS_AS(nodeListBenchmarkAllocateForNewForTesting(8, nullBenchmarkMalloc), std::bad_alloc);
}

TEST_CASE("virtual candidate benchmark reports allocator churn from real LVGL node-list syncs")
{
    NodeListBenchmarkOptions options{25, 1, 42, 0, NodeListBenchmarkImplementation::VirtualCandidate};
    const auto report = runNodeListBenchmark(options);

    CHECK(report.implementation.name == "virtual_candidate");
    CHECK(report.lvgl.totalObjects > 0);
    CHECK(report.lvgl.nodeListObjects > 0);
    REQUIRE(report.correctness.candidate.has_value());
    REQUIRE(report.allocatorTelemetry.has_value());
    REQUIRE_FALSE(report.allocatorTelemetry->trials.empty());
    CHECK(report.correctness.candidate->poolBounded);
    CHECK(report.correctness.candidate->objectCountStable);
    CHECK(report.correctness.candidate->presentation);

    constexpr uint64_t allocatorUsedCountSlack = 8;
    bool allocatorBlockCountStayedBounded = true;
    for (const auto &trial : report.allocatorTelemetry->trials) {
        if (!trial.warmup) {
            allocatorBlockCountStayedBounded =
                allocatorBlockCountStayedBounded && trial.after.usedCount <= trial.before.usedCount + allocatorUsedCountSlack;
        }
    }
    CHECK(report.correctness.candidate->allocatorChurnBounded == allocatorBlockCountStayedBounded);
    CHECK(report.correctness.all ==
          (report.correctness.ready && report.correctness.requestedNodeCount && report.correctness.duplicateUpdate &&
           report.correctness.changedNameAndRole && report.correctness.capPurge &&
           report.correctness.resyncPresentationPreservedNodes && report.correctness.offscreenUpdate &&
           report.correctness.candidate->poolBounded && report.correctness.candidate->objectCountStable &&
           allocatorBlockCountStayedBounded && report.correctness.candidate->presentation && report.memory.integrityOk));
}

TEST_CASE("virtual candidate JSON identifies candidate-only structural checks")
{
    const auto report = runNodeListBenchmark({25, 1, 42, 0, NodeListBenchmarkImplementation::VirtualCandidate});
    const auto outputPath = std::filesystem::temp_directory_path() /
                            ("device-ui-node-list-candidate-" + std::to_string(reinterpret_cast<uintptr_t>(&report)) + ".json");
    std::string error;
    REQUIRE(writeNodeListBenchmarkJson(report, outputPath.string(), error));

    std::ifstream input(outputPath, std::ios::binary);
    const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    rapidjson::Document document;
    document.Parse(json.c_str());

    REQUIRE_FALSE(document.HasParseError());
    REQUIRE(document["implementation"]["name"].IsString());
    CHECK(std::string(document["implementation"]["name"].GetString()) == "virtual_candidate");
    REQUIRE(document["timing"].HasMember("virtual_refresh_ns"));
    REQUIRE(document["timing"]["virtual_refresh_ns"].IsObject());
    REQUIRE(document["timing"]["filter_ns"].IsObject());
    REQUIRE(document["correctness"].HasMember("candidate"));
    const auto &candidate = document["correctness"]["candidate"];
    REQUIRE(candidate.IsObject());
    for (const char *field : {"pool_bounded", "object_count_stable", "presentation"}) {
        REQUIRE(candidate.HasMember(field));
        CHECK(candidate[field].IsBool());
        CHECK(candidate[field].GetBool());
    }
    REQUIRE(candidate.HasMember("allocator_churn_bounded"));
    CHECK(candidate["allocator_churn_bounded"].IsBool());
    REQUIRE(document.HasMember("allocator_telemetry"));
    const auto &allocatorTelemetry = document["allocator_telemetry"];
    REQUIRE(allocatorTelemetry.IsObject());
    REQUIRE(allocatorTelemetry.HasMember("meaning"));
    CHECK(allocatorTelemetry["meaning"].IsString());
    REQUIRE(allocatorTelemetry.HasMember("trials"));
    REQUIRE(allocatorTelemetry["trials"].IsArray());
    REQUIRE(allocatorTelemetry["trials"].Size() == 1);
    const auto &trial = allocatorTelemetry["trials"][0];
    REQUIRE(trial.IsObject());
    for (const char *section : {"before", "after", "peak"}) {
        REQUIRE(trial.HasMember(section));
        REQUIRE(trial[section].IsObject());
        for (const char *field : {"free_size", "biggest_free_block", "used_count", "fragmentation_percent"}) {
            REQUIRE(trial[section].HasMember(field));
            CHECK(trial[section][field].IsUint64());
        }
    }
    REQUIRE(trial.HasMember("delta"));
    REQUIRE(trial["delta"].IsObject());
    for (const char *field : {"free_size", "biggest_free_block", "used_count", "fragmentation_percent"}) {
        REQUIRE(trial["delta"].HasMember(field));
        CHECK(trial["delta"][field].IsInt64());
    }
    REQUIRE(trial.HasMember("sync_snapshots"));
    REQUIRE(trial["sync_snapshots"].IsArray());
    REQUIRE(trial["sync_snapshots"].Size() > 0);
    REQUIRE(trial.HasMember("operations"));
    REQUIRE(trial["operations"].IsArray());
    CHECK(trial["operations"].Size() >= 8);
    REQUIRE(trial.HasMember("iteration"));
    CHECK(trial["iteration"].IsUint64());
    REQUIRE(trial.HasMember("warmup"));
    CHECK(trial["warmup"].IsBool());

    const auto &before = trial["before"];
    const auto &after = trial["after"];
    const auto &delta = trial["delta"];
    const auto expectedDelta = [](uint64_t beforeValue, uint64_t afterValue) {
        return afterValue >= beforeValue ? static_cast<int64_t>(afterValue - beforeValue)
                                         : -static_cast<int64_t>(beforeValue - afterValue);
    };
    CHECK(delta["free_size"].GetInt64() == expectedDelta(before["free_size"].GetUint64(), after["free_size"].GetUint64()));
    CHECK(delta["biggest_free_block"].GetInt64() ==
          expectedDelta(before["biggest_free_block"].GetUint64(), after["biggest_free_block"].GetUint64()));
    CHECK(delta["used_count"].GetInt64() == expectedDelta(before["used_count"].GetUint64(), after["used_count"].GetUint64()));
    CHECK(delta["fragmentation_percent"].GetInt64() ==
          expectedDelta(before["fragmentation_percent"].GetUint64(), after["fragmentation_percent"].GetUint64()));

    constexpr uint64_t allocatorUsedCountSlack = 8;
    const bool usedBlockCountStayedBounded =
        after["used_count"].GetUint64() <= before["used_count"].GetUint64() + allocatorUsedCountSlack;
    uint64_t minimumFreeSize = before["free_size"].GetUint64();
    uint64_t minimumBiggestBlock = before["biggest_free_block"].GetUint64();
    uint64_t maximumUsedCount = before["used_count"].GetUint64();
    uint64_t maximumFragmentation = before["fragmentation_percent"].GetUint64();
    for (const auto &snapshot : trial["sync_snapshots"].GetArray()) {
        REQUIRE(snapshot.IsObject());
        REQUIRE(snapshot.HasMember("used_count"));
        REQUIRE(snapshot["used_count"].IsUint64());
        minimumFreeSize = std::min(minimumFreeSize, snapshot["free_size"].GetUint64());
        minimumBiggestBlock = std::min(minimumBiggestBlock, snapshot["biggest_free_block"].GetUint64());
        maximumUsedCount = std::max(maximumUsedCount, snapshot["used_count"].GetUint64());
        maximumFragmentation = std::max(maximumFragmentation, snapshot["fragmentation_percent"].GetUint64());
    }
    minimumFreeSize = std::min(minimumFreeSize, after["free_size"].GetUint64());
    minimumBiggestBlock = std::min(minimumBiggestBlock, after["biggest_free_block"].GetUint64());
    maximumUsedCount = std::max(maximumUsedCount, after["used_count"].GetUint64());
    maximumFragmentation = std::max(maximumFragmentation, after["fragmentation_percent"].GetUint64());
    const auto &peak = trial["peak"];
    CHECK(peak["free_size"].GetUint64() == minimumFreeSize);
    CHECK(peak["biggest_free_block"].GetUint64() == minimumBiggestBlock);
    CHECK(peak["used_count"].GetUint64() == maximumUsedCount);
    CHECK(peak["fragmentation_percent"].GetUint64() == maximumFragmentation);
    CHECK(candidate["allocator_churn_bounded"].GetBool() == usedBlockCountStayedBounded);

    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);
    CHECK_FALSE(removeError);
}

TEST_CASE("benchmark JSON records per-operation stability telemetry for all validation sizes")
{
    for (const auto implementation :
         {NodeListBenchmarkImplementation::Legacy, NodeListBenchmarkImplementation::VirtualCandidate}) {
        for (const size_t nodes : {25U, 100U, 250U}) {
            auto document = task7BenchmarkJson(implementation, nodes);
            CHECK(validTask7BenchmarkSchema(document, implementation, nodes));
            if (implementation == NodeListBenchmarkImplementation::VirtualCandidate) {
                REQUIRE(document["timing"].HasMember("filter_ns"));
                REQUIRE(document["timing"]["filter_ns"].IsObject());
            }
        }
    }
}

TEST_CASE("benchmark JSON schema validation rejects missing fields, stale deltas, stale peaks, and stale correctness")
{
    for (const auto implementation :
         {NodeListBenchmarkImplementation::Legacy, NodeListBenchmarkImplementation::VirtualCandidate}) {
        for (const size_t nodes : {25U, 100U, 250U}) {
            rapidjson::Document document = task7BenchmarkJson(implementation, nodes);
            REQUIRE(validTask7BenchmarkSchema(document, implementation, nodes));

            for (rapidjson::SizeType trialIndex = 0; trialIndex < document["allocator_telemetry"]["trials"].Size();
                 ++trialIndex) {
                rapidjson::Document missingOperations;
                missingOperations.CopyFrom(document, missingOperations.GetAllocator());
                missingOperations["allocator_telemetry"]["trials"][trialIndex].RemoveMember("operations");
                CHECK_FALSE(validTask7BenchmarkSchema(missingOperations, implementation, nodes));

                rapidjson::Document wrongOrder;
                wrongOrder.CopyFrom(document, wrongOrder.GetAllocator());
                wrongOrder["allocator_telemetry"]["trials"][trialIndex]["operations"][0]["name"].SetString(
                    "reorder", wrongOrder.GetAllocator());
                CHECK_FALSE(validTask7BenchmarkSchema(wrongOrder, implementation, nodes));

                rapidjson::Document missingOperationSnapshot;
                missingOperationSnapshot.CopyFrom(document, missingOperationSnapshot.GetAllocator());
                missingOperationSnapshot["allocator_telemetry"]["trials"][trialIndex]["operations"][0]["before"].RemoveMember(
                    "free_size");
                CHECK_FALSE(validTask7BenchmarkSchema(missingOperationSnapshot, implementation, nodes));

                rapidjson::Document staleOperationDelta;
                staleOperationDelta.CopyFrom(document, staleOperationDelta.GetAllocator());
                staleOperationDelta["allocator_telemetry"]["trials"][trialIndex]["operations"][0]["delta"]["used_count"].SetInt64(
                    123456);
                CHECK_FALSE(validTask7BenchmarkSchema(staleOperationDelta, implementation, nodes));

                rapidjson::Document staleOperationPeak;
                staleOperationPeak.CopyFrom(document, staleOperationPeak.GetAllocator());
                staleOperationPeak["allocator_telemetry"]["trials"][trialIndex]["operations"][0]["peak"]["used_count"].SetUint64(
                    0);
                CHECK_FALSE(validTask7BenchmarkSchema(staleOperationPeak, implementation, nodes));

                rapidjson::Document staleOperationStable;
                staleOperationStable.CopyFrom(document, staleOperationStable.GetAllocator());
                staleOperationStable["allocator_telemetry"]["trials"][trialIndex]["operations"][0]["object_count_stable"].SetBool(
                    false);
                CHECK_FALSE(validTask7BenchmarkSchema(staleOperationStable, implementation, nodes));

                rapidjson::Document missingTrialSnapshot;
                missingTrialSnapshot.CopyFrom(document, missingTrialSnapshot.GetAllocator());
                missingTrialSnapshot["allocator_telemetry"]["trials"][trialIndex]["before"].RemoveMember("free_size");
                CHECK_FALSE(validTask7BenchmarkSchema(missingTrialSnapshot, implementation, nodes));

                rapidjson::Document staleTrialDelta;
                staleTrialDelta.CopyFrom(document, staleTrialDelta.GetAllocator());
                staleTrialDelta["allocator_telemetry"]["trials"][trialIndex]["delta"]["used_count"].SetInt64(123456);
                CHECK_FALSE(validTask7BenchmarkSchema(staleTrialDelta, implementation, nodes));

                rapidjson::Document staleTrialPeak;
                staleTrialPeak.CopyFrom(document, staleTrialPeak.GetAllocator());
                staleTrialPeak["allocator_telemetry"]["trials"][trialIndex]["peak"]["used_count"].SetUint64(0);
                CHECK_FALSE(validTask7BenchmarkSchema(staleTrialPeak, implementation, nodes));

                rapidjson::Document staleTerminal;
                staleTerminal.CopyFrom(document, staleTerminal.GetAllocator());
                staleTerminal["allocator_telemetry"]["trials"][trialIndex]["terminal"]["allocator_used_count_bounded"].SetBool(
                    false);
                CHECK_FALSE(validTask7BenchmarkSchema(staleTerminal, implementation, nodes));
            }
        }
    }
}

TEST_CASE("benchmark API rejects an iteration count overflow")
{
    const auto report = runNodeListBenchmark({25, std::numeric_limits<size_t>::max(), 42, 1});

    CHECK_FALSE(report.correctness.all);
    CHECK(report.timing.insertNs.trials.empty());
}

TEST_CASE("benchmark JSON output is valid and contains the documented sections")
{
    const auto report = runNodeListBenchmark({25, 1, 42, 0});
    const auto outputPath = std::filesystem::temp_directory_path() /
                            ("device-ui-node-list-" + std::to_string(reinterpret_cast<uintptr_t>(&report)) + ".json");
    std::string error;
    REQUIRE(writeNodeListBenchmarkJson(report, outputPath.string(), error));

    std::ifstream input(outputPath, std::ios::binary);
    const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    rapidjson::Document document;
    document.Parse(json.c_str());

    CHECK_FALSE(document.HasParseError());
    REQUIRE(document.IsObject());
    REQUIRE(document.HasMember("schema_version"));
    CHECK(document["schema_version"].IsUint());

    REQUIRE(document.HasMember("source"));
    const auto &source = document["source"];
    REQUIRE(source.IsObject());
    REQUIRE(source.HasMember("revision"));
    CHECK(source["revision"].IsString());
    REQUIRE(source.HasMember("dirty"));
    CHECK(source["dirty"].IsBool());

    REQUIRE(document.HasMember("implementation"));
    const auto &implementation = document["implementation"];
    REQUIRE(implementation.IsObject());
    REQUIRE(implementation.HasMember("name"));
    CHECK(implementation["name"].IsString());
    REQUIRE(implementation.HasMember("comparison_scope"));
    CHECK(implementation["comparison_scope"].IsString());
    CHECK(std::string(implementation["comparison_scope"].GetString()).find("not hardware timing") != std::string::npos);

    REQUIRE(document.HasMember("lvgl"));
    const auto &lvgl = document["lvgl"];
    REQUIRE(lvgl.IsObject());
    REQUIRE(lvgl.HasMember("version"));
    CHECK(lvgl["version"].IsString());
    REQUIRE(lvgl.HasMember("total_objects"));
    CHECK(lvgl["total_objects"].IsUint64());
    REQUIRE(lvgl.HasMember("node_list_objects"));
    CHECK(lvgl["node_list_objects"].IsUint64());

    REQUIRE(document.HasMember("allocator"));
    const auto &allocator = document["allocator"];
    REQUIRE(allocator.IsObject());
    REQUIRE(allocator.HasMember("implementation"));
    CHECK(allocator["implementation"].IsString());
    REQUIRE(allocator.HasMember("configured_bytes"));
    CHECK(allocator["configured_bytes"].IsUint64());
    REQUIRE(allocator.HasMember("ram_size_bytes"));
    CHECK(allocator["ram_size_bytes"].IsUint64());

    REQUIRE(document.HasMember("scenario"));
    const auto &scenario = document["scenario"];
    REQUIRE(scenario.IsObject());
    REQUIRE(scenario.HasMember("nodes"));
    CHECK(scenario["nodes"].IsUint64());
    REQUIRE(scenario.HasMember("trials"));
    CHECK(scenario["trials"].IsUint64());
    REQUIRE(scenario.HasMember("seed"));
    CHECK(scenario["seed"].IsUint());
    REQUIRE(scenario.HasMember("warmup"));
    CHECK(scenario["warmup"].IsUint64());

    REQUIRE(document.HasMember("fixtures"));
    const auto &fixtures = document["fixtures"];
    REQUIRE(fixtures.IsObject());
    REQUIRE(fixtures.HasMember("unsupported"));
    REQUIRE(fixtures["unsupported"].IsArray());
    REQUIRE(fixtures["unsupported"].Size() == 1);
    const auto &unsupported = fixtures["unsupported"][0];
    REQUIRE(unsupported.IsObject());
    REQUIRE(unsupported.HasMember("name"));
    CHECK(unsupported["name"].IsString());
    REQUIRE(unsupported.HasMember("reason"));
    REQUIRE(unsupported["reason"].IsString());
    CHECK(unsupported["reason"].GetStringLength() > 0);

    REQUIRE(document.HasMember("memory"));
    const auto &memory = document["memory"];
    REQUIRE(memory.IsObject());
    for (const char *field : {"total_size", "free_count", "free_size", "biggest_free_block", "used_count", "max_used",
                              "used_percent", "fragmentation_percent"}) {
        REQUIRE(memory.HasMember(field));
        CHECK(memory[field].IsUint64());
    }
    REQUIRE(memory.HasMember("integrity_ok"));
    CHECK(memory["integrity_ok"].IsBool());

    REQUIRE(document.HasMember("allocator_telemetry"));
    REQUIRE(document["allocator_telemetry"].IsObject());
    REQUIRE(document["allocator_telemetry"].HasMember("trials"));
    CHECK(document["allocator_telemetry"]["trials"].IsArray());

    REQUIRE(document.HasMember("timing"));
    const auto &timing = document["timing"];
    REQUIRE(timing.IsObject());
    for (const char *operation : {"insert_ns", "update_ns", "reorder_insert_ns", "filter_ns"}) {
        REQUIRE(timing.HasMember(operation));
        const auto &entry = timing[operation];
        REQUIRE(entry.IsObject());
        REQUIRE(entry.HasMember("trials"));
        REQUIRE(entry["trials"].IsArray());
        for (const auto &trial : entry["trials"].GetArray()) {
            CHECK(trial.IsUint64());
        }
        REQUIRE(entry.HasMember("raw_ns"));
        CHECK(entry["raw_ns"].IsArray());
        for (const char *summary : {"median", "p95", "maximum"}) {
            REQUIRE(entry.HasMember(summary));
            CHECK(entry[summary].IsUint64());
        }
    }
    REQUIRE(timing.HasMember("virtual_refresh_ns"));
    CHECK(timing["virtual_refresh_ns"].IsNull());

    REQUIRE(document.HasMember("correctness"));
    const auto &correctness = document["correctness"];
    REQUIRE(correctness.IsObject());
    for (const char *field : {"ready", "requested_node_count", "duplicate_update", "changed_name_and_role", "cap_purge",
                              "resync_presentation_preserved_nodes", "offscreen_update", "all"}) {
        REQUIRE(correctness.HasMember(field));
        CHECK(correctness[field].IsBool());
    }
    REQUIRE(correctness.HasMember("candidate"));
    CHECK(correctness["candidate"].IsNull());

    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);
    CHECK_FALSE(removeError);
}
#endif
