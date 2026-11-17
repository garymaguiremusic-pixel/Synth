#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <cmath>

//==============================================================================
// Parameter IDs, choice lists and the 0..100 -> real-unit mappings shared by
// the DSP, the processor and the editor.
//==============================================================================
namespace vd
{
namespace ParamIDs
{
    // Mode
    inline constexpr auto mode       = "mode";
    inline constexpr auto send       = "send";
    inline constexpr auto feedback   = "feedback";

    // Classic
    inline constexpr auto pingPongRatio = "pingPongRatio";
    inline constexpr auto patternType   = "patternType";
    inline constexpr auto time       = "time";
    inline constexpr auto clock      = "clock";
    inline constexpr auto color      = "color";
    inline constexpr auto rate       = "rate";
    inline constexpr auto depth      = "depth";
    inline constexpr auto shape      = "shape";

    // Tape
    inline constexpr auto clockLeft  = "clockLeft";
    inline constexpr auto clockRight = "clockRight";
    inline constexpr auto tapeClock  = "tapeClock";
    inline constexpr auto centerFreq = "centerFreq";
    inline constexpr auto bandwidth  = "bandwidth";
    inline constexpr auto modulation = "modulation";

    // Spire
    inline constexpr auto spireDelayL   = "spireDelayL";
    inline constexpr auto spireDelayR   = "spireDelayR";
    inline constexpr auto spirePingPong = "spirePingPong";
    inline constexpr auto spireSync     = "spireSync";
    inline constexpr auto spireRate     = "spireRate";
    inline constexpr auto spireModulate = "spireModulate";
    inline constexpr auto spireFeedback = "spireFeedback";
    inline constexpr auto spireWide     = "spireWide";
    inline constexpr auto spireColor    = "spireColor";
    inline constexpr auto spireDryWet   = "spireDryWet";

    // Global output section (all modes)
    inline constexpr auto drySource  = "drySource";
    inline constexpr auto duckOn     = "duckOn";
    inline constexpr auto duckAmount = "duckAmount";
    inline constexpr auto moveHP     = "moveHP";
    inline constexpr auto movePhaser = "movePhaser";
    inline constexpr auto moveFlanger = "moveFlanger";
    inline constexpr auto moveRate   = "moveRate";
    inline constexpr auto moveDepth  = "moveDepth";
    inline constexpr auto moveMix    = "moveMix";
    inline constexpr auto width      = "width";

