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

    using Null = std::variant_alternative_t<(Index)NULL, _JSON_variant>;
    using Integer = std::variant_alternative_t<INTEGER, _JSON_variant>;
    using Number = std::variant_alternative_t<NUMBER, _JSON_variant>;
    using Bool = std::variant_alternative_t<BOOLEAN, _JSON_variant>;
    using String = std::variant_alternative_t<STRING, _JSON_variant>;
    using Array = std::variant_alternative_t<ARRAY, _JSON_variant>;
    using Object = std::variant_alternative_t<OBJECT, _JSON_variant>;

    //std::string_view raw;

    std::partial_ordering operator<=>(JSON const&json) const;
    bool operator==(JSON const&json) const { return 0==(*this<=>json); }

    JSON const& operator[](std::string_view key) const;
    JSON const& operator[](size_t idx) const;
    size_t size() const;

    bool truthy() const;
    std::string_view stringy() const;
    JSON const& dicty(std::string_view key, JSON const&dflt={}) const;

    String const&string() const
    { return std::get<String>(*this); }
    Array const&array() const
    { return std::get<Array>(*this); }
    Object const&object() const
    { return std::get<Object>(*this); }

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
    std::string_view encode() const; /* could we add indentation here please */
};

}
