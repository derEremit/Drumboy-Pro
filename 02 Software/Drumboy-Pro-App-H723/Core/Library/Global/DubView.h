// The dub desk (DUB page): the lane area shows the ten layers and the two returns as channel strips
// across the whole width, laid out like an old mixing console. Geometry only, hardware-free so it
// can be tested on a PC (see 02 Software/tests).
//
// Strip (76 px, twelve of them from x 24 to 936). From the top: the scribble plate (number and
// name), the bus switch (FILT | DIR), two knobs (ECHO and VERB sends, each with its value), the pan
// bar, the fader with its scale and an LED ladder meter, the level, and the foot plate with the
// channel number and a mute lamp. The returns use the same layout: ECHO (FEEDBACK and DAMP knobs)
// and VERB (DECAY and SIZE knobs), a fader for the WET.
#ifndef __DUBVIEW_H
#define __DUBVIEW_H

#include <math.h>
#include <stdint.h>

const uint16_t kDeskX = 24, kDeskStep = 76, kDeskWidth = 73;       // twelve columns: ten layers, ECHO, VERB
const uint8_t kDeskColumns = 12;

// Rows (y).
const uint16_t kDeskPlateY = 134, kDeskPlateHeight = 20;
const uint16_t kDeskBusY = 158, kDeskBusHeight = 11, kDeskBusCell = 35;
const uint16_t kDeskKnobY = 189, kDeskKnobRadius = 11;             // centre of the knobs
const uint16_t kDeskKnobX[2] = {18, 55};                           // knob centres from the column's x
const uint16_t kDeskKnobLabelY = 203, kDeskKnobValueY = 213;
const uint16_t kDeskPanY = 228, kDeskPanHeight = 7;
const uint16_t kDeskFaderY = 246, kDeskFaderHeight = 128, kDeskCapHeight = 11;
const uint16_t kDeskLevelY = 379;
const uint16_t kDeskFootY = 394, kDeskFootHeight = 38;

// The fader: scale numbers, ticks, rail, cap, meter (x offsets from the column's x).
const uint16_t kDeskScaleX = 2, kDeskTickX = 19, kDeskRailX = 30, kDeskCapX = 20, kDeskCapWidth = 21;
const uint16_t kDeskMeterX = 50, kDeskMeterWidth = 7;
const uint8_t kDeskMeterSegments = 13, kDeskMeterGreen = 9, kDeskMeterYellow = 12;  // segments 0-8 green, 9-11 yellow, 12 red

inline uint16_t deskX(uint8_t column_) { return static_cast<uint16_t>(kDeskX + column_ * kDeskStep); }

// Fader cap top for a level 0..100 (100 at the top).
inline uint16_t deskCapY(uint8_t level_) {
    if (level_ > 100) level_ = 100;
    return static_cast<uint16_t>(kDeskFaderY + ((100u - level_) * (kDeskFaderHeight - kDeskCapHeight)) / 100u);
}

// The knob pointer's end for 0..100 %: a sweep of 270 degrees, from lower left (0) over the top (50)
// to lower right (100).
inline void deskKnobPoint(uint16_t cx_, uint16_t cy_, uint16_t radius_, uint8_t percent_, int16_t& x_, int16_t& y_) {
    if (percent_ > 100) percent_ = 100;
    const float theta = (-135.0f + 270.0f * static_cast<float>(percent_) / 100.0f) * 0.0174532925f;  // clockwise from straight up
    x_ = static_cast<int16_t>(cx_ + lroundf(static_cast<float>(radius_) * sinf(theta)));
    y_ = static_cast<int16_t>(cy_ - lroundf(static_cast<float>(radius_) * cosf(theta)));
}

// x of the pan marker on a bar of width_ px for a pan 0..40 (20 in the centre).
inline uint16_t deskPanX(uint8_t pan_, uint16_t width_) {
    if (pan_ > 40) pan_ = 40;
    return static_cast<uint16_t>((static_cast<uint32_t>(pan_) * (width_ - 1)) / 40u);
}

// Lit segments of the LED ladder from the lane meter's pixels (0..full_).
inline uint8_t deskMeterLit(uint16_t pixels_, uint16_t full_) {
    if ((full_ == 0) || (pixels_ == 0)) return 0;
    if (pixels_ > full_) pixels_ = full_;
    const uint16_t n = static_cast<uint16_t>((static_cast<uint32_t>(pixels_) * kDeskMeterSegments + full_ - 1) / full_);
    return static_cast<uint8_t>((n > kDeskMeterSegments) ? kDeskMeterSegments : n);
}

// y of segment k (0 = bottom) and its height: 13 segments with a 2 px gap fill the fader's height.
inline uint16_t deskSegmentY(uint8_t k_) {
    const uint16_t pitch = kDeskFaderHeight / kDeskMeterSegments;  // 9
    return static_cast<uint16_t>(kDeskFaderY + kDeskFaderHeight - (k_ + 1) * pitch);
}
inline uint16_t deskSegmentHeight() { return static_cast<uint16_t>(kDeskFaderHeight / kDeskMeterSegments - 2); }

#endif