    // Global
    inline constexpr auto bypass     = "bypass";
    inline constexpr auto bpm        = "bpm";
}

//==============================================================================
enum DelayMode : int
{
    ModeSimple = 0,     // Simple, Ping Pong and Pattern share the "classic" digital engine
    ModePingPong,
    ModePattern,
    ModeTapeClocked,
    ModeTapeFree,
    ModeTapeDoppler,
    ModeSpire,
    NumDelayModes
};

enum LfoShape : int
{
    ShapeSine = 0,
    ShapeTriangle,
    ShapeSaw,
    ShapeSquare,
    ShapeSampleHold,
    NumLfoShapes
};

inline bool isTapeMode (int mode)   { return mode == ModeTapeClocked || mode == ModeTapeFree || mode == ModeTapeDoppler; }
inline bool isSpireMode (int mode)  { return mode == ModeSpire; }
inline bool isClassicMode (int mode) { return mode == ModeSimple || mode == ModePingPong || mode == ModePattern; }

inline juce::StringArray getShapeNames()
{
    return { "Sine", "Triangle", "Sawtooth", "Square", "S&H" };
}

inline juce::StringArray getModeNames()
{
    return { "Simple", "Ping Pong", "Pattern", "Tape Clocked", "Tape Free", "Tape Doppler", "Spire Delay" };
}

//==============================================================================
// Classic delay types
//==============================================================================
enum class ClassicKind { Off, Simple, PingPong, Pattern };

struct ClassicType
{
    const char* name;
    ClassicKind kind;
    int a, b;     // Ping Pong: L:R ratio. Pattern: the two step lengths.
};

// Ordered by first number, then second number.
inline constexpr ClassicType classicTypes[] =
{
    { "Off",           ClassicKind::Off,      0, 0 },
    { "Simple Delay",  ClassicKind::Simple,   1, 1 },
    { "Ping Pong 2:1", ClassicKind::PingPong, 2, 1 },
    { "Ping Pong 4:1", ClassicKind::PingPong, 4, 1 },
    { "Ping Pong 4:3", ClassicKind::PingPong, 4, 3 },
    { "Ping Pong 8:7", ClassicKind::PingPong, 8, 7 },
    { "Pattern 1+1",   ClassicKind::Pattern,  1, 1 },
    { "Pattern 2+1",   ClassicKind::Pattern,  2, 1 },
    { "Pattern 2+3",   ClassicKind::Pattern,  2, 3 },
    { "Pattern 2+5",   ClassicKind::Pattern,  2, 5 },
    { "Pattern 3+1",   ClassicKind::Pattern,  3, 1 },
    { "Pattern 3+2",   ClassicKind::Pattern,  3, 2 },
    { "Pattern 3+3",   ClassicKind::Pattern,  3, 3 },
    { "Pattern 3+4",   ClassicKind::Pattern,  3, 4 },
    { "Pattern 3+5",   ClassicKind::Pattern,  3, 5 },
    { "Pattern 4+1",   ClassicKind::Pattern,  4, 1 },
    { "Pattern 4+3",   ClassicKind::Pattern,  4, 3 },
    { "Pattern 4+5",   ClassicKind::Pattern,  4, 5 },
    { "Pattern 5+1",   ClassicKind::Pattern,  5, 1 },
    { "Pattern 5+2",   ClassicKind::Pattern,  5, 2 },
    { "Pattern 5+3",   ClassicKind::Pattern,  5, 3 },
    { "Pattern 5+4",   ClassicKind::Pattern,  5, 4 },
    { "Pattern 5+5",   ClassicKind::Pattern,  5, 5 },
};

inline constexpr int numClassicTypes = (int) std::size (classicTypes);
constexpr int kClassicOff = 0, kClassicSimple = 1;

inline const ClassicType& getClassicType (int index)
{
    return classicTypes[juce::jlimit (0, numClassicTypes - 1, index)];
}

inline juce::StringArray getClassicTypeNames()
{
    juce::StringArray names;
    for (auto& t : classicTypes)
        names.add (t.name);
    return names;
}

inline int findClassicType (const char* name)
{
    for (int i = 0; i < numClassicTypes; ++i)
        if (juce::String (classicTypes[i].name) == name)
            return i;
    return kClassicSimple;
}

/** Ping Pong ratios and Patterns as separate selector lists ("2:1", "3+4" ...). */
inline juce::StringArray getClassicSubNames (ClassicKind kind)
{
    juce::StringArray names;
    for (auto& t : classicTypes)
        if (t.kind == kind)
            names.add (juce::String (t.name).fromLastOccurrenceOf (" ", false, false));
    return names;
}

inline int firstClassicTypeOf (ClassicKind kind)
{
    for (int i = 0; i < numClassicTypes; ++i)
        if (classicTypes[i].kind == kind)
            return i;
    return kClassicSimple;
}

inline int countClassicTypesOf (ClassicKind kind)
{
    int n = 0;
    for (auto& t : classicTypes)
        n += t.kind == kind ? 1 : 0;
    return n;
}

/** The engine's type for a mode and its Ratio / Pattern selection (no allocation: audio thread). */
inline int classicTypeFor (int mode, int pingPongIndex, int patternIndex)
{
    if (mode == ModePingPong) return firstClassicTypeOf (ClassicKind::PingPong) + juce::jlimit (0, countClassicTypesOf (ClassicKind::PingPong) - 1, pingPongIndex);
    if (mode == ModePattern)  return firstClassicTypeOf (ClassicKind::Pattern)  + juce::jlimit (0, countClassicTypesOf (ClassicKind::Pattern) - 1, patternIndex);
    return kClassicSimple;
}

inline bool isPingPong (int type)  { return getClassicType (type).kind == ClassicKind::PingPong; }
inline bool isPattern (int type)   { return getClassicType (type).kind == ClassicKind::Pattern; }

/** Relative lengths of the two delay lines (A = left/first step, B = right/second step). */
inline void getLineRatios (int type, float& a, float& b)
{
    const auto& t = getClassicType (type);
    switch (t.kind)
    {
        case ClassicKind::PingPong:
            a = 1.0f;
            b = (float) t.b / (float) t.a;
            break;
        case ClassicKind::Pattern:
        {
            const float unit = 1.0f / (float) std::max (t.a, t.b);   // longest step = base time
            a = unit * (float) t.a;
            b = unit * (float) t.b;
            break;
        }
        case ClassicKind::Off:
        case ClassicKind::Simple:
        default:
            a = b = 1.0f;
            break;
    }
}

//==============================================================================
// Clock divisions
//==============================================================================
constexpr int kClockOff = 0;

/** Classic clock: Off or tempo divisions. */
inline juce::StringArray getClockNames()
{
    return { "Off", "1/64", "1/32", "1/16", "1/16 T", "1/16 .", "1/8", "1/8 T", "1/8 .",
             "1/4", "1/4 T", "1/4 .", "1/2", "1/1" };
}

/** Length of a classic clock division in quarter-note beats (0 for "Off"). */
inline double clockDivisionInBeats (int index)
{
    static constexpr double beats[] = { 0.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 6.0, 3.0 / 8.0,
                                        1.0 / 2.0, 1.0 / 3.0, 3.0 / 4.0, 1.0, 2.0 / 3.0, 1.5, 2.0, 4.0 };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, index)];
}

