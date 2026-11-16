// Per-note settings of a beat (backlog 33): velocity, chance, trig condition and nudge.
// Hardware-free so it can be tested on a PC (see 02 Software/tests).
//
// Stored in the Beat itself (Fill.h) so every move, sort, copy and paste carries them, in three
// bytes (internal RAM is tight): `velocity` and a 16-bit `note` word. All zero = the beat plays
// exactly as before V1.14, so old songs, new beats and cleared beats need nothing special.
//
//   velocity      0 = from the beat's pattern (FLAT 100 / GHOST 50 / MUTED), 1-127 = that velocity
//   note [4:0]    chance step: 0 = 100%, n = 100 - 5n %, 20 = 0%
//   note [10:5]   condition code (see below), 0 = always
//   note [15:11]  nudge, signed, in steps of 2 sequencer ticks (800 per quarter), -12..+12
//
// Conditions (Elektron-style trig conditions):
//   A:B   plays on loop A of every B loops (B 2-8)     !A:B  the other loops (B 2-4)
//   1ST   only the first loop since START or a bank change     !1ST  every loop but the first
//   PRE   plays if the layer's last conditioned beat played    !PRE  if it did not
//   FKEY  only while the F (FILL) key is held                   !FKEY only while it is not
//         (named FKEY on the device so it isn't mistaken for the layer's FILL switch)
// "Conditioned" = a beat with a condition other than PRE/!PRE, or a chance below 100%.
//
// Nudge moves a plain beat by up to +-24 ticks (+-12% of a 16th) when it plays; beats are at
// least 50 ticks apart (the finest grid), so nudged beats never swap order. A nudge that would
// land before the loop start or after its end is clamped. Fills keep their own timing.
//
// File: song files get a block after the song chain (older firmware stops reading at "EOF" + the
// chain, so no format version change): "BTX" | 1 | count (2 bytes LE) | count x entry, one entry
// per beat whose note settings are not all default:
//   layer | bank | start interval (2 bytes LE) | velocity | note (2 bytes LE)   = 7 bytes
#ifndef __BEATNOTE_H
#define __BEATNOTE_H

#include <stdint.h>
#include <string.h>

const uint8_t kNoteChanceSteps = 20;  // 5% steps
const int8_t kNoteNudgeMax = 12;      // steps of kNoteNudgeTicks
const uint8_t kNoteNudgeTicks = 2;

// --- condition codes --------------------------------------------------------------------------
enum NoteCondKind : uint8_t { COND_NONE, COND_RATIO, COND_NOT_RATIO, COND_FIRST, COND_NOT_FIRST, COND_PRE, COND_NOT_PRE, COND_FILL, COND_NOT_FILL };

const uint8_t kNoteCondRatioFirst = 1;    // 1..35: A:B, B = 2..8
const uint8_t kNoteCondNotRatioFirst = 36; // 36..44: !A:B, B = 2..4
const uint8_t kNoteCond1st = 45, kNoteCondNot1st = 46, kNoteCondPre = 47, kNoteCondNotPre = 48, kNoteCondFill = 49, kNoteCondNotFill = 50;
const uint8_t kNoteCondCount = 51;

struct NoteCond {
    NoteCondKind kind;
    uint8_t a, b;  // for A:B
};

// Code of A:B (1 <= A <= B), first code for B: 2 -> 0, 3 -> 2, 4 -> 5, ... (sum of 2..B-1).
inline uint8_t noteRatioIndex(uint8_t a_, uint8_t b_) { return static_cast<uint8_t>((b_ - 2) * (b_ + 1) / 2 + (a_ - 1)); }

