// Euclidean rhythms for the EUCLID page: spread HITS as evenly as possible over STEPS, rotate,
// and lay the pattern out as beats of a bank, repeating it to the song end. Kept free of
// hardware headers so it can be compiled and tested on a PC (see 02 Software/tests).
//
// The pattern comes from Bjorklund's grouping algorithm, which gives the standard rhythms
// listed by Toussaint ("The Euclidean algorithm generates traditional musical rhythms", 2005),
// e.g. E(3,8) = x..x..x. and E(5,8) = x.xx.xx. (x = hit). Bit i of a pattern is step i.
//
// Not to be confused with the EUCLID fill type in Fill.h, which places hits inside one beat.
#ifndef __EUCLID_H
#define __EUCLID_H

#include <stdint.h>

#include "BeatList.h"

const uint8_t kEuclidMaxSteps = 32;

// Hits spread over steps (1-32), rotated later by rotate steps. hits > steps counts as steps.
inline uint32_t euclidPattern(uint8_t steps_, uint8_t hits_, uint8_t rotate_) {
    if ((steps_ == 0) || (steps_ > kEuclidMaxSteps) || (hits_ == 0)) return 0;
    if (hits_ > steps_) hits_ = steps_;

    // Groups of steps: A starts as `hits` groups "1", B as `steps - hits` groups "0". Each
    // round appends one B group to each A group; whatever is left over becomes the new B.
    // It runs at least once, so that with more hits than rests (E(3,4) = x.xx) the rests are
    // spread before the leftover hits are.
    uint32_t bitsA[kEuclidMaxSteps], bitsB[kEuclidMaxSteps];
    uint8_t lenA[kEuclidMaxSteps], lenB[kEuclidMaxSteps];
    uint8_t na = hits_, nb = static_cast<uint8_t>(steps_ - hits_);
    for (uint8_t i = 0; i < na; i++) bitsA[i] = 1, lenA[i] = 1;
    for (uint8_t i = 0; i < nb; i++) bitsB[i] = 0, lenB[i] = 1;
    for (bool first = true; (nb > 1) || (first && (nb == 1)); first = false) {
        const uint8_t m = (na < nb) ? na : nb;
        for (uint8_t i = 0; i < m; i++) {
            bitsA[i] |= bitsB[i] << lenA[i];
            lenA[i] = static_cast<uint8_t>(lenA[i] + lenB[i]);
        }
        if (na > m) {  // leftover A groups become B
            for (uint8_t i = m; i < na; i++) bitsB[i - m] = bitsA[i], lenB[i - m] = lenA[i];
            nb = static_cast<uint8_t>(na - m);
        } else {  // leftover B groups stay B
            for (uint8_t i = m; i < nb; i++) bitsB[i - m] = bitsB[i], lenB[i - m] = lenB[i];
            nb = static_cast<uint8_t>(nb - m);
        }
        na = m;
    }

    uint32_t pattern = 0;
    uint8_t pos = 0;
    for (uint8_t i = 0; i < na; i++) pattern |= bitsA[i] << pos, pos = static_cast<uint8_t>(pos + lenA[i]);
    for (uint8_t i = 0; i < nb; i++) pattern |= bitsB[i] << pos, pos = static_cast<uint8_t>(pos + lenB[i]);

    rotate_ %= steps_;
    if (rotate_ == 0) return pattern;
    uint32_t rotated = 0;
    for (uint8_t i = 0; i < steps_; i++) {
        if (pattern & (1UL << i)) rotated |= 1UL << ((i + rotate_) % steps_);
    }
    return rotated;
}

// Accents for a pattern made with euclidPattern(steps_, hits_, rotate_): accents_ of its hits
// spread evenly over the hits, Euclid-style (the accents are E(accents, hits) over the hits in
// order, starting at the first hit), rotated with the pattern. Returns a subset of the pattern's
// bits. 0 accents (or as many as hits) means no accent pattern: every hit is accented.
inline uint32_t euclidAccents(uint8_t steps_, uint8_t hits_, uint8_t rotate_, uint8_t accents_) {
    const uint32_t pattern = euclidPattern(steps_, hits_, rotate_);
    if ((steps_ == 0) || (steps_ > kEuclidMaxSteps)) return 0;
    if (hits_ > steps_) hits_ = steps_;
    if ((accents_ == 0) || (accents_ >= hits_)) return pattern;
    const uint32_t base = euclidPattern(steps_, hits_, 0);
    const uint32_t which = euclidPattern(hits_, accents_, 0);  // bit k: k-th hit is accented
    uint32_t accents = 0;
    uint8_t k = 0;
    for (uint8_t i = 0; i < steps_; i++) {
        if (!(base & (1UL << i))) continue;
        if (which & (1UL << k)) accents |= 1UL << ((i + rotate_ % steps_) % steps_);
        k++;
    }
    return accents;
}

// Writes a pattern as x and . (out_ needs steps_ + 1 bytes). With an accent mask other than the
// pattern itself, accented hits are X and the others x.
inline void euclidText(uint32_t pattern_, uint8_t steps_, char* out_, uint32_t accents_ = 0xFFFFFFFFUL) {
    const bool marked = (accents_ & pattern_) != pattern_;
    for (uint8_t i = 0; i < steps_; i++) {
        const uint32_t bit = 1UL << i;
        out_[i] = (pattern_ & bit) ? ((marked && (accents_ & bit)) ? 'X' : 'x') : '.';
    }
    out_[steps_] = '\0';
}

// Builds out_ from the pattern, one step per grid_ intervals, repeated from the song start to
// the song end. With merge_ the beats of base_ are kept (a hit on an existing beat keeps that
// beat and its fill); otherwise only the pattern's beats remain. New hits outside accents_ are
// GHOST beats (BASIC pattern 1, half velocity); the others FLAT. Returns how many hits didn't
// fit in the bank (0 when all did).
inline uint16_t euclidApply(Bank& out_, const Bank& base_, bool merge_, uint32_t pattern_, uint8_t steps_, uint16_t grid_, uint16_t songInterval_, uint32_t accents_ = 0xFFFFFFFFUL) {
    if (merge_) {
        out_ = base_;
    } else {
        beatListClear(out_);
    }
    uint16_t dropped = 0;
    if ((steps_ > 0) && (steps_ <= kEuclidMaxSteps) && (grid_ > 0)) {
        const uint32_t cycle = static_cast<uint32_t>(steps_) * grid_;
        for (uint32_t start = 0; start < songInterval_; start += cycle) {
            for (uint8_t i = 0; i < steps_; i++) {
                const uint32_t at = start + static_cast<uint32_t>(i) * grid_;
                if (at >= songInterval_) break;
                if (!(pattern_ & (1UL << i))) continue;
                if (beatListFind(out_, static_cast<uint16_t>(at)) != -1) continue;  // keep the beat already there
                if (!beatListAdd(out_, static_cast<uint16_t>(at), songInterval_)) {
                    dropped++;
                } else if (!(accents_ & (1UL << i))) {
                    out_.beatLibrary[beatListFind(out_, static_cast<uint16_t>(at))].setFillPattern(1);  // GHOST
                }
            }
        }
    }
    beatListRebuild(out_, songInterval_);
    return dropped;
}

#endif
