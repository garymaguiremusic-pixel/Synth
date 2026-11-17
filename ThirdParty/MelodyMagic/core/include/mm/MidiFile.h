#pragma once

#include "Types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mm
{
    /** Notes of a standard MIDI file, merged over all tracks. Times in the file's ticks. */
    struct MidiClip
    {
        int ppq = 96;
        double bpm = 120.0;           // first tempo event (120 if none)
        std::vector<TimedNote> notes; // sorted by start, then note

        /** Same notes at another resolution. */
        MidiClip rescaled (int newPpq) const;
        double lengthTicks() const noexcept;
    };

    /** Reads format 0 / 1 SMF. Returns false on malformed data. */
    bool readMidi (const std::vector<std::uint8_t>& bytes, MidiClip&, std::string* error = nullptr);
    bool readMidiFile (const std::string& path, MidiClip&, std::string* error = nullptr);

    /** Writes a format 0 SMF (one track: name, tempo, 4/4, notes). Note times are rounded to ticks. */
    std::vector<std::uint8_t> writeMidi (const MidiClip&, const std::string& trackName = {});
    bool writeMidiFile (const std::string& path, const MidiClip&, const std::string& trackName = {});
}
