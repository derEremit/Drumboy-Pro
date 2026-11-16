# UX possibilities and a per-channel signal path: analysis

Written 2026-11-13 against firmware V1.14 (branch `claude/project-drumboy-visibility-hrtvk8`). Two questions from the owner:

1. What can we add to the user experience, and how much can the screen handle, in particular dedicated visualizations for specific features?
2. Is it viable to change the signal path to per-channel effects with sends?

Everything here comes from reading the code. Nothing was measured on the device; where a number is an estimate it says so, and section 5 lists what to measure first.

## Summary

- **The screen can do much more than it does today.** It is 960×540 in 16-bit colour and anything drawn stays on screen. The firmware uses it almost only for text fields, the ten song lanes and three small graphs.
- **The limit is not the screen, it is the main loop.** Drawing, key handling and the triggering of pad and MIDI notes all run in the same loop, so a big redraw delays notes. Small, incremental drawing is free; full-area animation needs notes moved out of that loop first.
- **Best value first:** make the per-note settings visible in the lanes, turn the text overviews (NOTE, EUCLID, SONG, ROUTING) into real graphics, draw filter and EQ curves, add a key-hint line while a modifier is held, and add level meters.
- **Per-channel effects with sends is viable, and most of it is an extension, not a rewrite.** The routing engine already sends one source to several destinations with a gain per sample. Sends are that plus a level per layer and a UI. A filter per layer costs an estimated 2–4% of the audio budget. Heavy effects per layer (delay, chorus, reverb) are not viable and not needed once sends exist.
- **Memory is less tight than it looks.** Internal RAM has about 3.6 KB left, but the 128 KB DTCM RAM is unused, and it is the fastest memory on the chip.

## 1. What the screen and the controls are

### 1.1 Hardware and driver

| Item | Fact | Source |
|---|---|---|
| Resolution | 960×540, landscape | `kLCD_WIDTH` 540, `kLCD_HEIGHT` 960 (rotated) |
| Colour | 16 bits per pixel (RGB565) | `RGB16Color` |
| Controller | NT35510 command set, with its own display memory | `Lcd/NT35510.h` |
| Bus | 16-bit parallel, driven by writing GPIO registers directly | `Lcd.h` `writeData()` |
| Frame buffer in the processor | None. Every draw call goes straight to the panel | `Lcd.cpp` |
| Tearing sync | Not used (no TE pin in `main.h`) | `main.h` |
| Read-back | Pin exists (`LCD_RD`), unused | `main.h` |
| Fonts | 5×7, 7×9, 10×14, 14×18. Uppercase, digits, symbols; no lowercase | `Lcd/Font_*.h` |
| Primitives | pixel, line, rectangle, circle, triangle (outline and filled), text, palette images with alpha and partial draw | `Lcd.h` |
| Images | Menu frames, lane frames and logo loaded from the SD card into SDRAM; icons in RAM_D3 | `Global.h` |

### 1.2 Layout today

```
y 29   ┌────────┬─────────┬─────────┬─────────┬─────────┬────────┐
       │ icon   │ tab 0   │ tab 1   │ tab 2   │ tab 3   │ icon   │  top menu, 102 px
y 110  ├────────┴─────────┴─────────┴─────────┴─────────┴────────┤
       │ transport icons, status text (5 chars), play line       │  ~20 px strip
y 131  ├─────────────────────────────────────────────────────────┤
       │ 10 layer lanes, 31 px each                              │
       │  number · instrument · M F S boxes │ song lane 623 px   │  310 px
y 441  ├─────────────────────────────────────────────────────────┤
y 451  │ icon   │ tab 4   │ tab 5   │ tab 6   │ tab 7   │ icon   │  bottom menu, 60 px
y 511  └─────────────────────────────────────────────────────────┘
       x 20     130       305       480       655       830    940
```

