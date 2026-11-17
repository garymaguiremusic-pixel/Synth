#pragma once

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace mm::json
{
    /** Minimal JSON value (enough for the preset libraries). */
    struct Value
    {
        enum class Type { Null, Bool, Number, String, Array, Object };

        Type type = Type::Null;
        bool b = false;
        double n = 0.0;
        std::string s;
        std::vector<Value> a;
        std::vector<std::pair<std::string, Value>> o;   // insertion order kept

        bool isNull() const noexcept   { return type == Type::Null; }
        bool isObject() const noexcept { return type == Type::Object; }
        bool isArray() const noexcept  { return type == Type::Array; }
        bool isNumber() const noexcept { return type == Type::Number; }

        const Value* get (const std::string& key) const noexcept;
        const Value& operator[] (const std::string& key) const noexcept;

        int asInt (int fallback = 0) const noexcept;
        double asDouble (double fallback = 0.0) const noexcept;
        bool asBool (bool fallback = false) const noexcept;
        std::string asString (const std::string& fallback = {}) const;
    };

    struct ParseError : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    Value parse (const std::string& text);
    Value parseFile (const std::string& path);
}
