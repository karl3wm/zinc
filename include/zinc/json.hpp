#pragma once

#include <span>
#include <string_view>
#include <variant>

namespace zinc {
class JSON;
using KeyJSONPair = std::pair<std::string_view, JSON>;
using _JSON_variant = std::variant<
    std::nullptr_t,
    bool, long, double, std::string_view,
    std::span<JSON>, std::span<KeyJSONPair>
>;

/*
 * The lifetime of parsed JSON objects is managed by the
 * returned JSON::Doc object.  Keep the JSON::Doc object around
 * until the storage for the entire tree can be reused.
 */
class JSON : public _JSON_variant {
public:
    using _JSON_variant::variant;

    enum Index {
        /*NULL = 0,*/
        BOOLEAN = 1,
        INTEGER = 2,
        NUMBER = 3,
        STRING = 4,
        ARRAY = 5,
        OBJECT = 6,
    };

    //std::string_view raw;

    std::partial_ordering operator<=>(JSON const&json) const;
    bool operator==(JSON const&json) const { return 0==(*this<=>json); }

    JSON const& operator[](std::string_view key) const;
    JSON const& operator[](size_t idx) const;

    auto& string() const
    { return std::get<std::string_view>(*this); }
    auto& array() const
    { return std::get<std::span<JSON>>(*this); }
    auto& object() const
    { return std::get<std::span<KeyJSONPair>>(*this); }

    class Doc {
    public:
        // this could instead be a JSON object with a flag
        // or a smart pointer (or both)
        Doc(Doc&&);

        JSON& operator*() { return *root_; }

        ~Doc();

    private:
        friend class JSON;
        Doc(JSON*);
        JSON* root_;
    };

    static Doc decode(std::string_view doc);
    std::string_view encode() const;
};

}
