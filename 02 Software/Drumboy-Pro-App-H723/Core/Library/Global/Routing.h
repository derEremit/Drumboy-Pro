// Signal routing: where each source and each processing module sends its audio.
//
// Nodes run in a fixed order (EQ, Filter 1, Filter 2, FX 1, FX 2, Reverb) and a module may
// only send to a later node or to the master, so every patch is loop-free and adds no
// latency. A destination is a bitmask (bit n = node n), so one signal can feed several
// nodes; an empty mask mutes it. Signals stay mono until the Reverb, which produces the
// stereo pair; mono signals that reach the master go to both channels.
//
// Kept free of hardware headers so the routing can be tested on a PC (02 Software/tests).
#ifndef __ROUTING_H
#define __ROUTING_H

#include <stdint.h>
#include <string.h>

enum RouteNode : uint8_t {
    NODE_EQ = 0,
    NODE_F1,
    NODE_F2,
    NODE_FX1,
    NODE_FX2,
    NODE_REV,
    NODE_MASTER,
    kRouteNodeCount  // 7
};

const uint8_t kRouteModuleCount = NODE_MASTER;  // 6 processing modules
const uint8_t kRouteAllNodes = (1u << kRouteNodeCount) - 1;

constexpr uint8_t routeBit(uint8_t node_) { return static_cast<uint8_t>(1u << node_); }

// Where each module's output goes.
struct RoutePatch {
    uint8_t moduleDest[kRouteModuleCount];
};

// Today's serial chain: EQ -> Filter 1 -> Filter 2 -> FX 1 -> FX 2 -> Reverb -> Master.
const RoutePatch kDefaultRoutePatch = {{routeBit(NODE_F1), routeBit(NODE_F2), routeBit(NODE_FX1), routeBit(NODE_FX2), routeBit(NODE_REV), routeBit(NODE_MASTER)}};

// DUB mode (DubMix.h): the bus strips feed EQ and Filter 1 (the dub filter) and go to the master;
// the echo send goes through Filter 2 into Effect 1; Effect 2 is the third send; the reverb is
// the reverb send. Every return goes to the master.
const RoutePatch kDubRoutePatch = {{routeBit(NODE_F1), routeBit(NODE_MASTER), routeBit(NODE_FX1), routeBit(NODE_MASTER), routeBit(NODE_MASTER), routeBit(NODE_MASTER)}};

// A module may send only to later nodes (no feedback); the Reverb always feeds the master.
inline bool routeModuleDestValid(uint8_t module_, uint8_t mask_) {
    if (module_ >= kRouteModuleCount) return false;
    if (module_ == NODE_REV) return mask_ == routeBit(NODE_MASTER);
    const uint8_t allowed = static_cast<uint8_t>(kRouteAllNodes & ~((routeBit(module_) << 1) - 1));  // bits above module_
    return (mask_ & ~allowed) == 0;
}

inline bool routePatchValid(const RoutePatch& patch_) {
    for (uint8_t m = 0; m < kRouteModuleCount; m++) {
        if (!routeModuleDestValid(m, patch_.moduleDest[m])) return false;
    }
    return true;
}

// The firmware's per-source "mixer in" setting (MixerInMode: EQ, FILTER_0, FILTER_1, EFFECT_0,
// EFFECT_1, REVERB, SEND) lists the nodes in the same order, SEND being the master. So an
// entry point converts directly to a single-destination mask; anything out of range is muted.
inline uint8_t routeMaskFromEntry(uint8_t mixerInMode_) {
    return (mixerInMode_ < kRouteNodeCount) ? routeBit(mixerInMode_) : 0;
}

// Old per-instrument settings (EQ on/off, Filter OFF/02/01+02, Effect OFF/02/01+02, Reverb on/off)
// <-> entry point. Kit and song files keep storing the four settings, so files stay compatible
// with older firmware. Entry point from the settings: the earliest stage that is on
// (same priority as the old layerInst_calculateMixMode).
inline uint8_t routeEntryFromLegacy(uint8_t eq_, uint8_t filter_, uint8_t effect_, uint8_t reverb_) {
    if (eq_) return NODE_EQ;
    if (filter_ == 2) return NODE_F1;  // 01+02
    if (filter_ == 1) return NODE_F2;  // 02
    if (effect_ == 2) return NODE_FX1;
    if (effect_ == 1) return NODE_FX2;
    if (reverb_) return NODE_REV;
    return NODE_MASTER;
}

