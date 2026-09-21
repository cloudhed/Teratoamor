// Standalone checks for the DSP engine. No JUCE and no audio device needed.
#include "DSP/Engine.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

namespace
{
    int failures = 0;

    void check (bool ok, const char* what)
    {
        std::printf ("[%s] %s\n", ok ? " ok " : "FAIL", what);
        if (! ok) ++failures;
    }

    EngineParams defaultParams()
    {
        EngineParams p;
        p.elements[0].enabled = true;   // reference default patch: Element 1 only, Width 90
        return p;
    }

    struct Capture { std::vector<float> l, r; };

    // Renders in irregular block sizes to exercise chunk handling.
    void renderBlocks (Engine& e, Capture& c, int total)
    {
        const int sizes[] = { 64, 37, 512, 1, 128, 333 };
        int done = 0, i = 0;
        while (done < total)
        {
            const int n = std::min (sizes[i++ % 6], total - done);
            c.l.resize ((size_t) (done + n));
            c.r.resize ((size_t) (done + n));
            e.render (c.l.data() + done, c.r.data() + done, n);
            done += n;
        }
    }

    bool allFinite (const Capture& c, float limit = 1.0f)
    {
        for (size_t i = 0; i < c.l.size(); ++i)
            if (! (std::abs (c.l[i]) <= limit && std::abs (c.r[i]) <= limit))
                return false;
        return true;
    }

    double rms (const std::vector<float>& v, size_t from, size_t to)
    {
        double s = 0;
        for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i];
        return std::sqrt (s / (double) (to - from));
    }

    // Strongest frequency in [lo, hi] using a Hann-windowed Goertzel scan.
    double peakFrequency (const std::vector<float>& x, double sr, double lo, double hi, double step)
    {
        const size_t n = x.size();
        double best = 0, bestPower = -1;
        for (double f = lo; f <= hi; f += step)
        {
            const double w = 2.0 * 3.14159265358979 * f / sr;
            double re = 0, im = 0;
            for (size_t i = 0; i < n; ++i)
            {
                const double win = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * (double) i / (double) (n - 1));
                re += x[i] * win * std::cos (w * (double) i);
                im += x[i] * win * std::sin (w * (double) i);
            }
            const double power = re * re + im * im;
            if (power > bestPower) { bestPower = power; best = f; }
        }
        return best;
    }

    // Averaged power spectrum (Hann window, 50% overlap). Bin width = sr / segment.
    struct Spectrum
    {
        std::vector<double> power;
        double binHz = 1.0;

        double mean (double lo, double hi) const
        {
            double s = 0; int n = 0;
            for (size_t i = (size_t) (lo / binHz); i <= (size_t) (hi / binHz); ++i) { s += power[i]; ++n; }
            return s / n;
        }

        // Strongest 5-bin average inside [lo, hi].
        double peak (double lo, double hi) const
        {
            double best = 0;
            for (size_t i = (size_t) (lo / binHz) + 2; i + 2 <= (size_t) (hi / binHz); ++i)
                best = std::max (best, (power[i - 2] + power[i - 1] + power[i] + power[i + 1] + power[i + 2]) / 5.0);
            return best;
        }
    };

    void fft (std::vector<std::complex<double>>& a)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap (a[i], a[j]);
        }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const std::complex<double> wl = std::polar (1.0, -2.0 * 3.14159265358979 / (double) len);
            for (size_t i = 0; i < n; i += len)
            {
                std::complex<double> w = 1.0;
                for (size_t k = 0; k < len / 2; ++k, w *= wl)
                {
                    const auto u = a[i + k], v = a[i + k + len / 2] * w;
                    a[i + k] = u + v;
                    a[i + k + len / 2] = u - v;
                }
            }
        }
    }

    Spectrum spectrum (const std::vector<float>& x, double sr, size_t from, size_t segment = 65536)
    {
        Spectrum s;
        s.binHz = sr / (double) segment;
        s.power.assign (segment / 2 + 1, 0.0);
        int count = 0;
        for (size_t start = from; start + segment <= x.size(); start += segment / 2, ++count)
        {
            std::vector<std::complex<double>> a (segment);
            for (size_t i = 0; i < segment; ++i)
                a[i] = x[start + i] * (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * (double) i / (double) segment));
            fft (a);
            for (size_t i = 0; i < s.power.size(); ++i) s.power[i] += std::norm (a[i]);
        }
        for (auto& p : s.power) p /= std::max (count, 1);
        return s;
    }

    double db (double powerRatio) { return 10.0 * std::log10 (powerRatio); }

    // One held middle C on Element 1 in the given filter mode.
    Capture playMode (FilterMode mode, double sr = 48000.0, int seconds = 5)
    {
        Engine e; e.prepare (sr);
        auto p = defaultParams();
        p.elements[0].filterMode = mode;
        e.setParams (p);
        e.noteOn (60, 1.0f);
        Capture c; renderBlocks (e, c, (int) sr * seconds);
        return c;
    }
}

