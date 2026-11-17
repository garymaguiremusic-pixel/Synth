#include "vc/AnyPreset.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <zlib.h>

namespace vc
{
namespace
{
    uint32_t be32 (const uint8_t* p) { return (uint32_t (p[0]) << 24) | (uint32_t (p[1]) << 16) | (uint32_t (p[2]) << 8) | p[3]; }
    uint32_t le32 (const uint8_t* p) { return (uint32_t (p[3]) << 24) | (uint32_t (p[2]) << 16) | (uint32_t (p[1]) << 8) | p[0]; }
    float beFloat (const uint8_t* p) { const uint32_t u = be32 (p); float f; std::memcpy (&f, &u, 4); return f; }
    float leFloat (const uint8_t* p) { const uint32_t u = le32 (p); float f; std::memcpy (&f, &u, 4); return f; }

    std::string lower (std::string s) { for (auto& c : s) c = (char) std::tolower ((unsigned char) c); return s; }
    bool endsWith (const std::string& s, const std::string& e) { return s.size() >= e.size() && lower (s.substr (s.size() - e.size())) == e; }

    std::string cleanName (const uint8_t* p, size_t n)
    {
        std::string s;
        for (size_t i = 0; i < n && p[i]; ++i) s += (p[i] >= 32 && p[i] < 127) ? (char) p[i] : ' ';
        while (! s.empty() && s.back() == ' ') s.pop_back();
        return s;
    }

    //==========================================================================================
    // Minimal JSON reader (objects, arrays, strings, numbers, true/false/null) for Spire presets.
    struct Json
    {
        enum class T { Null, Bool, Num, Str, Arr, Obj } t = T::Null;
        double num = 0; bool b = false; std::string str;
        std::vector<Json> arr;
        std::vector<std::pair<std::string, Json>> obj;
        const Json* get (const std::string& k) const { for (auto& kv : obj) if (kv.first == k) return &kv.second; return nullptr; }
    };

    struct JsonParser
    {
        const std::string& s; size_t i = 0; int depth = 0;
        explicit JsonParser (const std::string& text) : s (text) {}
        void ws() { while (i < s.size() && std::isspace ((unsigned char) s[i])) ++i; }
        bool parse (Json& out)
        {
            ws();
            if (i >= s.size() || ++depth > 64) return false;
            const char c = s[i];
            bool ok = false;
            if (c == '{')
            {
                ++i; out.t = Json::T::Obj; ws();
                if (i < s.size() && s[i] == '}') { ++i; ok = true; }
                else while (true)
                {
                    ws(); std::string key; if (! str (key)) break;
                    ws(); if (i >= s.size() || s[i] != ':') break; ++i;
                    Json v; if (! parse (v)) break;
                    out.obj.emplace_back (std::move (key), std::move (v));
                    ws(); if (i < s.size() && s[i] == ',') { ++i; continue; }
                    if (i < s.size() && s[i] == '}') { ++i; ok = true; }
                    break;
                }
            }
            else if (c == '[')
            {
                ++i; out.t = Json::T::Arr; ws();
                if (i < s.size() && s[i] == ']') { ++i; ok = true; }
                else while (true)
                {
                    Json v; if (! parse (v)) break;
                    out.arr.push_back (std::move (v));
                    ws(); if (i < s.size() && s[i] == ',') { ++i; continue; }
                    if (i < s.size() && s[i] == ']') { ++i; ok = true; }
                    break;
                }
            }
            else if (c == '"') { out.t = Json::T::Str; ok = str (out.str); }
            else if (s.compare (i, 4, "true") == 0) { out.t = Json::T::Bool; out.b = true; i += 4; ok = true; }
            else if (s.compare (i, 5, "false") == 0) { out.t = Json::T::Bool; i += 5; ok = true; }
            else if (s.compare (i, 4, "null") == 0) { i += 4; ok = true; }
            else
            {
                const char* start = s.c_str() + i; char* end = nullptr;
                out.num = std::strtod (start, &end);
                if (end != start) { out.t = Json::T::Num; i += (size_t) (end - start); ok = true; }
            }
            --depth;
            return ok;
        }
        bool str (std::string& out)
        {
            if (i >= s.size() || s[i] != '"') return false;
            ++i;
            while (i < s.size() && s[i] != '"')
            {
                if (s[i] == '\\' && i + 1 < s.size())
                {
                    const char e = s[i + 1]; i += 2;
                    switch (e) { case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'u': i += std::min<size_t> (4, s.size() - i); out += '?'; break; default: out += e; }
                }
                else out += s[i++];
            }
            if (i >= s.size()) return false;
            ++i;
            return true;
        }
    };

