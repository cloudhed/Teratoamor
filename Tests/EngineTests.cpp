// Standalone checks for the DSP engine. No JUCE and no audio device needed.
#include "DSP/Engine.h"

#include <algorithm>
#include <cmath>
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

    std::printf ("\n%s\n", failures == 0 ? "All engine tests passed." : "Engine tests FAILED.");
    return failures == 0 ? 0 : 1;
}
