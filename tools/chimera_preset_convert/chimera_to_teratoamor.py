"""Convert Chimera parameter dumps into local Teratoamor preset files.

Input is the CSV written by tools/vst2_param_dump (one row per program and
parameter, read from the plug-in through the public VST2 interface). Output is
one .teratoamor file per program plus a Markdown report of everything that could
not be translated exactly.

The value mappings below come from black-box measurements: notes were rendered
through Chimera with one control swept at a time, and pitch, spectra, echo times,
and modulation periods were measured from the audio. See the "Factory-program
parameter mappings" section in docs/CHIMERA_REFERENCE.md.

The converted presets contain Chimera's factory sounds and names. They are for
local A/B comparison only and must never be committed or distributed.

Usage (from the repository root):
    python tools/chimera_preset_convert/chimera_to_teratoamor.py \
        reference/chimera/param_dump/builtin.csv reference/chimera/converted_presets
"""

from __future__ import annotations

import argparse
import csv
import pathlib
import re
import struct
from collections import defaultdict
from xml.sax.saxutils import quoteattr

# ---------------------------------------------------------------------------
# Teratoamor side: parameter defaults (Source/Parameters/Parameters.cpp).
# ---------------------------------------------------------------------------

DELAY_RATES = [
    "1/64T", "1/64", "1/32T", "1/64D", "1/32", "1/16T", "1/32D", "1/16", "1/8T",
    "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/2T", "1/4D", "1/2", "1/1T", "1/2D",
    "1/1", "2/1T", "1/1D", "2/1", "4/1T", "2/1D", "4/1",
]  # Source/DSP/DelayRates.h, index = stored choice

MOD_TARGETS = [
    "Element1 Width", "Element1 Pitch", "Element1 Pan", "Element1 Warp", "Element1 Clip",
    "Element2 Width", "Element2 Pitch", "Element2 Pan", "Element2 Warp",
    "Element3 Width", "Element3 Pitch", "Element3 Pan", "Element3 Warp",
    "All Width", "All Pitch", "All Pan", "All Volume", "All Warp",
    "Distort Drive", "Distort Tone", "Filter Cutoff", "Filter Q",
    "Delay Pan", "Delay Filter",
    "Modulation1 Depth", "Modulation2 Depth", "Modulation3 Depth",
    "Modulation4 Rate", "Modulation5 Rate", "Modulation6 Rate",
    "Master Pan", "Master Volume",
    "Element1 Volume", "Element2 Volume", "Element3 Volume",
]  # Source/DSP/EngineParams.h, ModTargets::names


def mod_target_list(section: int) -> list[str]:
    """Mirror of ModTargets::listFor (section 0..5)."""
    items = []
    filter_q = MOD_TARGETS.index("Filter Q")
    el1_volume = MOD_TARGETS.index("Element1 Volume")
    own_rate = MOD_TARGETS.index("Modulation4 Rate") + (section - 3)
    for i, name in enumerate(MOD_TARGETS):
        if section < 3:
            if name in ("Distort Drive", "Distort Tone") or filter_q < i < el1_volume:
                continue
        elif i == own_rate:
            continue
        items.append(name)
    return items


def teratoamor_defaults() -> dict[str, float]:
    d: dict[str, float] = {
        "master_level": 70, "gf_type": 0, "gf_q": 0, "gf_cutoff": 100,
        "dist_crush": 0, "dist_tone": 50, "dist_type": 0,
        "delay_mix": 25, "delay_cut": 50, "master_pan": 0,
        "master_glide": 0, "master_glide_mode": 0, "master_sync": 1, "master_bpm": 120,
    }
    for e in (1, 2, 3):
        p = f"el{e}_"
        d.update({p + "on": 1 if e == 1 else 0, p + "filter": 2, p + "octave": 0, p + "semitone": 0,
                  p + "fine": 0, p + "width": 90, p + "warp": 0, p + "level": 80, p + "pan": 0,
                  p + "attack": 0, p + "decay": 0, p + "sustain": 100, p + "release": 5, p + "time": 50})
        if e != 2:
            d[p + "clip"] = 0
        if e != 1:
            d[p + "link"] = 1
    for d_ in (1, 2):
        p = f"delay{d_}_"
        d.update({p + "on": 0, p + "rate": DELAY_RATES.index("1/4"), p + "decay": 35, p + "pan": 0})
    for m in range(1, 7):
        p = f"mod{m}_"
        default_target = "Element1 Pitch" if m == 4 else "Element1 Width"
        d.update({p + "target": mod_target_list(m - 1).index(default_target), p + "depth": 0})
        if m <= 3:
            d.update({p + "attack": 0, p + "decay": 0, p + "sustain": 100, p + "release": 5, p + "time": 50})
        else:
            d.update({p + "wave": 0, p + "gate": 0, p + "soft": 0, p + "rate": 7})
    return d