// The four settings the old page produced for each entry point (its exclusive rules switch on
// every later stage), in the order eq, filter, effect, reverb.
const uint8_t kRouteLegacyFromEntry[kRouteNodeCount][4] = {
    {1, 2, 2, 1},  // EQ
    {0, 2, 2, 1},  // Filter 1 (01+02)
    {0, 1, 2, 1},  // Filter 2 (02)
    {0, 0, 2, 1},  // FX 1 (01+02)
    {0, 0, 1, 1},  // FX 2 (02)
    {0, 0, 0, 1},  // Reverb
    {0, 0, 0, 0},  // Master
};

// File storage. Kit and song files share a 1500-byte kit section whose spare bytes hold the patch:
//   490-495  module destinations (EQ ... Reverb), one mask each
//   498      marker 0xA5: the patch bytes are valid
//   240 + 25*layer + 23  the layer's destination mask (its entry point is also kept in the four
//            old settings at +16..+19, which stay authoritative so older firmware reads the file)
// Files without the marker (older firmware, or the blank-kit template whose spare bytes are
// spaces) load the default chain. An invalid module mask falls back to the default for that module.
const uint16_t kRouteFileModuleOffset = 490;
const uint16_t kRouteFileMarkerOffset = 498;
const uint8_t kRouteFileMarker = 0xA5;
const uint8_t kRouteFileLayerByte = 23;

inline void routeWritePatch(uint8_t* kit_, const RoutePatch& patch_) {
    for (uint8_t m = 0; m < kRouteModuleCount; m++) kit_[kRouteFileModuleOffset + m] = patch_.moduleDest[m];
    kit_[kRouteFileMarkerOffset] = kRouteFileMarker;
}

// Returns true when the file carried a patch.
inline bool routeReadPatch(const uint8_t* kit_, RoutePatch& patch_) {
    patch_ = kDefaultRoutePatch;
    if (kit_[kRouteFileMarkerOffset] != kRouteFileMarker) return false;
    for (uint8_t m = 0; m < kRouteModuleCount; m++) {
        const uint8_t mask = kit_[kRouteFileModuleOffset + m];
        if (routeModuleDestValid(m, mask)) patch_.moduleDest[m] = mask;
    }
    return true;
}

// The metronome destination is a global setting, like the other metronome settings, kept in
// the system preset's byte 11 upper nibble as entry + 1 (byte 11 bit 0 is the line-in mute,
// saved but never loaded; older firmware ignores the byte). 0 = not set: the reverb, as before.
inline uint8_t routePackMetro(uint8_t presetByte_, uint8_t metroEntry_) {
    return static_cast<uint8_t>((presetByte_ & 0x0F) | ((metroEntry_ < kRouteNodeCount ? metroEntry_ + 1 : 0) << 4));
}

inline uint8_t routeUnpackMetro(uint8_t presetByte_) {
    const uint8_t v = presetByte_ >> 4;
    return (v >= 1 && v <= kRouteNodeCount) ? static_cast<uint8_t>(v - 1) : static_cast<uint8_t>(NODE_REV);
}

// The destination shown for a mask: its first node (single-destination routes have one).
inline uint8_t routeFirstNode(uint8_t mask_) {
    for (uint8_t n = 0; n < kRouteNodeCount; n++)
        if (mask_ & routeBit(n)) return n;
    return NODE_MASTER;
}

// Encoder stepping for a module's OUT: only later nodes and MASTER, wrapping around.
inline uint8_t routeNextModuleOut(uint8_t module_, uint8_t current_, bool up_) {
    const uint8_t first = module_ + 1, last = NODE_MASTER;
    if (current_ < first || current_ > last) return first;
    if (up_) return (current_ < last) ? current_ + 1 : first;
    return (current_ > first) ? current_ - 1 : last;
}

// ROUTING page overview. Sources are a 16-bit set: bits 0-9 the instrument layers,
// kRouteSourceMetro and kRouteSourceLineIn the metronome and line-in.
const uint8_t kRouteSourceMetro = 10;
const uint8_t kRouteSourceLineIn = 11;

inline const char* routeNodeShortName(uint8_t node_) {
    static const char* const kNames[kRouteNodeCount] = {"EQ", "F1", "F2", "FX1", "FX2", "REV", "OUT"};
    return (node_ < kRouteNodeCount) ? kNames[node_] : "?";
}

