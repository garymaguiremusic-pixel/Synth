#pragma once

// Grid of the measured RT60 table (SpireRtTable.h): Decay and Damp 0..1, in steps of
// 0.05 and 0.1.

namespace lv::spire
{
constexpr int kRtDecaySteps = 21, kRtDampSteps = 11;
inline constexpr float kRtDampGrid[kRtDampSteps] = { 0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f };
inline float rtGridDecay (int i) { return (float) i / (float) (kRtDecaySteps - 1); }
inline float rtGridDamp (int j)  { return kRtDampGrid[j]; }
} // namespace lv::spire