inline NoteCond noteCondDecode(uint8_t code_) {
    NoteCond c{COND_NONE, 0, 0};
    if ((code_ >= kNoteCondRatioFirst) && (code_ < kNoteCondNotRatioFirst)) {
        uint8_t i = static_cast<uint8_t>(code_ - kNoteCondRatioFirst);
        for (uint8_t b = 2; b <= 8; b++) {
            if (i < b) return NoteCond{COND_RATIO, static_cast<uint8_t>(i + 1), b};
            i = static_cast<uint8_t>(i - b);
        }
    } else if ((code_ >= kNoteCondNotRatioFirst) && (code_ < kNoteCond1st)) {
        uint8_t i = static_cast<uint8_t>(code_ - kNoteCondNotRatioFirst);
        for (uint8_t b = 2; b <= 4; b++) {
            if (i < b) return NoteCond{COND_NOT_RATIO, static_cast<uint8_t>(i + 1), b};
            i = static_cast<uint8_t>(i - b);
        }
    } else {
        switch (code_) {
            case kNoteCond1st: c.kind = COND_FIRST; break;
            case kNoteCondNot1st: c.kind = COND_NOT_FIRST; break;
            case kNoteCondPre: c.kind = COND_PRE; break;
            case kNoteCondNotPre: c.kind = COND_NOT_PRE; break;
            case kNoteCondFill: c.kind = COND_FILL; break;
            case kNoteCondNotFill: c.kind = COND_NOT_FILL; break;
            default: break;
        }
    }
    return c;
}

inline uint8_t noteCondEncode(const NoteCond& c_) {
    switch (c_.kind) {
        case COND_RATIO:
            if ((c_.b < 2) || (c_.b > 8) || (c_.a < 1) || (c_.a > c_.b)) return 0;
            return static_cast<uint8_t>(kNoteCondRatioFirst + noteRatioIndex(c_.a, c_.b));
        case COND_NOT_RATIO:
            if ((c_.b < 2) || (c_.b > 4) || (c_.a < 1) || (c_.a > c_.b)) return 0;
            return static_cast<uint8_t>(kNoteCondNotRatioFirst + noteRatioIndex(c_.a, c_.b));
        case COND_FIRST: return kNoteCond1st;
        case COND_NOT_FIRST: return kNoteCondNot1st;
        case COND_PRE: return kNoteCondPre;
        case COND_NOT_PRE: return kNoteCondNotPre;
        case COND_FILL: return kNoteCondFill;
        case COND_NOT_FILL: return kNoteCondNotFill;
        default: return 0;
    }
}

// The order the COND field steps through: none, FILL, !FILL, PRE, !PRE, 1ST, !1ST, the ratios by
// B then A (1:2, 2:2, 1:3, ...), then the negated ratios.
inline uint8_t noteCondAtStep(uint8_t step_) {
    static const uint8_t kHead[7] = {0, kNoteCondFill, kNoteCondNotFill, kNoteCondPre, kNoteCondNotPre, kNoteCond1st, kNoteCondNot1st};
    if (step_ < 7) return kHead[step_];
    if (step_ < 7 + 35) return static_cast<uint8_t>(kNoteCondRatioFirst + (step_ - 7));
    if (step_ < kNoteCondCount) return static_cast<uint8_t>(kNoteCondNotRatioFirst + (step_ - 42));
    return 0;
}

inline uint8_t noteCondStepOf(uint8_t code_) {
    for (uint8_t s = 0; s < kNoteCondCount; s++)
        if (noteCondAtStep(s) == code_) return s;
    return 0;
}

// Next/previous condition in field order, wrapping.
inline uint8_t noteCondStep(uint8_t code_, bool up_) {
    const uint8_t s = noteCondStepOf(code_);
    const uint8_t n = up_ ? static_cast<uint8_t>((s + 1) % kNoteCondCount) : static_cast<uint8_t>((s + kNoteCondCount - 1) % kNoteCondCount);
    return noteCondAtStep(n);
}

// Field text, right-aligned in 10 characters (the device font has no lowercase).
inline void noteCondName(uint8_t code_, char* out_, uint8_t size_) {
    const NoteCond c = noteCondDecode(code_);
    char t[8] = "";
    switch (c.kind) {
        case COND_NONE: strcpy(t, "ALWAYS"); break;
        case COND_RATIO: t[0] = static_cast<char>('0' + c.a); t[1] = ':'; t[2] = static_cast<char>('0' + c.b); t[3] = 0; break;
        case COND_NOT_RATIO: t[0] = '!'; t[1] = static_cast<char>('0' + c.a); t[2] = ':'; t[3] = static_cast<char>('0' + c.b); t[4] = 0; break;
        case COND_FIRST: strcpy(t, "1ST"); break;
        case COND_NOT_FIRST: strcpy(t, "!1ST"); break;
        case COND_PRE: strcpy(t, "PRE"); break;
        case COND_NOT_PRE: strcpy(t, "!PRE"); break;
        case COND_FILL: strcpy(t, "FKEY"); break;
        case COND_NOT_FILL: strcpy(t, "!FKEY"); break;
    }
    if (size_ == 0) return;
    const uint8_t width = (size_ - 1 < 10) ? static_cast<uint8_t>(size_ - 1) : 10;
    const uint8_t len = static_cast<uint8_t>(strlen(t));
    uint8_t i = 0;
    for (; i + len < width; i++) out_[i] = ' ';
    for (uint8_t k = 0; (k < len) && (i < width); k++) out_[i++] = t[k];
    out_[i] = 0;
}

