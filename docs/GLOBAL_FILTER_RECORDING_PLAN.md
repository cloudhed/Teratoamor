# Global filter recording plan

> Status: optional. A basic global filter is already implemented from standard filter behaviour (Bypass, Lowpass, Highpass, Bandpass, Bandreject, Peak). Use this only to tune its Cutoff and Q curves later. A reduced batch is enough: Cutoff 25, 50, 75 at Q 0, plus Q 50 and 100 at Cutoff 50, for Lowpass and Peak only.

Clean-room behavioural capture of Chimera's global filter (the FILTER panel: Type, Q, Cutoff). Recordings only; no executable inspection.

## Why this setup

The global filter processes the mix of the Elements. Feeding it plain white noise shows its response directly, with no resonant Element filter in the way. Using three notes also shows whether Cutoff follows pitch.

## Fixed settings for every recording

- 44.1 kHz, 16-bit or better, stereo, same as the Width batch.
- MIDI: `Tests/MIDI/Teratoamor_Width_Listening_Test.mid` (notes 36, 60, 84, velocity 100, four seconds each).
- Master Volume 100, Master Pan 0.
- Element 1: filter **Bypass**, Width any, Warp 0, Clip 0, Octave 0, Semi 0, Detune 0, Pan 0, Vol 100, envelope at defaults (H 0, A 0, D 0, S 100, R at default, T 50).
- Elements 2 and 3 **off** (or Vol 0).
- Distort: **Off**. Delay Mix **0** and both delays **off**.
- No modulation depth, no MIDI CC or aftertouch routing.

## Step 0: list the filter types

Click through the Filter Type selector and write down every name in order (Bypass is the default; there may be Lowpass, Highpass, Bandpass, Notch, and so on). Also note the highest and lowest values shown for Q and Cutoff. Put the list in the first message when you upload.

## Batch A: Cutoff sweep, Q 0

For every type except Bypass: Cutoff 0, 25, 50, 75, 100 at Q 0. Five files per type.

File name: `gfilter_<type>_cut_<000|025|050|075|100>_q_000.wav`

## Batch B: Q sweep, Cutoff 50

For every type except Bypass: Q 25, 50, 75, 100 at Cutoff 50. Four files per type.

File name: `gfilter_<type>_cut_050_q_<025|050|075|100>.wav`

## Baseline

- `gfilter_bypass.wav`: Type Bypass, Cutoff 100, Q 0.

## Where to put them

`reference/chimera/sample batches/chimera_global_filter/` (this path is git-ignored).

## What I will measure

- Filter shape per type, from the spectrum of white noise (Welch estimate).
- Cutoff frequency versus knob position, and whether it follows note pitch.
- Slope (pole count) and how Q changes peak height and bandwidth.
- Level change with Cutoff and Q, and how close to self-oscillation Q 100 gets.
