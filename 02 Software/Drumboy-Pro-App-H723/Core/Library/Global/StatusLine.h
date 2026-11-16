// Status line and layer meters (V1.15). Hardware-free so they can be tested on a PC (see
// 02 Software/tests).
//
// STATUS LINE: one line of small text in the strip between the top menu and the lanes, right
// aligned. While a modifier key is held it says what the other keys do with it; otherwise it
// shows the MIDI clock and the autosave. The font has no lowercase and no "=", "(", ")" or "&".
//
// LAYER METERS: a small bar in each lane (22 x 4 px) showing the layer's peak level from -48 dB
// to 0 dB, rising at once and falling 2 px per update, with a red square when a block reached
// full scale.
#ifndef __STATUSLINE_H
#define __STATUSLINE_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ---- Status line ----------------------------------------------------------------------------
const uint16_t kStatusRightX = 928, kStatusY = 103, kStatusClearX = 305, kStatusClearWidth = 624, kStatusClearHeight = 9;
const uint8_t kStatusMaxChars = 88;  // 623 px / 7 px per character of the 5x7 font

enum StatusHeld : uint8_t {
    HELD_MUTE = 1,
    HELD_FILL = 2,
    HELD_SOLO = 4,   // the left S key
    HELD_INST = 8,   // the I key
    HELD_SONGP = 16, // the right S key (LAYER SONG)
    HELD_SONG = 32,  // the SONG key
};

// What the held keys do, or nullptr when none is held (the lowest bit wins when several are).
inline const char* statusHint(uint8_t held_) {
    if (held_ & HELD_MUTE) return "M HELD   PAD MUTES   < > UNDO REDO   ADD MUTE ALL   CLEAR UNMUTE ALL";
    if (held_ & HELD_FILL) return "F HELD   FKEY BEATS PLAY   !FKEY BEATS REST   KNOBS FAST";
    if (held_ & HELD_SOLO) return "S HELD   PAD SOLO   CLEAR UNSOLO ALL   KNOBS SLOW";
    if (held_ & HELD_INST) return "I HELD   PAD OPENS ITS INSTRUMENT PAGES   TAP I: NEXT PAGE";
    if (held_ & HELD_SONGP) return "S HELD   PAD OPENS ITS SONG PAGE   TAP S: SONG > EUCLID > NOTE";
    if (held_ & HELD_SONG) return "SONG HELD   PAD PICKS THE BANK   TAP SONG: THE CHAIN PAGE";
    return nullptr;
}

// The idle line. clockCentiBpm_ 0 = no MIDI clock; autosaveAgeMin_: -1 = off, -2 = on but nothing
// written yet, 0 = just now, else minutes since the last write. Returns the length.
inline int statusIdleText(char* out_, size_t size_, uint32_t clockCentiBpm_, int autosaveAgeMin_) {
    char clock[32] = "";
    if (clockCentiBpm_ > 0) {
        const uint32_t c = (clockCentiBpm_ > 99999) ? 99999 : clockCentiBpm_;
        snprintf(clock, sizeof clock, "CLK %lu.%02lu LOCKED", static_cast<unsigned long>(c / 100), static_cast<unsigned long>(c % 100));
    }
    char save[32];
    if (autosaveAgeMin_ == -1) {
        snprintf(save, sizeof save, "AUTOSAVE OFF");
    } else if (autosaveAgeMin_ == -2) {
        snprintf(save, sizeof save, "AUTOSAVE ON");
    } else if (autosaveAgeMin_ == 0) {
        snprintf(save, sizeof save, "AUTOSAVE JUST NOW");
    } else {
        snprintf(save, sizeof save, "AUTOSAVE %d MIN AGO", (autosaveAgeMin_ > 999) ? 999 : autosaveAgeMin_);
    }
    return snprintf(out_, size_, "%s%s%s", clock, clock[0] ? "   " : "", save);
}

// Minutes between two millisecond counts, for the autosave age (wraps of the counter are fine).
inline int statusAgeMinutes(uint32_t nowMs_, uint32_t thenMs_) { return static_cast<int>((nowMs_ - thenMs_) / 60000u); }

// Tempo of an incoming MIDI clock in hundredths of a BPM from the interval of one clock tick
// (24 per quarter note), 0 for no clock.
inline uint32_t statusCentiBpm(float tickIntervalUs_) {
    if (tickIntervalUs_ < 1.0f) return 0;
    const float bpm100 = 6000000000.0f / (tickIntervalUs_ * 24.0f);
    return (bpm100 > 99999.0f) ? 99999u : static_cast<uint32_t>(bpm100 + 0.5f);
}

// ---- Layer meters ---------------------------------------------------------------------------
const uint16_t kMeterX = 276, kMeterY = 13, kMeterWidth = 22, kMeterHeight = 4;  // y from the lane's top
const uint16_t kMeterBarWidth = 18;                                              // pixels of level; the rest is the clip square
const float kMeterFloorDb = -48.0f;
const uint16_t kMeterFallPixels = 2;

// Pixels of level for a peak (1.0 = full scale): 0 below -48 dB.
inline uint16_t meterPixels(float peak_) {
    if (!(peak_ > 0.0f)) return 0;
    const float db = 20.0f * log10f(peak_);
    if (db <= kMeterFloorDb) return 0;
    if (db >= 0.0f) return kMeterBarWidth;
    const uint16_t px = static_cast<uint16_t>(1.0f + (db - kMeterFloorDb) * (static_cast<float>(kMeterBarWidth) - 1.0f) / (-kMeterFloorDb) + 0.5f);
    return (px > kMeterBarWidth) ? kMeterBarWidth : px;
}

inline bool meterClip(float peak_) { return peak_ >= 0.999f; }

// Rises at once, falls 2 px per update.
inline uint16_t meterHold(uint16_t shown_, uint16_t now_) {
    if (now_ >= shown_) return now_;
    return ((shown_ - now_) > kMeterFallPixels) ? static_cast<uint16_t>(shown_ - kMeterFallPixels) : now_;
}

#endif
