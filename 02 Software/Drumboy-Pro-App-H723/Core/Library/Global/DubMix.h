// DUB mode (the dub mixer, V1.15): every layer is a strip with a bus and three send levels. A song
// is either CHAIN (today's patch, set on the ROUTING page) or DUB. Hardware-free so it can be
// tested on a PC (see 02 Software/tests).
//
// Strip: BUS = FILT (through EQ and Filter 1, the dub filter, to the master) or DIR (straight to
// the master); ECHO, VERB and FX = send levels 0..100 % into Filter 2 and Effect 1 (the echo),
// the reverb and Effect 2. The strip's LEVEL is the layer's own level. In DUB mode the effects
// and the reverb run without their DRY: what they get is only the sends, and their WET is the
// return level.
//
// File: song files get a block after the "BTX" block. Older firmware stops reading at "EOF" and
// never sees it.
//   "DUB" | 2 (block version) | flags (bit 0: DUB mode) | 10 x (bus, echo, verb, fx) | 10 x pan
// Version 1 (no pan) is still read: every layer is centred.
#ifndef __DUBMIX_H
#define __DUBMIX_H

#include <stdint.h>
#include <string.h>

#include "Pan.h"
#include "Routing.h"

const uint8_t kDubStrips = 10;
const uint8_t kDubMaxSend = 100;
const uint16_t kDubFileSizeV1 = 3 + 1 + 1 + 4 * kDubStrips;  // 45: version 1, no pan
const uint16_t kDubFileSizeV2 = kDubFileSizeV1 + kDubStrips;  // 55: version 2, with the pan of each layer
const uint16_t kDubFileSize = kDubFileSizeV2 + 1;             // 56: and the echo width (older firmware reads the first 55 bytes and ignores the last)
const uint8_t kDubMaxWidth = 100;

enum DubBus : uint8_t { DUB_BUS_FILT = 0, DUB_BUS_DIR = 1 };

struct DubStrip {
    uint8_t bus = DUB_BUS_FILT;
    uint8_t echo = 0;
    uint8_t verb = 0;
    uint8_t fx = 0;
};

struct DubMix {
    bool on = false;
    DubStrip strip[kDubStrips];
    uint8_t echoWidth = 0;  // 0 = the centred echo of today, 100 = full ping-pong (the repeats alternate left and right)
    uint8_t pan[kDubStrips] = {kPanCentre, kPanCentre, kPanCentre, kPanCentre, kPanCentre, kPanCentre, kPanCentre, kPanCentre, kPanCentre, kPanCentre};  // Pan.h, in CHAIN and in DUB
};

// A throw (EFFECT 1 + pad, REVERB + pad) opens that send fully while the keys are held.
enum DubThrow : uint8_t { THROW_ECHO = 1, THROW_VERB = 2 };

// The gain a strip sends to each node. Not on the strip: the dry path always passes at 1.
inline void dubNodeGains(const DubStrip& s_, uint8_t throws_, float gains_[kRouteNodeCount]) {
    for (uint8_t n = 0; n < kRouteNodeCount; n++) gains_[n] = 0.0f;
    gains_[(s_.bus == DUB_BUS_DIR) ? NODE_MASTER : NODE_EQ] = 1.0f;
    const float inv = 1.0f / static_cast<float>(kDubMaxSend);
    gains_[NODE_F2] = (throws_ & THROW_ECHO) ? 1.0f : static_cast<float>(s_.echo > kDubMaxSend ? kDubMaxSend : s_.echo) * inv;
    gains_[NODE_REV] = (throws_ & THROW_VERB) ? 1.0f : static_cast<float>(s_.verb > kDubMaxSend ? kDubMaxSend : s_.verb) * inv;
    gains_[NODE_FX2] = static_cast<float>(s_.fx > kDubMaxSend ? kDubMaxSend : s_.fx) * inv;
}

// Dry of the effects and the reverb: off in DUB mode (their input is only the sends).
inline float dubDryScale(bool dubMode_) { return dubMode_ ? 0.0f : 1.0f; }

inline void dubClear(DubMix& d_) { d_ = DubMix(); }

inline void dubWrite(const DubMix& d_, uint8_t* out_) {
    memcpy(out_, "DUB", 3);
    out_[3] = 2;
    out_[4] = d_.on ? 1 : 0;
    for (uint8_t i = 0; i < kDubStrips; i++) {
        out_[5 + 4 * i] = d_.strip[i].bus;
        out_[6 + 4 * i] = d_.strip[i].echo;
        out_[7 + 4 * i] = d_.strip[i].verb;
        out_[8 + 4 * i] = d_.strip[i].fx;
    }
    for (uint8_t i = 0; i < kDubStrips; i++) out_[kDubFileSizeV1 + i] = d_.pan[i];
    out_[kDubFileSizeV2] = d_.echoWidth;
}

// Returns false (and leaves the default mix, CHAIN) when the block is missing or not valid.
inline bool dubRead(DubMix& d_, const uint8_t* in_, uint16_t size_) {
    dubClear(d_);
    if ((size_ < kDubFileSizeV1) || (memcmp(in_, "DUB", 3) != 0) || ((in_[3] != 1) && (in_[3] != 2))) return false;
    if ((in_[3] == 2) && (size_ < kDubFileSizeV2)) return false;
    DubMix d;
    d.on = (in_[4] & 1) != 0;
    for (uint8_t i = 0; i < kDubStrips; i++) {
        const uint8_t bus = in_[5 + 4 * i], echo = in_[6 + 4 * i], verb = in_[7 + 4 * i], fx = in_[8 + 4 * i];
        if ((bus > DUB_BUS_DIR) || (echo > kDubMaxSend) || (verb > kDubMaxSend) || (fx > kDubMaxSend)) return false;
        d.strip[i].bus = bus;
        d.strip[i].echo = echo;
        d.strip[i].verb = verb;
        d.strip[i].fx = fx;
    }
    if (in_[3] == 2) {
        for (uint8_t i = 0; i < kDubStrips; i++) {
            if (in_[kDubFileSizeV1 + i] > kPanMax) return false;
            d.pan[i] = in_[kDubFileSizeV1 + i];
        }
        if (size_ >= kDubFileSize) {
            if (in_[kDubFileSizeV2] > kDubMaxWidth) return false;
            d.echoWidth = in_[kDubFileSizeV2];
        }
    }
    d_ = d;
    return true;
}

#endif
