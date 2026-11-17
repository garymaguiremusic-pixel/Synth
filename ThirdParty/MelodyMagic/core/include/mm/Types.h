#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace mm
{
    /** Phrase resolution: ticks per quarter note (fixed in the original format). */
    constexpr int kPhrasePpq = 96;
    constexpr int kTicksPerBar = 4 * kPhrasePpq;   // 4/4

    /** Centre octave lane: Number 50 = chord tone 0 as played. */
    constexpr int kCentreLane = 5;
    constexpr int kMinLane = 3, kMaxLane = 7;       // Numbers 30..79
    constexpr int kMaxRows = 10;

    //==============================================================================
    /** One note of a phrase. Number = lane * 10 + row. Ticks at 96 PPQ. */
    struct PhraseNote
    {
        int number = 50;
        int noteOffset = 0;   // signed diatonic shift (0 = none)
        int velocity = 100;
        int start = 0;
        int end = 24;

        int row() const noexcept   { return number % 10; }
        int lane() const noexcept  { return number / 10; }

        bool operator== (const PhraseNote& o) const noexcept
        {
            return number == o.number && noteOffset == o.noteOffset && velocity == o.velocity
                && start == o.start && end == o.end;
        }
    };

    /** A phrase (.fas "Secuencia Frases"). */
    struct Phrase
    {
        std::string name = "Init";
        std::string metrica = "4/4";
        std::string snap = "Auto";
        int ppq = kPhrasePpq;
        bool autoSize = true;      // Auto_Update_Total_Size
        int totalSize = kTicksPerBar;
        std::vector<std::pair<int, int>> numberGroups { { 1, 3 }, { 2, 3 }, { 3, 3 } };  // Id, notes per octave lane
        std::vector<PhraseNote> notes;

        /** Phrase length in ticks: Total_Size when manual, otherwise the end of the
            last bar that contains a note end (at least one bar). */
        int lengthTicks() const noexcept;

        /** Rows per octave lane (largest Number_Groups value, 1..10). */
        int rowsPerLane() const noexcept;

        /** Number of octave lanes shown (1, 3 or 5), from Number_Groups. */
        int lanesShown() const noexcept;

        void setLayout (int lanes, int rows);
        void sortNotes();

        bool operator== (const Phrase& o) const noexcept
        {
            return name == o.name && metrica == o.metrica && snap == o.snap && ppq == o.ppq
                && autoSize == o.autoSize && totalSize == o.totalSize && numberGroups == o.numberGroups
                && notes == o.notes;
        }
    };

    //==============================================================================
    /** MIDI event inside one processing block (time = sample offset). */
    struct Event
    {
        enum Type : std::uint8_t { Dropped = 0, On = 1, Off = 2 };

        std::uint8_t type = On;
        int time = 0;
        int note = 0;          // MIDI note, or the phrase Number when fromPhrase
        int vel = 0;
        bool fromPhrase = false;
        int channel = 1;       // 1..16
        int noteOffset = 0;    // phrase notes only

        static Event on (int time, int note, int vel, int channel = 1)  { Event e; e.type = On;  e.time = time; e.note = note; e.vel = vel; e.channel = channel; return e; }
        static Event off (int time, int note, int channel = 1)          { Event e; e.type = Off; e.time = time; e.note = note; e.vel = 0;   e.channel = channel; return e; }
    };

    /** A note with a start and an end, in ticks (used by MIDI files and offline rendering). */
    struct TimedNote
    {
        int note = 60;
        int vel = 100;
        int channel = 1;
        double start = 0.0;
        double end = 0.0;
    };
}
