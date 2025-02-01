<|begin_of_text|><|start_header_id|>user<|end_header_id|>

Write tests/test_configuration.cpp for include/zinc/configuration.hpp . Also, I don't have gtest. I do have boost and cmake.

**include/zinc/configuration.hpp**
```
#pragma once

#include <zinc/common.hpp>

#include <span>
#include <string_view>

namespace zinc {

class Configuration {
public:
    /*
     * Create the configuration directories if they don't exist.
     *
     * Returns true if new directories were created.
     */
    static bool init();

    /*
     * Get a per-project configuration path.
     */
    static std::string_view path_local(std::span<std::string_view> subpaths = {}, bool is_dir = false);

    /*
     * Get a per-user configuration path.
     */
    static std::string_view path_user(std::span<std::string_view> subpaths = {}, bool is_dir = false);
};

}
```

**src/configuration.cpp**
```
#include <zinc/configuration.hpp>

#include <filesystem>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;

namespace zinc {

namespace {

fs::path const & config_dir_user()
{
    static struct ConfigDirUser
    {
        ConfigDirUser()
        {
            path = fs::path(getenv("XDG_CONFIG_HOME"));
            if (path.empty()) {
                path = fs::path(getenv("HOME")) / ".config";
            }
            path /= "zinc";
        }

        fs::path path;
    } config_dir_user;

    return config_dir_user.path;
};

std::string_view path_helper(fs::path& path, std::span<std::string_view> subpaths, bool is_dir) {
    for (const auto& subpath : subpaths) {
        path /= subpath;
    }
    if (is_dir) {
        fs::create_directories(path);
    } else {
        fs::create_directories(path.parent_path());
    }
    return path.native();
}

} // namespace

bool Configuration::init() {
    try {
        path_local();
    } catch (std::invalid_argument const& e) {
        // Create a .zinc folder in the current directory if an exception is thrown
        fs::create_directory(".zinc");
        return true;
    }
    return false;
}

std::string_view Configuration::path_local(std::span<std::string_view> subpaths, bool is_dir) {
    static fs::path config_dir_local;

    {
        static std::mutex mtx;
        std::lock_guard<std::mutex> lock(mtx);

        if (config_dir_local.empty()) {
            fs::path zinc_dir;
            for (
                fs::path path = fs::current_path();
                !path.empty();
                path = path.parent_path()
            ) {
                zinc_dir = path / ".zinc";
                if (zinc_dir == config_dir_user()) {
                    break;
                }
                if (fs::exists(zinc_dir)) {
                    config_dir_local = zinc_dir;
                    break;
                }
            }
            if (config_dir_local.empty()) {
                throw std::invalid_argument("Could not find .zinc directory for project. Create one.");
            }
        }
    }

    static thread_local fs::path path;
    path = config_dir_local;
    return path_helper(path, subpaths, is_dir);
}

std::string_view Configuration::path_user(std::span<std::string_view> subpaths, bool is_dir) {
    static thread_local fs::path path;
    path = config_dir_user();
    return path_helper(path, subpaths, is_dir);
}

} // namespace zinc
```<|eot_id|><|start_header_id|>assistant<|end_header_id|>

Here is an example of how you can write tests for the `Configuration` class using Boost.Test and CMake:

**tests/test_configuration.cpp**
```cpp
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

BOOST_AUTO_TEST_CASE(path_local_subpaths)
{
    // Create a temporary directory with a .zinc subdirectory
    fs::path temp_dir = fs::temp_directory_path() / "zinc-test";
    fs::create_directory(temp_dir);
    fs::create_directory(temp_dir / ".zinc");

    // Change into the temporary directory
    fs::current_path(temp_dir);

    // Get the local configuration path with subpaths
    std::string_view path = zinc::Configuration::path_local({"subdir", "subsubdir"}, true);

    // Check that the path is correct
    BOOST_CHECK_EQUAL(path, (temp_dir / ".zinc" / "subdir" / "subsubdir").native());

    // Clean up
    fs::remove_all(temp_dir);
}

BOOST_AUTO_TEST_CASE(path_user_subpaths)
{
    // Get the user configuration path with subpaths
    std::string_view path = zinc::Configuration::path_user({"subdir", "subsubdir"}, true);

    // Check that the path is correct
    fs::path expected_path = fs::path(getenv("XDG_CONFIG_HOME"));
    if (expected_path.empty()) {
        expected_path = fs::path(getenv("HOME")) / ".config";
    }
    expected_path /= "zinc";
    expected_path /= "subdir";
    expected_path /= "subsubdir";
    BOOST_CHECK_EQUAL(path, expected_path.native());
}

BOOST_AUTO_TEST_SUITE_END()
```

**CMakeLists.txt**
```cmake
cmake_minimum_required(VERSION 3.10)
project(zinc)

add_library(zinc STATIC src/configuration.cpp)

add_executable(test_configuration tests/test_configuration.cpp)
target_link_libraries(test_configuration zinc boost_unit_test_framework)

enable_testing()
add_test(NAME test_configuration COMMAND test_configuration)
```

Note that this is just an example, and you may need to modify the tests to fit your specific use case. Additionally, you will need to install Boost.Test and configure CMake to use it.
