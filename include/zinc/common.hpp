#pragma once

// zinc::generator is based on c++23 std::generator,
// with the change that initial_suspend is never_suspend.
// Because of this change, passed temporaries stay in scope.
#include <zinc/__generator.hpp>

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

namespace zinc {

using StringViewPair = std::pair<std::string_view, std::string_view>;
using StringPair = std::pair<std::string, std::string>;

// Helper function to create a literal span
template <typename T>
std::span<T> span(std::initializer_list<T> contiguous)
{
    return std::span((T*)contiguous.begin(), contiguous.size());
}

// Helper functions for substrings of strings
std::span<std::pair<size_t, size_t>> find_all_of(
    std::string_view haystack,
    std::span<std::string_view> needles
);
std::pair<size_t, size_t> find_first_of(
    std::string_view haystack,
    std::span<std::string_view> needles
);
std::string_view replaced(
    std::string_view haystack,
    std::span<StringViewPair> replacements
);
std::string_view trim(std::string_view text);

// Run some commands in the user's shell
generator<std::string_view> shell(std::string_view cmdline);

}
