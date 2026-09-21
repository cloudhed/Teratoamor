#pragma once
#include <array>
#include <algorithm>
#include <cmath>

// Quarter-note beats. Choice indices are host state: append rather than reorder.
namespace DelayRates
{
    struct Rate { const char* name; double beats; };
    inline constexpr std::array<Rate, 26> values {{
        { "1/64T", 1.0/24 }, { "1/64", 1.0/16 }, { "1/32T", 1.0/12 },
        { "1/64D", 3.0/32 }, { "1/32", 1.0/8 }, { "1/16T", 1.0/6 },
        { "1/32D", 3.0/16 }, { "1/16", 1.0/4 }, { "1/8T", 1.0/3 },
        { "1/16D", 3.0/8 }, { "1/8", 1.0/2 }, { "1/4T", 2.0/3 },
        { "1/8D", 3.0/4 }, { "1/4", 1.0 }, { "1/2T", 4.0/3 },
        { "1/4D", 1.5 }, { "1/2", 2.0 }, { "1/1T", 8.0/3 },
        { "1/2D", 3.0 }, { "1/1", 4.0 }, { "2/1T", 16.0/3 },
        { "1/1D", 6.0 }, { "2/1", 8.0 }, { "4/1T", 32.0/3 },
        { "2/1D", 12.0 }, { "4/1", 16.0 }
    }};
    inline constexpr double minimumBpm = 20.0, maximumBpm = 400.0;
    inline constexpr double maximumSeconds = 48.0; // 4/1 at 20 BPM
    inline constexpr int defaultRate = 13;
    inline double validBpm (double bpm) noexcept
    {
        return std::isfinite (bpm) && bpm > 0.0 ? std::clamp (bpm, minimumBpm, maximumBpm) : 120.0;
    }
    inline double seconds (int rate, double bpm) noexcept
    {
        return values[static_cast<size_t> (std::clamp (rate, 0, static_cast<int> (values.size()) - 1))].beats * 60.0 / validBpm (bpm);
    }
}
