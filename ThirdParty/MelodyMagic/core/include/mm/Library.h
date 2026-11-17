#pragma once

#include "Formats.h"
#include "Json.h"
#include "MidiFile.h"

#include <map>
#include <string>
#include <vector>

namespace mm
{
    /**
        Preset library, keyed by relative path ("Phrases/kick101/Bass/Bass1.fas").
        Loads the pre-converted JSON libraries (nora_fas/qas/has/mid_library.json) and/or
        raw .fas / .qas / .has / .mid files found under a folder.
    */
    class Library
    {
    public:
        std::map<std::string, Phrase> phrases;
        std::map<std::string, Groove> grooves;
        std::map<std::string, KeyMap> keyMaps;
        std::map<std::string, MidiClip> clips;   // chord / scale clips, rescaled to 96 PPQ

        void clear();
        size_t size() const noexcept   { return phrases.size() + grooves.size() + keyMaps.size() + clips.size(); }

        /** Loads one JSON library file; the kind comes from its name (fas / qas / has / mid). Returns entries added. */
        int loadJsonLibrary (const std::string& file, std::vector<std::string>* errors = nullptr);

        /** Loads every nora_*_library.json and every raw preset file under 'folder' (recursive). */
        int scanFolder (const std::string& folder, std::vector<std::string>* errors = nullptr);

        // JSON entry converters (exposed for tests)
        static Phrase phraseFromJson (const json::Value&, const std::string& key);
        static Groove grooveFromJson (const json::Value&);
        static KeyMap keyMapFromJson (const json::Value&, const std::string& key);
        static MidiClip clipFromJson (const json::Value&);
    };

    /** Folder part of a library key ("Phrases/kick101/Bass"). */
    std::string folderOf (const std::string& key);
}
