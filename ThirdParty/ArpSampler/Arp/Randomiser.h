#pragma once

#include <juce_core/juce_core.h>
#include "Pattern.h"

namespace as::arp
{
    struct RandomiseSettings
    {
        float density = 0.75f;        // chance a step plays (0..1)
        int range = 12;               // transpose range, +/- semitones
        float tieProbability = 0.15f; // chance an active step ties into the next
        float ratchetProbability = 0.0f; // chance an active step gets 2-4 ratchets
        float reverseProbability = 0.0f; // chance an active step plays the sample backwards
        float accentProbability = 0.0f;  // chance an active step is accented
        int velocityMin = 60, velocityMax = 127;
        bool lockPitch = false, lockVelocity = false, lockRests = false, lockTies = false, lockRatchets = false,
             lockReverse = false,
             lockAccents = false;
        bool scaleLock = false;
        bool fileNotes = false;          // pick pitches only from the pattern's note pool (the imported file's notes)
        int scaleKey = 0, scaleIndex = 0;
    };

    /**
        Generates a new pattern from a seed. Locked lanes are copied from 'current'.
        Rests are steps with Volume 0; Lock Rests keeps the Volume lane as it is.
        The same seed, settings and current pattern always give the same result, and
        every lane draws its random numbers whether it is locked or not, so locking one
        lane never changes what another lane gets for a given seed.
    */
    Pattern randomisePattern (const Pattern& current, const RandomiseSettings&, juce::uint32 seed);
}
