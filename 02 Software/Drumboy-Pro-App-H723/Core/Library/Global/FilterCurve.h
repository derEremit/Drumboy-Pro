// Filter curve view (FILTER 1 and FILTER 2 pages): the filter's response drawn over the lane area,
// 20 Hz to 20 kHz on a logarithmic axis, +18 dB to -48 dB. Hardware-free so it can be tested on a
// PC (see 02 Software/tests).
//
// This file also holds the filter design itself (6, 12 and 24 dB per octave), so the audio code and
// the curve use the same coefficients: the curve shows what is heard, including the page's DRY and
// WET added with their phase. DRIVE is left out: it is a gain with a soft clip before the filter,
// not part of its shape. The EQ page uses the same plot for its six bands.
#ifndef __FILTERCURVE_H
#define __FILTERCURVE_H

#include <math.h>
#include <stdint.h>

// The plot: one point every 2 px.
const uint16_t kCurveX = 80, kCurveWidth = 840, kCurveStep = 2, kCurvePoints = 421;
const uint16_t kCurveY = 160, kCurveHeight = 231;
const float kCurveTopDb = 18.0f, kCurveBottomDb = -48.0f;  // 3.5 px per dB
const float kCurveMinHz = 20.0f, kCurveDecades = 3.0f;     // 20 Hz .. 20 kHz
const uint16_t kCurveLegendY = 140, kCurveLabelY = 398;

inline uint16_t curvePointX(uint16_t point_) { return static_cast<uint16_t>(kCurveX + point_ * kCurveStep); }

// Frequency of a point, and the x of a frequency (clamped to the plot).
inline float curvePointHz(uint16_t point_) {
    return kCurveMinHz * powf(10.0f, kCurveDecades * static_cast<float>(point_) / static_cast<float>(kCurvePoints - 1));
}

inline uint16_t curveXOfHz(float hz_) {
    if (hz_ < kCurveMinHz) hz_ = kCurveMinHz;
    float turn = log10f(hz_ / kCurveMinHz) / kCurveDecades;
    if (turn > 1.0f) turn = 1.0f;
    return static_cast<uint16_t>(kCurveX + lroundf(turn * static_cast<float>(kCurveWidth)));
}

// The y of a level, clamped to the plot.
inline uint16_t curveYOfDb(float db_) {
    if (db_ > kCurveTopDb) db_ = kCurveTopDb;
    if (db_ < kCurveBottomDb) db_ = kCurveBottomDb;
    const float perDb = static_cast<float>(kCurveHeight) / (kCurveTopDb - kCurveBottomDb);
    return static_cast<uint16_t>(kCurveY + lroundf((kCurveTopDb - db_) * perDb));
}

// The y of a level on another scale (the EQ view uses +24 to -36 dB).
inline uint16_t curveYOfDbRange(float db_, float topDb_, float bottomDb_) {
    if (db_ > topDb_) db_ = topDb_;
    if (db_ < bottomDb_) db_ = bottomDb_;
    return static_cast<uint16_t>(kCurveY + lroundf((topDb_ - db_) * static_cast<float>(kCurveHeight) / (topDb_ - bottomDb_)));
}

// One second-order section as the audio code runs it (a0 = 1).
struct BiquadCoef {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
};

// Filter design (FILTER 1 and 2). type_: 1 low-pass, 2 high-pass, 3 band-pass, 4 band-stop.
// slope_: 0 = 6 dB, 1 = 12 dB, 2 = 24 dB per octave.
//   6 dB   low/high-pass: a first-order filter (RESONANCE has no effect); band types: one section
//          at half the Q, so a wide band.
//   12 dB  one section with the RESONANCE's Q, as before V1.14.
//   24 dB  two sections. Low/high-pass: the first at Q 0.54 and the second at 1.85 x Q, which is a
//          Butterworth response at the lowest RESONANCE (Q 0.70) and a single peak of about Q
//          above it. Band types: the same section twice.
// Returns the number of sections (1 or 2); an unknown type passes the signal through.
const float kFilterStage1Q = 0.5412f, kFilterStage2QScale = 1.848f;

