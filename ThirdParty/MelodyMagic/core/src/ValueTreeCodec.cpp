#include "mm/ValueTreeCodec.h"

#include <cmath>
#include <string>
#include <cstring>
#include <fstream>
#include <iterator>

namespace mm::vt
{
    //==============================================================================
    int Var::toInt (int fallback) const
    {
        switch (type)
        {
            case Type::Int:
            case Type::Int64:     return (int) i;
            case Type::Double:    return (int) d;
            case Type::BoolTrue:  return 1;
            case Type::BoolFalse: return 0;
            case Type::String:    try { return std::stoi (s); } catch (...) { return fallback; }
            default:              return fallback;
        }
    }

    double Var::toDouble (double fallback) const
    {
        switch (type)
        {
            case Type::Int:
            case Type::Int64:     return (double) i;
            case Type::Double:    return d;
            case Type::BoolTrue:  return 1.0;
            case Type::BoolFalse: return 0.0;
            case Type::String:    try { return std::stod (s); } catch (...) { return fallback; }
            default:              return fallback;
        }
    }

    bool Var::toBool (bool fallback) const
    {
        switch (type)
        {
            case Type::BoolTrue:  return true;
            case Type::BoolFalse: return false;
            case Type::Int:
            case Type::Int64:     return i != 0;
            case Type::Double:    return d != 0.0;
            case Type::String:    return s == "1" || s == "true";
            default:              return fallback;
        }
    }

    std::string Var::toString() const
    {
        switch (type)
        {
            case Type::String:    return s;
            case Type::Int:
            case Type::Int64:     return std::to_string (i);
            case Type::Double:    return std::to_string (d);
            case Type::BoolTrue:  return "1";
            case Type::BoolFalse: return "0";
            default:              return {};
        }
    }

    //==============================================================================
    const Var* Tree::find (const std::string& name) const noexcept
    {
        for (auto& p : props)
            if (p.first == name)
                return &p.second;
        return nullptr;
    }

    int Tree::getInt (const std::string& n, int f) const                    { auto* v = find (n); return v ? v->toInt (f) : f; }
    double Tree::getDouble (const std::string& n, double f) const           { auto* v = find (n); return v ? v->toDouble (f) : f; }
    bool Tree::getBool (const std::string& n, bool f) const                 { auto* v = find (n); return v ? v->toBool (f) : f; }
    std::string Tree::getString (const std::string& n, const std::string& f) const { auto* v = find (n); return v ? v->toString() : f; }

    Tree& Tree::set (const std::string& name, Var v)
    {
        for (auto& p : props)
            if (p.first == name)
            {
                p.second = std::move (v);
                return *this;
            }
        props.emplace_back (name, std::move (v));
        return *this;
    }

    const Tree* Tree::child (const std::string& t) const noexcept
    {
        for (auto& c : children)
            if (c.type == t)
                return &c;
        return nullptr;
    }

    static bool varEquals (const Var& a, const Var& b)
    {
        if (a.type != b.type) return false;
        switch (a.type)
        {
            case Var::Type::Int:
            case Var::Type::Int64:  return a.i == b.i;
            case Var::Type::Double: return std::memcmp (&a.d, &b.d, sizeof (double)) == 0;
            case Var::Type::String:
            case Var::Type::Binary: return a.s == b.s;
            case Var::Type::Array:
                if (a.array.size() != b.array.size()) return false;
                for (size_t k = 0; k < a.array.size(); ++k)
                    if (! varEquals (a.array[k], b.array[k])) return false;
                return true;
            default: return true;
        }
    }

    bool Tree::operator== (const Tree& o) const
    {
        if (type != o.type || props.size() != o.props.size() || children.size() != o.children.size())
            return false;
        for (size_t k = 0; k < props.size(); ++k)
            if (props[k].first != o.props[k].first || ! varEquals (props[k].second, o.props[k].second))
                return false;
        for (size_t k = 0; k < children.size(); ++k)
            if (! (children[k] == o.children[k]))
                return false;
        return true;
    }

    //==============================================================================
    namespace
    {
        struct Reader
        {
            const std::uint8_t* data;
            size_t size, pos = 0;

            void need (size_t n) const
            {
                if (pos + n > size)
                    throw ParseError ("unexpected end of data at " + std::to_string (pos));
            }

            std::uint8_t byte() { need (1); return data[pos++]; }

            int cint()
            {
                const auto b = byte();
                const int n = b & 0x7f;
                if (n > 4)
                    throw ParseError ("bad compressed int at " + std::to_string (pos - 1));
                need ((size_t) n);
                std::uint32_t v = 0;
                for (int k = 0; k < n; ++k)
                    v |= (std::uint32_t) data[pos++] << (8 * k);
                return (b & 0x80) ? -(int) v : (int) v;
            }

            std::string str()
            {
                const size_t start = pos;
                while (pos < size && data[pos] != 0) ++pos;
                if (pos >= size)
                    throw ParseError ("unterminated string at " + std::to_string (start));
                std::string r ((const char*) data + start, pos - start);
                ++pos;
                return r;
            }

            template <typename T> T raw()
            {
                need (sizeof (T));
                T v;
                std::memcpy (&v, data + pos, sizeof (T));   // little endian hosts only (x86/ARM)
                pos += sizeof (T);
                return v;
            }

