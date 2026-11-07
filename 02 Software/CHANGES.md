# Changes over factory V1.01

This branch starts at Randomwaves commit `858322d`, the factory V1.01 source, and adds one fix per commit so each can be cherry-picked on its own. Hardware-tested on one Drumboy Pro from 2026-11-07, installed over SWD; see each commit message for details.

| Commit | Change | Tested |
|---|---|---|
| `43ec49c` | Fill-slot leak. `Bank::resetBeat` freed nothing because it deactivated the beat before checking who owned the slot; the fill table filled with orphans and BASIC_2/BASIC_4 stopped being selectable until a reboot. Slot is freed first; a full clear drops the whole table. Host test in `tests/`. | Yes, three Clear / New File rounds |
| `41e1355` | daupuck's "fix buffer overflows and silent SD read/write failures": WAV `chunkSize < 24` clamp, `sd_read` treats a short read as an error, longer instrument file name buffer. Application files only. | Builds; daupuck tested it in their fork |
| `9c38001` | `kVersionMinor = 0x0B` (V1.11) so SD packages from this branch are not labelled 1.01. The app never shows this value. | n/a |
| `16f68f4` | `lcd_drawGlobalMenuData` drew through a pointer to a buffer declared inside a `case`; the paste action compared a bool against -1. Both from daupuck's split commit, without the split. | Builds, compiler warnings gone |
| `bf07c1d` | `CMakeLists.txt` and `cmake/arm-none-eabi.cmake` in `Drumboy-Pro-App-H723/`, CubeIDE Debug flags. | Builds with Arm GNU 14.3.rel1 |
| `2f4c43f` | Randomwaves/Drumboy-Pro#1. After `HAL_I2C_EV_IRQHandler`, an I2C3 or I2C4 event with no owning transfer (`XferISR == NULL`) is cleared and its enables dropped instead of re-entering forever. `i2c3_orphan` / `i2c4_orphan` count the events for an SWD read. | Compiles; not yet triggered on the unit |
| `ca5c2b6` | `keyboard_resetI2C` did a bare RCC reset, leaving I2C3 disabled while the HAL handle said READY; the next blocking codec write would hang the main loop. Now `HAL_I2C_DeInit`, reset pulse, `MX_I2C3_Init`. | Compiles |
| `9ff7444` | Upstream PR #3 by grafster: SD DMA reads into buffers that are not word aligned landed 2 bytes early (SDMMC IDMA ignores the low address bits), corrupting WAVs whose data chunk is not at a multiple of 4. Enables the FatFs scratch-buffer path for unaligned buffers, waits for the card between scratch sectors, aligns the sample load buffer. | Builds; grafster tested it on their unit |
| `9e71acd` | `Firmware/make_sd_package.py` builds and checks SD packages (same format as `Drumboy-Pro-Firmware-Builder.cpp`); CMake post-build writes `Drumboy_Pro_Fw_01_11.bin`. | Repacks the factory payload byte for byte |
| `385e9ec` | This file. | n/a |
| `5f76f21` | Five sprintf buffers one byte too short for their terminator (`fileNum[3]` with `%03d`, `numSample[4]` with `%04d`, the precount `kCountDownData[2]` with `%02d`, `kData[10]` with a 10-character format), and an unassigned encoder no longer reads one past `kEncoderFuncDataLibrary`. Found with cyberyak's simulator on Linux: the precount NUL cleared a saved `this` there and REC from idle stopped working. | Builds; all four simulator tests pass with the fixes |

## Build

Arm GNU Toolchain 14.3.rel1, CMake, Ninja. The toolchain file looks in `$ARM_GNU_HOME/bin`, then a `tools/arm-gnu-toolchain-*-arm-none-eabi/` directory up to four levels above the project, then `/tmp/arm-gnu-14.3`, then `PATH`:

```
cd "02 Software/Drumboy-Pro-App-H723"
cmake -G Ninja -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
ninja -C build
```

The public toolchain does not reproduce the factory bytes (factory source builds to 579144 bytes, the shipped payload is 579224).

## Install

Put exactly one `Drumboy_Pro_Fw*.bin` in the SD card root and power up. The factory bootloader installs the first match it finds, CRC-checks before and after writing, then moves the package to `System/Firmware/` on the card (wiping that folder first) or deletes it on failure. Keep the factory package `Firmware/Drumboy_Pro_Fw_01_01.bin` off the card until you need it.

Over SWD: `probe-rs download --chip STM32H723ZG --protocol swd --binary-format bin --base-address 0x08020000 --verify build/Drumboy-Pro-App-H723.bin`. The bootloader stays untouched either way.