/** Tape clock divisions (Clock Left / Clock Right / Clock). */
inline juce::StringArray getTapeClockNames()
{
    return { "1/32", "1/16", "2/16", "3/16", "4/16", "5/16" };
}

inline double tapeClockInBeats (int index)
{
    static constexpr double beats[] = { 0.125, 0.25, 0.5, 0.75, 1.0, 1.25 };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, index)];
}

/** Time knob is used in Simple / Ping Pong / Pattern when the clock is Off. */
inline bool isTimeActive (int clock)  { return clock == kClockOff; }

//==============================================================================
constexpr float kMinDelayMs      = 1.0f;
constexpr float kMaxKnobDelayMs  = 2000.0f;
constexpr float kMaxDelayMs      = 2500.0f;   // longest delay the engines accept
constexpr float kMaxModDepthMs   = 12.0f;
constexpr float kMaxFeedback     = 0.95f;
constexpr float kDefaultBpm      = 120.0f;

// All knobs run 0..100 (centre = 50) ------------------------------------------
inline float norm100 (float v)         { return juce::jlimit (0.0f, 100.0f, v) / 100.0f; }
inline float timeKnobToMs (float v)    { return kMinDelayMs * std::pow (kMaxKnobDelayMs / kMinDelayMs, norm100 (v)); }
inline float feedbackToGain (float v)  { return kMaxFeedback * norm100 (v); }
inline float sendToGain (float v)      { return norm100 (v); }
inline float depthToMs (float v)       { const float n = norm100 (v); return kMaxModDepthMs * n * n; }
inline float centerToHz (float v)      { return 80.0f * std::pow (100.0f, norm100 (v)); }          // 80 Hz .. 8 kHz
inline float bandwidthToOctaves (float v) { return 0.3f + 9.7f * norm100 (v); }                    // 0.3 .. 10 octaves
inline float modulationToAmount (float v) { return norm100 (v); }

/** Color: 0..50 -> -1..0 (low-pass), 50 = flat, 50..100 -> 0..1 (high-pass). */
inline float colorToBipolar (float v)  { return juce::jlimit (-1.0f, 1.0f, (v - 50.0f) / 50.0f); }

inline float colorLowpassHz (float bipolar)  { return bipolar < 0.0f ? 20000.0f * std::pow (250.0f / 20000.0f, -bipolar) : 20000.0f; }
inline float colorHighpassHz (float bipolar) { return bipolar > 0.0f ? 20.0f * std::pow (200.0f, bipolar) : 20.0f; }

//==============================================================================
// Rate knobs (Classic LFO, Spire, Movement): tempo-synced note values, slow -> fast
//==============================================================================
inline juce::StringArray getRateDivisionNames()
{
    return { "8/1", "4/1", "2/1", "1/1", "1/2", "1/2 T", "1/4 .", "1/4", "1/4 T",
             "1/8 .", "1/8", "1/8 T", "1/16 .", "1/16", "1/16 T", "1/32" };
}

