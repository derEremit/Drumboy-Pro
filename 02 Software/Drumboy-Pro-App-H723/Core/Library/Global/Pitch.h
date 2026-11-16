// Layer pitch, hardware-free so it can be tested on a PC (see 02 Software/tests).
//
// One byte per layer holds a pitch code:
//   0 .. 48    semitones -24 .. +24 (24 = no change), factor 2^(n/12). Used from V1.11.
//   49 .. 199  the old linear speed scale (index 0 .. 150 = 0.50x .. 2.00x in 0.01 steps), kept
//              for layers loaded from older kits and songs, so they play exactly as before. The
//              first PITCH turn snaps such a layer to the nearest semitone.
//
// Kit and song files: the layer record keeps an old-scale value in byte 1 (older firmware reads
// that, so a V1.11 file still plays at about the right pitch there) and the semitone value + 1 in
// byte 24 (0 = none). Byte 24 is only trusted in files with format version 1 or later
// (FileFormat.h), because older files have a space there.
#ifndef __PITCH_H
#define __PITCH_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>

const int8_t kPitchMinSemi = -24;
const int8_t kPitchMaxSemi = 24;
const uint8_t kPitchSemiCodeCenter = 24;
const uint8_t kPitchSemiCodeMax = 48;
const uint8_t kPitchLegacyCodeBase = 49;
const uint8_t kPitchLegacyMax = 150;
const uint8_t kPitchCodeMax = kPitchLegacyCodeBase + kPitchLegacyMax;  // 199
const uint8_t kPitchDefaultCode = kPitchSemiCodeCenter;               // 0 semitones
const uint8_t kPitchLfoSemiRange = 24;                                // full LFO depth: +-12 semitones

// The old table's factors, exactly (kLayerPitchDataLibrary in older firmware).
const float kPitchLegacyFactor[kPitchLegacyMax + 1] = {0.50f, 0.51f, 0.52f, 0.53f, 0.54f, 0.55f, 0.56f, 0.57f, 0.58f, 0.59f, 0.60f, 0.61f, 0.62f, 0.63f, 0.64f, 0.65f, 0.66f, 0.67f, 0.68f, 0.69f, 0.70f, 0.71f, 0.72f, 0.73f, 0.74f, 0.75f, 0.76f, 0.77f, 0.78f, 0.79f, 0.80f, 0.81f, 0.82f, 0.83f, 0.84f, 0.85f, 0.86f, 0.87f, 0.88f, 0.89f, 0.90f, 0.91f, 0.92f, 0.93f, 0.94f, 0.95f, 0.96f, 0.97f, 0.98f, 0.99f, 1.00f, 1.01f, 1.02f, 1.03f, 1.04f, 1.05f, 1.06f, 1.07f, 1.08f, 1.09f, 1.10f, 1.11f, 1.12f, 1.13f, 1.14f, 1.15f, 1.16f, 1.17f, 1.18f, 1.19f, 1.20f, 1.21f, 1.22f, 1.23f, 1.24f, 1.25f, 1.26f, 1.27f, 1.28f, 1.29f, 1.30f, 1.31f, 1.32f, 1.33f, 1.34f, 1.35f, 1.36f, 1.37f, 1.38f, 1.39f, 1.40f, 1.41f, 1.42f, 1.43f, 1.44f, 1.45f, 1.46f, 1.47f, 1.48f, 1.49f, 1.50f, 1.51f, 1.52f, 1.53f, 1.54f, 1.55f, 1.56f, 1.57f, 1.58f, 1.59f, 1.60f, 1.61f, 1.62f, 1.63f, 1.64f, 1.65f, 1.66f, 1.67f, 1.68f, 1.69f, 1.70f, 1.71f, 1.72f, 1.73f, 1.74f, 1.75f, 1.76f, 1.77f, 1.78f, 1.79f, 1.80f, 1.81f, 1.82f, 1.83f, 1.84f, 1.85f, 1.86f, 1.87f, 1.88f, 1.89f, 1.90f, 1.91f, 1.92f, 1.93f, 1.94f, 1.95f, 1.96f, 1.97f, 1.98f, 1.99f, 2.00f};

// 2^((code - 24) / 12)
const float kPitchSemiFactor[kPitchSemiCodeMax + 1] = {0.250000000f, 0.264865774f, 0.280615512f, 0.297301779f, 0.314980262f, 0.333709964f, 0.353553391f, 0.374576769f, 0.396850263f, 0.420448208f, 0.445449359f, 0.471937156f, 0.500000000f, 0.529731547f, 0.561231024f, 0.594603558f, 0.629960525f, 0.667419927f, 0.707106781f, 0.749153538f, 0.793700526f, 0.840896415f, 0.890898718f, 0.943874313f, 1.000000000f, 1.059463094f, 1.122462048f, 1.189207115f, 1.259921050f, 1.334839854f, 1.414213562f, 1.498307077f, 1.587401052f, 1.681792831f, 1.781797436f, 1.887748625f, 2.000000000f, 2.118926189f, 2.244924097f, 2.378414230f, 2.519842100f, 2.669679708f, 2.828427125f, 2.996614154f, 3.174802104f, 3.363585661f, 3.563594873f, 3.775497251f, 4.000000000f};

