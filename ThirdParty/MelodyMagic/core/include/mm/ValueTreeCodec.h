#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace mm::vt
{
    /** A juce::var as stored by var::writeToStream. */
    struct Var
    {
        enum class Type : std::uint8_t { Void = 0, Int = 1, BoolTrue = 2, BoolFalse = 3, Double = 4, String = 5, Int64 = 6, Array = 7, Binary = 8 };

        Type type = Type::Void;
        std::int64_t i = 0;          // Int, Int64
        double d = 0.0;              // Double
        std::string s;               // String (UTF-8) or Binary bytes
        std::vector<Var> array;

        static Var ofInt (int v)                 { Var x; x.type = Type::Int; x.i = v; return x; }
        static Var ofInt64 (std::int64_t v)      { Var x; x.type = Type::Int64; x.i = v; return x; }
        static Var ofBool (bool v)               { Var x; x.type = v ? Type::BoolTrue : Type::BoolFalse; return x; }
        static Var ofDouble (double v)           { Var x; x.type = Type::Double; x.d = v; return x; }
        static Var ofString (std::string v)      { Var x; x.type = Type::String; x.s = std::move (v); return x; }

        bool isVoid() const noexcept   { return type == Type::Void; }
        bool isString() const noexcept { return type == Type::String; }

        /** Numeric / bool conversion, like juce::var's operator int. */
        int toInt (int fallback = 0) const;
        double toDouble (double fallback = 0.0) const;
        bool toBool (bool fallback = false) const;
        std::string toString() const;
    };

    struct Tree
    {
        std::string type;
        std::vector<std::pair<std::string, Var>> props;   // order preserved
        std::vector<Tree> children;

        Tree() = default;
        explicit Tree (std::string t) : type (std::move (t)) {}

        const Var* find (const std::string& name) const noexcept;
        bool has (const std::string& name) const noexcept   { return find (name) != nullptr; }
        int getInt (const std::string& name, int fallback = 0) const;
        double getDouble (const std::string& name, double fallback = 0.0) const;
        bool getBool (const std::string& name, bool fallback = false) const;
        std::string getString (const std::string& name, const std::string& fallback = {}) const;

        Tree& set (const std::string& name, Var v);   // replaces or appends
        Tree& add (Tree child)                        { children.push_back (std::move (child)); return children.back(); }

        const Tree* child (const std::string& childType) const noexcept;

        bool operator== (const Tree&) const;
    };

    struct ParseError : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    /** Reads a sequence of top-level trees (throws ParseError on malformed data). */
    std::vector<Tree> read (const std::uint8_t* data, size_t size);
    std::vector<Tree> read (const std::vector<std::uint8_t>& bytes);

    /** Writes trees exactly as juce::ValueTree::writeToStream would. */
    std::vector<std::uint8_t> write (const std::vector<Tree>& trees);
    void writeTree (const Tree&, std::vector<std::uint8_t>& out);

    std::vector<std::uint8_t> readFileBytes (const std::string& path);
    bool writeFileBytes (const std::string& path, const std::vector<std::uint8_t>& bytes);
}
