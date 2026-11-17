#pragma once

#include <cstdint>
#include <algorithm>
#include <cstring>
#include <vector>

namespace lv
{

//==============================================================================
/** Decoded audio: one vector per channel, plus the sample rate. Empty on failure. */
struct AudioData
{
    std::vector<std::vector<float>> channels;
    double sampleRate = 0.0;

    bool isValid() const noexcept { return ! channels.empty() && ! channels[0].empty() && sampleRate > 0.0; }
};

/**
    Minimal RIFF/WAVE decoder for impulse responses: 16, 24 or 32-bit PCM and
    32-bit float, including WAVE_FORMAT_EXTENSIBLE. Unknown chunks are skipped.
*/
inline AudioData readWav (const void* bytes, size_t size)
{
    AudioData result;
    const auto* d = static_cast<const uint8_t*> (bytes);
    auto u16 = [d] (size_t p) { return (uint32_t) d[p] | ((uint32_t) d[p + 1] << 8); };
    auto u32 = [u16] (size_t p) { return u16 (p) | (u16 (p + 2) << 16); };

    if (size < 12 || std::memcmp (d, "RIFF", 4) != 0 || std::memcmp (d + 8, "WAVE", 4) != 0)
        return result;

    uint32_t format = 0, channels = 0, rate = 0, bits = 0;
    size_t dataPos = 0, dataSize = 0;
    for (size_t p = 12; p + 8 <= size;)
    {
        const uint32_t chunkSize = u32 (p + 4);
        if (std::memcmp (d + p, "fmt ", 4) == 0 && chunkSize >= 16 && p + 24 <= size)
        {
            format = u16 (p + 8);
            channels = u16 (p + 10);
            rate = u32 (p + 12);
            bits = u16 (p + 22);
            if (format == 0xFFFE && chunkSize >= 40 && p + 34 <= size)
                format = u16 (p + 32);   // the sub-format's first two bytes hold the real format tag
        }
        else if (std::memcmp (d + p, "data", 4) == 0)
        {
            dataPos = p + 8;
            dataSize = std::min<size_t> (chunkSize, size - dataPos);
        }
        p += 8 + chunkSize + (chunkSize & 1);
    }

    const bool isFloat = format == 3 && bits == 32;
    const bool isPcm = format == 1 && (bits == 16 || bits == 24 || bits == 32);
    if (dataPos == 0 || channels == 0 || rate == 0 || ! (isFloat || isPcm))
        return result;

    const size_t bytesPerSample = bits / 8, frames = dataSize / (bytesPerSample * channels);
    result.channels.assign (channels, std::vector<float> (frames));
    result.sampleRate = rate;

    for (size_t f = 0; f < frames; ++f)
        for (uint32_t c = 0; c < channels; ++c)
        {
            const uint8_t* s = d + dataPos + (f * channels + c) * bytesPerSample;
            float v = 0.0f;
            if (isFloat)
                std::memcpy (&v, s, 4);
            else if (bits == 16)
                v = (float) (int16_t) (s[0] | (s[1] << 8)) / 32768.0f;
            else if (bits == 24)
                v = (float) ((int32_t) ((uint32_t) s[0] << 8 | (uint32_t) s[1] << 16 | (uint32_t) s[2] << 24) >> 8) / 8388608.0f;
            else
                v = (float) (int32_t) ((uint32_t) s[0] | (uint32_t) s[1] << 8 | (uint32_t) s[2] << 16 | (uint32_t) s[3] << 24) / 2147483648.0f;
            result.channels[c][f] = v;
        }
    return result;
}

} // namespace lv