- Each tab is 175 px wide and belongs to one of the 8 encoders.
- Pages that need a picture take over tabs 2–3 of the top row: **325×60 px**. That is where the LFO shape, the sample waveform, the fill graph and the text overviews (ROUTING, EUCLID, SONG, NOTE) go.
- The lanes take **920×310 px**, more than half the screen, and always show the same thing.

### 1.3 Other feedback and input

- 10 layer LEDs and two 12-LED level meters, left and right (driven over I2C, refreshed by a timer). The master level is already metered there, not on screen.
- 8 encoders, one per on-screen field, plus one encoder key.
- Two 3×3 key blocks, the M F S and L I S rows, 10 pads, the SONG key and power.

### 1.4 What limits drawing

1. **Everything is drawn from the main loop.** The same loop reads keys and encoders, plays pad and MIDI-in notes (`layer_playBeat` from the note queue), services the SD card and runs autosave. Time spent drawing is time a note waits.
2. **No measured drawing speed.** A fill is two register writes per pixel; text and images add work per pixel. My estimate from the code is tens of megapixels per second for fills, so a full-screen fill of 518,400 pixels would take roughly 10–25 ms, and text or images several times longer per pixel. Treat these as estimates until measured (section 5).
3. **No tearing sync and no double buffering.** Small updates look clean. Large areas that change every frame would tear.
4. **Memory.**
   - Flash: 640 KB used of 896 KB, so about 256 KB free for code, glyphs and tables.
   - Internal RAM (RAM_D1, 320 KB): about 3.6 KB free beyond the 24 KB reserved for stack and heap.
   - DTCM RAM (128 KB): unused except 64 bytes for the crash record. Neither the firmware nor the bootloader links anything there.
   - SDRAM: about 650 KB free between the undo history and the images (the preview ring, the bank clipboard and the undo history used the rest of the old 1.1 MB gap).

**What this means in practice**

| Kind of drawing | Size | Verdict |
|---|---|---|
| A graph in the 325×60 area | 19,500 px | Fine on every change, also while playing |
| A panel over three lanes (sound-page curves) | 623×90 ≈ 56,000 px | Fine on a field change |
| One lane redrawn | 623×29 ≈ 18,000 px | Fine |
| All ten lanes | ≈ 285,000 px | Fine on a page or bank change (done today); not as animation |
| Meters, cursors, moving dots | hundreds of px each | Fine at 20–30 updates per second |
| Full-area animation | 285,000+ px per frame | Only after notes no longer depend on the main loop |

## 2. UX possibilities

Sizes: S = a day, M = a few days, L = a week or more, including the hardware test. "Area" says where it is drawn.

### 2.1 Make existing data visible (highest value, low cost)

| # | Idea | Area | Size | Notes |
|---|---|---|---|---|
| U1 | **Per-note settings in the lanes.** Beat brightness or height = velocity; hollow = chance below 100%; a dot = has a condition; a small offset = nudge | Lanes | S–M | Today a beat with velocity 20 and chance 30% looks the same as a plain one. `lcd_drawBeat` already draws each beat |
| U2 | **NOTE page graph.** One bar per beat of the layer, height = velocity, hatch = chance, tag = condition, selected beat highlighted | 325×60 | S | Replaces the text overview. 64 beats fit at 5 px each |
| U3 | **EUCLID necklace.** The pattern on a ring (radius about 28 px) with hits, accents and rotation, next to the linear row | 325×60 | S | `drawCircle` and `fillCircle` exist |
| U4 | **SONG chain as blocks.** One block per step, width = repeats, colour = bank, the playing step highlighted with its loop count | 325×60 | S | Replaces three text lines |
| U5 | **ROUTING as a diagram.** Boxes for EQ, F1, F2, FX1, FX2, REV, OUT joined by lines, unused modules dimmed | 325×60 | M | The data is in `routePatch`; needs a small layout routine |
| U6 | **Solo, mute and style at a glance.** Dim the whole lane of a muted or non-soloed layer | Lanes | S | Today only a 10 px box changes |