// The chain a signal entering at entry_ travels: each module's first destination, up to the
// master. Stops early at a muted module (empty mask). Returns the number of nodes in path_.
inline uint8_t routePath(const RoutePatch& patch_, uint8_t entry_, uint8_t path_[kRouteNodeCount]) {
    uint8_t count = 0, node = entry_;
    while (count < kRouteNodeCount && node < kRouteNodeCount) {
        path_[count++] = node;
        if (node == NODE_MASTER) break;
        const uint8_t mask = (node == NODE_REV) ? routeBit(NODE_MASTER) : patch_.moduleDest[node];
        if (mask == 0) break;
        const uint8_t next = routeFirstNode(mask);
        if (next <= node) break;  // invalid backwards route: stop rather than loop
        node = next;
    }
    return count;
}

// Modules that receive any signal: directly from a source or from a module that is fed.
inline uint8_t routeFedModules(const RoutePatch& patch_, const uint8_t* sourceMasks_, uint8_t sourceCount_) {
    uint8_t fed = 0;
    for (uint8_t s = 0; s < sourceCount_; s++) fed |= sourceMasks_[s];
    for (uint8_t m = 0; m < kRouteModuleCount; m++) {
        if (!(fed & routeBit(m))) continue;
        const uint8_t dest = (m == NODE_REV) ? routeBit(NODE_MASTER) : patch_.moduleDest[m];
        fed |= dest & static_cast<uint8_t>(~((routeBit(m) << 1) - 1));
    }
    return fed & static_cast<uint8_t>(routeBit(NODE_MASTER) - 1);
}

// One overview line: the sources entering at entry_, then their chain, e.g.
// "L1-3 L7 MT > F1 > F2 > FX1 > FX2 > REV > OUT". Layers are numbered from 1 and runs of three
// or more are shortened. A line that does not fit in size_ - 1 characters drops the spaces around
// ">" ("L1-10 LI>EQ>F1>...>OUT"); one that still does not fit ends in "..". Returns the length.
inline uint8_t routeOverviewLineSep(char* out_, uint8_t size_, uint16_t sources_, uint8_t entry_, const RoutePatch& patch_, const char* sep_) {
    char buf[96];
    uint8_t len = 0;
    auto put = [&](const char* t) {
        while (*t && len < sizeof(buf) - 1) buf[len++] = *t++;
    };
    auto putNum = [&](uint8_t v) {
        char d[4];
        uint8_t n = 0;
        do { d[n++] = static_cast<char>('0' + v % 10); v /= 10; } while (v && n < 3);
        while (n) { char c[2] = {d[--n], 0}; put(c); }
    };
    // Layers, with runs of 3+ as "Lx-y".
    for (uint8_t i = 0; i < 10; i++) {
        if (!(sources_ & (1u << i))) continue;
        uint8_t j = i;
        while (j + 1 < 10 && (sources_ & (1u << (j + 1)))) j++;
        if (len) put(" ");
        put("L");
        putNum(i + 1);
        if (j - i >= 2) {
            put("-");
            putNum(j + 1);
            i = j;
        }
    }
    if (sources_ & (1u << kRouteSourceMetro)) { if (len) put(" "); put("MT"); }
    if (sources_ & (1u << kRouteSourceLineIn)) { if (len) put(" "); put("LI"); }
    uint8_t path[kRouteNodeCount];
    const uint8_t n = routePath(patch_, entry_, path);
    for (uint8_t k = 0; k < n; k++) {
        put(sep_);
        put(routeNodeShortName(path[k]));
    }
    buf[len] = 0;
    if (size_ == 0) return 0;
    if (len >= size_) {
        len = size_ - 1;
        if (len >= 2) buf[len - 2] = buf[len - 1] = '.';
    }
    memcpy(out_, buf, len);
    out_[len] = 0;
    return len;
}

inline uint8_t routeOverviewLine(char* out_, uint8_t size_, uint16_t sources_, uint8_t entry_, const RoutePatch& patch_) {
    char wide[96];
    const uint8_t len = routeOverviewLineSep(wide, sizeof(wide), sources_, entry_, patch_, " > ");
    if (len < size_) {
        memcpy(out_, wide, len + 1);
        return len;
    }
    return routeOverviewLineSep(out_, size_, sources_, entry_, patch_, ">");
}

