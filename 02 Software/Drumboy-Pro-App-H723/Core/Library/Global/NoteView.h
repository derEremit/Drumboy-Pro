// The step edit screen (NOTE page): the lane area shows one layer's beats across the full width,
// with a velocity lane and a chance lane under them, so a whole bar can be read and edited at once.
// Geometry only, hardware-free so it can be tested on a PC (see 02 Software/tests).
//
// Rows from the top: title, ruler (a tick per beat, the bar number at each bar), the beats as blocks
// (a fill is as long as it lasts), condition dots, the VELOCITY lane (bars up from a baseline) and
// the CHANCE lane. The selected beat is magenta.
#ifndef __NOTEVIEW_H
#define __NOTEVIEW_H

#include <stdint.h>

const uint16_t kNoteViewX = 78, kNoteViewWidth = 852;
const uint16_t kNoteViewTitleY = 137, kNoteViewRulerY = 156, kNoteViewTickY = 166, kNoteViewTickHeight = 4;
const uint16_t kNoteViewBlockY = 174, kNoteViewBlockHeight = 20, kNoteViewDotY = 198;
const uint16_t kNoteViewVelBase = 322, kNoteViewVelHeight = 100;      // bars rise from the baseline
const uint16_t kNoteViewChanceBase = 425, kNoteViewChanceHeight = 60;
const uint16_t kNoteViewBlockWidth = 8, kNoteViewBarWidth = 6;         // plain beats

// x of an interval (0 .. songInterval_) on the plot.
inline uint16_t noteViewX(uint32_t interval_, uint32_t songInterval_) {
    if (songInterval_ == 0) return kNoteViewX;
    if (interval_ >= songInterval_) interval_ = songInterval_ - 1;
    return static_cast<uint16_t>(kNoteViewX + (interval_ * (kNoteViewWidth - 1)) / songInterval_);
}

// Width of a beat's block: a plain beat has a fixed width, a fill lasts from start to end.
inline uint16_t noteViewBlockWidth(bool fill_, uint32_t start_, uint32_t end_, uint32_t songInterval_) {
    if (!fill_ || (songInterval_ == 0) || (end_ <= start_)) return kNoteViewBlockWidth;
    const uint32_t w = noteViewX(end_, songInterval_) - noteViewX(start_, songInterval_);
    return static_cast<uint16_t>((w < kNoteViewBlockWidth) ? kNoteViewBlockWidth : w);
}

// The x range a beat occupies in all rows (for clearing it): from its left edge to its block's end.
inline void noteViewSpan(uint16_t x_, uint16_t blockWidth_, uint16_t& from_, uint16_t& to_) {
    const uint16_t left = static_cast<uint16_t>(x_ - 2);  // the bars are centred on the block
    from_ = (left < kNoteViewX - 4) ? static_cast<uint16_t>(kNoteViewX - 4) : left;
    const uint16_t right = static_cast<uint16_t>(x_ + blockWidth_ + 1);
    to_ = (right > kNoteViewX + kNoteViewWidth + 3) ? static_cast<uint16_t>(kNoteViewX + kNoteViewWidth + 3) : right;
}

// Height of the velocity bar (1..127 -> 1..100 px) and of the chance bar (0..100 % -> 0..60 px).
inline uint16_t noteViewVelHeight(uint8_t velocity_) {
    if (velocity_ > 127) velocity_ = 127;
    if (velocity_ == 0) return 0;
    const uint16_t h = static_cast<uint16_t>((static_cast<uint32_t>(velocity_) * kNoteViewVelHeight + 63) / 127u);
    return (h == 0) ? 1 : h;
}

inline uint16_t noteViewChanceHeight(uint8_t percent_) {
    if (percent_ > 100) percent_ = 100;
    return static_cast<uint16_t>((static_cast<uint32_t>(percent_) * kNoteViewChanceHeight + 50) / 100u);
}

// The ruler: a tick at every measure (beat), longer at every bar, the bar number when there is room.
// Calls place_(x, isBar, barNumber) for each tick. Returns the number of ticks.
template <typename Place>
inline uint16_t noteViewRuler(uint32_t songInterval_, uint32_t barInterval_, uint32_t measureInterval_, Place place_) {
    if ((songInterval_ == 0) || (measureInterval_ == 0)) return 0;
    uint16_t n = 0;
    for (uint32_t at = 0; at < songInterval_; at += measureInterval_) {
        const bool bar = (barInterval_ > 0) && ((at % barInterval_) == 0);
        place_(noteViewX(at, songInterval_), bar, static_cast<uint16_t>(barInterval_ > 0 ? at / barInterval_ + 1 : 1));
        n++;
    }
    return n;
}

#endif
