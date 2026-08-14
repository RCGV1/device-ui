#include "NodeListBenchmark.h"
#include "libs/thorvg/rapidjson/document.h"
#include <algorithm>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

#ifdef DEVICE_UI_HEADLESS_TEST
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

    bool allocatorBlockCountStayedBounded = true;
    for (const auto &trial : report.allocatorTelemetry->trials) {
        allocatorBlockCountStayedBounded = allocatorBlockCountStayedBounded && trial.after.usedCount <= trial.before.usedCount;
        for (const auto &snapshot : trial.syncSnapshots) {
            allocatorBlockCountStayedBounded = allocatorBlockCountStayedBounded && snapshot.usedCount <= trial.before.usedCount;
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
    CHECK(document["timing"]["filter_ns"].IsNull());
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

    bool usedBlockCountStayedBounded = after["used_count"].GetUint64() <= before["used_count"].GetUint64();
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
        usedBlockCountStayedBounded =
            usedBlockCountStayedBounded && snapshot["used_count"].GetUint64() <= before["used_count"].GetUint64();
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
    CHECK(document["allocator_telemetry"].IsNull());

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
