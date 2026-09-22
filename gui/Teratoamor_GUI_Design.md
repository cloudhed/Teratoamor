# Teratoamor GUI Design Specification

**Document status:** Design target with an initial JUCE implementation (September 2026)
**Product:** Teratoamor 64-bit VST3 synthesizer  
**Purpose:** Give human contributors, Codex, and Claude Code a single reference for the plugin editor’s structure, controls, appearance, interaction rules, and implementation boundaries.

---

## Current implementation and overrides

The cleaner mockup is the visual reference. The initial working editor uses the
embedded PNG glyph, procedurally drawn panels and controls, three permanently
visible Elements, and a lower workspace with **Global / Modulation (Env) /
Modulation (Osc) / Space** tabs.
Master and the stereo output meter are together in the permanent top bar, as
requested. Tempo and host sync also remain visible. The meter reads accumulated
output peaks at 30 Hz, falls by 36 dB/second, and displays a 1.5-second CLIP label.
It is a sample-peak meter, not a true-peak or loudness meter.

The processor supersedes older inventory details below:

- There is no Hold. Elements have Attack, Decay, Sustain, Release and Time.
- Element 2 has no Clip. Linked Elements use Element 1's envelope, including its
  graph; their stored local envelope controls are dimmed and disabled.
- Distortion has Bypass / Drive, Drive and Tone. Delay has one shared Cut.
- Modulation (Env) shows envelopes 1–3 side by side, with vertical A/D/S/R/Time
  sliders, a small shape preview and Depth for each. Modulation (Osc) shows
  oscillators 4–6 side by side. All controls remain attached when hidden;
  navigation never alters parameters.
- Space retains shared delay Mix/Cut on the left, stacks Delay 1 above Delay 2
  in the middle, and reserves the rightmost panel for future Reverb. Reverb has
  no controls, parameters or audio processing yet.
- The current active coral is brighter peach-coral `#F5A399`, chosen visually
  from the supplied swatch. Warm toggle outlines retain `#D67A7B`. General borders
  use 1.6 logical pixels, accent outlines 2, and knob arcs 4–6.
- Enabled knob arcs and slider handles, on-state switch lights, and selected
  button outlines have a warm orange halo around coral lights. `Theme::glowColours`, `Theme::glowSpread`,
  `glowOpacity`, and `glowLayers` in `Source/ui/Theme.h` control its softness and
  strength. Layered translucent strokes keep it procedural and scale with the
  editor; the crisp control is drawn over the halo. Disabled controls omit it.
  The two outer layers use `#FF965B`; the six inner layers step through
  `#FF915B`, `#FF8D5B`, `#FF885B`, `#FF835B`, `#FF7F5B`, and `#FF7A5B`,
  retaining the same fading alpha on every layer.
- Mockup tuning uses pale peach arc cores, the first glow pass's opacity and
  linear falloff with a wider 6-pixel spread (originally 4),
  recessed dark knob rings, shaded charcoal knob faces with subtle rims, and
  peach switch lights in dark wells. Selected buttons use a luminous outline
  without an underline. This is procedural styling, not a pixel-exact replica.
- Parameter readouts retain processor units (including 0–100 control values).
  They do not claim those values are Hz, dB, seconds or Q factors. Envelope plots
  show a normalised shape with an illustrative sustain duration, not live voices.
- Manual BPM remains editable with sync enabled because it is also the fallback
  when no host tempo is available, including the standalone application.
- Preset management, A/B, MIDI assignments and live voice/MIDI activity are future
  work. The first editor does not display nonfunctional controls for them.

### Adjusting the layout

- `Source/ui/Theme.h`: palette, reference canvas, window limits, region heights,
  padding, gaps, and Element proportions.
- `Source/ui/Theme.cpp`: shared knobs, sliders, toggles, tabs and focus styling.
- `Source/ui/EditorContent.cpp`: reusable parameter controls and panel classes.
  `ElementPanel::resized()` controls the Element arrangement; the local ID/label
  arrays control its inventory. `ControlPanel` lays out processing controls.
  `ModEnvelopePanel` and `ModOscillatorPanel` lay out individual modulation slots;
  `SpacePanel` controls the delay/reverb proportions and `DelayChannelPanel` the
  compact delay rows. Related size ratios are in `Theme.h`.
  `EditorContent::resized()` controls the overall region order.
