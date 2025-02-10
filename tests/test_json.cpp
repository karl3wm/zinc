#define BOOST_TEST_MODULE JSONTest
#include <boost/test/included/unit_test.hpp>
#include <zinc/json.hpp>
#include <string_view>
#include <span>

using namespace zinc;

BOOST_AUTO_TEST_CASE(test_decode_encode_simple_object) {
    const std::string_view input = R"({"key":"value"})";
    JSON::Doc doc = JSON::decode(input);
    JSON& root = *doc;
    BOOST_CHECK_EQUAL(root.encode(), input);
}

BOOST_AUTO_TEST_CASE(test_access_object_element) {
    const std::string_view input = R"({"key":42})";
    JSON::Doc doc = JSON::decode(input);
    JSON& root = *doc;
    
    const JSON& value = root["key"];
    BOOST_REQUIRE(std::holds_alternative<long>(value));
    BOOST_CHECK_EQUAL(std::get<long>(value), 42);
}

BOOST_AUTO_TEST_CASE(test_null_type) {
    JSON::Doc doc = JSON::decode("null");
    JSON& root = *doc;
    BOOST_CHECK(std::holds_alternative<std::nullptr_t>(root));
    BOOST_CHECK_EQUAL(root.encode(), "null");
}

BOOST_AUTO_TEST_CASE(test_boolean_types) {
    {
        JSON::Doc doc = JSON::decode("true");
        JSON& root = *doc;
        BOOST_CHECK(std::holds_alternative<bool>(root));
        BOOST_CHECK_EQUAL(std::get<bool>(root), true);
    }
    {
        JSON::Doc doc = JSON::decode("false");
        JSON& root = *doc;
        BOOST_CHECK(std::holds_alternative<bool>(root));
        BOOST_CHECK_EQUAL(std::get<bool>(root), false);
    }
}

BOOST_AUTO_TEST_CASE(test_number_types) {
    {
        JSON::Doc doc = JSON::decode("12345");
        JSON& root = *doc;
        BOOST_CHECK(std::holds_alternative<long>(root));
        BOOST_CHECK_EQUAL(std::get<long>(root), 12345);
    }
    {
        JSON::Doc doc = JSON::decode("123.45");
        JSON& root = *doc;
        BOOST_CHECK(std::holds_alternative<double>(root));
        BOOST_CHECK_CLOSE(std::get<double>(root), 123.45, 1e-6);
    }
}

BOOST_AUTO_TEST_CASE(test_string_type) {
    const std::string_view input = R"("hello\u0020world")";
    JSON::Doc doc = JSON::decode(input);
    JSON& root = *doc;
    BOOST_CHECK(std::holds_alternative<std::string_view>(root));
    BOOST_CHECK_EQUAL(std::get<std::string_view>(root), "hello world");
}

BOOST_AUTO_TEST_CASE(test_array_type) {
    const std::string_view input = R"([1,"two",3.0])";
    JSON::Doc doc = JSON::decode(input);
    JSON& root = *doc;
    
    const auto& arr = root.array();
    BOOST_REQUIRE_EQUAL(arr.size(), 3);
    BOOST_CHECK(std::holds_alternative<long>(arr[0]));
    BOOST_CHECK(std::holds_alternative<std::string_view>(arr[1]));
    BOOST_CHECK(std::holds_alternative<double>(arr[2]));
}

BOOST_AUTO_TEST_CASE(test_object_type) {
    const std::string_view input = R"({"a":1,"b":"two"})";
    JSON::Doc doc = JSON::decode(input);
    JSON& root = *doc;
    
    const auto& obj = root.object();
    BOOST_REQUIRE_EQUAL(obj.size(), 2);
    BOOST_CHECK((obj[0].first == "a" && std::get<long>(obj[0].second) == 1) ||
                (obj[1].first == "a" && std::get<long>(obj[1].second) == 1));
}

BOOST_AUTO_TEST_CASE(test_comparison_operators) {
    JSON::Doc doc1 = JSON::decode("42");
    JSON::Doc doc2 = JSON::decode("42");
    JSON::Doc doc3 = JSON::decode("43");
    
    BOOST_CHECK(*doc1 == *doc2);
    BOOST_CHECK((*doc1 <=> *doc3) == std::partial_ordering::less);
}

BOOST_AUTO_TEST_CASE(test_nested_structures) {
    const std::string_view input = R"({"arr":[1,{"nested":true}],"obj":{}})";
    JSON::Doc doc = JSON::decode(input);
    JSON& root = *doc;
    
    const JSON& arr = root["arr"];
    const auto& elements = arr.array();
    BOOST_REQUIRE_EQUAL(elements.size(), 2);
    BOOST_CHECK(std::holds_alternative<std::span<KeyJSONPair>>(elements[1]));
}

BOOST_AUTO_TEST_CASE(test_edge_cases) {
    {
        JSON::Doc doc = JSON::decode("[]");
        BOOST_CHECK_EQUAL((*doc).encode(), "[]");
    }
    {
        JSON::Doc doc = JSON::decode("{}");
        BOOST_CHECK_EQUAL((*doc).encode(), "{}");
    }
}
