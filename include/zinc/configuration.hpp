#pragma once

#include <zinc/common.hpp>

#include <span>
#include <string_view>

namespace zinc {

// => If file lock contention is an issue, a read-only or in-memory mode for the config object would likely alleviate that.
// => If hardcoded defaults are desired, the config object is designed to use multiple default sources and a shared defaults object could be added.

class Configuration {
public:
    /*
     * Create the configuration directories if they don't exist.
     *
     * Returns true if new directories were created.
     */
    static bool init();

    /*
     * Constructor
     */
    Configuration(std::span<std::string_view const> subpath, bool user_wide = false);

    /*
     * Accessor for configuration values within a configuration file.
     */
    std::string & operator[](std::span<std::string_view const> locator);

    /*
     * Generator to iterate over sections (subcategories).
     */
    zinc::generator<std::string_view> sections(std::span<std::string_view const> locator = {});

    /*
     * Generator to iterate over key-value pairs.
     */
    zinc::generator<StringViewPair> values(std::span<std::string_view const> locator);

    /*
     * Destructor
     */
    ~Configuration();

    /*
     * Get a per-project configuration path.
     */
    static std::string_view path_local(std::span<std::string_view const> subpath = {}, bool is_dir = false);

    /*
     * Get a per-user configuration path.
     */
    static std::string_view path_user(std::span<std::string_view const> subpath = {}, bool is_dir = false);

private:
    void* impl_;
};

} // namespace zinc
