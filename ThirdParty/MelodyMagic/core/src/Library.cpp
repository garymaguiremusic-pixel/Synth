#include "mm/Library.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace mm
{
    std::string folderOf (const std::string& key)
    {
        const auto slash = key.find_last_of ('/');
        return slash == std::string::npos ? std::string() : key.substr (0, slash);
    }

    void Library::clear()
    {
        phrases.clear();
        grooves.clear();
        keyMaps.clear();
        clips.clear();
    }

    //==============================================================================
    Phrase Library::phraseFromJson (const json::Value& v, const std::string& key)
    {
        Phrase p;
        const auto& props = v["props"];
        p.name = props["Name"].asString (baseName (key));
        p.metrica = props["Metrica"].asString ("4/4");
        p.snap = props["Snap"].asString ("Auto");
        p.ppq = kPhrasePpq;
        p.autoSize = props["Auto_Update_Total_Size"].asBool (true);
        p.totalSize = props["Total_Size"].asInt (kTicksPerBar);

        p.numberGroups.clear();
        for (auto& g : v["numberGroups"].a)
            p.numberGroups.emplace_back (g["Id"].asInt(), g["Value"].asInt (3));
        if (p.numberGroups.empty())
            p.setLayout (3, 3);

        for (auto& n : v["notes"].a)
        {
            PhraseNote pn;
            pn.number = n["number"].asInt (50);
            pn.noteOffset = n["noteOffset"].asInt (0);   // absent in version 1 files
            pn.velocity = n["velocity"].asInt (100);
            pn.start = n["start"].asInt (0);
            pn.end = n["end"].asInt (pn.start + 24);
            p.notes.push_back (pn);
        }
        return p;
    }

    Groove Library::grooveFromJson (const json::Value& v)
    {
        Groove g;
        g.name = v["name"].asString();
        g.totalPpq = v["totalPPQ"].asInt (384);
        g.globalPpq = v["globalPPQ"].asInt (96);
        for (auto& x : v["grid"].a)
            g.grid.push_back (x.asInt());
        std::sort (g.grid.begin(), g.grid.end());
        return g;
    }

    KeyMap Library::keyMapFromJson (const json::Value& v, const std::string& key)
    {
        KeyMap km;
        km.name = baseName (key);
        km.extraOctaves = v["extraOctaves"].asInt (1);
        for (auto& [pcText, offsets] : v["map"].o)
        {
            const int pc = std::atoi (pcText.c_str());
            if (pc < 0 || pc > 11)
                continue;
            for (auto& o : offsets.a)
                km.map[(size_t) pc].push_back (o.asInt());
            std::sort (km.map[(size_t) pc].begin(), km.map[(size_t) pc].end());
        }
        return km;
    }

    MidiClip Library::clipFromJson (const json::Value& v)
    {
        MidiClip c;
        c.ppq = std::max (1, v["ppq"].asInt (96));
        for (auto& t : v["tracks"].a)
            for (auto& n : t["notes"].a)
            {
                TimedNote tn;
                tn.note = n["note"].asInt (60);
                tn.channel = n["ch"].asInt (0) + 1;
                tn.vel = n["vel"].asInt (100);
                tn.start = n["start"].asDouble();
                tn.end = n["end"].asDouble();
                c.notes.push_back (tn);
            }
        std::stable_sort (c.notes.begin(), c.notes.end(), [] (const TimedNote& a, const TimedNote& b)
        {
            return a.start != b.start ? a.start < b.start : a.note < b.note;
        });
        return c.rescaled (kPhrasePpq);
    }

    //==============================================================================
    int Library::loadJsonLibrary (const std::string& file, std::vector<std::string>* errors)
    {
        auto lower = baseName (file);
        std::transform (lower.begin(), lower.end(), lower.begin(), [] (unsigned char c) { return (char) std::tolower (c); });

        json::Value root;
        try
        {
            root = json::parseFile (file);
        }
        catch (const std::exception& e)
        {
            if (errors) errors->push_back (file + ": " + e.what());
            return 0;
        }
        if (! root.isObject())
            return 0;

        int added = 0;
        for (auto& [key, v] : root.o)
        {
            try
            {
                if (lower.find ("fas") != std::string::npos)      phrases[key] = phraseFromJson (v, key);
                else if (lower.find ("qas") != std::string::npos) grooves[key] = grooveFromJson (v);
                else if (lower.find ("has") != std::string::npos) keyMaps[key] = keyMapFromJson (v, key);
                else if (lower.find ("mid") != std::string::npos) clips[key] = clipFromJson (v);
                else continue;
                ++added;
            }
            catch (const std::exception& e)
            {
                if (errors) errors->push_back (key + ": " + e.what());
            }
        }
        return added;
    }

    int Library::scanFolder (const std::string& folder, std::vector<std::string>* errors)
    {
        std::error_code ec;
        const fs::path root = fs::u8path (folder);
        if (! fs::is_directory (root, ec))
            return 0;

        int added = 0;
        for (fs::recursive_directory_iterator it (root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment (ec))
        {
            if (ec)
                break;
            if (! it->is_regular_file (ec))
                continue;

            const auto path = it->path();
            auto ext = path.extension().u8string();
            std::transform (ext.begin(), ext.end(), ext.begin(), [] (unsigned char c) { return (char) std::tolower (c); });
            const auto full = path.u8string();
            auto key = fs::relative (path, root, ec).generic_u8string();
            if (ec) key = path.filename().u8string();

            if (ext == ".json")
            {
                const auto name = path.filename().u8string();
                if (name.rfind ("nora_", 0) == 0 && name.find ("_library") != std::string::npos)
                    added += loadJsonLibrary (full, errors);
                continue;
            }

            std::string err;
            if (ext == ".fas")
            {
                Phrase p;
                if (loadPhraseFile (full, p, &err)) { phrases[key] = std::move (p); ++added; }
                else if (errors) errors->push_back (key + ": " + err);
            }
            else if (ext == ".qas")
            {
                Groove g;
                if (loadGrooveFile (full, g, &err)) { grooves[key] = std::move (g); ++added; }
                else if (errors) errors->push_back (key + ": " + err);
            }
            else if (ext == ".has")
            {
                KeyMap k;
                if (loadKeyMapFile (full, k, &err)) { keyMaps[key] = std::move (k); ++added; }
                else if (errors) errors->push_back (key + ": " + err);
            }
            else if (ext == ".mid" || ext == ".midi")
            {
                MidiClip c;
                if (readMidiFile (full, c, &err)) { clips[key] = c.rescaled (kPhrasePpq); ++added; }
                else if (errors) errors->push_back (key + ": " + err);
            }
        }
        return added;
    }
}
