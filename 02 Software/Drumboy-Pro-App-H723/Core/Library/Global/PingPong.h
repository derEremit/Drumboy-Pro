// Ping-pong echo: the repeats of the delay alternate between the left and the right channel. It needs
// no second delay line: the delay reads its buffer at two points (one echo time back for the left,
// two for the right) and feeds back from the right one. WIDTH blends from the centred echo of before
// (0) to the full ping-pong (1). Hardware-free so it can be tested on a PC (see 02 Software/tests).
#ifndef __PINGPONG_H
#define __PINGPONG_H

#include <stdint.h>

// Where the second read point is: one echo time (lag_ samples) before the first one, in a circular
// buffer of size_ samples.
inline uint32_t pingPongSecondIndex(uint32_t firstIndex_, uint32_t lag_, uint32_t size_) {
    return (firstIndex_ >= lag_) ? (firstIndex_ - lag_) : (firstIndex_ + size_ - lag_);
}

// The two read points need two echo times in the buffer. Past that the echo stays centred.
inline bool pingPongFits(uint32_t lag_, uint32_t size_) { return (lag_ > 0) && (2u * lag_ < size_); }

// Width as a gain: 0..100 %, and 0 when two echo times do not fit.
inline float pingPongWidth(uint8_t percent_, uint32_t lag_, uint32_t size_) {
    if (!pingPongFits(lag_, size_) || (percent_ == 0)) return 0.0f;
    return static_cast<float>((percent_ > 100) ? 100 : percent_) * 0.01f;
}

// The right channel's echo, which is also what feeds back: the first tap at width 0 (exactly, so the
// centred echo is unchanged); at full width the second tap times the feedback. The feedback loop is then
// two echo times long, and this factor makes every repeat FEEDBACK times the one before it, as in the
// centred echo (without it the level would fall once per two repeats).
inline float pingPongRight(float tap1_, float tap2_, float width_, float feedback_) { return (width_ > 0.0f) ? (tap1_ + width_ * (feedback_ * tap2_ - tap1_)) : tap1_; }

#endif
