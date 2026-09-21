# Width comparison findings

## Purpose and method

This is a clean-room behavioural comparison of the permitted Chimera WAV recordings with output rendered from Teratoamor's independently written DSP engine. It does not inspect executable code.

The matched batch contains BP Wide, BP Narrow, Peak Wide, and Peak Narrow at Width 0, 25, 50, 75, 90, and 100. Every file uses 44.1 kHz audio and the same MIDI sequence: notes 36, 60, and 84 at velocity 100, held for four seconds each. Chimera and Teratoamor were both set to Master 100, Element 1 level 80, centred pan, default envelope, zero Warp and Clip, with Elements 2 and 3 off.

SciPy's Welch power-spectrum estimate was measured over a steady 2.8-second region of every note. The comparison uses power relative to the resonant peak at 0.5, 0.9, 1.1, and 2 times the note frequency, plus steady RMS level. The reproducible analysis is `Tests/analyze_width_comparison.py`; its detailed CSV is written beneath the ignored `reference/analysis/` directory.

Four-second notes cannot provide reliable absolute Q estimates when the resonance becomes extremely narrow at Width 100. The findings therefore use direct, matched spectral differences at Width 100 and treat proposed caps as starting estimates for implementation and listening—not exact recovered constants.

## Findings

### BP Wide

BP Wide is already close enough to preserve. From Width 0 through 90, Teratoamor's measured off-frequency power is generally within about 3 dB of Chimera. At Width 100 Teratoamor is only about 3-5 dB narrower at the sampled skirt frequencies. The existing base mapping should not be changed before listening to the final A/B set.

### BP Narrow

The fixed `0.3 * base Q` stage scaling is not valid across the full Width range. Teratoamor is too broad from Width 0 through 75: far-skirt power is about 8-14 dB too high at Width 25-75. Width 90 is close. At Width 100 the direction reverses and Teratoamor is about 13-17 dB too narrow.

A two-cascade-stage model fitted over the resolvable range gives approximate Chimera per-stage Q targets of 1.0, 1.8, 3.0, 8.1, and 37 at Width 0, 25, 50, 75, and 90. Teratoamor's present effective targets are approximately 0.5, 0.53, 1.2, 4.8, and 30. Width 100 should start in the per-stage Q 120-130 range for listening, rather than the current 480, but that endpoint is less certain than the lower values.

### Peak Wide

Peak Wide is close from Width 0 through 90; the sampled spectral differences are generally within about 4 dB. Chimera then changes very little between Width 90 and 100, while Teratoamor continues toward its base maximum Q of 1600. At Width 100 Teratoamor is about 11-13 dB too narrow close to the peak. Peak Wide therefore needs a mode-specific high-end cap, with roughly Q 100-150 as the first listening range. Its dry-noise gain should be rechecked after the cap is applied.

### Peak Narrow

The fixed `0.8 * base Q` stage scaling is also invalid across the full range. Teratoamor is much too broad from Width 0 through 75: far-skirt power is about 14-27 dB too high, with the near skirts also too broad by as much as about 21 dB. Width 90 is closer but its skirt shape still differs. At Width 100 the resonant core is about 24-25 dB too narrow, while the far residual floor is about 3-6 dB too high.

Approximate Chimera per-stage Q targets are 2.8, 7.1, 18, 45, and 100 at Width 0, 25, 50, 75, and 90. Teratoamor currently uses approximately 0.8, 1.4, 3.2, 12.8, and 80. Width 100 should start in the per-stage Q 200-220 range for listening, rather than the current 1280. The `0.003` dry-noise gain must be refitted after correcting the resonance curve.

### Width-dependent level

Teratoamor deliberately normalises filter output toward constant RMS, but Chimera becomes substantially louder as Width rises. Relative to Width 0, the median steady levels across the three notes changed as follows:

| Mode | Instrument | W25 | W50 | W75 | W90 | W100 |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| BP Wide | Chimera | +1.7 dB | +2.6 dB | +6.8 dB | +7.4 dB | +11.5 dB |
| BP Wide | Teratoamor | 0.0 dB | +0.1 dB | +0.3 dB | +1.1 dB | +0.8 dB |
| BP Narrow | Chimera | +3.7 dB | +4.7 dB | +2.7 dB | +7.5 dB | +11.0 dB |
| BP Narrow | Teratoamor | 0.0 dB | +0.1 dB | +0.3 dB | +0.7 dB | +1.0 dB |
| Peak Wide | Chimera | -0.8 dB | -0.3 dB | +3.0 dB | +5.6 dB | +5.6 dB |
| Peak Wide | Teratoamor | 0.0 dB | 0.0 dB | -0.1 dB | +0.6 dB | +0.3 dB |
| Peak Narrow | Chimera | +3.2 dB | +5.4 dB | +10.4 dB | +10.7 dB | +6.3 dB |
| Peak Narrow | Teratoamor | 0.0 dB | +0.1 dB | +0.3 dB | +2.0 dB | -1.7 dB |

