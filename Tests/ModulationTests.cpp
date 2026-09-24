// Standalone checks for the modulation sections. No JUCE and no audio device needed.
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

    constexpr double sr = 44100.0;
    constexpr int rate1over1 = 19;   // "1/1": 2 seconds at 120 BPM

    EngineParams basePatch()
    {
        EngineParams p;
        p.elements[0].enabled = true;
        p.elements[0].width = 50.0f;
        return p;
    }

    float unitsOf (const Modulation::VoiceState& s, ModTarget t) { return s.get()[Modulation::idx (t)]; }

    // Runs the per-chunk modulation update for a while and returns the target's units at the end.
    float unitsAfter (Modulation::VoiceState& s, const EngineParams& p, ModTarget t, double seconds)
    {
        const std::array<float, 3> none {};
        const int chunks = static_cast<int> (seconds * sr / 16.0);
        for (int i = 0; i < chunks; ++i)
            s.update (16, p, none, true);
        return unitsOf (s, t);
    }

    // Zero crossings per second over a window: a cheap pitch estimate for narrow-band output.
    double crossingsPerSecond (const std::vector<float>& x, double from, double to)
    {
        int count = 0;
        const auto a = static_cast<size_t> (from * sr), b = static_cast<size_t> (to * sr);
        for (size_t i = a + 1; i < b; ++i)
            if ((x[i - 1] < 0.0f) != (x[i] < 0.0f))
                ++count;
        return count / (to - from);
    }
}

