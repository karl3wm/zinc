<|begin_of_text|><|start_header_id|>user<|end_header_id|>

Blargh. I accidentally clobbered my src/configuration.cpp file. Can you write another? It had (a) a helper to access a statically-allocated user configuration directory. this used a local struct with a constructor function that found a good path for user configuration files and appended .zinc to it. (b) a helper that took a reference to a statically-allocated thread-local filesystem path, added a list (a span of stringviews) of subdirectories, and returned .native() cast as a string_view. (c) the init() function called the path_local() function with no arguments, and if there was an exception thrown, it generated a .zinc folder in the current directory. (d) the path_local() function had a statically allocated path for the root local configuration directory, initially empty. it opened a locked scope and tested if this was empty, and if it was it iterated parent paths from the current directory. for each path, it considered a .zinc subfolder for hte path. if this subfolder was the same as the user configuration directory, it broke the loop. if it existed, then it assigned to the static variable and break the loop. after the loop it checked if this variable was still empty and threw the exception if it was. (e) then the end of both path_local() and path_user() would call the second helper to return their value, returning a reference to the storage held by thread local static paths allocated by the caller.

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
```<|eot_id|><|start_header_id|>assistant<|end_header_id|>

Here is the implementation of the `src/configuration.cpp` file based on the provided specification:
```cpp
#include "zinc/configuration.hpp"

#include <filesystem>
#include <stdexcept>

namespace zinc {

namespace {

struct UserConfigDir {
    std::filesystem::path path;

    UserConfigDir() {
        // Find a good path for user configuration files and append .zinc to it
        path = std::filesystem::path(getenv("XDG_CONFIG_HOME"));
        if (path.empty()) {
            path = std::filesystem::path(getenv("HOME")) / ".config";
        }
        path /= ".zinc";
    }
};

std::filesystem::path userConfigDir = UserConfigDir().path;

std::string_view pathHelper(std::filesystem::path& path, std::span<std::string_view> subpaths, bool is_dir) {
    for (const auto& subpath : subpaths) {
        path /= subpath;
    }
    if (is_dir && !std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }
    return path.native();
}

} // namespace

bool Configuration::init() {
    try {
        path_local();
    } catch (const std::exception& e) {
        // Create a .zinc folder in the current directory if an exception is thrown
        std::filesystem::create_directory(".zinc");
        return true;
    }
    return false;
}

std::string_view Configuration::path_local(std::span<std::string_view> subpaths, bool is_dir) {
    static thread_local std::filesystem::path localPath;

    if (localPath.empty()) {
        // Find the root local configuration directory
        std::filesystem::path currentDir = std::filesystem::current_path();
        while (true) {
            std::filesystem::path zincDir = currentDir / ".zinc";
            if (zincDir == userConfigDir) {
                break;
            }
            if (std::filesystem::exists(zincDir)) {
                localPath = zincDir;
                break;
            }
            currentDir = currentDir.parent_path();
            if (currentDir.empty()) {
                throw std::runtime_error("Could not find local configuration directory");
            }
        }
    }

    return pathHelper(localPath, subpaths, is_dir);
}

std::string_view Configuration::path_user(std::span<std::string_view> subpaths, bool is_dir) {
    static thread_local std::filesystem::path userPath = userConfigDir;

    return pathHelper(userPath, subpaths, is_dir);
}

} // namespace zinc
```
Note that I've used the `std::filesystem` library to work with file paths, which is a C++17 feature. If you're using an earlier version of C++, you may need to use a different library or approach. Additionally, I've assumed that the `getenv` function is available to retrieve environment variables; if this is not the case, you may need to use a different approach to determine the user configuration directory.
