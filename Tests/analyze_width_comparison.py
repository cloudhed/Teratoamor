"""Matched spectral analysis for the Chimera/Teratoamor Width recordings.

Requires NumPy and SciPy. Run from the repository root with the isolated research
environment documented in docs/WIDTH_COMPARISON.md.
"""

from __future__ import annotations

import argparse
import csv
import pathlib
import re
import wave

import numpy as np
from scipy.optimize import least_squares
from scipy.signal import welch


NOTES = (36, 60, 84)
WIDTHS = (0, 25, 50, 75, 90, 100)
MODES = ("bpwide", "bpnarrow", "peakwide", "peaknarrow")
RATIOS = (0.5, 0.9, 1.1, 2.0)


def read_mono(path: pathlib.Path) -> tuple[int, np.ndarray]:
    with wave.open(str(path), "rb") as source:
        sample_rate = source.getframerate()
        channels = source.getnchannels()
        samples = np.frombuffer(source.readframes(source.getnframes()), dtype="<i2")
    return sample_rate, samples.reshape(-1, channels).mean(axis=1) / 32768.0


def band_power(frequencies: np.ndarray, power: np.ndarray, frequency: float, bin_hz: float) -> float:
    half_width = max(2.0 * bin_hz, 0.006 * frequency)
    selected = power[np.abs(frequencies - frequency) <= half_width]
    return float(np.mean(selected))


def fit_equivalent_q(
    segment: np.ndarray, sample_rate: int, f0_nominal: float, mode: str, width: int
) -> tuple[float, float, float]:
    """Fit a one-stage or two-cascade analogue-shape model to the measured PSD.

    For Narrow modes the returned Q is the equivalent Q of each identical stage.
    At Width 100 the finite recording length limits this estimate; direct skirt
    measurements remain the authoritative matched comparison there.
    """
    frequencies, power = welch(
        segment,
        fs=sample_rate,
        window="hann",
        nperseg=65536,
        noverlap=49152,
        detrend=False,
    )
    selected = (frequencies > max(10.0, 0.3 * f0_nominal)) & (
        frequencies < min(0.47 * sample_rate, 3.0 * f0_nominal)
    )
    fit_frequencies = frequencies[selected]
    fit_power = power[selected]
    stages = 2 if "narrow" in mode else 1

    def residual(log_values: np.ndarray) -> np.ndarray:
        f0, q, amplitude, floor = np.exp(log_values)
        ratio = fit_frequencies / f0
        shape = (1.0 / (1.0 + (q * (ratio - 1.0 / ratio)) ** 2)) ** stages
        predicted = amplitude * shape + floor
        return np.log(predicted + 1.0e-30) - np.log(fit_power + 1.0e-30)

    floor_guess = max(float(np.percentile(fit_power, 10)), 1.0e-22)
    amplitude_guess = max(float(np.percentile(fit_power, 99)) - floor_guess, floor_guess)
    q_guess = {0: 1.0, 25: 2.0, 50: 4.0, 75: 16.0, 90: 100.0, 100: 500.0}[width]
    result = least_squares(
        residual,
        np.log((f0_nominal, q_guess, amplitude_guess, floor_guess)),
        bounds=(
            np.log((0.94 * f0_nominal, 0.2, 1.0e-24, 1.0e-26)),
            np.log((1.06 * f0_nominal, 1.0e5, 10.0, 10.0)),
        ),
        loss="soft_l1",
        f_scale=0.6,
        max_nfev=3000,
    )
    _, q, amplitude, floor = np.exp(result.x)
    fit_error = float(np.sqrt(np.mean(residual(result.x) ** 2)))
    return float(q), float(10.0 * np.log10(floor / amplitude + 1.0e-30)), fit_error


def analyze_file(path: pathlib.Path, instrument: str) -> list[dict[str, float | int | str]]:
    match = re.search(r"_(bpwide|bpnarrow|peakwide|peaknarrow)_width_(\d+)", path.name)
    if match is None:
        return []

    mode, width_text = match.groups()
    width = int(width_text)
    sample_rate, audio = read_mono(path)

    # Teratoamor has an exact one-second pre-roll. The Chimera captures include
    # roughly another 0.5-1.2 seconds before that timeline; these conservative
    # fixed windows remain inside all three steady note regions in every take.
    starts = (2.5, 7.5, 12.5) if instrument == "Chimera" else (1.65, 6.65, 11.65)
    rows: list[dict[str, float | int | str]] = []

    for start, note in zip(starts, NOTES):
        segment = audio[int(start * sample_rate) : int((start + 2.8) * sample_rate)]
        frequencies, power = welch(
            segment,
            fs=sample_rate,
            window="hann",
            nperseg=32768,
            noverlap=24576,
            detrend=False,
        )
        f0 = 440.0 * 2.0 ** ((note - 69) / 12.0)
        center = band_power(frequencies, power, f0, sample_rate / 32768.0)
        equivalent_q, floor_relative_db, fit_error = fit_equivalent_q(
            segment, sample_rate, f0, mode, width
        )
        row: dict[str, float | int | str] = {
            "instrument": instrument,
            "mode": mode,
            "width": width,
            "midi_note": note,
            "rms_dbfs": 20.0 * np.log10(np.sqrt(np.mean(segment * segment)) + 1.0e-30),
            "equivalent_stage_q": equivalent_q,
            "fitted_floor_relative_db": floor_relative_db,
            "fit_rms_log_error": fit_error,
        }
        for ratio in RATIOS:
            measured = band_power(frequencies, power, f0 * ratio, sample_rate / 32768.0)
            row[f"power_at_{ratio:g}f0_db"] = 10.0 * np.log10(measured / center + 1.0e-30)
        rows.append(row)

    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        default=pathlib.Path("reference/analysis/width_comparison.csv"),
    )
    args = parser.parse_args()

    folders = {
        "Chimera": pathlib.Path("reference/chimera/sample batches/chimera_width_across_all_elements"),
        "Teratoamor": pathlib.Path("reference/teratoamor/width_across_all_elements"),
    }
    rows: list[dict[str, float | int | str]] = []
    for instrument, folder in folders.items():
        for path in sorted(folder.glob("*.wav")):
            rows.extend(analyze_file(path, instrument))

    expected = 2 * len(MODES) * len(WIDTHS) * len(NOTES)
    if len(rows) != expected:
        raise RuntimeError(f"Expected {expected} measurements, found {len(rows)}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as destination:
        writer = csv.DictWriter(destination, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(f"Wrote {len(rows)} matched measurements to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
