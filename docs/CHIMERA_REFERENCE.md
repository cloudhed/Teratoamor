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
- Hold, Attack, Decay, Sustain, Release, and Time controls
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

Hypotheses implemented (labelled uncertain): Narrow modes are two cascaded band-passes with stage Q equal to 0.3 x (BP Narrow) or 0.8 x (Peak Narrow) of the Width-derived Q; Peak modes add unfiltered noise (gain 0.36 wide, 0.003 narrow, relative to unit-RMS noise); Off silences the Element. The Off behaviour is an assumption, since no recording of it exists.

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

Time (`07_time_000.wav`, `07_time_100.wav`, with the default recording as Time 50; everything else default): pitch (MIDI 60.05, 59.92, 60.01) and spectral skirts are unchanged. The release tail measures roughly 0.05 s, 0.45 s, and 0.8 s at Time 0, 50, and 100, about a Time/50 multiplier on the 0.5 s default Release. The Time 100 file also has a soft onset (about 0.1 s), assumed to be MIDI keypress timing rather than Time. Further recordings (`07_time_{000,050,100}_release_025.wav` and `07_time_{000,050,100}_decay_050_sustain_000.wav`): with Release 25 the tail scales the same way (about 0, 2.3 s, 4.7 s at Time 0, 50, 100), so Time is a multiplier of about Time/50 on Release. With Decay 50 and Sustain 0 the note fades away in about 0.5 s at Time 0, and is still fading at about 3.5 s (Time 50) and about 4 s (Time 100), where the key was presumably released; fitted linear decay lengths are about 0.45 s, 3-3.5 s, and 6-7 s, so Time scales Decay by the same factor and Decay 50 is about 3.3 s, not 5 s. The level readings are noisy (about +/-30% from the narrowband resonator), so the decay shape (linear versus exponential) is not settled. Not yet known: whether Time affects Attack or Hold.

Hold, Decay, and Sustain on their own (listed per Element in the VSTXML export) have no recordings yet; Decay was only seen through the Time recordings above. The factory program data is an opaque binary chunk and was not decoded. Teratoamor's Hold, Decay, and Sustain are provisional assumptions. Recordings that would settle this (default patch, Element 1, note 60, held about 12 seconds, then released; change one control at a time):

- `07_hold_025/050/075/100`: Hold only.
- `08_decay_025/050/075/100`: Decay only, with Sustain 0 and Hold 0.
- `09_sustain_000/025/050/075`: Sustain only, with Decay about 50.
- Time with Hold at 50 (to see whether Time scales Hold), and the decay shape with the note held about 12 seconds so the whole fall is visible.

## Measurement practice

When refining behaviour:

1. Change one control at a time and record the test conditions, including sample rate, MIDI note, velocity, and level settings.
2. Compare observable output such as spectrum, amplitude envelope, pitch, stereo relationship, and crest factor.
3. Record conclusions here as measurements or hypotheses; label uncertainty clearly.
4. Implement from general DSP knowledge without examining prohibited executable code.
5. Use original Teratoamor names and visual presentation for all shipped features and presets.

