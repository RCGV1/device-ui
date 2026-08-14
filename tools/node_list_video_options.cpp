#include "NodeListVideo.h"
#include <charconv>
#include <limits>
#include <string_view>

namespace
{
bool parseUnsigned(std::string_view value, uint64_t &result)
{
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}
} // namespace

bool parseNodeListVideoCommandLine(int argc, const char *const *argv, NodeListVideoOptions &options, std::string &error)
{
    options = {};
    error.clear();
    bool sawImplementation = false;
    bool sawNodes = false;
    bool sawSeed = false;
    bool sawOutputFrames = false;
    bool sawOutputDirectory = false;

    for (int index = 1; index < argc; index += 2) {
        if (index + 1 >= argc) {
            error = "each option requires a value";
            return false;
        }
        const std::string_view flag(argv[index]);
        const std::string_view value(argv[index + 1]);
        uint64_t parsed = 0;
        if (flag == "--implementation" && !sawImplementation && (value == "legacy" || value == "virtual_candidate")) {
            options.implementation =
                value == "legacy" ? NodeListVideoImplementation::Legacy : NodeListVideoImplementation::VirtualCandidate;
            sawImplementation = true;
        } else if (flag == "--nodes" && !sawNodes && parseUnsigned(value, parsed) && parsed > 0 && parsed <= 250) {
            options.nodes = static_cast<size_t>(parsed);
            sawNodes = true;
        } else if (flag == "--seed" && !sawSeed && parseUnsigned(value, parsed) &&
                   parsed <= std::numeric_limits<uint32_t>::max()) {
            options.seed = static_cast<uint32_t>(parsed);
            sawSeed = true;
        } else if (flag == "--output-frames" && !sawOutputFrames && parseUnsigned(value, parsed) && parsed > 0 &&
                   parsed <= std::numeric_limits<size_t>::max()) {
            options.outputFrames = static_cast<size_t>(parsed);
            sawOutputFrames = true;
        } else if (flag == "--output-dir" && !sawOutputDirectory && !value.empty()) {
            options.outputDirectory = std::string(value);
            sawOutputDirectory = true;
        } else {
            error = "invalid or repeated option: " + std::string(flag);
            return false;
        }
    }

    if (!sawImplementation || !sawNodes || !sawSeed || !sawOutputFrames || !sawOutputDirectory) {
        error = "required: --implementation --nodes --seed --output-frames --output-dir";
        return false;
    }
    return true;
}
