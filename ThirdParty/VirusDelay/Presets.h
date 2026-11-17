#pragma once

#include "Parameters.h"

namespace vd
{

/** Factory preset. Knob values are on the 0..100 scale; choices are indices. */
struct FactoryPreset
{
    const char* name;
    int   mode;
    float send;
    float feedback;

    // Classic
    const char* classicType;
    float time;
    int   clock;
    float color;
    float rate;
    float depth;
    int   shape;

    // Tape
    int   clockLeft;
    int   clockRight;
    int   tapeClock;
    float centerFreq;
    float bandwidth;
    float modulation;

    // Spire
    float spireDelayL   = 50.0f;     // 1/4
    float spireDelayR   = 60.0f;     // 3/8
    bool  spirePingPong = false;
    bool  spireSync     = true;
    float spireRate     = 20.0f;     // 1/1
    float spireModulate = 0.0f;
    float spireFeedback = 40.0f;
    float spireWide     = 100.0f;
    float spireColor    = 50.0f;
    float spireDryWet   = 30.0f;
};

// Classic clock: 0 Off, 1 1/64, 2 1/32, 3 1/16, 4 1/16T, 5 1/16., 6 1/8, 7 1/8T, 8 1/8.,
//                9 1/4, 10 1/4T, 11 1/4., 12 1/2, 13 1/1
// Tape clock:    0 1/32, 1 1/16, 2 2/16, 3 3/16, 4 4/16, 5 5/16
// Rate knobs:    13 = 2/1, 20 = 1/1, 27 = 1/2, 47 = 1/4, 67 = 1/8
inline constexpr FactoryPreset factoryPresets[] =
{
    //  name                 mode             send  fb    type             time clk color rate depth shape          cL cR tC  centre bw    mod
    { "Slapback",           ModeSimple,      72.f,  9.f, "Simple Delay",  63.f, 0, 42.f, 20.f,  0.f, ShapeSine,     3, 4, 4, 50.f, 79.f, 16.f },
    { "Dotted Eighth",      ModeSimple,      63.f, 49.f, "Simple Delay",  76.f, 8, 45.f, 13.f,  6.f, ShapeSine,     3, 4, 4, 50.f, 79.f, 16.f },
    { "Ping Pong Wide",     ModePingPong,     68.f, 57.f, "Ping Pong 2:1", 76.f, 9, 47.f, 20.f, 11.f, ShapeTriangle, 3, 4, 4, 50.f, 79.f, 16.f },
    { "Pattern 3+4",        ModePattern,      65.f, 52.f, "Pattern 3+4",   76.f, 9, 56.f, 13.f,  5.f, ShapeSine,     3, 4, 4, 50.f, 79.f, 16.f },
    { "Chorus Echo",        ModeSimple,      79.f, 24.f, "Simple Delay",  39.f, 0, 50.f, 20.f, 47.f, ShapeTriangle, 3, 4, 4, 50.f, 79.f, 16.f },
    { "Tape Clocked Dub",   ModeTapeClocked, 71.f, 74.f, "Simple Delay",  76.f, 0, 50.f, 20.f,  0.f, ShapeSine,     3, 4, 4, 41.f, 55.f, 22.f },
    { "Tape Free Wobble",   ModeTapeFree,    71.f, 60.f, "Simple Delay",  76.f, 0, 50.f, 20.f,  0.f, ShapeSine,     3, 4, 4, 50.f, 71.f, 67.f },
    { "Tape Doppler Swirl", ModeTapeDoppler, 69.f, 55.f, "Simple Delay",  76.f, 0, 50.f, 20.f,  0.f, ShapeSine,     3, 4, 3, 57.f, 79.f, 55.f },

    // Spire presets:                                                                                          delL  delR  pp    sync  rate  mod   fb    wide   color dry/wet
    { "Spire Ping Pong",    ModeSpire,       63.f, 38.f, "Simple Delay",  76.f, 0, 50.f, 20.f,  0.f, ShapeSine,     3, 4, 4, 50.f, 79.f, 16.f, 50.f, 50.f, true, true, 20.f,  6.f, 55.f, 100.f, 42.f, 30.f },
    { "Spire Cross Wide",   ModeSpire,       63.f, 38.f, "Simple Delay",  76.f, 0, 50.f, 20.f,  0.f, ShapeSine,     3, 4, 4, 50.f, 79.f, 16.f, 60.f, 45.f, true, true, 20.f, 15.f, 50.f,   0.f, 60.f, 28.f },
};

inline constexpr int numFactoryPresets = (int) std::size (factoryPresets);

} // namespace vd
