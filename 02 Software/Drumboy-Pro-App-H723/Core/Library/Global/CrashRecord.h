// Crash record: what the processor was doing when the firmware crashed, kept across the reset and
// shown at the next power-on. Plain C (the fault handlers in stm32h7xx_it.c use it too) and
// hardware-free, so it can be tested on a PC (see 02 Software/tests).
//
// On a fault (HardFault, MemManage, BusFault, UsageFault, NMI) or Error_Handler, the handler fills
// a record at the top of the DTCM RAM, which neither the firmware nor the bootloader uses, and
// resets the processor: RAM keeps its content across a reset (not across power-off), so the
// firmware restarts by itself instead of staying frozen. At the end of start-up the firmware
// reads the record, appends it to System/Crash.log on the SD card and shows
// "CRASH k AT pppppppp" (k = kind, pppppppp = the address that crashed) until ADD or CLEAR.
//
// A second crash before the first record was read (a crash during start-up) does not reset
// again: the handler stays in its loop, as before V1.14, so the device can't reboot forever.
//
// A stall that is not a fault (a wait that never ends) is caught by the watchdog (kind 7): the
// main loop feeds the hardware watchdog every pass and writes what it is doing (a stage number,
// WdStage below) into DTCM RAM; if the loop stops for WATCHDOG_SECONDS the device restarts, and at
// start-up the record says "CRASH 7 S06 P18 L98": S = the stage the loop was in, P = the page (hex), L = the audio load in percent (LOV from 99 %).
#ifndef __CRASHRECORD_H
#define __CRASHRECORD_H

#include <stdint.h>
#include <stdio.h>

#define CRASH_RECORD_ADDRESS 0x2001FFC0u  // top 64 bytes of DTCM RAM (0x20000000, 128 KB)
#define CRASH_MAGIC 0x43525348u           // "CRSH"
#define CRASH_CHECK_SEED 0xA5C35A3Cu

enum {
    CRASH_HARDFAULT = 1,
    CRASH_MEMMANAGE = 2,
    CRASH_BUSFAULT = 3,
    CRASH_USAGEFAULT = 4,
    CRASH_NMI = 5,
    CRASH_ERROR_HANDLER = 6,
    CRASH_WATCHDOG = 7,  // the main loop stopped for too long; the watchdog restarted the device
};

typedef struct {
    uint32_t magic;
    uint32_t kind;     // CRASH_*
    uint32_t pc;       // address that crashed (Error_Handler: who called it)
    uint32_t lr;       // return address of the function that crashed
    uint32_t psr;
    uint32_t sp;       // stack pointer at the fault
    uint32_t cfsr;     // fault status registers (SCB)
    uint32_t hfsr;
    uint32_t bfar;
    uint32_t mmfar;
    uint32_t version;  // (major << 8) | minor
    uint32_t check;    // crashCheck() of the fields above
} CrashRecord;

static inline uint32_t crashCheck(const CrashRecord* r_) {
    return CRASH_CHECK_SEED ^ r_->magic ^ r_->kind ^ (r_->pc << 1) ^ r_->lr ^ r_->psr ^ r_->sp ^ r_->cfsr ^ r_->hfsr ^ r_->bfar ^ r_->mmfar ^ r_->version;
}

// Seals a filled record (magic and check last, so a half-written one is never valid).
static inline void crashSeal(CrashRecord* r_) {
    r_->magic = CRASH_MAGIC;
    r_->check = crashCheck(r_);
}

// A record left by a crash (RAM after power-on holds noise: magic, kind and check must all fit).
static inline int crashValid(const CrashRecord* r_) {
    return (r_->magic == CRASH_MAGIC) && (r_->kind >= CRASH_HARDFAULT) && (r_->kind <= CRASH_WATCHDOG) && (r_->check == crashCheck(r_));
}

static inline void crashClear(CrashRecord* r_) {
    r_->magic = 0;
    r_->check = 0;
}