constexpr int kNumRateDivisions = 16;

/** One LFO cycle in quarter-note beats. */
inline double rateDivisionInBeats (int index)
{
    static constexpr double beats[] = { 32.0, 16.0, 8.0, 4.0, 2.0, 4.0 / 3.0, 1.5, 1.0, 2.0 / 3.0,
                                        0.75, 0.5, 1.0 / 3.0, 0.375, 0.25, 1.0 / 6.0, 0.125 };
    return beats[juce::jlimit (0, kNumRateDivisions - 1, index)];
}

inline int rateKnobToDivision (float v)            { return juce::roundToInt (norm100 (v) * (float) (kNumRateDivisions - 1)); }
inline float rateKnobToHz (float v, double bpm)    { return (float) (std::max (1.0, bpm) / 60.0 / rateDivisionInBeats (rateKnobToDivision (v))); }

//==============================================================================
// Human readable units (tooltips / host display)
inline juce::String formatMs (float ms)
{
    return ms < 100.0f ? juce::String (ms, 1) + " ms" : juce::String (juce::roundToInt (ms)) + " ms";
}

inline juce::String formatHz (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz"
                         : juce::String (hz, hz < 10.0f ? 2 : 0) + " Hz";
}

inline juce::String describeColor (float v)
{
    const float c = colorToBipolar (v);
    if (std::abs (c) < 0.01f) return "Flat";
    return c < 0.0f ? "Low-pass " + formatHz (colorLowpassHz (c))
                    : "High-pass " + formatHz (colorHighpassHz (c));
}

inline juce::String describeTime (float v)       { return formatMs (timeKnobToMs (v)); }
inline juce::String describeFeedback (float v)   { return juce::String (feedbackToGain (v) * 100.0f, 1) + " %"; }
inline juce::String describeSend (float v)       { return juce::String (sendToGain (v) * 100.0f, 1) + " %"; }
inline juce::String describeRate (float v)       { return getRateDivisionNames()[rateKnobToDivision (v)]; }
inline juce::String describeRateAtTempo (float v, double bpm) { return describeRate (v) + " = " + formatHz (rateKnobToHz (v, bpm)); }
inline juce::String describeDepth (float v)      { return "+" + juce::String (depthToMs (v), 2) + " ms"; }
inline juce::String describeCenter (float v)     { return formatHz (centerToHz (v)); }
inline juce::String describeBandwidth (float v)  { return juce::String (bandwidthToOctaves (v), 1) + " octaves"; }
inline juce::String describeModulation (float v) { return juce::String (norm100 (v) * 100.0f, 0) + " %"; }

//==============================================================================
// Knob value display: whole numbers normally, one decimal after Shift fine-tuning
inline juce::String formatKnobValue (double v)
{
    const double r = std::round (v * 10.0) / 10.0;
    return std::abs (r - std::round (r)) < 0.05 ? juce::String (juce::roundToInt (r)) : juce::String (r, 1);
}

//==============================================================================
// Global output section
inline float drySourceToGain (float v)        { const float n = norm100 (v); return n * n; }   // 50 ~ -12 dB, 100 = 0 dB
inline float duckAmountToNorm (float v)       { return norm100 (v); }
inline float moveDepthToNorm (float v)        { return norm100 (v); }

inline juce::String describeDrySource (float v)
{
    const float g = drySourceToGain (v);
    return g <= 0.0f ? juce::String ("Off") : juce::String (20.0f * std::log10 (g), 1) + " dB of untouched dry";
}
inline juce::String describeDuckAmount (float v) { return "up to -" + juce::String (36.0f * duckAmountToNorm (v), 1) + " dB while the source plays"; }
inline juce::String describeMoveDepth (float v)  { return juce::String (norm100 (v) * 100.0f, 0) + " %"; }
/** Width: 0 = mono, 50 = as is, 100 = extra wide. Returns the 0..2 width factor. */
inline float widthToFactor (float v)            { return 2.0f * norm100 (v); }
inline juce::String describeWidth (float v)
{
    const float w = widthToFactor (v);
    if (w < 0.01f) return "Mono repeats";
    if (std::abs (w - 1.0f) < 0.01f) return "Normal";
    return w < 1.0f ? juce::String (w * 100.0f, 0) + " % width" : "Extra wide +" + juce::String ((w - 1.0f) * 100.0f, 0) + " %";
}
inline juce::String describeMoveMix (float v)    { return juce::String (norm100 (v) * 100.0f, 0) + " % effected"; }

