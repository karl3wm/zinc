#pragma once

#include <string>
#include <string_view>
#include <generator>
#include <span>

namespace zinc {

class Http {
public:
    // Perform an HTTP request (GET or POST) with custom headers and yield lines as they are received
    static std::generator<std::string_view> request(std::string_view method, std::string_view url, std::span<const std::pair<std::string_view, std::string_view>> headers = {}, std::string_view body = {});

    // Perform an HTTP request (GET or POST) with custom headers and return the entire response as a string
    static std::string request_string(std::string_view method, std::string_view url, std::span<const std::pair<std::string_view, std::string_view>> headers = {}, std::string_view body = {});
};

} // namespace zinc
