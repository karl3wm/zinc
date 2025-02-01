#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <zinc/log.hpp>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;

BOOST_AUTO_TEST_SUITE(LogTest)

BOOST_AUTO_TEST_CASE(log_file_creation)
{
    // Create a temporary directory
    fs::path temp_dir = fs::temp_directory_path() / "zinc-test";
    fs::create_directory(temp_dir);

    // Create a temporary .zinc folder
    fs::path zinc_dir = temp_dir / ".zinc";
    fs::create_directory(zinc_dir);

    // Change into the temporary directory
    fs::current_path(temp_dir);

    // Log something
    zinc::Log::log(zinc::span<zinc::StringViewPair>({
        {"key", "value"}
    }));

    // Check that a log file was created
    fs::path logs_dir = zinc_dir / "logs";
    BOOST_CHECK(fs::exists(logs_dir));
    BOOST_CHECK(!fs::directory_iterator(logs_dir)->path().empty());
}

BOOST_AUTO_TEST_CASE(log_file_name)
{
    // Create a temporary directory
    fs::path temp_dir = fs::temp_directory_path() / "zinc-test";
    fs::create_directory(temp_dir);

    // Create a temporary .zinc folder
    fs::path zinc_dir = temp_dir / ".zinc";
    fs::create_directory(zinc_dir);

    // Change into the temporary directory
    fs::current_path(temp_dir);

    // Log something
    zinc::Log::log(zinc::span<zinc::StringViewPair>({
        {"key", "value"}
    }));

    // Check that the log file name is in the correct format
    fs::path logs_dir = zinc_dir / "logs";
    fs::path log_file = fs::directory_iterator(logs_dir)->path();
    std::string log_file_name = log_file.filename().string();
    BOOST_CHECK(log_file_name.size() == 24); // YYYY-MM-DDTHHMMSSZ.log
    BOOST_CHECK(log_file_name.substr(0, 4).find_first_not_of("0123456789") == std::string::npos); // Year
    BOOST_CHECK(log_file_name.substr(4, 1) == "-");
    BOOST_CHECK(log_file_name.substr(5, 2).find_first_not_of("0123456789") == std::string::npos); // Month
    BOOST_CHECK(log_file_name.substr(7, 1) == "-");
    BOOST_CHECK(log_file_name.substr(8, 2).find_first_not_of("0123456789") == std::string::npos); // Day
    BOOST_CHECK(log_file_name.substr(10, 1) == "T");
    BOOST_CHECK(log_file_name.substr(11, 2).find_first_not_of("0123456789") == std::string::npos); // Hour
    BOOST_CHECK(log_file_name.substr(13, 1) == ":");
    BOOST_CHECK(log_file_name.substr(14, 2).find_first_not_of("0123456789") == std::string::npos); // Minute
    BOOST_CHECK(log_file_name.substr(16, 1) == ":");
    BOOST_CHECK(log_file_name.substr(17, 2).find_first_not_of("0123456789") == std::string::npos); // Second
    BOOST_CHECK(log_file_name.substr(19, 1) == "Z");
    BOOST_CHECK(log_file_name.substr(20) == ".log");
}

BOOST_AUTO_TEST_CASE(log_file_contents)
{
    // Create a temporary directory
    fs::path temp_dir = fs::temp_directory_path() / "zinc-test";
    fs::create_directory(temp_dir);

    // Create a temporary .zinc folder
    fs::path zinc_dir = temp_dir / ".zinc";
    fs::create_directory(zinc_dir);

    // Change into the temporary directory
    fs::current_path(temp_dir);

    // Log something
    zinc::Log::log(zinc::span<zinc::StringViewPair>({
        {"key", "value"}
    }));

    // Check that the log file contains the expected contents
    fs::path logs_dir = zinc_dir / "logs";
    fs::path log_file = fs::directory_iterator(logs_dir)->path();
    std::ifstream log_stream(log_file);
    std::string log_contents((std::istreambuf_iterator<char>(log_stream)), std::istreambuf_iterator<char>());
    BOOST_CHECK(log_contents.find("\"key\":\"value\"") != std::string::npos);
    BOOST_CHECK(log_contents.find("\"ts\":") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(log_multiple_fields)
{
    // Create a temporary directory
    fs::path temp_dir = fs::temp_directory_path() / "zinc-test";
    fs::create_directory(temp_dir);

    // Create a temporary .zinc folder
    fs::path zinc_dir = temp_dir / ".zinc";
    fs::create_directory(zinc_dir);

    // Change into the temporary directory
    fs::current_path(temp_dir);

    // Log something with multiple fields
    zinc::Log::log(zinc::span<zinc::StringViewPair>({
        {"key1", "value1"},
        {"key2", "value2"}
    }));

    // Check that the log file contains the expected contents
    fs::path logs_dir = zinc_dir / "logs";
    fs::path log_file = fs::directory_iterator(logs_dir)->path();
    std::ifstream log_stream(log_file);
    std::string log_contents((std::istreambuf_iterator<char>(log_stream)), std::istreambuf_iterator<char>());
    BOOST_CHECK(log_contents.find("\"key1\":\"value1\"") != std::string::npos);
    BOOST_CHECK(log_contents.find("\"key2\":\"value2\"") != std::string::npos);
    BOOST_CHECK(log_contents.find("\"ts\":") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