//==============================================================================
// Spire Delay
//==============================================================================
constexpr float kSpireMaxDelayMs = 32000.0f;   // 16/1 at 120 BPM


inline juce::StringArray getSpireDivisionNames()
{
    return { "1/64", "1/32", "1/24", "3/64", "1/16", "1/12", "3/32", "1/8", "1/6", "3/16", "1/4",
             "1/3", "3/8", "1/2", "2/3", "3/4", "1/1", "2/1", "4/1", "8/1", "16/1" };
}

/** Division length in quarter-note beats (1/1 = one 4/4 bar). */
inline double spireDivisionInBeats (int index)
{
    static constexpr double beats[] = { 1.0 / 16.0, 1.0 / 8.0, 1.0 / 6.0, 3.0 / 16.0, 1.0 / 4.0, 1.0 / 3.0,
                                        3.0 / 8.0, 1.0 / 2.0, 2.0 / 3.0, 3.0 / 4.0, 1.0, 4.0 / 3.0, 1.5,
                                        2.0, 8.0 / 3.0, 3.0, 4.0, 8.0, 16.0, 32.0, 64.0 };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, index)];
}

constexpr int kNumSpireDivisions = 21;

/** With SYNC on, the Delay L/R knobs step through the 21 divisions. */
inline int spireKnobToDivision (float v)       { return juce::roundToInt (norm100 (v) * (float) (kNumSpireDivisions - 1)); }
/** With SYNC off, the Delay L/R knobs set a free time from 1 ms to 2 s. */
inline float spireKnobToFreeMs (float v)        { return kMinDelayMs * std::pow (kMaxKnobDelayMs / kMinDelayMs, norm100 (v)); }
inline float spireFeedbackToGain (float v)      { return kMaxFeedback * norm100 (v); }
inline float spireModulateToMs (float v)        { const float n = norm100 (v); return 8.0f * n * n; }
inline float spireToBipolar (float v)           { return juce::jlimit (-1.0f, 1.0f, (v - 50.0f) / 50.0f); }
inline float spireDryWetToMix (float v)         { return norm100 (v); }

inline juce::String describeSpireDelay (float v, bool sync, double bpm)
{
    if (sync)
    {
        const int index = spireKnobToDivision (v);
        const double ms = std::min ((double) kSpireMaxDelayMs, 60000.0 / std::max (1.0, bpm) * spireDivisionInBeats (index));
        return getSpireDivisionNames()[index] + " = " + formatMs ((float) ms);
    }
    return formatMs (spireKnobToFreeMs (v));
}

inline juce::String describeSpireWide (float v, bool pingPong)
{
    const float w = spireToBipolar (v);
    if (std::abs (w) < 0.002f)
        return "Mono";
    const juce::String amount = juce::String (std::abs (w) * 100.0f, 0) + " % width";
    if (pingPong)
        return (w < 0.0f ? "Cross feedback, " : "Ping pong, ") + amount;
    return (w < 0.0f ? "L/R swapped, " : "Normal, ") + amount;
}

inline juce::String describeSpireFeedback (float v) { return juce::String (spireFeedbackToGain (v) * 100.0f, 1) + " %"; }
inline juce::String describeSpireModulate (float v) { return "+" + juce::String (spireModulateToMs (v), 2) + " ms"; }
inline juce::String describeSpireDryWet (float v)   { return juce::String (spireDryWetToMix (v) * 100.0f, 0) + " % wet"; }

inline juce::String describeSpireColor (float v)
{
    const float c = spireToBipolar (v);
    if (std::abs (c) < 0.01f) return "Flat";
    return c < 0.0f ? "Hi cut " + formatHz (colorLowpassHz (c))
                    : "Low cut " + formatHz (colorHighpassHz (c));
}

} // namespace vd