// --- the note word -----------------------------------------------------------------------------
inline uint8_t noteChanceStep(uint16_t note_) { return static_cast<uint8_t>(note_ & 0x1F); }
inline uint8_t noteCondCode(uint16_t note_) { return static_cast<uint8_t>((note_ >> 5) & 0x3F); }
inline int8_t noteNudge(uint16_t note_) {
    int8_t n = static_cast<int8_t>((note_ >> 11) & 0x1F);
    if (n & 0x10) n = static_cast<int8_t>(n - 32);
    return n;
}

inline uint16_t noteMake(uint8_t chanceStep_, uint8_t cond_, int8_t nudge_) {
    if (chanceStep_ > kNoteChanceSteps) chanceStep_ = kNoteChanceSteps;
    if (cond_ >= kNoteCondCount) cond_ = 0;
    if (nudge_ > kNoteNudgeMax) nudge_ = kNoteNudgeMax;
    if (nudge_ < -kNoteNudgeMax) nudge_ = -kNoteNudgeMax;
    return static_cast<uint16_t>((chanceStep_ & 0x1F) | ((cond_ & 0x3F) << 5) | ((static_cast<uint16_t>(nudge_) & 0x1F) << 11));
}

// A note word read from a file: out-of-range fields fall back to their defaults.
inline uint16_t noteSanitize(uint16_t note_) {
    const uint8_t chance = noteChanceStep(note_);
    const uint8_t cond = noteCondCode(note_);
    const int8_t nudge = noteNudge(note_);
    return noteMake((chance <= kNoteChanceSteps) ? chance : 0, (cond < kNoteCondCount) ? cond : 0,
                    ((nudge >= -kNoteNudgeMax) && (nudge <= kNoteNudgeMax)) ? nudge : 0);
}

inline uint8_t noteChancePercent(uint16_t note_) { return static_cast<uint8_t>(100 - 5 * noteChanceStep(note_)); }

inline uint16_t noteWithChancePercent(uint16_t note_, int16_t percent_) {
    if (percent_ < 0) percent_ = 0;
    if (percent_ > 100) percent_ = 100;
    const uint8_t step = static_cast<uint8_t>((100 - percent_ + 2) / 5);
    return noteMake(step, noteCondCode(note_), noteNudge(note_));
}
inline uint16_t noteWithCond(uint16_t note_, uint8_t cond_) { return noteMake(noteChanceStep(note_), cond_, noteNudge(note_)); }
inline uint16_t noteWithNudge(uint16_t note_, int16_t nudge_) {
    if (nudge_ > kNoteNudgeMax) nudge_ = kNoteNudgeMax;
    if (nudge_ < -kNoteNudgeMax) nudge_ = -kNoteNudgeMax;
    return noteMake(noteChanceStep(note_), noteCondCode(note_), static_cast<int8_t>(nudge_));
}

// Nudge as percent of a 16th note (200 ticks), for the display: 1 step = 1%.
inline int8_t noteNudgePercent(uint16_t note_) { return noteNudge(note_); }

// When the beat plays, in sequencer ticks: its start moved by the nudge, inside the loop.
inline uint16_t noteDueInterval(uint16_t start_, uint16_t note_, uint16_t songInterval_) {
    const int32_t due = static_cast<int32_t>(start_) + static_cast<int32_t>(noteNudge(note_)) * kNoteNudgeTicks;
    if (due < 0) return 0;
    if ((songInterval_ > 0) && (due >= static_cast<int32_t>(songInterval_))) return static_cast<uint16_t>(songInterval_ - 1);
    return static_cast<uint16_t>(due);
}

// --- playing a note ---------------------------------------------------------------------------
struct NoteState {
    uint16_t loop = 0;    // loops of this bank since START or a bank change (0 = the first)
    bool fillHeld = false; // FILL key held
    bool pre = true;       // result of the layer's last conditioned beat (per layer)
};