### 2.2 Curves for the sound pages

The Filter, EQ, effect and reverb pages use all eight fields, so tabs 2–3 are not free there. Their graphics go in a **panel over the song part of the top three lanes** (about 623×90 px) while the page is open; the three lanes are redrawn on leaving (about 54,000 pixels). The LFO and instrument pages keep their 325×60 area.

| # | Idea | Area | Size | Notes |
|---|---|---|---|---|
| U7 | **Filter response curve** from type, cutoff, resonance and slope | Lane panel | S–M | Evaluate the filter's magnitude at about 160 frequencies when a field changes. Runs in the main loop, no audio cost |
| U8 | **EQ curve**, six bands summed, with a marker per band | Lane panel | S–M | Same method |
| U9 | **Compressor and expander transfer curve** with a live gain-reduction bar | Lane panel | M | The gain reduction needs one value handed over from the audio interrupt |
| U10 | **Delay taps** on a beat grid (time, feedback, damping) | Lane panel | S | Static drawing from the settings |
| U11 | **Reverb shape**: pre-delay gap and decay slope | Lane panel | S | Static drawing from the settings |
| U12 | **LFO phase dot** moving on the existing shape while playing, plus the list of what the LFO modulates | 325×60 | S | The graph exists |
| U13 | **Sample waveform cursor**: play position, start and end handles, direction | 325×60 | S–M | The waveform exists |

### 2.3 Live feedback

| # | Idea | Area | Size | Notes |
|---|---|---|---|---|
| U14 | **Level meters** per layer, with a clip mark | Lanes | M | Peak per block computed in the audio interrupt (one compare per sample), read by the main loop. The master already has the two LED meters |
| U15 | **Limiter activity** next to the master meter | Strip | S | Needs U14 |
| U16 | **MIDI indicators**: in, out, clock locked or free, the measured tempo with decimals, error count | Strip | S | The data exists since V1.13 |
| U17 | **Unsaved-changes dot** and time since the last autosave | Strip | S | The autosave checksum already says whether the song changed |
| U18 | **Undo depth**: how many steps back and forward | Strip | S | From `UndoLog` |

### 2.4 Finding and understanding functions

These are not visualizations, but they address what has caused the most confusion while testing: two S keys, hidden key combinations, FILL meaning two things, a 5-character status spot.

| # | Idea | Area | Size | Notes |
|---|---|---|---|---|
| U19 | **Key-hint line.** While ADD, M, F, S, SONG, L, I, the right S or the encoder key is held, show what the other keys do on this page | Strip or bottom menu | M | A table per page and modifier. The largest single gain in discoverability |
| U20 | **A real status line** instead of the 5-character spot: "UNDO: MOVE", "CHAIN OFF: BANK 3", "FKEY HELD", "SAVING" | Strip | S–M | The strip between the top menu and the lanes has room left of the play line |
| U21 | **Page dots** for pages that cycle: instrument 1/2/3, song / EUCLID / NOTE | Icon area | S | |
| U22 | **Large value popup** while a knob turns (name, value, unit, a bar for the range) | Tab or centre | M | Helps on stage |
| U23 | **Lowercase and extra glyphs** in the 7×9 and 10×14 fonts (arrows, note, mute, solo, lock) | Fonts | S–M | About 256 KB of flash is free. Removes workarounds such as `+` for ghost hits |
| U24 | **Consistent colour roles**: one colour per meaning (header, value, status, error, selected, modulated) and optionally a colour per layer | All | M | Mostly a cleanup pass |

### 2.5 Full-area views (the lanes area, 920×310)

These replace the lanes while a page is open. They are the "dedicated visualizations" in the strong sense.