int main()
{
    // 1. Silence with no notes.
    {
        Engine e; e.prepare (48000.0); e.setParams (defaultParams());
        Capture c; renderBlocks (e, c, 4800);
        check (rms (c.l, 0, c.l.size()) == 0.0, "silent when no note is held");
    }

    // 2. Note 60 sounds at middle C, identical in both channels at centre pan.
    {
        Engine e; e.prepare (48000.0); e.setParams (defaultParams());
        e.noteOn (60, 1.0f);
        Capture c; renderBlocks (e, c, 96000);
        check (allFinite (c), "note output finite and within +/-1");
        const double level = rms (c.l, 48000, 96000);
        std::printf ("       RMS level %.4f\n", level);
        check (level > 0.005 && level < 0.5, "note has sensible level");
        check (c.l == c.r, "centre-panned Element is identical in L and R");
        std::vector<float> tail (c.l.begin() + 24000, c.l.end());
        const double f = peakFrequency (tail, 48000.0, 250.0, 275.0, 0.1);
        std::printf ("       measured pitch %.2f Hz (target 261.63)\n", f);
        check (std::abs (f - 261.63) < 1.0, "note 60 is tuned to middle C");
    }

    // 3. Octave / semitone offsets shift pitch.
    {
        Engine e; e.prepare (48000.0);
        auto p = defaultParams(); p.elements[0].octave = 1; p.elements[0].semitone = 2;
        e.setParams (p); e.noteOn (60, 1.0f);
        Capture c; renderBlocks (e, c, 96000);
        std::vector<float> tail (c.l.begin() + 24000, c.l.end());
        const double target = 261.6256 * std::pow (2.0, 14.0 / 12.0);
        const double f = peakFrequency (tail, 48000.0, 560.0, 600.0, 0.1);
        std::printf ("       measured pitch %.2f Hz (target %.2f)\n", f, target);
        check (std::abs (f - target) < 1.5, "octave + semitone tuning");
    }

    // 4. Stability: Width 0..100, every pitch, several sample rates, all Elements, fast param changes.
    {
        bool ok = true;
        for (double sr : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
            for (float width : { 0.0f, 50.0f, 90.0f, 99.0f, 100.0f })
            {
                Engine e; e.prepare (sr);
                auto p = defaultParams();
                for (auto& el : p.elements) { el.enabled = true; el.width = width; el.level = 100.0f; }
                p.master = 100.0f;
                e.setParams (p);
                for (int note = 0; note <= 127; note += 3) e.noteOn (note, 1.0f);
                for (int block = 0; block < 20; ++block)
                {
                    p.elements[0].width = (block % 2) ? 100.0f : 20.0f;   // rapid Width swings
                    p.elements[1].semitone = (block % 5) - 2;
                    e.setParams (p);
                    Capture part; renderBlocks (e, part, (int) (sr / 20));
                    ok = ok && allFinite (part);
                }
            }
        check (ok, "finite bounded output across Width, pitch, sample rate, rapid changes");
    }

    // 5. Voice stealing: more notes than voices never breaks the output.
    {
        Engine e; e.prepare (48000.0); e.setParams (defaultParams());
        for (int n = 40; n < 60; ++n) e.noteOn (n, 0.8f);
        Capture c; renderBlocks (e, c, 24000);
        check (allFinite (c) && rms (c.l, 12000, 24000) > 0.001, "voice stealing with 20 overlapping notes");
    }

    // 6. Release fades to silence (default Release 5 = ~0.5 s), and all voices free up.
    {
        Engine e; e.prepare (48000.0); e.setParams (defaultParams());
        e.noteOn (60, 1.0f);
        Capture c; renderBlocks (e, c, 24000);
        e.noteOff (60);
        Capture d; renderBlocks (e, d, 48000);
        check (rms (d.l, 0, 2400) > rms (d.l, 20000, 22400), "release decays");
        check (rms (d.l, 30000, 48000) == 0.0, "silent after release finishes");
    }

    // 7. Slow attack: output ramps up.
    {
        Engine e; e.prepare (48000.0);
        auto p = defaultParams(); p.elements[0].attack = 20.0f;   // 2 seconds
        e.setParams (p); e.noteOn (60, 1.0f);
        Capture c; renderBlocks (e, c, 48000 * 3);
        const double early = rms (c.l, 4800, 9600);
        const double mid   = rms (c.l, 48000, 52800);
        const double late  = rms (c.l, 48000 * 2 + 4800, 48000 * 2 + 9600);
        check (early < mid && mid < late, "attack ramps up gradually");
    }

    // 8. Determinism: same input -> same output.
    {
        Capture a, b;
        for (Capture* c : { &a, &b })
        {
            Engine e; e.prepare (48000.0); e.setParams (defaultParams());
            e.noteOn (64, 0.7f); e.noteOn (67, 0.7f);
            renderBlocks (e, *c, 10000);
        }
        check (a.l == b.l && a.r == b.r, "output is deterministic");
    }

    // 9. Hard pan sends an Element to one side only.
    {
        Engine e; e.prepare (48000.0);
        auto p = defaultParams(); p.elements[0].pan = -100.0f;
        e.setParams (p); e.noteOn (60, 1.0f);
        Capture c; renderBlocks (e, c, 24000);
        check (rms (c.l, 12000, 24000) > 0.01 && rms (c.r, 12000, 24000) < 1.0e-4, "hard-left pan");
    }

    // 10. Sustain pedal keeps a released note sounding until the pedal is lifted.
    {
        Engine e; e.prepare (48000.0); e.setParams (defaultParams());
        e.setSustainPedal (true);
        e.noteOn (60, 1.0f);
        Capture c; renderBlocks (e, c, 12000);
        e.noteOff (60);
        Capture d; renderBlocks (e, d, 48000);
        check (rms (d.l, 40000, 48000) > 0.01, "note keeps sounding while pedal is down");
        e.setSustainPedal (false);
        Capture f; renderBlocks (e, f, 48000);
        check (rms (f.l, 40000, 48000) == 0.0, "note releases when pedal is lifted");
    }

    // 11. Pitch bend: full up bends 2 semitones.
    {
        Engine e; e.prepare (48000.0);
        e.setPitchBend (1.0f); e.setParams (defaultParams());
        e.noteOn (60, 1.0f);
        Capture c; renderBlocks (e, c, 96000);
        std::vector<float> tail (c.l.begin() + 24000, c.l.end());
        const double target = 261.6256 * std::pow (2.0, 2.0 / 12.0);
        const double f = peakFrequency (tail, 48000.0, 285.0, 300.0, 0.1);
        std::printf ("       measured pitch %.2f Hz (target %.2f)\n", f, target);
        check (std::abs (f - target) < 1.5, "pitch bend up 2 semitones");
    }

    // 12. Filter modes (Milestone 2). Spectra are compared with the reference-recording findings.
    {
        const double sr = 48000.0;
        const auto wide   = playMode (FilterMode::bpWide);
        const auto bypass = playMode (FilterMode::bypass);
        const auto bpNar  = playMode (FilterMode::bpNarrow);
        const auto pkWide = playMode (FilterMode::peakWide);
        const auto pkNar  = playMode (FilterMode::peakNarrow);
        const size_t from = 96000;

        // Off: silent, and the voice frees up so a later note still works.
        {
            Engine e; e.prepare (sr);
            auto p = defaultParams(); p.elements[0].filterMode = FilterMode::off;
            e.setParams (p); e.noteOn (60, 1.0f);
            Capture c; renderBlocks (e, c, 24000);
            check (rms (c.l, 0, c.l.size()) == 0.0, "filter Off is silent");
            e.noteOff (60);
            p.elements[0].filterMode = FilterMode::bpWide; e.setParams (p);
            Capture d; renderBlocks (e, d, 48000);
            e.noteOn (62, 1.0f);
            Capture f; renderBlocks (e, f, 24000);
            check (rms (f.l, 12000, 24000) > 0.005, "engine still plays after an Off note");
        }

        // Every audible mode lands within a few dB of the BP Wide level.
        const double wideRms = rms (wide.l, from, wide.l.size());
        bool levelsOk = true;
        for (const auto* c : { &bypass, &bpNar, &pkWide, &pkNar })
        {
            const double d = 20.0 * std::log10 (rms (c->l, from, c->l.size()) / wideRms);
            std::printf ("       level vs BP Wide: %.2f dB\n", d);
            levelsOk = levelsOk && std::abs (d) < 3.0;
        }
        check (levelsOk, "filter modes are level-matched within 3 dB");

        // Bypass is unfiltered noise: flat spectrum, nothing tonal.
        {
            const auto sp = spectrum (bypass.l, sr, from);
            check (std::abs (db (sp.mean (8000.0, 16000.0) / sp.mean (200.0, 400.0))) < 3.0, "Bypass spectrum is flat (white)");
        }

        const auto spWide = spectrum (wide.l, sr, from);
        const auto spNar  = spectrum (bpNar.l, sr, from);
        const auto spPkW  = spectrum (pkWide.l, sr, from);
        const auto spPkN  = spectrum (pkNar.l, sr, from);

        auto skirt = [] (const Spectrum& s) { return db (s.mean (4000.0, 8000.0) / s.peak (255.0, 268.0)); };
        std::printf ("       level 4-8 kHz vs peak (dB): BP Wide %.1f, BP Narrow %.1f, Peak Wide %.1f, Peak Narrow %.1f\n",
                     skirt (spWide), skirt (spNar), skirt (spPkW), skirt (spPkN));

        check (skirt (spNar) < skirt (spWide) - 15.0, "BP Narrow has much steeper skirts than BP Wide");
        check (skirt (spPkW) > skirt (spWide) + 10.0, "Peak Wide keeps far more broadband noise than BP Wide");
        check (skirt (spPkW) > -52.0 && skirt (spPkW) < -40.0, "Peak Wide noise floor near -46 dB (reference)");
        check (skirt (spPkN) < skirt (spPkW) - 20.0, "Peak Narrow is dominated by the fundamental");

        const std::vector<float> narTail (bpNar.l.begin() + 24000, bpNar.l.end());
        const double pitchNar = peakFrequency (narTail, sr, 255.0, 268.0, 0.1);
        std::printf ("       BP Narrow pitch %.2f Hz\n", pitchNar);
        check (std::abs (pitchNar - 261.63) < 1.5, "BP Narrow stays tuned to the note");
    }

    // 13. Stability of every filter mode across Width, pitch, sample rate, and live mode switching.
    {
        bool ok = true;
        for (double sr : { 22050.0, 44100.0, 96000.0, 192000.0 })
            for (float width : { 0.0f, 50.0f, 99.0f, 100.0f })
            {
                Engine e; e.prepare (sr);
                auto p = defaultParams();
                for (auto& el : p.elements) { el.enabled = true; el.width = width; el.level = 100.0f; }
                p.master = 100.0f;
                e.setParams (p);
                for (int note = 0; note <= 127; note += 5) e.noteOn (note, 1.0f);
                for (int block = 0; block < 24; ++block)
                {
                    for (int el = 0; el < 3; ++el)
                        p.elements[(size_t) el].filterMode = static_cast<FilterMode> ((block + el) % 6);   // switch every block
                    e.setParams (p);
                    Capture part; renderBlocks (e, part, (int) (sr / 20));
                    ok = ok && allFinite (part);
                }
            }
        check (ok, "all filter modes finite and bounded across Width, pitch, sample rate, live switching");
    }

    std::printf ("\n%s\n", failures == 0 ? "All engine tests passed." : "Engine tests FAILED.");
    return failures == 0 ? 0 : 1;
}