// Whether the condition lets the beat play (before the chance roll).
inline bool noteCondPasses(uint8_t code_, const NoteState& s_) {
    const NoteCond c = noteCondDecode(code_);
    switch (c.kind) {
        case COND_NONE: return true;
        case COND_RATIO: return (s_.loop % c.b) == static_cast<uint16_t>(c.a - 1);
        case COND_NOT_RATIO: return (s_.loop % c.b) != static_cast<uint16_t>(c.a - 1);
        case COND_FIRST: return s_.loop == 0;
        case COND_NOT_FIRST: return s_.loop != 0;
        case COND_PRE: return s_.pre;
        case COND_NOT_PRE: return !s_.pre;
        case COND_FILL: return s_.fillHeld;
        case COND_NOT_FILL: return !s_.fillHeld;
    }
    return true;
}

// Decides whether a beat plays. roll_ is a random number 0..9999 (used only with a chance below
// 100%). Updates s_.pre for conditioned beats, as PRE expects.
inline bool notePlays(uint16_t note_, NoteState& s_, uint16_t roll_) {
    const uint8_t code = noteCondCode(note_);
    const NoteCondKind kind = noteCondDecode(code).kind;
    const uint8_t chance = noteChancePercent(note_);
    const bool condOk = noteCondPasses(code, s_);
    const bool chanceOk = (chance >= 100) || (static_cast<uint32_t>(roll_) < static_cast<uint32_t>(chance) * 100u);
    const bool plays = condOk && chanceOk;
    const bool preKind = (kind == COND_PRE) || (kind == COND_NOT_PRE);
    if (!preKind && ((kind != COND_NONE) || (chance < 100))) s_.pre = plays;
    return plays;
}

// Velocity -> level 0-100 for a plain beat: 0 = the pattern's level (FLAT 100, GHOST 50).
inline uint8_t noteLevel(uint8_t velocity_, uint8_t patternLevel_) {
    if (velocity_ == 0) return patternLevel_;
    if (velocity_ > 127) velocity_ = 127;
    const uint8_t level = static_cast<uint8_t>((velocity_ * 100 + 63) / 127);
    return (level == 0) ? 1 : level;
}

inline bool noteIsDefault(uint8_t velocity_, uint16_t note_) { return (velocity_ == 0) && (note_ == 0); }

// --- file block --------------------------------------------------------------------------------
const uint8_t kNoteEntrySize = 7;
const uint8_t kNoteHeaderSize = 6;

inline void noteWriteHeader(uint8_t* out_, uint16_t count_) {
    memcpy(out_, "BTX", 3);
    out_[3] = 1;
    out_[4] = static_cast<uint8_t>(count_ & 0xFF);
    out_[5] = static_cast<uint8_t>(count_ >> 8);
}

// Returns the entry count, or -1 when there is no valid block.
inline int32_t noteReadHeader(const uint8_t* in_, uint16_t size_) {
    if ((size_ < kNoteHeaderSize) || (memcmp(in_, "BTX", 3) != 0) || (in_[3] != 1)) return -1;
    return static_cast<int32_t>(in_[4] | (in_[5] << 8));
}

struct NoteEntry {
    uint8_t layer, bank;
    uint16_t start;
    uint8_t velocity;
    uint16_t note;
};

inline void noteWriteEntry(uint8_t* out_, const NoteEntry& e_) {
    out_[0] = e_.layer;
    out_[1] = e_.bank;
    out_[2] = static_cast<uint8_t>(e_.start & 0xFF);
    out_[3] = static_cast<uint8_t>(e_.start >> 8);
    out_[4] = e_.velocity;
    out_[5] = static_cast<uint8_t>(e_.note & 0xFF);
    out_[6] = static_cast<uint8_t>(e_.note >> 8);
}

// False for an entry that can't be used (layer/bank out of range); the values are sanitized.
inline bool noteReadEntry(const uint8_t* in_, NoteEntry& e_, uint8_t layers_, uint8_t banks_) {
    e_.layer = in_[0];
    e_.bank = in_[1];
    e_.start = static_cast<uint16_t>(in_[2] | (in_[3] << 8));
    e_.velocity = (in_[4] > 127) ? 127 : in_[4];
    e_.note = noteSanitize(static_cast<uint16_t>(in_[5] | (in_[6] << 8)));
    return (e_.layer < layers_) && (e_.bank < banks_);
}

#endif