**Owner's direction (2026-11-13): use this big area for the dedicated views.** Opening one draws the area once (about 285,000 pixels, like a bank change today) and then only what changes, so static views with a moving playhead or bar do not need E2; only continuous animation of the whole area does. Further views in this style, mocked up in the app's Screen ideas tab: **Euclid rings** (ten layers as concentric rings with a clock-hand playhead), **Song timeline** (the chain plus all five banks as thumbnails), **Routing** (modules and one row per layer), **Filter and EQ curves** over the whole area.

| # | Idea | Size | Notes |
|---|---|---|---|
| U25 | **MIX view.** The ten lanes become ten channel strips: level, pan, sends and filter as horizontal bars, with live meters | M–L | The natural home for per-channel sends (section 3). Lanes are already one row per layer |
| U26 | **Note editor for one layer.** The loop across the full width, beats as blocks, a velocity lane and a chance lane below | L | A larger NOTE page. Needs cursor movement with the arrow keys |
| U27 | **Sample editor.** The waveform at full width with zoom, start and end, and later slices | L | The waveform shown today has 60 points per sample; a wide view needs a pass over the sample in SDRAM |
| U28 | **Routing patchbay.** Sources as rows, destinations as columns, a cell per send level | M–L | Pairs with U25 |
| U29 | **Performance view.** Large mute and solo states, chain progress, bar counter and tempo | M | For playing live |

### 2.6 Enablers

| # | Enabler | Why | Size |
|---|---|---|---|
| E1 | **Drawing benchmark** on the test page: fill, text and image speed in megapixels per second, and the main loop's pass time while playing | Turns the estimates in 1.4 into numbers and sets a draw budget per pass | S |
| E2 | **Trigger pad and MIDI notes outside the main loop** (from a timer interrupt reading the note queue) | Makes note timing independent of drawing, SD writes and autosave. Required for U25–U29 and any animation; also removes the note delay during an autosave | M |
| E3 | **Draw budget per pass**: a small queue of dirty regions, drawn a few at a time | Keeps keys responsive during large redraws | M |
| E4 | **Meter taps in the audio interrupt**: peak per layer and master, gain reduction | U9, U14, U15, U25 | S |
| E5 | **Use the DTCM RAM** for new state | Internal RAM is nearly full | S |

### 2.7 Suggested order

1. **E1**, then **U1, U2, U3, U4, U6**: the features added since V1.10 become visible. About a sprint.
2. **U19, U20, U21, U23**: the device explains itself.
3. **U7, U8, U12, U13, E4, U14, U16, U17**: sound pages and live feedback.
4. **E2, E3**, then **U25** together with the signal path work in section 3.

## 3. Viability: per-channel effects with sends

### 3.1 The signal path today

- **Sources:** 10 layers (one mono voice each), the metronome and line-in.
- **Modules, in a fixed order:** EQ (6 bands), Filter 1, Filter 2, Effect 1, Effect 2, Reverb. One instance of each, shared by everything routed through it.
- **Routing (`Routing.h`, V1.09):** each layer has one entry point (OUT). Each module sends its output to a later module or to the master. The patch is loop-free and adds no latency.
- **Mono until the reverb.** The reverb's SURROUND creates the right channel. The mixer PAN is a balance on the final output.
- **Effects carry their own DRY and WET.** Two layers through the same effect always get the same dry/wet mix.
- **Cost:** 34% of the audio block in the worst case, measured at V1.04 (`BACKLOG.md`). Not re-measured since the routing engine.

What a user cannot do today: give the snare a lot of reverb and the kick a little; filter one layer without filtering the others on the same route; place layers left and right.

### 3.2 What "per-channel effects with sends" would be

The usual drum machine layout, with what each part needs:

```
layer voice ─► [channel: filter · drive] ─► level · pan ─┬─► MASTER (dry, stereo)
                                                         ├─► send A ─► EFFECT 1 ─┐
                                                         ├─► send B ─► EFFECT 2 ─┤─► returns ─► MASTER
                                                         └─► send R ─► REVERB  ──┘
MASTER ─► EQ ─► limiter ─► out
```

