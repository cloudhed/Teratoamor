# Teratoamor repository instructions

These instructions apply to the whole repository.

## Project intent

Teratoamor is an original, clean-room, modern 64-bit synthesizer inspired by the behaviour of Majken's Chimera. It must not contain copied source code, binary code, branding, graphics, proprietary presets, preset names, or trade dress from Chimera.

Before planning or implementing changes, read:

- `docs/CHIMERA_REFERENCE.md` for the permitted clean-room findings and measurement boundary.
- `docs/DEVELOPMENT_PLAN.md` for scope, status, and milestone order.

## Hard boundaries

- Never modify anything under `external/JUCE`.
- Never commit build directories or anything under `reference/`.
- Treat `reference/chimera/` as local, ignored research material only. Never distribute it.
- You may inspect recorded WAV files, the VSTXML parameter export, FXB/FXP metadata, the screenshot, and documented observations only as behavioural references.
- Never disassemble, decompile, translate, or inspect executable code from `Majken's Chimera.dll`, `.SEP` files, `.SEM` files, or extracted PE sections such as `.text`, `.rdata`, `.data`, `.rsrc`, or `.reloc`. Do not delegate that work to another agent.
- Implement all DSP independently from behavioural measurements and published, general DSP techniques.
- Do not commit or push unless the user explicitly asks.
- Preserve unrelated user changes and prefer small, reviewable edits.

## Engineering rules

- Keep DSP code separate from GUI code.
- Keep the audio thread allocation-free and lock-free.
- Smooth continuously adjustable audio parameters.
- Guard resonant filters against instability, NaN, and infinity, especially near maximum Width.
- Use deterministic, independently seeded noise sources for every voice and Element.
- Retain host parameter IDs once released, and save and restore parameter state.
- Do not add a dependency with a potentially incompatible licence without explaining it and obtaining approval first.
- After implementation changes, configure and build when possible. Report the exact commands, results, and output artifact locations.
- Explain commands and errors in plain language suitable for someone new to C++, CMake, JUCE, and VS Code.

## Product and toolchain baseline

- Product: Teratoamor
- Targets: Windows x64 VST3 instrument and standalone application
- Primary test host: Cubase 10
- Language: C++17, or a newer minimum if JUCE requires it
- Build system: CMake 4.4.3
- Compiler: current Microsoft Visual C++ Build Tools / Visual Studio 18
- Framework: JUCE 9.0.2, pinned at `external/JUCE`
- Intended licence: AGPLv3 while JUCE is used under its AGPLv3 option

Once a root `CMakeLists.txt` exists, the expected initial build commands are:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
```

