#include "mm/Json.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace mm::json
{
    const Value* Value::get (const std::string& key) const noexcept
    {
        if (type != Type::Object)
            return nullptr;
        for (auto& kv : o)
            if (kv.first == key)
                return &kv.second;
        return nullptr;
    }

    const Value& Value::operator[] (const std::string& key) const noexcept
    {
        static const Value null;
        auto* v = get (key);
        return v ? *v : null;
    }

    int Value::asInt (int f) const noexcept
    {
        if (type == Type::Number) return (int) std::llround (n);
        if (type == Type::Bool)   return b ? 1 : 0;
        return f;
    }

    double Value::asDouble (double f) const noexcept   { return type == Type::Number ? n : (type == Type::Bool ? (b ? 1.0 : 0.0) : f); }
    bool Value::asBool (bool f) const noexcept         { return type == Type::Bool ? b : (type == Type::Number ? n != 0.0 : f); }

    std::string Value::asString (const std::string& f) const
    {
        if (type == Type::String) return s;
        if (type == Type::Number)
        {
            std::ostringstream os;
            os << n;
            return os.str();
        }
        return f;
    }

    namespace
    {
        struct Parser
        {
            const std::string& t;
            size_t p = 0;

            [[noreturn]] void fail (const char* what) const
            {
                throw ParseError (std::string ("JSON: ") + what + " at " + std::to_string (p));
            }

            void ws()
            {
                while (p < t.size() && (t[p] == ' ' || t[p] == '\n' || t[p] == '\r' || t[p] == '\t'))
                    ++p;
            }

            bool lit (const char* s)
            {
                size_t k = 0;
                while (s[k] != 0)
                {
                    if (p + k >= t.size() || t[p + k] != s[k])
                        return false;
                    ++k;
                }
                p += k;
                return true;
            }

            static void utf8 (std::string& out, unsigned cp)
            {
                if (cp < 0x80) out += (char) cp;
                else if (cp < 0x800) { out += (char) (0xc0 | (cp >> 6)); out += (char) (0x80 | (cp & 0x3f)); }
                else if (cp < 0x10000) { out += (char) (0xe0 | (cp >> 12)); out += (char) (0x80 | ((cp >> 6) & 0x3f)); out += (char) (0x80 | (cp & 0x3f)); }
                else { out += (char) (0xf0 | (cp >> 18)); out += (char) (0x80 | ((cp >> 12) & 0x3f)); out += (char) (0x80 | ((cp >> 6) & 0x3f)); out += (char) (0x80 | (cp & 0x3f)); }
            }

            unsigned hex4()
            {
                if (p + 4 > t.size()) fail ("bad \\u escape");
                unsigned v = 0;
                for (int k = 0; k < 4; ++k)
                {
                    const char c = t[p++];
                    v <<= 4;
                    if (c >= '0' && c <= '9') v |= (unsigned) (c - '0');
                    else if (c >= 'a' && c <= 'f') v |= (unsigned) (c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') v |= (unsigned) (c - 'A' + 10);
                    else fail ("bad hex digit");
                }
                return v;
            }

            std::string str()
            {
                if (t[p] != '"') fail ("expected string");
                ++p;
                std::string out;
                while (true)
                {
                    if (p >= t.size()) fail ("unterminated string");
                    const char c = t[p++];
                    if (c == '"') break;
                    if (c != '\\') { out += c; continue; }
                    if (p >= t.size()) fail ("bad escape");
                    const char e = t[p++];
                    switch (e)
                    {
                        case '"': out += '"'; break;
                        case '\\': out += '\\'; break;
                        case '/': out += '/'; break;
                        case 'b': out += '\b'; break;
                        case 'f': out += '\f'; break;
                        case 'n': out += '\n'; break;
                        case 'r': out += '\r'; break;
                        case 't': out += '\t'; break;
                        case 'u':
                        {
                            unsigned cp = hex4();
                            if (cp >= 0xd800 && cp < 0xdc00 && p + 1 < t.size() && t[p] == '\\' && t[p + 1] == 'u')
                            {
                                p += 2;
                                const unsigned lo = hex4();
                                cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
                            }
                            utf8 (out, cp);
                            break;
                        }
                        default: fail ("bad escape");
                    }
                }
                return out;
            }

            Value value()
            {
                ws();
                if (p >= t.size()) fail ("unexpected end");
                Value v;
                const char c = t[p];
                if (c == '{')
                {
                    v.type = Value::Type::Object;
                    ++p; ws();
                    if (t[p] == '}') { ++p; return v; }
                    while (true)
                    {
                        ws();
                        auto key = str();
                        ws();
                        if (t[p] != ':') fail ("expected ':'");
                        ++p;
                        v.o.emplace_back (std::move (key), value());
                        ws();
                        if (t[p] == ',') { ++p; continue; }
                        if (t[p] == '}') { ++p; break; }
                        fail ("expected ',' or '}'");
                    }
                }
                else if (c == '[')
                {
                    v.type = Value::Type::Array;
                    ++p; ws();
                    if (t[p] == ']') { ++p; return v; }
                    while (true)
                    {
                        v.a.push_back (value());
                        ws();
                        if (t[p] == ',') { ++p; continue; }
                        if (t[p] == ']') { ++p; break; }
                        fail ("expected ',' or ']'");
                    }
                }
                else if (c == '"') { v.type = Value::Type::String; v.s = str(); }
                else if (lit ("true"))  { v.type = Value::Type::Bool; v.b = true; }
                else if (lit ("false")) { v.type = Value::Type::Bool; v.b = false; }
                else if (lit ("null"))  { v.type = Value::Type::Null; }
                else
                {
                    const char* begin = t.c_str() + p;
                    char* end = nullptr;
                    v.n = std::strtod (begin, &end);
                    if (end == begin) fail ("unexpected character");
                    v.type = Value::Type::Number;
                    p += (size_t) (end - begin);
                }
                return v;
            }
        };
    }

    Value parse (const std::string& text)
    {
        Parser parser { text };
        auto v = parser.value();
        parser.ws();
        if (parser.p != text.size())
            parser.fail ("trailing characters");
        return v;
    }

    Value parseFile (const std::string& path)
    {
        std::ifstream f (path, std::ios::binary);
        if (! f)
            throw ParseError ("cannot open " + path);
        std::stringstream ss;
        ss << f.rdbuf();
        return parse (ss.str());
    }
}