# ---------------------------------------------------------------------------
# Chimera side: measured knob mappings.
# ---------------------------------------------------------------------------

def zone(value: float, steps: int) -> int:
    """Chimera's stepped knobs select round(value * (steps - 1)) (measured for every stepped control)."""
    return max(0, min(steps - 1, int(value * (steps - 1) + 0.5)))


# Element filter knob zones (0.2 wide) in Chimera order -> Teratoamor FilterMode index
# (Off, Bypass, BP Wide, BP Narrow, Peak Wide, Peak Narrow).
ELEMENT_FILTER = [0, 2, 3, 4, 5, 1]  # Off, BP Wide, BP Narrow, Peak Wide, Peak Narrow, Bypass

# Delay Time and LFO Rate share 23 measured steps (at 120 BPM, echo/period in beats).
CHIMERA_RATES = [
    "1/64T", "1/64", "1/64D", "1/32T", "1/32", "1/32D", "1/16T", "1/16", "1/16D",
    "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D",
    "1/1T", "1/1", "1/1D", "2/1", "4/1",
]

# Measured absolute level: Teratoamor Element Level 100 and Master 100 is 9.2 dB louder
# than Chimera Volume 1.0 and Master Volume 1.0 (BP Wide, Width 90, note 60, velocity 100).
MASTER_SCALE = 34.9


def pct(v: float) -> float:
    return round(max(0.0, min(1.0, v)) * 100.0, 1)


def bipolar(v: float, span: float = 100.0) -> float:
    return round((max(0.0, min(1.0, v)) - 0.5) * 2.0 * span, 1)


def display_number(text: str) -> float | None:
    match = re.search(r"-?\d+(\.\d+)?", text)
    return float(match.group()) if match else None