// Buffers for one block. Every module node is a stereo pair: nodeL[m] and nodeR[m] are module m's
// input, processed in place to become its output (the Reverb makes its stereo field from them).
// A source that is not panned goes into both channels equally, so a signal that is never panned
// sounds exactly as it did when the nodes were mono.
struct RouteBuffers {
    float* nodeL[kRouteModuleCount];
    float* nodeR[kRouteModuleCount];
    float* masterLeft;
    float* masterRight;
};

inline float* routeLeft(const RouteBuffers& buf_, uint8_t node_) { return (node_ == NODE_MASTER) ? buf_.masterLeft : buf_.nodeL[node_]; }
inline float* routeRight(const RouteBuffers& buf_, uint8_t node_) { return (node_ == NODE_MASTER) ? buf_.masterRight : buf_.nodeR[node_]; }

// Adds a mono buffer into both channels of every node in mask_. gain_ (optional) scales each
// sample, for crossfades.
template <uint16_t kBlock>
inline void routeScatter(const RouteBuffers& buf_, const float* src_, uint8_t mask_, const float* gain_ = nullptr) {
    if (mask_ == 0) return;
    for (uint8_t n = 0; n < kRouteNodeCount; n++) {
        if (!(mask_ & routeBit(n))) continue;
        float* left = routeLeft(buf_, n);
        float* right = routeRight(buf_, n);
        if (gain_) {
            for (uint16_t i = 0; i < kBlock; i++) {
                const float v = src_[i] * gain_[i];
                left[i] += v;
                right[i] += v;
            }
        } else {
            for (uint16_t i = 0; i < kBlock; i++) {
                left[i] += src_[i];
                right[i] += src_[i];
            }
        }
    }
}

// Adds a stereo pair into the same channels of every node in mask_ (a module's output).
template <uint16_t kBlock>
inline void routeScatterPair(const RouteBuffers& buf_, const float* srcL_, const float* srcR_, uint8_t mask_, const float* gain_ = nullptr) {
    if (mask_ == 0) return;
    for (uint8_t n = 0; n < kRouteNodeCount; n++) {
        if (!(mask_ & routeBit(n))) continue;
        float* left = routeLeft(buf_, n);
        float* right = routeRight(buf_, n);
        if (gain_) {
            for (uint16_t i = 0; i < kBlock; i++) {
                left[i] += srcL_[i] * gain_[i];
                right[i] += srcR_[i] * gain_[i];
            }
        } else {
            for (uint16_t i = 0; i < kBlock; i++) {
                left[i] += srcL_[i];
                right[i] += srcR_[i];
            }
        }
    }
}

// Click-free route changes. A continuously running signal (line-in, metronome, a module's
// output) whose destination changes fades out of the old nodes while fading into the new ones
// over kRouteFadeLength samples. A change that arrives during a fade waits until it has
// finished, so fast encoder turns never cut a fade short. A route that has never been set
// (kRouteUnset) takes its first destination at once.
const uint8_t kRouteFadeLength = 64;  // samples, ~1.5 ms at 44.1 kHz
const uint8_t kRouteUnset = 0xFF;

struct RouteFade {
    uint8_t mask = kRouteUnset;            // current destination
    uint8_t previous = 0;                  // destination fading out
    uint8_t position = kRouteFadeLength;   // samples into the running fade
};

inline bool routeFading(const RouteFade& fade_) { return fade_.position < kRouteFadeLength; }

// Call once per block with the wanted destination.
inline void routeFadeTarget(RouteFade& fade_, uint8_t mask_) {
    if (fade_.mask == kRouteUnset) {
        fade_.mask = mask_;
        fade_.position = kRouteFadeLength;
    } else if ((mask_ != fade_.mask) && !routeFading(fade_)) {
        fade_.previous = fade_.mask;
        fade_.mask = mask_;
        fade_.position = 0;
    }
}

// The gains of a running fade over one block.
template <uint16_t kBlock>
inline void routeFadeGains(const RouteFade& fade_, float* up_, float* down_) {
    for (uint16_t i = 0; i < kBlock; i++) {
        float t = (float)(fade_.position + i + 1) / (float)kRouteFadeLength;
        if (t > 1.0f) t = 1.0f;
        up_[i] = t;
        down_[i] = 1.0f - t;
    }
}

template <uint16_t kBlock>
inline void routeAdvanceFade(RouteFade& fade_) {
    const uint16_t next = fade_.position + kBlock;
    fade_.position = (next < kRouteFadeLength) ? static_cast<uint8_t>(next) : kRouteFadeLength;
}

