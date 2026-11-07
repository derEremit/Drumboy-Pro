# Drumboy Pro simulator

Runs the real firmware on macOS or Linux (x86_64) so changes can be checked without flashing: as a visual panel you play with keyboard and mouse, or headless, driven by a script or a socket (by you or an AI assistant). Each run boots the firmware, then presses keys, turns encoders, takes screenshots, records audio and checks state. Jira epic DRUMB-29; MIDI comes later.

## Build

Needs CMake and Ninja: on PATH (`brew install cmake ninja`), or the copies the STM32 VS Code extension bundles under `~/Library/Application Support/stm32cube/bundles/`. `run_sim.sh` finds both bundles by itself; with a plain `cmake` the preset's toolchain file (`Simulator/cmake/host.cmake`) finds the bundled Ninja. The first build fetches SDL3.

```sh
cmake --preset Simulator
cmake --build --preset Simulator          # -> build/Simulator/Simulator/DrumboySim

# SD card image from a folder with the card contents (once)
Simulator/tools/make_card_image.sh <SD card folder> ~/Library/Application\ Support/DrumboySim/drumboy.img
```

On Linux the preset is `SimulatorLinux` (clang, system SDL3, poppler-glib and cairo for the panel drawing, `dosfstools` and `mtools` for card images; on Arch: `sdl3 poppler-glib cairo dosfstools mtools clang cmake ninja`). The executable is linked without PIE so that the MCU addresses can be mapped below 4 GB. Card images default to `$XDG_DATA_HOME/DrumboySim` (`~/.local/share/DrumboySim`), the shortcuts use Ctrl instead of Cmd and Alt instead of Option. `DRUMBOY_SIM_OPT=-O0` configures an unoptimised build for debugging.

```sh
cmake --preset SimulatorLinux
cmake --build --preset SimulatorLinux     # -> build/SimulatorLinux/Simulator/DrumboySim
Simulator/tools/make_card_image.sh <SD card folder> ~/.local/share/DrumboySim/drumboy.img
```

## Card images

The images live outside the repo, so every checkout and worktree shares them. Defaults and overrides (`Simulator/tools/sim_env.sh`):

| Variable | Default | Used for |
|---|---|---|
| `DRUMBOY_SIM_DATA` | `~/Library/Application Support/DrumboySim` (macOS), `$XDG_DATA_HOME/DrumboySim` (Linux) | Folder for the two images below |
| `DRUMBOY_SIM_CARD` | `$DRUMBOY_SIM_DATA/drumboy.img` | Card you play with (`run_sim.sh`; `DrumboySim` uses it when `--card` is not given) |
| `DRUMBOY_SIM_TEST_CARD` | `$DRUMBOY_SIM_DATA/test.img` | Card the tests run on (`run_tests.sh`) |
| `DRUMBOY_SD_BACKUP` | none | Folder with SD card contents, copied onto the test card when it does not exist yet |

A card image given as the first argument of `run_sim.sh` / `run_tests.sh`, or `--card` for `DrumboySim`, wins over the variables. A missing image stops the script with a message saying how to make one.

## Window

```sh
Simulator/tools/run_sim.sh                # builds if needed, opens the panel on the playing card
Simulator/tools/run_sim.sh other.img      # on another card image
Simulator/tools/run_sim.sh --card-copy    # same, but changes don't stay on the card image
```

