// Sample view (instrument page 2, START and END): the layer's sample drawn at full width over the
// lane area, with the START and END points as handles and the unused part dimmed. Hardware-free so
// it can be tested on a PC (see 02 Software/tests).
//
// The waveform is 840 columns, one per pixel; each column holds the peak of its stretch of the
// sample (0..100 = half the plot height), computed once when the sample changes.
#ifndef __SAMPLEVIEW_H
#define __SAMPLEVIEW_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>

const uint16_t kSampleCols = 840, kSampleX = 60;                         // x of column 0
const uint16_t kSampleCenterY = 280, kSampleHalf = 100;                  // the plot: y 180..380
const uint16_t kSampleTitleY = 134, kSampleScaleY = 392;                 // title row, tick labels
const uint16_t kSampleHandleTop = 168, kSampleHandleBottom = 392;        // handles reach past the plot
const uint16_t kSamplePeakStride = 64;                                   // at most this many reads per column

// Peaks of the sample: data_[0..size_-1] in -1..1, scaled by norm_ (the playback normalisation).
// out_[c] = 0..kSampleHalf. A column with sound always shows at least 1; silence shows 0.
inline void samplePeakColumns(const float* data_, uint32_t size_, float norm_, uint8_t* out_, uint16_t cols_) {
    for (uint16_t c = 0; c < cols_; c++) out_[c] = 0;
    if ((data_ == nullptr) || (size_ == 0) || (cols_ == 0)) return;
    for (uint16_t c = 0; c < cols_; c++) {
        const uint32_t from = static_cast<uint32_t>((static_cast<uint64_t>(c) * size_) / cols_);
        uint32_t to = static_cast<uint32_t>((static_cast<uint64_t>(c + 1) * size_) / cols_);
        if (to <= from) to = from + 1;
        if (to > size_) to = size_;
        uint32_t step = (to - from) / kSamplePeakStride;
        if (step == 0) step = 1;
        float peak = 0.0f;
        for (uint32_t i = from; i < to; i += step) {
            const float a = fabsf(data_[i]) * norm_;
            if (a > peak) peak = a;
        }
        if (peak > 1.0f) peak = 1.0f;
        uint8_t h = static_cast<uint8_t>(peak * static_cast<float>(kSampleHalf) + 0.5f);
        if ((h == 0) && (peak > 0.0f)) h = 1;
        out_[c] = h;
    }
}

// x of a position 0..100 % (the layer's START / END), and the column range it spans.
inline uint16_t sampleXOfPercent(uint8_t percent_) {
    if (percent_ > 100) percent_ = 100;
    return static_cast<uint16_t>(kSampleX + (static_cast<uint32_t>(percent_) * (kSampleCols - 1)) / 100u);
}

// Is column c_ inside the played part: START <= column < END.
inline bool sampleColumnActive(uint16_t c_, uint8_t start_, uint8_t end_) {
    const uint16_t s = static_cast<uint16_t>(sampleXOfPercent(start_) - kSampleX), e = static_cast<uint16_t>(sampleXOfPercent(end_) - kSampleX);
    return (c_ >= s) && (c_ <= e);
}

// The columns to redraw when START or END moves: from the left-most to the right-most of the four
// handles, widened by the handle width. Returns false when nothing moved.
inline bool sampleDirtyColumns(uint8_t oldStart_, uint8_t oldEnd_, uint8_t newStart_, uint8_t newEnd_, uint16_t& from_, uint16_t& to_) {
    if ((oldStart_ == newStart_) && (oldEnd_ == newEnd_)) return false;
    uint16_t lo = kSampleCols, hi = 0;
    const uint8_t pts[4] = {oldStart_, oldEnd_, newStart_, newEnd_};
    for (uint8_t i = 0; i < 4; i++) {
        const uint16_t c = static_cast<uint16_t>(sampleXOfPercent(pts[i]) - kSampleX);
        if (c < lo) lo = c;
        if (c > hi) hi = c;
    }
    from_ = (lo >= 3) ? static_cast<uint16_t>(lo - 3) : 0;
    to_ = (hi + 3 < kSampleCols) ? static_cast<uint16_t>(hi + 3) : static_cast<uint16_t>(kSampleCols - 1);
    // A move that leaves both ends in place between old and new needs only the changed stretches,
    // but one range keeps the redraw simple; it is at most the whole waveform.
    return true;
}

// Length of the played part in seconds (hundredths): the whole sample, START to END, at a pitch.
inline uint32_t sampleSecondsCenti(float totalSeconds_, uint8_t start_, uint8_t end_, float pitchFactor_) {
    if ((end_ <= start_) || (pitchFactor_ <= 0.0f) || (totalSeconds_ <= 0.0f)) return 0;
    const float s = totalSeconds_ * static_cast<float>(end_ - start_) / 100.0f / pitchFactor_;
    return static_cast<uint32_t>(s * 100.0f + 0.5f);
}

#endif
