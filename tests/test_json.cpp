#define BOOST_TEST_MODULE JSONTest
#include <boost/test/unit_test.hpp>
#include <zinc/json.hpp>
#include <string_view>
#include <span>
#include <stdexcept>

using namespace zinc;

BOOST_AUTO_TEST_CASE(test_decode_encode_simple_object) {
    const std::string_view input = R"({"key":"value"})";
    JSON::Doc doc = JSON::decode(input);
    JSON& root = *doc;
    BOOST_TEST(root.encode() == input);
}

BOOST_AUTO_TEST_CASE(test_access_object_element) {
    const std::string_view input = R"({"key":42})";
    JSON::Doc doc = JSON::decode(input);
    JSON& root = *doc;

    const JSON& value = root["key"];
    BOOST_TEST(std::holds_alternative<long>(value));
    BOOST_TEST(std::get<long>(value) == 42);
}

BOOST_AUTO_TEST_CASE(test_null_type) {
    JSON::Doc doc = JSON::decode("null");
    JSON& root = *doc;
    BOOST_TEST(std::holds_alternative<std::nullptr_t>(root));
    BOOST_TEST(root.encode() == "null");
}

BOOST_AUTO_TEST_CASE(test_boolean_types) {
    {
        JSON::Doc doc = JSON::decode("true");
        JSON& root = *doc;
        BOOST_TEST(std::holds_alternative<bool>(root));
        BOOST_TEST(std::get<bool>(root) == true);
    }
    {
        JSON::Doc doc = JSON::decode("false");
        JSON& root = *doc;
        BOOST_TEST(std::holds_alternative<bool>(root));
        BOOST_TEST(std::get<bool>(root) == false);
    }
}

BOOST_AUTO_TEST_CASE(test_number_types) {
    {
        JSON::Doc doc = JSON::decode("12345");
        JSON& root = *doc;
        BOOST_TEST(std::holds_alternative<long>(root));
        BOOST_TEST(std::get<long>(root) == 12345);
    }
    {
        JSON::Doc doc = JSON::decode("123.45");
        JSON& root = *doc;
        BOOST_TEST(std::holds_alternative<double>(root));
        BOOST_TEST(std::get<double>(root) == 123.45, boost::test_tools::tolerance(1e-6));
    }
}

BOOST_AUTO_TEST_CASE(test_string_type) {
    const std::string_view input = R"("hello\u0020world")";
    JSON::Doc doc = JSON::decode(input);
    JSON& root = *doc;
    BOOST_TEST(std::holds_alternative<std::string_view>(root));
    BOOST_TEST(std::get<std::string_view>(root) == "hello world");
}

BOOST_AUTO_TEST_CASE(test_array_type) {
    const std::string_view input = R"([1,"two",3.0])";
    JSON::Doc doc = JSON::decode(input);
    JSON& root = *doc;

    const auto& arr = root.array();
    BOOST_REQUIRE_EQUAL(arr.size(), 3);
    BOOST_TEST(std::holds_alternative<long>(arr[0]));
    BOOST_TEST(std::get<long>(arr[0]) == 1);
    BOOST_TEST(std::holds_alternative<std::string_view>(arr[1]));
    BOOST_TEST(std::get<std::string_view>(arr[1]) == "two");
    BOOST_TEST(std::holds_alternative<double>(arr[2]));
    BOOST_TEST(std::get<double>(arr[2]) == 3.0);
}

BOOST_AUTO_TEST_CASE(test_object_type) {
    const std::string_view input = R"({"a":1,"b":"two"})";
    JSON::Doc doc = JSON::decode(input);
    JSON& root = *doc;

    const auto& obj = root.object();
    BOOST_REQUIRE_EQUAL(obj.size(), 2);
    BOOST_TEST(obj[0].first == "a");
    BOOST_TEST(std::get<long>(obj[0].second) == 1);
    BOOST_TEST(obj[1].first == "b");
    BOOST_TEST(std::get<std::string_view>(obj[1].second) == "two");
}

BOOST_AUTO_TEST_CASE(test_comparison_operators) {
    JSON::Doc doc1 = JSON::decode("42");
    JSON::Doc doc2 = JSON::decode("42");
    JSON::Doc doc3 = JSON::decode("43");

    BOOST_TEST((*doc1 == *doc2));
    BOOST_TEST(((*doc1 <=> *doc3) == std::partial_ordering::less));
}

BOOST_AUTO_TEST_CASE(test_nested_structures) {
    const std::string_view input = R"({"arr":[1,{"nested":true}],"obj":{}})";
    JSON::Doc doc = JSON::decode(input);
    JSON& root = *doc;

    const JSON& arr = root["arr"];
    const auto& elements = arr.array();
    BOOST_REQUIRE_EQUAL(elements.size(), 2);
    BOOST_TEST(std::holds_alternative<long>(elements[0]));
    BOOST_TEST(std::get<long>(elements[0]) == 1);
    BOOST_TEST(std::holds_alternative<std::span<KeyJSONPair>>(elements[1]));
    const JSON& nestedObj = elements[1];
    BOOST_TEST(std::holds_alternative<bool>(nestedObj["nested"]));
    BOOST_TEST(std::get<bool>(nestedObj["nested"]) == true);

    const JSON& obj = root["obj"];
    BOOST_TEST(std::holds_alternative<std::span<KeyJSONPair>>(obj));
    BOOST_TEST(obj.object().empty());
}

BOOST_AUTO_TEST_CASE(test_edge_cases) {
    {
        JSON::Doc doc = JSON::decode("[]");
        BOOST_TEST((*doc).encode() == "[]");
    }
    {
        JSON::Doc doc = JSON::decode("{}");
        BOOST_TEST((*doc).encode() == "{}");
    }
}

// Additional tests for edge cases and invalid inputs
BOOST_AUTO_TEST_CASE(test_invalid_json) {
    BOOST_CHECK_THROW(JSON::decode("{invalid_json}"), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(test_large_numbers) {
    JSON::Doc doc = JSON::decode("9223372036854775807"); // Maximum long value
    JSON& root = *doc;
    BOOST_TEST(std::holds_alternative<long>(root));
    BOOST_TEST(std::get<long>(root) == 9223372036854775807L);
}

BOOST_AUTO_TEST_CASE(test_deeply_nested_structures) {
    const std::string_view input = R"({"level1":{"level2":{"level3":42}}})";
    JSON::Doc doc = JSON::decode(input);
    JSON& root = *doc;

    const JSON& level1 = root["level1"];
    const JSON& level2 = level1["level2"];
    const JSON& level3 = level2["level3"];
    BOOST_TEST(std::holds_alternative<long>(level3));
    BOOST_TEST(std::get<long>(level3) == 42);
}
