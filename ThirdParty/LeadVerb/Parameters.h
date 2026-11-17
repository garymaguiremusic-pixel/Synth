#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/ReverbMath.h"
#include "DSP/MassiveReverb.h"

//==============================================================================
// Parameter IDs, choice lists and human-readable units shared by the
// processor and the editor. The knob -> unit mappings live in DSP/ReverbMath.h.
//==============================================================================
namespace lv
{
namespace ParamIDs
{
    inline constexpr auto mode     = "mode";
    inline constexpr auto sync     = "sync";
    inline constexpr auto predelay = "predelay";
    inline constexpr auto damp     = "damp";
    inline constexpr auto wide     = "wide";
    inline constexpr auto decay    = "decay";
    inline constexpr auto color    = "color";
    inline constexpr auto dryWet   = "dryWet";
    inline constexpr auto shelf    = "shelf";
    inline constexpr auto sidechain = "sidechain";
    inline constexpr auto externalSidechain = "externalSidechain";
    inline constexpr auto lift      = "lift";
    inline constexpr auto drySource = "drySource";
    inline constexpr auto modulation = "modulation";
    inline constexpr auto delayOn       = "delayOn";
    inline constexpr auto delayTime     = "delayTime";
    inline constexpr auto delayType     = "delayType";
    inline constexpr auto delayFeedback = "delayFeedback";
    inline constexpr auto delayMix      = "delayMix";

    // Reverb section: Mode 1 (Spire) or Mode 2 (Massive), and Mode 2's controls
    inline constexpr auto section        = "section";
    inline constexpr auto massiveType    = "massiveType";
    inline constexpr auto massiveDryWet  = "massiveDryWet";
    inline constexpr auto massiveSize    = "massiveSize";
    inline constexpr auto massiveDensity = "massiveDensity";
    inline constexpr auto massiveColor   = "massiveColor";