// The alert on screen: "CRASH 1 AT 0801A2B4" (19 characters).
static inline void crashAlertText(const CrashRecord* r_, char* out_, size_t size_) {
    if (r_->kind == CRASH_WATCHDOG) {
        // "CRASH 7 S06 P18 L98": S = the stage, P = the page (hex), L = the audio load in percent
        // (LOV when it was at or over 99 %: the audio engine starved the main loop). Shown on the
        // screen so the card need not be read.
        const unsigned long stage = (r_->pc >> 8) & 0xFF, page = r_->pc & 0xFF, load = r_->sp / 10;
        if (load >= 99) {
            snprintf(out_, size_, "CRASH 7 S%02lu P%02lX LOV", stage, page);
        } else {
            snprintf(out_, size_, "CRASH 7 S%02lu P%02lX L%02lu", stage, page, load);
        }
        return;
    }
    snprintf(out_, size_, "CRASH %lu AT %08lX", (unsigned long)r_->kind, (unsigned long)r_->pc);
}

// One line for System/Crash.log.
static inline int crashLogLine(const CrashRecord* r_, char* out_, size_t size_) {
    return snprintf(out_, size_, "V%lu.%02lu kind=%lu pc=%08lX lr=%08lX psr=%08lX sp=%08lX cfsr=%08lX hfsr=%08lX bfar=%08lX mmfar=%08lX\r\n",
                    (unsigned long)(r_->version >> 8), (unsigned long)(r_->version & 0xFF), (unsigned long)r_->kind, (unsigned long)r_->pc,
                    (unsigned long)r_->lr, (unsigned long)r_->psr, (unsigned long)r_->sp, (unsigned long)r_->cfsr, (unsigned long)r_->hfsr,
                    (unsigned long)r_->bfar, (unsigned long)r_->mmfar);
}

// Watchdog. The main loop writes where it is; DTCM RAM keeps it across the watchdog reset.
#define WATCHDOG_STAGE_ADDRESS 0x2001FFA0u  // 16 bytes below the crash record
#define WATCHDOG_SECONDS 6

enum WdStageCode {
    WD_STAGE_NONE = 0,
    WD_STAGE_INPUT = 1,      // keys and encoders
    WD_STAGE_SCREEN = 2,     // screen refresh
    WD_STAGE_VIEW = 3,       // a full-area view
    WD_STAGE_SD = 4,         // SD card maintenance
    WD_STAGE_RHYTHM = 5,
    WD_STAGE_MIXER = 6,
    WD_STAGE_AUTOSAVE = 7,
    WD_STAGE_MIDI = 8,
    WD_STAGE_TEST = 9,
    WD_STAGE_FILE = 10,      // loading or saving a file
    WD_STAGE_PRESET = 11,    // saving the system or encoder preset to the SD card
    WD_STAGE_CLIPBOARD = 12, // copy, paste and reset of LFOs, instruments and songs
    WD_STAGE_NAV = 13,       // long key navigation
    WD_STAGE_MIDITX = 14,    // MIDI out
    WD_STAGE_MIDIRX = 15,    // MIDI notes in
    WD_STAGE_TRANSPORT = 16, // MIDI transport and clock in
    WD_STAGE_SYNC = 17,      // sync in
};

typedef struct {
    uint32_t stage;  // WdStageCode
    uint32_t menu;   // page being shown
    uint32_t loops;  // main loop passes since power-on
    uint32_t spare;  // the audio load (the CPU bar) in tenths of a percent, as last shown
} WdStage;

// The record a watchdog restart leaves: pc = (stage << 8) | menu so the screen shows it.
static inline void crashFromWatchdog(CrashRecord* r_, const WdStage* w_, uint32_t version_) {
    r_->magic = 0;
    r_->kind = CRASH_WATCHDOG;
    r_->pc = ((w_->stage & 0xFFFFu) << 8) | (w_->menu & 0xFFu);
    r_->lr = w_->loops;
    r_->psr = 0;
    r_->sp = w_->spare;  // the audio load when it stopped: near 1000 means the audio starved the loop
    r_->cfsr = 0;
    r_->hfsr = 0;
    r_->bfar = 0;
    r_->mmfar = 0;
    r_->version = version_;
    crashSeal(r_);
}

// Independent watchdog reload for a timeout: LSI is about 32 kHz, prescaler 256 -> 125 Hz.
static inline uint32_t watchdogReload(uint32_t seconds_) {
    const uint32_t r = seconds_ * 125u;
    return (r > 4095u) ? 4095u : r;
}

// The capture itself (stm32h7xx_it.c): fills the record from an exception stack frame and resets.
#ifdef __cplusplus
extern "C" {
#endif
void Crash_Capture(uint32_t* frame, uint32_t kind);
#ifdef __cplusplus
}
#endif

#endif
