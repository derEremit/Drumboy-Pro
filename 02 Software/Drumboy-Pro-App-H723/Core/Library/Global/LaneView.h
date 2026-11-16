// What a beat looks like in a lane, and where a beat sits on a ring of the EUCLID view
// (backlog 38 and 45). Hardware-free so it can be tested on a PC (see 02 Software/tests).
//
// Lanes: a plain beat is a mark 2 px wide in the 9 px song row. Its height shows its velocity
// (FLAT 9 px, GHOST 6 px, velocity 1 = 2 px), bottom-aligned; a beat with a chance below 100% is
// drawn hollow, 3 px wide; a beat with a condition gets a 2x2 dot above the row. A beat without
// note settings looks exactly as before V1.14.
//
// Rings: the EUCLID page shows the ten layers as concentric rings, layer 1 outside. One turn is
// one loop, starting at the top and going clockwise.
#ifndef __LANEVIEW_H
#define __LANEVIEW_H

#include <math.h>
#include <stdint.h>

#include "BeatNote.h"
#include "Routing.h"
#include "SongChain.h"

const uint8_t kLaneMarkMaxHeight = 9;   // kSongHeight
const uint8_t kLaneMarkMinHeight = 2;
const uint8_t kLaneDotRise = 4;         // the condition dot sits 4 px above the song row, 2 px high

// Mark height for a plain beat: velocity_ 0 = from its pattern (patternLevel_ 100 FLAT, 50 GHOST).
inline uint8_t laneMarkHeight(uint8_t velocity_, uint8_t patternLevel_) {
    const uint8_t level = noteLevel(velocity_, patternLevel_);  // 0-100
    const uint8_t span = kLaneMarkMaxHeight - kLaneMarkMinHeight;
    return static_cast<uint8_t>(kLaneMarkMinHeight + (level * span + 50) / 100);
}

inline bool laneMarkHollow(uint16_t note_) { return noteChancePercent(note_) < 100; }
inline bool laneMarkDot(uint16_t note_) { return noteCondCode(note_) != 0; }

// Rings.
const int16_t kRingCenterX = 200, kRingCenterY = 286;
const uint16_t kRingOuter = 140, kRingStep = 12;   // layer 1: r 140 ... layer 10: r 32
const uint16_t kRingPlayRadius = 150;              // the playhead marker orbits outside the rings

inline uint16_t ringRadius(uint8_t layer_) { return static_cast<uint16_t>(kRingOuter - layer_ * kRingStep); }

struct RingPoint {
    int16_t x, y;
};

// Where interval_ (0 .. songInterval_) lies on a ring of radius r_: the loop start is at the top,
// time runs clockwise.
inline RingPoint ringPoint(uint32_t interval_, uint32_t songInterval_, uint16_t r_) {
    const float turn = (songInterval_ > 0) ? static_cast<float>(interval_ % songInterval_) / static_cast<float>(songInterval_) : 0.0f;
    const float a = turn * 6.2831853f;
    return RingPoint{static_cast<int16_t>(kRingCenterX + lroundf(static_cast<float>(r_) * sinf(a))),
                     static_cast<int16_t>(kRingCenterY - lroundf(static_cast<float>(r_) * cosf(a)))};
}

// Dot radius of a beat on a ring: larger on the selected layer, and for louder beats there.
inline uint8_t ringDotRadius(bool selectedLayer_, uint8_t velocity_, uint8_t patternLevel_) {
    if (!selectedLayer_) return 2;
    return (noteLevel(velocity_, patternLevel_) >= 75) ? 4 : 3;
}

// Song timeline (SONG page): the chain as boxes across the lane area, each as wide as its repeats,
// then a status line, then the five banks as thumbnails of their ten lanes.
const uint16_t kSongViewX = 40, kSongViewWidth = 872, kSongViewY = 146, kSongViewHeight = 52;
const uint16_t kSongViewMaxUnit = 73, kSongViewMinUnit = 3;       // px per loop: 16 steps x 16 repeats still fit
const uint16_t kSongViewBarY = kSongViewY + 44, kSongViewBarHeight = 4;  // the playing loop's progress bar, inside its box
const uint16_t kSongViewLoopY = 209, kSongViewTextY = 220;
const uint16_t kSongThumbX = 40, kSongThumbStep = 178, kSongThumbWidth = 166, kSongThumbLabelY = 250, kSongThumbY = 264, kSongThumbHeight = 164;
const uint16_t kSongThumbRowY = 270, kSongThumbRowStep = 15, kSongThumbRowHeight = 8, kSongThumbInset = 4, kSongThumbInnerWidth = 158;

// Pixels per loop: the whole chain fits the width, a short chain does not stretch beyond 73.
inline uint16_t songViewUnit(const SongChain& c_) {
    const uint16_t total = chainTotalLoops(c_);
    if (total == 0) return kSongViewMaxUnit;
    const uint16_t unit = static_cast<uint16_t>(kSongViewWidth / total);
    return (unit > kSongViewMaxUnit) ? kSongViewMaxUnit : ((unit < kSongViewMinUnit) ? kSongViewMinUnit : unit);
}