- `Source/PluginEditor.cpp`: uniform canvas scaling and the host resize constraint.

The reference canvas is 1600 × 1000; the initial window is 1440 × 900, with a
1200 × 750 minimum and 2400 × 1500 maximum. JUCE handles display DPI separately.
The UI uses the system sans-serif font and needs no new font or library licence.
Only the logo is a bitmap; panel geometry and control positions are independent.
Drag the window corner to resize. Every numeric value supports up/down dragging,
Shift-drag for fine adjustment, click-to-type and double-click to restore the
parameter default. A single click waits for the double-click interval before
opening text editing; Enter also opens editing when the value has keyboard focus.
Number dragging follows the slider's range, steps and skew and sends automation
begin/end gestures through the existing attachment. Knob and fader gestures remain available.
Mouse-wheel parameter changes are disabled.

`TeratoamorEditorTests` exercises all parameter attachments in both directions,
linked envelopes, state restoration, tab navigation without patch changes,
meter consumption and visible component bounds. It renders every workspace at
minimum/default/maximum sizes, including all six grouped modulation editors, into the ignored
`build/gui-previews/` directory. Visual review complements the bounds checks.
Interactive Cubase validation and Windows display-scale checks remain manual.

---

## 1. Design intent

Teratoamor should feel like a contemporary relative of Majken’s Chimera without reproducing Chimera’s artwork or trade dress. It keeps the original instrument’s unusually direct, parameter-dense workflow while giving it an independent visual identity derived from the Teratoamor three-petal glyph.

The GUI should be:

- dark, organic, and slightly uncanny;
- readable during long sessions;
- compact enough to show the whole signal path without becoming cramped;
- clearly divided into sound generation, processing, modulation, MIDI, and output;
- practical to implement and maintain in JUCE;
- scalable without relying on one large bitmap background.

The interface should avoid fake hardware, screws, wood, brushed metal, excessive gradients, and purely decorative technical markings. Curves and asymmetric panel corners may subtly echo the three-petal logo, but controls must remain predictable and easy to scan.

---

## 2. Reference assets

The repository should keep the following source references together, for example under `design/gui/`:

- the Teratoamor logo or glyph, preferably as SVG;
- the first exploratory GUI mockup;
- the control-complete GUI mockup;
- this document.

Suggested names:

```text
design/gui/
├── teratoamor-glyph.svg
├── teratoamor-gui-concept-01.png
├── teratoamor-gui-concept-02-controls.png
└── GUI_DESIGN.md
```

The **first, cleaner mockup is the primary visual reference** for composition, spacing, atmosphere, panel shapes, and overall beauty. Its inaccurate or placeholder labels must not be copied into the working interface.

The **second, control-complete mockup is an inventory reference only**. It demonstrates the required control families and possible grouping, but its compressed density is not the target layout. The finished GUI should preserve the first mockup's breathing room while exposing the complete specification through tabs or contextual panels.

Neither mockup is a pixel-perfect implementation blueprint. The parameter model and the specifications below take priority whenever either image contains inaccurate text or invented details.

---

## 3. Colour system

The palette is derived from the final logo. Colours should be declared centrally rather than repeated as raw literals throughout the editor code.

| Token | Hex | Intended use |
|---|---:|---|
| `background` | `#0E0F14` | Window background, deepest wells, empty display areas |
| `panelDeep` | `#44344E` | Main panel surfaces and large grouped regions |
| `violetDark` | `#6C5076` | Inactive control arcs, borders, secondary surfaces |
| `violetMid` | `#856489` | Hovered controls, separators, secondary emphasis |
| `mauve` | `#AE85A2` | Secondary text, inactive values, subtle highlights |
| `blush` | `#E4A9A9` | Primary labels, titles, bright indicators |
| `coral` | `#D67A7B` | Active states, current values, modulation amount, focus |

Recommended derived colours may be produced by changing alpha or lightness in code. Do not introduce unrelated blues, greens, or saturated warning colours unless a genuine status or error needs them.

### Colour behaviour

- **Normal:** muted violet body, pale label, mauve or coral value.
- **Hover:** slightly brighter border or control arc; no large glow.
- **Active/on:** coral accent and increased contrast.
- **Keyboard focus:** a thin pale-blush outline distinct from hover.
- **Disabled:** reduced opacity while preserving legibility.
- **Clipping/error:** use a brighter coral-red derived from the palette, accompanied by shape or text so colour is not the only signal.

