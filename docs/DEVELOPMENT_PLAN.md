# Teratoamor development plan

## Goal

Build an original, clean-room, modern 64-bit synthesizer inspired by the observable behaviour of Majken's Chimera. The first priority is a small playable instrument, not a full recreation of the original interface.

## Target platform and tools

- Windows x64
- VST3 instrument and standalone application
- Cubase 10 as the primary DAW test host
- CMake 4.4.3
- Microsoft Visual C++ Build Tools / Visual Studio 18
- C++17, or the minimum language level required by JUCE
- JUCE 9.0.2 pinned as a Git submodule at `external/JUCE`
- VS Code as the development environment

The intended repository licence is AGPLv3 while JUCE is used under its AGPLv3 option. Distribution that does not comply with AGPLv3 may require a commercial JUCE licence. Any additional dependency needs a licence review before adoption.

## Current status

Complete:

- [x] Development environment setup: VS Code workflow, CMake 4.4.3, and Visual Studio 18 Build Tools
- [x] JUCE integration: JUCE 9.0.2 pinned as the `external/JUCE` Git submodule
- [x] Phase 0 project skeleton (VST3 + Standalone build; silent processor)
- [x] Parameter IDs, ranges, and host state save/restore for master level and three Elements (on, octave, semitone, fine, width, attack, release, level, pan), with a temporary generic GUI

- [x] First sound: 8-voice engine in `Source/DSP/` (seeded noise -> stability-guarded TPT band-pass -> cubic-attack/linear-release envelope, per Element), MIDI note on/off with sample-accurate timing, and `TeratoamorEngineTests` (`build\Debug\TeratoamorEngineTests.exe`)

- [x] Sustain pedal (CC 64) and pitch bend (+/-2 semitones)
- [x] Cubase 10 verification (user-confirmed): VST3 loads and plays, level matches the reference, eight simultaneous voices do not overload

- [x] Cubase 10 session save/restore and automation (user-confirmed)

Still pending for Milestone 1: standalone MIDI check, a designed GUI, and the remaining measurement-based verification items.

## Phase 0: project skeleton

- [x] Add the root `CMakeLists.txt` without modifying `external/JUCE`.
- [x] Define VST3 instrument and standalone targets for Windows x64.
- [x] Add a minimal source layout that keeps DSP, parameters/state, and GUI concerns separate.
- [x] Establish warning settings and Debug build defaults suitable for MSVC.
- [x] Add the intended AGPLv3 repository licence and required notices.
- [x] Configure successfully and record the generated build directory.
- [x] Build both targets and report their exact artifact locations.

