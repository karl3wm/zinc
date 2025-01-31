<|begin_of_text|><|start_header_id|>user<|end_header_id|>

Can you implement src/configuration.cpp ? This is c++20. It uses .zinc paths like .git . Note that naive directory-walking could interpret a .zinc path in the user's home folder as a project path wrongly, and this should be detected.

**include/zinc/configuration.hpp**
```
#pragma once

#include <zinc/common.hpp>

#include <stringview>

namespace zinc {

class Configuration {
public:
    /*
     * @brief Create the configuration directories if they don't exist.
     *
     * @param path The root project path under which the .zinc path will reside.
     */
    static void init(std::string_view path = ".");

    /*
     * Get a per-project configuration path.
     */
    static std::string_view path_local(std::span<std::string_view> subpaths = zinc::span({}));

    /*
     * Get a per-user configuration path.
     */
    static std::string_view path_user(std::span<std::string_view> subpaths = zinc::span({}));
}

}
```<|eot_id|><|start_header_id|>assistant<|end_header_id|>

Here is a possible implementation of `src/configuration.cpp`:
```cpp
#include "zinc/configuration.hpp"
#include <filesystem>
#include <stdexcept>

namespace zinc {

namespace fs = std::filesystem;

// Helper function to check if a path is a .zinc directory
bool is_zinc_dir(const fs::path& path) {
    return path.filename() == ".zinc";
}

// Helper function to get the project root directory
fs::path get_project_root(const fs::path& path) {
    auto parent = path.parent_path();
    while (!parent.empty()) {
        if (is_zinc_dir(parent)) {
            return parent.parent_path();
        }
        parent = parent.parent_path();
    }
    throw std::runtime_error("Could not find project root directory");
}

void Configuration::init(std::string_view path) {
    fs::path project_root = get_project_root(fs::path(path));
    fs::path zinc_dir = project_root / ".zinc";
    if (!fs::exists(zinc_dir)) {
        fs::create_directories(zinc_dir);
    }
}

std::string_view Configuration::path_local(std::span<std::string_view> subpaths) {
    fs::path project_root = get_project_root(fs::current_path());
    fs::path zinc_dir = project_root / ".zinc";
    fs::path local_path = zinc_dir;
    for (const auto& subpath : subpaths) {
        local_path /= subpath;
    }
    return local_path.string();
}

std::string_view Configuration::path_user(std::span<std::string_view> subpaths) {
    fs::path user_dir = fs::path(getenv("HOME")) / ".zinc";
    fs::path user_path = user_dir;
    for (const auto& subpath : subpaths) {
        user_path /= subpath;
    }
    // Check if the user path is actually a project path
    if (is_zinc_dir(user_path.parent_path())) {
        throw std::runtime_error("User path is actually a project path");
    }
    return user_path.string();
}

} // namespace zinc
```
Note that this implementation uses the `std::filesystem` library to work with paths and directories. It also uses the `getenv` function to get the user's home directory.

The `get_project_root` function is used to find the project root directory by walking up the directory tree until it finds a `.zinc` directory.

The `init` function creates the `.zinc` directory if it doesn't exist.

The `path_local` function returns a path relative to the project root directory, while the `path_user` function returns a path relative to the user's home directory. Both functions take an optional `subpaths` parameter to allow for subdirectories.

The `path_user` function also checks if the user path is actually a project path by checking if the parent directory is a `.zinc` directory. If it is, it throws an exception.