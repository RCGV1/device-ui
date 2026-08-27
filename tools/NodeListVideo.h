#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

struct NodeListVideoOptions {
    size_t nodes = 25;
    uint32_t seed = 1;
    size_t outputFrames = 1;
    std::string outputDirectory;
};

bool parseNodeListVideoCommandLine(int argc, const char *const *argv, NodeListVideoOptions &options, std::string &error);