inline void filterDesignSection(uint8_t type_, float hz_, float q_, float rate_, BiquadCoef& c_) {
    const float w = 6.2831853f * hz_ / rate_, cs = cosf(w), alpha = sinf(w) / (2.0f * q_);
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    const float a0 = 1.0f + alpha, a1 = -2.0f * cs, a2 = 1.0f - alpha;
    switch (type_) {
        case 1:
            b0 = (1.0f - cs) * 0.5f, b1 = 1.0f - cs, b2 = (1.0f - cs) * 0.5f;
            break;
        case 2:
            b0 = (1.0f + cs) * 0.5f, b1 = -(1.0f + cs), b2 = (1.0f + cs) * 0.5f;
            break;
        case 3:
            b0 = alpha, b1 = 0.0f, b2 = -alpha;
            break;
        case 4:
            b0 = 1.0f, b1 = -2.0f * cs, b2 = 1.0f;
            break;
        default:
            c_ = BiquadCoef();
            return;
    }
    const float inv = 1.0f / a0;
    c_.b0 = b0 * inv, c_.b1 = b1 * inv, c_.b2 = b2 * inv, c_.a1 = a1 * inv, c_.a2 = a2 * inv;
}

inline uint8_t filterDesign(uint8_t type_, uint8_t slope_, float hz_, float q_, float rate_, BiquadCoef& s1_, BiquadCoef& s2_) {
    s1_ = BiquadCoef();
    s2_ = BiquadCoef();
    if ((type_ < 1) || (type_ > 4)) return 1;
    if (hz_ > 0.45f * rate_) hz_ = 0.45f * rate_;
    const bool band = (type_ >= 3);
    if (slope_ == 0) {
        if (band) {
            filterDesignSection(type_, hz_, q_ * 0.5f, rate_, s1_);
        } else {
            // First order: K = tan(w/2); low-pass (K + K z^-1)/((1+K) + (K-1) z^-1), high-pass (1 - z^-1)/(same).
            const float k = tanf(3.14159265f * hz_ / rate_), inv = 1.0f / (1.0f + k);
            s1_.a1 = (k - 1.0f) * inv;
            if (type_ == 1) {
                s1_.b0 = k * inv, s1_.b1 = k * inv;
            } else {
                s1_.b0 = inv, s1_.b1 = -inv;
            }
        }
        return 1;
    }
    if (slope_ == 1) {
        filterDesignSection(type_, hz_, q_, rate_, s1_);
        return 1;
    }
    if (band) {
        filterDesignSection(type_, hz_, q_, rate_, s1_);
        s2_ = s1_;
    } else {
        filterDesignSection(type_, hz_, kFilterStage1Q, rate_, s1_);
        filterDesignSection(type_, hz_, q_ * kFilterStage2QScale, rate_, s2_);
    }
    return 2;
}

// Response of count_ sections in series at hz_, as a complex number (re_, im_).
inline void biquadResponse(const BiquadCoef* sections_, uint8_t count_, float hz_, float rate_, float& re_, float& im_) {
    const float w = 6.2831853f * hz_ / rate_;
    const float c1 = cosf(w), s1 = sinf(w), c2 = cosf(2.0f * w), s2 = sinf(2.0f * w);
    re_ = 1.0f;
    im_ = 0.0f;
    for (uint8_t i = 0; i < count_; i++) {
        const BiquadCoef& c = sections_[i];
        // H = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2), z^-1 = cos w - j sin w
        const float nr = c.b0 + c.b1 * c1 + c.b2 * c2, ni = -(c.b1 * s1 + c.b2 * s2);
        const float dr = 1.0f + c.a1 * c1 + c.a2 * c2, di = -(c.a1 * s1 + c.a2 * s2);
        const float dd = dr * dr + di * di;
        if (dd < 1.0e-20f) continue;
        const float hr = (nr * dr + ni * di) / dd, hi = (ni * dr - nr * di) / dd;
        const float r = re_ * hr - im_ * hi;
        im_ = re_ * hi + im_ * hr;
        re_ = r;
    }
}

inline float curveDb(float re_, float im_) {
    const float mag2 = re_ * re_ + im_ * im_;
    return (mag2 < 1.0e-12f) ? -120.0f : 10.0f * log10f(mag2);
}

// What one filter does to the signal.
struct FilterShape {
    BiquadCoef section[2];
    uint8_t sections;  // 1 or 2
    float wet, dry;    // 0..1 and above
    bool on;           // false: the filter is off and passes the signal unchanged
};

// Its response at hz_ in dB (not below -120): the sections in series, times WET, plus DRY.
inline float filterShapeDb(const FilterShape& s_, float hz_, float sampleRate_) {
    if (!s_.on) return 0.0f;
    float re, im;
    biquadResponse(s_.section, s_.sections, hz_, sampleRate_, re, im);
    return curveDb(s_.wet * re + s_.dry, s_.wet * im);
}

// The EQ: its six sections in series (low shelf, four peaks, high shelf).
inline float eqShapeDb(const BiquadCoef* sections_, uint8_t count_, float hz_, float sampleRate_) {
    float re, im;
    biquadResponse(sections_, count_, hz_, sampleRate_, re, im);
    return curveDb(re, im);
}

#endif