Expected initial commands once `CMakeLists.txt` exists:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
```

If compiler discovery fails in ordinary PowerShell, use a Visual Studio Developer PowerShell or configure the generator explicitly. Explain any such step in plain language.

## Milestone 1: first playable instrument

Deliver the smallest useful instrument before expanding the feature set.

### Build and host integration

- [x] Produce a Windows x64 VST3.
- [x] Produce a Windows x64 standalone application.
- [x] Verify that the VST3 is discoverable and loads in Cubase 10.
- [x] Verify that the standalone application starts and accepts MIDI input (user-confirmed with a MIDI keyboard).

### Voice engine

- [x] Implement eight-voice polyphony with a clear voice-allocation policy.
- [x] Generate three independent white-noise Elements per voice.
- [x] Use deterministic, independently seeded noise sources for each voice and Element.
- [x] Tune resonators from MIDI notes, with note 60 targeting middle C.
- [x] Add octave, semitone, and fine-detune controls per Element.

### Element signal path

- [x] Implement a stable resonant band-pass mode.
- [x] Implement Width using the measured mapping documented in `CHIMERA_REFERENCE.md`.
- [x] Clamp extreme resonance safely and test for NaN, infinity, and runaway output.
- [x] Add an amplitude envelope per Element.
- [x] Start with the measured approximately linear time mapping and curved Attack response.
- [x] Add level and pan per Element.

### Parameters, state, and GUI

- [x] Define stable parameter IDs and practical ranges.
- [x] Smooth continuously adjustable audio parameters.
- [x] Save and restore parameter state through the host.
- [x] Add master level.
- [x] Build a simple, functional, original JUCE GUI for the Milestone 1 controls (`Source/PluginEditor.*`: master plus one column per Element, envelope in the order Attack, Decay, Sustain, Release, Time, with Link greying out the linked envelope). A designed GUI is still Milestone 5.
- [ ] Avoid Chimera branding, artwork, silhouette, layout trade dress, and preset names.

### Milestone 1 verification

- [x] Exercise MIDI note-on, note-off, overlapping notes, and voice stealing (note-on, note-off, and overlapping notes user-confirmed in Cubase; voice stealing covered by the engine test with 20 overlapping notes, not yet confirmed by ear).
- [x] Test common sample rates and audio buffer sizes (engine tests: sample rates 22.05 to 192 kHz, and buffer sizes 1 to 2048 giving the same level within 0.01 dB; not yet run in a real host at each setting).
- [x] Confirm the real-time audio path performs no allocation and takes no locks (engine test counts heap allocations across 300 blocks of notes, parameter changes, and rendering: zero; code review of `processBlock` and `Source/DSP` found no locks, strings, or containers. The JUCE host layer itself was not instrumented).
- [x] Stress Width near 100 across the supported pitch range (engine tests: Width 0 to 100 at every third note from 0 to 127, five sample rates, with rapid changes, in every filter mode).
- [x] Confirm silence and finite output after resets, rapid parameter changes, and state restoration (engine tests cover reset and rapid changes; state restoration was confirmed by the user in Cubase).
- [x] Complete the remaining clean-room Width comparison. Pitch, envelope timing, and stereo behaviour are considered good enough and effectively final by listening feedback; Width still needs the matched analysis below.
  - [x] Record Chimera at 44.1 kHz with the shared three-note MIDI sequence for BP Wide, BP Narrow, Peak Wide, and Peak Narrow at Width 0, 25, 50, 75, 90, and 100 (`reference/chimera/sample batches/chimera_width_across_all_elements/`; Master 100, all other settings held constant).
  - [x] Render the same 24 conditions through Teratoamor with identical MIDI and control settings (`reference/teratoamor/width_across_all_elements/`; generated by `TeratoamorWidthRenderer`).
  - [x] Use SciPy spectral estimation and curve fitting on both batches to compare resonance, bandwidth/Q, residual noise, level, and behaviour across the three pitches (`Tests/analyze_width_comparison.py`; findings in `docs/WIDTH_COMPARISON.md`).
  - [x] Decide from the matched results whether Width 100 needs mode-specific resonance caps. BP Wide remains close, but BP Narrow, Peak Wide, and Peak Narrow need separate high-end behaviour; both Narrow modes also need independent nonlinear Width mappings below 90. See `docs/WIDTH_COMPARISON.md`.
  - [x] Adjust the Width mapping and per-mode caps, refit the Peak-mode noise floors, add measured Width-dependent level curves, rebuild, rerender all 24 conditions, and rerun the numerical-stability and extreme-Width tests. Median sampled skirt errors are now generally within about 3.5 dB (worst 4.8 dB at BP Wide 100), Width-level shapes are within 0.5 dB, final renders do not clip, and all engine tests pass. See `docs/WIDTH_COMPARISON.md`.
  - [x] Prepare the focused Width 90/100 comparisons for every active filter mode and complete the objective A/B evaluation (`reference/analysis/width_ab_listening/`; generated by `Tests/create_width_ab_listening_files.py`). The first round of measurements supported keeping the DSP unchanged, but listening (next item) found otherwise.
  - [x] First listening pass (user): BP Narrow 90/100 and Peak Wide 90/100 close; BP Wide 90/100 had slightly more high end in Chimera; Peak Narrow 90/100 was audibly noisier in Teratoamor. Measured with a new tone-to-6-16 kHz-floor and octave-band comparison, then fixed: Peak Narrow noise curve refitted and subtracted from the cascade (floor now within about 1 dB at every Width), BP Wide capped at Q 400 (Width 100 median error 4.8 to 3.7 dB). Engine tests pass and the A/B files were regenerated. See `docs/WIDTH_COMPARISON.md`.
  - [x] Final human playback judgment (user, after the corrections): Peak Narrow 90/100 and BP Wide 100 practically identical; Peak Wide 90/100 and BP Narrow 90/100 already close; BP Wide 90 leaves Chimera feeling slightly harsher or more saturated on high notes (small, accepted for now; measured Chimera high-band floor is about 3.6 dB above Teratoamor's at note 84, and a mild saturation stage has not been investigated).
- [x] Confirm saved sessions restore the audible state.

## Later milestones

### Milestone 2: Element character

- [x] Add the remaining Element filter modes: Off, Bypass, BP Narrow, Peak Wide, and Peak Narrow (new `el<N>_filter` choice parameter, version hint 2; `Source/DSP/ElementFilter.h`). Fitted to `02_filter_*.wav` at an assumed Width 90; needs a listening check and confirmation of the batch's Width settings.
- [x] Implement Warp as an original asymmetric waveshaping stage (`Source/DSP/ElementShaper.h`; `el<N>_warp`, version hint 3, on all three Elements). Deliberately stronger than `03_warp_*.wav` after listening feedback (second harmonic -2.9 dB at Warp 100 vs -8.1 dB; fourth -26 dB vs -25 dB in the reference; squared plus rectified terms, concave curve).
- [x] Implement Clip, including the strong Clip 100 behaviour (`el<N>_clip`, version hint 3). The VSTXML export lists Clip on Elements 1 and 3 only, so Element 2 has none. Clip 100 level, crest factor, and odd harmonics are close to `04_clip_100.wav`, but the onset is deliberately earlier than the reference (audible from about Clip 50, listening feedback), with automatic loudness compensation (Clip 100 is about +1.6 dB louder than no Clip, where the reference is about +12 dB; deliberate, listening feedback); the reference third harmonic is about 5 dB stronger and it also has a second harmonic (-27 dB) that Teratoamor lacks (unexplained).
- [x] Add Decay and Sustain (`el<N>_decay`, `el<N>_sustain`, version hint 4; Attack -> Decay -> Sustain -> Release in `ElementEnvelope.h`), fitted to `09_decay_*`, `10_sustain_*`, and `11_combined_*`: Decay is 0.104 s per unit and the output amplitude is a linear ramp raised to 1.95, which also sets the Sustain level (Sustain 50 is about 11 dB down). Defaults match the reference (Decay 0, Sustain 100). Values are read at note-on. Hold was dropped: it did not play at all in the reference above 0, so Teratoamor has no Hold control.
- [x] Add Time as a Decay and Release length control (`el<N>_time`, version hint 4, default 50): both lengths are multiplied by Time/50, with a floor of 0.1 at Time 0. Fitted to the `07_time_*` recordings: the default Release tail (about 0.05 s, 0.45 s, 0.8 s at Time 0, 50, 100), Release 25 (about 0, 2.3 s, 4.7 s), and Decay 50 with Sustain 0 (about 0.5 s at Time 0). Onset was assumed unaffected (the slower onset in the Time 100 file is probably MIDI keypress timing).
- [ ] Deferred (by choice): check whether Time affects Attack, and measure the exact Release curve shape.
- [x] Add Element 2 and 3 Link (`el2_link`, `el3_link`, version hint 5, default on by the user's choice; Element 1 has none). Per the user, Link is a checkbox: on = the Element uses Element 1's Attack, Decay, Sustain, Release, and Time, off = its own. Resolved in `Engine::setParams`. While Link is on the Element's own Attack to Time controls are greyed out in the editor.

### Milestone 3: global processing

- [x] Add the global multimode filter (`Source/DSP/GlobalFilter.h`; `gf_type`, `gf_cutoff`, `gf_q`, version hint 6). Type list from the user: Bypass, Lowpass, Highpass, Bandpass, Bandreject, Peak. Deliberately basic: a stereo 2-pole TPT state-variable filter after the summed Elements and before Master, Cutoff 20 Hz to 20 kHz (exponential), Q 0.71 to 25 (exponential), Peak is a +12 dB bell. The mappings are first estimates, not fitted to recordings; engine tests cover each type's shape, resonance, and stability. Optional later: a small recording batch to tune the Cutoff and Q curves and the Peak gain (`docs/GLOBAL_FILTER_RECORDING_PLAN.md`).
- [ ] Add level compensation to the global filter's resonant level (high Q on Lowpass and Highpass currently gets louder). Deferred: decide by ear or from a recording whether to compensate, and by how much.
- [ ] Polish the global filter's Bandreject: above about Q 40-50 the notch is too narrow to hear much change (user listening). Consider a gentler Q curve for Bandreject, or a wider notch at the top of the range.
- [x] Add distortion with Type: Bypass, Drive (`dist_type`, version hint 8), plus Drive and Tone (existing `dist_crush`, `dist_tone` IDs retained). After global filter, before Master. Drive maximum increased from 8x to 10x with modest makeup; Tone now has a 700 Hz split and +/-12 dB treble range. Drive 0 is dry; new instances default to Bypass. Knobs and type transitions are smoothed; older distortion states migrate to Drive, including the retired Bitcrush choice (removed after listening feedback). User-requested original extensions, pending listening feedback.
- [x] Add two parallel mono delay lines (`Source/DSP/ParallelDelay.h`), independently enabled, with shared Mix and wet-only Cut; each has Rate, Decay, and Pan (-100..100, centre mono). Parameters appended at version hint 9; older states disable both lines. Preallocated 48-second buffers, smoothed controls, read-head crossfades, bounded feedback, state-restore reset, and host tail reporting. Dedicated `TeratoamorDelayTests` passes; listening refinement of Cut and Decay remains. See `CHIMERA_REFERENCE.md` for exact initial mappings.
- [x] Sync delay Rates (1/64T through 4/1, straight/triplet/dotted) to host tempo, with 120 BPM fallback when absent. Supported BPM 20..400; tempo and Rate changes crossfade. Future tempo-based features still need their own integration.
- [ ] Add other types of noise.
- [ ] Add potential way of letting user load their own noise.

### Milestone 4: modulation and performance

- [x] Add six modulation sections (`Source/DSP/Modulation.h`, `mod<N>_*` and `master_pan`, version hint 10; `TeratoamorModulationTests`). No editor controls yet: they are reachable through host parameters only. Modulation 1-3 are per-note Attack/Decay/Sustain/Release/Time envelopes (same fitted timing as Elements), 4-6 are tempo-synced oscillators (Off plus 17 wavetables, Soft, Rate 1/64T to 4/1 default 1/16, Gate Trig). Every section has a target dropdown and a bipolar Depth (-100..100); the target lists follow the user's research (envelopes: Elements, All, Filter; oscillators add Distort, Delay, Modulation 1-3 Depth, other oscillators' Rate, Master). Design decisions:
  - Per note: envelopes and Gate Trig oscillators run per voice for Element and All targets. Filter, Distort, Delay, Master and Rate/Depth targets cannot be per voice, so they follow the newest sounding note (or the free-running oscillators alone when nothing sounds). Gate Trig off uses one free-running oscillator per section.
  - Depth scale: the user measured 0.48 semitones per unit on the original (Depth 25 = an octave, 100 = four octaves) but found Depth 100 unplayably extreme on Pitch, so Pitch is scaled to 0.36 semitones per unit (100 = three octaves; the old Depth 75). Every other target moves one control unit per Depth unit (Depth 100 can sweep the whole control); Rate moves 4 octaves per 100 units. Oscillators are bipolar (-1..1) and envelopes 0..1, both times Depth.
  - Soft is a one-pole smoother with a time constant proportional to the period (0.2 x period at Soft 100). Fitted by design, not measured.
  - Wavetable shapes are guesses from their names and the user's screenshots (Saw rises, Pulse100% is a full-swing square starting positive, Random is stepped sample-and-hold). From the wavetable icons: Sine/Triangle start at zero and rise, Saw rises, Ramp falls, Pulse100% is half high/half low starting high, Pulse50% and Pulse25% are high for 75% and 87.5% of the cycle, Hump is a positive-only arch. Still approximations (icons hard to read): Peak, Dip, RipSaw/RipRamp, SharkR/L.
  - Modulation is applied after the parameter smoothers so fast modulation is not blurred, but Distortion and Delay smooth their own inputs (about 10 ms), which softens very fast modulation of those two.
  - Modulation cannot revive an Element that is off or in Off mode (Volume). Old saved projects load with Depth 0 and Wave Off.
- [ ] Add MIDI routing and aftertouch.
- [ ] Add glide.
- [x] Add master pan (`master_pan`, -100..100, same constant-power law as the Elements). Remaining master controls: to be decided.

### Milestone 5: product finish

- [ ] Design and implement a polished, original Teratoamor GUI.
- [ ] Create original Teratoamor presets without Chimera preset compatibility or copied names.
- [ ] Complete DAW compatibility, state, automation, performance, and long-run stability testing.
- [ ] Audit licences, notices, packaging, and the clean-room boundary before distribution.

## Working rules

- Follow `AGENTS.md` and `CHIMERA_REFERENCE.md` for every implementation change.
- Keep changes small enough to review and verify.
- Do not modify the JUCE submodule.
- Keep `build/`, `out/`, `.vscode/`, and `reference/` out of Git.
- Do not commit or push unless explicitly requested.
- After code or build-system changes, run the relevant configure/build command when possible and report both the command and artifacts.
- Explain the purpose of commands and errors for a developer new to C++, CMake, JUCE, and VS Code.
