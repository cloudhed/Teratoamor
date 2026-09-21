"""Create focused, level-matched Width 90/100 Chimera-to-Teratoamor A/B WAVs."""

from __future__ import annotations

import pathlib
import wave

import numpy as np


SAMPLE_RATE = 44100
MODES = ("bpwide", "bpnarrow", "peakwide", "peaknarrow")
WIDTHS = (90, 100)


def read_stereo(path: pathlib.Path) -> np.ndarray:
    with wave.open(str(path), "rb") as source:
        if (source.getframerate(), source.getnchannels(), source.getsampwidth()) != (SAMPLE_RATE, 2, 2):
            raise RuntimeError(f"Unexpected WAV format: {path}")
        return np.frombuffer(source.readframes(source.getnframes()), dtype="<i2").reshape(-1, 2).astype(np.float64) / 32768.0


def steady_notes(audio: np.ndarray, starts: tuple[float, float, float]) -> list[np.ndarray]:
    return [audio[int(start * SAMPLE_RATE) : int((start + 2.8) * SAMPLE_RATE)] for start in starts]


def write_stereo(path: pathlib.Path, audio: np.ndarray) -> None:
    pcm = np.rint(np.clip(audio, -1.0, 1.0) * np.where(audio < 0.0, 32768.0, 32767.0)).astype("<i2")
    with wave.open(str(path), "wb") as destination:
        destination.setnchannels(2)
        destination.setsampwidth(2)
        destination.setframerate(SAMPLE_RATE)
        destination.writeframes(pcm.tobytes())


def main() -> int:
    chimera = pathlib.Path("reference/chimera/sample batches/chimera_width_across_all_elements")
    teratoamor = pathlib.Path("reference/teratoamor/width_across_all_elements")
    output = pathlib.Path("reference/analysis/width_ab_listening")
    output.mkdir(parents=True, exist_ok=True)

    short_gap = np.zeros((int(0.3 * SAMPLE_RATE), 2))
    long_gap = np.zeros((SAMPLE_RATE, 2))
    edge = np.zeros((int(0.5 * SAMPLE_RATE), 2))

    for mode in MODES:
        for width in WIDTHS:
            suffix = f"{mode}_width_{width:03d}.wav"
            source_a = read_stereo(chimera / f"chimera_{suffix}")
            source_b = read_stereo(teratoamor / f"teratoamor_{suffix}")
            notes_a = steady_notes(source_a, (2.5, 7.5, 12.5))
            notes_b = steady_notes(source_b, (1.65, 6.65, 11.65))

            rms_a = np.sqrt(np.mean(np.concatenate(notes_a) ** 2))
            rms_b = np.sqrt(np.mean(np.concatenate(notes_b) ** 2))
            gain_b = rms_a / rms_b
            notes_b = [note * gain_b for note in notes_b]

            section_a = np.concatenate((notes_a[0], short_gap, notes_a[1], short_gap, notes_a[2]))
            section_b = np.concatenate((notes_b[0], short_gap, notes_b[1], short_gap, notes_b[2]))
            combined = np.concatenate((edge, section_a, long_gap, section_b, edge))
            destination = output / f"AB_chimera_then_teratoamor_{mode}_width_{width:03d}.wav"
            write_stereo(destination, combined)
            print(f"{destination} (Teratoamor level match {20.0 * np.log10(gain_b):+.2f} dB)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