// Adds a mono src_ to its destination, crossfading if a fade is running, and advances the fade.
template <uint16_t kBlock>
inline void routeScatterFaded(const RouteBuffers& buf_, const float* src_, RouteFade& fade_) {
    if (!routeFading(fade_)) {
        routeScatter<kBlock>(buf_, src_, fade_.mask);
        return;
    }
    float up[kBlock], down[kBlock];
    routeFadeGains<kBlock>(fade_, up, down);
    routeScatter<kBlock>(buf_, src_, fade_.mask & fade_.previous);                                      // unchanged nodes
    routeScatter<kBlock>(buf_, src_, static_cast<uint8_t>(fade_.mask & ~fade_.previous), up);          // new nodes
    routeScatter<kBlock>(buf_, src_, static_cast<uint8_t>(fade_.previous & ~fade_.mask), down);        // old nodes
    routeAdvanceFade<kBlock>(fade_);
}

// The same for a stereo pair (a module's output).
template <uint16_t kBlock>
inline void routeScatterPairFaded(const RouteBuffers& buf_, const float* srcL_, const float* srcR_, RouteFade& fade_) {
    if (!routeFading(fade_)) {
        routeScatterPair<kBlock>(buf_, srcL_, srcR_, fade_.mask);
        return;
    }
    float up[kBlock], down[kBlock];
    routeFadeGains<kBlock>(fade_, up, down);
    routeScatterPair<kBlock>(buf_, srcL_, srcR_, fade_.mask & fade_.previous);
    routeScatterPair<kBlock>(buf_, srcL_, srcR_, static_cast<uint8_t>(fade_.mask & ~fade_.previous), up);
    routeScatterPair<kBlock>(buf_, srcL_, srcR_, static_cast<uint8_t>(fade_.previous & ~fade_.mask), down);
    routeAdvanceFade<kBlock>(fade_);
}

// A panned source (an instrument layer): it goes to every node with a left and a right gain, each
// moving linearly from its value at the end of the last block to the target over the block, so a
// knob turned while the signal plays makes no zipper noise. state_ keeps the gains between blocks:
// kRouteNodeCount for the left, then kRouteNodeCount for the right. A gain that is 1.0 before and
// after leaves the signal exactly as it is.
const uint8_t kRouteGainStateSize = 2 * kRouteNodeCount;

template <uint16_t kBlock>
inline void routeScatterLR(const RouteBuffers& buf_, const float* src_, const float* targetL_, const float* targetR_, float* state_) {
    for (uint8_t n = 0; n < kRouteNodeCount; n++) {
        const float fromL = state_[n], fromR = state_[kRouteNodeCount + n];
        const float toL = targetL_[n], toR = targetR_[n];
        state_[n] = toL;
        state_[kRouteNodeCount + n] = toR;
        if ((fromL == 0.0f) && (toL == 0.0f) && (fromR == 0.0f) && (toR == 0.0f)) continue;
        float* left = routeLeft(buf_, n);
        float* right = routeRight(buf_, n);
        const float stepL = (toL - fromL) / static_cast<float>(kBlock), stepR = (toR - fromR) / static_cast<float>(kBlock);
        float gL = fromL, gR = fromR;
        for (uint16_t i = 0; i < kBlock; i++) {
            gL += stepL;
            gR += stepR;
            left[i] += src_[i] * gL;
            right[i] += src_[i] * gR;
        }
    }
}

// The same for a stereo source (a layer playing a stereo sample): the left signal goes to the left
// of every node with the left gain, the right signal to the right with the right gain. With the pan
// gains of Pan.h this makes the pan a balance: the centre passes both channels untouched.
template <uint16_t kBlock>
inline void routeScatterStereoLR(const RouteBuffers& buf_, const float* srcL_, const float* srcR_, const float* targetL_, const float* targetR_, float* state_) {
    for (uint8_t n = 0; n < kRouteNodeCount; n++) {
        const float fromL = state_[n], fromR = state_[kRouteNodeCount + n];
        const float toL = targetL_[n], toR = targetR_[n];
        state_[n] = toL;
        state_[kRouteNodeCount + n] = toR;
        if ((fromL == 0.0f) && (toL == 0.0f) && (fromR == 0.0f) && (toR == 0.0f)) continue;
        float* left = routeLeft(buf_, n);
        float* right = routeRight(buf_, n);
        const float stepL = (toL - fromL) / static_cast<float>(kBlock), stepR = (toR - fromR) / static_cast<float>(kBlock);
        float gL = fromL, gR = fromR;
        for (uint16_t i = 0; i < kBlock; i++) {
            gL += stepL;
            gR += stepR;
            left[i] += srcL_[i] * gL;
            right[i] += srcR_[i] * gR;
        }
    }
}

