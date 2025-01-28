#pragma once

// zinc::generator is based on c++23 std::generator,
// with the change that initial_suspend is never_suspend.
// Because of this change, passed temporaries stay in scope.
#include <zinc/__generator.hpp>

#include <initializer_list>
#include <span>

namespace zinc {

// Helper function to create a literal span
template <typename T>
std::span<T const> span(std::initializer_list<T> const & contiguous)
{
    return std::span((T const*)contiguous.begin(), contiguous.size());
}

}
