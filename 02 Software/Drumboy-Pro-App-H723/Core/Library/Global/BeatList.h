// Beat list editing for one bank of a layer: rebuild, move, rotate and insert beats while
// keeping each beat's fill. Kept free of hardware headers so it can be compiled and tested
// on a PC (see 02 Software/tests).
//
// A bank's active beats sit at the front of beatLibrary, sorted by start interval, with
// unique starts. Each beat ends where the next one starts; the last one ends at the song
// end. A beat with a complex fill owns one beatFillLibrary slot (fillTarget) whose micro
// hits lie between its start and end, so whenever a beat's length changes its fill is
// recomputed from its stored setting (type, pattern, time, level) instead of being cleared.
//
// Callers own concurrency: the sequencer reads the bank from an interrupt, so the firmware
// runs these functions with interrupts held off and then recomputes the play position.
#ifndef __BEATLIST_H
#define __BEATLIST_H

#include <stdint.h>
#include <stdio.h>

#include "Fill.h"

const uint8_t kFillLevelRandom = 4;  // kFillLevelDataLibrary: RANDOM

// Index of the active beat starting at start_, or -1.
inline int16_t beatListFind(const Bank& bank_, uint16_t start_) {
    for (int16_t i = 0; i <= bank_.lastActiveBeatNum; i++) {
        if (bank_.beatLibrary[i].getActive() && (bank_.beatLibrary[i].getStartInterval() == start_)) return i;
    }
    return -1;
}

// Restores the bank's invariants after starts were changed or beats were added: compacts the
// active beats, sorts them by start, sets end intervals and recomputes every fill (the last
// beat's too). Beats that share a start are merged, keeping the first one found.
inline void beatListRebuild(Bank& bank_, uint16_t songInterval_) {
    // 1. Compact: active beats to the front, in their current order.
    uint8_t count = 0;
    for (uint8_t i = 0; i < kBeatLibrarySize; i++) {
        if (bank_.beatLibrary[i].getActive()) {
            if (i != count) bank_.beatLibrary[count] = bank_.beatLibrary[i];
            count++;
        }
    }
    for (uint8_t i = count; i < kBeatLibrarySize; i++) bank_.beatLibrary[i].reset();

    // 2. Stable insertion sort by start interval.
    for (uint8_t i = 1; i < count; i++) {
        Beat beat = bank_.beatLibrary[i];
        int16_t j = i - 1;
        while ((j >= 0) && (bank_.beatLibrary[j].getStartInterval() > beat.getStartInterval())) {
            bank_.beatLibrary[j + 1] = bank_.beatLibrary[j];
            j--;
        }
        bank_.beatLibrary[j + 1] = beat;
    }

    // 3. Merge duplicates: drop later beats with the same start (and free their fill slot).
    uint8_t unique = 0;
    for (uint8_t i = 0; i < count; i++) {
        if ((unique > 0) && (bank_.beatLibrary[i].getStartInterval() == bank_.beatLibrary[unique - 1].getStartInterval())) {
            uint8_t target = bank_.beatLibrary[i].getFillTarget();
            if (target != NULL_FILL_TARGET) bank_.beatFillLibrary[target].reset();
            continue;
        }
        if (i != unique) bank_.beatLibrary[unique] = bank_.beatLibrary[i];
        unique++;
    }
    for (uint8_t i = unique; i < count; i++) bank_.beatLibrary[i].reset();
    bank_.lastActiveBeatNum = static_cast<int8_t>(unique) - 1;

    // 4. End intervals and fills. The RANDOM level setting draws new levels on every
    //    recompute; keep the ones already drawn so a move or rotation only changes timing
    //    (and rotating back gives the identical bank).
    for (uint8_t i = 0; i < unique; i++) {
        Beat& beat = bank_.beatLibrary[i];
        beat.setEndInterval((i + 1 < unique) ? bank_.beatLibrary[i + 1].getStartInterval() : songInterval_);
        const uint8_t oldTarget = beat.getFillTarget();
        uint8_t levels[kBeatMicroLibrarySize];
        uint8_t oldSteps = 0;
        if ((oldTarget != NULL_FILL_TARGET) && (beat.getFillLevel() == kFillLevelRandom) && bank_.beatFillLibrary[oldTarget].getActive()) {
            oldSteps = bank_.beatFillLibrary[oldTarget].getStepSize();
            for (uint8_t k = 0; k < kBeatMicroLibrarySize; k++) levels[k] = bank_.beatFillLibrary[oldTarget].beatMicroLibrary[k].getLevel();
        }
        const uint8_t target = bank_.setBeatFill(i, beat.getFillType(), beat.getFillPattern(), beat.getFillTime(), beat.getFillLevel());
        if (oldSteps && (target == oldTarget) && (bank_.beatFillLibrary[target].getStepSize() == oldSteps)) {
            for (uint8_t k = 0; k < oldSteps && k < kBeatMicroLibrarySize; k++) bank_.beatFillLibrary[target].beatMicroLibrary[k].setLevel(levels[k]);
        }
    }
}

