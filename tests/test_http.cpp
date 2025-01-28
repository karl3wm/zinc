#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <span>
#include <utility>


#include <zinc/http.hpp>

void test_http_client() {
    using namespace zinc;

    // Define URLs for testing
    const std::string_view http_get_url = "http://httpbin.org/get";
    const std::string_view https_get_url = "https://jsonplaceholder.typicode.com/posts/1";
    const std::string_view http_post_url = "http://httpbin.org/post";
    const std::string_view https_post_url = "https://jsonplaceholder.typicode.com/posts";

    // Define headers for testing
    auto headers = std::to_array<HTTP::Header>({
        {"User-Agent", "zinc-http-client"},
        {"Accept", "application/json"}
    });

    // Define body for POST requests
    std::string_view post_body = R"({"title": "foo", "body": "bar", "userId": 1})";

    // Test GET with string
    std::cout << "\nTesting GET with string on HTTP URL:\n";
    std::string response_str = HTTP::request_string("GET", http_get_url, {}, headers);
    std::cout << response_str << "\n";

    std::cout << "\nTesting GET with string on HTTPS URL:\n";
    response_str = HTTP::request_string("GET", https_get_url, {}, headers);
    std::cout << response_str << "\n";

    // Test GET with generator
    std::cout << "Testing GET with generator on HTTP URL:\n";
    for (auto line : HTTP::request_lines("GET", http_get_url, {}, headers)) {
        std::cout << line << "\n";
    }

    std::cout << "\nTesting GET with generator on HTTPS URL:\n";
    for (auto line : HTTP::request_lines("GET", https_get_url, {}, headers)) {
        std::cout << line << "\n";
    }

    // Test POST with string
    std::cout << "\nTesting POST with string on HTTP URL:\n";
    response_str = HTTP::request_string("POST", http_post_url, post_body, headers);
    std::cout << response_str << "\n";

    std::cout << "\nTesting POST with string on HTTPS URL:\n";
    response_str = HTTP::request_string("POST", https_post_url, post_body, headers);
    std::cout << response_str << "\n";

    // Test POST with generator
    std::cout << "\nTesting POST with generator on HTTP URL:\n";
    for (auto line : HTTP::request_lines("POST", http_post_url, post_body, headers)) {
        std::cout << line << "\n";
    }

    std::cout << "\nTesting POST with generator on HTTPS URL:\n";
    for (auto line : HTTP::request_lines("POST", https_post_url, post_body, headers)) {
        std::cout << line << "\n";
    }
}

int main() {
    test_http_client();
    return 0;
}
