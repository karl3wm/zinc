#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <span>
#include <utility>
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/json/src.hpp>

#include <zinc/http.hpp>

BOOST_AUTO_TEST_SUITE(HTTPTest)

#define headers_init_list {\
    {"User-Agent", "zinc-http-client"},\
    {"Accept", "application/json"}\
}
/* testing that generators safely accept temporary arguments should go in test_generator.cpp but hasn't yet */
#define headers_temporary zinc::span<zinc::HTTP::Header>(headers_init_list)
std::vector<zinc::HTTP::Header> headers_global(headers_init_list);

// Helper function to print server responses
void printResponse(const std::string& response) {
    std::cout << "Server Response: " << response << std::endl;
}

// Helper function to parse JSON responses
boost::json::value parseJsonResponse(const std::string& response) {
    try {
        return boost::json::parse(response);
    } catch (std::exception const & e) {
        std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
        return boost::json::value();
    }
}

BOOST_AUTO_TEST_CASE(get_string_http)
{
    using namespace zinc;

    const std::string_view http_get_url = "http://httpbin.org/get";

    std::string response_str = HTTP::request_string("GET", http_get_url, {}, headers_global);
    printResponse(response_str);

    BOOST_CHECK(response_str.find("http://httpbin.org/get") != std::string::npos);
    BOOST_CHECK(response_str.find("application/json") != std::string::npos);
    BOOST_CHECK(response_str.find("zinc-http-client") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(get_string_https)
{
    using namespace zinc;

    const std::string_view https_get_url = "https://jsonplaceholder.typicode.com/posts/1";

    std::string response_str = HTTP::request_string("GET", https_get_url, {}, headers_global);
    printResponse(response_str);

    BOOST_CHECK(response_str.find("sunt aut facere repellat provident occaecati excepturi optio reprehenderit") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(get_generator_http)
{
    using namespace zinc;

    const std::string_view http_get_url = "http://httpbin.org/get";

    bool found_url = false;
    bool found_accept = false;
    bool found_user_agent = false;
    for (auto line : HTTP::request_lines("GET", http_get_url, {}, headers_temporary)) {
        if (line.find("http://httpbin.org/get") != std::string::npos) {
            found_url = true;
        }
        if (line.find("application/json") != std::string::npos) {
            found_accept = true;
        }
        if (line.find("zinc-http-client") != std::string::npos) {
            found_user_agent = true;
        }
    }
    BOOST_CHECK(found_url);
    BOOST_CHECK(found_accept);
    BOOST_CHECK(found_user_agent);
}

BOOST_AUTO_TEST_CASE(get_generator_https)
{
    using namespace zinc;

    const std::string_view https_get_url = "https://jsonplaceholder.typicode.com/posts/1";

    bool found_title = false;
    for (auto line : HTTP::request_lines("GET", https_get_url, {}, headers_temporary)) {
        if (line.find("sunt aut facere repellat provident occaecati excepturi optio reprehenderit") != std::string::npos) {
            found_title = true;
        }
    }
    BOOST_CHECK(found_title);
}

BOOST_AUTO_TEST_CASE(post_string_http)
{
    using namespace zinc;

    const std::string_view http_post_url = "http://httpbin.org/post";
    std::string_view post_body = R"({"title": "foo", "body": "bar", "userId": 1})";

    std::string response_str = HTTP::request_string("POST", http_post_url, post_body, headers_global);
    printResponse(response_str);

    boost::json::value json_response = parseJsonResponse(response_str);
    BOOST_CHECK(json_response.is_object());
    BOOST_CHECK(json_response.as_object().contains("data"));
    BOOST_CHECK(json_response.at("data").as_string() == post_body);
    BOOST_CHECK(json_response.at("url").as_string() == "http://httpbin.org/post");
    BOOST_CHECK(json_response.at("headers").at("User-Agent").as_string() == "zinc-http-client");
    BOOST_CHECK(json_response.at("headers").at("Accept").as_string() == "application/json");
}

BOOST_AUTO_TEST_CASE(post_string_https)
{
    using namespace zinc;

    const std::string_view https_post_url = "https://jsonplaceholder.typicode.com/posts";
    std::string_view post_body = R"({"title": "foo", "body": "bar", "userId": 1})";

    std::string response_str = HTTP::request_string("POST", https_post_url, post_body, headers_global);
    printResponse(response_str);

    boost::json::value json_response = parseJsonResponse(response_str);
    BOOST_CHECK(json_response.is_object());
    BOOST_CHECK(json_response.as_object().contains("id"));
}

BOOST_AUTO_TEST_CASE(post_generator_http)
{
    using namespace zinc;

    const std::string_view http_post_url = "http://httpbin.org/post";
    std::string_view post_body = R"({"title": "foo", "body": "bar", "userId": 1})";

    std::string response_str = "";
    for (auto line : HTTP::request_lines("POST", http_post_url, post_body, headers_temporary)) {
        response_str += line;
        response_str += "\n";
    }
    printResponse(response_str);

    boost::json::value json_response = parseJsonResponse(response_str);
    BOOST_CHECK(json_response.is_object());
    BOOST_CHECK(json_response.as_object().contains("data"));
    BOOST_CHECK(json_response.at("data").as_string() == post_body);
    BOOST_CHECK(json_response.at("url").as_string() == "http://httpbin.org/post");
    BOOST_CHECK(json_response.at("headers").at("User-Agent").as_string() == "zinc-http-client");
    BOOST_CHECK(json_response.at("headers").at("Accept").as_string() == "application/json");
}

BOOST_AUTO_TEST_CASE(post_generator_https)
{
    using namespace zinc;

    const std::string_view https_post_url = "https://jsonplaceholder.typicode.com/posts";
    std::string_view post_body = R"({"title": "foo", "body": "bar", "userId": 1})";

    bool found_id = false;
    for (auto line : HTTP::request_lines("POST", https_post_url, post_body, headers_temporary)) {
        if (line.find("id") != std::string::npos) {
            found_id = true;
        }
    }
    BOOST_CHECK(found_id);
}

BOOST_AUTO_TEST_SUITE_END()