Coral should be used sparingly. If everything is coral, active controls become difficult to identify.

---

## 4. Typography and iconography

Use one clean sans-serif family with several weights. The first implementation may use a redistributable bundled font or a dependable system fallback. Font licensing must be verified before shipping.

Suggested hierarchy at the reference size:

| Use | Approximate size | Style |
|---|---:|---|
| Product name | 20–24 px | Semibold, increased tracking |
| Major panel title | 13–15 px | Semibold, uppercase |
| Control label | 10–12 px | Medium |
| Value/readout | 10–12 px | Regular or tabular numerals |
| Secondary routing text | 9–11 px | Regular |

Use tabular numerals for changing numeric values to prevent labels from shifting. Do not use all-uppercase text for long routing names.

Prefer the Teratoamor SVG glyph and simple vector paths for utility icons. Do not use emoji or platform-dependent symbol fonts inside the plugin.

---

## 5. Window and responsive layout

### Visual priority

When layout choices conflict, use this order of priority:

1. preserve the clean composition and visual character of the first mockup;
2. keep the three Elements immediately visible and easy to compare;
3. keep frequently adjusted processing controls directly accessible;
4. place detailed modulation and MIDI editing behind clear tabs;
5. avoid shrinking controls or text merely to display everything simultaneously.

The interface should never resemble the second mockup's full control inventory at the default window size. Completeness is achieved through well-organised views, not through maximum simultaneous density.

### Reference size

Design at approximately **1600 × 1000 px** logical coordinates, a 16:10 ratio. The initial shipping size may be smaller, but the editor should preserve this ratio and support a useful zoom range such as 75–150%.

JUCE should lay out components from bounds, proportions, and shared spacing constants. Do not hard-code hundreds of unrelated absolute coordinates.

### Top-level regions

```text
┌──────────────────────────────── Header ────────────────────────────────┐
├──────────── Element 1 ───────────┬──────── Element 2 ────────┬──────── Element 3 ────────┤
├──── Filter ────┬── Distort ─────┬────────── Dual Delay ──────┬──────── Master ──────────┤
├──────────────────────────── Modulation / MIDI workspace ────────────────────────────────┤
└─────────────────────────────────────────────────────────────────────────────────────────┘
```

The three Element panels have equal width and visual weight. Processing follows signal-flow order from left to right. Modulation and MIDI occupy the lowest-priority vertical region.

### Modulation workspace

The production layout uses:

- six compact selector tabs labelled `MOD 1` through `MOD 6`;
- one full-size modulation editor for the selected slot;
- a neighbouring `MIDI` tab or panel for MIDI assignments;
- optional overview indicators on each tab showing enabled state and activity.

Selecting a modulation tab changes only the editor area; it must not enable, disable, or otherwise alter that modulation slot. An expanded or large-window view may show several modulation panels together later, but it is not required for the first finished GUI.

---

## 6. Header specification

The header contains, from left to right:

1. Teratoamor glyph and wordmark.
2. Previous-preset button.
3. Current preset name / preset menu.
4. Next-preset button.
5. `INIT` button.
6. `SAVE` button.
7. `A` and `B` comparison buttons.
8. MIDI activity indicator.
9. Voice-count display.
10. Settings button.
11. Compact master level indication and/or master output control.

### Behaviour

- The preset name must truncate gracefully rather than overlap adjacent controls.
- `INIT` should require confirmation if it would discard an edited state.
- `SAVE` opens the chosen preset-saving workflow.
- A/B state must be obvious both through colour and the selected button shape.
- MIDI activity should flash briefly on note or relevant controller input.
- Voice count should display the number of currently sounding voices, not the configured polyphony limit, unless explicitly labelled otherwise.

---

## 7. Element panels

There are three instances of the same reusable component:

- `ELEMENT 1`
- `ELEMENT 2`
- `ELEMENT 3`

Element 1 is always the reference element. Elements 2 and 3 each include a `LINK` control for behaviour that is actually supported by the audio engine. The exact link semantics must be documented by the DSP implementation before the control is enabled.

### 7.1 Mode selector