- **Per channel (insert):** light processing that belongs to one sound: a multimode filter, drive, and the bit reduction that already exists per layer (RESOLUTION).
- **Sends:** a level per layer into each shared effect.
- **Shared effects:** delay, chorus, flanger, phaser and reverb stay single instances, fed by the sends.
- **Master:** EQ and the limiter act on the sum.

### 3.3 Viability by part

| Part | Viable? | Why |
|---|---|---|
| **Sends with a level per layer** | Yes, low risk | The engine already routes a source to several destinations (bitmask), and `routeScatter()` already takes a gain per sample for its crossfades. Backlog items 4 and 5 describe exactly this: the engine side is done, the UI and file side are not |
| **A filter per layer** | Yes | Ten small filters. Estimated 2–4% of the audio block (below) |
| **Drive or distortion per layer** | Yes | A few operations per sample per layer |
| **EQ per layer (2 bands)** | Probably | Two more filters per layer, another estimated 3–7% |
| **Delay, chorus or reverb per layer** | No | A delay line is 384 KB and a chorus 96 KB of SDRAM each, and the cost multiplies by ten. Sends make it unnecessary |
| **Pan per layer on the dry path** | Yes | Backlog item 36 A: two gains per layer and a stereo dry bus |
| **Stereo through the effects** | Uncertain | Doubles the cost of every effect. Needs measuring first (item 36 B) |
| **A third send effect** | One more, not two | A delay plus a chorus buffer is 480 KB and about 650 KB of SDRAM is free. Two more (960 KB, item 6) no longer fit unless the undo history shrinks or one sample slot (960 KB) is given up |

### 3.4 Cost estimates

The audio block is 32 samples at 44.1 kHz, so 726 µs, which is about 399,000 processor cycles at 550 MHz.

| Addition | Estimate per block | Share of the block |
|---|---|---|
| Send mixing: 10 layers × 4 destinations × 32 samples | ~4,000–8,000 cycles | 1–2% |
| Filter per layer: 10 × 32 samples × 20–40 cycles | 6,400–12,800 cycles | 1.6–3.2% |
| Drive per layer | ~3,000 cycles | under 1% |
| 2-band EQ per layer | 12,800–25,600 cycles | 3–6.5% |
| Pan and a stereo dry bus | ~2,000–4,000 cycles | under 1% |

- These are estimates from counting operations, not measurements. Idle layers cost nothing, so typical load is lower.
- Added to the 34% measured at V1.04, sends plus a filter and drive per layer land around 40%, well under the 80% gate the backlog sets.
- The data cache is off (`main.cpp` enables only the instruction cache). Turning it on could lower every figure, but it needs care with the DMA buffers for audio, SD and MIDI, so I would not count on it.

### 3.5 Memory

- **New state is small:** a filter per layer is about 40 bytes of state and coefficients, sends are 3 bytes per layer.
- **Internal RAM is nearly full,** so new audio state should go to the unused DTCM RAM (128 KB). It is also the fastest memory, which suits per-sample state.
- **Audio buffers:** a stereo dry bus is two more 32-sample buffers (256 bytes).

### 3.6 Files and old songs

- **The layer record is full** (25 bytes per layer). Sends, pan and the channel filter need about 8 bytes per layer, so 80 bytes: a new block after the song's existing extra blocks (chain `CHN`, notes `BTX`). Older firmware stops reading before it, as it does today.
- **This is the same file extension** that backlog items 3, 24, 35 and 36 A are waiting for. One block can serve all of them.
- **Old songs must sound the same.** Today a layer's OUT is one entry point into a serial chain. Two ways to keep that:
  - **Keep both models.** A song is either CHAIN (today) or SENDS. The routing engine is table-driven, so SENDS is mostly a different patch: layers go to MASTER plus the three send destinations, and Effect 1, Effect 2 and Reverb go to MASTER. Old songs load as CHAIN.
  - **Convert on load.** Map each old OUT to sends. This changes the sound when several layers shared one effect's dry/wet setting, so I would not do it silently.