    // Global
    inline constexpr auto bypass   = "bypass";
    inline constexpr auto bpm      = "bpm";
}

inline juce::StringArray getModeNames()
{
    return { "Plate1", "Plate2", "Hall", "Sparkles", "Ambience", "Dmension", "Aura" };
}

inline juce::StringArray getSectionNames()     { return { "Mode 1 (Spire)", "Mode 2 (Massive)" }; }
inline juce::StringArray getMassiveTypeNames() { return { "Reverb", "Small Reverb" }; }

/** Display name of whatever the engine is running: "Plate1" .. "Aura", "Reverb", "Small Reverb". */
inline juce::String engineModeName (int mode)
{
    if (isSpireMode (mode))
        return getModeNames()[mode];
    return getMassiveTypeNames()[mode == ModeMassiveSmall ? 1 : 0];
}

inline juce::StringArray getPredelayDivisionNames()
{
    return { "1/64", "1/32", "1/24", "3/64", "1/16", "1/12", "3/32", "1/8", "1/6", "3/16", "1/4",
             "1/3", "3/8", "1/2", "2/3", "3/4", "1/1", "2/1", "4/1", "8/1", "16/1" };
}

inline juce::StringArray getDelayNoteNames()     { return { "1/32", "1/16", "1/8", "1/4", "1/2", "1/1" }; }
inline juce::StringArray getDelayNoteTypeNames() { return { "Normal", "Dotted", "Triplet" }; }

/** "1/8D", "1/16T", "1/4" */
inline juce::String shortDelayNote (int note, int type)
{
    return getDelayNoteNames()[juce::jlimit (0, kNumDelayNotes - 1, note)] + (type == DelayDotted ? "D" : type == DelayTriplet ? "T" : "");
}

//==============================================================================
inline juce::String formatMs (float ms)
{
    return ms < 100.0f ? juce::String (ms, 1) + " ms" : juce::String (juce::roundToInt (ms)) + " ms";
}

inline juce::String formatHz (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz"
                         : juce::String (hz, hz < 10.0f ? 2 : 0) + " Hz";
}

inline juce::String formatSeconds (float s)
{
    return s < 10.0f ? juce::String (s, 2) + " s" : juce::String (s, 1) + " s";
}

/** Short form for the LCDs: "45ms", "1.25s", "1/16". */
inline juce::String shortPredelay (float knob, bool sync)
{
    if (sync)
        return getPredelayDivisionNames()[predelayKnobToDivision (knob)];
    const float ms = predelayKnobToMs (knob);
    return ms < 10.0f ? juce::String (ms, 1) + "ms" : juce::String (juce::roundToInt (ms)) + "ms";
}

inline juce::String shortSeconds (float s)
{
    return s < 10.0f ? juce::String (s, 2) + "s" : juce::String (s, 1) + "s";
}

inline juce::String describePredelay (float v, bool sync, double bpm)
{
    if (sync)
        return getPredelayDivisionNames()[predelayKnobToDivision (v)] + " = " + formatMs (syncedPredelayMs (v, bpm));
    return formatMs (predelayKnobToMs (v));
}

/** "2.95s", or "inf" when a Spire mode rings practically for ever. */
inline juce::String shortDecay (float seconds)
{
    return seconds >= kEndlessSeconds ? juce::String ("inf") : shortSeconds (seconds);
}

inline juce::String describeDecay (float v, int mode, float dampKnob = 15.0f)
{
    const float s = decayKnobToSeconds (v, mode, dampKnob);
    if (s >= kEndlessSeconds)
        return "Endless (the tank never decays)";
    return "RT60 " + formatSeconds (s) + " (at this Damp)";
}

inline juce::String describeDamp (float v) { return "Tank low-pass " + formatHz (spire::dampHz (norm100 (v))); }
inline juce::String describeModulation (float v)
{
    return v < 0.05f ? juce::String ("Off")
                     : "Vibrato +-" + juce::String (modulationKnobToCents (v), 1) + " cents at " + juce::String (modulationKnobToRateHz (v), 1) + " Hz";
}

/** Typed decay time -> seconds: "4.5", "4.5s", "4.5 sec" or "4500ms". Returns a
    negative value when the text isn't a time. */
inline float parseDecayText (const juce::String& text)
{
    auto t = text.trim().toLowerCase().removeCharacters (" ");
    if (t.isEmpty() || (! juce::CharacterFunctions::isDigit (t[0]) && t[0] != '.'))
        return -1.0f;
    const bool millis = t.endsWith ("ms");
    const float value = t.getFloatValue();
    if (value <= 0.0f)
        return -1.0f;
    return millis ? value * 0.001f : value;
}

/** Knob value as shown to the user: "12" for whole numbers, "12.3" otherwise. */
inline juce::String formatKnobValue (float v)
{
    const float tenths = std::round (v * 10.0f) / 10.0f;
    return std::abs (tenths - std::round (tenths)) < 0.01f ? juce::String (juce::roundToInt (tenths)) : juce::String (tenths, 1);
}
inline juce::String describeShelf (float v)           { return v < 0.05f ? juce::String ("Off") : "+" + juce::String (shelfKnobToDb (v), 1) + " dB above 4 kHz"; }
inline juce::String describeSidechain (float v)       { return v < 0.05f ? juce::String ("Off") : "Up to -" + juce::String (sidechainKnobToAmount (v) * 40.0f, 0) + " dB ducking"; }
inline juce::String describeLift (float v)            { return v < 0.05f ? juce::String ("Off") : juce::String (juce::roundToInt (liftKnobToGain (v) * 100.0f)) + " % octave-up into the reverb"; }
inline juce::String describeDrySource (float v)       { return v < 0.05f ? juce::String ("Off") : "+" + juce::String (juce::roundToInt (drySourceKnobToAmount (v) * 100.0f)) + " % untouched dry"; }
inline juce::String describeDelayFeedback (float v)  { return juce::String (juce::roundToInt (delayFeedbackKnobToGain (v) * 100.0f)) + " % fed back each repeat"; }
inline juce::String describeDelayMix (float v)       { return juce::String (juce::roundToInt (delayMixKnobToMix (v) * 100.0f)) + " % echoes"; }
inline juce::String describeDryWet (float v)
{
    const float m = dryWetToMix (v);
    return "Dry " + juce::String (juce::roundToInt (mixDryGain (m) * 100.0f)) + " %, wet " + juce::String (juce::roundToInt (mixWetGain (m) * 100.0f)) + " %";
}

inline juce::String describeWide (float v)
{
    const float w = wideKnobToWidth (v);
    if (w < 0.005f) return "Mono";
    return juce::String (w * 100.0f, 0) + " % width" + (w > 1.005f ? " (widened)" : "");
}

/** Mode 1: Spire's Color, a 1-pole low-pass and high-pass before the reverb. */
inline juce::String describeColor (float v)
{
    const float c = norm100 (v);
    return "Low-pass " + formatHz (spire::colorLowpassHz (c)) + ", high-pass " + formatHz (spire::colorHighpassHz (c));
}

//==============================================================================
// Mode 2 (Massive)
inline juce::String describeMassiveDryWet (float v)
{
    return "Dry " + juce::String (juce::roundToInt (massiveDryGain (v) * 100.0f)) + " %, wet " + juce::String (juce::roundToInt (massiveWetGain (v) * 100.0f)) + " %";
}

/** Mode 2: the RT60 the Size knob (0..100) asks for. */
inline float massiveTailSeconds (float v, int type) { return MassiveReverb::tailSeconds (norm100 (v), type); }

inline juce::String describeMassiveSize (float v, int type)
{
    return "About " + formatSeconds (massiveTailSeconds (v, type)) + " of tail";
}

inline juce::String describeMassiveDensity (float v)
{
    return v < 30.0f ? juce::String ("Sparse, distinct echoes") : v < 70.0f ? juce::String ("Smooth") : juce::String ("Dense, diffuse");
}

inline juce::String describeMassiveColor (float v, int type)
{
    const auto in = MassiveReverb::defaultInternals (type);
    const float hz = MassiveReverb::constants (type).dampHz * std::pow (2.0f, (norm100 (v) - 0.5f) * 2.0f * in.colorOctaves);
    return "Damping low-pass " + formatHz (std::min (hz, 20000.0f));
}

} // namespace lv
