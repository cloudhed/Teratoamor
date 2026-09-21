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

    // 14. Warp and Clip (Milestone 2), compared with the reference recordings 03_warp_* and 04_clip_*.
    {
        const double sr = 48000.0;
        struct Result { double h2, h3, h4, rmsDb, crestDb, dc; };
        auto measure = [sr] (float warp, float clip)
        {
            Engine e; e.prepare (sr);
            auto p = defaultParams(); p.elements[0].warp = warp; p.elements[0].clip = clip;
            e.setParams (p); e.noteOn (60, 1.0f);
            Capture c; renderBlocks (e, c, (int) sr * 6);
            const auto sp = spectrum (c.l, sr, 48000);
            const double f1 = sp.peak (250.0, 275.0);
            Result r;
            r.h2 = db (sp.peak (508.0, 538.0) / f1);
            r.h3 = db (sp.peak (770.0, 800.0) / f1);
            r.h4 = db (sp.peak (1030.0, 1065.0) / f1);
            const double level = rms (c.l, 48000, c.l.size());
            r.rmsDb = 20.0 * std::log10 (level);
            double peak = 0, sum = 0;
            for (size_t i = 48000; i < c.l.size(); ++i) { peak = std::max (peak, (double) std::abs (c.l[i])); sum += c.l[i]; }
            r.crestDb = 20.0 * std::log10 (peak / level);
            r.dc = sum / (double) (c.l.size() - 48000) / level;
            return r;
        };

        const auto base = measure (0.0f, 0.0f);
        std::printf ("       reference warp 25/50/75/100 -> 2nd harmonic -18.7/-14.9/-10.7/-8.1 dB (Teratoamor is deliberately stronger)\n       Teratoamor:");
        double previous = -100.0;
        bool warpRises = true;
        Result w100 {};
        for (float w : { 25.0f, 50.0f, 75.0f, 100.0f })
        {
            const auto r = measure (w, 0.0f);
            std::printf (" %.1f", r.h2);
            warpRises = warpRises && r.h2 > previous;
            previous = r.h2;
            if (w == 100.0f) w100 = r;
        }
        std::printf (" dB\n       Warp 100: 3rd harmonic %.1f dB, 4th harmonic %.1f dB (reference -25 dB), level %+.1f dB vs no Warp, DC/RMS %.4f\n",
                     w100.h3, w100.h4, w100.rmsDb - base.rmsDb, w100.dc);
        check (warpRises, "Warp adds a steadily growing second harmonic");
        check (w100.h2 > -6.0 && w100.h2 < 0.0, "Warp 100 second harmonic about -3 dB (stronger than the reference)");
        check (w100.h4 > w100.h3 + 6.0, "Warp adds a ladder of even harmonics (4th stronger than 3rd)");
        check (w100.h2 > w100.h3 + 6.0, "Warp adds mostly even harmonics");
        check (std::abs (w100.dc) < 0.02 && std::abs (w100.rmsDb - base.rmsDb) < 2.0, "Warp adds no DC and keeps the level");

        const auto clip50  = measure (0.0f, 50.0f);
        const auto clip75  = measure (0.0f, 75.0f);
        const auto clip100 = measure (0.0f, 100.0f);
        std::printf ("       Clip 50: level %+.1f dB, crest %.1f dB; Clip 75: level %+.1f dB, crest %.1f dB\n"
                     "       Clip 100 (reference: level +12, crest 3.0 dB, 3rd harmonic -13 dB): level %+.1f dB, crest %.1f dB, 3rd %.1f dB, 2nd %.1f dB\n",
                     clip50.rmsDb - base.rmsDb, clip50.crestDb, clip75.rmsDb - base.rmsDb, clip75.crestDb,
                     clip100.rmsDb - base.rmsDb, clip100.crestDb, clip100.h3, clip100.h2);
        check (clip50.crestDb < base.crestDb - 0.5 && clip75.crestDb < clip50.crestDb && clip100.crestDb < clip75.crestDb,
               "Clip has an audible effect from about 50 and grows steadily");
        check (std::abs (clip50.rmsDb - base.rmsDb) < 1.5 && std::abs (clip75.rmsDb - base.rmsDb) < 2.5, "Clip 50 and 75 change tone without a jump in volume");
        check (clip100.rmsDb - base.rmsDb > 0.0 && clip100.rmsDb - base.rmsDb < 4.0, "Clip 100 is only slightly louder than no Clip (reference +12 dB, compensated)");
        check (clip100.crestDb > 2.0 && clip100.crestDb < 4.5, "Clip 100 has a crest factor near 3 dB");
        check (clip100.h3 > clip100.h2 + 6.0 && clip100.h3 > -20.0, "Clip 100 produces prominent odd harmonics");
    }

    // 15. Warp and Clip stay finite and bounded at their extremes in every filter mode.
    {
        bool ok = true;
        for (double sr : { 44100.0, 96000.0 })
            for (int mode = 0; mode < 6; ++mode)
            {
                Engine e; e.prepare (sr);
                auto p = defaultParams();
                for (auto& el : p.elements) { el.enabled = true; el.width = 100.0f; el.level = 100.0f; el.warp = 100.0f; el.clip = 100.0f; el.filterMode = static_cast<FilterMode> (mode); }
                p.master = 100.0f;
                e.setParams (p);
                for (int note = 20; note <= 110; note += 7) e.noteOn (note, 1.0f);
                for (int block = 0; block < 20; ++block)
                {
                    for (auto& el : p.elements) { el.warp = (block % 2) ? 100.0f : 0.0f; el.clip = (block % 3) ? 100.0f : 0.0f; }
                    e.setParams (p);
                    Capture part; renderBlocks (e, part, (int) (sr / 20));
                    ok = ok && allFinite (part);
                }
            }
        check (ok, "Warp and Clip extremes finite and bounded across modes, pitches, sample rates");
    }

    // 16. Decay and Sustain, fitted to 09_decay_* and 10_sustain_*: Attack -> Decay -> Sustain -> Release.
    // The amplitude follows u^1.95 where u falls linearly from 1 to the Sustain setting; Decay lasts
    // 0.104 s per unit. A broad filter (Width 20) keeps the RMS readings steady.
    {
        const double sr = 48000.0;
        auto windowDb = [] (const Capture& c, double from, double to, double rate)
        {
            return 20.0 * std::log10 (rms (c.l, (size_t) (from * rate), (size_t) (to * rate)) + 1.0e-12);
        };

        // Sustain settles at the measured levels: 25 -> -22.6 dB, 50 -> -11.0 dB, 75 -> -4.3 dB (Decay 25 = 2.6 s).
        {
            const struct { float sustain; double referenceDb; } cases[] = { { 25.0f, -22.6 }, { 50.0f, -11.0 }, { 75.0f, -4.3 } };
            bool ok = true;
            for (const auto& s : cases)
            {
                Engine e; e.prepare (sr);
                auto p = defaultParams();
                p.elements[0].width = 20.0f; p.elements[0].decay = 25.0f; p.elements[0].sustain = s.sustain;
                e.setParams (p); e.noteOn (60, 1.0f);
                Capture c; renderBlocks (e, c, (int) sr * 7);
                const double start = windowDb (c, 0.1, 0.4, sr), settled = windowDb (c, 4.0, 6.0, sr);
                std::printf ("       Sustain %.0f settles %.1f dB down (reference %.1f dB)\n", s.sustain, settled - start, s.referenceDb);
                ok = ok && std::abs ((settled - start) - s.referenceDb) < 1.5;

                if (s.sustain == 50.0f)
                {
                    e.noteOff (60);
                    Capture d; renderBlocks (e, d, (int) sr);
                    check (rms (d.l, (size_t) (0.8 * sr), (size_t) sr) == 0.0, "Release from the sustain stage reaches silence");
                }
            }
            check (ok, "Sustain settles at the reference levels (about Sustain^1.95 in amplitude)");
        }

        // Decay 100 / Sustain 0 (default Time): 10.4 s long, about -11 dB at the halfway point, -22 dB at 75%.
        {
            Engine e; e.prepare (sr);
            auto p = defaultParams();
            p.elements[0].width = 20.0f; p.elements[0].decay = 100.0f; p.elements[0].sustain = 0.0f;
            e.setParams (p); e.noteOn (60, 1.0f);
            Capture c; renderBlocks (e, c, (int) sr * 12);
            const double start = windowDb (c, 0.05, 0.5, sr), half = windowDb (c, 5.0, 5.4, sr), threeQuarter = windowDb (c, 7.6, 8.0, sr);
            double last = 0;
            for (int step = 0; step < 240; ++step)
                if (rms (c.l, (size_t) (step * 0.05 * sr), (size_t) ((step + 1) * 0.05 * sr)) > 1.0e-6)
                    last = (step + 1) * 0.05;
            std::printf ("       Decay 100: lasts %.2f s, halfway %.1f dB, 75%% %.1f dB (reference 10.4 s, -11, -22)\n",
                         last, half - start, threeQuarter - start);
            check (last > 10.2 && last < 10.7, "Decay 100 lasts about 10.4 s");
            check (std::abs ((half - start) + 11.0) < 2.0 && std::abs ((threeQuarter - start) + 22.0) < 2.5, "Decay follows the reference curve");
            check (rms (c.l, (size_t) (11 * sr), (size_t) (12 * sr)) == 0.0, "Sustain 0 decays to silence while the key is held");
            e.noteOff (60);
            Capture d; renderBlocks (e, d, (int) sr);
            check (rms (d.l, 0, d.l.size()) == 0.0, "releasing a fully decayed note stays silent");
        }

        // Defaults (Decay 0, Sustain 100) hold full level.
        {
            Engine e; e.prepare (sr);
            auto p = defaultParams(); p.elements[0].width = 20.0f;
            e.setParams (p); e.noteOn (60, 1.0f);
            Capture c; renderBlocks (e, c, (int) sr * 4);
            check (std::abs (windowDb (c, 0.2, 0.7, sr) - windowDb (c, 3.0, 4.0, sr)) < 1.0, "default Sustain 100 holds full level");
        }
    }

    // 17. Time scales the Release tail: about 0.05 s at Time 0, unchanged at 50, twice as long at 100.
    {
        const double sr = 48000.0;
        // Seconds from key-up until the output is silent (checked in 50 ms steps).
        auto tailSeconds = [sr] (float time)
        {
            Engine e; e.prepare (sr);
            auto p = defaultParams(); p.elements[0].width = 20.0f; p.elements[0].time = time;
            e.setParams (p); e.noteOn (60, 1.0f);
            Capture c; renderBlocks (e, c, (int) sr);
            e.noteOff (60);
            Capture d; renderBlocks (e, d, (int) (sr * 3));
            double last = 0;
            for (int step = 0; step < 60; ++step)
                if (rms (d.l, (size_t) (step * 0.05 * sr), (size_t) ((step + 1) * 0.05 * sr)) > 1.0e-6)
                    last = (step + 1) * 0.05;
            return last;
        };

        const double t0 = tailSeconds (0.0f), t50 = tailSeconds (50.0f), t100 = tailSeconds (100.0f);
        std::printf ("       release tail: Time 0 %.2f s, Time 50 %.2f s, Time 100 %.2f s (reference about 0.05 / 0.45 / 0.8 s)\n", t0, t50, t100);
        check (t0 <= 0.1, "Time 0 gives a very short release");
        check (t50 > 0.4 && t50 <= 0.6, "Time 50 leaves the default 0.5 s release unchanged");
        check (t100 > 0.9 && t100 <= 1.1, "Time 100 doubles the release tail");

        // With Release 25 (2.5 s) the tail should scale the same way: about 2.5 s at Time 50, 5 s at 100.
        auto longTail = [sr] (float time)
        {
            Engine e; e.prepare (sr);
            auto p = defaultParams(); p.elements[0].width = 20.0f; p.elements[0].time = time; p.elements[0].release = 25.0f;
            e.setParams (p); e.noteOn (60, 1.0f);
            Capture c; renderBlocks (e, c, (int) sr);
            e.noteOff (60);
            Capture d; renderBlocks (e, d, (int) (sr * 8));
            double last = 0;
            for (int step = 0; step < 160; ++step)
                if (rms (d.l, (size_t) (step * 0.05 * sr), (size_t) ((step + 1) * 0.05 * sr)) > 1.0e-6)
                    last = (step + 1) * 0.05;
            return last;
        };
        const double l50 = longTail (50.0f), l100 = longTail (100.0f);
        std::printf ("       Release 25 tail: Time 50 %.2f s, Time 100 %.2f s (reference about 2.3 / 4.7 s)\n", l50, l100);
        check (l50 > 2.3 && l50 <= 2.7 && l100 > 4.8 && l100 <= 5.2, "Time scales a longer Release the same way");

        // A Decay 50 / Sustain 0 note fades away by itself: quickly at Time 0, slowly at Time 100.
        auto fadeSeconds = [sr] (float time)
        {
            Engine e; e.prepare (sr);
            auto p = defaultParams(); p.elements[0].width = 20.0f; p.elements[0].time = time;
            p.elements[0].decay = 50.0f; p.elements[0].sustain = 0.0f;
            e.setParams (p); e.noteOn (60, 1.0f);
            Capture c; renderBlocks (e, c, (int) (sr * 12));
            double last = 0;
            for (int step = 0; step < 240; ++step)
                if (rms (c.l, (size_t) (step * 0.05 * sr), (size_t) ((step + 1) * 0.05 * sr)) > 1.0e-6)
                    last = (step + 1) * 0.05;
            return last;
        };
        const double f0 = fadeSeconds (0.0f), f50 = fadeSeconds (50.0f), f100 = fadeSeconds (100.0f);
        std::printf ("       Decay 50 fades out in: Time 0 %.2f s, Time 50 %.2f s, Time 100 %.2f s (reference about 0.5 s, then 5.2 s and 10.4 s at Time 50 and Time 100 if scaled)\n", f0, f50, f100);
        check (f0 > 0.4 && f0 < 0.65 && f50 > 5.0 && f50 < 5.5 && f100 > 10.2 && f100 < 10.9, "Time scales Decay");
    }

    // 18. Link: Elements 2 and 3 can follow Element 1's Attack, Decay, Sustain, Release, and Time.
    // Element 1 is panned hard left and Element 2 hard right, so each channel is one Element.
    {
        const double sr = 48000.0;
        auto play = [sr] (bool link, float el1Release, float el1Time, float el1Sustain, float el1Decay)
        {
            Engine e; e.prepare (sr);
            auto p = defaultParams();
            p.elements[0].width = 20.0f; p.elements[0].pan = -100.0f; p.elements[0].release = el1Release;
            p.elements[0].time = el1Time; p.elements[0].sustain = el1Sustain; p.elements[0].decay = el1Decay;
            p.elements[1].enabled = true; p.elements[1].width = 20.0f; p.elements[1].pan = 100.0f;   // own settings: defaults
            p.elements[1].link = link;
            p.elements[0].link = true;   // Element 1 has no Link: this must be ignored
            e.setParams (p); e.noteOn (60, 1.0f);
            Capture c; renderBlocks (e, c, (int) (sr * 7));
            e.noteOff (60);
            Capture d; renderBlocks (e, d, (int) (sr * 6));
            return std::make_pair (c, d);
        };
        auto tail = [sr] (const std::vector<float>& x)
        {
            double last = 0;
            for (int step = 0; step < 120; ++step)
                if (rms (x, (size_t) (step * 0.05 * sr), (size_t) ((step + 1) * 0.05 * sr)) > 1.0e-6)
                    last = (step + 1) * 0.05;
            return last;
        };

        // Release: Element 1 has Release 25 (2.5 s), Element 2 keeps the default 0.5 s unless linked.
        const auto unlinked = play (false, 25.0f, 50.0f, 100.0f, 0.0f);
        const auto linked   = play (true,  25.0f, 50.0f, 100.0f, 0.0f);
        const double leftTail = tail (unlinked.second.l), ownTail = tail (unlinked.second.r), linkedTail = tail (linked.second.r);
        std::printf ("       tails: Element 1 %.2f s; Element 2 own %.2f s, linked %.2f s\n", leftTail, ownTail, linkedTail);
        check (leftTail > 2.3 && leftTail < 2.7, "Element 1 keeps its own release (Link on Element 1 is ignored)");
        check (ownTail > 0.4 && ownTail < 0.6, "unlinked Element 2 uses its own release");
        check (linkedTail > 2.3 && linkedTail < 2.7, "linked Element 2 follows Element 1's release");

        // Time follows too: Element 1 at Time 100 doubles a linked Element 2's default release.
        const auto slow = play (true, 5.0f, 100.0f, 100.0f, 0.0f);
        const double slowTail = tail (slow.second.r);
        check (slowTail > 0.9 && slowTail < 1.1, "linked Element 2 follows Element 1's Time");

        // Sustain and Decay follow: Element 1 sustains 50 (about -11 dB); Element 2 would otherwise stay at full level.
        const auto own = play (false, 5.0f, 50.0f, 50.0f, 25.0f);
        const auto sus = play (true,  5.0f, 50.0f, 50.0f, 25.0f);
        const double ownDrop = 20.0 * std::log10 (rms (own.first.r, (size_t) (5.0 * sr), (size_t) (6.0 * sr)) / rms (own.first.r, (size_t) (0.1 * sr), (size_t) (0.4 * sr)));
        const double linkedDrop = 20.0 * std::log10 (rms (sus.first.r, (size_t) (5.0 * sr), (size_t) (6.0 * sr)) / rms (sus.first.r, (size_t) (0.1 * sr), (size_t) (0.4 * sr)));
        std::printf ("       Element 2 level after decay: own %.1f dB, linked %.1f dB\n", ownDrop, linkedDrop);
        check (std::abs (ownDrop) < 1.0 && linkedDrop < -8.0 && linkedDrop > -13.0, "linked Element 2 follows Element 1's Decay and Sustain");
    }

    std::printf ("\n%s\n", failures == 0 ? "All engine tests passed." : "Engine tests FAILED.");
    return failures == 0 ? 0 : 1;
}