- **Recommendation:** keep both models, selected per song.

### 3.7 What does not fit the current engine

- **EQ on the master.** Modules run in a fixed order with EQ first, so "EQ after everything" needs either a user-set module order (backlog item 7) or a separate master stage. A separate stage is simpler. On a stereo master it costs twice the mono EQ.
- **DRY inside the send effects.** With sends, an effect's DRY should be off and its WET is the return level. The effect pages need to show that in SENDS mode.
- **Destination latched per note.** A layer's destination is fixed when its note starts, so a change applies at the next hit. Send levels should instead follow the knob with a short ramp, which the crossfade code can do.
- **Level into the limiter.** Dry plus three returns is louder than a serial chain. The return levels and the limiter ceiling need a gain-staging pass by ear.

### 3.8 User interface

- **Per layer:** a MIX page with FILTER TYPE, CUTOFF, RESONANCE, DRIVE on the top row and PAN, SEND A, SEND B, SEND REV on the bottom row. Eight fields, one per encoder. LEVEL stays on instrument page 1.
- **All layers at once:** the MIX view (U25), where each lane shows its level, pan, sends and filter as bars with a live meter. Pads select the layer; the encoders edit the selected layer.
- **Routing page:** shows the mode (CHAIN or SENDS) and, in SENDS mode, the return destinations.

### 3.9 Phases

| Phase | Content | Size | Depends on |
|---|---|---|---|
| S0 | Measure today's load with `make PROFILE=1` on real songs | S | — |
| S1 | Sends: a level per layer into Effect 1, Effect 2 and Reverb plus dry to master; SENDS mode per song; the file block; the MIX page | M | S0 |
| S2 | Filter and drive per layer, state in DTCM; LFO to cutoff (a first part of backlog item 3) | M | S1 |
| S3 | Pan per layer and a stereo dry bus (item 36 A) | S–M | S1 |
| S4 | MIX view with meters (U25, E4) | M–L | S1, E2 |
| S5 | Master EQ stage; stereo returns; a third send effect | M–L | measurements from S0–S3 |

### 3.10 Risks

- **Sound quality can only be judged on the device.** Host tests cover the routing arithmetic (`test_routing`), not how it sounds.
- **Unmeasured load.** Every figure in 3.4 is an estimate. S0 comes first.
- **Two models to maintain** (CHAIN and SENDS), in the UI and in the tests.
- **More pages.** The keypad has no free menu keys; the MIX page would be a fourth instrument page reached by tapping I.

### 3.11 Verdict

Viable, and best done as an extension of the routing engine rather than a new signal path. Sends (S1) give most of the musical benefit for the least risk, because the engine work is already done and tested. The channel filter (S2) is the part that makes it "per-channel effects". Stereo beyond the dry path and extra effect slots should wait for measurements.

## 4. How the two halves connect

- The MIX view (U25) is the screen the sends need. Building sends without it means ten separate pages; building it without sends leaves it half empty.
- Notes triggered outside the main loop (E2) serve both: richer drawing, and no note delay during autosave or SD work.
- The file block for sends is the one that also unblocks LFO destinations (items 3, 35), velocity modulation (24) and pan (36).

## 5. What to measure before deciding

1. Drawing speed: fill, text and image, in megapixels per second (E1).
2. Main-loop pass time while playing, idle and during a lane redraw.
3. Audio load today on a full song, with `make PROFILE=1`: average, peak, and the UDR/OVR counters.
4. Whether enabling the data cache is safe with the current DMA use, as a separate experiment.

## 6. Open decisions for the owner

1. Which visualization group to build first (2.7 suggests the order).
2. Sends as a per-song mode next to today's chain, or a full replacement.
3. Whether a channel filter per layer should replace Filter 1 and Filter 2, or sit alongside them as bus filters.
4. Whether a third send effect is worth giving up undo history or a sample slot.
