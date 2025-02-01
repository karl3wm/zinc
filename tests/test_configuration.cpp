#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <zinc/configuration.hpp>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

BOOST_AUTO_TEST_SUITE(ConfigurationTest)

BOOST_AUTO_TEST_CASE(init)
{
    // Create a temporary directory
    fs::path temp_dir = fs::temp_directory_path() / "zinc-test";
    fs::create_directory(temp_dir);

    // Change into the temporary directory
    fs::current_path(temp_dir);

    // Call init() and check that a .zinc directory is created
    BOOST_CHECK(zinc::Configuration::init());
    BOOST_CHECK(fs::exists(".zinc"));

    // Clean up
    fs::remove_all(temp_dir);
}

BOOST_AUTO_TEST_CASE(path_local)
{
    // Create a temporary directory with a .zinc subdirectory
    fs::path temp_dir = fs::temp_directory_path() / "zinc-test";
    fs::create_directory(temp_dir);
    fs::create_directory(temp_dir / ".zinc");

    // Change into the temporary directory
    fs::current_path(temp_dir);

    // Get the local configuration path
    std::string_view path = zinc::Configuration::path_local();

    // Check that the path is correct
    BOOST_CHECK_EQUAL(path, (temp_dir / ".zinc").native());

    // Clean up
    fs::remove_all(temp_dir);
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
    // Create a temporary directory with a .zinc subdirectory
    fs::path temp_dir = fs::temp_directory_path() / "zinc-test";
    fs::create_directory(temp_dir);
    fs::create_directory(temp_dir / ".zinc");

    // Change into the temporary directory
    fs::current_path(temp_dir);

    // Get the local configuration path with subpaths
    std::string_view path = zinc::Configuration::path_local(zinc::span<std::string_view>({"subdir", "subsubdir"}), true);

    // Check that the path is correct
    BOOST_CHECK_EQUAL(path, (temp_dir / ".zinc" / "subdir" / "subsubdir" / "").native());

    // Clean up
    fs::remove_all(temp_dir);
}

BOOST_AUTO_TEST_CASE(path_user_subpaths)
{
    fs::path expected_path = zinc::Configuration::path_user();

    // Get the user configuration path with subpaths
    std::string_view path = zinc::Configuration::path_user(zinc::span<std::string_view>({"subdir", "subsubdir"}), true);

    // Check that the path is correct
    /*fs::path expected_path = fs::path(getenv("XDG_CONFIG_HOME"));
    if (expected_path.empty()) {
        expected_path = fs::path(getenv("HOME")) / ".config";
    }
    expected_path /= "zinc";
    */
    expected_path /= "subdir";
    expected_path /= "subsubdir";
    expected_path /= "";
    BOOST_CHECK_EQUAL(path, expected_path.native());
}

BOOST_AUTO_TEST_SUITE_END()