inline bool pitchIsLegacy(uint8_t code_) { return code_ >= kPitchLegacyCodeBase; }

inline uint8_t pitchCodeFromSemi(int semi_) {
    if (semi_ < kPitchMinSemi) semi_ = kPitchMinSemi;
    if (semi_ > kPitchMaxSemi) semi_ = kPitchMaxSemi;
    return static_cast<uint8_t>(semi_ + kPitchSemiCodeCenter);
}

inline uint8_t pitchClampCode(uint8_t code_) { return (code_ > kPitchCodeMax) ? kPitchDefaultCode : code_; }

inline float pitchFactor(uint8_t code_) {
    code_ = pitchClampCode(code_);
    return pitchIsLegacy(code_) ? kPitchLegacyFactor[code_ - kPitchLegacyCodeBase] : kPitchSemiFactor[code_];
}

// Nearest semitone (legacy codes are converted from their factor).
inline int pitchSemi(uint8_t code_) {
    code_ = pitchClampCode(code_);
    if (!pitchIsLegacy(code_)) return static_cast<int>(code_) - kPitchSemiCodeCenter;
    return static_cast<int>(lroundf(12.0f * log2f(pitchFactor(code_))));
}

// One step of the PITCH encoder: semitones (+-1) or to the next octave line (+-12 snaps to
// -24, -12, 0, 12, 24). A legacy pitch snaps to its nearest semitone first. Clamped, no wrap.
inline uint8_t pitchStep(uint8_t code_, bool up_, bool octave_) {
    int s = pitchSemi(code_);
    if (octave_) {
        s = up_ ? ((s >= 0) ? (s / 12 + 1) * 12 : -((-s - 1) / 12) * 12) : ((s > 0) ? ((s - 1) / 12) * 12 : -((-s) / 12 + 1) * 12);
    } else {
        s += up_ ? 1 : -1;
    }
    return pitchCodeFromSemi(s);
}

// The pitch a hit plays with an LFO on PITCH (lfo_ 0..1, depth_ 0..1). Legacy layers keep the
// old rule (+-50 index steps at full depth, i.e. what older firmware did); semitone layers move
// +-12 semitones at full depth.
inline uint8_t pitchWithLfo(uint8_t code_, float lfo_, float depth_) {
    code_ = pitchClampCode(code_);
    if (pitchIsLegacy(code_)) {
        int n = static_cast<int>(code_ - kPitchLegacyCodeBase) + static_cast<int>(round(100 * (lfo_ - 0.5) * depth_));
        n = (n < 0) ? 0 : ((n > kPitchLegacyMax) ? kPitchLegacyMax : n);
        return static_cast<uint8_t>(kPitchLegacyCodeBase + n);
    }
    return pitchCodeFromSemi(pitchSemi(code_) + static_cast<int>(round(kPitchLfoSemiRange * (lfo_ - 0.5) * depth_)));
}

// Field text, right-aligned in 10 characters: "+7", "-12", "0" for semitones; legacy codes keep
// the old "1.37" style so it's clear they were not converted yet.
inline void pitchName(uint8_t code_, char* out_, size_t size_) {
    code_ = pitchClampCode(code_);
    if (pitchIsLegacy(code_)) {
        const unsigned i = code_ - kPitchLegacyCodeBase + 50;  // 50..200 = 0.50..2.00
        snprintf(out_, size_, "%7u.%02u", i / 100, i % 100);
    } else {
        const int s = pitchSemi(code_);
        snprintf(out_, size_, s ? "%+10d" : "%10d", s);
    }
}

// File bytes (layer record byte 1 and byte 24).
inline uint8_t pitchFileOldByte(uint8_t code_) {
    code_ = pitchClampCode(code_);
    if (pitchIsLegacy(code_)) return static_cast<uint8_t>(code_ - kPitchLegacyCodeBase);
    long i = lroundf((pitchFactor(code_) - 0.5f) * 100.0f);
    return static_cast<uint8_t>((i < 0) ? 0 : ((i > kPitchLegacyMax) ? kPitchLegacyMax : i));
}

inline uint8_t pitchFileSemiByte(uint8_t code_) {
    code_ = pitchClampCode(code_);
    return pitchIsLegacy(code_) ? 0 : static_cast<uint8_t>(code_ + 1);
}

inline uint8_t pitchFromFile(uint8_t oldByte_, uint8_t semiByte_, uint8_t formatVersion_) {
    if ((formatVersion_ >= 1) && (semiByte_ >= 1) && (semiByte_ <= kPitchSemiCodeMax + 1)) return static_cast<uint8_t>(semiByte_ - 1);
    if (oldByte_ <= kPitchLegacyMax) return static_cast<uint8_t>(kPitchLegacyCodeBase + oldByte_);
    return kPitchDefaultCode;
}

#endif
