# Drumboy Pro firmware backlog

Open work for the application firmware (`02 Software/Drumboy-Pro-App-H723`), as of **V1.09**. Each item says what is wrong or missing, what the code does today, and where to start. Function names are used instead of line numbers, which move.

Before you start, read the README's **Firmware** section: `make WERROR=1`, `make -C "02 Software/tests"`, `make PROFILE=1` for DSP load, and the release steps. The hardware-free parts of the firmware (`Fill.h`, `Routing.h`) have host tests; put new logic there when you can, so it can be tested on a PC.

This work lives in the fork **MiguelMoraleja/Drumboy-Pro**. Upstream (**Randomwaves/Drumboy-Pro**) has none of it yet.

| # | Item | Type | Size |
|---|---|---|---|
| 1 | [LFO-modulated values don't move on the display](#1-lfo-modulated-values-dont-move-on-the-display) | UX bug | S |
| 2 | [Instrument pitch should step in semitones and octaves](#2-instrument-pitch-should-step-in-semitones-and-octaves) | Feature | M |
| 3 | [LFOs can't modulate filters or effects](#3-lfos-cant-modulate-filters-or-effects) | Feature | L |
| 4 | [Routing: multiple destinations per source](#4-routing-multiple-destinations-per-source) | Feature | M |
| 5 | [Routing: send levels](#5-routing-send-levels) | Feature | M |
| 6 | [Routing: extra effect slots](#6-routing-extra-effect-slots) | Feature | L |
| 7 | [Routing: open design questions](#7-routing-open-design-questions) | Design | — |
| 8 | [LIMITER setting does nothing](#8-limiter-setting-does-nothing) | Bug | S |
| 9 | [Song/kit file format version](#9-songkit-file-format-version) | Groundwork | M |
| 10 | [Housekeeping](#10-housekeeping) | Chores | S |
| 11 | [Euclidean rhythm page (EUCLID)](#11-euclidean-rhythm-page-euclid) | Feature | M |
| 12 | [Free beat placement on Layer Song](#12-free-beat-placement-on-layer-song) | Feature | M |
| 13 | [Sample preview while browsing](#13-sample-preview-while-browsing) | Feature | L |
| 14 | [EUCLID: velocity and variance](#14-euclid-velocity-and-variance) | Feature | S–M |
| 15 | [Song mode: chain the banks](#15-song-mode-chain-the-banks) | Feature | M–L |
| 16 | [Kids firmware](#16-kids-firmware) | Feature, on hold | L |
| 17 | [Sample rate conversion (48 kHz and others)](#17-sample-rate-conversion-48-khz-and-others) | Bug | M |
| 18 | [Swing / shuffle (LXR)](#18-swing--shuffle-lxr) | Feature | S–M |
| 19 | [Choke groups (LXR)](#19-choke-groups-lxr) | Feature | S |
| 20 | [Parameter locks per beat (LXR)](#20-parameter-locks-per-beat-lxr) | Feature | L |
| 21 | [Probability per beat (LXR)](#21-probability-per-beat-lxr) | Feature | M |
| 22 | [Live roll (LXR)](#22-live-roll-lxr) | Feature | S–M |
| 23 | [Per-layer length (polymeter) (LXR)](#23-per-layer-length--polymeter-lxr) | Feature | L |
| 24 | [Velocity modulates a parameter (LXR)](#24-velocity-modulates-a-parameter-lxr) | Feature | M |
| 25 | [LFO retrigger and free-running (LXR)](#25-lfo-retrigger-and-free-running-lxr) | Feature | S |
| 26 | [Random step in the song chain (LXR)](#26-random-step-in-the-song-chain-lxr) | Feature | S |
| 27 | [Erase while recording (LXR)](#27-erase-while-recording-lxr) | Feature | S |
| 28 | [Undo for bank edits (LXR)](#28-undo-for-bank-edits-lxr) | Feature | S–M |
| 29 | [New effects: ring modulator, tilt EQ, master saturation (LXR)](#29-new-effects-ring-modulator-tilt-eq-master-saturation-lxr) | Feature | M each |
| 30 | [MIDI in/out: velocity, running status, note queue, CC](#30-midi-inout-velocity-running-status-note-queue-cc) | Bug + feature | S (CC: M) |
| 31 | [Kit morph (LXR)](#31-kit-morph-lxr) | Feature | M |
| 32 | [Autosave](#32-autosave) | Feature | A: M, B: M–L |
| 33 | [Per-note editing: velocity, probability and more](#33-per-note-editing-velocity-probability-and-more) | Feature | M–L |
| 34 | [MIDI clock slave: drift and 200 BPM runaway](#34-midi-clock-slave-drift-and-200-bpm-runaway) | Bug | M |
| 35 | [LFO destinations for the per-note settings](#35-lfo-destinations-for-the-per-note-settings) | Feature | M |
| 36 | [Per-layer panning](#36-per-layer-panning) | Feature | A: S–M, B: L |
| 37 | [Measure first: drawing speed, loop time, audio load](#37-measure-first-drawing-speed-loop-time-audio-load) | Groundwork | S |
| 38 | [Per-note settings and layer state visible in the lanes](#38-per-note-settings-and-layer-state-visible-in-the-lanes) | UX | S–M |
| 39 | [Graphic overviews: NOTE, EUCLID, SONG, ROUTING](#39-graphic-overviews-note-euclid-song-routing) | UX | M |
| 40 | [Curves and cursors on the sound pages](#40-curves-and-cursors-on-the-sound-pages) | UX | M |
| 41 | [Live feedback: layer meters and status indicators](#41-live-feedback-layer-meters-and-status-indicators) | UX | M |
| 42 | [Key hints, status line, page dots, value popup](#42-key-hints-status-line-page-dots-value-popup) | UX | M |
| 43 | [Fonts with lowercase and symbols; colour roles](#43-fonts-with-lowercase-and-symbols-colour-roles) | UX | M |
| 44 | [Notes triggered outside the main loop; draw budget](#44-notes-triggered-outside-the-main-loop-draw-budget) | Groundwork | M |
| 45 | [Full-area views: MIX, note editor, sample editor, patchbay, performance](#45-full-area-views-mix-note-editor-sample-editor-patchbay-performance) | UX | L each |
| 46 | [Sends: a level per layer into the shared effects (SENDS mode)](#46-sends-a-level-per-layer-into-the-shared-effects-sends-mode) | Feature | M |
| 47 | [Channel filter and drive per layer](#47-channel-filter-and-drive-per-layer) | Feature | M |
| 48 | [Master EQ stage, stereo returns, a third send effect](#48-master-eq-stage-stereo-returns-a-third-send-effect) | Feature | M–L |

Sizes are rough: S = a day, M = a few days, L = a week or more, including hardware testing.

## Plan

**Sprint 1, V1.10 "Sequencer"** (done, tested on hardware):
1. ✅ Beat-list editing in a hardware-free header with host tests (`BeatList.h`, `test_beatlist`): rebuild, move, rotate, insert. Items 11 and 12 both use it.
2. ✅ [12](#12-free-beat-placement-on-layer-song) Free beat placement.
3. ✅ [11](#11-euclidean-rhythm-page-euclid) EUCLID page.
4. ✅ [1](#1-lfo-modulated-values-dont-move-on-the-display) LFO values on the display.
5. ✅ [8](#8-limiter-setting-does-nothing) LIMITER.
6. ✅ Reproducible release builds (from [10](#10-housekeeping)), before tagging V1.10.

**Sprint 2, V1.11 "Formats and sound"** (done, tested on hardware): ✅ [14](#14-euclid-velocity-and-variance) part A ACCENTS, ✅ [9](#9-songkit-file-format-version) file format version, ✅ [2](#2-instrument-pitch-should-step-in-semitones-and-octaves) pitch in semitones, ✅ [13](#13-sample-preview-while-browsing) sample preview.

**Sprint 3, "Routing phase 3":** decide [7](#7-routing-open-design-questions), then [4](#4-routing-multiple-destinations-per-source) and [5](#5-routing-send-levels).

**V1.12 "Song mode"** (done, tested on hardware, released as v1.12): ✅ [15](#15-song-mode-chain-the-banks) song mode.

**Owner's priorities (2026-11-07), planned in sprint 4 below:** [34](#34-midi-clock-slave-drift-and-200-bpm-runaway) MIDI clock slave drift (with 30, same setup), [32](#32-autosave) autosave (work gets lost today) and [33](#33-per-note-editing-velocity-probability-and-more) per-note velocity and probability (it absorbs 14 part B and 21, and gives 30's incoming velocity somewhere to go).

**Sprint 4, "MIDI, autosave and notes" (proposed 2026-11-07).** Two releases, so the MIDI fix and the safety net reach the device first.

*V1.13 "MIDI and autosave"* (done, tested on hardware, released as v1.13)
1. ✅ MIDI IN that loses nothing: USART1 RX FIFO or a circular DMA ring, an overrun counter shown on the test page, and a hardware-free parser (`MidiParser.h`) with running status, Note On velocity 0 = Note Off, and a 16-event queue. From [30](#30-midi-inout-velocity-running-status-note-queue-cc); CC input stays for later. (S–M)
2. ✅ Incoming notes play at their velocity; MIDI out sends the velocity that played. From [30](#30-midi-inout-velocity-running-status-note-queue-cc). (S)
3. ✅ Clock slave locked to the incoming ticks (`MidiClock.h`): the playhead never runs ahead of the clock, microsecond tempo measurement, fractional tempo inside; no independent clock out while slaved (forward the incoming ticks or none), doubled ticks rejected. [34](#34-midi-clock-slave-drift-and-200-bpm-runaway). (M)
4. ✅ Autosave, part A: checksum-based change detection, AUTOSAVE OFF/2/5/10/20 min, two alternating reserved files written via rename, `RESTORE AUTOSAVE?` at power-on, autosave before LOAD over unsaved work. [32](#32-autosave). (M)
5. ✅ Hardware test with Loopy Pro → Scarlett: 30 minutes at a fractional tempo in time, no runaway with a MIDI loop set up on purpose, overrun counter at 0 with clock and dense notes, power pulled mid-session restores the autosave. Then package, app (MIDI and autosave explained in their tabs), release.

*V1.14 "Notes"* (done, tested on hardware, released as v1.14). Also in V1.14, asked by the owner: undo/redo ([28](#28-undo-for-bank-edits-lxr)), SONG + pads never turn the chain off, and the left S key (STYLE) became SOLO, with STYLE moved to a field on instrument page 3. After a freeze on the owner's device (cause not found yet): the clock lock no longer speeds up the sequencer timer while stopped, autosave shows SAVE / SDERR and its duration, and a crash now restarts the firmware and leaves a record (`CrashRecord.h`, `System/Crash.log`). The watchdog for stalls that are not faults is built too (6 s, `CRASH 7`, see the status of the crash record). First UX items, also in V1.14: per-note marks and dimmed lanes on the main screen ([38](#38-per-note-settings-and-layer-state-visible-in-the-lanes)) and the Euclid rings view over the lane area ([45](#45-full-area-views-mix-note-editor-sample-editor-patchbay-performance)). The Song timeline and Routing views followed, and the CPU and MEM monitor. The Filter and EQ curves followed too, and the filters got a 24 dB slope and a real 6 dB one ([50](#50-filter-slopes-24-db-and-a-real-6-db)). Next view: the sample view or the dub mixer.
6. ✅ Per-beat data block after EOF (`BTX`, 4 bytes per beat with room for PITCH), carried through move, rebuild, copy/paste, the bank clipboard, Euclid and record; old songs load at FLAT/100%. [33](#33-per-note-editing-velocity-probability-and-more). (M)
7. ✅ NOTE page with VEL and PROB (plus undo/redo of beat and bank edits, [28](#28-undo-for-bank-edits-lxr)); MIDI recording stores the incoming velocity; ADD + turn changes the whole bank. (S–M)
8. ✅ COND (`A:B`, `!A:B`, `1ST`, `PRE`, `FILL` and negations) with the FILL performance key. (M)
9. ✅ NUDGE. (S)
10. Hardware test, package, app (a Notes tab to try VEL/PROB/COND on a pattern), release.

Every step keeps the rules: hardware-free logic in `Core/Library/Global/*.h` with host tests, `make WERROR=1` clean, and songs from V1.12 load unchanged. Left for after sprint 4: phase B of 33 (PITCH, DECAY per beat), 14 part B (EUCLID VELOCITY, now just a way to fill VEL), CC input from 30, autosave history (32 B).

**After sprint 4 (ideas from the LXR firmware):** [18](#18-swing--shuffle-lxr) swing, [19](#19-choke-groups-lxr) choke groups, [22](#22-live-roll-lxr) live roll, [25](#25-lfo-retrigger-and-free-running-lxr) LFO retrigger, [27](#27-erase-while-recording-lxr) erase while recording. Later, together with [14](#14-euclid-velocity-and-variance) part B (they share one per-beat data extension and one file format bump): [20](#20-parameter-locks-per-beat-lxr), [21](#21-probability-per-beat-lxr), [24](#24-velocity-modulates-a-parameter-lxr).

**UX and signal path (analysis of 2026-11-13, [`docs/ux-and-signal-path-analysis.md`](docs/ux-and-signal-path-analysis.md); mockups in the app's Screen ideas tab).** Suggested order:
1. [37](#37-measure-first-drawing-speed-loop-time-audio-load) measurements, then [38](#38-per-note-settings-and-layer-state-visible-in-the-lanes) and [39](#39-graphic-overviews-note-euclid-song-routing): what V1.10–V1.14 added becomes visible.
2. [42](#42-key-hints-status-line-page-dots-value-popup) and [43](#43-fonts-with-lowercase-and-symbols-colour-roles): the device explains itself.
3. [40](#40-curves-and-cursors-on-the-sound-pages) and [41](#41-live-feedback-layer-meters-and-status-indicators): sound pages and live feedback.
4. Full-area views from [45](#45-full-area-views-mix-note-editor-sample-editor-patchbay-performance) that need no new engine work (Euclid rings, Song timeline, Routing, Filter curve): the owner wants the big main area used for dedicated views. Then [44](#44-notes-triggered-outside-the-main-loop-draw-budget), [46](#46-sends-a-level-per-layer-into-the-shared-effects-sends-mode) sends with the MIX page, [47](#47-channel-filter-and-drive-per-layer) channel filter, [36](#36-per-layer-panning) A pan, and the MIX view from [45](#45-full-area-views-mix-note-editor-sample-editor-patchbay-performance).
5. [48](#48-master-eq-stage-stereo-returns-a-third-send-effect) only after the measurements.

The sends, the channel filter and the MIX view are now designed as one dub mixer page ([49](#49-dub-mixer-page-dub-mode)), in phases D1–D5; its own open decisions are listed there. Open decisions for the owner: sends as a per-song mode next to today's chain (recommended) or a replacement; whether the channel filter replaces Filter 1/2; whether a third send effect is worth giving up undo history or a sample slot.

**On hold:** [16](#16-kids-firmware) kids firmware (shape decided, waiting for the owner's go).

**Later:** [3](#3-lfos-cant-modulate-filters-or-effects) (needs 9), together with [24](#24-velocity-modulates-a-parameter-lxr), [35](#35-lfo-destinations-for-the-per-note-settings) and [36](#36-per-layer-panning) part A (one format extension for the new per-layer bytes), [6](#6-routing-extra-effect-slots) (needs CPU measurement), the rest of [10](#10-housekeeping) alongside other work.

---

## LFO and instrument

### 1. LFO-modulated values don't move on the display

**Status: done in V1.10.** While playing, a field with an active LFO (and the layer's STYLE on) shows what the last hit used, in yellow; when stopped it shows the set value again. `layer_playBeat()` stores the values in `LayerPlayData::shownValue`, and `lcd_drawLayerInstLfoValues()` redraws at most 20 times a second. The LFO page playhead is not done.

**What happens.** With an LFO assigned to a layer parameter (level, pitch, start, …), the sound changes, but the field on the instrument page keeps showing the base value.

**Why.** LFO modulation is sample-and-hold at note trigger. `Controller::layer_playBeat()` reads each assigned LFO and computes `nLevel`, `nPitch`, `nStart`, `nEnd`, `nTiming`, `nProbability`, `nResolution`, `nDirection` as **local variables**, applies them to the note (`BeatPlayData`), and keeps nothing. The `lcd_drawLayerInst_*Data()` functions draw `layer.level`, `layer.pitch`, … only.

**Proposal.**
- Store the last applied values per layer, for example in `LayerPlayData` (`lastLevel`, `lastPitch`, …), written in `layer_playBeat()`. That's a few bytes per hit and nothing per sample.
- While a layer has an LFO on a parameter and the sequencer runs, show the last applied value. Either replace the value in a second colour, or show it in the sign box next to the LFO label. Refresh from the main loop at most ~20 Hz, not from the audio interrupt.
- Optional: a moving playhead on the LFO page graph (`lcd_drawLfo_GraphData()`) from `lfo.sampleCounter`.

**Done when.** Level with a sine LFO, playing: the LEVEL field visibly follows each hit. When stopped, it shows the base value again. No flicker, and no change in DSP load (check with `make PROFILE=1`).

### 2. Instrument pitch should step in semitones and octaves

**Status: done in V1.11** (`Pitch.h`, `test_pitch`). −24 to +24 semitones; a fast turn steps to the next octave line; SHIFT still picks the pitch LFO; LFO depth ±12 semitones. Layers from older files keep their exact old pitch (shown as before, e.g. `1.37`) until PITCH is turned, then snap to the nearest semitone. Not done: anti-aliasing above 2× (linear interpolation only, so pitching up more than an octave can sound harsh on bright samples).

**What happens.** PITCH on instrument page 0 is a playback-speed factor from 0.50 to 2.00 in steps of 0.01 (`kLayerPitchDataLibrary`, 151 entries, index 50 = 1.00). So it covers only one octave down and one up, and most values are not musical intervals. Patterning-style use (dial a pitch in semitones, jump by octaves) is not possible.

**Proposal.**
- Musical scale: index = semitones from −24 to +24 (or ±12), factor = 2^(n/12). Shown as `-12`, `+7`, `+12` and so on.
- Encoder steps one semitone; with SHIFT it steps one octave (12). SHIFT on this encoder currently picks the LFO for pitch (`encoder_incValue`, `ENC_INST_0_PITCH`), so a different modifier or a setting is needed; decide on the device.
- LFO on pitch: today it adds up to ±100 table steps. With the new scale, define depth in semitones.
- Check the sample player at factors above 2.0 (the `increment` in `BeatPlayData`, end-of-sample handling, aliasing) before allowing +24.

**Compatibility.** Kits and songs store the pitch index (layer record byte 1). Old files must keep their pitch. Either convert the old linear index to the nearest semitone on load (changes some pitches slightly) or keep both scales with a per-layer flag. Needs item 9 (format version).

**Done when.** Pitch steps in semitones and octaves on the device, old kits sound the same (or as documented), and the table and conversion have host tests.

### 3. LFOs can't modulate filters or effects

**What happens.** Confirmed in code: LFOs have exactly eight destinations, all per layer: level, pitch, direction, resolution, start, end, timing, probability (`Layer::lfoLevel … lfoProbability`, applied in `layer_playBeat()`). Nothing connects an LFO to EQ, the filters, the effects or the reverb. This is a missing feature, not a bug.

**Proposal.**
- Module destinations, e.g. filter frequency/resonance, effect parameters A–D and wet, reverb wet. Unlike layer targets these are continuous, so update them per audio block, not per hit.
- Smooth the value per block to avoid zipper noise. For filters, recomputing biquad coefficients per block is the main cost: measure with `make PROFILE=1` (V1.04 worst case: 34% of the block, so there is headroom).
- UI: the module menus have no free encoder slots. Reuse the layer pattern (SHIFT + encoder picks the LFO for that parameter, as instrument page 0 does).
- Files: one byte per assignable parameter; find spare bytes in the kit section or use item 9.

**Done when.** An LFO on filter 1 frequency sweeps audibly with no clicks, UDR/OVR stay 0 in the profile build, and assignments save and load.

---

## Routing, phase 3

Phases 0–2 of the routing design are done (V1.05–V1.09): routing engine, layer OUT, ROUTING page (MIXER twice) with overview, click-free changes, saved routing. The engine and files were built with these next steps in mind.

### 4. Routing: multiple destinations per source

E.g. a layer to MASTER and REVERB at once (dry plus reverb send).

- **Engine:** done. Destinations are bitmasks; `routeBlock()` and the crossfades handle several bits, and `test_routing` covers it.
- **UI:** OUT fields choose one destination. Needs a way to toggle several (e.g. press the encoder to add or remove the shown destination), and the overview shows the first only (`routeFirstNode`).
- **Files:** layer masks are already written (layer record byte 23) but the four old settings (bytes 16–19) are authoritative on load. With the marker present, load byte 23 instead, and keep writing the nearest single entry point to 16–19 for older firmware.

### 5. Routing: send levels

A level per route, so a layer can go to MASTER at full level and to REVERB at 30%.

- Needs a gain per (source, destination) in the scatter step. `routeScatter()` already takes an optional per-sample gain for the crossfades.
- UI: the three empty slots on instrument page 2 (tabs 5–7) were kept free for this.
- Files: bytes 175–179 and 499 of the kit section are reserved.

### 6. Routing: extra effect slots

- **Memory:** SDRAM 0xC0EA6000–0xC0FB9640 (1,128,000 bytes, between the chorus buffers and the images) is not referenced anywhere in the code (static check). Two more effect slots need 960 KB (delay + chorus buffers). Confirm at runtime that nothing writes there.
- **CPU:** measure the worst case with `make PROFILE=1` before and after. Gate: < 80% of the block, UDR/OVR = 0.
- **Routing:** the node count goes from 7 to 9, so masks need more than the 7 bits of a `uint8_t` (use `uint16_t`). Update the file layout and `kRouteDestDataLibrary`.
- **UI:** new effect menus and keys. The keypad has no free menu keys; consider a second page on EFFECT 1/2.

### 7. Routing: open design questions

Carried over from the design document; decide before building:
- User-set module order (e.g. FX 1 before Filter 1): cheap in the engine (an order array), harder to read in the UI.
- Routing presets that can be recalled across songs, or routing per song only (today: per kit/song file).
- Per-layer pan before the master: one multiply per sample per layer in the scatter step.

---

## Bugs and groundwork

### 8. LIMITER setting does nothing

**Status: done in V1.10.** LIMITER is now the output ceiling: the soft-knee clip in `processAudioSend()` is scaled to it. At +0 dB (the default) the output is unchanged.

The mixer's LIMITER (0–10) is saved in the system preset (byte 6), shown and editable, but no audio code reads `mixer.limiter`. The output only has the fixed soft-knee clip (`softKneeClip`, threshold 0.667) in `processAudioSend()`. Either make LIMITER set the clip threshold or a real limiter, or remove the field.

### 9. Song/kit file format version

**Status: done in V1.11** (`FileFormat.h`). Kit section byte 496 = version, 497 = check byte; covers kit and song files. Old files are version 0; a newer file is refused before anything changes. Version 1 = semitone pitch in layer record byte 24.

Kit and song files have a signature but no format version. Several items above change what is stored (pitch scale, multi-destination routing, LFO module targets, fill types). Add a version byte in a spare kit-section byte (and the song section) so new firmware can convert old files and fail cleanly on newer ones. The routing marker (`0xA5` at kit byte 498) is the pattern to follow.

### 10. Housekeeping

- **Fill types:** 15 of 16 possible. The type is stored in 4 bits; a 17th type needs item 9.
- **`ENC_ROUTE_LINEIN_OUT`:** still has encoder handlers but is no longer on the ROUTING page (line-in is set with the mixer's MIXER IN). Remove it or put it back on the page.
- **Releases:** the Release workflow only runs for tags starting with lowercase `v` (`v1.10`). The `V1.04` release has no build attached for that reason.
- ~~**Reproducible release builds**~~ (done in V1.10): the v1.09 release file differed from the local build by 4 bytes of `.data`. Same toolchain packages; the cause was link order, because the Makefile listed sources with `find`, whose order depends on the file system. The lists are now sorted. Rebuilding the v1.09 commit locally gives exactly the file tested on hardware.
- **Vendor warnings:** two GCC warnings remain in FatFs (`ff.c`), both known false positives. Our code is held at zero (`make WERROR=1` in CI).
- **Bootloader:** it ignores update files with a long name (FatFs `_MAX_LFN` 32 says 32+, but a 31-character name was ignored on the owner's device on 2026-11-14), so `package_firmware.py` now enforces ≤ 28. It also takes only the first file it finds: copy one package at a time. (The "cache runs fine" report of that day may have been the old firmware: the 31-character `_dcache` file was ignored, so the cache build is tested again as `_dc`.)
- **Upstream:** offer the fixes and the routing work to Randomwaves/Drumboy-Pro as a pull request from the fork.

---

## Sequencer and samples

### 11. Euclidean rhythm page (EUCLID)

**Status: done in V1.10** (`Euclid.h`, `test_euclid`, `euclid_*` in `Controller.cpp`). On the song page, tap S (right side, L I S row) on its own, or hold S and press the same layer pad again. Fields STEPS, HITS, ROTATE, GRID, MODE; CLEAR restores the layer.

**Today.** The only beat generator is a long ADD press on the Layer Song page, and only when the layer's bank is empty (`layerBeatKeyFlag` is set only when `lastActiveBeatNum == -1`). Each step of the hold runs a fixed preset from `layerSong_generateBeat()`: one beat per bar, two off-beat layouts, then BASIC_4 and BASIC_8 fills on every beat. Nothing is adjustable, and an existing pattern can't be extended. (The EUCLID *fill type* in `Fill.h` is different: it places hits inside one beat, not beats across the bar.)

**Proposal.** A EUCLID page for the selected layer and bank, opened the same way as ROUTING (press the Layer Song key again):
- **STEPS** 1–32, **HITS** 0–STEPS, **ROTATE** 0–STEPS−1, **GRID** (a quantize value from `kQuantizeInterval`, e.g. 1/16 = 200 ticks, since `kMeasureInterval` = 800 per quarter note). The pattern repeats to the end of the song (`songInterval`).
- Changes apply live while the sequencer plays: rebuild the bank's beats and call `layerSong_calculatePlayBeatNum()`, as `layerSong_generateBeat()` does now.
- **MERGE** adds the Euclidean hits to the beats already there. A hit that lands on an existing beat keeps that beat and its fill.
- **CLEAR** restores the bank as it was when the page was opened. Keep a snapshot in a `LayerSongCopy`-style buffer; don't use the copy/paste clipboard.
- Put the distribution (Bjorklund / Bresenham) in a hardware-free header (e.g. `Euclid.h`) with host tests in `02 Software/tests`: hit counts, rotation, known patterns such as E(3,8) = `x..x..x.` and E(5,8).

**Limits to respect.** A bank holds 64 beats (`kBeatLibrarySize`) and 32 fills (`kBeatFillLibrarySize`). 32 steps over several bars can exceed 64, so stop at the limit and show it. Every rebuild must keep beats sorted with correct end intervals (`layerSong_arrangeActiveBeats()`).

**Done when.** E(3,8) and E(5,16) sound right on the device while playing; MERGE keeps the beats and fills already there; CLEAR restores exactly what was there; songs save and load unchanged (the result is ordinary beats, so no file format change).

### 12. Free beat placement on Layer Song

**Status: done in V1.10** (`BeatList.h`, `test_beatlist`). MOVE on tab 6 (or ADD + LEFT/RIGHT), SHIFT ALL on tab 7, short ADD press inserts. Also fixed: recording into a full bank wrote past the beat list, and the old sort skipped the last beat.

**Today.** SHIFT + LEFT/RIGHT (`layerSong_shiftSelectedBeat()`) moves the selected beat one quantize step, but it stops at the neighbouring beat and resets the fill of the moved beat and of the one before it (`setBeatFill(…, 0, 0, 0, 0)`). There is no way to shift a whole layer, and the only way to add a single beat is live recording (`record()`).

**Proposal.**
- **MOVE:** the selected beat steps one quantize step and can pass its neighbours. Take it out, insert it at the new position, re-sort and fix the end intervals (`layerSong_arrangeActiveBeats()`), and keep the selection on the moved beat. Its fill is kept; recompute it for the new length with `layerSong_calculateBeatFill()` rather than clearing it. Show the exact position as bar.beat.16th (e.g. `2.3.4`) while moving.
- **SHIFT ALL:** rotate every beat of the layer's bank by one quantize step, wrapping at `songInterval`. Reversible: rotating back returns the identical pattern, fills included.
- **Insert:** a short ADD press adds one beat in the middle of the gap after the selected beat, snapped to the quantize grid (`layerSong_setBeat()`). A long ADD press keeps its current meaning.
- Put the move, rotate and insert arithmetic on the beat list in a hardware-free header with host tests (wrap-around, passing neighbours, full bank, rotation followed by its inverse).

**Done when.** A beat can move anywhere in the bar while keeping its fill; SHIFT ALL left then right gives back the identical bank; inserting into a full bank (64 beats) is refused with a message; all of it works while playing without glitches.

### 13. Sample preview while browsing

**Status: done in V1.11** (`Preview.h`, `test_preview`). PREVIEW and PREV LEVEL on the Metronome page (tabs 4–5); 256 KB ring in the SDRAM gap at 0xC0EA6000; FatFs `_FS_LOCK` raised to 3. The SDRAM gap was only checked statically (nothing in the code references it); watch for any glitch in the preview that could mean something else writes there.

**Today.** On the instrument page, turning SAMPLE only selects a file (`layerInst_setSampleSelected()` reads its WAV header). To hear it, you have to load it with ADD (`layerInst_setSampleLoaded()`), which replaces the layer's sample, copies the whole file from SD to SDRAM and calls `keyboard_disable()` until it's done. The sample is cut at 5.44 s (`kSampleSize` = 240,000 samples).

**Proposal.**
- When the SAMPLE encoder stops (e.g. 250 ms after the last step), play the selected sample on a separate preview voice. The layer key replays it. The layer's loaded sample doesn't change until ADD.
- Stream the preview from the SD card through a small ring buffer in SDRAM, filled from the main loop in chunks, so even long files start at once and nothing locks the keyboard. Stop the preview when another sample is selected, the menu is left or ADD loads it.
- The audio interrupt only reads the ring buffer. FatFs is not reentrant, so every SD read stays in the main loop. When the buffer runs dry, fade out instead of clicking.
- **PREVIEW** on/off and **PREVIEW LEVEL** in the Metronome menu (which already has its own voice and level). The system preset has no free bytes (all 21 are used), so it needs to grow, and older system files (21 bytes) must still load.
- Measure with `make PROFILE=1`: one more voice plus SD reads while the sequencer plays. UDR/OVR must stay 0.

**Done when.** Turning through samples plays each one after a short pause, including files longer than 5 s; the keyboard never locks while browsing; the loaded sample is untouched until ADD; the preview setting survives a restart; no I2S errors in the profile build.

### 14. EUCLID: velocity and variance

**Today.** Every hit the EUCLID page makes is a plain beat at full velocity (BASIC / FLAT), so the pattern sounds mechanical. Plain beats have only three velocities: FLAT (100), GHOST (50) and MUTED (0), set by the beat's fill PATTERN; the VELOCITY field (FLAT, STEP, DESCEND, ASCEND, RANDOM) only shapes the micro-hits of a complex fill, not single beats. Variance is possible today only indirectly: an LFO on the layer's LEVEL (RANDOM type for humanising) with STYLE on changes each hit, and since V1.10 the instrument page shows the value used.

**Status: part A done in V1.11** (ACCENTS on EUCLID tab 7, `euclidAccents()` in `Euclid.h`). Part B open; the per-beat velocity exists since V1.14 ([33](#33-per-note-editing-velocity-probability-and-more)), so part B is now only a way to fill it from the EUCLID page.

**Proposal**, in two parts:

- **A. ACCENTS field (S, no file change).** A second Euclidean pattern over the hits: ACCENTS 0–HITS spreads that many accents evenly over the pattern's hits (and follows ROTATE). Accented hits stay FLAT (100); the others become GHOST (50). Uses the beat states that already exist, so songs load on any firmware. Tab 7 on the EUCLID page is free for it. Example: E(5,8) with 2 accents gives `X.xX.xx.` (X loud, x ghost; the accents are E(2,5) over the five hits). The device shows ghost hits as `+` (`X.+X.++.`): its font has no lowercase.
- **B. VELOCITY field (M).** Apply the fill VELOCITY modes across the Euclidean hits: FLAT, STEP (100/75 alternating), DESCEND and ASCEND over the cycle, and RANDOM (40–100) for variance; optionally a SPREAD amount (± random per hit, drawn once and stored, so the pattern stays the same on every loop). This needs a velocity per plain beat. The beat's 4-bit level field is unused on plain beats and is already saved and loaded, but the loader resets any level above 4 (`setBeatFill` range check), and older firmware plays all plain beats at FLAT/GHOST anyway. Decide the encoding together with [9](#9-songkit-file-format-version) (file format version), then play plain beats at their stored velocity in the sequencer (`kNumberDataLibrary[...]` multiplier instead of the fixed 100/50/0).
- Put the accent and velocity distribution in `Euclid.h` with host tests (accent spacing, rotation, RANDOM stored per hit, values stay within 0–100).
- The app's Euclid tab gets the same fields, so the result can be heard before building.

**Done when.** On the device, E(5,8) with 2 accents plays two loud and three ghost hits on every loop; with VELOCITY RANDOM the levels vary per hit but repeat identically each loop and survive save/load; older kits and songs sound the same.

### 15. Song mode: chain the banks

**Status: done in V1.12** (`SongChain.h`, `test_songchain`). SONG page: tap the SONG key (the one held to pick banks); fields STEP, BANK, REPEATS, LENGTH, END (LOOP/STOP), CHAIN. Decided: a manual bank pick jumps the chain to the step using that bank; since V1.14 a bank not in the chain plays one loop as a detour and the chain goes on (it used to turn chain play off, which the owner found confusing; `chainManualBank`); STOP keeps the position, RESET goes to step 1. The chain is saved after the song file's EOF (marker `CHN`), so older firmware ignores it and no format version change was needed. Also in V1.12: whole-bank copy/paste on the SONG page (long COPY / PASTE, all 10 layers; clipboard in SDRAM after the preview ring).

**Today.** Each layer has 5 song banks (`kBankLibrarySize`), and the active bank (`activeSongBank`) is the same for all layers. Holding the SONG key and pressing a pair of layer pads picks a bank (pads 1–2 = bank 1 … 9–10 = bank 5). While playing, `layerSong_triggerBank()` arms the change (`bankShiftFlag`, `targetSongBank`) and the sequencer switches at the next loop end; when stopped it switches at once (`layerSong_selectBank()`). Nothing plays the banks in sequence: every change is by hand.

**Proposal.**
- **Chain:** a list of up to 16 steps, each a bank and a repeat count (1–16 loops), for example `1×4, 2×2, 1×4, 3×1`. Optionally an END step that stops, or the chain loops.
- **Playback:** with chain play on, count loops at the loop end (where `bankShiftFlag` is handled today) and, when a step's repeats are done, switch to the next step's bank exactly like a manual trigger. A manual bank press while the chain plays either jumps the chain to that bank's first step or pauses the chain (decide on the device).
- **Page:** a SONG page (e.g. tap the SONG key, the same tap pattern as I and S) with the chain as a list: STEP, BANK, REPEATS, plus INSERT/DELETE step and CHAIN ON/OFF. The top-right area shows the chain with the current step and the repeats left; the bank tabs keep showing the active bank.
- **Transport and sync:** RESET goes back to step 1; STOP keeps the position (or resets, decide). With external sync, follow the incoming clock; loop ends are already counted by the sequencer, so nothing new is needed for sync.
- **Files:** the chain is saved with the song (about 33 bytes: count + 16 × bank/repeats). Needs [9](#9-songkit-file-format-version) so older firmware ignores it and older songs load with chain play off.
- Put the chain stepping (count, advance, END, jump) in a hardware-free header with host tests.

**Done when.** A chain such as `1×4, 2×2, 3×1` plays back by itself with bank changes exactly on loop ends, survives save/load, RESET restarts it, manual bank presses behave as decided, and songs without a chain play as before.

### 16. Kids firmware

**Idea (open, "we will see").** A separate, much simpler firmware for children on the same hardware: few pages, big obvious actions, nothing that can break a kit or song, safe volume.

**Approach to decide first.**
- **A build variant of this firmware** (e.g. `make KIDS=1`), sharing the audio engine, sample player, sequencer, files and bootloader, with a different, reduced UI layer. Recommended: every engine fix reaches both, and the hardware-free headers (`Fill.h`, `BeatList.h`, `Euclid.h`, `Pitch.h`, `Preview.h`) are reused as they are.
- **Or a KIDS MODE inside the normal firmware** (a locked mode entered with a key combination). Simpler to ship (one file), but the full UI code stays reachable, and every new feature has to be hidden from it.
- **Or a firmware written from scratch.** Most freedom, most work; only worth it if the kids version shares little.
- The update file must still match the bootloader's `Drumboy_Pro_Fw?*.bin` pattern and stay within 31 characters (e.g. `Drumboy_Pro_Fw_01_11_kids.bin`, 29). Switching between the normal and the kids firmware is then just flashing the other file.

**First ideas for what it does** (to try with a child before building much):
- Pads play sounds straight away; a fixed set of kits (animals, instruments, voices) chosen with one knob, no instrument/sample browsing.
- A simple loop: press record, play the pads, it repeats; CLEAR erases the loop (with a confirm). Maybe the EUCLID idea as one "make a rhythm" knob.
- One knob each for tempo, volume, and one fun effect (e.g. pitch in octaves, echo).
- Big, colourful screen; no numbers or menus deeper than one level.
- **Safety:** a hard cap on master and headphone volume (`kMaxMixerHpVolume` is 20 steps today), no file deletion, no firmware or SD maintenance actions, settings that can't get stuck.

**Decided (owner, 2026-11-02).** On hold until the owner says go.
- Ages 9–11.
- All the ideas above are in.
- A separate firmware file (the build variant above; switching = flashing the other file).
- Saving is not needed for the proof of concept.

**Still open.** Which kits ship with it, and whether 9–11-year-olds want more than one effect knob (try the POC with a child first).

**Done when.** To be defined once the shape is chosen; a first milestone could be a kids build with pads, kit knob, volume cap and a one-button loop.

### 17. Sample rate conversion (48 kHz and others)

**What happens.** The device plays at 44.1 kHz, but samples at several common rates are not converted, only repeated or thinned out by whole frames, so they play about 8.8% slow (roughly 1.5 semitones low, and 9% longer):

| File rate | What the loader does | Plays as |
|---|---|---|
| 8, 16, 24 kHz | repeats each frame 6, 3, 2 times | 48 kHz content at 44.1 kHz: 8.8% slow |
| 48 kHz | as is | 8.8% slow |
| 96, 192 kHz | keeps every 2nd / 4th frame, no filter | 8.8% slow, and can alias |
| 11.025, 22.05 kHz | repeats 4, 2 times | correct (but stair-stepped, no smoothing) |
| 44.1, 88.2, 176.4 kHz | as is / every 2nd / every 4th | correct (the last two can alias) |

Same rule in the layer loader (`layerInst_setSampleLoaded()`) and the sample preview (`Preview.h`, which copies it on purpose so the preview sounds like the loaded sample).

**Proposal.**
- A hardware-free `Resample.h`: convert any accepted rate to 44.1 kHz with a fractional step (ratio = file rate / 44100) and interpolation (linear at least; a short windowed-sinc or polyphase filter for downsampling 88.2–192 kHz so it doesn't alias). Used by both the loader and the preview, streaming-friendly (keeps its phase between chunks).
- Check the cost: the loader converts once at load (fine); the preview converts in the main loop per chunk (measure, it must still keep up; see the V1.11 throughput fix).
- The 5.44 s limit (`kSampleSize`) then counts in 44.1 kHz samples, so a 48 kHz file holds 8.8% less of its length than today; document it.
- **Compatibility to decide:** kits made with 48 kHz samples were tuned by ear to the slow playback and will sound about 1.5 semitones higher after the fix. Options: accept it (it's the correct pitch), or for files saved before the change (format version 1 or older, `FileFormat.h`) add -1.5 semitones back. Semitone pitch can't express that exactly, so an old-file flag would be needed.
- Related, optional: stereo files use only the left channel today; mixing to mono would keep both sides.

**Done when.** A 48 kHz and a 44.1 kHz recording of the same sound play at the same pitch and length, loaded and previewed; 96/192 kHz files don't alias; host tests for the converter (pitch of a sine at each rate, no clicks at chunk boundaries); the preview still never underruns on 48 kHz 24-bit stereo.

---

## Ideas from the LXR firmware (items 18–29)

Reviewed from the owner's fork of the Erica Synths LXR V2 desktop firmware (MiguelMoraleja/lxr-desktop-app, 2026-11-05). **Licence:** that firmware is GPL-3.0 and Drumboy-Pro is MIT, so its code must not be copied here (it would put the Drumboy firmware under the GPL), and "LXR" / "Erica Synths" are trademarks. Take the ideas only and write our own code. Not taken: the LXR's synthesised drum voices (the Drumboy is sample-based), its second stereo output (no such hardware), a kit per pattern (the Drumboy keeps one kit per song).

### 18. Swing / shuffle (LXR)

**LXR:** one shuffle amount per pattern that delays the off-beat steps. **Today:** only the SWING fill type, inside one beat; nothing swings the whole groove. **Proposal:** a SWING amount per song (or per bank) on the RHYTHM page that delays beats on off-beat 16ths (or 8ths) by a fraction of a step at playback time, without moving the stored beats, so it can be changed live and saved. Applies to fills' micro-hits too. Host tests for the timing.

### 19. Choke groups (LXR)

**LXR:** open and closed hi-hats cut each other off. **Today:** every layer rings over the others; cheap to add per layer. **Proposal:** a CHOKE group per layer (OFF, 1–4) on an instrument page; a hit fades out (a few ms, no click) any other layer in the same group still sounding. Saved per kit (needs a spare byte; see item 9).

### 20. Parameter locks per beat (LXR)

**LXR:** each step can set up to 2 parameters (e.g. pitch, decay), recorded by turning a knob while recording. The biggest creative gain of the list. On the Drumboy it is per beat, built on the beat list (`BeatList.h`, item 12), and needs a format version bump (item 9 makes that possible). **Today:** parameters only per layer or through an LFO. **Proposal:** up to 2 locks per beat (parameter + value) applied at the hit like the LFO values in `layer_playBeat()`; set by turning an instrument knob while a beat is selected (or while recording). Needs per-beat storage beyond `Beat` (16-bit data is full): extend the song section with item 9; plan together with 21, 24 and 14B.

### 21. Probability per beat (LXR)

**LXR:** a 0–100% probability per step. **Today:** one PROB per layer. **Proposal:** a probability per beat (and per fill micro-hit?) on the song page, rolled at each hit. Same per-beat storage as 20. **Done in V1.14** as CHANCE on the NOTE page ([33](#33-per-note-editing-velocity-probability-and-more)), per beat; a fill plays or not as a whole.

### 22. Live roll (LXR)

**LXR:** hold ROLL and a pad: the pad repeats at a set rate (1/16, 1/32, …) in time with the sequencer, with the step probability applied. **Today:** rolls only as fill types. A performance feature that sits next to fills; apply the layer's PROB to each repeat. **Proposal:** a roll modifier (key combination to choose) + pad repeats the layer at a rate (1/8, 1/16, 1/32, triplets) locked to the tempo while held; while recording, the repeats are written as beats (or as one beat with a ROLL fill).

### 23. Per-layer length / polymeter (LXR)

**LXR:** each track has its own length (e.g. 5 steps against 16). **Today:** every layer loops over the song length. **Proposal:** a LENGTH per layer and bank (in 16ths, up to the song length); the layer's beats wrap at its own length. Larger change for the free-placement beat model (playback pointers per layer, drawing the lane); host-test the wrap.

### 24. Velocity modulates a parameter (LXR)

**LXR:** `modulationNode` sends each hit's velocity to any parameter. **Today:** velocity only changes volume. Pairs with the MIDI velocity fix in 30 (incoming velocity becomes a real value) and is a cheaper first step toward 3 (LFO → filters/FX): the same "modulate a module parameter per hit" plumbing. **Proposal:** per layer, a VELO → destination (pitch, start, filter, ...) and amount, applied at the hit. Needs per-beat velocity (14B).

### 25. LFO retrigger and free-running (LXR)

**LXR:** an LFO can restart on each note of a chosen voice, can run synced or free, and has exponential up/down shapes. **Today:** LFOs are tempo-synced, never restart, and have SIN, TRI, SAW, SQR, RND. **Proposal:** per LFO a RETRIG source (OFF or a layer): a hit on that layer resets the LFO phase; optionally a FREE mode in Hz; add EXP UP and EXP DOWN shapes (`kLfoDataLibrary`, Lfo.h). Small; Lfo.h + `layer_playBeat()`.

### 26. Random step in the song chain (LXR)

**LXR:** each pattern says which pattern comes next, including "random among the previous ones", and switches every N bars. **Today:** song mode is a fixed chain (item 15) whose REPEATS already cover "every N bars". Adding RANDOM as a step type costs almost nothing. **Proposal:** a step's BANK can be RANDOM (any bank, or not the same as the last); `SongChain.h` with a seeded random for tests; file block version 2.

### 27. Erase while recording (LXR)

**LXR:** while recording, hold a key and the hits that pass the play position are erased. **Today:** only CLEAR (one beat or the whole layer). **Proposal:** while recording, hold CLEAR + a layer pad: beats of that layer under the play position are removed as it passes.

### 28. Undo for bank edits (LXR)

**Status: done in V1.14** (`UndoLog.h`, `test_undo`). Multi-level undo and redo (MUTE + LEFT / RIGHT) for every beat and bank edit: 64 entries of one bank each in SDRAM after the bank clipboard (about 185 KB); undo/redo swap the stored bank with the live one under the interrupt lock; a knob gesture (2 s), an EUCLID visit and a recording take are one step; a whole-bank paste is one step of 10 banks. The history is cleared on LOAD, NEW, restore and when MEASURE/BAR/QUANTIZE change the grid. Sound settings (knobs) are not undoable (see the owner's choice, 2026-11-08: note level is enough).

**LXR:** a backup buffer to return a pattern to how it was. **Today:** only EUCLID CLEAR. **Proposal:** one level of undo per bank for destructive edits (paste, EUCLID apply, CLEAR all, SHIFT ALL): snapshot before, a key combination to swap back. The SDRAM gap has room (one bank of 10 layers is ~28 KB, like the bank clipboard).

### 29. New effects: ring modulator, tilt EQ, master saturation (LXR)

**LXR:** ring modulator and compressor on the FX bus, a tilt EQ, and a tube-style saturation on the master. **Today:** 9 effect types in the two effect slots (no ring mod), a 6-band EQ, a soft-knee clip at the LIMITER ceiling. **Proposal:** ring modulator as a 10th effect type; a TILT control (one knob, dark ↔ bright) on the mixer; an optional warm saturation before the output clip. Measure DSP load for each (`make PROFILE=1`).

### 30. MIDI in/out: velocity, running status, note queue, CC

**Status: done in V1.13 except CC input** (`MidiParser.h`, `test_midi`). Running status, velocity 0 = note off, a 16-event queue, notes at their velocity (soft notes record as GHOST until 33), MIDI out with each hit's velocity (FLAT 127, GHOST 64), UART RX FIFO on and receive interrupts above the sequencer timer, receive errors shown as `ERR nnnn` instead of SLAVE on the Rhythm page. CC input still open.

Matters for playing the Drumboy from Loopy Pro (through a Scarlett interface). Checked in the code (`Controller.cpp`: the MIDI byte parser and the inbound processing in the main loop):

- **Velocity ignored:** the parser stores the note-on velocity, but the main loop plays every received note at full level (`layer_playBeat(layerNum, 1.0f)`) and records it as a FLAT beat.
- **Note On with velocity 0** (sent by many controllers instead of Note Off) plays a hit at full level.
- **No running status:** each note must start with its own status byte (`rxNoteOnReadStage` only leaves stage 0 on a status byte), so when a controller omits repeated status bytes, the second and later notes of a run are dropped.
- **One pending note:** a single slot (`rxNoteOnKey` / `rxNoteOnWriteFlag`); a second note arriving before the main loop handles the first overwrites it, so two notes landing together lose one.
- **MIDI out velocity** is always 100 (0x64) in Note On and Note Off.
- **No CC input** at all (the LXR maps its parameters to CC and NRPN).

**Proposal.**
- A hardware-free MIDI parser (e.g. `MidiParser.h`) with running status (data bytes reuse the last channel status; real-time bytes 0xF8–0xFF don't cancel it, other system messages do), note-on velocity 0 = note off, and a small queue (e.g. 16 events) between the UART interrupt and the main loop. Host tests with byte streams: running status runs, interleaved clock bytes, chords, velocity 0.
- Play received notes at their velocity (cubed like the other levels, or as is; decide by ear), and record them with it once per-beat velocity exists (14B); until then, map low velocities to GHOST.
- MIDI out sends the velocity the hit actually played with (fill level × FLAT/GHOST).
- CC input (M): a fixed CC map for the main per-layer parameters (level, pitch, start, end, …) and the mixer, on the receive channel; learn mode later. NRPN not needed at first.

**Done when.** From Loopy Pro: soft and hard notes play at different levels, a fast run with running status plays every note, two simultaneous notes both play, velocity-0 notes are silent, and the MIDI monitor shows the real velocity on MIDI out.

### 31. Kit morph (LXR)

**LXR:** one knob crossfades every sound parameter of the kit toward a second kit; the interpolation is spread over audio ticks, one parameter per tick, so it never spikes the CPU; routing is excluded. **Today:** nothing; a kit is loaded whole. **Proposal:** load a second kit as the morph target (parameters only, not samples: a layer whose sample differs keeps its own), a MORPH knob 0–100% that interpolates level, pitch, start/end, filter, EQ, effect and reverb parameters, updated a few parameters per audio block; routing (OUT and the ROUTING page) and samples stay as they are, which matches the routing design. Host-test the interpolation (endpoints exact, no parameter left half-updated across a full sweep).

### 32. Autosave

**Status: part A done in V1.13** (`Autosave.h`, `test_autosave`). AUTOSAVE OFF/2/5/10/20 min on Metronome tab 6 (system preset byte 23); files `System/File/Autosave_A.rwf` / `_B.rwf` sealed with a trailer instead of written via rename (a cut-short write fails the CRC, the other file survives); SAVE deletes them; LOAD and NEW autosave unsaved work first. Part B open.

**Today.** Nothing is saved unless SAVE is used on the FILE page (`file_saveAction()` → `sd_saveFile()`), so a power cut, a crash or loading another song without saving loses the work. A song file is small (kit 1,500 bytes + 10 layers × 1,286 bytes + the 38-byte chain block, about 14 KB) and is written in one go with the keys disabled (`keyboard_disable()`), so a save takes tens of milliseconds. The system and encoder presets already use a "dirty flag, save later" pattern (`systemPresetSaveFlag`).

**Proposal**, in two parts:

- **A. Simple autosave (M).**
  - **When something changed:** instead of hooking every edit, keep a checksum of the song state (kit block, the song banks, routing, chain) and compare it every minute; the STM32H723 has a hardware CRC unit, and 14 KB takes well under a millisecond. Changed since the last save (manual or auto) = dirty.
  - **When to write:** an AUTOSAVE setting on the Metronome/system page: OFF, 2, 5, 10 (default), 20 minutes, stored in the system preset (byte 23, growing it to 24 bytes the way the preview bytes did). Write only when dirty and safe: not recording, no precount, no load or save in progress; while playing, at a loop end, so the short SD write never lands in the middle of the user's gesture. Decide on the device whether writing during playback is inaudible (the audio runs in the interrupt, so it should be); if not, autosave only while stopped plus once on STOP.
  - **Where:** never over the user's numbered slots. A reserved file outside the 1–99 library (e.g. `Song/Autosave/AUTO_A.rwf` and `AUTO_B.rwf`), written alternately, each first to a `.tmp` name and then renamed, so a power cut during the write always leaves the previous good copy. Each file also records the slot it came from and a running sequence number (the board has no battery-backed clock, so no wall-clock time).
  - **Restore:** at power-on, if the newest autosave is newer than the last manual save, show `RESTORE AUTOSAVE?` (YES loads it like a song file, NO keeps the normal start). It also appears as an AUTOSAVE entry on the FILE page, so it can be loaded and then saved into any slot.
  - **Safety net for LOAD:** loading another song while the current one is dirty writes an autosave first.
  - Hardware-free logic (`Autosave.h`): the timer and dirty decision, which of A/B to write, which is newest, the restore decision, with host tests (power cut mid-write simulated by an unrenamed `.tmp`, sequence wraparound, setting OFF).
- **B. History and branching (M–L), later.** Keep the last N autosaves per song slot (e.g. 8, as a ring in `Song/Autosave/NN/`), browsable on the FILE page with their age in minutes and the slot they belong to. Loading one and saving it into a new slot is the "branch"; the autosave history then follows the new slot. Pairs with [28](#28-undo-for-bank-edits-lxr) (undo inside a session); autosave covers the time between sessions.

**Done when.** With AUTOSAVE 10, editing and then pulling the power loses at most 10 minutes of work; the next power-on offers the restore; pulling the power during an autosave write still leaves a loadable autosave; manual slots are never touched by autosave.

### 33. Per-note editing: velocity, probability and more

**Status: phase A done in V1.14** (`BeatNote.h`, `test_beatnote`). Stored in the Beat itself in 3 bytes (velocity + a 16-bit word: chance in 5% steps, 51 conditions, nudge ±12 steps of 2 ticks), because internal RAM is tight (Beat 8 → 10 bytes, about 4 KB left); the BTX block is sparse (7 bytes per beat with settings) and matched by start on load. !A:B only for B up to 4 and no NEI, to fit 6 bits. Phase B (PITCH, DECAY) needs RAM: move the song banks to SDRAM first.

**Today.** A beat (a note on Layer Song) has a start, a fill (type, pattern, time, level) and an active flag, stored in 4 bytes per beat in the song file (257 bytes per bank per layer). Plain beats only have FLAT (100), GHOST (50) and MUTED (0). Probability exists only per layer (PROB). Incoming MIDI velocity is ignored ([30](#30-midi-inout-velocity-running-status-note-queue-cc)), and MIDI out always sends 100.

**Proposal.** One per-beat extension that several items need, done once:

- **Data:** four extra bytes per beat: VELOCITY (0–127), PROBABILITY (0–100%), CONDITION (7 bits, see phase A) and NUDGE (signed ticks), with PITCH in a fifth byte when phase B starts; reserve the room now so the block version only changes once. In memory next to `beatLibrary`; in the file as a block after the song's "EOF" with its own marker (e.g. `BTX`), the same trick as the song chain: older firmware stops at "EOF" and ignores it, and a file without the block loads with velocity from FLAT/GHOST/MUTED and probability 100%. About 13–16 KB per song for 10 layers × 5 banks × 64 beats × 4–5 bytes. Bank copy/paste, the bank clipboard, BeatList moves and rebuilds, EUCLID and record must carry the new fields along with the beat.
- **Editing on the device:** on Layer Song, with a beat selected (the MOVE cursor from 12), a NOTE page with tabs VEL, PROB, COND, NUDGE (later PITCH) for that beat; ADD + turning the encoder changes all beats of the bank at once (relative), for quick humanising. The pattern line can show low-probability beats differently (the font has no lowercase, like the `+` ghost hits).
- **Playback:** the sequencer plays a plain beat at its stored velocity, rolls its probability at each loop (combined with the layer's PROB), and MIDI out sends the velocity actually played.
- **Recording:** live and MIDI-in recording store the velocity that came in (needs 30's velocity fix); pads record at FLAT.
- **What the Elektron step sequencer (Digitakt / Digitakt II) offers, compared** (researched 2026-11-07; Elektron's names, used here only as reference):
  - Per-step velocity, probability (1–100%), trig conditions, micro timing, retrig, note/pitch, note length, parameter locks, trigless lock trigs, slide (Digitakt II), sample/sound locks, per-track length and speed, swing, a Euclidean generator per track, and a FILL performance mode.
  - The Drumboy already has: retrig and more (the fills: ROLL, RATCHET with ACCEL/DECEL, TRIPLET…, with VELOCITY modes DESCEND/ASCEND/RANDOM), a muted step (MUTED), Euclid (11), coarse beat moves (12).
  - Elsewhere in this backlog: parameter locks, trigless locks and slide → [20](#20-parameter-locks-per-beat-lxr); per-layer length → [23](#23-per-layer-length--polymeter-lxr); swing → [18](#18-swing--shuffle-lxr).
  - Left out for now: sample lock (each layer holds one sample in SDRAM), the NEI condition (neighbour track).
- **Phase A, the core (this item):**
  1. **VEL** 0–127.
  2. **PROB** 0–100%, combined with the layer's PROB.
  3. **COND:** `A:B` (plays on loop A of every B, up to 8:8) and its negation `!A:B`, `1ST` / `!1ST` (first time the bank plays since START or a bank change), `PRE` / `!PRE` (the previous conditioned beat of this layer played / did not), `FILL` / `!FILL` (plays only while a FILL key is held / only while it is not; shown as `FKEY` / `!FKEY` on the device so it isn't mistaken for the layer's FILL switch). FILL needs a performance key: hold a key combination (e.g. ADD + PLAY) while playing. The loop counter per layer and bank resets on START and on a bank change (song chain steps included).
  4. **NUDGE:** micro timing, a signed offset of up to ± half a quantize step applied at playback. The beat keeps its place in the beat list, so moves, Euclid and copy/paste are unaffected. Resolution: the sequencer tick (800 per quarter, 200 per 16th), finer than Elektron's 1/384 of a step.
- **Phase B, toward parameter locks (after A is tested):** **PITCH** per beat in semitones (reusing `Pitch.h`), then **DECAY / LENGTH** per beat, then the full parameter locks of [20](#20-parameter-locks-per-beat-lxr).
- **Ties to other items:** this is the storage that [14](#14-euclid-velocity-and-variance) part B (EUCLID VELOCITY / RANDOM spread) and [21](#21-probability-per-beat-lxr) (probability per beat) were waiting for, and the base of [20](#20-parameter-locks-per-beat-lxr) (parameter locks). EUCLID can then fill VEL and PROB with its own patterns.
- Hardware-free code (`BeatExt.h` or inside `BeatList.h`): the block's read/write with checks, carrying the fields through move/rebuild/copy, the probability roll and the condition logic, with host tests (old file without the block, round trip, a move keeps each beat's velocity, probability 0/100 exact, condition "1:4" plays exactly once in four loops).
- The app gets a tab to try velocity and probability on a pattern before building.

**Done when.** On the device a single beat can be set to velocity 30 and probability 50%, it plays softly about half the time, a beat with `1:4` plays exactly once every 4 loops and one with `FILL` only while the FILL key is held, a nudged beat plays measurably early/late, its settings survive move, copy/paste and save/load, MIDI recording keeps the played velocity, and the same song still loads (and plays at FLAT/100%) on V1.12.

### 34. MIDI clock slave: drift and 200 BPM runaway

**Status: done in V1.13** (`MidiClock.h`, `test_midi`: 10 minutes at 97.43 BPM with ±1 ms jitter stay within one clock, echoed clocks ignored, lost clocks counted, tempo changes followed). While following MIDI clock the Drumboy sends no MIDI clock or transport of its own; the sync jack follows the incoming clock.

**Reported (V1.12).** Setup: Loopy Pro → Scarlett → MIDI IN, sending clock, transport and notes. (1) The Drumboy ran away at 200 BPM; the owner traced it to a MIDI loop sending signals back into the system. (2) Even without the loop it slowly drifts out of sync, "as if the signal did not arrive continuously".

**What the code does (checked).**
- **The clock only sets the tempo; it never steers the playhead.** On 0xF8 the UART handler (`receiveMidiCommand()`) counts 48 ticks, measures them with `HAL_GetTick()` (1 ms) and computes an **integer** BPM (`rxClockBpm`). The main loop applies it with `rhythm_setTempo()`, which only changes the period of the sequencer timer TIM15. After START the sequencer runs free on its own timer; the incoming ticks never move the play position again (`rxClockTickPhase` is only used to start on a beat). So any difference between the two tempos accumulates until the next STOP/START.
- **Integer tempo.** `rhythm.tempo` is a `uint8_t`. Loopy Pro's tempo is often fractional (it comes from the length of the first loop, e.g. 97.43 BPM). Running at 97 instead of 97.43 is 0.44% slow: a 16th note behind after about 57 beats, well under a minute. When the real tempo sits near .5, the measurement flips between two values every 2 beats.
- **Timer rounding.** `playTimerPeriod = 60,000,000 / (tempo × 800)` in whole microseconds (TIM15 at 1 MHz): exact at 120 or 125 BPM, but e.g. 133 BPM gives 563 µs instead of 563.9 (0.16% fast). This adds to the drift even when the master's tempo is a whole number.
- **Lost clock bytes would make it worse.** MIDI IN (USART1) receives one byte per DMA transfer and re-arms in the completion callback, with the UART FIFO disabled. That interrupt has the same priority as the sequencer timer (preempt 2) and below audio and SD (preempt 0). If it is held off for longer than about two byte times (640 µs), the UART overruns and bytes are lost; `HAL_UART_ErrorCallback` re-arms it, so it recovers silently. Lost 0xF8 bytes read as a slower tempo; lost note bytes drop notes. Not confirmed on the device yet. Count the overruns to check: add a counter in the error callback and show it on the test page.
- **Clock out while slaved enables the loop.** MIDI OUT clock (`txClock`, TIM4 at the Drumboy's own tempo) is independent of MIDI IN clock (`rxClock`), so a slaved Drumboy keeps sending its own clock. If anything echoes MIDI OUT back to MIDI IN (MIDI thru in Loopy Pro or the interface routing), the input sees roughly twice the ticks, measures about twice the tempo, clamps at `kMaxTempo` = 200 and sends 200 BPM clock back: the runaway.

**Proposal.**
1. **Lock the playhead to the incoming clock (the real fix).** While slaved to MIDI clock, the sequencer may advance only as far as the clock allows: tick k since START corresponds to position k × 800 / 24 sequencer ticks (33⅓, so keep the fraction). The timer runs about 1% faster than the measured tempo and waits at the allowed position until the next 0xF8. It can never get ahead, and it is never more than one clock tick (1/24 beat) behind. A clock tick that arrives early lets it catch up at once. Song position after CONTINUE: keep the counter. Measure the tempo with microsecond timestamps (DWT cycle counter or a free-running timer) averaged over 24–48 ticks. Keep a fractional tempo internally for the timer, the delay effects and the LFOs; the screen can still show whole BPM.
2. **No echo loop.** While MIDI IN clock is on, MIDI OUT clock either forwards the incoming ticks (true thru) or is off. It never sends a second, independent clock. Ignore 0xF8 that arrive impossibly fast (more than 2× the measured rate) as a safety net.
3. **Robust MIDI IN.** Enable the USART1 RX FIFO, or move to a circular DMA ring read by the parser (this fits the parser and queue planned in 30), so no byte is lost while higher-priority interrupts run. Count overruns for diagnostics.
4. Hardware-free logic (`MidiClock.h`): tempo estimation from timestamps, the allowed-position rule, start/stop/continue, rejection of doubled ticks. Host tests: a fractional tempo (97.43 BPM) for 10 minutes stays within 1/24 beat; jittered ticks (±1 ms); one dropped tick; doubled ticks from an echo; tempo changes.

**Done when.** Slaved to Loopy Pro at a fractional tempo for 30 minutes, the Drumboy stays in time (by ear and on a recording, against a click from Loopy). With a MIDI loop deliberately set up, it neither runs away nor doubles its tempo. The overrun counter stays at 0 with clock and dense notes coming in together.

**Workarounds until then.** Turn the Drumboy's MIDI OUT clock off while it is slaved (or turn MIDI thru off in Loopy Pro) to avoid the loop. Set Loopy Pro to a whole-number tempo, preferably one that divides evenly into the timer (120, 125, 100, 150…); this reduces the drift but does not remove it. STOP/START from Loopy realigns it.

**Update V1.16, built, waiting for the hardware test:** MIDI thru of the real-time bytes (`midiThruForwards` in `MidiParser.h`, `midi_thru`, host test in `test_midi`). With SYNC on MIDI IN and on MIDI OUT, an accepted incoming clock tick and the transport bytes (start, continue, stop, reset) are passed on to MIDI OUT from the receive interrupt; the Drumboy sends no clock or transport of its own while it follows (already so since V1.13). A tick the clock lock rejects as too fast is not passed, so a loop through an interface dies out. Notes and controllers are not passed on: the Drumboy plays incoming notes and sends its own, so forwarding them could double.

### 35. LFO destinations for the per-note settings

**Asked by the owner (2026-11-08):** can VELOCITY, CHANCE, CONDITION and NUDGE (NOTE page, [33](#33-per-note-editing-velocity-probability-and-more)) be moved by an LFO?

**Today.** No. They are fixed values stored in each beat. The layer's LFO slots (LEVEL, PITCH, DIRECTION, RESOLUTION, START, END, TIMING, PROB, only while the layer's STYLE is on, sampled at each hit) get close:
- LFO → LEVEL multiplies with each note's velocity.
- LFO → PROB is a second roll on top of each note's chance.
- LFO → TIMING only delays hits.

**Proposal.**
- **New LFO destinations per layer:**
  - **NOTE VEL:** an offset to each note's velocity, so accents keep their order and the line breathes.
  - **CHANCE:** an offset to each note's chance, applied before the roll.
  - **NUDGE:** an offset in both directions, unlike TIMING, clamped to ±12 % of a 16th like the field.
- **No CONDITION destination:** conditions are on/off rules per loop, not a scale.
- **Where the offsets apply:** at playback, in `note_plays` and in the level and due-time computation (`BeatNote.h`). The beats themselves don't change, so turning the LFO off gives the written pattern back.
- **Applies to:** plain beats and fills alike, like the fields. Only while STYLE is on, like the other LFO slots.
- **Depth:** the existing LFO DEPTH, mapped to a fixed full-scale range per destination (e.g. ±64 velocity, ±50 % chance, ±12 nudge steps).
- **Storage:** the layer record in the kit block is full (25 bytes per layer: 0–22 parameters and LFO slots, 23 routing, 24 semitones). The three new LFO slot numbers need a format extension. Either:
  - a kit block after the song's BTX block (same "older firmware stops before it" trick), or
  - a format version bump together with [3](#3-lfos-cant-modulate-filters-or-effects) and [24](#24-velocity-modulates-a-parameter-lxr), which also add modulation slots.

  Doing all three at once costs one format change instead of three.
- **UI:** a fourth layer instrument page (LFO slots for NOTE VEL, CHANCE, NUDGE, and later the [3](#3-lfos-cant-modulate-filters-or-effects) destinations), or extra fields on instrument page 3.
- **Tests:** hardware-free offset math in `BeatNote.h` with host tests (clamping at 1/127, 0/100 %, ±12; depth 0 = no change; nudge offsets keep neighbours from swapping, so the ±12 limit stays).

**Done when.** A RANDOM LFO on NOTE VEL makes a straight 16th hi-hat vary in loudness while its accents stay louder than the rest; a SINE LFO on CHANCE thins the layer out and fills it back over the LFO's period; with the LFO off or STYLE off the pattern plays exactly as written; songs from V1.14 load unchanged.

### 36. Per-layer panning

**Asked by the owner (2026-11-08).**

**Today.**
- **One PAN only:** the Mixer page has a single PAN (0–40, centre 20, `mixer.pan`, saved in the system preset and the song's kit block). It is a balance on the whole output: `mixer.volumeLeftFloat` and `mixer.volumeRightFloat` are applied at the very end of `processAudioBlock`, after the reverb.
- **Mono up to the reverb:** layers are summed into one channel and EQ, filters and effects process mono ("copy mono accumulation to stereo output" when an effect is bypassed).
- **The reverb is the only stereo:** its SURROUND makes the right side. There is no per-layer or per-note pan.

**Proposal**, in three parts:

- **A. Pan on the direct path (S–M), first.**
  - **Field:** a PAN per layer (e.g. 0–40 like the mixer, constant-power law) on an instrument page.
  - **Where it acts:** only on what a layer sends straight to MASTER (layer OUT = MASTER in the routing patch). Those layers are summed into a separate left/right direct bus, added to the output before master volume and the mixer PAN.
  - **Other routes:** layers routed through EQ, filters or effects stay centred, as now. The field shows that when the layer's OUT isn't MASTER, e.g. the header dimmed like unused modules on the ROUTING page.
  - **CPU:** two gains per active layer per sample, plus one stereo sum.
  - **Storage:** one byte per layer. The layer record in the kit block is full (25 bytes), so it needs the same format extension as [3](#3-lfos-cant-modulate-filters-or-effects), [24](#24-velocity-modulates-a-parameter-lxr) and [35](#35-lfo-destinations-for-the-per-note-settings): do them together, one change.
  - **Old songs:** load centred, so they sound the same.
- **B. Stereo all the way (L), only after measuring.**
  - **Change:** the whole chain processes left and right, and PAN works for every route.
  - **Cost:** roughly doubles the DSP cost of EQ, filters and effects. V1.04 worst case was 34 % of the audio block, more since routing.
  - **Gate:** measure with `make PROFILE=1` first, against the gate of [4](#4-routing-multiple-destinations-per-source) (< 80 % of the block, UDR/OVR = 0).
  - **Effects:** some (delay, chorus) could become true stereo; others may need to stay mono to fit.
- **C. Modulation, on top of A or B.**
  - **Auto-pan:** an LFO → PAN slot per layer, in the same new LFO page as [35](#35-lfo-destinations-for-the-per-note-settings).
  - **Pan per note:** a PAN on the NOTE page. Beats have no room left (RAM about 4 KB free since V1.14), so this waits until the song banks move to SDRAM, like phase B of [33](#33-per-note-editing-velocity-probability-and-more).

**Tests.** Hardware-free pan law (`Pan.h`) with host tests: centre −3 dB per side, the ends fully one side, monotonic, and sums that don't clip more than today.

**Done when (A).** Hats panned left and a shaker panned right are heard on their sides through headphones while kick and snare (through the EQ) stay centred. The mixer PAN still balances everything. CPU is measured before and after. Songs from V1.14 load centred and sound the same.

---

## UX and signal path (items 37–53)

From the analysis in [`docs/ux-and-signal-path-analysis.md`](docs/ux-and-signal-path-analysis.md), which has the facts about the screen (960×540, 16-bit colour, drawn directly, no frame buffer), the layout, the cost estimates and the reasoning. The codes in brackets (U1, E2, S1…) are that document's. Mockups of the proposed screens, drawn with the device's own fonts and frame, are in the app's **Screen ideas** tab. Nothing here was measured on the device: item 37 comes first.

### 37. Measure first: drawing speed, loop time, audio load

[E1, S0, analysis section 5.] Every size and viability claim in items 38–48 rests on estimates.

- **Drawing benchmark** on the test page: fill, text and image speed in megapixels per second (DWT cycle counter).
- **Main-loop pass time** while idle, while playing, and during a full lane redraw; shown like the DSP profile.
- **Audio load today** on full songs with `make PROFILE=1`: average, peak, UDR/OVR. The last figure is 34% worst case at V1.04, before the routing engine.
- **Data cache experiment** (separate): is `SCB_EnableDCache()` safe with the DMA buffers for audio, SD and MIDI, and what does it save.

**Done when.** The four numbers are in this file, and a draw budget per main-loop pass is set from them.

**Status: the audio load is now on screen in every build** (V1.14, `LoadMeter.h`, `test_loadmeter`): the CPU bar in the top-right box shows the worst audio block of the last quarter second as a share of the block time, with its percentage, magenta from 80%. So the audio-load figure can be read on real songs without a special build; `make PROFILE=1` still adds average, peak, maximum and the I2S error counts as text. Open: the drawing benchmark, the main-loop pass time and the data-cache experiment.

### 38. Per-note settings and layer state visible in the lanes

[U1, U6.] **Today:** a beat with velocity 20, chance 30% and a condition looks the same as a plain beat; a muted or non-soloed layer differs only by a 10 px box.

- **Beats** (`lcd_drawBeat`): height or brightness = velocity; hollow = chance below 100%; a dot above = has a condition; drawn at its nudged position.
- **Lanes:** a muted layer, or a layer silenced by another layer's solo, is drawn dimmed (name and beats).
- Hardware-free helper for the beat's look (velocity → height, flags) with a host test.

**Done when.** On the main screen you can tell loud from soft beats, see which beats are uncertain or conditional, and see at a glance which layers sound.

**Status: built in V1.14, tested on hardware, released in V1.14** (`LaneView.h`, `test_laneview`). Height = velocity (FLAT 9 px, GHOST 6 px, velocity 1 = 2 px), hollow = chance below 100%, a yellow dot above = condition; a layer that is muted or silenced by a solo is drawn in dark gray (number, name, beats). A beat without note settings looks as before. Open: drawing a beat at its nudged position (a nudge is at most 2 px on a 32-step loop).

### 39. Graphic overviews: NOTE, EUCLID, SONG, ROUTING

[U2–U5.] The 325×60 area on tabs 2–3 shows text on these four pages.

- **NOTE:** one bar per beat (height = velocity, hatch = chance, tag = condition), the selected beat highlighted. 64 beats fit at 5 px each.
- **EUCLID:** the pattern on a ring (radius about 28 px) with hits, accents and rotation, next to the linear row.
- **SONG:** one block per chain step, width = repeats, colour = bank, the playing step highlighted with its loop count.
- **ROUTING:** boxes for EQ, F1, F2, FX1, FX2, REV, OUT joined by lines; modules nothing feeds are dimmed.

**Done when.** Each page can be read without parsing text, and the drawing stays inside the 325×60 area and the draw budget from 37.

### 40. Curves and cursors on the sound pages

[U7–U13.] Computed in the main loop when a field changes (no audio cost). The Filter, EQ, effect and reverb pages use all eight fields, so their graphics go in a panel over the song part of the top three lanes (about 623×90 px) while the page is open, and those lanes are redrawn on leaving. The LFO and instrument pages keep their 325×60 area.

- **Filter:** response curve from type, cutoff, resonance and slope (magnitude at about 160 frequencies).
- **EQ:** the six bands summed, a marker per band.
- **Compressor / expander:** transfer curve; a live gain-reduction bar (needs a value from the audio interrupt, see 41).
- **Delay:** taps on a beat grid. **Reverb:** pre-delay gap and decay slope.
- **LFO:** a phase dot moving on the existing shape while playing, and the list of what the LFO modulates.
- **Sample waveform:** play cursor, start and end handles, direction.
- Curve maths in hardware-free headers with host tests (known filter magnitudes at known frequencies).

**Done when.** Turning CUTOFF or an EQ gain moves the curve as you turn, also while playing, with no audio glitches.

**Status: the filter curve is built in V1.14 as a full-area view** (see [45](#45-full-area-views-mix-note-editor-sample-editor-patchbay-performance)), tested on hardware, released in V1.14. The EQ curve is built too. Open: cursors on the effect and reverb pages.

### 41. Live feedback: layer meters and status indicators

[U14–U18, E4.]

- **Meter taps in the audio interrupt:** peak per layer per block (one compare per sample), gain reduction of the compressor and limiter; read and reset by the main loop.
- **Layer meters** in the lanes with a clip mark. The master already has the two 12-LED meters.
- **Indicators** in the strip between the top menu and the lanes: MIDI in / out / clock (locked or free, tempo with decimals, error count), unsaved-changes dot and time since the last autosave, undo depth.

**Done when.** Each layer's level is visible while it plays, and the strip shows MIDI, save and undo state without opening a page.

**Status: layer meters and part of the strip built in V1.15, tested on hardware, released in V1.15** (`StatusLine.h`, `test_statusline`): a peak tap per layer in the audio interrupt (one compare per sample), a 22 x 4 px bar in each lane, 25 times a second; the strip shows the MIDI clock tempo and the autosave age. Open: the compressor and limiter gain reduction, the unsaved-changes dot and the undo depth.

**Status: a first indicator is built in V1.14** (asked by the owner with a picture of it): CPU and MEM bars in the top-right box of every page (`lcd_drawMonitor`). MEM is the loaded samples as a share of the ten sample slots. The rest is open.

### 42. Key hints, status line, page dots, value popup

[U19–U22.] The confusion met while testing V1.14 (two S keys, hidden combinations, FILL meaning two things, a 5-character status spot) is about finding functions, not about missing ones.

- **Key-hint line:** while ADD, M, F, S, SONG, L, I, the right S or the encoder key is held, show what the other keys do on the current page (a table per page and modifier, in flash).
- **Status line** instead of the 5-character spot: "UNDO: MOVE", "CHAIN: BANK 3 FOR 1 LOOP", "FKEY HELD", "SAVING".
- **Page dots** for pages that cycle (instrument 1/2/3, song / EUCLID / NOTE).
- **Value popup** while a knob turns: name, value, unit and a bar for the range.

**Done when.** Holding any modifier shows its combinations for the current page, and every action confirms itself in words.

**Status: key hints built in V1.15, tested on hardware, released in V1.15.** While M, F, the left S, I, the right S or SONG is held, the status line (right aligned, 5x7 font, in the strip) says what the other keys do with it. The hints are the same on every page; a table per page is open. Not built: the line replacing the 5-character spot (UNDO, COPY, FULL stay there), page dots, the value popup.

### 43. Fonts with lowercase and symbols; colour roles

[U23, U24.]

- **Glyphs:** lowercase for the 7×9 and 10×14 fonts, and symbols (arrows, note, mute, solo, lock, ghost). About 256 KB of flash is free. Removes workarounds such as `+` for ghost hits and uppercase-only text.
- **Colour roles:** one colour per meaning (header, value, status, error, selected, modulated), and optionally a colour per layer, applied across all pages.

**Done when.** `X.xX.xx.` shows as written on the EUCLID page, and the same colour always means the same thing.

### 44. Notes triggered outside the main loop; draw budget

[E2, E3.] **Today:** pad and MIDI-in notes are triggered from the main loop, the same loop that draws, reads keys, writes to the SD card and runs autosave, so anything slow there delays notes.

- **Trigger notes from an interrupt** (a short timer tick reading the MIDI note queue and the pad events), keeping recording and undo capture in the main loop.
- **Draw budget:** a small queue of dirty regions, drawn a few per pass, so a large redraw never holds the loop.
- Check the shared state touched by `layer_playBeat` (it already runs from the sequencer interrupt) and the recording path.

**Done when.** A note from Loopy sounds at the same time whether or not an autosave, a bank redraw or a sample load is running; the loop-time figure from 37 stays under the budget during a full redraw. Required for 45.

### 45. Full-area views: MIX, note editor, sample editor, patchbay, performance

[U25–U29.] **Owner's direction (2026-11-13): the dedicated visualizations should use the big main area**, where the ten lanes are (920×310), not only the small 325×60 spot. While a feature's page is open, the lane area shows that feature; leaving the page redraws the lanes. Mockups of each are in the app's Screen ideas tab.

- **Euclid rings:** the ten layers as rings around one centre with the playhead as a clock hand, the selected layer highlighted, and every layer's pattern as a row beside it (different lengths compare at a glance).
- **Song timeline:** the chain across the full width (step width = repeats, the playing step and loop marked) and all five banks as thumbnails of their ten lanes.
- **Routing:** the modules along the top joined as patched, one row per layer with a mark under the module it enters; with sends (46) each mark becomes a level bar. This is the patchbay.
- **Filter and EQ curves:** the response over the whole area (both filters, cutoff marked); replaces the three-lane panel of [40](#40-curves-and-cursors-on-the-sound-pages).
- **Sample:** the waveform at full width with START and END handles, the unused part dimmed, a play cursor; later zoom and slices. The waveform shown today has 60 points, so this needs a pass over the sample in SDRAM.
- **MIX view:** the ten lanes as channel strips: level, pan, sends and filter as bars with live meters ([41](#41-live-feedback-layer-meters-and-status-indicators)). The home of the sends (46).
- **Note editor:** one layer across the full width, beats as blocks, a velocity lane and a chance lane below.
- **Performance view:** large mute and solo states, chain progress, bar counter and tempo.

**Cost.** Opening such a view draws the whole area once (about 285,000 pixels, like a bank change today), then only what changes: a playhead, a bar, a handle. That does not need [44](#44-notes-triggered-outside-the-main-loop-draw-budget). Continuous animation of the whole area does. Measure the page-open time in [37](#37-measure-first-drawing-speed-loop-time-audio-load) first.

**Order.** Euclid rings and Song timeline first (the data exists, no new engine work), then Routing and the Filter curve, then MIX with 46, then the editors.

**Status: Euclid rings built in V1.14, tested on hardware, released in V1.14** (`LaneView.h`, `lcd_drawEuclidView`). While the EUCLID page is open the lane area shows the ten layers as rings (layer 1 outside, one turn = one loop from the top, the selected layer cyan with its grid as ticks, larger dots for louder beats) and every layer's beats as a row with its count. The playhead is a marker that orbits outside the rings and one above the rows, not a clock hand: a hand would have to be erased and redrawn across all the dots. Everything that draws into a lane is held back while the view is shown (`laneViewHidden()`) and asks for a refresh of the view instead, at most every 80 ms; leaving the page redraws the lane frames, measure marks and lanes (`lcd_restoreLanes`). The mechanism is now general (`lcd_drawLaneView`, `lcd_updateLaneView`): a page names its view, the view redraws only what changed. To check on the device: how long the view takes to open and to refresh while a knob turns, and whether pads still feel immediate meanwhile.

**Also built in V1.14, tested on hardware, released in V1.14:**
- **Song timeline** (`lcd_drawSongViewData`): the chain as boxes as wide as their repeats (the selected step cyan), a bar in the playing loop that fills in yellow, a line back to the start for LOOP or a stop mark, a status line (step, loop, what comes next, also a detour), and the five banks as thumbnails. A thumbnail is redrawn only when its beats or its colour change (a fingerprint per bank), so a bank change at a loop end redraws two thumbnails, not five.
- **Routing** (`lcd_drawRoutingViewData`): the six modules and the master as boxes joined as patched (next module: a line between the boxes; further: over them, each module at its own height), unfed modules dimmed, and one row per source (ten layers, metronome, line-in) with a mark under the module it enters. With sends ([49](#49-dub-mixer-page-dub-mode)) the marks become level bars.

- **Filter curve** (`FilterCurve.h`, `test_filtercurve`, `lcd_drawFilterViewData`): on the FILTER 1 and FILTER 2 pages, the response from 20 Hz to 20 kHz (+18 to -48 dB) computed from the coefficients the audio code runs, including the 6 dB setting and DRY/WET with their phase; the page's filter in cyan (gray while off) with its cutoff as a dotted line, the other filter behind it when it is on. A changed curve is erased by redrawing it in black (its points are kept in a scratch area at the end of the SDRAM), so turning FREQ does not clear the plot.

- **EQ curve** (`lcd_drawEqViewData`): on the EQ page, the six bands in series as one curve (+24 to -36 dB) with a handle per band at its frequency and gain and its name above the plot (LO, 1-4, HI); gray while the EQ is off.

- **Sample** (V1.15, tested on hardware, released in V1.15; `SampleView.h`, `test_sampleview`, `lcd_drawSampleViewData`): on instrument page 2 (START, END) the sample at full width: 840 columns, each the peak of its stretch (read from the sample's SDRAM sector, at most 64 reads per column, computed again only when the sample changes), cyan between START and END and gray outside, yellow handles with their values, title and length of the played part. Moving START or END redraws only the columns the handle crossed. Not built: a play cursor, zoom, slices; instrument pages 1 and 3 do not use the view.

- **Step edit screen** (V1.15, tested on hardware, released in V1.15; `NoteView.h`, `test_noteview`, `lcd_drawNoteViewData`): the NOTE page's view. The layer's beats across the full width (852 px; 1/32 steps of a two-bar loop still get 13 px each): blocks (a fill as long as it lasts, a muted beat an outline, a ghost half height), a velocity lane and a chance lane under them (a hollow bar below 100 %), a yellow dot for a condition, a ruler with a tick per beat and the bar numbers, the selected beat in magenta and its details at the top right. A knob turn or another selected beat draws only the changed columns (a fingerprint per beat, kept in the SDRAM scratch); ADD + turn, a new layer or bank draws everything. Selecting and editing still use the NOTE fields (beat, velocity, chance, condition, nudge). Not built: placing and moving beats from this screen, pad entry and a play cursor.

Open from this item: MIX (as the dub mixer, [49](#49-dub-mixer-page-dub-mode)), Note editor, Performance view.

### 46. Sends: a level per layer into the shared effects (SENDS mode)

[S1.] Finishes [4](#4-routing-multiple-destinations-per-source) and [5](#5-routing-send-levels): the routing engine already sends one source to several destinations with a gain per sample; the UI and the file side are missing.

- **Per layer:** dry to MASTER plus SEND A (Effect 1), SEND B (Effect 2) and SEND REV, each with a level that follows the knob with a short ramp.
- **Per song:** a mode, CHAIN (today) or SENDS. In SENDS mode Effect 1, Effect 2 and Reverb return to MASTER, and an effect's DRY is off (its WET is the return level). Old songs load as CHAIN and sound the same.
- **Files:** about 8 bytes per layer for sends, pan (36) and the channel filter (47) in one block after the song's `CHN` and `BTX` blocks; older firmware ignores it. The same block serves [3](#3-lfos-cant-modulate-filters-or-effects), [24](#24-velocity-modulates-a-parameter-lxr), [35](#35-lfo-destinations-for-the-per-note-settings).
- **UI:** a MIX page per layer (a fourth instrument page): FILTER TYPE, CUTOFF, RESONANCE, DRIVE | PAN, SEND A, SEND B, SEND REV. The ROUTING page shows the mode.
- **Gain staging:** dry plus three returns is louder than the serial chain; set return levels and the limiter ceiling by ear.
- **Cost (estimate):** 1–2% of the audio block. Host tests in `test_routing` for levels, ramps and the mode.

**Done when.** The snare has a long reverb and the kick almost none, on the same reverb; a V1.14 song loads as CHAIN and sounds identical.

### 47. Channel filter and drive per layer

[S2.] The part that makes it "per-channel effects".

- **Per layer:** one multimode filter (12 dB) and a drive stage, processed on the layer's own buffer before routing. State in the unused DTCM RAM (128 KB, the fastest memory), since internal RAM is nearly full.
- **Modulation:** LFO → cutoff per layer, a first part of [3](#3-lfos-cant-modulate-filters-or-effects).
- **Not per layer:** delay, chorus and reverb (384 KB and 96 KB of SDRAM per instance, and ten times the cost); the sends cover them.
- **Decide:** whether Filter 1 and Filter 2 stay as bus filters or go.
- **Cost (estimate):** filter 1.6–3.2% of the audio block, drive under 1%; idle layers cost nothing. A 2-band EQ per layer would add 3–6.5%.

**Done when.** A low-pass on the hats does not touch the snare routed the same way; load measured before and after stays under the 80% gate.

### 48. Master EQ stage, stereo returns, a third send effect

[S5.] Only after the measurements of 37 and the experience of 46–47.

- **Master EQ:** modules run in a fixed order with EQ first, so EQ on the sum needs a separate master stage (simpler than a user-set module order, [7](#7-routing-open-design-questions)). On a stereo master it costs twice the mono EQ.
- **Stereo returns:** stereo through the effects doubles their cost ([36](#36-per-layer-panning) B).
- **A third send effect:** a delay plus a chorus buffer is 480 KB and about 650 KB of SDRAM is free, so one more fits; two (960 KB, [6](#6-routing-extra-effect-slots)) do not unless the undo history shrinks or a sample slot (960 KB) is given up.

### 49. Dub mixer page (DUB mode)

Asked by the owner on 2026-11-13: the mixer page in the fashion of a dub desk, with sends to the effects and filters. Design: [`docs/dub-mixer-design.md`](docs/dub-mixer-design.md); mockup in the app's Screen ideas tab ("Dub mixer"). It is the concrete form of [46](#46-sends-a-level-per-layer-into-the-shared-effects-sends-mode), [47](#47-channel-filter-and-drive-per-layer) and the MIX view of [45](#45-full-area-views-mix-note-editor-sample-editor-patchbay-performance).

- **Mode per song:** CHAIN (today) or DUB. In DUB mode each layer and line-in is a strip: channel filter, level, a bus (FILT: EQ and Filter 1 as the dub filter; DIR: straight to master), sends to the echo (through Filter 2 into Effect 1), the reverb and Effect 2. The echo return can feed the reverb.
- **Page:** MIXER → DUB → ROUTING. The lane area shows eleven strips with faders, the ECHO and VERB returns, and the DUB FILTER as a big dial over the master meters. Top row = selected strip (LEVEL, FILTER, ECHO, VERB); bottom row = the desk, always the same (FEEDBACK, ECHO RTN, VERB RTN, DUB FILTER).
- **Gestures:** drop (MUTE + pad; tails ring on), throw (EFFECT 1 + pad or REVERB + pad opens that send fully while held, also on a muted strip), feedback past 100 % with a soft clipper, the dub filter in steps.
- **Stereo** (owner: yes, if the processing room allows): everything is mono until the reverb today. The design adds pan on strips that go straight to the master (DIR) and a ping-pong echo (the repeats alternate left and right, from a second read point in the same delay buffer, ECHO WIDTH 0–100 %). Estimated under 1 % of the audio block together; to confirm with [37](#37-measure-first-drawing-speed-loop-time-audio-load).
- **Phases:** D1 mode, sends, returns, page, file block; D2 throws, feedback past 100 %, dotted echo times, echo into reverb, ping-pong echo; D3 channel filter and drive, pan on DIR strips ([36](#36-per-layer-panning) A), page 2; D4 meters ([41](#41-live-feedback-layer-meters-and-status-indicators)) and the line-in strip; D5 a stereo FILT bus and a filter over the whole mix ([36](#36-per-layer-panning), [48](#48-master-eq-stage-stereo-returns-a-third-send-effect)).
- **Decided by the owner (2026-11-13, "yours on all of them"):** a filter per strip plus a BUS switch (FILT or DIR); throws with EFFECT 1 + pad and REVERB + pad; a throw pad does not play the sound; the big knob acts on the FILT bus only; Effect 2 is a third send; page order MIXER, DUB, ROUTING. Details in section 6 of the design. Phase D1 starts after the owner has tested V1.15.

**Status: D1 first step built in V1.15, tested on hardware, released in V1.15** (`DubMix.h`, `test_dubmix`; page `DUB_MENU`). MIXER, then DUB, then ROUTING. DUB mode per song with the fixed patch (`kDubRoutePatch`: EQ to Filter 1 to the master, Filter 2 to Effect 1 to the master, Effect 2 and the reverb to the master). Per layer: BUS (FILT or DIR), ECHO and VERB send levels (FX send exists in the engine and file, no field yet), applied as gains that follow the knob with a ramp per block (`routeScatterGains`). In DUB mode the effects and the reverb run without their DRY (`fxDryScale`) and a switched-off effect returns silence; turning MODE to DUB therefore makes Effect 1 a delay if it is another type and switches Effect 1 and the reverb on (found in the first hardware test: effects start off, so the echo was silent). FEEDBACK shows NO DELAY while Effect 1 is not a delay. Fields: top LEVEL, BUS, ECHO, VERB (the strip a pad press selects); bottom MODE, FEEDBACK (the delay's, up to 80), ECHO RTN (Effect 1's WET), VERB RTN (the reverb's WET). File: a `DUB` block after the notes. Line-in and the metronome keep their entry points. The desk view is built too, and redesigned on 2026-11-14 at the owner's request in the style of an old mixing console: twelve columns of 76 px fill the whole lane area (before, ten strips of 60 px took about two thirds), a fader of 128 px instead of 208 to make room for the sends (two knobs with names and values per column), scribble plates, a FILT | DIR switch, a pan bar, a scale and an LED ladder per fader, and foot plates with a mute lamp. Geometry in `DubView.h`, drawing in `lcd_drawDeskColumn`. Earlier version: ten strips (number, name, ECHO and VERB bars, a LEVEL fader with a live meter, the bus) and the ECHO and VERB returns in the lane area, each strip drawn again only when its fingerprint changes. Not built yet: the FL row, the line-in strip, the dub filter dial and master meters of the mockup, throws, feedback past 100 %, ping-pong and the dotted times, the dub filter steps, the channel filter, pan, page 2, the line-in strip, meters on the strips.

**Done when (D1 + D2).** With a pattern playing, the snare is thrown into the echo for one hit, FEEDBACK is ridden until the echo builds and pulled back, and the dub filter thins the drums while a kick on DIR keeps its weight. A V1.14 song loads as CHAIN and sounds identical.

### 50. Filter slopes: 24 dB and a real 6 dB

Asked by the owner on 2026-11-13: "a 24 would be nice, actually the 6 is more EQ than filter".

**Before:** SLOPE 12 was one biquad; SLOPE 06 was that same biquad mixed half and half with the unfiltered signal, so it never took off more than 6 dB.

**Status: built in V1.14, tested on hardware, released in V1.14** (`filterDesign` in `FilterCurve.h`, `test_filtercurve`). The design moved out of `Global.h` into a hardware-free function that the audio code and the curve share.
- **24:** two sections in series. Low- and high-pass: the first at Q 0.54, the second at 1.85 x the RESONANCE's Q, which is a Butterworth response at the lowest RESONANCE and one peak of about Q above it. Band-pass and band-stop: the same section twice. Costs one more biquad per sample for that filter.
- **06:** low- and high-pass are a first-order filter (6 dB per octave, RESONANCE has no effect); band-pass and band-stop use one section at half the Q, a wide band. **A song that used 06 sounds different.**
- **12:** unchanged.
- **Files:** SLOPE is stored as before (0, 1, now also 2). Older firmware refuses the value 2 and keeps its default for that filter.

To judge on the device: whether 24 dB with high RESONANCE is too loud at the peak (up to about +15 dB), and whether the old 06 sound is missed. If it is, it can come back as a separate SLOPE value.

### 51. Watchdog for stalls

**Status: built in V1.14, tested on hardware, released in V1.14.** The independent watchdog (IWDG1) starts when start-up ends (start-up, with its animation and SD scan, is not watched) and is fed once per main-loop pass; 6 seconds without a feed restarts the device. The loop writes a stage number and the page into DTCM RAM as it goes (`WdStage` in `CrashRecord.h`), which survives the restart, so at start-up the record reads `CRASH 7 S06 P18 L98` since V1.15 (stage, page in hex, audio load; before: `CRASH 7 AT 0000sssm`) and goes to `System/Crash.log` like a fault. Stages: 1 keys, 2 screen, 3 full-area view, 4 SD, 5 rhythm, 6 mixer, 7 autosave, 9 test page, and (V1.15) 11 presets, 12 clipboards, 13 navigation, 14 MIDI out, 15 MIDI notes in, 16 MIDI transport, 17 sync. The log line carries `lr` = loop passes since power-on and `sp` = the audio load (the CPU bar, tenths of a percent) as last shown, to tell a blocked loop from an audio engine that starved it. **First real catch (owner, 2026-11-13): `CRASH 7 AT 00000618` while testing the dub mixer: stage 6, page DUB (0x18).** Stage 6 covered the whole tail of the loop then, so the cause is not found yet; the finer stages and the load figure are there to find it. It cannot name the exact function, only the part of the loop. Unverified: that the bootloader leaves the reset flag (RCC_RSR) alone; if a freeze restarts the device but shows no CRASH 7, that is why. Open: feeding it inside very long operations if a legitimate one ever exceeds 6 s (autosave, loading a song).

### 52. Bug: fills do not appear in the new views

Reported by the owner (2026-11-13) while testing V1.15: fills are not drawn on the step edit screen ([45](#45-full-area-views-mix-note-editor-sample-editor-patchbay-performance)), and the owner says they do not appear in the other views either ("neither"). Not investigated yet. To check first, on a layer with a fill: the step edit screen (NOTE page), the Euclid rings and rows, the Song thumbnails, and the main lanes, to see which of them show it. The views take a fill from `getFillType() != 0` with its end from `getEndInterval()`; if fills are stored another way (for example as a beat with a fill pattern and a time, as the lane code reads them), that is the first place to look. Left in the backlog by the owner's choice.

### 53. Stereo engine: pan per channel through the whole path

**Asked by the owner (2026-11-13):** panning by channel is what is missed most, and sound quality comes first: never sacrifice it, and specialized firmwares are fine if the engine needs them (rule recorded in `CLAUDE.md`). This is [36](#36-per-layer-panning) B, made the main plan: A alone (pan only for a layer that goes straight to the master) would do nothing in the default chain, where every layer goes through the EQ.

**Law.** Constant power with 0 dB at the centre (left = √2 cos a, right = √2 sin a): a centred layer is exactly as it is today, so old songs sound the same, and a hard-panned one is as loud as a centred one. Smoothed per block, no zipper noise.

**Phases, each a test build:**
1. **Pan on the direct path and the DUB DIR strips:** a PAN field per layer (0-40, centre 20; the song block, shared with 3, 24, 35), applied to what goes straight to the master. Cheap, and the first thing to hear and judge the law. Layers that go through the chain stay centred and the field says so.
2. **Stereo buses:** EQ, Filter 1 and Filter 2 process left and right (the same coefficients, a second set of states): twice the cost of those three, no change in the sound of a centred signal. Pan then survives the chain up to the effects.
3. **Stereo effects (first part built in V1.16, ping-pong echo, below):** the reverb already makes stereo from a mono input and stays so. Delay, chorus, flanger, phaser, overdrive, distortion, bitcrusher and the dynamics get a right channel. The cost is the memory, not the processor: a second 384 KB delay line per delay (about 650 KB of SDRAM is free, so two stereo delays need the undo history or a sample slot, a choice for the owner) and a second chorus buffer. Until then an effect's wet is mono and its dry stays stereo, which keeps the pan of the dry signal but not of the repeats.
4. **The whole mix in stereo:** the DUB mixer's FILT bus, dub filter and returns in stereo ([49](#49-dub-mixer-page-dub-mode) D5, [48](#48-master-eq-stage-stereo-returns-a-third-send-effect)).

**Status of the headroom work: the data cache is on by default since V1.15, and it works.** Owner's measurement with the PROFILE build on the same song (CHAIN, ten layers, reverb page open), per stage as a share of the audio block:

| Stage | no cache | cache |
|---|---|---|
| Layers | 9.8 | 3.5 |
| Reverb | 8.9 | 2.7 |
| Routing | 8.2 | 2.8 |
| EQ | 4.4 | 1.1 |
| Effect 1 / Effect 2 | 3.0 / 3.0 | 0.9 / 1.0 |
| Filter 1 / Filter 2 | 2.5 / 2.6 | 0.8 / 0.8 |
| Output | 2.4 | 1.1 |
| **Total (average, peak)** | **45.0, 46.4** | **14.8, 16.6** |

No underruns or overruns in either. About a third of the cost everywhere: the audio code was waiting for memory (the SDRAM, uncached at about 100 MHz, and the state in the main RAM), not computing. The sound is the same: the same code and data, only cached. (The first "saves little" report of that day came from the old firmware: the 31-character `_dcache` file was ignored by the bootloader.)

How it works: the SDRAM is cacheable through the MPU (write-back, write-allocate; only the CPU uses it); RAM_D3 (the I2C DMA buffers) is not cacheable; the DMA buffers in the AXI RAM are cleaned and invalidated by address (the I2S halves around each audio block, the MIDI-in byte alone in its 32-byte line, the MIDI-out buffer); the SD driver's cache maintenance and aligned scratch buffer are on. `make DCACHE=0` builds without it. The Makefile does not see a change of DCACHE or PROFILE: use a clean build directory after changing them. To keep watching while testing: sample loading, song save and load, MIDI in and out, keys and LEDs (a missed DMA buffer shows up as odd data, not a crash).

**Headroom first, now mostly done.** Stereo EQ and filters, and then effects, add to a load that the owner's heavy dub test (several effects, 24 dB filters, reverb) took to a watchdog restart before the cache (`CRASH 7`, [51](#51-watchdog-for-stalls)). So the order is: read the load figure of the next restart, [37](#37-measure-first-drawing-speed-loop-time-audio-load) measurements, then room: the data cache (off today; the audio, SD and MIDI DMA buffers need cache-safe placement), faster inner loops that sound the same, and a specialized build for heavy use if all that is not enough. No thinning of the sound.

**Status: step 1 built in V1.15, tested on hardware, released in V1.15** (`Pan.h`, `test_pan`; the router's master pan in `Routing.h`). PAN on instrument page 3 (tab 6, next to STYLE; ADD + turn moves all layers), 0 to 40 with 20 the centre, constant power with 0 dB at the centre (the centre is exactly 1 on both sides, tested), gains ramped per block. Stored in the DUB block (version 2, 55 bytes; version 1 still reads, everything centred), in CHAIN and in DUB.

**Step 2 built in V1.15, tested on hardware, released in V1.15:** the router, the EQ, Filter 1 and Filter 2 run left and right (same coefficients, their own history: `EqState`, `FilterState`), so PAN is heard for every layer in CHAIN and DUB and the field is never gray. The right channels and the route/pan gain memory sit in the DTCM RAM (`StereoDsp`, section `.dtcm`, 1,216 bytes, zeroed in `main`, linker assert below the crash record), which also freed the RAM_D1 slack. Effects and reverb run once on the mid signal (L+R)/2 and carry the dry difference around it with their own DRY level (`effectDryGain`), so a centred signal is bit-identical to the mono engine (host test: centred stereo chain equals V1.04, worst difference 1.4e-6) and a panned layer keeps its pan in the dry part; the effects' own wet stays centred until step 3. Sound rule: the stereo path does not thin any filter, effect or bit depth; it costs a second run of EQ and filters (to measure with the PROFILE build). Also fixed: distortion and bitcrusher DRY ignored the DUB dry-off.

**Done when (1).** A hat panned left and a shaker panned right, both on DIR, are heard on their sides; a centred layer and a song from V1.14 sound identical. **(2)** The same through the EQ and filters.

**Step 3a, ping-pong echo, built in V1.16, waiting for the hardware test** (`PingPong.h`, `test_pingpong`; the delay in `processAudioEffectHelper`). ECHO WIDTH (0-100 %, steps of 5) is on the second DUB page (press MIXER again while on DUB; MIXER, DUB 1, DUB 2, ROUTING): the delay reads its buffer at two points, one echo time back for the left channel and two for the right, and feeds back from the right one, scaled by FEEDBACK so every repeat is FEEDBACK times the one before it (left at 1, right at 2, left at 3 echo times and so on). No second delay line, so no memory and no change to the sound at width 0, where the output is bit-identical to V1.15 (tested against a plain delay). It needs two echo times inside the 2.18 s buffer; past that the echo stays centred. Stored in the song's DUB block as a 56th byte (older firmware reads 55 bytes and ignores it; older songs read as 0). The same page has the selected strip's PAN and FX SEND. Sound rule: nothing was thinned; the cost is one more buffer read per sample for Effect 1 while the width is above 0.

### 54. Stereo samples

**Asked by the owner (2026-11-15).** A stereo WAV used to load as its left channel only (the loader read one value per frame). The sample memory is the hard limit: the SDRAM is 16 MB (9 column and 12 row bits, 4 banks, 16 bits wide) and the fifteen 240,000-sample slots of 32-bit floats take 14.4 MB of it.

**Decision (owner, "it makes sense, yes"):** a stereo sample keeps its two channels in one slot, as two planes (the left at the start of the slot, the right half a slot later, `kStereoPlaneBytes`). The length is half (120,000 frames, 2.7 s; mono stays 5.44 s), the memory is unchanged and the storage stays 32-bit float: nothing is thinned. All ten layers can be stereo at once. Longer stereo files are cut at 2.7 s, as long mono files are cut at 5.44 s. Packing samples as 24 bits would give longer stereo samples (later, if needed).

**Status: step 1 built in V1.16, tested on hardware by the owner ("the stereo is fine").** The loader reads both channels (`SampleSector::stereo`, `BeatPlayData::stereoOffset`), the normalisation looks at the louder of the two, and the sample graph shows the louder one. The layer engine fills a right buffer (`StereoDsp::layerR` in the DTCM RAM) with the same position, fades, bitcrush and level as the left, in both the fast and the interpolating path. The router (`routeScatterStereoLR`, host-tested in `test_dubmix`) sends the left to the left of every node and the right to the right, so the PAN of a stereo layer is a balance with 0 dB in the centre (a centred stereo layer is untouched). The layer meter takes the louder channel. Mono samples are unchanged.

**Sample rates (read while in the loader, unchanged).** The engine runs at 44.1 kHz. Files at 44.1 and 48 kHz play at their own sample count, so a 48 kHz file sounds about 1.5 semitones low (the factory library is 48 kHz and tuned that way: do not change it silently). Files below 44.1 kHz are filled by repeating each sample (8 kHz six times, for instance) and files from 88.2 kHz up are decimated by skipping samples, with no filter. A proper resampler (interpolated filling, low-pass before decimation) would improve those rare files and keep their pitch; it is step 2 and needs its own host-tested module.

**To do.** Step 2: the resampler. Step 3: the SAMPLE page text showing STEREO, a stereo sample view, and a stereo preview check. Measure the cost of ten stereo layers with the PROFILE build (estimated 1-2 % of the audio block).

**Done when.** A stereo sample with different left and right content is heard on both sides, a hard-panned stereo layer is a balance (one side silent), a mono sample sounds as before, and a 4 s stereo file loads cut at 2.7 s.

### 55. MISSING SAMPLE names the sample

**Asked by the owner (2026-11-15).** After an SD card recovery, presets reported MISSING SAMPLE with no hint which sample, so the cause (a card copy that dropped files) was guesswork. The alert now has a second line under its box with the first missing layer, instrument and sample (`L3 Kick/Name`, cut to 35 characters), and stays on screen 3 s instead of 1 s. It covers song and kit loads and the in-use check (`missing_note`, `missingDetail`). The count (MISSING SAMPLES) still tells how many; only the first is named.

**Status: built in V1.16, waiting for the hardware test.** Done when a preset that names a sample that is not on the card shows its layer, instrument and sample.

### 56. SD card corruption with the data cache (V1.15), fixed in V1.16

**Found by the owner's second SD card failure (2026-11-15), the first being the card recovered on 2026-11-14.** Both cards showed damaged directories (`System/Preset` turned into a file, `Sample` holding macOS log files, PRESET FOLDER ERROR). The cause is in `FATFS/Target/sd_diskio.c`, added with the data cache (V1.15): FatFs writes its directory and table sectors through buffers that are not 32-byte aligned, and for those the driver copies each sector into a scratch buffer and lets the DMA write it. With the data cache on, the copy stays in the cache (write-back) and was never cleaned, so the DMA wrote what the RAM held before: stale sectors, into the directory and the FAT. The fast path (aligned buffers) did clean, and the read path invalidates correctly. V1.14 had no data cache and does not have the bug.

**Fix:** `SCB_CleanDCache_by_Addr` on the scratch buffer after each sector copy, before the DMA write.

**Who is affected:** V1.15 (released, tag `v1.15`) with the data cache on. Anything that writes the card can damage it: saving files, the autosave, the system preset, the crash log. Test builds from V1.15 on until this fix have the same bug. Reads are safe.

**Status: fixed in V1.16, waiting for the hardware test.** Done when a card survives a long session of saving songs and presets and a check of its file system afterwards shows no errors. Until the release, use V1.14, or V1.15 with the card backed up.
