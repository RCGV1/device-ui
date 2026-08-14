#include "NodeListVideo.h"
#include <doctest/doctest.h>

#ifdef DEVICE_UI_HEADLESS_TEST
TEST_CASE("node list video CLI accepts one deterministic implementation capture")
{
    const char *arguments[] = {
        "node_list_video", "--implementation",    "virtual_candidate", "--nodes", "25", "--seed", "42", "--output-frames", "9",
        "--output-dir",    "/tmp/node-list-video"};
    NodeListVideoOptions options{};
    std::string error;

    REQUIRE(parseNodeListVideoCommandLine(11, arguments, options, error));
    CHECK(options.implementation == NodeListVideoImplementation::VirtualCandidate);
    CHECK(options.nodes == 25);
    CHECK(options.seed == 42);
    CHECK(options.outputFrames == 9);
    CHECK(options.outputDirectory == "/tmp/node-list-video");
}

TEST_CASE("node list video CLI rejects incomplete and unknown arguments")
{
    const char *incomplete[] = {"node_list_video", "--implementation", "legacy", "--nodes", "25"};
    NodeListVideoOptions options{};
    std::string error;
    CHECK_FALSE(parseNodeListVideoCommandLine(5, incomplete, options, error));

    const char *unknown[] = {
        "node_list_video", "--implementation", "legacy",  "--nodes", "25", "--seed", "42", "--output-frames", "9",
        "--output-dir",    "/tmp/out",         "--bogus", "no"};
    CHECK_FALSE(parseNodeListVideoCommandLine(13, unknown, options, error));
}
#endif
