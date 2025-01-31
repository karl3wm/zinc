#pragma once

#include <span>
#include <string_view>

#include <zinc/common.hpp>

namespace zinc {

class Log {
public:
    static void log(std::span<StringViewPair const> fields);
};

} // namespace zinc
