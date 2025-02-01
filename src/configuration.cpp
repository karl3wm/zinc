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
            char const* xdg_config_home = getenv("XDG_CONFIG_HOME");
            if (xdg_config_home != nullptr) {
                path = fs::path(xdg_config_home);
            }
            if (path.empty()) {
                char const* home = getenv("HOME");
                if (home != nullptr) {
                    path = fs::path(home) / ".config";
                }
            }
            if (!path.empty()) {
                path /= "zinc";
            } else {
                // Handle the case where neither XDG_CONFIG_HOME nor HOME is set.
                // You could throw an exception or set a default path.
                throw std::runtime_error("Neither XDG_CONFIG_HOME nor HOME is set.");
            }
        }

        fs::path path;
    } config_dir_user;

    return config_dir_user.path;
};

std::string_view path_helper(fs::path& path, std::span<std::string_view const> subpaths, bool is_dir) {
    for (const auto& subpath : subpaths) {
        path /= subpath;
    }
    if (is_dir) {
        fs::create_directories(path);
        path /= "";
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

std::string_view Configuration::path_local(std::span<std::string_view const> subpaths, bool is_dir) {
    static fs::path config_dir_local;

    {
        static std::mutex mtx;
        std::lock_guard<std::mutex> lock(mtx);

        if (config_dir_local.empty()) {
            fs::path zinc_dir;
            for (
                fs::path path = fs::current_path(), parent_path = path.parent_path();
                !path.empty() && path != parent_path;
                path = parent_path, parent_path = path.parent_path()
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

std::string_view Configuration::path_user(std::span<std::string_view const> subpaths, bool is_dir) {
    static thread_local fs::path path;
    path = config_dir_user();
    return path_helper(path, subpaths, is_dir);
}

} // namespace zinc