def convert_program(params: dict[str, tuple[float, str]], notes: list[str]) -> dict[str, float]:
    """params maps Chimera parameter *index-qualified* names to (normalised, display)."""
    out = teratoamor_defaults()

    def n(name: str) -> float:
        return params[name][0]

    def disp(name: str) -> str:
        return params[name][1]

    # Elements
    for e in (1, 2, 3):
        c, t = f"Element{e} ", f"el{e}_"
        mode = ELEMENT_FILTER[zone(n(c + "Filter"), 6)]
        out[t + "on"] = 0 if mode == 0 else 1
        out[t + "filter"] = mode
        out[t + "octave"] = zone(n(c + "Octave"), 6) - 3          # -3..+2 octaves
        out[t + "semitone"] = zone(n(c + "Semi"), 25) - 12        # -12..+12
        out[t + "fine"] = bipolar(n(c + "Detune"), 50.0)          # +/-50 cents
        out[t + "width"] = pct(n(c + "Bandwidth"))
        if c + "Warp" in params:
            out[t + "warp"] = pct(n(c + "Warp"))
        elif mode != 0:
            # Chimera exposes no host parameter for Element 3 Warp, so its value cannot be read.
            notes.append(f"Element {e} Warp unknown (not exposed by Chimera); left at 0, check the screenshot")
        out[t + "level"] = pct(n(c + "Volume"))                   # linear amplitude in both
        out[t + "pan"] = bipolar(n(c + "Pan"))
        out[t + "attack"] = pct(n(c + "Attack"))
        out[t + "decay"] = pct(n(c + "Decay"))
        out[t + "sustain"] = pct(n(c + "Sustain"))
        out[t + "release"] = pct(n(c + "Release"))
        out[t + "time"] = pct(n(c + "Time"))
        if e != 2:
            out[t + "clip"] = pct(n(c + "Clip"))
        if e != 1:
            out[t + "link"] = 1 if n(c + "Link") < 0.5 else 0      # Chimera's Link switch is inverted
        if mode != 0 and n(c + "Hold") > 0.0:
            # Measured: any Hold above 0 makes the Element completely silent in Chimera.
            out[t + "on"] = 0
            notes.append(f"Element {e} turned off: its Hold {disp(c + 'Hold')} silences it in Chimera")

    # Global filter: same six zones and order as Teratoamor.
    out["gf_type"] = zone(n("Filter Type"), 6)
    out["gf_cutoff"] = pct(n("Filter Cutoff"))
    out["gf_q"] = pct(n("Filter Q"))

    # Distortion (approximate: Chimera Crush is bipolar around 0.5, Tone is a resonant low-pass sweep).
    if n("Distort On/Off") >= 0.5:
        out["dist_type"] = 1
        out["dist_crush"] = round(min(100.0, abs(n("Distort Crush") - 0.5) * 200.0), 1)
        out["dist_tone"] = round(n("Distort Tone") * 50.0, 1)
        notes.append(f"Distortion approximated (Crush {disp('Distort Crush')}, Tone {disp('Distort Tone')})")
    else:
        out["dist_type"] = 0

    # Delays
    out["delay_mix"] = pct(n("Delay Mix"))
    out["delay_cut"] = pct(n("Delay Filter"))
    for d in (1, 2):
        c, t = f"Delay{d} ", f"delay{d}_"
        on = n(c + "On/Off") >= 0.5
        out[t + "on"] = 1 if on else 0
        rate = CHIMERA_RATES[zone(n(c + "Time"), 23)]
        out[t + "rate"] = DELAY_RATES.index(rate)
        out[t + "decay"] = pct(n(c + "Decay"))
        out[t + "pan"] = bipolar(n(c + "Pan"))
        if on and rate in ("1/1D", "2/1", "4/1"):
            notes.append(f"Delay {d} is {rate}; Chimera's own delay wraps above about 3 s, so it sounds shorter there")

    # Master
    out["master_level"] = round(min(100.0, n("Master Volume") * MASTER_SCALE), 1)
    out["master_pan"] = bipolar(n("Master Pan"))
    out["master_glide"] = pct(n("Master Glide"))
    out["master_glide_mode"] = 0
    out["master_sync"] = 1 if n("Master Sync/BPM") < 0.5 else 0
    bpm = display_number(disp("Master BPM"))
    out["master_bpm"] = max(20.0, min(400.0, bpm if bpm is not None else 120.0))

    if abs(n("MIDI Aftertouch Depth") - 0.5) > 0.01:
        notes.append(f"Aftertouch {disp('MIDI Aftertouch Depth')} to {disp('MIDI Aftertouch Routing')} dropped")

    # Modulation. The plug-in's own names for some indices are off by one; see SECTION_PARAMS.
    for m in range(1, 7):
        t = f"mod{m}_"
        names = SECTION_PARAMS[m]
        target = disp(names["routing"]).replace("Bandwidth", "Width").replace("Distort Crush", "Distort Drive")
        targets = mod_target_list(m - 1)
        depth = display_number(disp(names["depth"]))
        depth = 0.0 if depth is None else max(-100.0, min(100.0, depth * 10.0))
        if target in targets:
            out[t + "target"] = targets.index(target)
            out[t + "depth"] = round(depth, 1)
        else:
            out[t + "depth"] = 0.0
            if abs(depth) > 0.0:
                notes.append(f"Modulation {m} target '{target}' does not exist in Teratoamor; depth set to 0")
        if m <= 3:
            for stage in ("attack", "decay", "sustain", "release", "time"):
                out[t + stage] = pct(n(names[stage]))
            if abs(depth) > 0.0 and n(names["hold"]) > 0.0:
                # Measured: an envelope with Hold above 0 never modulates in Chimera.
                out[t + "depth"] = 0.0
                notes.append(f"Modulation {m} depth set to 0: its Hold {disp(names['hold'])} disables it in Chimera")
        else:
            out[t + "wave"] = zone(n(names["wave"]), 18)
            out[t + "gate"] = 1 if n(names["gate"]) >= 0.5 else 0
            out[t + "soft"] = pct(n(names["soft"]))
            rate = CHIMERA_RATES[zone(n(names["rate"]), 23)]
            out[t + "rate"] = DELAY_RATES.index(rate)

    return out


