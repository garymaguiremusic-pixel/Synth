#pragma once

#include "Pattern.h"
#include "Rng.h"
#include "Scales.h"

#include <string>
#include <vector>

namespace mm
{
    /**
        Melody Maker: writes leads the way classic trance melodies are built.

          - an implied chord progression (one chord per bar, e.g. Am F C G);
          - notes on the beat are chord tones, the notes between them move by
            scale step (passing and neighbour notes), with the occasional leap;
          - a one-bar hook (A) is repeated over every chord, moved to fit it, and the
            phrase ends with an answer bar: 4 bars = A A A B, 8 bars = A A A B A A A C;
          - rhythms come from classic trance lead rhythms, or from an imported MIDI file.

        Everything is in scale degrees of the Key + Scale, so the result is always in key.
    */

    /** What the Melody Maker learns from an imported MIDI file (or the built-in trance style). */
    struct MelodyStyle
    {
        bool fromFile = false;
        std::vector<double> degreeWeight;   // per scale degree (0..N-1): how often the file uses it
        std::vector<double> stepWeight;     // melodic interval in scale steps, -7..+7 (index + 7)
        int lowDegree = -2, highDegree = 9; // range in scale degrees from the key's root
        std::vector<PatternNote> rhythmA;   // one bar of the file's rhythm (offsets unused), empty = templates
        std::vector<PatternNote> rhythmB;   // its last bar (the answer), may be empty
    };

    /** The built-in style (classic trance). */
    MelodyStyle defaultMelodyStyle (const ScaleContext&);

    /** Learns rhythm, intervals, favourite notes and range from a pattern (an imported MIDI file). */
    MelodyStyle styleFromPattern (const Pattern&, const ScaleContext&, int key);

    // Progressions (scale degrees, one chord per bar) -------------------------------
    const std::vector<std::string>& progressionNames();   // [0] = "Auto"
    std::vector<int> progressionDegrees (int index, const ScaleContext&, int key);

    /** Chord symbol of the triad on a scale degree, e.g. "Am", "F", "Bdim". */
    std::string chordName (int degree, const ScaleContext&, int key);

    struct MelodySettings
    {
        int progression = 0;   // index into progressionNames(), 0 = Auto (minor or major)
        int density = 60;      // busy-ness of the template rhythms (0..100)
        int range = 14;        // melody span in semitones
        int gate = 80;         // note length, % of the template length
    };

    /** A new melody: rhythm and notes. */
    Pattern makeMelody (int bars, const MelodySettings&, const MelodyStyle&, const ScaleContext&, int key, Rng&);

    /** New notes for an existing rhythm (starts, lengths and velocities stay). The melody stays in
        the pattern's register: from its lowest note up.
        keepFirstBar: a variation - bar 1 stays as it is and the other bars are re-written around it.
        topOnly: the bottom notes (the lowest note of the pattern and the lower notes of stacked
        chords) stay; only the notes above them change. */
    void melodiseNotes (Pattern&, const MelodySettings&, const MelodyStyle&, const ScaleContext&, int key, Rng&,
                        bool keepFirstBar = false, bool topOnly = false);

    /** Keeps the melody (the notes, in order, bar by bar) and gives it a new rhythm. */
    void rhythmiseMelody (Pattern&, const MelodySettings&, const MelodyStyle&, Rng&);

    // Scale-degree helpers (exposed for tests) -----------------------------------------
    int degreeToOffset (int degree, const ScaleContext&, int key);
    int offsetToDegree (int offset, const ScaleContext&, int key);   // nearest scale note
}
