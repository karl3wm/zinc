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
