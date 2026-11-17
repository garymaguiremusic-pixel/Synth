#pragma once

#include "Formats.h"
#include "Types.h"

#include <vector>

namespace mm
{
    /** A note ready for playback (after the groove quantizer), ticks at 96 PPQ.
        'id' identifies it in the events the player sends (Event::note = id). */
    struct PlayNote
    {
        int id = 0, vel = 100;
        double start = 0.0, end = 24.0;
    };

    struct PlayClip
    {
        std::vector<PlayNote> notes;
        double length = kTicksPerBar;
    };

    //==============================================================================
    // Groove quantizer (keep the rule in these two functions)

    /** Nearest grid line to tick t (96 PPQ). Grid = groove ticks * 96 / Global_PPQ, repeated every Total_PPQ. */
    double nearestGridTick (double t, const Groove&);

    /** t' = t + strength * (nearestGrid(t) - t). */
    double quantizeTick (double t, double strength, const Groove&);

    /** Moves a note's start / end towards the groove ('link' uses the start strength for both).
        The end never comes before start + 1 tick, and both stay inside 0..length. */
    void quantizeNote (double& start, double& end, double length, const Groove*, double startStrength, double endStrength, bool link);

    //==============================================================================
    /**
        Plays a looping clip against a tick position and produces note events
        (fromPhrase = true, note = PlayNote::id, vel = its velocity) at sample offsets.
    */
    class ClipPlayer
    {
    public:
        /** Window [startTick, startTick + numSamples * ticksPerSample) in unwrapped phrase ticks.
            A jump in position (seek, loop, speed change) stops the sounding notes first. */
        void process (const PlayClip&, double startTick, double ticksPerSample, int numSamples,
                      int timeOffset, std::vector<Event>& out);

        /** Note-offs for every sounding note. */
        void stopAll (int time, std::vector<Event>& out);

        /** Forget the expected position, so the next block is not treated as a jump. */
        void resetPosition() noexcept   { hasExpected = false; }

        bool isSounding() const noexcept { return ! sounding.empty(); }

    private:
        struct Sounding { int index, id; };
        std::vector<Sounding> sounding;
        double expected = 0.0;
        bool hasExpected = false;
    };
}