// Left edge of a step's box (step_ == count: where the chain ends).
inline uint16_t songViewStepX(const SongChain& c_, uint8_t step_, uint16_t unit_) {
    uint16_t loops = 0;
    for (uint8_t i = 0; (i < step_) && (i < c_.count); i++) loops += c_.steps[i].repeats;
    return static_cast<uint16_t>(kSongViewX + loops * unit_);
}

inline uint16_t songViewBoxWidth(uint8_t repeats_, uint16_t unit_) {
    const uint16_t w = static_cast<uint16_t>(repeats_ * unit_ - ((unit_ >= 12) ? 4 : 1));
    return (w < 2) ? 2 : w;
}

// The progress bar of one loop inside its step's box: offset from the loop's left edge, and width.
inline uint16_t songViewCellInset(uint16_t unit_) { return (unit_ >= 12) ? 3 : 1; }
inline uint16_t songViewCellWidth(uint16_t unit_) { return (unit_ >= 12) ? static_cast<uint16_t>(unit_ - 10) : ((unit_ > 3) ? static_cast<uint16_t>(unit_ - 3) : 1); }

// Filled pixels of that bar at interval_ of a loop songInterval_ long.
inline uint16_t songViewProgress(uint32_t interval_, uint32_t songInterval_, uint16_t unit_) {
    if (songInterval_ == 0) return 0;
    if (interval_ >= songInterval_) interval_ = songInterval_ - 1;
    return static_cast<uint16_t>(1 + (interval_ * songViewCellWidth(unit_)) / songInterval_);
}

// Banks the chain uses, bit n = bank n.
inline uint8_t chainUsedBanks(const SongChain& c_) {
    uint8_t mask = 0;
    for (uint8_t i = 0; i < c_.count; i++) mask |= static_cast<uint8_t>(1u << c_.steps[i].bank);
    return mask;
}

// What follows the step playing now: after `loops` more loop ends (counting the one playing), the
// chain goes to `bank` (-1: the song ends there).
struct ChainNext {
    int8_t bank;
    uint8_t loops;
};

inline ChainNext chainNext(const SongChain& c_, const ChainPlay& p_) {
    const uint8_t step = (p_.step < c_.count) ? p_.step : 0;
    const uint8_t repeats = c_.steps[step].repeats;
    ChainNext n;
    n.loops = static_cast<uint8_t>((p_.loopsDone < repeats) ? (repeats - p_.loopsDone) : 1);
    if (step + 1 < c_.count) {
        n.bank = static_cast<int8_t>(c_.steps[step + 1].bank);
    } else {
        n.bank = c_.loop ? static_cast<int8_t>(c_.steps[0].bank) : -1;
    }
    return n;
}

inline uint16_t songThumbX(uint8_t bank_) { return static_cast<uint16_t>(kSongThumbX + bank_ * kSongThumbStep); }

// Where interval_ lies inside a bank's thumbnail.
inline uint16_t songThumbBeatX(uint8_t bank_, uint32_t interval_, uint32_t songInterval_) {
    if (songInterval_ == 0) return static_cast<uint16_t>(songThumbX(bank_) + kSongThumbInset);
    if (interval_ >= songInterval_) interval_ = songInterval_ - 1;
    return static_cast<uint16_t>(songThumbX(bank_) + kSongThumbInset + (interval_ * kSongThumbInnerWidth) / songInterval_);
}

// Routing view (ROUTING page): the six modules and the master as boxes along the top, joined as
// patched; below, one row per source (ten layers, metronome, line-in) with a mark under the module
// it enters. A route to the next module is a line between the boxes; a longer one goes over them,
// each module at its own height and offset so the lines stay apart.
const uint16_t kRouteViewModuleX = 300, kRouteViewModuleStep = 96, kRouteViewBoxWidth = 78, kRouteViewBoxHeight = 22, kRouteViewBoxY = 158;
const uint16_t kRouteViewRowY = 200, kRouteViewRowStep = 19, kRouteViewMark = 11;
const uint16_t kRouteViewNameX = 40, kRouteViewGuideX = 196;
const uint8_t kRouteViewSources = 12;  // layers 1-10, metronome, line-in

inline uint16_t routeViewModuleX(uint8_t node_) { return static_cast<uint16_t>(kRouteViewModuleX + node_ * kRouteViewModuleStep); }  // box centre
inline uint16_t routeViewArcY(uint8_t module_) { return static_cast<uint16_t>(kRouteViewBoxY - 6 - 3 * module_); }
inline uint16_t routeViewArcOffset(uint8_t module_) { return static_cast<uint16_t>(6 + 5 * module_); }
inline uint16_t routeViewRowY(uint8_t source_) { return static_cast<uint16_t>(kRouteViewRowY + source_ * kRouteViewRowStep); }

#endif
