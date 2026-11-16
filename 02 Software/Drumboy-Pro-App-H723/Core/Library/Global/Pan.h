// Panning per layer (V1.15, backlog 36 and 53). Hardware-free so it can be tested on a PC (see
// 02 Software/tests).
//
// Position 0..40 with 20 in the centre (as the mixer's PAN). Constant power with 0 dB at the
// centre: left = sqrt(2) cos a, right = sqrt(2) sin a with a = position / 40 * 90 degrees. A centred
// layer gets exactly 1.0 on both sides, so it is as it is without panning and old songs sound the
// same; the power left^2 + right^2 is 2 everywhere, so a hard-panned layer is as loud as a centred one.
#ifndef __PAN_H
#define __PAN_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>

const uint8_t kPanMax = 40, kPanCentre = 20;

inline void panGains(uint8_t position_, float& left_, float& right_) {
    if (position_ > kPanMax) position_ = kPanMax;
    if (position_ == kPanCentre) {
        left_ = 1.0f;
        right_ = 1.0f;
        return;
    }
    const float a = static_cast<float>(position_) * (1.57079633f / static_cast<float>(kPanMax));
    left_ = 1.41421356f * cosf(a);
    right_ = 1.41421356f * sinf(a);
}

// "C", "L 12" (12 steps to the left) or "R  3" (at most 4 characters).
inline void panLabel(char* out_, size_t size_, uint8_t position_) {
    if (position_ > kPanMax) position_ = kPanMax;
    if (position_ == kPanCentre) {
        snprintf(out_, size_, "C");
    } else if (position_ < kPanCentre) {
        snprintf(out_, size_, "L%2u", static_cast<unsigned>(kPanCentre - position_));
    } else {
        snprintf(out_, size_, "R%2u", static_cast<unsigned>(position_ - kPanCentre));
    }
}

#endif