The window shows the official panel drawing (`01 Hardware/Drawing/Drumboy Pro Graphic.ai`, rendered at the window's resolution) with the live LCD. Sound plays through the Mac's default output.

- **Keys:** the Mac shortcut is printed in each key's bottom-right corner; clicking works too. A pressed key is highlighted, including presses from scripts and `simctl`.

  | Keys | Shortcuts |
  |---|---|
  | RESET, PLAY/STOP, REC | delete, space, R |
  | SELECT, UP, CLEAR / LEFT, GLOBAL, RIGHT / COPY, DOWN, PASTE | return, ↑, X / ←, esc, → / C, ↓, V |
  | MUTE, FILL, STYLE; SONG (bottom left) | Q, W, E; tab |
  | FILE, DRUMKIT, MIXER | F, D, M |
  | RHYTHM, METRONOME, IO / FILTER 1, FILTER 2, EQ / EFFECT 1, EFFECT 2, REVERB | T, N, O / H, J, G / Y, U, B |
  | LFO, LAYER INST, LAYER SONG; KNOB (bottom right) | L, I, S; K |
  | Pads 1–10 | 1 … 9, 0 |

- **Knobs:** drag left/right (or up/down), two-finger scroll over a knob, or hold ⌥ Option over a knob and move the pointer (the pointer stays put while Option is held). Knobs are numbered 1–8 (top row 1–4); `turn` uses 0–7.
- **LEDs:** pads light from the firmware's pad LED expander (a voice playing), the side meters from the EQ LED expanders.
- **Power cycle:** ⌘R (needed after switching plugins in the manager). **Quit:** ⌘Q or close the window.

SONG and KNOB are assigned to the bottom-left and bottom-right keys by keypad wiring; swap them in `src/sim_window.cpp` if the device disagrees.

## Test

```sh
DRUMBOY_SD_BACKUP=<SD card folder> Simulator/tools/run_tests.sh   # first run: creates the test card
Simulator/tools/run_tests.sh              # every Simulator/tests/*.sim, prints PASS/FAIL
Simulator/tools/run_tests.sh card.img Simulator/tests/smoke.sim   # one test on another card
```

Tests use their own card, `$DRUMBOY_SIM_TEST_CARD` (made from `$DRUMBOY_SD_BACKUP` on first run, so your playing card's plugins and files don't affect results), and each test runs on a throwaway clone of it (`--card-copy`). The log, screenshots and WAVs go to `build/Simulator/test-output/<test>/`.

### Interactive (step by step, e.g. from an AI session)

```sh
cd build/Simulator && Simulator/DrumboySim --card ~/Library/Application\ Support/DrumboySim/drumboy.img --card-copy \
    --delay-scale 0.2 --control sim.sock --out . &
../../Simulator/tools/simctl --socket sim.sock wait-ready "press RHYTHM" "screenshot rhythm.png" state
../../Simulator/tools/simctl --socket sim.sock quit
```

Socket paths are limited to 103 characters, so a relative path is easiest.

### Commands

One per line in a script, or one per argument to `simctl`. Each command replies `ok …` or `error …`, and a script stops at the first error (exit code 1).

| Command | |
|---|---|
| `press KEY[+KEY…] [hold_ms]` | Press and release; `+` holds keys together (`press MUTE+3`) |
| `down KEY` / `up KEY` | Hold / release |
| `keys` | Key names: RESET PLAYSTOP(PLAY) REC ADD UP CLEAR LEFT CENTER RIGHT COPY DOWN PASTE MUTE FILL STYLE SONG FILE DRUMKIT MIXER RHYTHM METRONOME IO FILTER_0 FILTER_1 EQ EFFECT_0 EFFECT_1 REVERB LFO LAYER_INST LAYER_SONG KNOB, pads LAYER_0…LAYER_9 (or 0…9) |
| `turn ENC DETENTS` | Encoder 0–7, negative = counter-clockwise; decoded by the firmware's own quadrature filter |
| `button` | Function button (test mode) |
| `wait MS`, `wait-ready`, `wait-menu NAME`, `wait-stable [quiet_ms]` | Waits: fixed time, main loop reached, menu shown, screen unchanged for quiet_ms |
| `screenshot PATH` | LCD as PNG (960×540) |
| `window-screenshot PATH` | Whole panel window as PNG (with the window open) |
| `record PATH MS`, `level [MS]` | Audio output as WAV (44.1 kHz, int32 stereo); peak/RMS in dBFS |
| `state`, `get FIELD` | JSON: menu, play, record, tempo, layer, bank, alert, lcd.*, audio.*, leds |
| `expect FIELD OP VALUE` | Assertion, OP is `== != < <= > >=` (`expect menu == RHYTHM`) |
| `warnings` | Firmware problems detected at run time (see below) |
| `restart` | Power cycle (re-exec on the same card, e.g. after switching plugins) |
| `quit [code]` | Exit |

Options: `--card IMAGE`, `--card-copy`, `--script FILE` (or `-` for stdin), `--control SOCKET`, `--out DIR`, `--run-ms N`, `--delay-scale F`, `--window` / `--headless`. The window opens unless `--script` or `--run-ms` is given, and `--window` shows it while a script runs. A scale below 1 shortens `HAL_Delay`, mostly the boot animation (boot takes ~2.5 s at 0.2, ~9 s at 1).

### Firmware problems it reports

- **sprintf overflows:** the sim writes only the bytes that fit, as they land on the device. The extra bytes are dropped, so the screen matches the device. Each call site is reported once in `warnings` and on stderr.
- **Crashes:** the sim prints the signal, PC and load address. Symbolize with `atos -arch x86_64 -o DrumboySim -l LOAD PC`. Integer division by zero crashes here, but returns 0 on the Cortex-M7.
- **`-DDRUMBOY_SIM_STACK_CHECK=ON`:** builds the firmware with `-fstack-protector-all` to catch other stack overruns.

## How it works

The application (`Core/Library`, `Core/Plugin`, `main.cpp`), the CubeMX init files (`Core/Src`) and FatFs compile against the real ST headers. None of them change for the sim. Only the hardware layer is replaced:

| Device | Simulator |
|---|---|
| CMSIS core (`core_cm7.h`, intrinsics) | `hal/core_cm7.h`; interrupt masking in `src/sim_cpu.cpp` |
| HAL driver sources | `src/sim_hal.cpp` |
| SDRAM, RAM_D3, peripheral and core registers | memory mapped at the same addresses (`src/sim_memory.cpp`) |
| Interrupts | timer thread (`src/sim_timers.cpp`), audio clock, panel events. ISRs run under one interrupt lock that `__disable_irq` also takes |
| LCD (NT35516, 16-bit 8080 bus) | `SimReg32` GPIO proxies (`hal/stm32h7xx.h`) feed a bus decoder (`src/sim_gpio.cpp`, `src/sim_lcd.cpp`) |
| Keys, encoders, LEDs (6 × MCP23017, EXTI) | expander register model + EXTI (`src/sim_panel.cpp`); panel window with SDL3 (`src/sim_window.cpp`) |
| Audio (I2S DMA, SGTL5000) | 44.1 kHz thread calling the DMA half/full callbacks, 60 s history (`src/sim_audio.cpp`), SDL3 playback |
| SD card (`sd_diskio.c`) | FatFs on a raw card image (`src/sim_diskio.cpp`) |
| `main()` | compiled as `firmware_main()` and run on its own thread (`src/sim_main.cpp`) |

`hal/sim_prelude.h` is force-included into every source. It neutralises the MCU `section` attributes, gives FatFs a 32-bit `DWORD`, restores `__weak` and routes `sprintf` through the checked wrapper (`hal/sim_sprintf.h`; `hal/cstdio` puts the macro back after libstdc++'s `<cstdio>` undefines it, which otherwise let the firmware's overflowing `sprintf` calls reach libc on Linux).

`Lcd.cpp` is compiled from a generated copy (`cmake/lcd_source.cmake`). Its pixel loops cache `volatile uint32_t* reg = &(port->BSRR)`, which would bypass the GPIO proxies, so the copy declares them `auto`.

**Why x86_64:** the firmware keeps MCU addresses in `uint32_t` (SDRAM `0xC0000000`, icons at `0x38000000`, `DWT_CYCCNT`). The simulator therefore maps memory at those exact addresses, below 4 GB. arm64 macOS reserves the whole low 4 GB, while an x86_64 binary linked with a small `__PAGEZERO` may use it. On Apple silicon it runs under Rosetta. On Linux the executable is linked with `-no-pie` and the regions are mapped with `MAP_FIXED_NOREPLACE`; an AddressSanitizer build (`-fsanitize=address` in the C/C++ flags) maps the SDRAM and core regions over ASan's shadow gap, which works for instrumented code but not for libc interceptors such as `memset` or `fread` into SDRAM.

The firmware builds don't change: simulator code lives only in `Simulator/` and behind the `DRUMBOY_SIMULATOR` CMake option.
