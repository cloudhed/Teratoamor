#include "DSP/ParallelDelay.h"
#include "DSP/Engine.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <vector>

namespace { bool counting = false; size_t allocations = 0; int failures = 0; }
void* operator new (std::size_t n)
{
    if (counting) ++allocations;
    if (void* p = std::malloc (n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void check (bool ok, const char* text)
{
    std::printf ("[%s] %s\n", ok ? " ok " : "FAIL", text);
    if (!ok) ++failures;
}
bool close (float a, float b) { return std::abs (a - b) < 1.0e-5f; }
struct Audio { std::vector<float> l, r; explicit Audio (int n) : l ((size_t)n), r ((size_t)n) {} };
void process (ParallelDelay& delay, Audio& audio, int block = 257)
{
    for (int i = 0; i < (int)audio.l.size(); i += block)
        delay.process (audio.l.data()+i, audio.r.data()+i, std::min (block, (int)audio.l.size()-i));
}
DelayParams wet()
{
    DelayParams p; p.mix = 100; p.lines[0].enabled = true; p.lines[0].decay = 0;
    return p;
}
int main()
{
    ParallelDelay d; d.prepare (48000);
    auto p = wet(); p.lines[0].decay = 50; d.setParameters (p);
    Audio impulse (72001); impulse.l[0] = impulse.r[0] = 0.2f; process (d, impulse);
    check (impulse.l[0] == 0 && close (impulse.l[24000], 0.2f)
        && close (impulse.l[48000], 0.095f) && close (impulse.l[72000], 0.045125f)
        && impulse.l == impulse.r, "quarter-note echoes at 120 BPM, geometric Decay, centre is mono");

    p = wet(); p.lines[0].pan = -100;
    p.lines[1] = { true, 10, 0, 100 }; // eighth note, right
    d.reset(); d.setParameters (p);
    Audio parallel (48001); parallel.l[0] = parallel.r[0] = 0.2f; process (d, parallel);
    check (close (parallel.r[12000], 0.141421356f) && parallel.l[12000] == 0
        && close (parallel.l[24000], 0.141421356f) && parallel.r[24000] == 0
        && parallel.l[36000] == 0 && parallel.r[36000] == 0,
        "independent parallel delays, hard left/right, Decay zero gives one echo without serial repeats");
    p.lines[0].enabled = false;
    d.reset(); d.setParameters (p);
    Audio secondOnly (24001); secondOnly.l[0] = secondOnly.r[0] = 0.2f; process (d, secondOnly);
    check (close (secondOnly.r[12000], 0.282842712f) && secondOnly.l[12000] == 0,
        "Delay 2 operates independently with Delay 1 off");

    bool timing = true;
    for (double bpm : { 60.0, 120.0, 240.0 })
        for (int rate = 0; rate < (int) DelayRates::values.size(); ++rate)
        {
            p = wet(); p.bpm = bpm; p.lines[0].rate = rate;
            d.reset(); d.setParameters (p);
            const double at = DelayRates::seconds (rate, bpm) * 48000;
            const int first = (int) std::floor (at);
            std::array<float, 257> l {}, r {};
            double sum = 0.0;
            for (int start = 0; start < (int)std::ceil (at) + 2; start += 257)
            {
                l.fill (0); r.fill (0); if (start == 0) l[0] = r[0] = 0.2f;
                const int n = std::min (257, (int)std::ceil (at) + 2 - start);
                d.process (l.data(), r.data(), n);
                for (int i = 0; i < n; ++i)
                {
                    sum += l[(size_t)i];
                    if (start + i < first) timing = timing && l[(size_t)i] == 0;
                }
            }
            timing = timing && std::abs (sum - 0.2) < 1.0e-5;
        }
    check (timing, "all 26 straight/triplet/dotted rates at three tempos have correctly timed fractional echoes");
    check (DelayRates::seconds (0, 120) == 1.0/48 && DelayRates::seconds (25, 20) == 48
        && DelayRates::validBpm (0) == 120 && DelayRates::validBpm (std::numeric_limits<double>::quiet_NaN()) == 120,
        "rate endpoints and missing/invalid tempo fallback");

    auto toneLevel = [&d] (float cut, float frequency)
    {
        auto params = wet(); params.cut = cut; params.lines[0].rate = 0;
        d.reset(); d.setParameters (params);
        Audio a (48000);
        for (size_t i = 0; i < a.l.size(); ++i)
            a.l[i] = a.r[i] = 0.1f * (float)std::sin (6.28318530717959 * frequency * (double)i / 48000);
        process (d, a);
        double sum = 0; for (int i = 24000; i < 48000; ++i) sum += a.l[(size_t)i] * a.l[(size_t)i];
        return std::sqrt (sum / 24000);
    };
    const double neutralLow = toneLevel (50, 100), neutralHigh = toneLevel (50, 6000);
    check (toneLevel (0, 6000) < neutralHigh * 0.02 && toneLevel (0, 100) > neutralLow * 0.9,
        "Cut 0 removes highs from echoes and keeps lows");
    check (toneLevel (100, 100) < neutralLow * 0.01 && toneLevel (100, 6000) > neutralHigh * 0.8,
        "Cut 100 removes lows from echoes and keeps highs");
    check (std::abs (neutralLow - std::sqrt (0.005)) < 0.0001 && std::abs (neutralHigh - neutralLow) < 0.0001,
        "Cut 50 leaves wet signal unfiltered");
    bool dryUnchanged = true;
    for (float cut : { 0.0f, 50.0f, 100.0f })
    {
        p = wet(); p.mix = 0; p.cut = cut;
        d.reset(); d.setParameters (p);
        Audio a (48000);
        for (size_t i = 0; i < a.l.size(); ++i) { a.l[i] = 0.17f; a.r[i] = -0.06f; }
        process (d, a);
        for (size_t i = 0; i < a.l.size(); ++i) dryUnchanged &= a.l[i] == 0.17f && a.r[i] == -0.06f;
        p.mix = 100; p.lines[0].enabled = false;
        d.reset(); d.setParameters (p); process (d, a);
        for (size_t i = 0; i < a.l.size(); ++i) dryUnchanged &= a.l[i] == 0.17f && a.r[i] == -0.06f;
    }
    check (dryUnchanged, "Mix zero and both delays off preserve dry stereo exactly at every Cut setting");
    p = wet(); p.mix = 50; d.reset(); d.setParameters (p);
    Audio half (24001); half.l[0] = 0.2f; half.r[0] = 0.1f; process (d, half);
    check (close (half.l[0], 0.1f) && close (half.r[0], 0.05f)
        && close (half.l[24000], 0.075f) && close (half.r[24000], 0.075f),
        "Mix 50 blends unchanged stereo dry with mono wet equally");

    p = wet(); p.lines[0].rate = 0; d.reset(); d.setParameters (p);
    Audio warm (12000); std::fill (warm.l.begin(), warm.l.end(), 0.2f); warm.r = warm.l; process (d, warm);
    p.lines[0].rate = 25; p.bpm = 93; d.setParameters (p);
    Audio changed (2000); std::fill (changed.l.begin(), changed.l.end(), 0.2f); changed.r = changed.l; process (d, changed);
    float jump = 0, previous = 0.2f;
    for (float x : changed.l) { jump = std::max (jump, std::abs (x - previous)); previous = x; }
    check (jump < 0.001f, "large Rate and tempo changes crossfade without an abrupt output jump");
    p.lines[0].enabled = false; d.setParameters (p); Audio off (16000); process (d, off);
    p.lines[0].enabled = true; p.lines[0].rate = 0; d.setParameters (p);
    Audio reenabled (48000); process (d, reenabled);
    bool silence = true; for (float x : reenabled.l) silence &= x == 0;
    check (silence, "turning a delay off clears history and re-enabling never recalls old repeats");

    // Same automation timeline rendered with different chunking must be identical.
    auto automated = [] (int block)
    {
        ParallelDelay delay; delay.prepare (22050); auto params = wet(); params.lines[0].rate = 0;
        delay.setParameters (params); Audio a (9000);
        for (size_t i = 0; i < a.l.size(); ++i) a.l[i] = a.r[i] = 0.1f * std::sin ((float)i * 0.1f);
        for (int start = 0; start < 9000; start += 3000)
        {
            params.cut = (float)start / 60; params.lines[0].pan = (float)start / 30 - 100;
            params.lines[0].decay = (float)start / 60; params.lines[0].rate = start / 1000;
            delay.setParameters (params);
            for (int i = start; i < start + 3000; i += block)
                delay.process (a.l.data()+i, a.r.data()+i, std::min (block, start + 3000 - i));
        }
        return a;
    };
    const auto small = automated (1), large = automated (511);
    check (small.l == large.l && small.r == large.r, "delay automation is independent of host block size");

    bool stable = true, allocationFree = true;
    for (double sr : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        ParallelDelay delay; delay.prepare (sr); p = wet(); p.lines[1].enabled = true;
        std::array<float, 512> l {}, r {};
        allocations = 0; counting = true;
        for (int block = 0; block < 600; ++block)
        {
            p.cut = (float)(block % 101); p.bpm = (block % 2) ? 20 : 400;
            p.lines[0].rate = block % 26; p.lines[1].rate = (block + 7) % 26;
            p.lines[0].decay = p.lines[1].decay = 100;
            p.lines[0].enabled = (block % 13) != 0;
            delay.setParameters (p);
            for (size_t i = 0; i < l.size(); ++i) { l[i] = (i % 2) ? 4.0f : -4.0f; r[i] = l[i]; }
            if (block == 0) l[0] = std::numeric_limits<float>::quiet_NaN();
            delay.process (l.data(), r.data(), (int)l.size());
            for (size_t i = 0; i < l.size(); ++i)
                stable &= std::isfinite (l[i]) && std::isfinite (r[i]) && std::abs (l[i]) < 100;
        }
        delay.reset(); delay.setParameters (p); l.fill (0); r.fill (0);
        delay.process (l.data(), r.data(), (int)l.size());
        for (float x : l) stable &= x == 0;
        counting = false; allocationFree &= allocations == 0;
    }
    check (stable && allocationFree, "finite bounded output at maximum Decay, rapid changes, five sample rates; reset silent; no audio allocations");
    // Longest supported delay: no truncation at the 48-second buffer boundary.
    d.reset(); p = wet(); p.lines[0].rate = 25; p.bpm = 20; d.setParameters (p);
    Audio longest (48 * 48000 + 1); longest.l[0] = longest.r[0] = 0.2f; process (d, longest, 1024);
    check (close (longest.l.back(), 0.2f), "4/1 at 20 BPM fits the complete 48-second delay");
    // Verify wiring after the instrument's distortion and before master output.
    Engine engine; engine.prepare (48000); EngineParams ep; ep.elements[0].enabled = true;
    ep.delay = wet(); ep.delay.lines[0].pan = -100; engine.setParams (ep); engine.noteOn (60, 1);
    Audio engineAudio (25000); engine.render (engineAudio.l.data(), engineAudio.r.data(), 25000);
    double early = 0, late = 0, right = 0;
    for (size_t i = 0; i < engineAudio.l.size(); ++i)
    {
        if (i < 24000) early += std::abs (engineAudio.l[i]); else late += std::abs (engineAudio.l[i]);
        right += std::abs (engineAudio.r[i]);
    }
    check (early == 0 && late > 0 && right == 0, "engine routes delayed audio and applies delay pan");
    std::printf ("%s\n", failures ? "Delay tests FAILED" : "All delay tests passed");
    return failures ? 1 : 0;
}
