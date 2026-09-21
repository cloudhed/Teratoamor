# Chimera clean-room reference

## Purpose and boundary

This document records behavioural observations of Majken's Chimera that may guide an independent implementation of Teratoamor. These observations are a reference for behaviour, not a source-code specification. Teratoamor must use original code, naming, graphics, presets, and visual design.

The original Chimera is a 32-bit Windows VST2 built with SynthEdit. Its package includes compiled Chris Kerry and David Haupt SynthEdit modules. Their code must not be extracted, copied, translated, modified, or reused.

Permitted local research sources under the ignored `reference/chimera/` directory are:

- Recorded WAV output
- The VSTXML host-parameter export
- FXB/FXP metadata
- The screenshot
- Existing documented observations and new black-box behavioural measurements

The following are explicitly outside the clean-room boundary:

- Disassembly, decompilation, translation, or executable-code inspection of `Majken's Chimera.dll`
- Inspection of executable code in `.SEP` or `.SEM` modules
- Inspection of extracted PE sections, including `.text`, `.rdata`, `.data`, `.rsrc`, and `.reloc`
- Delegating any prohibited analysis to another person or agent
- Reusing Chimera code, binary code, branding, GUI artwork, the human silhouette, preset names, proprietary presets, or trade dress

All material in `reference/` is local research material. It must never be committed or distributed.

## Observed instrument structure

Chimera creates pitched sounds by feeding white noise through pitch-controlled resonant filters. Its interface contains:

- Three sound Elements
- Per-Element filter type
- Width/resonance
- Detune
- Octave and semitone tuning
- Warp
- Clip on applicable Elements
- Hold, Attack, Decay, Sustain, Release, and Time controls (Teratoamor omits Hold)
- Per-Element pan and volume
- Link controls for Elements 2 and 3
- A global multimode filter
- Distortion
- Two delay lines
- Six modulation sections
- MIDI routing and aftertouch
- Master BPM/synchronisation, glide, volume, and pan

The VST exposes 110 host parameters.

Observed Element filter selections are:

- Off
- Bypass
- BP Wide
- BP Narrow
- Peak Wide
- Peak Narrow

## Factory-bank metadata

The exported factory bank reports 64 program slots:

- 50 authored sounds
- 4 category-marker programs
- 10 unused `Default` programs

Preset compatibility is not required for the initial implementation. Chimera preset names must not be reproduced in Teratoamor's shipped content.

## Measured default patch

For the reference default patch:

- MIDI note 60 produces approximately 261.39 Hz.
- The measured frequency corresponds to MIDI pitch 59.98, effectively exact middle C.
- Element output is identical in the left and right channels before panning.
- Recorded peak level was approximately -14.8 dBFS.
- Element 1 uses `BP Wide` with Width 90.
- Elements 2 and 3 are off.
- The global filter is bypassed.
- Distortion and both delays are off.

These levels were measured from the available recordings and are reference observations, not a requirement to copy hidden implementation details.

## Width and resonance

Measured Width behaviour:

| Width | Approximate Q |
| ---: | ---: |
| 0 | 1 |
| 25 | 2 |
| 50 | 4 |
| 75 | 16 |
| 90 | 98 |
| 100 | Extremely narrow; approaches self-oscillation |

A close measured mapping is:

```text
normalisedWidth = width / 100
Q = 1 / (1 - normalisedWidth)^2
```

The implementation must clamp the denominator or maximum Q safely near Width 100. Exact limits should be chosen through stability tests across supported sample rates and pitches; no filter may emit NaN or infinity.

## Filter observations

- `Bypass` produces essentially unfiltered white noise.
- `BP Narrow` is extremely tonal.
- `Peak Narrow` is dominated heavily by the fundamental.
- `Peak Wide` retains more broadband noise.
- `BP Wide` at Width 90 measured near Q 98.

