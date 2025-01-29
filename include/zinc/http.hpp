#pragma once

#include <string>
#include <string_view>

#include <zinc/common.hpp>

namespace zinc {

class HTTP {
public:
    using Header = StringViewPair;

    // Perform an HTTP request (GET or POST) with custom headers and yield lines as they are received
    static zinc::generator<std::string_view> request_lines(
        std::string_view method,
        std::string_view url,
        std::string_view body = {},
        std::span<Header const> headers = {}
    );

    // Perform an HTTP request (GET or POST) with custom headers and return the entire response as a string
    static std::string request_string(
        std::string_view method,
        std::string_view url,
        std::string_view body = {},
        std::span<Header const> headers = {}
    );
};

} // namespace zinc
