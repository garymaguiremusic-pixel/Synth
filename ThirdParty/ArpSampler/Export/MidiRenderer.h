#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include "../Arp/ArpEngine.h"

namespace as::exporter
{
    static constexpr int kTicksPerQuarter = 960;

    struct ExportRequest
    {
        arp::ArpSettings settings;            // mode, rate, gate, swing, octave, velocity mode, scale...
        arp::Pattern pattern;
        std::vector<arp::HeldNote> input;     // keys "held" for the whole render, in press order
        double bpm = 120.0;
        int bars = 2;                         // 4/4 bars
        juce::int64 randomSeed = 1;           // for Random mode
    };

    /**
        Renders the pattern to a Standard MIDI File (type 1, 960 PPQ) by running the real
        ArpEngine with a host-style clock where one "sample" is one tick. So the file
        has exactly the notes, timing, ties, rests, velocities and scale snapping the
        arp plays. Notes still sounding at the end are cut at the loop end.
    */
    juce::MidiFile renderPattern (const ExportRequest&);

    /** The chord intervals for the "Export Chord" choices (Major, Minor, Maj7, Min7, Dom7, Sus4, Octave, Single). */
    std::vector<int> chordIntervals (int chordIndex);

    /** A descriptive file name, e.g. "ArpSampler Up C3 2 bars 120 BPM". */
    juce::String suggestedName (const ExportRequest&);

    /** Writes the file to the temp folder (ArpSampler MIDI), removing exports older than a day. */
    juce::File writeToTempFile (const juce::MidiFile&, const juce::String& baseName);
}