# Chimera reports these host names. Index 84 is reported as a second "Modulation2 Time"
# and index 89 as "Modulation3 Rate"; measurement showed 89 sets Modulation 4's rate,
# so 84 is taken to be Modulation 3's Time. Keys are "name#index" to keep duplicates apart.
SECTION_PARAMS: dict[int, dict[str, str]] = {
    1: {"routing": "Modulation1 Routing#73", "depth": "Modulation1 Depth#70", "attack": "Modulation1 Attack#68",
        "decay": "Modulation1 Decay#69", "hold": "Modulation1 Hold#71", "release": "Modulation1 Release#72",
        "sustain": "Modulation1 Sustain#74", "time": "Modulation1 Time#75"},
    2: {"routing": "Modulation2 Routing#81", "depth": "Modulation2 Depth#78", "attack": "Modulation2 Attack#76",
        "decay": "Modulation2 Decay#77", "hold": "Modulation2 Hold#79", "release": "Modulation2 Release#80",
        "sustain": "Modulation2 Sustain#82", "time": "Modulation2 Time#83"},
    3: {"routing": "Modulation3 Routing#91", "depth": "Modulation3 Depth#87", "attack": "Modulation3 Attack#85",
        "decay": "Modulation3 Decay#86", "hold": "Modulation3 Hold#88", "release": "Modulation3 Release#90",
        "sustain": "Modulation3 Sustain#92", "time": "Modulation2 Time#84"},
    4: {"routing": "Modulation4 Routing#95", "depth": "Modulation4 Depth#93", "gate": "Modulation4 Gatetrig#94",
        "soft": "Modulation4 Soft#96", "wave": "Modulation4 Waveform#97", "rate": "Modulation3 Rate#89"},
    5: {"routing": "Modulation5 Routing#101", "depth": "Modulation5 Depth#98", "gate": "Modulation5 Gatetrig#99",
        "soft": "Modulation5 Soft#102", "wave": "Modulation5 Waveform#103", "rate": "Modulation5 Rate#100"},
    6: {"routing": "Modulation6 Routing#107", "depth": "Modulation6 Depth#104", "gate": "Modulation6 Gatetrig#105",
        "soft": "Modulation6 Soft#108", "wave": "Modulation6 Waveform#109", "rate": "Modulation6 Rate#106"},
}


# ---------------------------------------------------------------------------
# File output
# ---------------------------------------------------------------------------

def preset_bytes(values: dict[str, float], name: str) -> bytes:
    """Same layout as JUCE's copyXmlToBinary: 'VC2!', uint32 UTF-8 length (without the NUL), XML, NUL."""
    params = "".join(f'<PARAM id="{key}" value="{float(value)!r}"/>' for key, value in values.items())
    xml = (f'<?xml version="1.0" encoding="UTF-8"?> <TeratoamorState version="1" presetName={quoteattr(name)}>'
           f"{params}</TeratoamorState>")
    data = xml.encode("utf-8")
    return b"VC2!" + struct.pack("<I", len(data)) + data + b"\0"


def safe_filename(name: str) -> str:
    return re.sub(r'[<>:"/\\|?*]', "", name).strip().rstrip(".")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("csv", type=pathlib.Path, help="builtin.csv from vst2_param_dump")
    parser.add_argument("out", type=pathlib.Path, help="output folder for .teratoamor files and the report")
    args = parser.parse_args()

    programs: dict[int, dict[str, tuple[float, str]]] = defaultdict(dict)
    program_names: dict[int, str] = {}
    with args.csv.open(newline="", encoding="utf-8", errors="replace") as source:
        for row in csv.DictReader(source):
            index = int(row["program"])
            program_names[index] = row["program_name"]
            value = (float(row["normalised"]), row["display"])
            programs[index][f"{row['param_name']}#{row['param_id']}"] = value
            programs[index].setdefault(row["param_name"], value)

    args.out.mkdir(parents=True, exist_ok=True)
    report = ["# Converted Chimera programs", "",
              "Local A/B material only. Approximations and dropped features per program:", ""]
    category = ""
    written = 0

    for index in sorted(programs):
        name = program_names[index]
        if name.startswith("---"):
            category = name.strip("- ").strip()
            continue
        if name == "Default":
            continue

        notes: list[str] = []
        values = convert_program(programs[index], notes)
        file_name = safe_filename(f"{index:02d} {category} - {name}")
        (args.out / f"{file_name}.teratoamor").write_bytes(preset_bytes(values, file_name))
        written += 1
        report.append(f"- **{file_name}**" + (": " + "; ".join(notes) if notes else ": exact mapping"))

    (args.out / "conversion_report.md").write_text("\n".join(report) + "\n", encoding="utf-8")
    print(f"Wrote {written} presets and conversion_report.md to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