    double jsonNumber (const Json& j)
    {
        if (j.t == Json::T::Num) return j.num;
        if (j.t == Json::T::Str) return std::strtod (j.str.c_str(), nullptr);
        if (j.t == Json::T::Bool) return j.b ? 1.0 : 0.0;
        return 0.0;
    }
}

//==============================================================================
bool zlibInflate (const uint8_t* data, size_t size, std::vector<uint8_t>& out, size_t sizeHint)
{
    z_stream zs {};
    if (inflateInit (&zs) != Z_OK) return false;
    zs.next_in = const_cast<Bytef*> (data);
    zs.avail_in = (uInt) size;
    out.clear();
    out.resize (std::max<size_t> (sizeHint, size * 4 + 1024));
    int rc = Z_OK;
    while (rc == Z_OK)
    {
        if (zs.total_out >= out.size()) out.resize (out.size() * 2);
        zs.next_out = out.data() + zs.total_out;
        zs.avail_out = (uInt) (out.size() - zs.total_out);
        rc = inflate (&zs, Z_NO_FLUSH);
        if (out.size() > (size_t) 512 << 20) { rc = Z_MEM_ERROR; break; }
    }
    const bool ok = rc == Z_STREAM_END;
    out.resize (zs.total_out);
    inflateEnd (&zs);
    return ok;
}

bool parseSylenthChunk (const std::vector<uint8_t>& p, std::vector<AnyPatch>& out, std::string& err)
{
    if (p.size() < 20 || std::memcmp (p.data(), "1lys", 4) != 0) { err = "not a Sylenth1 chunk"; return false; }
    const uint32_t version = le32 (p.data() + 4);
    const uint32_t numPresets = le32 (p.data() + 8);
    const uint32_t numParams = le32 (p.data() + 12);
    constexpr size_t kNameLen = 36;
    if (numParams == 0 || numParams > 4096 || numPresets == 0 || numPresets > 4096) { err = "implausible Sylenth1 header"; return false; }
    const size_t rec = numParams * 4 + kNameLen;
    if (p.size() < 20 + rec) { err = "Sylenth1 chunk too short"; return false; }
    const size_t available = (p.size() - 20) / rec;
    for (size_t k = 0; k < std::min<size_t> (numPresets, available); ++k)
    {
        const uint8_t* r = p.data() + 20 + k * rec;
        AnyPatch a;
        a.type = PatchType::Sylenth;
        a.name = cleanName (r + numParams * 4, kNameLen);
        if (a.name.empty()) continue;                       // unused slot
        a.sylenth.version = (int) version;
        a.sylenth.values.resize (numParams);
        for (uint32_t i = 0; i < numParams; ++i) a.sylenth.values[i] = std::clamp (leFloat (r + i * 4), 0.0f, 1.0f);
        out.push_back (std::move (a));
    }
    if (out.empty()) { err = "no named presets in the Sylenth1 data"; return false; }
    return true;
}

bool parseSpireJson (const std::string& text, AnyPatch& out, std::string& err)
{
    Json root;
    JsonParser jp (text);
    if (! jp.parse (root) || root.t != Json::T::Obj) { err = "not valid JSON"; return false; }
    // .spf2 / .repatch: {"author", "bank", "tags", "parameters": {key: value}}
    // VST3 state JSON:   {"preset": {"params": [725 strings]}, "preset_meta": {...}}
    out.type = PatchType::Spire;
    if (auto* a = root.get ("author")) out.author = a->str;
    if (auto* t = root.get ("tags")) if (t->t == Json::T::Arr && ! t->arr.empty()) out.category = t->arr[0].str;
    if (auto* params = root.get ("parameters"))
    {
        for (auto& kv : params->obj) out.spire[kv.first] = (float) jsonNumber (kv.second);
        if (out.spire.empty()) { err = "no parameters"; return false; }
        return true;
    }
    if (auto* preset = root.get ("preset"))
        if (auto* arr = preset->get ("params"); arr && arr->t == Json::T::Arr)
        {
            for (size_t i = 0; i < arr->arr.size(); ++i) out.spire["#" + std::to_string (i)] = (float) jsonNumber (arr->arr[i]);
            if (auto* meta = root.get ("preset_meta")) { if (auto* n = meta->get ("name")) out.name = n->str; if (auto* au = meta->get ("author")) out.author = au->str; }
            return true;
        }
    err = "JSON without Spire parameters";
    return false;
}

//==============================================================================
namespace
{
    /** VST2 .fxb / .fxp (big-endian 'CcnK'). */
    bool importFx (const uint8_t* d, size_t n, AnyImport& res)
    {
        if (n < 60 || std::memcmp (d, "CcnK", 4) != 0) return false;
        const std::string magic (reinterpret_cast<const char*> (d + 8), 4);
        const std::string fxId (reinterpret_cast<const char*> (d + 16), 4);
        const bool isSylenth = fxId == "syl1";
        auto chunkPatches = [&] (const uint8_t* chunk, size_t len)
        {
            if (len >= 8 && std::memcmp (chunk, "1lys", 4) == 0)
            {
                std::vector<uint8_t> payload;
                if (! zlibInflate (chunk + 8, len - 8, payload)) { res.error = "Sylenth1 chunk could not be decompressed"; return; }
                std::string err;
                if (! parseSylenthChunk (payload, res.patches, err)) res.error = err;
                return;
            }
            res.error = "unknown VST2 chunk (plugin ID '" + fxId + "')";
        };

        if (magic == "FBCh" || magic == "FPCh")
        {
            const size_t hdr = magic == "FBCh" ? 160 : 60;      // fxb: +numPrograms, future[128]; fxp: name[28]
            if (n < hdr) { res.error = "truncated " + magic; return true; }
            const uint32_t len = be32 (d + hdr - 4);
            if (hdr + len > n) { res.error = "truncated chunk"; return true; }
            res.formatName = std::string (isSylenth ? "Sylenth1 " : "VST2 ") + (magic == "FBCh" ? "bank" : "program") + " (" + magic + ")";
            chunkPatches (d + hdr, len);
            if (magic == "FPCh" && res.patches.size() == 1 && res.patches[0].name.empty())
                res.patches[0].name = cleanName (d + 28, 28);
            return true;
        }
        if (magic == "FxCk" || magic == "FxBk")
        {
            // plain parameter values (big-endian floats)
            auto readProgram = [&] (const uint8_t* p, size_t avail) -> size_t
            {
                if (avail < 56) return 0;
                const uint32_t np = be32 (p + 24);
                if (np > 4096 || 56 + np * 4 > avail) return 0;
                AnyPatch a;
                a.type = isSylenth ? PatchType::Sylenth : PatchType::Virus;
                a.name = cleanName (p + 28, 28);
                a.sylenth.values.resize (np);
                for (uint32_t i = 0; i < np; ++i) a.sylenth.values[i] = std::clamp (beFloat (p + 56 + i * 4), 0.0f, 1.0f);
                res.patches.push_back (std::move (a));
                return 56 + np * 4;
            };
            res.formatName = isSylenth ? "Sylenth1 (plain parameters)" : "VST2 parameters";
            if (magic == "FxCk") readProgram (d, n);
            else
            {
                const uint32_t numProgs = be32 (d + 24);
                size_t pos = 156;
                for (uint32_t k = 0; k < numProgs && pos < n; ++k)
                {
                    const size_t used = readProgram (d + pos, n - pos);
                    if (! used) break;
                    pos += used;
                }
            }
            if (! isSylenth) res.error = "VST2 file for plugin ID '" + fxId + "', not Sylenth1";
            return true;
        }
        res.error = "unknown VST2 file type " + magic;
        return true;
    }
}

AnyImport importAnyData (const uint8_t* data, size_t size, const std::string& fileName)
{
    AnyImport res;
    if (data == nullptr || size == 0) { res.error = "empty file"; return res; }

    // Sylenth (VST2 fx container)
    if (size >= 4 && std::memcmp (data, "CcnK", 4) == 0)
    {
        importFx (data, size, res);
        if (res.patches.empty() && res.error.empty()) res.error = "no presets found";
        return res;
    }
    // Spire JSON
    size_t k = 0;
    while (k < size && std::isspace (data[k])) ++k;
    if (k < size && data[k] == '{')
    {
        AnyPatch a;
        std::string err;
        if (parseSpireJson (std::string (reinterpret_cast<const char*> (data), size), a, err))
        {
            if (a.name.empty())
            {
                std::string base = fileName;
                const auto slash = base.find_last_of ("/\\");
                if (slash != std::string::npos) base = base.substr (slash + 1);
                const auto dot = base.find_last_of ('.');
                a.name = dot == std::string::npos ? base : base.substr (0, dot);
            }
            res.formatName = "Spire preset (JSON)";
            res.patches.push_back (std::move (a));
        }
        else res.error = "Spire preset: " + err;
        return res;
    }
    // Virus SysEx (.mid / .syx)
    auto v = importData (data, size);
    res.warnings = std::move (v.warnings);
    res.error = v.error;
    for (auto& s : v.singles)
    {
        AnyPatch a;
        a.type = PatchType::Virus;
        a.name = s.name();
        a.virus = std::move (s);
        res.patches.push_back (std::move (a));
    }
    if (! res.patches.empty()) res.formatName = "Virus single dumps";
    if (res.patches.empty() && res.error.empty())
        res.error = endsWith (fileName, ".spf") || endsWith (fileName, ".sbf")
                  ? "legacy Spire .spf/.sbf files are not supported yet (save the preset as .spf2 in Spire)"
                  : "no patches found";
    return res;
}

AnyImport importAnyFile (const std::string& path)
{
    auto data = readFile (path);
    if (! data) { AnyImport r; r.error = "Could not read " + path; return r; }
    return importAnyData (data->data(), data->size(), path);
}
} // namespace vc
