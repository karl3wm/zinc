#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include <zinc/configuration.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

// RAII class for managing temporary directories
class TemporaryDirectory {
public:
    TemporaryDirectory() {
        // Generate a unique directory name using PID, thread ID, and timestamp
        auto pid = getpid();
        auto tid = std::this_thread::get_id();
        static auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        std::ostringstream oss;
        oss << "zinc-test-" << pid << "-" << tid << "-" << timestamp;
        path_ = fs::temp_directory_path() / oss.str();

        // Create the directory
        fs::create_directory(path_);
    }

    ~TemporaryDirectory() {
        // Clean up: remove the directory and its contents
        fs::remove_all(path_);
    }

    const fs::path& path() const {
        return path_;
    }

private:
    fs::path path_;
};

BOOST_AUTO_TEST_SUITE(ConfigurationTest)

BOOST_AUTO_TEST_CASE(init)
{
    TemporaryDirectory temp_dir;
    fs::current_path(temp_dir.path());

    BOOST_CHECK(zinc::Configuration::init());
    BOOST_CHECK(fs::exists(".zinc"));
}

BOOST_AUTO_TEST_CASE(path_local)
{
    TemporaryDirectory temp_dir;
    fs::create_directory(temp_dir.path() / ".zinc");
    fs::current_path(temp_dir.path());

    std::string_view path = zinc::Configuration::path_local();
    BOOST_CHECK_EQUAL(path, (temp_dir.path() / ".zinc").native());
}

BOOST_AUTO_TEST_CASE(path_user)
{
    fs::path expected_path;
    if (const char* xdg_config_home = getenv("XDG_CONFIG_HOME")) {
        expected_path = fs::path(xdg_config_home);
    } else if (const char* home = getenv("HOME")) {
        expected_path = fs::path(home) / ".config";
    } else {
        BOOST_FAIL("Neither XDG_CONFIG_HOME nor HOME is set.");
    }
    expected_path /= "zinc";

    std::string_view path = zinc::Configuration::path_user();
    BOOST_CHECK_EQUAL(path, expected_path.native());
}

BOOST_AUTO_TEST_CASE(path_local_subpaths)
{
    TemporaryDirectory temp_dir;
    fs::create_directory(temp_dir.path() / ".zinc");
    fs::current_path(temp_dir.path());

    std::string_view path = zinc::Configuration::path_local(zinc::span<std::string_view>({"subdir", "subsubdir"}), true);
    BOOST_CHECK_EQUAL(path, (temp_dir.path() / ".zinc" / "subdir" / "subsubdir" / "").native());
}

BOOST_AUTO_TEST_CASE(path_user_subpaths)
{
    fs::path expected_path = zinc::Configuration::path_user();
    std::string_view path = zinc::Configuration::path_user(zinc::span<std::string_view>({"subdir", "subsubdir"}), true);

    expected_path /= "subdir";
    expected_path /= "subsubdir";
    expected_path /= "";
    BOOST_CHECK_EQUAL(path, expected_path.native());

    fs::remove_all(expected_path.parent_path().parent_path());
}

BOOST_AUTO_TEST_CASE(default_to_user_parameters)
{
    TemporaryDirectory temp_dir;
    fs::current_path(temp_dir.path());

    // Create a user config file with some parameters
    fs::create_directories(zinc::Configuration::path_user());
    std::ofstream user_config(std::string(zinc::Configuration::path_user(zinc::span<std::string_view>({"test.ini"}))));
    user_config << "[section]\nkey = value_from_user\n";
    user_config.close();

    // Create a local config file with the same section but different key
    fs::create_directories(zinc::Configuration::path_local());
    std::ofstream local_config(std::string(zinc::Configuration::path_local(zinc::span<std::string_view>({"test.ini"}))));
    local_config << "[section]\nother_key = value_from_local\n";
    local_config.close();

    // Inspect the local config file
    zinc::Configuration config(zinc::span<std::string_view>({"test.ini"}));

    // Check that the key from the user config is used
    BOOST_CHECK_EQUAL(config[zinc::span<std::string_view>({"section", "key"})], "value_from_user");

    // Check that the local-only key is used
    BOOST_CHECK_EQUAL(config[zinc::span<std::string_view>({"section", "other_key"})], "value_from_local");
}

BOOST_AUTO_TEST_CASE(write_changed_parameters)
{
    TemporaryDirectory temp_dir;
    fs::current_path(temp_dir.path());

    // Create a user config file with some parameters
    fs::create_directories(zinc::Configuration::path_user());
    std::ofstream user_config(std::string(zinc::Configuration::path_user(zinc::span<std::string_view>({"test.ini"}))));
    user_config << "[section]\nkey = value_from_user\n";
    user_config.close();

    // Create a local config file
    fs::create_directories(zinc::Configuration::path_local());
    std::ofstream local_config(std::string(zinc::Configuration::path_local(zinc::span<std::string_view>({"test.ini"}))));
    local_config << "[section]\nother_key = value_from_local\n";
    local_config.close();

    // Modify a parameter from the user config
    {
        zinc::Configuration config(zinc::span<std::string_view>({"test.ini"}));
        config[zinc::span<std::string_view>({"section", "key"})] = "new_value";
    }

    // Verify that the change was written to the local config file
    std::ifstream local_config_updated(std::string(zinc::Configuration::path_local(zinc::span<std::string_view>({"test.ini"}))));
    std::string content((std::istreambuf_iterator<char>(local_config_updated)), std::istreambuf_iterator<char>());
    BOOST_CHECK(content.find("key = new_value") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(do_not_write_unmodified_parameters)
{
    TemporaryDirectory temp_dir;
    fs::current_path(temp_dir.path());

    // Create a user config file with some parameters
    fs::create_directories(zinc::Configuration::path_user());
    std::ofstream user_config(std::string(zinc::Configuration::path_user(zinc::span<std::string_view>({"test.ini"}))));
    user_config << "[section]\nkey = value_from_user\n";
    user_config.close();

    // Create a local config file
    fs::create_directories(zinc::Configuration::path_local());
    std::ofstream local_config(std::string(zinc::Configuration::path_local(zinc::span<std::string_view>({"test.ini"}))));
    local_config << "[section]\nother_key = value_from_local\n";
    local_config.close();

    // Read a parameter from the user config without modifying it
    {
        zinc::Configuration config(zinc::span<std::string_view>({"test.ini"}));
        std::string value = config[zinc::span<std::string_view>({"section", "key"})];
    }

    // Verify that the user config was not written to the local config file
    std::ifstream local_config_updated(std::string(zinc::Configuration::path_local(zinc::span<std::string_view>({"test.ini"}))));
    std::string content((std::istreambuf_iterator<char>(local_config_updated)), std::istreambuf_iterator<char>());
    BOOST_CHECK(content.find("key = value_from_user") == std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
