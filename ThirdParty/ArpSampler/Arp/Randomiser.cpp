#include "Randomiser.h"
#include "Scales.h"

namespace as::arp
{
    Pattern randomisePattern (const Pattern& current, const RandomiseSettings& s, juce::uint32 seed)
    {
        juce::Random rng ((juce::int64) seed);

        // candidate transposes: semitones, or with Scale Lock notes of the scale (the Range knob is
        // in semitones, so +/-1 octave is +/-7 steps of a major scale, +/-5 of a pentatonic)
        int candidates[49];
        int numCandidates = 0;
        const int range = juce::jlimit (0, 24, s.range);
        const int maxT = s.scaleLock ? juce::roundToInt (range * scaleSize (s.scaleIndex) / 12.0) : range;
        for (int t = -maxT; t <= maxT; ++t)
            candidates[numCandidates++] = t;

        const int vMin = juce::jlimit (1, 127, juce::jmin (s.velocityMin, s.velocityMax));
        const int vMax = juce::jlimit (1, 127, juce::jmax (s.velocityMin, s.velocityMax));

        // File Notes: only the imported file's exact notes, the commoner ones more often
        int poolTotal = 0;
        if (s.fileNotes)
            for (auto c : current.notePool)
                poolTotal += c;
        const bool usePool = s.fileNotes && poolTotal > 0;
        auto pickFromPool = [&] (int roll)
        {
            for (int t = -24; t <= 24; ++t)
            {
                roll -= current.poolCount (t);
                if (roll < 0)
                    return t;
            }
            return 0;
        };

        Pattern p;
        p.notePool = current.notePool;       // the pool stays until another file is imported
        p.scaleSteps = s.scaleLock;
        p.key = s.scaleKey;
        p.scale = s.scaleIndex;
        for (size_t i = 0; i < p.steps.size(); ++i)
        {
            const auto& old = current.steps[i];
            auto& step = p.steps[i];

            // always draw all four values (see header)
            const float activeRoll = rng.nextFloat();
            const int pitchRoll = rng.nextInt (juce::jmax (1, usePool ? poolTotal : numCandidates));
            const int velocityRoll = vMin + rng.nextInt (vMax - vMin + 1);
            const float tieRoll = rng.nextFloat();
            const float ratchetRoll = rng.nextFloat();
            const float ratchetCountRoll = rng.nextFloat();
            const float reverseRoll = rng.nextFloat();
            const float accentRoll = rng.nextFloat();     // drawn last, so older lanes keep their values per seed

            const bool active = s.lockRests ? ! old.isRest() : activeRoll < s.density;

            step.transpose = s.lockPitch ? old.transpose
                           : usePool ? pickFromPool (pitchRoll)
                           : (numCandidates > 0 ? candidates[pitchRoll] : 0);

            // rests are Volume 0; playing steps keep the volume you drew (full if it was a rest)
            if (s.lockRests)
                step.volume = old.isRest() ? 0 : old.volume;     // a rest stays a rest whatever made it one
            else
                step.volume = active ? (old.volume > 0 ? old.volume : 127) : 0;

            if (s.lockVelocity)
                step.velocity = old.velocity > 0 ? old.velocity : 100;
            else
                step.velocity = velocityRoll;

            step.tie = s.lockTies ? old.tie : (active && tieRoll < s.tieProbability);

            // ratchets: 2 is most common, then 3, then 4
            if (s.lockRatchets)
                step.ratchet = old.ratchet;
            else if (active && ratchetRoll < s.ratchetProbability)
                step.ratchet = ratchetCountRoll < 0.5f ? 2 : (ratchetCountRoll < 0.8f ? 3 : 4);
            else
                step.ratchet = 1;

            if (s.lockReverse)
                step.reverse = old.reverse;
            else
                step.reverse = active && reverseRoll < s.reverseProbability;

            if (s.lockAccents)
                step.accent = old.accent;
            else
                step.accent = active && accentRoll < s.accentProbability;
        }
        return p;
    }
}
