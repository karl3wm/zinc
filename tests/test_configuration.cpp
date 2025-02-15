#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <zinc/configuration.hpp>
#include <filesystem>
#include <thread>
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

// RAII class for managing temporary directories
class TemporaryDirectory {
public:
    TemporaryDirectory() {
        // std::filesystem does not provide a function to generate unique temporary paths.
        // boost::filesystem has non-standard directory separator semantics in some cases.

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
    // RAII-managed temporary directory
    TemporaryDirectory temp_dir;

    // Change into the temporary directory
    fs::current_path(temp_dir.path());

    // Call init() and check that a .zinc directory is created
    BOOST_CHECK(zinc::Configuration::init());
    BOOST_CHECK(fs::exists(".zinc"));
}

BOOST_AUTO_TEST_CASE(path_local)
{
    // RAII-managed temporary directory
    TemporaryDirectory temp_dir;

    // Create a .zinc subdirectory
    fs::create_directory(temp_dir.path() / ".zinc");

    // Change into the temporary directory
    fs::current_path(temp_dir.path());

    // Get the local configuration path
    std::string_view path = zinc::Configuration::path_local();

    // Check that the path is correct
    BOOST_CHECK_EQUAL(path, (temp_dir.path() / ".zinc").native());
}

/*
BOOST_AUTO_TEST_CASE(path_user)
{
    // Get the user configuration path
    std::string_view path = zinc::Configuration::path_user();

    // Check that the path is correct
    fs::path expected_path = fs::path(getenv("XDG_CONFIG_HOME"));
    if (expected_path.empty()) {
        expected_path = fs::path(getenv("HOME")) / ".config";
    }
    expected_path /= "zinc";
    BOOST_CHECK_EQUAL(path, expected_path.native());
}
*/

BOOST_AUTO_TEST_CASE(path_local_subpaths)
{
    // RAII-managed temporary directory
    TemporaryDirectory temp_dir;

    // Create a .zinc subdirectory
    fs::create_directory(temp_dir.path() / ".zinc");

    // Change into the temporary directory
    fs::current_path(temp_dir.path());

    // Get the local configuration path with subpaths
    std::string_view path = zinc::Configuration::path_local(zinc::span<std::string_view>({"subdir", "subsubdir"}), true);

    // Check that the path is correct
    BOOST_CHECK_EQUAL(path, (temp_dir.path() / ".zinc" / "subdir" / "subsubdir" / "").native());
}

BOOST_AUTO_TEST_CASE(path_user_subpaths)
{
    // Get the user configuration path
    fs::path expected_path = zinc::Configuration::path_user();

    // Get the user configuration path with subpaths
    std::string_view path = zinc::Configuration::path_user(zinc::span<std::string_view>({"subdir", "subsubdir"}), true);

    expected_path /= "subdir";
    expected_path /= "subsubdir";
    expected_path /= "";
    BOOST_CHECK_EQUAL(path, expected_path.native());

    // Clean up
    fs::remove_all(expected_path.parent_path().parent_path());
}

BOOST_AUTO_TEST_SUITE_END()