// Removes every beat and fill.
inline void beatListClear(Bank& bank_) {
    for (uint8_t i = 0; i < kBeatLibrarySize; i++) bank_.beatLibrary[i].reset();
    for (uint8_t i = 0; i < kBeatFillLibrarySize; i++) bank_.beatFillLibrary[i].reset();
    bank_.lastActiveBeatNum = -1;
    bank_.playBeatNum = -1;
    bank_.playBeatMicroNum = -1;
}

// Adds a plain beat at start_ without rebuilding (call beatListRebuild afterwards; several
// beats can be added first). Returns false when start_ is outside the song, already taken,
// or the bank is full.
inline bool beatListAdd(Bank& bank_, uint16_t start_, uint16_t songInterval_) {
    if (start_ >= songInterval_) return false;
    int16_t freeSlot = -1;
    for (uint8_t i = 0; i < kBeatLibrarySize; i++) {
        const Beat& beat = bank_.beatLibrary[i];
        if (beat.getActive()) {
            if (beat.getStartInterval() == start_) return false;
        } else if (freeSlot == -1) {
            freeSlot = i;
        }
    }
    if (freeSlot == -1) return false;
    Beat& beat = bank_.beatLibrary[freeSlot];
    beat.reset();
    beat.setActive(true);
    beat.setStartInterval(start_);
    beat.setEndInterval(start_);
    if (freeSlot > bank_.lastActiveBeatNum) bank_.lastActiveBeatNum = static_cast<int8_t>(freeSlot);
    return true;
}

// Next grid position after (forward_) or before an interval, wrapping around the song.
// An interval off the grid moves to the nearest grid line in that direction.
// The grid starts at 0; when the song length isn't a multiple of the step, the last cell is
// shorter and the grid restarts at 0 after it.
inline uint16_t beatListStep(uint16_t interval_, bool forward_, uint16_t step_, uint16_t songInterval_) {
    if (forward_) {
        const uint32_t target = (static_cast<uint32_t>(interval_) / step_ + 1) * step_;
        return (target < songInterval_) ? static_cast<uint16_t>(target) : 0;
    }
    if (interval_ == 0) return static_cast<uint16_t>(((songInterval_ - 1) / step_) * step_);
    return (interval_ % step_) ? static_cast<uint16_t>((interval_ / step_) * step_) : static_cast<uint16_t>(interval_ - step_);
}

// Moves beat beatNum_ one grid step forward or back, jumping over positions taken by other
// beats and wrapping around the song end. Its fill setting is kept. Returns the beat's new
// index, or -1 if there is nowhere to go.
inline int16_t beatListMove(Bank& bank_, uint8_t beatNum_, bool forward_, uint16_t step_, uint16_t songInterval_) {
    if ((step_ == 0) || (static_cast<int16_t>(beatNum_) > bank_.lastActiveBeatNum)) return -1;
    Beat& beat = bank_.beatLibrary[beatNum_];
    const uint16_t start = beat.getStartInterval();
    uint16_t target = start;
    for (uint32_t tries = 0; tries <= static_cast<uint32_t>(songInterval_ / step_) + 1; tries++) {
        target = beatListStep(target, forward_, step_, songInterval_);
        if (target == start) return -1;
        if (beatListFind(bank_, target) == -1) {
            beat.setStartInterval(target);
            beatListRebuild(bank_, songInterval_);
            return beatListFind(bank_, target);
        }
    }
    return -1;
}

// Shifts every beat by delta_ (positive = later), wrapping around the song end. Fills follow
// their beats. Rotating by -delta_ afterwards restores the bank exactly.
inline void beatListRotate(Bank& bank_, int32_t delta_, uint16_t songInterval_) {
    if (bank_.lastActiveBeatNum < 0) return;
    int32_t shift = delta_ % songInterval_;
    if (shift < 0) shift += songInterval_;
    for (int16_t i = 0; i <= bank_.lastActiveBeatNum; i++) {
        Beat& beat = bank_.beatLibrary[i];
        beat.setStartInterval(static_cast<uint16_t>((beat.getStartInterval() + shift) % songInterval_));
    }
    beatListRebuild(bank_, songInterval_);
}