            Var var()
            {
                const int sz = cint();
                Var v;
                if (sz == 0)
                    return v;
                if (sz < 0 || pos + (size_t) sz > size)
                    throw ParseError ("bad var size " + std::to_string (sz) + " at " + std::to_string (pos));

                const size_t end = pos + (size_t) sz;
                const auto t = data[pos++];
                switch (t)
                {
                    case 1: v.type = Var::Type::Int;       v.i = raw<std::int32_t>(); break;
                    case 2: v.type = Var::Type::BoolTrue;  break;
                    case 3: v.type = Var::Type::BoolFalse; break;
                    case 4: v.type = Var::Type::Double;    v.d = raw<double>(); break;
                    case 5:
                        v.type = Var::Type::String;
                        v.s.assign ((const char*) data + pos, end - pos - 1);   // drop the terminator
                        break;
                    case 6: v.type = Var::Type::Int64;     v.i = raw<std::int64_t>(); break;
                    case 7:
                    {
                        v.type = Var::Type::Array;
                        const int n = cint();
                        for (int k = 0; k < n; ++k)
                            v.array.push_back (var());
                        break;
                    }
                    case 8:
                        v.type = Var::Type::Binary;
                        v.s.assign ((const char*) data + pos, end - pos);
                        break;
                    default:
                        throw ParseError ("unknown var type " + std::to_string (t));
                }
                pos = end;
                return v;
            }

            Tree tree()
            {
                Tree t (str());
                const int np = cint();
                if (np < 0) throw ParseError ("negative property count");
                for (int k = 0; k < np; ++k)
                {
                    auto name = str();
                    t.props.emplace_back (std::move (name), var());
                }
                const int nc = cint();
                if (nc < 0) throw ParseError ("negative child count");
                for (int k = 0; k < nc; ++k)
                    t.children.push_back (tree());
                return t;
            }
        };

        // juce::OutputStream::writeCompressedInt
        void putCint (std::vector<std::uint8_t>& out, int value)
        {
            auto un = value < 0 ? (std::uint32_t) -(std::int64_t) value : (std::uint32_t) value;
            std::uint8_t data[5];
            int num = 0;
            while (un > 0)
            {
                data[++num] = (std::uint8_t) un;
                un >>= 8;
            }
            data[0] = (std::uint8_t) num;
            if (value < 0)
                data[0] |= 0x80;
            out.insert (out.end(), data, data + num + 1);
        }

        template <typename T> void putRaw (std::vector<std::uint8_t>& out, T v)
        {
            std::uint8_t b[sizeof (T)];
            std::memcpy (b, &v, sizeof (T));
            out.insert (out.end(), b, b + sizeof (T));
        }

        void putStr (std::vector<std::uint8_t>& out, const std::string& s)
        {
            out.insert (out.end(), s.begin(), s.end());
            out.push_back (0);
        }

        void putVar (std::vector<std::uint8_t>& out, const Var& v)
        {
            switch (v.type)
            {
                case Var::Type::Void:      putCint (out, 0); break;
                case Var::Type::Int:       putCint (out, 5); out.push_back (1); putRaw (out, (std::int32_t) v.i); break;
                case Var::Type::BoolTrue:  putCint (out, 1); out.push_back (2); break;
                case Var::Type::BoolFalse: putCint (out, 1); out.push_back (3); break;
                case Var::Type::Double:    putCint (out, 9); out.push_back (4); putRaw (out, v.d); break;
                case Var::Type::String:
                    putCint (out, (int) v.s.size() + 2);
                    out.push_back (5);
                    putStr (out, v.s);
                    break;
                case Var::Type::Int64:     putCint (out, 9); out.push_back (6); putRaw (out, (std::int64_t) v.i); break;
                case Var::Type::Array:
                {
                    std::vector<std::uint8_t> buffer;
                    putCint (buffer, (int) v.array.size());
                    for (auto& e : v.array)
                        putVar (buffer, e);
                    putCint (out, 1 + (int) buffer.size());
                    out.push_back (7);
                    out.insert (out.end(), buffer.begin(), buffer.end());
                    break;
                }
                case Var::Type::Binary:
                    putCint (out, 1 + (int) v.s.size());
                    out.push_back (8);
                    out.insert (out.end(), v.s.begin(), v.s.end());
                    break;
            }
        }
    }

    std::vector<Tree> read (const std::uint8_t* data, size_t size)
    {
        Reader r { data, size };
        std::vector<Tree> trees;
        while (r.pos < size)
            trees.push_back (r.tree());
        return trees;
    }

    std::vector<Tree> read (const std::vector<std::uint8_t>& bytes)   { return read (bytes.data(), bytes.size()); }

    void writeTree (const Tree& t, std::vector<std::uint8_t>& out)
    {
        putStr (out, t.type);
        putCint (out, (int) t.props.size());
        for (auto& [name, value] : t.props)
        {
            putStr (out, name);
            putVar (out, value);
        }
        putCint (out, (int) t.children.size());
        for (auto& c : t.children)
            writeTree (c, out);
    }

    std::vector<std::uint8_t> write (const std::vector<Tree>& trees)
    {
        std::vector<std::uint8_t> out;
        for (auto& t : trees)
            writeTree (t, out);
        return out;
    }

    std::vector<std::uint8_t> readFileBytes (const std::string& path)
    {
        std::ifstream f (path, std::ios::binary);
        if (! f)
            return {};
        return std::vector<std::uint8_t> (std::istreambuf_iterator<char> (f), {});
    }

    bool writeFileBytes (const std::string& path, const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream f (path, std::ios::binary | std::ios::trunc);
        if (! f)
            return false;
        f.write ((const char*) bytes.data(), (std::streamsize) bytes.size());
        return (bool) f;
    }
}