Each Element exposes these modes, in this order:

1. `Off`
2. `Bypass`
3. `BP Wide`
4. `BP Narrow`
5. `Peak Wide`
6. `Peak Narrow`

Use either a compact segmented selector or a dropdown plus an abbreviated status display. A segmented selector is preferable when sufficient width is available because it makes the available modes discoverable.

`Off` and `Bypass` must remain visually and behaviourally distinct. Their exact DSP meanings must come from the parameter specification and tests; the GUI must not silently treat them as aliases.

### 7.2 Pitch and width controls

Each Element contains:

- `Width` — the most visually prominent parameter in the panel;
- `Detune`;
- `Octave`;
- `Semi`.

Display units and ranges from the audio processor’s parameter definitions. Do not independently encode ranges in the editor.

Stepped parameters such as octave and semitone should feel stepped during dragging and keyboard adjustment. Continuous parameters should use smooth movement.

### 7.3 Shape controls

Each Element contains:

- `Warp`;
- `Clip`.

These may use narrow vertical sliders to distinguish them from pitch and width controls. Their value labels remain visible and update during automation.

### 7.4 Element envelope

Each Element contains the full six-stage envelope:

- `H` — Hold;
- `A` — Attack;
- `D` — Decay;
- `S` — Sustain;
- `R` — Release;
- `T` — the original instrument’s T stage, with final meaning and units defined by the DSP specification.

The panel should include a compact live envelope graph. The graph is a visualisation of the six parameter values and is not required to be directly editable in the first version.

The graph must cope sensibly with zero values and long release times. It should show the entire envelope rather than clipping its tail to the panel edge.

### 7.5 Output controls

Each Element ends with:

- `Pan`;
- `Vol`.

Use a centred bipolar display for pan and decibels for volume if those units match the processor parameter model.

---

## 8. Processing panels

### 8.1 Filter

The Filter panel contains:

- a mode selector/status display;
- `Q`;
- `Cutoff`.

Only include modes implemented by the DSP. Do not copy temporary mode names from an image mockup into code without checking the parameter specification.

### 8.2 Distort

The Distort panel contains:

- an on/off or mode selector;
- `Crush`;
- `Tone`.

The off state should mute the visual emphasis of its controls without making their labels unreadable.

### 8.3 Delay

The Delay panel contains global controls:

- `Mix`;
- `Lowcut`;
- `Highcut`.

It then contains two delay subsections:

- `DELAY 1`;
- `DELAY 2`.

Each delay contains:

- enable switch;
- `Rate`;
- `Decay`;
- `Pan`.

When synchronised, Rate displays musical divisions such as `1/4` or `3/8`. When free-running, it displays the unit selected by the DSP, normally milliseconds or hertz. The GUI must obtain the formatted value through the parameter layer rather than guessing.

### 8.4 Master

The Master panel contains:

- `Sync`;
- `Glide`;
- `Volume`;
- `Pan`;
- stereo level meters.

Meters should be smooth enough to read but must not trigger excessive repainting. Meter values are read-only visual state and should not be stored in the plugin state tree.

---

## 9. Modulation system

Teratoamor contains six modulation slots:

- `MODULATION 1` through `MODULATION 6`.

Every slot contains:

- `H`, `A`, `D`, `S`, `R`, `T`;
- `Depth`;
- source selector;
- destination selector;
- compact envelope graph;
- activity indication when the modulation is running.

Modulation 4–6 additionally contain:

- `GATE TRIG`;
- `Soft`;
- `Rate`.

The exact source and destination lists must be generated from or mapped to the processor’s authoritative parameter IDs. Destination names should be friendly display names such as `Element 1 Width`, while stable internal identifiers remain independent of GUI wording.

Depth should use a centred bipolar control when negative modulation is supported. If the DSP supports only positive modulation, use a unipolar control and avoid drawing a false centre position.

---

## 10. MIDI assignment panel

The MIDI panel shows assignments in a compact table with these columns:

| Column | Content |
|---|---|
| `CC` | MIDI continuous-controller number |
| `Destination` | Friendly parameter name |
| `Amount` | Mapping depth, direction, or range |

The UI should support selection, editing, and deletion of a mapping. MIDI learn may be added if supported by the implementation. Empty rows should look deliberately empty rather than broken.

