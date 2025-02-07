#pragma once

#include <zinc/common.hpp>

#include <filesystem>
#include <fstream>

namespace zinc {

class UnifiedDiffGenerator {
public:
    static generator<std::string_view> generateDiff(
        const std::filesystem::path& filepath,
        std::ifstream& file,
        generator<std::string_view> newContent,
        size_t context_lines = 3
    );
};

} // namespace zinc
