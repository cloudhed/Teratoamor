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
- [ ] Verify that the standalone application starts and accepts MIDI input.

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
- [ ] Build a simple, functional, original JUCE GUI for the Milestone 1 controls.
- [ ] Avoid Chimera branding, artwork, silhouette, layout trade dress, and preset names.

### Milestone 1 verification

- [ ] Exercise MIDI note-on, note-off, overlapping notes, and voice stealing.
- [ ] Test common sample rates and audio buffer sizes.
- [ ] Confirm the real-time audio path performs no allocation and takes no locks.
- [ ] Stress Width near 100 across the supported pitch range.
- [ ] Confirm silence and finite output after resets, rapid parameter changes, and state restoration.
- [ ] Compare pitch, Width response, envelope timing, and stereo behaviour with permitted clean-room measurements.
- [x] Confirm saved sessions restore the audible state.

## Later milestones

### Milestone 2: Element character

- [ ] Add the remaining Element filter modes: Off, Bypass, BP Narrow, Peak Wide, and Peak Narrow.
- [ ] Implement and refine Warp as an original asymmetric waveshaping stage.
- [ ] Implement and refine Clip, including the strong Clip 100 behaviour.
- [ ] Add Hold, Decay, Sustain, and Time behaviour as measurements justify it.
- [ ] Add Element 2 and 3 Link behaviour after it is cleanly specified.

### Milestone 3: global processing

- [ ] Add the global multimode filter.
- [ ] Add distortion.
- [ ] Add two delay lines.
- [ ] Add tempo synchronisation where required.

### Milestone 4: modulation and performance

- [ ] Add six modulation sections with an original, maintainable routing design.
- [ ] Add MIDI routing and aftertouch.
- [ ] Add glide.
- [ ] Add master pan and remaining master controls.

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