MIDI assignments must be stored with plugin state and restored with the project or preset according to the agreed preset rules.

---

## 11. Reusable control language

Create a small, consistent family of custom components instead of styling each control independently.

### Rotary knob

- Circular or slightly organic body.
- Muted violet inactive track.
- Coral active arc.
- Clear pointer line.
- Label above or below according to panel density.
- Numeric value always available.
- Fine adjustment with Shift-drag.
- Double-click restores the processor-defined default.
- Right-click exposes parameter actions where appropriate, such as reset or MIDI learn.

### Vertical slider

- Narrow dark track.
- Mauve handle, coral when active or focused.
- Large enough hit target despite the slim drawing.
- Value readout aligned consistently beneath it.

### Toggle

- Text label plus a clear two-state shape.
- On/off must remain distinguishable in greyscale.
- Avoid tiny indicator lights as the only click target.

### Selector

- Use segmented buttons when the set is small and stable.
- Use a dropdown when names are long or space is limited.
- The selected option must be visible without opening the menu.

### Envelope graph

- Near-black plot area.
- Fine violet guides only if helpful.
- Mauve fill at low opacity.
- Coral line and nodes.
- No decorative animation when audio is idle.

### Meter

- Blush/coral bars on a near-black well.
- Peak hold may be shown with a thin marker.
- Clip state persists long enough to notice and can be cleared by clicking if implemented.

---

## 12. Interaction and usability rules

- Every parameter control must display its current value and unit.
- Hovering may reveal extra precision, but essential information must not depend on hover.
- Parameter changes must remain visible when driven by host automation.
- Controls must support normal JUCE keyboard focus and arrow-key adjustment.
- Tooltips should explain abbreviated labels such as `H` and `T`.
- Hit targets should be at least roughly 24 × 24 logical pixels, preferably larger.
- Do not make mouse-wheel parameter changes the default unless they can be disabled; accidental scroll changes are common in plugins.
- Drag direction and sensitivity should be consistent across all rotary controls.
- Disabled controls should explain why they are disabled when the reason is not obvious.
- The UI must remain usable at Windows display scaling values such as 100%, 125%, 150%, and 200%.

---

## 13. JUCE implementation structure

The editor should be composed from reusable components. A likely structure is:

```text
TeratoamorAudioProcessorEditor
├── TeratoamorLookAndFeel
├── HeaderComponent
├── ElementPanel × 3
│   ├── ModeSelector
│   ├── ParameterKnob / ParameterSlider
│   └── EnvelopeDisplay
├── FilterPanel
├── DistortionPanel
├── DelayPanel
│   └── DelayChannelPanel × 2
├── MasterPanel
│   └── StereoMeter
└── LowerWorkspace
    ├── ModulationEditor
    └── MidiMappingPanel
```

Recommended separation:

- `PluginProcessor` owns audio behaviour, parameter definitions, and persistent state.
- `PluginEditor` owns top-level GUI composition only.
- Individual panel classes own their local layout.
- `TeratoamorLookAndFeel` owns shared drawing rules.
- A central theme header owns colour, typography, radius, and spacing tokens.
- Parameter attachments connect controls to `AudioProcessorValueTreeState` or the final parameter system.

The GUI must not duplicate DSP state in ad-hoc member variables. Controls should attach to stable parameter IDs. Never rename a released parameter ID merely to improve a visible label; change the display name instead.

### Suggested source layout

```text
Source/
├── PluginProcessor.h
├── PluginProcessor.cpp
├── PluginEditor.h
├── PluginEditor.cpp
└── ui/
    ├── Theme.h
    ├── TeratoamorLookAndFeel.h
    ├── TeratoamorLookAndFeel.cpp
    ├── HeaderComponent.h
    ├── HeaderComponent.cpp
    ├── ElementPanel.h
    ├── ElementPanel.cpp
    ├── EnvelopeDisplay.h
    ├── EnvelopeDisplay.cpp
    ├── ProcessingPanels.h
    ├── ProcessingPanels.cpp
    ├── ModulationEditor.h
    ├── ModulationEditor.cpp
    ├── MidiMappingPanel.h
    └── MidiMappingPanel.cpp
```

This is a suggested organisation, not a requirement to create every file before it is useful.

---

## 14. Layout implementation guidance

Use a small set of layout constants, for example:

