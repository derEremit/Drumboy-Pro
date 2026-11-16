# Notes for AI assistants working on this repo

Fork: MiguelMoraleja/Drumboy-Pro. Firmware: `02 Software/Drumboy-Pro-App-H723`. Planned work: `BACKLOG.md`.

## Before pushing firmware changes

- `make -C "02 Software/Drumboy-Pro-App-H723" WERROR=1` (our code builds with zero warnings)
- `make -C "02 Software/tests"` (host tests: fills, routing, beat list, Euclid)
- Put hardware-free logic in `Core/Library/Global/*.h` with a host test.
- Test builds for the owner's device: `make package`; the file must reach the SD card at its exact size (the bootloader shows SIZE-CHECK otherwise).

## Sound quality comes first

The owner's standing rule (2026-11-13): never trade sound quality for speed, memory or simplicity. If the audio engine runs out of processor time, find the room elsewhere (data cache, faster code, the unused DTCM RAM, a leaner screen, a specialized firmware build for heavy use) and do not thin out filters, effects, bit depth or sample rate. A cheaper algorithm is only acceptable if it sounds the same. Say so in the backlog when a choice touches this.

SD writes with the data cache on need a cache clean before every DMA (`FATFS/Target/sd_diskio.c`): a missing clean in the scratch-buffer path corrupted two cards (backlog 56). Any new DMA from a cacheable buffer needs the same.

The Makefile does not see a change of `DCACHE` or `PROFILE` (the data cache is on by default since V1.15; `DCACHE=0` turns it off): use a clean or another `BUILD` directory after changing them, and build the update package with a clean `build/`. Update file names stay within 28 characters (the bootloader ignored a 31-character name).

## Crash reports

A crash restarts the firmware and shows `CRASH k AT pppppppp` (also a line in `System/Crash.log` on the SD card; `CrashRecord.h`). To find the code: check out the commit of that firmware, `make`, then `arm-none-eabi-addr2line -f -C -e build/app.elf 0xpppppppp` (also the `lr` value from the log). Builds are reproducible, so the addresses match the released file. kind: 1 HardFault, 2 MemManage, 3 BusFault, 4 UsageFault, 5 NMI, 6 Error_Handler.

## Every release

1. Bump `Version.h`, merge to `main`; the owner creates the release `vX.YY` (lowercase v) on github.com.
2. Check the Release workflow attached `Drumboy_Pro_Fw_XX_YY.bin` and that it matches the hardware-tested file.
3. Update the app `docs/app/drumboy-pro-inside.html`: new row at the top of `RELEASES`, version in the header, and every tab the release changes (new features get explained there). Republish it to https://claude.ai/artifact/VvFjAK4W1L5SNTW9hCqYEj (pass that URL so the link stays the same).
4. Update `BACKLOG.md` status and the README feature list.
