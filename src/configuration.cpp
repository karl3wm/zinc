#include <zinc/configuration.hpp>

#include <filesystem>
#include <stdexcept>
#include <thread>
#include <unordered_set>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
//#include <regex>

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

/*
// Helper to convert Git-style [section "subsection"] to Boost Property Tree path
std::string git_to_ptree_path(std::span<std::string_view const> locator) {
    std::string path;
    for (const auto& part : locator) {
        if (!path.empty()) {
            path += ".";
        }
        path += part;
    }
    return path;
}
*/

class ConfigurationImpl {
public:
    ConfigurationImpl(std::span<std::string_view const> subpath, bool user_wide)
    : ptreep_(std::string(user_wide ? Configuration::path_user(subpath) : Configuration::path_local(subpath)), {})
    {
        if (!user_wide) {
            auto path_user = Configuration::path_user(subpath);
            if (fs::exists(path_user)) {
                dflts_.emplace_back(std::string(path_user), decltype(ptreep_.second)());
                boost::property_tree::ini_parser::read_ini(dflts_[0].first, dflts_[0].second);
            }
        }
        if (fs::exists(ptreep_.first)) {
            boost::property_tree::ini_parser::read_ini(ptreep_.first, ptreep_.second);
        }
    }

    ~ConfigurationImpl() {
        bool changed = false;
        bool possibly_returned_to_normal;
        do {
            possibly_returned_to_normal = true;
            for (auto it = created_.begin(); it != created_.end();) {
                auto & [pt, hash] = *it;
                if (std::hash<std::string_view>()(pt->data()) != hash) {
                    changed = true;
                    it = created_.erase(it);
                } else {
                    if (pt->empty()) {
                        it = created_.erase(it);
                        possibly_returned_to_normal = false;
                    }
                }
            }
        } while (!possibly_returned_to_normal);
        assert(possibly_returned_to_normal == true);
        assert(created_.empty() || changed); // things should only dangle if they have dangling children or are changed; so recursively the former implies the latter
        /*
        for (auto & [ptr, [hash, path_offset]] : chks_) {
            if (std::hash<std::string_view>()(ptr->data()) == hash) {
                // the new value was not set so remove it
                auto * path_begin = &chks_paths_[path_offset];
                for (auto * path_end = path_begin; path_end->second != ptr; ++ path_end);
                auto * path_it = path_end;
                bug with a second value being a subpath of a first
                the first leaves ancestor paths in
                one approach is to detect newness earlier
                another is to loop over all created paths,
                removing empty ones until there are none
                while (path_it->second.empty()) {
                    auto & key = path_it->first;
                    -- path_it;
                    path_it->second.erase(key);
                }
                do {
                    if (path_it->second.empty()) {
                    } else {
                        break;
                    }
                } while (p != path_begin;
                do {
                    size_t sz = *(size_t*)(void*)data;
                    data += sizeof(sz);
                    std::string_view key{data, data + sz};
                    data += sz;

                }
            } else {
                changed = true;
            }
        }
        */
        if (changed) {
            boost::property_tree::ini_parser::write_ini(ptreep_.first, ptreep_.second);
        }
    }

    std::string& operator[](std::span<std::string_view const> locator) {
        return get(locator).data();
        //auto path = git_to_ptree_path(locator);
        //dirty_ = true;
        //return ptree_.get_child(path).data();
    }

    zinc::generator<std::string_view> sections(std::span<std::string_view const> locator) {
        for (auto & [key, pt] : get_below(locator)) {
            if (pt.size()) {
                co_yield key;
            }
        }
    }