- outer margin;
- panel gap;
- panel inner padding;
- title height;
- standard control row height;
- corner radius;
- compact and full control sizes.

Top-level layout can use `juce::Grid`, `juce::FlexBox`, carefully divided rectangles, or a combination. Nested panel layout should remain local to the panel class.

Panel curves should be drawn procedurally with `juce::Path` or rounded rectangles. Avoid rasterising complete panels. The logo may be loaded as SVG and converted to a `juce::Drawable`.

Expensive paths, text layouts, and static background images should be cached where useful. Repaint only the area that changed, especially for meters and modulation activity.

---

## 15. Parameter and display-source rules

The audio processor is authoritative for:

- parameter IDs;
- ranges;
- skew factors;
- step intervals;
- defaults;
- units;
- value-to-text formatting;
- automatable status.

The design document is authoritative for grouping, labels, visual hierarchy, and interaction patterns. If this document and the processor disagree about a numeric range, fix the documentation or parameter definition deliberately; do not conceal the mismatch in editor-only conversion code.

Where the clone’s behaviour is still being measured, mark the parameter as provisional in the parameter specification. The GUI can be built with the correct control type while the exact curve is refined later.

---

## 16. Accessibility and testing

The GUI should be checked for:

- readable contrast between text, values, and panel surfaces;
- distinguishable on/off and selected/unselected states without relying only on hue;
- keyboard focus visibility;
- sensible screen-reader names where JUCE accessibility support permits;
- stable rendering at multiple plugin sizes and Windows scaling settings;
- no clipped labels with long preset or destination names;
- no control overlap at the minimum supported size;
- correct attachment and automation for every visible parameter;
- no GUI operations performed directly on the real-time audio thread;
- no unnecessary repaint storms while meters or animations are active.

Take screenshots at the minimum, default, and maximum supported editor sizes and compare them during review.

---

## 17. First implementation milestones

### Milestone 1 — static shell

- Theme colours and typography.
- Resizable editor and major panel boundaries.
- SVG logo and header.
- Three empty Element panels.

### Milestone 2 — one complete Element

- Implement Element 1 using real parameter attachments.
- Establish reusable knob, slider, selector, and envelope-display components.
- Verify value formatting and automation.

### Milestone 3 — replicated sound controls

- Reuse the Element component for Elements 2 and 3.
- Add link controls when their semantics are implemented.
- Add Filter, Distort, Delay, and Master.

### Milestone 4 — modulation and MIDI

- Add the tabbed modulation workspace.
- Implement all six modulation slots.
- Add MIDI mapping table and persistence.

### Milestone 5 — polish and validation

- Resizing and Windows scaling tests.
- Keyboard, focus, and tooltip pass.
- Meter optimisation.
- Host automation and preset-state testing.
- Final visual comparison against the first mockup for aesthetics and the control-complete mockup for feature coverage.

---

## 18. Definition of done

The GUI is ready for an initial public build when:

- every implemented processor parameter has the intended visible control;
- every visible control changes the correct parameter and follows host automation;
- saved plugin state reopens with the same visible settings;
- the three Element panels are reusable instances rather than duplicated implementations;
- the default view retains the first mockup's open spacing and does not show all six modulation editors simultaneously;
- the interface scales without clipped or overlapping controls;
- active, hovered, focused, bypassed, and disabled states are visually distinct;
- the logo and palette clearly establish Teratoamor’s identity;
- CPU use remains stable when the editor is open;
- closing the editor has no effect on audio processing;
- the plugin has been tested in the target host, initially Cubase on 64-bit Windows.

---

## 19. Decisions still requiring confirmation

These should remain explicit tasks rather than being guessed during GUI coding:

- exact semantic difference between Element `Off` and `Bypass`;
- exact meaning, range, and display unit of envelope stage `T`;
- exact Element link behaviour;
- Filter mode list;
- Distort selector modes and bypass semantics;
- free-running Delay Rate unit and range;
- whether Master `Sync` governs both delays or another timing system;
- precise modulation source and destination lists;
- MIDI learn workflow and whether mappings belong to presets, global settings, or both;
- final minimum/default/maximum window sizes;
- final font family and its redistribution licence.

Until these are resolved, the GUI may display provisional controls, but their parameter IDs and persisted behaviour should not be treated as final.
