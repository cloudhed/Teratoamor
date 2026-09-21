#pragma once

#include <cstdint>

// Deterministic white-noise generator (xorshift64*). Real-time safe: no allocation, no locks.
// Every voice/Element seeds its own instance so the streams are independent and repeatable.
class NoiseSource
{
public:
    void seed (std::uint64_t seedValue) noexcept
    {
        // splitmix64 spreads nearby seeds (0, 1, 2...) into unrelated states.
        std::uint64_t z = seedValue + 0x9E3779B97F4A7C15ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        state = z != 0 ? z : 0x1ull;
    }

    // Uniform white noise in [-1, 1), variance 1/3.
    float next() noexcept
    {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        const auto r = state * 0x2545F4914F6CDD1Dull;
        return static_cast<float> (static_cast<std::int64_t> (r) >> 40) / 8388608.0f;
    }

private:
    std::uint64_t state = 1;
};
