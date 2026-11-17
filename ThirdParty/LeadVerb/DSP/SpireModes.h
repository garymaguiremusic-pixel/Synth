#pragma once

// Data and lookup helpers for the Spire-style reverb modes (see SpireReverb.h).
// Every constant here comes from the specification recovered from Spire's binary
// (docs/spire_reverb_spec.md, docs/spire_rev_modes.json) and is kept exactly,
// quirks included (wrap-around "next" indices, integer truncations, ...).
// Deliberately free of JUCE so it can be tested on its own.

#include <algorithm>
#include <cmath>

namespace lv::spire
{
constexpr float kRefRate = 29761.0f;   // Dattorro's reference rate: all delays are samples at this rate

//==============================================================================
// Tables
//==============================================================================
inline constexpr float kDecay32[32] = { 0.0f, 0.1225f, 0.2279f, 0.3184f, 0.3967f, 0.4651f, 0.5247f, 0.5773f, 0.6242f, 0.6654f, 0.7026f,
                                        0.7355f, 0.7652f, 0.792f, 0.8159f, 0.8375f, 0.857f, 0.8747f, 0.8907f, 0.9052f, 0.9183f, 0.9302f,
                                        0.9409f, 0.9506f, 0.9594f, 0.9673f, 0.9744f, 0.9808f, 0.9864f, 0.9915f, 0.996f, 1.0f };
inline constexpr float kDamp32[32] = { 0.0f, 0.0058f, 0.012f, 0.0187f, 0.0258f, 0.0336f, 0.0421f, 0.0514f, 0.0616f, 0.0727f, 0.0848f,
                                       0.098f, 0.1125f, 0.1282f, 0.1455f, 0.1644f, 0.185f, 0.2075f, 0.2321f, 0.259f, 0.2884f, 0.321f,
                                       0.3569f, 0.3964f, 0.4402f, 0.4897f, 0.5448f, 0.608f, 0.68f, 0.7657f, 0.8691f, 1.0f };
inline constexpr float kMix32[32] = { 0.0f, 0.011f, 0.0226f, 0.0349f, 0.048f, 0.0619f, 0.0769f, 0.0929f, 0.1099f, 0.128f, 0.1475f,
                                      0.1682f, 0.1905f, 0.2143f, 0.2398f, 0.2673f, 0.2968f, 0.3284f, 0.3624f, 0.399f, 0.4379f, 0.4798f,
                                      0.5242f, 0.5712f, 0.6208f, 0.6725f, 0.7259f, 0.7805f, 0.8358f, 0.8912f, 0.946f, 1.0f };
inline constexpr float kNotchQ128[128] = {
    0.083f, 0.0855f, 0.0881f, 0.0906f, 0.0931f, 0.0956f, 0.0982f, 0.1007f, 0.1031f, 0.1056f, 0.108f, 0.1105f, 0.1129f, 0.1154f, 0.1178f,
    0.1202f, 0.1226f, 0.1251f, 0.1275f, 0.1299f, 0.1323f, 0.1348f, 0.1373f, 0.1397f, 0.1422f, 0.1447f, 0.1472f, 0.1497f, 0.1523f, 0.1549f,
    0.1575f, 0.1601f, 0.1628f, 0.1655f, 0.1683f, 0.1711f, 0.1739f, 0.1767f, 0.1798f, 0.1829f, 0.186f, 0.189f, 0.1921f, 0.1956f, 0.1991f,
    0.2026f, 0.2061f, 0.2096f, 0.2136f, 0.2176f, 0.2217f, 0.2257f, 0.2299f, 0.2346f, 0.2394f, 0.2441f, 0.2488f, 0.2542f, 0.2598f, 0.2654f,
    0.2709f, 0.2771f, 0.2837f, 0.2903f, 0.2969f, 0.3042f, 0.312f, 0.3199f, 0.3278f, 0.3367f, 0.346f, 0.3554f, 0.3648f, 0.3759f, 0.387f,
    0.3982f, 0.4105f, 0.4237f, 0.4369f, 0.4509f, 0.4664f, 0.482f, 0.4982f, 0.5165f, 0.5348f, 0.5542f, 0.5756f, 0.5971f, 0.6206f, 0.6456f,
    0.6706f, 0.6994f, 0.7285f, 0.7596f, 0.7933f, 0.8272f, 0.866f, 0.9047f, 0.948f, 0.9924f, 1.0407f, 1.0914f, 1.1459f, 1.2034f, 1.2652f,
    1.33f, 1.4002f, 1.4728f, 1.5526f, 1.6344f, 1.7239f, 1.8169f, 1.9153f, 2.02f, 2.128f, 2.2444f, 2.3647f, 2.4898f, 2.6221f, 2.7576f,
    2.8991f, 3.0453f, 3.1944f, 3.3499f, 3.5076f, 3.6689f, 3.8337f, 4.0f };

/** 32-point curve at p (0..1), index p*31, linear interpolation; the next index wraps to 0 at the end. */
inline float curve32 (const float (&table)[32], float p)
{
    const float x = std::clamp (p, 0.0f, 1.0f) * 31.0f;
    const int i = (int) x;
    const int next = i < 31 ? i + 1 : 0;
    return table[i] + (table[next] - table[i]) * (x - (float) i);
}

inline float decayCurve (float p) { return curve32 (kDecay32, p); }
inline float dampCurve (float p)  { return curve32 (kDamp32, p); }
inline float mixCurve (float p)   { return curve32 (kMix32, p); }

inline float notchQ (float q)
{
    const float x = std::clamp (q, 0.0f, 1.0f) * 127.0f;
    const int i = (int) x;
    const int next = i < 127 ? i + 1 : 0;
    return kNotchQ128[i] + (kNotchQ128[next] - kNotchQ128[i]) * (x - (float) i);
}

/** Semitone table T[i] = 440 * 2^((i-53)/12), i = 0..119, linearly interpolated. */
inline float noteTable (int i) { return 440.0f * std::pow (2.0f, (float) (i - 53) / 12.0f); }

inline float noteHz (float x)
{
    static const auto table = []
    {
        struct T { float v[120]; } t {};
        for (int i = 0; i < 120; ++i)
            t.v[i] = noteTable (i);
        return t;
    }();
    const float c = std::clamp (x, 0.0f, 119.0f);
    const int i = (int) c;
    const int next = i < 119 ? i + 1 : 0;
    return table.v[i] + (table.v[next] - table.v[i]) * (c - (float) i);
}

/** One-pole coefficient sin(pi*hz/fs), from the 1024-point table sin(pi*i/1023)
    indexed by hz/fs*1023 (the next index wraps to 0 at the end). */
inline float onePoleCoef (float hz, float invFs)
{
    static const auto table = []
    {
        struct T { float v[1024]; } t {};
        for (int i = 0; i < 1024; ++i)
            t.v[i] = (float) std::sin (3.14159265358979323846 * (double) i / 1023.0);
        return t;
    }();
    const float x = std::clamp (hz * invFs * 1023.0f, 0.0f, 1023.0f);
    const int i = (int) x;
    const int next = i < 1023 ? i + 1 : 0;
    return table.v[i] + (table.v[next] - table.v[i]) * (x - (float) i);
}

/** Predelay sync table in "seconds x BPM": 1/64 .. 16/1. */
inline constexpr float kSyncSecondsTimesBpm[21] = { 3.75f, 7.5f, 10.0f, 11.25f, 15.0f, 20.0f, 22.5f, 30.0f, 40.0f, 45.0f, 60.0f,
                                                     80.0f, 90.0f, 120.0f, 160.0f, 180.0f, 240.0f, 480.0f, 960.0f, 1920.0f, 3840.0f };

//==============================================================================
// Mode data (exactly as embedded in Spire; missing JSON entries are 0)
//==============================================================================
struct Tap  { float delay, gain; };
struct Wg   { float delay, depthA, depthB; };   // depthB is only read for WG_L[1]
struct Out  { int node; float delay, gain; };
struct Notch { float hz, q, gainDb; };

struct ModeData
{
    const char* name;
    int trr;
    int useMt, useDf, useTl, usePd;
    float lfoHz[4];
    float sizeMt[2], sizeDiff[2], decTail[2];
    int numMt;   Tap mt[6];
    int numDf;   Tap df[6];
    float inside[2];
    Tap apL[2];  Wg wgL[2];
    Tap apR[2];  Wg wgR[2];
    Out clL[7], clR[7];
    Notch notch[2];
};

inline constexpr ModeData kModes[7] =
{
    { "plate 1", 2, 1, 1, 1, 0, { 3, 1, 4, 12 }, { 0.1f, 0.9f }, { 1, 0 }, { 0, 1 },
      6, { { 100, 0.1f }, { 2000, 0.2f }, { 2200, 0.3f }, { 2400, 0.5f }, { 3400, 0.8f }, { 5000, 0.6f } },
      4, { { 142, 0.5f }, { 107, 0.5f }, { 379, 0.5f }, { 227, 0.5f } },
      { 0.2f, 0.5f },
      { { 672, 0.5f }, { 1800, 0.5f } }, { { 4453, 50, 0 }, { 3720, 40, 50 } },
      { { 908, 0.5f }, { 2656, 0.5f } }, { { 4217, 5, 0 }, { 3163, 20, 0 } },
      { { 3, 266, 0.6f }, { 3, 2974, 0.6f }, { 4, 1913, -0.6f }, { 5, 1996, 0.6f }, { 0, 1990, -0.6f }, { 1, 187, -0.6f }, { 2, 1066, -0.6f } },
      { { 0, 353, 0.6f }, { 0, 3627, 0.6f }, { 1, 1218, -0.6f }, { 2, 2673, 0.6f }, { 3, 2111, -0.6f }, { 4, 335, -0.6f }, { 5, 121, -0.6f } },
      { { 11000, 1, -20 }, { 400, 0.7f, -4 } } },

    { "plate 2", 1, 0, 1, 1, 1, { 3, 1, 4, 12 }, { 0.1f, 0.9f }, { 1, 0 }, { 0, 1 },
      6, { { 100, 0.1f }, { 2000, 0.2f }, { 2200, 0.3f }, { 2400, 0.5f }, { 3400, 0.8f }, { 5000, 0.6f } },
      4, { { 142, 0.5f }, { 107, 0.5f }, { 379, 0.5f }, { 227, 0.5f } },
      { 0.2f, 0.5f },
      { { 672, 0.5f }, { 1800, 0.5f } }, { { 4453, 50, 0 }, { 3720, 40, 50 } },
      { { 908, 0.5f }, { 2656, 0.5f } }, { { 4217, 5, 0 }, { 3163, 20, 0 } },
      { { 3, 266, 0.6f }, { 3, 2974, 0.6f }, { 4, 1913, -0.6f }, { 5, 1996, 0.6f }, { 0, 1990, -0.6f }, { 1, 187, -0.6f }, { 2, 1066, -0.6f } },
      { { 0, 353, 0.6f }, { 0, 3627, 0.6f }, { 1, 1218, -0.6f }, { 2, 2673, 0.6f }, { 3, 2111, -0.6f }, { 4, 335, -0.6f }, { 5, 121, -0.6f } },
      { { 11000, 1, -20 }, { 400, 0.7f, -4 } } },

    { "hall", 2, 1, 1, 1, 1, { 1, 6, 6, 4 }, { 0.8f, 1 }, { 1, 0 }, { 0, 1 },
      6, { { 762, 0.4f }, { 1780, 0.26f }, { 2270, -0.18f }, { 2822, 0.12f }, { 3730, -0.03f }, { 4826, 0.01f } },
      6, { { 168, 0.5f }, { 204, 0.5f }, { 132, 0.5f }, { 167, 0.5f }, { 199, 0.5f }, { 238, 0.5f } },
      { 3.5f, 4.3f },
      { { 580, 0.5f }, { 1630, 0.5f } }, { { 800, 30, 0 }, { 80, 20, 30 } },
      { { 640, 0.5f }, { 1680, 0.5f } }, { { 30, 40, 0 }, { 830, 30, 20 } },
      { { 0, 50, -0.6f }, { 1, 20, 0.2f }, { 2, 100, -0.8f }, { 3, 340, -0.7f }, { 4, 100, 0.5f }, { 3, 1800, 0.3f }, { 0, 0, 0 } },
      { { 0, 680, 0.6f }, { 4, 2600, -0.2f }, { 5, 2640, 0.8f }, { 0, 230, 0.6f }, { 3, 880, -0.5f }, { 0, 2200, -0.16f }, { 5, 0, 0 } },
      { { 340, 0.82f, -3.4f }, { 14160, 0.54f, -5.2f } } },

    { "sparkles", 1, 0, 1, 1, 0, { 4, 6, 6, 6 }, { 0.5f, 1 }, { 1, 0 }, { 0.4f, 0.6f },
      6, { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
      3, { { 2, 0.46f }, { 6, 0.46f }, { 10, 0.46f } },
      { 0.2f, 2.2f },
      { { 3580, 0.5f }, { 1630, 0.5f } }, { { 2800, 30, 0 }, { 180, 30, 50 } },
      { { 5640, 0.5f }, { 3680, 0.5f } }, { { 2830, 40, 0 }, { 80, 50, 20 } },
      { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
      { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
      { { 12000, 0.8f, 2.4f }, { 300, 0.74f, -1.6f } } },

    { "ambience", 1, 1, 1, 1, 1, { 2, 2, 3, 4 }, { 0.9f, 1.9f }, { 1, 0 }, { 0, 1 },
      3, { { 298, 0.3f }, { 526, -0.2f }, { 597, -0.1f } },
      3, { { 168, 0.4f }, { 204, 0.4f }, { 332, 0.4f } },
      { 1.6f, 1.7f },
      { { 640, 0.5f }, { 830, 0.5f } }, { { 340, 30, 0 }, { 800, 30, 30 } },
      { { 660, 0.5f }, { 980, 0.5f } }, { { 840, 20, 0 }, { 800, 50, 50 } },
      { { 0, 580, 0.8f }, { 1, 980, -0.7f }, { 2, 1536, 0.4f }, { 3, 2600, -0.2f }, { 4, 3040, -0.1f }, { 5, 3820, 0.1f }, { 0, 0, 0 } },
      { { 3, 632, -0.74f }, { 4, 920, 0.7f }, { 5, 1154, 0.4f }, { 0, 2530, 0.2f }, { 1, 3140, -0.1f }, { 2, 3260, 0.1f }, { 0, 0, 0 } },
      { { 475, 0.92f, -10 }, { 175, 0.96f, -14 } } },

    { "dmension", 2, 1, 0, 1, 1, { 0, 0, 0, 0 }, { 0.04f, 1.8f }, { 1, 0 }, { 0, 0.08f },
      6, { { 162, 0.5f }, { 380, -0.56f }, { 1270, 0.48f }, { 2022, -0.34f }, { 2230, 0.2f }, { 2826, -0.16f } },
      5, { { 16.8f, 0.1f }, { 20.4f, 0.2f }, { 43.2f, 0.3f }, { 16.7f, 0.2f }, { 29.9f, 0.1f } },
      { 1, 1 },
      { { 3, 0.1f }, { 3, 0.1f } }, { { 10, 0, 0 }, { 10, 0, 0 } },
      { { 3, 0.1f }, { 3, 0.1f } }, { { 10, 0, 0 }, { 10, 0, 0 } },
      { { 0, 10, 0.8f }, { 1, 165, 0.6f }, { 2, 320, 0.5f }, { 0, 400, -0.3f }, { 1, 520, 0.7f }, { 2, 600, 0.5f }, { 0, 720, 0.8f } },
      { { 3, 10, 0.8f }, { 4, 160, 0.6f }, { 5, 300, -0.5f }, { 3, 420, 0.6f }, { 4, 500, -0.3f }, { 5, 620, -0.8f }, { 3, 700, -0.4f } },
      { { 340, 0.9f, -2 }, { 600, 0.7f, 2 } } },

    { "aura", 2, 0, 0, 1, 1, { 3, 3, 3, 3 }, { 0.03f, 0.9f }, { 1, 0 }, { 0.1f, 0.6f },
      3, { { 62, 0.2f }, { 186, -0.16f }, { 558, 0.1f } },
      4, { { 4.8f, 0.3f }, { 20.4f, 0.2f }, { 30.4f, 0.5f }, { 90.9f, 0.4f } },
      { 27.2f, 27.7f },
      { { 128, 0.5f }, { 44, 0.5f } }, { { 12, 30, 0 }, { 14, 30, 30 } },
      { { 108, 0.5f }, { 24, 0.5f } }, { { 12, 20, 0 }, { 14, 50, 50 } },
      { { 0, 3, 0.8f }, { 1, 95, 0.5f }, { 2, 120, -0.4f }, { 0, 575, 0.5f }, { 1, 302, -0.3f }, { 0, 0, 0 }, { 0, 0, 0 } },
      { { 3, 6, 0.8f }, { 4, 90, -0.5f }, { 5, 120, -0.4f }, { 3, 565, 0.5f }, { 4, 290, -0.3f }, { 0, 0, 0 }, { 0, 0, 0 } },
      { { 80, 0.76f, 8 }, { 20, 0.88f, -16 } } },
};

/** Tank feedback gain for a decay knob position (0..1). */
inline float tankFeedback (int mode, float decay)
{
    const auto& m = kModes[std::clamp (mode, 0, 6)];
    return m.decTail[0] + decayCurve (decay) * m.decTail[1];
}

/** Low-pass cutoff of the in-tank damping for a damp knob position (0..1). */
inline float dampHz (float damp) { return noteHz ((1.0f - dampCurve (damp)) * 119.0f); }

/** Color: the low-pass and high-pass cutoffs of the tilt filter (0..1, 0.5 = neutral). */
inline float colorLowpassHz (float color)  { return color < 0.49f ? noteHz (color * 238.0f) : 20000.0f; }
inline float colorHighpassHz (float color) { return color < 0.51f ? noteHz (23.8f) : noteHz ((color - 0.5f) * 190.4f + 23.8f); }

/** Length of one trip round the figure-of-eight tank, in seconds (sample-rate independent). */
inline double tankLoopSeconds (int mode)
{
    const auto& m = kModes[std::clamp (mode, 0, 6)];
    const double ap = (double) m.trr * (m.apL[0].delay + m.apL[1].delay + m.apR[0].delay + m.apR[1].delay);
    const double wg = m.inside[0] * (m.wgL[0].delay + m.wgR[0].delay) + m.inside[1] * (m.wgL[1].delay + m.wgR[1].delay);
    return (ap + wg) / kRefRate;
}
} // namespace lv::spire