int main()
{
    // Every wavetable stays in range and is finite.
    {
        bool ok = true;
        for (int w = 1; w < static_cast<int> (ModWave::count); ++w)
            for (int i = 0; i < 1000; ++i)
            {
                const float v = Modulation::waveValue (static_cast<ModWave> (w), i / 1000.0f);
                ok = ok && std::isfinite (v) && std::abs (v) <= 1.0001f;
            }
        check (ok, "all wavetables stay within -1..1");
        check (Modulation::waveValue (ModWave::pulse100, 0.0f) == 1.0f, "Pulse starts positive");
        check (Modulation::waveValue (ModWave::saw, 0.9f) > Modulation::waveValue (ModWave::saw, 0.1f), "Saw rises");
        check (Modulation::waveValue (ModWave::ramp, 0.9f) < Modulation::waveValue (ModWave::ramp, 0.1f), "Ramp falls");
        check (Modulation::waveValue (ModWave::pulse50, 0.5f) == 1.0f && Modulation::waveValue (ModWave::pulse50, 0.9f) == -1.0f
               && Modulation::waveValue (ModWave::pulse25, 0.8f) == 1.0f && Modulation::waveValue (ModWave::pulse25, 0.95f) == -1.0f,
               "Pulse50% and Pulse25% are mostly high with a short low part");
        check (Modulation::waveValue (ModWave::hump, 0.5f) > 0.99f && Modulation::waveValue (ModWave::hump, 0.0f) == 0.0f,
               "Hump is a positive-only arch");
    }

    // Target lists: an oscillator never offers its own Rate; envelopes offer no Distort or Delay.
    {
        const auto env = ModTargets::listFor (0);
        const auto osc4 = ModTargets::listFor (3);
        const auto osc5 = ModTargets::listFor (4);
        auto has = [] (const ModTargets::List& l, ModTarget t)
        {
            return std::find (l.items.begin(), l.items.begin() + l.size, t) != l.items.begin() + l.size;
        };
        check (env.size == 23 && env.items[19] == ModTarget::filterQ && ! has (env, ModTarget::distortTone), "envelope keeps old target indices");
        check (osc4.size == 34 && osc4.items[30] == ModTarget::masterVolume
            && ! has (osc4, ModTarget::mod4Rate) && has (osc4, ModTarget::mod5Rate), "oscillator keeps old target indices");
        check (! has (osc5, ModTarget::mod5Rate) && has (osc5, ModTarget::masterPan), "Modulation 5 omits its own Rate");
        bool allVolumes = true;
        for (int section = 0; section < 6; ++section)
        {
            const auto list = ModTargets::listFor (section);
            allVolumes = allVolumes && list.items[(size_t) list.size - 3] == ModTarget::el1Volume
                && list.items[(size_t) list.size - 2] == ModTarget::el2Volume
                && list.items[(size_t) list.size - 1] == ModTarget::el3Volume;
        }
        check (allVolumes, "all six modulator dropdowns append three Element Volume targets");
        Modulation::Units volumeUnits {};
        volumeUnits[Modulation::idx (ModTarget::el2Volume)] = -30.0f;
        volumeUnits[Modulation::idx (ModTarget::allVolume)] = 10.0f;
        check (Modulation::elementUnits (volumeUnits, 0, Modulation::Kind::volume) == 10.0f
            && Modulation::elementUnits (volumeUnits, 1, Modulation::Kind::volume) == -20.0f
            && Modulation::elementUnits (volumeUnits, 2, Modulation::Kind::volume) == 10.0f,
            "individual Volume affects only its Element and combines with All Volume");
    }

    // Pitch depth scale (0.36 semitones per unit, Depth 100 = three octaves): a Pulse on All Pitch. The free oscillator value
    // is fed in by hand so the wave position does not matter.
    {
        auto p = basePatch();
        auto& m = p.mods[3];
        m.wave = ModWave::pulse100; m.target = ModTarget::allPitch; m.gateTrig = false;
        Modulation::VoiceState s;
        s.prepare (sr, 0);
        s.noteOn (p);

        const float depths[]   = { 10.0f, 20.0f, 25.0f, 50.0f, -50.0f };
        const float expected[] = { -3.6f, -7.2f, -9.0f, -18.0f, 18.0f };   // low half of the Pulse
        const char* names[] = { "Depth 10 is 3.6 semitones down", "Depth 20 is 7.2 semitones down",
                                "Depth 25 is 9 semitones down", "Depth 50 is 18 semitones down",
                                "Depth -50 inverts the wave: 18 semitones up" };

        for (int i = 0; i < 5; ++i)
        {
            m.depth = depths[i];
            s.update (16, p, { -1.0f, 0.0f, 0.0f }, true);
            check (std::abs (unitsOf (s, ModTarget::allPitch) * Modulation::pitchSemitonesPerUnit - expected[i]) < 0.001f, names[i]);
        }
    }

    // Envelope section: follows Sustain, releases to zero, and inverts with negative Depth.
    {
        auto p = basePatch();
        auto& m = p.mods[0];
        m.target = ModTarget::el1Warp; m.depth = 60.0f; m.sustain = 50.0f; m.release = 10.0f;
        Modulation::VoiceState s;
        s.prepare (sr, 0);
        s.noteOn (p);
        const float held = unitsAfter (s, p, ModTarget::el1Warp, 0.5);
        check (held > 0.0f && held < 30.0f, "envelope holds at its Sustain level");
        s.noteOff (p);
        check (unitsAfter (s, p, ModTarget::el1Warp, 3.0) == 0.0f, "envelope returns to zero after Release");

        m.depth = -60.0f;
        Modulation::VoiceState s2;
        s2.prepare (sr, 0);
        s2.noteOn (p);
        check (unitsAfter (s2, p, ModTarget::el1Warp, 0.5) == -held, "negative Depth inverts the envelope");
    }

    // An oscillator can modulate an envelope section's Depth.
    {
        auto p = basePatch();
        p.mods[0].target = ModTarget::allWarp; p.mods[0].depth = 20.0f;
        p.mods[3].wave = ModWave::pulse100; p.mods[3].target = ModTarget::mod1Depth; p.mods[3].depth = 30.0f;
        Modulation::VoiceState s;
        s.prepare (sr, 0);
        s.noteOn (p);
        for (int i = 0; i < 4000; ++i)
            s.update (16, p, { 1.0f, 0.0f, 0.0f }, true);
        check (std::abs (unitsOf (s, ModTarget::allWarp) - 50.0f) < 1.0f, "an oscillator adds to an envelope's Depth (20 + 30)");
    }

    // Gate Trig restarts the pattern per note; without it the voice follows the free oscillator.
    {
        auto p = basePatch();
        auto& m = p.mods[3];
        m.wave = ModWave::saw; m.target = ModTarget::allWarp; m.depth = 100.0f; m.rate = rate1over1; m.gateTrig = true;
        Modulation::VoiceState s;
        s.prepare (sr, 0);
        s.noteOn (p);
        const float early = unitsAfter (s, p, ModTarget::allWarp, 0.3);
        const float later = unitsAfter (s, p, ModTarget::allWarp, 0.8);
        s.noteOn (p);
        const float restarted = unitsAfter (s, p, ModTarget::allWarp, 0.3);
        check (later > early && std::abs (restarted - early) < 2.0f, "Gate Trig restarts the wave on every note");

        m.gateTrig = false;
        Modulation::VoiceState f;
        f.prepare (sr, 0);
        f.noteOn (p);
        f.update (16, p, { 0.5f, 0.0f, 0.0f }, true);
        check (std::abs (unitsOf (f, ModTarget::allWarp) - 50.0f) < 0.001f, "without Gate Trig the free oscillator is used");
    }

    // Soft smooths a Pulse: the biggest step between chunks shrinks a lot.
    {
        auto biggestStep = [] (float soft)
        {
            Modulation::Oscillator o;
            o.seed (1);
            float previous = 0.0f, biggest = 0.0f;
            for (int i = 0; i < 20000; ++i)
            {
                const float v = o.advance (ModWave::pulse100, soft * 0.01f, 2.0, 16, sr);
                biggest = std::max (biggest, std::abs (v - previous));
                previous = v;
            }
            return biggest;
        };
        check (biggestStep (0.0f) > 1.5f && biggestStep (100.0f) < 0.05f, "Soft 100 turns an instant Pulse jump into a slow slide");
    }

    // Engine: a slow Pulse on All Pitch really moves the pitch of the audible output.
    {
        auto p = basePatch();
        p.elements[0].width = 85.0f;   // narrow enough that zero crossings follow the pitch
        p.mods[3].wave = ModWave::pulse100; p.mods[3].target = ModTarget::allPitch;
        p.mods[3].depth = 100.0f / 3.0f; p.mods[3].rate = rate1over1;   // +/-12 semitones
        Engine e;
        e.prepare (sr);
        e.setParams (p);
        e.noteOn (60, 1.0f);
        std::vector<float> l (static_cast<size_t> (2.2 * sr)), r (l.size());
        for (size_t at = 0; at < l.size(); at += 512)
        {
            const int n = static_cast<int> (std::min<size_t> (512, l.size() - at));
            e.setParams (p);
            e.render (l.data() + at, r.data() + at, n);
        }
        const double high = crossingsPerSecond (l, 0.5, 0.95), low = crossingsPerSecond (l, 1.5, 1.95);
        std::printf ("       high half %.0f crossings/s, low half %.0f\n", high, low);
        check (high > 2.5 * low && high < 6.0 * low, "Pulse on All Pitch alternates two octaves apart at Depth 33");
    }

    // Engine: Glide makes a new note slide up from the previous note; Glide 0 does not.
    {
        auto glideRatio = [] (float glide)
        {
            auto p = basePatch();
            p.elements[0].width = 85.0f;
            p.glide = glide;
            Engine e;
            e.prepare (sr);
            std::vector<float> l (512), r (512), heard;
            auto run = [&] (double seconds, bool keep)
            {
                for (int done = 0; done < static_cast<int> (seconds * sr); done += 512)
                {
                    e.setParams (p);
                    e.render (l.data(), r.data(), 512);
                    if (keep) heard.insert (heard.end(), l.begin(), l.end());
                }
            };
            e.setParams (p);
            e.noteOn (48, 1.0f);
            run (0.3, false);
            e.noteOff (48);
            run (1.5, false);          // let the first note die away completely
            e.noteOn (72, 1.0f);       // two octaves up
            run (2.6, true);
            const double early = crossingsPerSecond (heard, 0.1, 0.3), late = crossingsPerSecond (heard, 2.2, 2.6);
            return early / late;
        };
        const double slid = glideRatio (100.0f), plain = glideRatio (0.0f);
        std::printf ("       early/late crossing rate: Glide 100 = %.2f, Glide 0 = %.2f\n", slid, plain);
        check (slid < 0.7 && plain > 0.8 && plain < 1.25, "Glide slides a new note up from the previous note; Glide 0 does not");
    }

    // Engine: in Rate mode a bigger jump takes longer; in Time mode it does not.
    {
        auto crossingsAt = [] (bool byRate, double from, double to)
        {
            auto p = basePatch();
            p.elements[0].width = 85.0f;
            p.glide = 100.0f;
            p.glideByRate = byRate;
            Engine e;
            e.prepare (sr);
            std::vector<float> l (512), r (512), heard;
            const auto run = [&] (double seconds, bool keep)
            {
                for (int done = 0; done < static_cast<int> (seconds * sr); done += 512)
                {
                    e.setParams (p);
                    e.render (l.data(), r.data(), 512);
                    if (keep) heard.insert (heard.end(), l.begin(), l.end());
                }
            };
            e.setParams (p);
            e.noteOn (48, 1.0f);
            run (0.3, false);
            e.noteOff (48);
            run (1.5, false);
            e.noteOn (72, 1.0f);   // 24 semitones: 2 s in Time mode, 4 s in Rate mode
            run (5.0, true);
            return crossingsPerSecond (heard, from, to);
        };
        const double timeMode = crossingsAt (false, 2.2, 2.6) / crossingsAt (false, 4.6, 5.0);
        const double rateMode = crossingsAt (true, 2.2, 2.6) / crossingsAt (true, 4.6, 5.0);
        std::printf ("       pitch at 2.4 s relative to final: Time mode %.2f, Rate mode %.2f\n", timeMode, rateMode);
        check (timeMode > 0.85 && rateMode < 0.8, "Glide Rate mode takes longer for a big jump; Time mode does not");
    }

    // Engine: Master Pan, and Volume modulation cannot revive a silent Element.
    {
        auto p = basePatch();
        p.elements[0].width = 40.0f;
        p.masterPan = 100.0f;
        Engine e;
        e.prepare (sr);
        e.setParams (p);
        e.noteOn (60, 1.0f);
        std::vector<float> l (8192), r (8192);
        e.render (l.data(), r.data(), 8192);
        double el = 0.0, er = 0.0;
        for (size_t i = 4096; i < l.size(); ++i) { el += l[i] * l[i]; er += r[i] * r[i]; }
        check (er > 0.0 && el < er * 1.0e-4, "Master Pan 100 puts the sound on the right");

        auto q = basePatch();
        q.elements[0].enabled = false;
        q.mods[3].wave = ModWave::pulse100; q.mods[3].target = ModTarget::allVolume; q.mods[3].depth = 100.0f;
        Engine e2;
        e2.prepare (sr);
        e2.setParams (q);
        e2.noteOn (60, 1.0f);
        e2.render (l.data(), r.data(), 8192);
        double peak = 0.0;
        for (size_t i = 0; i < l.size(); ++i)
            peak = std::max ({ peak, static_cast<double> (std::abs (l[i])), static_cast<double> (std::abs (r[i])) });
        check (peak < 1.0e-6, "Volume modulation cannot revive an Element that is switched off");
    }

    // Stress: every wave and target at extreme Depth and Rate, finite and bounded output.
    {
        Engine e;
        e.prepare (sr);
        bool finite = true;
        std::vector<float> l (512), r (512);
        for (int round = 0; round < 40; ++round)
        {
            auto p = basePatch();
            for (auto& el : p.elements) { el.enabled = true; el.width = 100.0f; }
            p.globalFilter.type = GlobalFilterType::lowpass;
            p.distortion.type = DistortionType::drive;
            p.delay.lines[0].enabled = true;

            for (int m = 0; m < EngineParams::numMods; ++m)
            {
                const auto list = ModTargets::listFor (m);
                auto& mp = p.mods[(size_t) m];
                mp.target = list.items[(size_t) ((round * 7 + m * 5) % list.size)];
                mp.depth = (round + m) % 2 == 0 ? 100.0f : -100.0f;
                mp.wave = static_cast<ModWave> (1 + (round + m * 3) % (static_cast<int> (ModWave::count) - 1));
                mp.gateTrig = (round + m) % 3 == 0;
                mp.rate = (round * 11 + m * 5) % 26;
                mp.soft = static_cast<float> ((round * 13) % 101);
                mp.attack = static_cast<float> (round % 5);
                mp.sustain = 60.0f;
            }

            e.setParams (p);
            e.noteOn (30 + round, 1.0f);
            e.noteOn (90 - round, 1.0f);

            for (int b = 0; b < 40; ++b)
            {
                e.setParams (p);
                e.render (l.data(), r.data(), 512);
                for (int i = 0; i < 512; ++i)
                    finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]) && std::abs (l[i]) <= 1.0f && std::abs (r[i]) <= 1.0f;
            }

            e.allNotesOff();
        }
        check (finite, "extreme modulation of every target stays finite and within range");
    }

    std::printf (failures == 0 ? "All modulation tests passed.\n" : "%d modulation test(s) FAILED.\n", failures);
    return failures == 0 ? 0 : 1;
}
