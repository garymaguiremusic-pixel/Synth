#pragma once

#include "Parameters.h"

namespace lv
{

/** Factory preset. Mode 1: Predelay and Decay are 0..1000, the other knobs 0..100.
    Mode 2: Dry/Wet, Size, Density and Color are 0..100. Every preset sets both
    sections, so switching section after loading one gives sensible settings. */
struct FactoryPreset
{
    const char* name;
    int   section = SectionSpire;

    // Mode 1 (Spire)
    int   mode = ModePlate1;
    bool  sync = false;
    float predelay = 0.0f;
    float damp = 15.0f;
    float wide = 50.0f;
    float decay = 635.7f;
    float color = 65.0f;
    float dryWet = 50.0f;
    float shelf = 0.0f;
    float sidechain = 0.0f;
    float lift = 0.0f;
    float drySource = 0.0f;
    float modulation = 0.0f;

    // PingPong delay (both sections)
    bool  delayOn = false;
    int   delayTime = 2;      // 0 1/32, 1 1/16, 2 1/8, 3 1/4, 4 1/2, 5 1/1
    int   delayType = DelayDotted;
    float delayFeedback = 45.0f;
    float delayMix = 30.0f;

    // Mode 2 (Massive)
    int   massiveType = MassiveReverb::Reverb;
    float massiveDryWet = 35.0f;
    float massiveSize = 50.0f;
    float massiveDensity = 50.0f;
    float massiveColor = 50.0f;
};

/** A Mode 1 preset: Spire mode, sync, predelay, damp, wide, decay, color, dry/wet. */
constexpr FactoryPreset mode1 (const char* name, int mode, bool sync, float predelay, float damp, float wide,
                               float decay, float color, float dryWet)
{
    FactoryPreset p { name };
    p.mode = mode; p.sync = sync; p.predelay = predelay; p.damp = damp; p.wide = wide;
    p.decay = decay; p.color = color; p.dryWet = dryWet;
    return p;
}

/** A Mode 2 preset: Massive type, dry/wet, size, density, color. */
constexpr FactoryPreset mode2 (const char* name, int type, float dryWet, float size, float density, float color)
{
    FactoryPreset p { name };
    p.section = SectionMassive;
    p.massiveType = type; p.massiveDryWet = dryWet; p.massiveSize = size; p.massiveDensity = density; p.massiveColor = color;
    return p;
}

constexpr FactoryPreset withDelay (FactoryPreset p, int time, int type, float feedback, float mix)
{
    p.delayOn = true; p.delayTime = time; p.delayType = type; p.delayFeedback = feedback; p.delayMix = mix;
    return p;
}

constexpr FactoryPreset withExtras (FactoryPreset p, float shelf, float sidechain, float lift, float drySource, float modulation)
{
    p.shelf = shelf; p.sidechain = sidechain; p.lift = lift; p.drySource = drySource; p.modulation = modulation;
    return p;
}

// Mode 1 Dry/Wet follows Spire's law: the dry stays at full level up to about 85
// while the wet comes up (50 = wet at about -8 dB), then the dry fades out.
// Mode 2 Dry/Wet is an equal-power crossfade.
//
// Synced predelay: the knob steps through 21 divisions, 50 per step:
//   0 1/64, 50 1/32, 100 1/24, 150 3/64, 200 1/16, 250 1/12, 300 3/32, 350 1/8, 400 1/6,
//   450 3/16, 500 1/4, 550 1/3, 600 3/8, 650 1/2, 700 2/3, 750 3/4, 800 1/1, 850 2/1,
//   900 4/1, 950 8/1, 1000 16/1
inline constexpr FactoryPreset factoryPresets[] =
{
    //            name                  mode           sync   predly damp  wide   decay  color dry/wet
    // Mode 1: Spire's seven modes at Spire's own settings (Plate1 at the capture's 2.95 s)
    mode1 ("Spire Plate 1",        ModePlate1,    false,   0.f, 15.f,  50.f, 310.f,  65.f, 50.f),
    mode1 ("Spire Plate 2",        ModePlate2,    false,   0.f, 15.f,  50.f, 635.7f, 65.f, 50.f),
    mode1 ("Spire Hall",           ModeHall,      false,   0.f, 15.f,  50.f, 635.7f, 65.f, 45.f),
    mode1 ("Spire Sparkles",       ModeSparkles,  false,   0.f, 15.f,  50.f, 400.f,  65.f, 45.f),
    mode1 ("Spire Ambience",       ModeAmbience,  false,   0.f, 15.f,  50.f, 635.7f, 65.f, 50.f),
    mode1 ("Spire Dmension",       ModeDmension,  false,   0.f, 15.f,  50.f, 635.7f, 65.f, 60.f),
    mode1 ("Spire Aura",           ModeAura,      false,   0.f, 15.f,  50.f, 635.7f, 65.f, 50.f),
    mode1 ("Synced Spire Plate",   ModePlate1,    true,  200.f, 20.f,  80.f, 450.f,  60.f, 55.f),
    //                                                                                                 shelf s/chain lift  drysrc mod
    withExtras (mode1 ("EDM Ducked Plate",     ModePlate1, true, 200.f, 20.f, 75.f, 420.f, 60.f, 62.f), 45.f, 70.f, 0.f, 0.f, 0.f),
    withExtras (mode1 ("Trance Lead Plate",    ModePlate1, true, 200.f, 20.f, 80.f, 420.f, 60.f, 66.f), 35.f, 40.f, 35.f, 100.f, 0.f),
    withExtras (mode1 ("Pumping Spire Hall",   ModeHall,   false, 150.f, 25.f, 85.f, 600.f, 60.f, 69.f), 30.f, 85.f, 0.f, 0.f, 0.f),
    withExtras (mode1 ("Vibrato Sparkles",     ModeSparkles, false, 250.f, 20.f, 85.f, 450.f, 65.f, 62.f), 30.f, 0.f, 25.f, 60.f, 35.f),
    withDelay (withExtras (mode1 ("PingPong Trance Plate", ModePlate1, true, 200.f, 20.f, 80.f, 420.f, 60.f, 66.f), 35.f, 40.f, 30.f, 100.f, 15.f),
               2, DelayDotted, 50.f, 35.f),
    withDelay (mode1 ("Triplet Echo Hall",    ModeHall,   true, 350.f, 25.f, 85.f, 600.f, 62.f, 62.f), 2, DelayTriplet, 45.f, 30.f),

    // Mode 2: Massive's reverbs                     type                     dry/wet size  dens  color
    mode2 ("Massive Reverb",          MassiveReverb::Reverb, 35.f,  50.f, 50.f, 50.f),
    mode2 ("Massive Big Space",       MassiveReverb::Reverb, 40.f,  90.f, 70.f, 45.f),
    mode2 ("Massive Dark Wash",       MassiveReverb::Reverb, 45.f, 100.f, 85.f, 15.f),
    mode2 ("Massive Bright Verb",     MassiveReverb::Reverb, 30.f,  60.f, 50.f, 85.f),
    mode2 ("Massive Small Reverb",    MassiveReverb::Small,  35.f,  50.f, 50.f, 50.f),
    mode2 ("Massive Tight Room",      MassiveReverb::Small,  30.f,  20.f, 70.f, 60.f),
    mode2 ("Massive Open Small",      MassiveReverb::Small,  40.f,  85.f, 40.f, 85.f),
    withDelay (mode2 ("Massive PingPong Verb", MassiveReverb::Reverb, 40.f, 70.f, 60.f, 55.f), 2, DelayDotted, 50.f, 35.f),
};

inline constexpr int numFactoryPresets = (int) std::size (factoryPresets);

} // namespace lv
