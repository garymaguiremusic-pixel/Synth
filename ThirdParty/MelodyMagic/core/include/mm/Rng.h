#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>

namespace mm
{
    /** Injectable random source. Derived classes supply nextInt(); the helpers match
        juce::Random bit for bit when nextInt() does. */
    class Rng
    {
    public:
        virtual ~Rng() = default;
        virtual int nextInt() noexcept = 0;

        /** 0 .. maxValue-1 (juce::Random::nextInt (int)). */
        int nextInt (int maxValue) noexcept
        {
            if (maxValue <= 0)
                return 0;
            return (int) (((std::uint64_t) (std::uint32_t) nextInt() * (std::uint64_t) maxValue) >> 32);
        }

        /** lo .. hi inclusive. */
        int nextRange (int lo, int hi) noexcept   { return hi <= lo ? lo : lo + nextInt (hi - lo + 1); }

        /** [0, 1): (uint32) nextInt / 2^32, like juce::Random::nextFloat. */
        float nextFloat() noexcept
        {
            const auto result = (float) (std::uint32_t) nextInt() / ((float) std::numeric_limits<std::uint32_t>::max() + 1.0f);
            return std::min (result, 1.0f - std::numeric_limits<float>::epsilon());
        }

        double nextDouble() noexcept
        {
            return (double) (std::uint32_t) nextInt() / ((double) std::numeric_limits<std::uint32_t>::max() + 1.0);
        }

        /** juce::Random::nextBool: bit 30 of the next int. */
        bool nextBool() noexcept   { return (nextInt() & 0x40000000) != 0; }

        /** True with the given probability (0..1). */
        bool chance (double probability) noexcept
        {
            if (probability <= 0.0) return false;
            if (probability >= 1.0) return true;
            return nextDouble() < probability;
        }
    };

    /** The 48-bit LCG used by juce::Random: seed = (seed * 0x5DEECE66D + 11) mod 2^48. */
    class JuceLcg final : public Rng
    {
    public:
        explicit JuceLcg (std::int64_t initialSeed) noexcept : seed (initialSeed) {}

        /** Seeded from the clock (like juce::Random's default constructor). */
        JuceLcg() noexcept : seed (timeSeed()) {}

        void setSeed (std::int64_t s) noexcept   { seed = s; }
        std::int64_t getSeed() const noexcept    { return seed; }

        int nextInt() noexcept override
        {
            seed = (std::int64_t) (((std::uint64_t) seed * 0x5deece66dULL + 11ULL) & 0xffffffffffffULL);
            return (int) (seed >> 16);
        }

        static std::int64_t timeSeed() noexcept
        {
            const auto t = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            return (std::int64_t) ((std::uint64_t) t ^ 0x2545F4914F6CDD1DULL);
        }

    private:
        std::int64_t seed;
    };
}