// A source for one block: its buffer, where it goes, and how.
//   Line-in and the metronome run continuously: a mask and a fade state (routeScatterFaded).
//   An instrument layer (gainState set): its mask (CHAIN, latched at the note) or its per-node
//   gains (DUB), times its pan, ramped (routeScatterLR).
struct RouteSource {
    const float* buffer;
    uint8_t mask;
    RouteFade* fade;
    const float* gains = nullptr;  // per node, kRouteNodeCount of them (DUB); null = the mask, every node 1
    float* gainState = nullptr;    // kRouteGainStateSize floats kept between blocks; null = not panned
    float panLeft = 1.0f;          // Pan.h gains
    float panRight = 1.0f;
    const float* bufferR = nullptr;  // the right channel of a stereo source (needs gainState); null = mono
};

// Fade state of the module outputs, kept between blocks.
struct RouteState {
    RouteFade module[kRouteModuleCount];
};

// Runs one block: clears the node inputs and the master, adds every source into its
// destinations, then processes the modules in order, each one passing its output on.
//   process_(m): processes node m (both channels) in place; a module that is switched off must
//                leave its buffers unchanged (pass-through). The Reverb makes its stereo field
//                from the pair it gets and writes it back into the same pair.
template <uint16_t kBlock, typename Process>
inline void routeBlock(const RoutePatch& patch_, RouteState& state_, const RouteBuffers& buf_, RouteSource* sources_, uint8_t sourceCount_, Process process_) {
    // 1. CLEAR
    for (uint8_t n = 0; n < kRouteModuleCount; n++) {
        memset(buf_.nodeL[n], 0, kBlock * sizeof(float));
        memset(buf_.nodeR[n], 0, kBlock * sizeof(float));
    }
    memset(buf_.masterLeft, 0, kBlock * sizeof(float));
    memset(buf_.masterRight, 0, kBlock * sizeof(float));

    // 2. SCATTER SOURCES
    for (uint8_t s = 0; s < sourceCount_; s++) {
        RouteSource& src = sources_[s];
        if (src.gainState) {
            float gl[kRouteNodeCount], gr[kRouteNodeCount];
            for (uint8_t n = 0; n < kRouteNodeCount; n++) {
                const float base = src.gains ? src.gains[n] : (((src.mask >> n) & 1u) ? 1.0f : 0.0f);
                gl[n] = base * src.panLeft;
                gr[n] = base * src.panRight;
            }
            if (src.bufferR)
                routeScatterStereoLR<kBlock>(buf_, src.buffer, src.bufferR, gl, gr, src.gainState);
            else
                routeScatterLR<kBlock>(buf_, src.buffer, gl, gr, src.gainState);
        } else if (src.fade) {
            routeFadeTarget(*src.fade, src.mask);
            routeScatterFaded<kBlock>(buf_, src.buffer, *src.fade);
        } else {
            routeScatter<kBlock>(buf_, src.buffer, src.mask);
        }
    }

    // 3. PROCESS MODULES IN ORDER, PASSING EACH OUTPUT ON
    for (uint8_t m = 0; m < kRouteModuleCount; m++) {
        process_(m);
        if (m == NODE_REV) {
            if (patch_.moduleDest[m] & routeBit(NODE_MASTER)) {
                for (uint16_t i = 0; i < kBlock; i++) {
                    buf_.masterLeft[i] += buf_.nodeL[NODE_REV][i];
                    buf_.masterRight[i] += buf_.nodeR[NODE_REV][i];
                }
            }
        } else {
            // Only later nodes are allowed, so an output never lands in an already processed node.
            const uint8_t dest = patch_.moduleDest[m] & static_cast<uint8_t>(~((routeBit(m) << 1) - 1));
            routeFadeTarget(state_.module[m], dest);
            routeScatterPairFaded<kBlock>(buf_, buf_.nodeL[m], buf_.nodeR[m], state_.module[m]);
        }
    }
}

#endif