The absolute level difference at Master 100 is not by itself a Width defect because Teratoamor's default Master is 70 and its overall level was previously accepted by ear. The missing *change in level with Width* is a real behavioural difference. Whether to reproduce all of that rise or retain some loudness compensation is a product/listening decision, but the current near-constant response should not be mistaken for a match.

## Implemented DSP corrections

The correction pass made these changes in `Source/DSP/ElementFilter.h`:

1. Preserved BP Wide's existing resonance mapping.
2. Replaced both fixed Narrow-mode multipliers with independent log-interpolated per-stage Q curves. BP Narrow uses 1.05, 1.75, 3, 8.1, 37, and 150; Peak Narrow uses 2.8, 7.1, 18, 45, 100, and 180 at the six measured Width points.
3. Capped Peak Wide at Q 100 above Width 90, matching the measured plateau.
4. Refit both Peak-mode dry-noise components after the resonance correction. They are Width-dependent because a single constant could not match the recorded floors across the full range; Peak Narrow also showed correlated cancellation when too much of the same excitation noise was mixed back in.
5. Added per-mode Width-dependent gain curves, anchored at 0 dB correction at Width 90 so the accepted default-patch level is unchanged.

After rerendering all 24 conditions, median sampled skirt errors are generally within about 3.5 dB. The largest remaining median error is 4.8 dB for BP Wide at Width 100, which was deliberately left unchanged pending listening because the four-second reference cannot resolve its extreme Q reliably. The Width-dependent level shapes track Chimera within 0.5 dB at every measured mode and Width. All final renders remain unclipped (hottest peak approximately -6.1 dBFS).

The full Debug build and engine stability suite pass. The remaining task is a focused listening decision using Width 90 and 100 for each active mode.

## Focused Width 90/100 evaluation

Eight 20-second listening files are generated under `reference/analysis/width_ab_listening/` by `Tests/create_width_ab_listening_files.py`. Each file presents Chimera first and Teratoamor second, contains steady excerpts of MIDI notes 36, 60, and 84, and level-matches Teratoamor to Chimera for that pair. No file clips.

The final objective comparison supports keeping the corrected DSP unchanged:

- BP Narrow's sampled skirts differ by at most 3.5 dB at Width 90 and 1.3 dB at Width 100.
- Peak Wide differs by at most 1.7 dB at Width 90 and 1.8 dB at Width 100.
- Peak Narrow differs by at most 2.3 dB at Width 90 and 2.6 dB at Width 100.
- BP Wide differs by up to 3.2 dB at Width 90 and 4.8 dB at Width 100. No further change is justified from the four-second recording because the Width-100 resonance is narrower than the recording can resolve reliably.
- The relative Width-dependent level shape remains within 0.5 dB for every mode and measured Width.

This completes the repeatable objective A/B evaluation. A human playback judgment remains useful for subjective texture and preference; it is not represented as completed by spectral analysis alone.

These changes must retain finite output and the existing safety clamp across all supported pitches and sample rates.

## Listening feedback and high-frequency correction

A first listening pass found: BP Narrow 90/100 close; BP Wide 90/100 with slightly more high end in Chimera; Peak Wide 90/100 almost identical; and Peak Narrow with clearly audible noise in Teratoamor where Chimera has a faint hiss (Width 90) or none (Width 100). The earlier skirt metrics (0.5-2x the note frequency) could not see this, so a second measurement compares the tone with the 6-16 kHz floor and the spectrum in octave bands out to 64x the note frequency.

- **Peak Narrow.** Chimera's floor is flat above about 2x the note frequency, independent of pitch, and falls steadily with Width (tone-to-floor about 10, 20, 33, 44, 55 and 60 dB at Width 0, 25, 50, 75, 90 and 100). The previous fit was 40 dB too clean at Width 0 and 10-14 dB too noisy at Width 90/100. Cause: the dry noise was fitted only near the note. Its fix has two parts: the dry gain curve was refitted to the measured floor (0.53 to 0.0014), and the dry noise is now *subtracted* from the two-stage cascade, because adding it cancelled the skirts (a 10 dB dip 2-4x above the note). Tone-to-floor now matches within about 1 dB at every Width; near-peak skirt errors are median 0.8-4.6 dB, worst case 8.6 dB (Width 90, low side).
- **BP Wide.** Chimera's skirts change by only 3-4 dB between Width 90 and 100, so BP Wide is capped at Q 400 (as Peak Wide is at 100). Width 100 median skirt error improved from 4.8 to 3.7 dB and the high-side skirts moved 6-13 dB closer.
- **BP Narrow.** The 6-16 kHz band is at the 16-bit quantisation floor in both instruments; no change.
- **Not addressed.** BP Wide Width 90 note 60 shows Chimera 11-13 dB stronger 0.1-0.25x below the note; the tone level of Chimera also varies with pitch at high Width (Teratoamor's is deliberately constant). Neither was audible in the first pass, so they were left alone.