Spectral measurements of `02_filter_*.wav` (44.1 kHz, note 60, Welch average; the Width used for each recording is not recorded, and Width 90 is assumed because Peak Wide's skirts match the default patch):

| Mode | Level at 0.9 f0 | Level at 0.5 f0 | Level 5 kHz | Reading |
| --- | ---: | ---: | ---: | --- |
| BP Wide (default) | -25 dB | -42 dB | -63 dB | 2-pole band-pass, Q about 98 |
| BP Narrow | -29 dB | -66 dB | -100 dB | 4-pole: skirts fall about twice as fast; stage Q about 29 if two identical stages |
| Peak Wide | -23 dB | -40 dB | -48 dB | same skirts as BP Wide plus a flat noise floor about 46 dB below the peak |
| Peak Narrow | -50 dB | -83 dB | -90 dB | 4-pole, stage Q about 80; noise floor at or below -89 dB (may be recording-limited) |
| Bypass | n/a | n/a | n/a | flat white noise, RMS about -29 dBFS, matching the default patch level |

Hypotheses implemented (labelled uncertain): Narrow modes are two cascaded band-passes, but the September 2026 matched Width batch showed that fixed stage-Q multipliers were inaccurate away from Width 90. BP Narrow and Peak Narrow now use independent log-interpolated per-stage Q curves at Width 0, 25, 50, 75, 90, and 100. Peak Wide uses the ordinary Width mapping through 90 and caps Q at 100 above it. Peak modes add a small, Width-dependent unfiltered-noise component fitted after the Q curves. Off silences the Element. The Off behaviour is an assumption, since no recording of it exists. Full measurements and implementation targets are recorded in `WIDTH_COMPARISON.md`.

These descriptions identify audible targets. Filter topology and numerical implementation must be independently designed.

## Warp observations

- Increasing Warp primarily introduces even harmonics.
- At Warp 100, the second harmonic is approximately 6 dB below the fundamental.
- A suitable independent starting point is a controllable asymmetric waveshaping stage, refined with listening tests and output measurements.
- Measured from `03_warp_*.wav` (note 60, Element 1, Width 90): second harmonic relative to the fundamental is -18.7, -14.9, -10.7, -8.1 dB at Warp 25, 50, 75, 100, roughly linear in amplitude. The fourth and sixth harmonics rise too, while odd harmonics stay weak. The recordings have no DC offset and about the same level (within 1.5 dB) as unwarped output.
- The parameter export lists Warp on all three Elements.

## Clip observations

- Values 25 through 75 are restrained at the default signal level.
- Clip 100 behaves like strong hard clipping.
- Clip 100 produces prominent odd harmonics and an approximately 3 dB crest factor.

- Measured from `04_clip_*.wav`: Clip 25, 50, and 75 show no added harmonics (level within 2.4 dB of unclipped). Clip 100 is about 12 dB louder, peaks near -10.3 dBFS with a 3.0 dB crest factor, has a third harmonic near -13 dB, a fifth near -25 dB, and a smaller second harmonic near -27 dB.
- The parameter export lists Clip on Elements 1 and 3 only; Element 2 has none.
- Teratoamor implements drive (14 dB x Clip, linear) followed by a hard clip at 5.7 times the filter's RMS output, with the output scaled back by the calculated post-clip RMS (80% compensation), so Clip 100 is about +1.6 dB louder rather than the reference +12 dB. Clip 100 is fitted to the numbers above; the earlier onset (audible from about Clip 50 instead of about 90) is a deliberate departure chosen by ear.

The transfer function must be independently designed and compared against behavioural recordings.

## Envelope observations

Attack and Release appear to map approximately linearly from the control value to time:

| Value | Approximate time |
| ---: | ---: |
| 25 | 2.5 seconds |
| 50 | 5 seconds |
| 75 | 7.5 seconds |
| 100 | 10 seconds |

Additional observations:

- Default Release 5 is approximately 0.5 seconds.
- The Attack shape is strongly curved, approximately cubic rather than a linear gain ramp.
- Full reference recordings are available locally for later clean-room measurement.

Default envelope settings: Hold 0, Attack 0, Decay 0, Sustain 100, Release 5, Time 50.

Link (Elements 2 and 3 only, per the user): a checkbox. When on, the Element uses Element 1's Attack, Decay, Sustain, Release, and Time; when off it uses its own.

Time (`07_time_*.wav`, with the default recording as Time 50; everything else default): pitch (MIDI 60.05, 59.92, 60.01) and spectral skirts are unchanged. The release tail measures roughly 0.05 s, 0.45 s, and 0.8 s at Time 0, 50, and 100, about a Time/50 multiplier on the 0.5 s default Release. With Release 25 the tail scales the same way (about 0, 2.3 s, 4.7 s), so Time multiplies Release by about Time/50. The Time 100 file also has a soft onset (about 0.1 s), assumed to be MIDI keypress timing rather than Time. With Decay 50 and Sustain 0 the note fades away in about 0.5 s at Time 0, so Time scales Decay by the same factor (about 0.1 at Time 0).

Decay and Sustain (`09_decay_*.wav`, `10_sustain_*.wav`, `11_combined_d030_s040.wav`; Filter Bypass so the level is steady white noise, default Time 50):

- Decay length is linear in the control: with Sustain 0 the note fades to silence in 2.6, 5.2, 7.8, and 10.4 s at Decay 25, 50, 75, and 100, or 0.104 s per unit.
- The amplitude during Decay follows (1 - t/T)^p, with p between 1.85 and 2.0 (about 0.25 dB RMS fit error). Equivalent view: a control value u falls linearly from 1 to the Sustain setting, and the output is u^p.
- Sustain settles at that same power law: 25, 40, 50, and 75 settle at -22.6, -14.9, -11.0, and -4.3 dB relative to the first half second (which already includes a little decay), about Sustain^1.95 in amplitude.
- The time to reach the sustain level is the Decay time regardless of the Sustain value (Sustain 25, 50, and 75 with Decay 25 all settle at about 2.6 s).
- Hold above 0 did not play at all in the reference (reported from the user's testing), so it was not measured and Teratoamor has no Hold control.

The factory program data is an opaque binary chunk and was not decoded. Not yet measured: whether Time affects Attack, and the exact Release curve shape.

## Measurement practice

When refining behaviour:

1. Change one control at a time and record the test conditions, including sample rate, MIDI note, velocity, and level settings.
2. Compare observable output such as spectrum, amplitude envelope, pitch, stereo relationship, and crest factor.
3. Record conclusions here as measurements or hypotheses; label uncertainty clearly.
4. Implement from general DSP knowledge without examining prohibited executable code.
5. Use original Teratoamor names and visual presentation for all shipped features and presets.

## Distortion listening observation

User listening feedback (2026-09-21): one distortion type, with Crush and Tone controls. Maximum Crush feels closer to halfway on a distortion pedal: coloration rather than severe destruction. No distortion transfer function or Tone response has been measured.

Teratoamor's first independent approximation uses gentle symmetric soft saturation after the global filter and before Master. The control is named Drive in Teratoamor (the host ID remains `dist_crush` for compatibility). Drive 0 bypasses the entire stage; Tone 50 is neutral, with a broad dark-to-bright treble shelf. These mappings, routing, defaults, and bypass behaviour are implementation choices awaiting listening feedback, not measured reference facts.

Follow-up Teratoamor listening request: slightly stronger Drive, stronger Tone extremes, and Type choices Bypass, Drive, Bitcrush. These are deliberate product changes, not new Chimera observations. Drive maximum is now 10x rather than 8x; Tone uses a 700 Hz split and +/-12 dB treble gain at full Drive. Bitcrush is independent amplitude quantisation (16 to 4 bits), with Drive controlling depth and Tone shaping its output. Type changes crossfade; new instances start bypassed, and older distortion states select Drive.

Subsequent listening decision: remove Bitcrush; it does not suit this synth. Teratoamor now offers only Bypass and Drive, retaining the stronger Drive and Tone ranges. Saved patches using the retired Bitcrush index load as Drive.

## Delay observations and initial implementation

User description and supplied screenshot (2026-09-21): a shared Mix and cut slider; cut 50 is unchanged, 0 is high-cut, 100 is low-cut, affecting only the delays. Two identical, independently enabled delays run in parallel, each with Rate (1/64T through 4/1), Decay 0..100, and Pan. User clarified Pan as -100 full left, 0 centre/mono, +100 full right. The permitted VSTXML export independently confirms shared Delay Filter/Mix and per-line Decay, On/Off, Pan, and Time parameters. No filter curves, feedback gains, or full intermediate Rate list have been measured.

Teratoamor implementation choices, awaiting listening feedback:

- After distortion and before Master: stereo dry path plus two independent mono delay sends (average of left and right), panned on return. Centre echoes are identical in both channels; dry stereo is preserved.
- Shared Mix is a linear dry/wet crossfade. Both lines off bypasses the entire block. Two active returns are averaged to avoid doubling identical echoes.
- Rate uses 26 ascending straight, triplet (T), and dotted (D) note lengths across the requested endpoints. Time is quarter-note beats times 60/BPM, independent of time signature. Host tempo is read each block; absent/invalid tempo falls back to 120 BPM. Supported tempo is 20..400 BPM, with 48 seconds of preallocated history per line supporting 4/1 at 20 BPM. Standalone currently uses the fallback tempo.
- Decay maps linearly to feedback 0..0.95: zero gives one echo, 100 gives long but fading repeats. It is not infinite hold.
- Cut is outside the feedback loops and affects only the summed wet return. Below 50, a two-pole low-pass sweeps 20 kHz to 500 Hz, blended progressively into the return; above 50, a two-pole high-pass sweeps 20 Hz to 2 kHz with the same blend. Exactly 50 bypasses filtering. Endpoints are estimates for listening, not measurements.
- Continuous controls and on/off are smoothed over approximately 10 ms. Rate/tempo changes use a 20 ms read-head crossfade. Turning a line off fades it out and invalidates its history, so re-enabling does not recall old echoes. Reset and state restoration clear delay history without allocating or clearing long buffers on the audio thread.
- New and older patches start with both lines off, Mix 25, Cut 50, each Rate 1/4, Decay 35, Pan 0. Parameter IDs use `delay_mix`, `delay_cut`, and `delay1_`/`delay2_` with `on`, `rate`, `decay`, and `pan` suffixes (version hint 9).

`TeratoamorDelayTests` verifies timing, fractional delay interpolation, parallel routing, both pan endpoints, independent on/off, Mix, wet-only filter response, rate-change smoothing, block-size independence, maximum delay length, finite output at five sample rates, resets, audio-thread allocation count, and engine routing. Cut strength and Decay feel remain for human listening.