// Inserts a plain beat on the grid in the middle of the gap after beat beatNum_ (the gap after
// the last beat runs to the first beat of the next loop). In an empty bank the beat goes at the
// song start. Returns the new beat's index, or -1 if the bank is full or the gap has no free
// grid position.
inline int16_t beatListInsertAfter(Bank& bank_, int16_t beatNum_, uint16_t step_, uint16_t songInterval_) {
    if ((step_ == 0) || (bank_.lastActiveBeatNum >= static_cast<int16_t>(kBeatLibrarySize) - 1)) return -1;
    uint16_t target = 0;
    if (bank_.lastActiveBeatNum >= 0) {
        if ((beatNum_ < 0) || (beatNum_ > bank_.lastActiveBeatNum)) return -1;
        const uint32_t start = bank_.beatLibrary[beatNum_].getStartInterval();
        const uint32_t end = (beatNum_ < bank_.lastActiveBeatNum) ? bank_.beatLibrary[beatNum_ + 1].getStartInterval()
                                                                  : bank_.beatLibrary[0].getStartInterval() + static_cast<uint32_t>(songInterval_);
        uint32_t mid = ((start + (end - start) / 2) / step_) * step_;
        if (mid <= start) mid = (start / step_ + 1) * step_;
        if (mid >= end) return -1;
        target = static_cast<uint16_t>(mid % songInterval_);
    }
    if (!beatListAdd(bank_, target, songInterval_)) return -1;
    beatListRebuild(bank_, songInterval_);
    return beatListFind(bank_, target);
}

// Writes an interval as bar.beat.16th, 1-based (e.g. "2.3.4"). Positions between 16ths add
// the remaining 64ths ("2.3.4+1"). quarter_ is the interval of one beat (kMeasureInterval).
inline void beatListPosition(uint16_t interval_, uint16_t barInterval_, uint16_t quarter_, char* out_, size_t size_) {
    const uint16_t sixteenth = quarter_ / 4;
    const uint16_t sixtyFourth = quarter_ / 16;
    const unsigned bar = interval_ / barInterval_ + 1;
    const unsigned beat = (interval_ % barInterval_) / quarter_ + 1;
    const unsigned step = (interval_ % quarter_) / sixteenth + 1;
    const unsigned rest = (interval_ % sixteenth) / sixtyFourth;
    if (rest) {
        snprintf(out_, size_, "%u.%u.%u+%u", bar, beat, step, rest);
    } else {
        snprintf(out_, size_, "%u.%u.%u", bar, beat, step);
    }
}

// Writes a signed distance as bars.beats.16ths, 0-based ("+0.1.2" = one beat and two 16ths
// later, "0.0.0" = none). Distances between 16ths add the remaining 64ths ("-0.0.1+2").
inline void beatListOffset(int32_t delta_, uint16_t barInterval_, uint16_t quarter_, char* out_, size_t size_) {
    const uint32_t d = static_cast<uint32_t>((delta_ < 0) ? -delta_ : delta_);
    const uint16_t sixteenth = quarter_ / 4;
    const uint16_t sixtyFourth = quarter_ / 16;
    const char* sign = (delta_ > 0) ? "+" : ((delta_ < 0) ? "-" : "");
    const unsigned bars = d / barInterval_;
    const unsigned beats = (d % barInterval_) / quarter_;
    const unsigned steps = (d % quarter_) / sixteenth;
    const unsigned rest = (d % sixteenth) / sixtyFourth;
    if (rest) {
        snprintf(out_, size_, "%s%u.%u.%u+%u", sign, bars, beats, steps, rest);
    } else {
        snprintf(out_, size_, "%s%u.%u.%u", sign, bars, beats, steps);
    }
}

// Wraps a shift around the song into the range -song/2 < shift <= song/2, so the shown
// distance is the short way round.
inline int32_t beatListWrapShift(int32_t shift_, uint16_t songInterval_) {
    int32_t s = shift_ % songInterval_;
    if (s < 0) s += songInterval_;
    if (s > songInterval_ / 2) s -= songInterval_;
    return s;
}

#endif
