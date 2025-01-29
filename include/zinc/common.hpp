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

using std::generator;

using StringViewPair = std::pair<std::string_view, std::string_view>;
using StringPair = std::pair<std::string, std::string>;

// Helper function to create a literal span
template <typename T>
std::span<T const> span(std::initializer_list<T> const & contiguous)
{
    return std::span((T const*)contiguous.begin(), contiguous.size());
}

// Helper functions to replace substrings
std::string replaced(
    std::string_view haystack,
    std::span<StringViewPair const> replacements
);

}

// vim: set ts=4 sw=4 et:
