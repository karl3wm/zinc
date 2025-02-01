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
