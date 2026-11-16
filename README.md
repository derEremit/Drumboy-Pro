# Drumboy Pro

📖 **[Developer Wiki](https://www.randomwaves.io/drumboy-pro/drumboy-pro-wiki.html)** — full hardware specs, schematics walkthrough, firmware architecture, DSP internals, and memory map.

**An open-source hardware groovebox** — sequencer, sampler, and DSP effects engine built around an STM32H723 (Cortex-M7 @ 550 MHz), with a 540×960 touch-free color display, 42 keys, 8 rotary encoders, and full MIDI/Sync I/O.

This repository holds the complete hardware design (KiCad) and firmware (STM32CubeIDE / C++) for Drumboy Pro, released by [Randomwaves](https://randomwaves.io) as an open hardware/software project.

![Drumboy Pro](<01 Hardware/Drawing/Drumboy Pro.png>)

---

## Overview

The MCU sits at the center of six subsystems: an NT35510 display driven over a 16-bit parallel GPIO bus, 16 MB of external SDRAM for sample streaming, an SGTL5000 audio codec over I2S/I2C, a microSD card for storage, a control surface fanned out over six MCP23017 I/O expanders, and external connectivity (MIDI, Sync, USB-C power).

| | |
|---|---|
| **MCU** | STM32H723ZGT6 — Cortex-M7 @ 550 MHz (VOS0), LQFP-144 |
| **Display** | NT35510, 540×960, 16-bit parallel |
| **SDRAM** | 16 MB (IS42S16800J-7TL-TR), FMC bus |
| **Audio codec** | SGTL5000 — I2S3 audio / I2C3 control |
| **Storage** | microSD (TF-01A), SDMMC2, FAT filesystem |
| **Control surface** | 42 keys, 42 LEDs, 8 rotary encoders — 6× MCP23017 over I2C |
| **Connectivity** | MIDI In/Out, Sync In/Out, USB-C power |
| **Flash** | 1024 KB, split between bootloader and application |

## Features

- **10 sample layers**, each independently modulated by a 10-slot LFO matrix (sample-and-hold at note-trigger time)
- **Per-layer routing** into a shared DSP chain: parametric EQ → 2× multimode filter → 2× send effect → stereo reverb
- **9 effect types** per send slot: Delay, Chorus, Flanger, Phaser, Compressor, Expander, Overdrive, Distortion, Bitcrusher
- **Freeverb/Schroeder-Moorer style reverb** with pre-delay and stereo surround derivation
- **Sequencer** with 5 song banks per layer, 64 beats per bank, and algorithmic complex-fill generation (timing alignment × level curve templates)
- **Layer pages** (V1.10): hold I or S (right side, L I S row) and press a layer pad to open that layer's instrument or song page; tap I or S on its own to step through the current layer's pages (instrument 1 → 2 → 3, song ↔ EUCLID) or to switch between its instrument and song pages
- **Beat editing** on the Layer Song page (V1.10): MOVE a beat past its neighbours while keeping its fill (position shown as bar.beat.16th), SHIFT ALL to rotate a whole layer, and a short ADD press to insert a beat
- **EUCLID page** (V1.10, the song page's second page): Euclidean rhythms with steps, hits, rotation and grid, applied live, REPLACE or MERGE with the layer's beats, CLEAR to undo; ACCENTS (V1.11) keeps some hits loud and makes the rest ghost notes
- **Pitch in semitones** (V1.11): −24 to +24, a fast turn jumps by octaves; kits from older firmware keep their exact pitch until you change it
- **Song mode** (V1.12): chain the 5 banks, each for 1–16 loops, up to 16 steps, looping or stopping at the end; tap the SONG key on its own for the SONG page; there, a long COPY / PASTE copies a whole bank (all 10 layers) to another
- **Sample preview** (V1.11): turning SAMPLE plays the sample straight from the SD card (no loading, no length limit); the layer pad replays it; on/off and level on the Metronome page
- **MIDI In/Out** (opto-isolated input) and **Sync In/Out** for external clock/gate sync; since V1.13 the Drumboy follows an incoming MIDI clock tick by tick (fractional tempos don't drift), plays incoming notes at their velocity (running status, chords), sends each hit's real velocity, and sends no clock or transport of its own while it follows one (no MIDI loops); MIDI receive errors show as ERR on the Rhythm page
- **NOTE page** (V1.14, the song page's third page: tap LAYER SONG on the EUCLID page): each beat gets its own VELOCITY, CHANCE (0–100 % in 5 % steps), CONDITION (A:B, !A:B, 1ST, PRE, FKEY and their negations, Elektron-style; FKEY plays while the F key is held, shown as FKEY on screen) and NUDGE (±12 % of a 16th); ADD + turn changes every beat of the bank, CLEAR resets a beat; MIDI recording keeps each note's velocity; songs from older firmware load unchanged
- **SOLO** (V1.14): the left S key (M F S row) + a layer pad soloes that layer (several at once; S + CLEAR clears them); not saved, MUTE untouched. STYLE, which that key used to toggle, is now a field on the layer's instrument page 3 (ADD + turn sets it on every layer)
- **Undo / redo** (V1.14): MUTE + LEFT undoes, MUTE + RIGHT redoes the last beat or bank edit (MOVE, SHIFT ALL, insert, CLEAR, fills, paste, whole-bank paste, EUCLID, NOTE, a recording take), up to 64 steps; a knob gesture or an EUCLID visit is one step; works while playing
- **Per-note marks and dimmed lanes** (V1.14): on the main screen a beat's height shows its velocity, a hollow beat has a chance below 100 %, a yellow dot above marks a condition, and a layer that does not sound (muted, or another layer is soloed) is drawn dimmed.
- **Euclid rings** (V1.14): while the EUCLID page is open, the lane area shows all ten layers as rings around one centre (layer 1 outside, one turn = one loop, a yellow marker orbits with the playhead) and every layer's beats as a row; leaving the page brings the lanes back.
- **Song timeline** (V1.14): while the SONG page is open, the lane area shows the chain as boxes as wide as their repeats, a bar that fills as the playing loop goes by, what comes next, and the five banks as thumbnails of their ten lanes.
- **Routing diagram** (V1.14): while the ROUTING page is open, the lane area shows the modules joined as patched (unfed modules dimmed) and a row per source with a mark under the module it enters.
- **Filter curve** (V1.14): while a FILTER page is open, the lane area shows that filter's response from 20 Hz to 20 kHz, computed from the numbers the audio uses, with the cutoff marked and the other filter behind it.
- **PAN per layer** (V1.15): instrument page 3, next to STYLE (ADD + turn: all layers). Constant power with 0 dB in the centre, so centred layers sound as before. Heard for every layer: the router, the EQ and both filters run in stereo; the effects and the reverb keep the pan of the dry signal and make their own sound in the middle (stereo effects come later).
- **Step edit screen** (V1.15): while the NOTE page is open, the lane area shows the layer's beats across the full width with a velocity lane and a chance lane under them, a dot under beats with a condition and a bar ruler; the selected beat is magenta, with its details at the top right.
- **MISSING SAMPLE names the sample** (V1.16): a second line under the alert gives the first missing layer, instrument and sample (`L3 Kick/Name`), for 3 seconds.
- **Stereo samples** (V1.16): a stereo WAV keeps both channels (it used to keep only the left one). A stereo sample holds half the length (2.7 s, mono stays 5.44 s) in the same memory, in 32-bit float; its PAN is a balance. Longer stereo files are cut at 2.7 s.
- **MIDI thru of clock and transport** (V1.16): with MIDI IN and MIDI OUT both on SYNC or NOTE+SYNC, the incoming clock and start, continue, stop and reset pass on to MIDI OUT, and the Drumboy sends none of its own while it follows, so a synth behind it sees exactly one clock. Notes are not passed on.
- **Ping-pong echo and DUB page 2** (V1.16): press MIXER again while on DUB for a second page with the selected strip's PAN and FX SEND and the echo's ECHO WIDTH (0 = the centred echo, 100 % = the repeats alternate left and right, from the same delay memory).
- **Dub mixer, first step** (V1.15): MIXER, then DUB (then ROUTING). MODE switches the song between CHAIN (the ROUTING patch) and DUB, where every layer is a strip with a BUS (FILT through the EQ and Filter 1, or DIR to the master) and ECHO and VERB sends into Effect 1 and the reverb; ECHO RTN and VERB RTN set the returns. Saved in the song file. While the DUB page is open, the lane area is a console-style desk: twelve wide columns (ten layers, ECHO, VERB) with scribble plates, the FILT | DIR switch, two knobs per column for the sends, a pan bar, a fader with an LED ladder meter and a mute lamp.
- **Sample view** (V1.15): on instrument page 2 (START, END) the lane area shows the sample at full width, cyan between the START and END handles and gray outside, with their values and the length of the played part.
- **Layer meters and status line** (V1.15): a level bar in each lane (peak, red square at full scale); in the strip under the top menu, a line that tells what the other keys do while M, F, S, I or SONG is held, and otherwise shows the MIDI clock tempo and the age of the last autosave.
- **Filter slopes** (V1.14): SLOPE 24 (two sections, flat to the cutoff at the lowest RESONANCE) is new; SLOPE 06 is now a real 6 dB per octave filter instead of a half-and-half mix with the unfiltered sound (songs that used 06 sound different); 12 is unchanged.
- **EQ curve** (V1.14): while the EQ page is open, the lane area shows the six bands as one curve with a handle per band.
- **CPU and MEM monitor** (V1.14): two bars with their percentage in the top-right corner of every page. CPU is the audio engine's load (the slowest audio block of the last quarter second as a share of the block time, magenta from 80 %); MEM is the sample memory in use.
- **Watchdog** (V1.14): if the main loop stops for 6 seconds the device restarts by itself and shows `CRASH 7 S06 P18 L98` (S = what the loop was doing, P = the page in hex, L = the audio load in percent, LOV from 99 %), also written to `System/Crash.log`.
- **Diagnostics** (V1.14): the screen shows SAVE while an autosave writes to the SD card (SDERR if it fails) and the Metronome page shows how long the last one took; after a crash the firmware restarts by itself, shows `CRASH k AT address` and appends the details to `System/Crash.log` on the SD card
- **Autosave** (V1.13): every 2–20 minutes (AUTOSAVE on the Metronome page, default 10) the song goes to two reserved files on the SD card; after a power cut or crash the Drumboy offers RESTORE AUTOSAVE? at power-on (ADD restores, CLEAR discards); LOAD and NEW autosave unsaved work first
- **SD-card firmware updates** via a dedicated bootloader with CRC-32 verified staged writes
- Fully open hardware (KiCad) and firmware (C/C++ against the STM32 HAL) — no CMSIS-DSP or external DSP library, all hand-rolled biquad/delay-line DSP on the Cortex-M7 FPU

## Repository structure

```
01 Hardware/
  Main/       KiCad project — main board (Drumboy_Main)
  Top/        KiCad project — top/control-surface board (Drumboy_Top)
  Drawing/    Mechanical drawings, enclosure models, graphics, fonts

02 Software/
  Drumboy-Pro-Boot-H723/   Bootloader — STM32CubeIDE project (own repo, linked below)
  Drumboy-Pro-App-H723/    Application firmware — STM32CubeIDE project (own repo, linked below)
  Firmware/                 Prebuilt firmware binaries
  Sd Card/                  Reference SD card contents (samples, presets, drumkits, system data)
```

### Related repositories

Firmware is split into two independently-built projects that share one flash device — a bad application build can never brick the update path:

| Project | Responsibility |
|---|---|
| [Drumboy-Pro-Boot-H723](https://github.com/Randomwaves-Team/Drumboy-Pro-Boot-H723) | Checks the SD card for a firmware package, validates and programs it, then jumps to the application |
| [Drumboy-Pro-App-H723](https://github.com/Randomwaves-Team/Drumboy-Pro-App-H723) | The sequencer, DSP engine, UI, and all runtime behavior |

## Hardware

Designed in **KiCad 7**. Open `01 Hardware/Main/Drumboy_Main.kicad_pro` for the main board, or `01 Hardware/Top/Drumboy_Top.kicad_pro` for the control-surface board. All third-party symbols, footprints, and 3D models are vendored under each project's `Library/` folder.

Mechanical design (enclosure and 3D model) lives in `01 Hardware/Drawing/` as **Rhinoceros 8** files — `Drumboy Pro 2D.3dm` and `Drumboy Pro 3D.zip` (zipped to keep it under GitHub's file size limit; unzip to get the `.3dm`).

A full BOM for the main board is generated from KiCad at `01 Hardware/Main/Production/bom.csv`. See the [wiki](https://www.randomwaves.io/drumboy-pro/drumboy-pro-wiki.html) for the annotated schematic walkthrough, clock tree, power-domain isolation scheme, and key component reference.

## Firmware

Built with **STM32CubeIDE**, written in C/C++ against the STM32 HAL. The application can also be built from the command line, which is what CI uses:

```sh
cd "02 Software/Drumboy-Pro-App-H723"
make WERROR=1       # build/app.elf, build/app.bin; warnings in our code fail the build
make package        # build/Drumboy_Pro_Fw_01_04.bin, ready for the SD card
make -C "../tests"  # fill engine tests, run on the PC
```

For performance work, `make BUILD=build-profile PROFILE=1 package` makes a measurement build (`Drumboy_Pro_Fw_01_04_dsp.bin`) that shows the DSP load (average, peak, maximum) and I2S error counts, and, since V1.15, a second line with the average cost of each stage as a share of the block time (IN, MET, LAY, EQ, F1, F2, X1, X2, REV, RTE for the routing, OUT) as text at the bottom of the screen. Normal builds show the load as the CPU bar in the top-right corner (since V1.14), not the text.

Since V1.15 the data cache is on in every build: the SDRAM, where the samples and the effect buffers live, and the main RAM are cached, and the DMA buffers are maintained (see [BACKLOG item 53](BACKLOG.md)). On the owner's device the audio load of a typical song fell from 45 % to 15 % with identical sound. `make DCACHE=0` builds without it (file name ending in `_nc`).

This needs `arm-none-eabi-gcc` (on Ubuntu: `apt install gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib`). `02 Software/tools/package_firmware.py --check <file>` validates a package the same way the bootloader does.

Every push runs the tests and a warning-free build in GitHub Actions. Our own code (`Core/`) builds with zero warnings; keep it that way, since a new warning is often a real bug.

### Releasing a version

The version lives in one place, `Core/Library/Global/Version.h`. To release:

1. Bump `FW_VERSION_MINOR` (or `MAJOR`) in `Version.h` and merge it to `main`.
2. Tag that commit with the same version and push the tag: `git tag v1.04 && git push origin v1.04`. Without a computer, use github.com instead: **Releases → Draft a new release**, type `v1.04` as a new tag on `main`, and publish.
3. The **Release** workflow checks the tag matches `Version.h`, runs the tests, builds, and publishes `Drumboy_Pro_Fw_01_04.bin` on the repo's Releases page.
4. Update the app ([`docs/app/drumboy-pro-inside.html`](docs/app/drumboy-pro-inside.html)): add the version to the **Releases** tab (`RELEASES` in the script), update the version in the header and any tab the release changes, and republish it. The app explains how the firmware works (signal path, fills and volume, Euclid and beat editing) and is kept current with every release.

- Bootloader owns flash sector 0 (`0x08000000`–`0x0801FFFF`)
- Application owns sectors 1–7 (`0x08020000`–`0x080FFFFF`), with the vector table relocated at startup

### Prebuilt firmware

`02 Software/Firmware/` holds SD-card update packages (36-byte header + application binary, see `02 Software/tools/package_firmware.py`):

| File | Version | Notes |
|---|---|---|
| `Drumboy_Pro_Fw_01_01.bin` | V1.01 | Original release |
| `Drumboy_Pro_Fw_01_02.bin` | V1.02 | Adds 8 fill types (ROLL, TECHNO, BREAKS, GARAGE, TRIPLET, EUCLID, RATCHET, FOOTWORK) to the original 7. The application source in this repo contains the same fill library |
| `Drumboy_Pro_Fw_01_03.bin` | V1.03 | First package built from this repo's source with `make package` (arm-none-eabi-gcc 13.2). Same fills as V1.02, plus a range check that makes unknown fill types in a song file load as plain beats. Superseded by V1.04 |
| `Drumboy_Pro_Fw_01_04.bin` | V1.04 | **Current, tested on hardware.** Fixes memory bugs found by the compiler (buffer overflows in on-screen text and long instrument names, dangling text pointers, paste with an empty clipboard). Same fills as V1.03. Also on the repo's Releases page |

To update, copy **one** package to the SD card root and power on. The bootloader flashes the first file it finds matching `Drumboy_Pro_Fw?*.bin`, so don't leave more than one on the card. Songs saved on V1.02 with the new fill types should not be opened on V1.01, which doesn't range-check fill types on load.

### Adding fill types

Fills live in `Core/Library/Global/Fill.h`, which has no hardware dependencies so the tests in `02 Software/tests` can run it on a PC: `kFillTypeDataLibrary` (type names) and `kFillPatternDataLibrary` (8 patterns per type). Each pattern lists `stepSize` hits as start times in `0.0`–`1.0` of the beat, followed by a final `1.0`; `stepSize` can be at most 16. To add a type, bump `kFillTypeLibrarySize` and `kMaxLayerFillType` and append the new entries at the end, so fill types stored in existing song files keep their meaning. Song files store the type in 4 bits, so there is room for one more type (16 in total); compile-time checks enforce this. Run `make -C "02 Software/tests"` after changing a pattern: it checks every type, pattern and mode.

To flash over SWD, wire an ST-Link V2 to the board's `SWDIO` (PA13), `SWCLK` (PA14), and `GND` pins, then build and flash from STM32CubeIDE. See the wiki's [Build, Flash & Debug](https://www.randomwaves.io/drumboy-pro/drumboy-pro-wiki.html) section for details.

## Backlog

Planned work, known issues and where to start in the code: [BACKLOG.md](BACKLOG.md).

## License

Released under the [MIT License](LICENSE).

## Community

- 💬 [Discord community](https://discord.gg/5atevNH6UU) — join the discussion
- 🌐 [randomwaves.io](https://www.randomwaves.io)
- ✉️ [hello@randomwaves.io](mailto:hello@randomwaves.io)
