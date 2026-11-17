#pragma once

#include "Types.h"
#include "ValueTreeCodec.h"

#include <array>
#include <string>
#include <vector>

namespace mm
{
    //==============================================================================
    /** Quantizer / groove pattern (.qas "Quantizer Pattern"). Grid lines in Global_PPQ ticks. */
    struct Groove
    {
        std::string name;
        int totalPpq = 384;
        int globalPpq = 96;
        std::vector<int> grid;

        bool operator== (const Groove& o) const noexcept
        {
            return name == o.name && totalPpq == o.totalPpq && globalPpq == o.globalPpq && grid == o.grid;
        }
    };

    /** MIDI key map / harmonizer (.has "Harmonizer"). */
    struct KeyMap
    {
        std::string name;
        int extraOctaves = 1;
        /** map[pc] = output offsets in semitones relative to the input key's octave base B. */
        std::array<std::vector<int>, 12> map;

        /** Input key k emits B + offset for each offset of map[k % 12] (kept within 0..127). */
        std::vector<int> expand (int key) const;
        bool isEmpty() const noexcept;
    };

    //==============================================================================
    // Model <-> ValueTree
    Phrase phraseFromTrees (const std::vector<vt::Tree>&);
    std::vector<vt::Tree> phraseToTrees (const Phrase&);

    Groove grooveFromTrees (const std::vector<vt::Tree>&);
    std::vector<vt::Tree> grooveToTrees (const Groove&);

    KeyMap keyMapFromTrees (const std::vector<vt::Tree>&);
    std::vector<vt::Tree> keyMapToTrees (const KeyMap&);

    /** File type of a stream ("Secuencia Frases", "Quantizer Pattern", "Harmonizer" ...). */
    std::string fileTypeOf (const std::vector<vt::Tree>&);

    // Files (return false and fill 'error' on failure)
    bool loadPhraseFile (const std::string& path, Phrase&, std::string* error = nullptr);
    bool savePhraseFile (const std::string& path, const Phrase&);
    bool loadGrooveFile (const std::string& path, Groove&, std::string* error = nullptr);
    bool saveGrooveFile (const std::string& path, const Groove&);
    bool loadKeyMapFile (const std::string& path, KeyMap&, std::string* error = nullptr);
    bool saveKeyMapFile (const std::string& path, const KeyMap&);

    /** File name without folder and extension. */
    std::string baseName (const std::string& path);
}