    zinc::generator<StringViewPair> values(std::span<std::string_view const> locator) {
        for (auto & [key, pt] : get_below(locator)) {
            if (!pt.data().empty()) {
                co_yield {key, pt.data()};
            }
        }
    }

private:
    boost::property_tree::ptree & get(std::span<std::string_view const> locator) {
        bool found;
        //size_t chks_offset;
        auto * value = get(ptreep_, locator, true/*&chks_offset*/, found);
        if (!found) {
            bool dflt_found = false;
            auto * dflt = value;
            for (auto it = dflts_.begin(); !dflt_found && it != dflts_.end(); ++ it) {
                dflt = get(*it, locator, false/*nullptr*/, dflt_found);
            }
            if (dflt_found) {
                value->data() = dflt->data();
            }
            /*
            assert(chks_.find(value) == chks_.end());
            chks_.emplace(value, {std::hash<std::string_view>()(value->data()), chks_offset});
            */
        }
        return *value;
    }
    zinc::generator<boost::property_tree::ptree::value_type&> get_below(std::span<std::string_view const> locator) {
        bool found;
        auto * value = get(ptreep_, locator, false, found);
        std::unordered_set<size_t> found_keys;
        if (found) {
            for (auto & val : *value) {
                co_yield val;
                found_keys.insert(std::hash<std::string_view>()(val.first));
            }
        }
        for (auto & dflt : dflts_) {
            value = get(dflt, locator, false, found);
            if (found) {
                for (auto & val : *value) {
                    auto [_, inserted] = found_keys.insert(std::hash<std::string_view>()(val.first));
                    if (inserted) {
                        co_yield val;
                    }
                }
            }
        }
    }
    boost::property_tree::ptree * get(boost::property_tree::ptree::value_type & ptp, std::span<std::string_view const> locator, bool create, bool & found) {
        static thread_local std::stringstream ss;
        if (locator.empty()) {
            found = true;
            return &ptp.second;
        }
        ss.clear();
        ss << locator[0];
        if (locator.size() >= 2) {
            ss << ' ' << std::quoted(locator[1]);
            locator = {locator.begin() + 2, locator.end()};
        } else {
            locator = {};
        }
        static thread_local std::string fragment; // because ptree uses std::string for find()
        fragment = ss.view();
        auto* key_pt = &ptp;
        auto pt_it = key_pt->second.find(fragment);
        auto fragment_it = locator.begin();
        while(true) {
            if (pt_it == key_pt->second.not_found()) {
                found = false;
                if (create) {
                    break;
                } else {
                    return nullptr;
                }
            } else {
                found = true;
            }
            key_pt = &*pt_it;
            if (locator.end() == fragment_it) {
                break;
            }
            fragment = *fragment_it;
            pt_it = key_pt->second.find(fragment);
            ++ fragment_it;
        }
        if (false == found) {
            assert(create);
            std::lock_guard<std::mutex> lk(mtx);
            /*
            *create_and_chk = chks_paths_.size();
            chks_paths_.reserve(locator.end() - fragment_it + 1);
            chks_paths_.push_back(key_pt);
            //logic errors expected
            */
            while (true) {
                key_pt = &*key_pt->second.push_back(std::make_pair(fragment, boost::property_tree::ptree()));
                //chks_paths_.push_back(key_pt);

                // note: this approach does not use defaults if a section is also a value.
                created_[&key_pt->second] = std::hash<std::string_view>()(key_pt->second.data());
                if (locator.end() == fragment_it) {
                    break;
                }
                fragment = *fragment_it;
            }
        }
        fragment.clear();
        return &key_pt->second;
    }

    //boost::property_tree::ptree& ptree_() { return ptreep_.second };

    boost::property_tree::ptree::value_type ptreep_;
    std::vector<boost::property_tree::ptree::value_type> dflts_;
    //std::vector<boost::property_tree::ptree::value_type*> chks_paths_;
    //std::unordered_map<boost::property_tree::ptree*node, std::pair<size_t, size_t>> chks_;
    std::mutex mtx;
    std::unordered_map<boost::property_tree::ptree*, size_t> created_;
        /*
         * the idea is to store every created path with a hash of its value.
         * on destruction, all paths are iterated, and ones that both match their hash and are empty are removed.
         * the iteration is repeated (walking upward) until all empty ones are removed.
         */
};

} // namespace

bool Configuration::init() {
    try {
        path_local();
    } catch (std::invalid_argument const& e) {
        fs::create_directory(".zinc");
        return true;
    }
    return false;
}

Configuration::Configuration(std::span<std::string_view const> subpath, bool user_wide)
    : impl_(reinterpret_cast<void*>(new ConfigurationImpl(subpath, user_wide))) {}

Configuration::~Configuration() {
    delete reinterpret_cast<ConfigurationImpl*>(impl_);
}

std::string& Configuration::operator[](std::span<std::string_view const> locator) {
    return (*reinterpret_cast<ConfigurationImpl*>(impl_))[locator];
}

zinc::generator<std::string_view> Configuration::sections(std::span<std::string_view const> locator) {
    return reinterpret_cast<ConfigurationImpl*>(impl_)->sections(locator);
}

zinc::generator<StringViewPair> Configuration::values(std::span<std::string_view const> locator) {
    return reinterpret_cast<ConfigurationImpl*>(impl_)->values(locator);
}

std::string_view Configuration::path_local(std::span<std::string_view const> subpaths, bool is_dir) {
    // just looked at this briefly, if this were managed with static lifetime as a local class the check and mutex could be avoided
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
