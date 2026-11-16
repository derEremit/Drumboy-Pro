// Song mode: a chain of song banks played one after the other, each for a number of loops.
// Hardware-free so it can be tested on a PC (see 02 Software/tests).
//
// The sequencer calls chainLoopEnd() at every loop end while the chain plays; it says which bank
// to play next, or that the song is over. A bank picked by hand (chainManualBank) never turns the
// chain off: a bank the chain uses jumps the chain to its step (from the next loop end while
// playing, at once when stopped); a bank the chain doesn't use plays one loop as a detour, then
// the chain goes on from where it was (the detour loop doesn't count as one of its repeats).
// STOP keeps the position; RESET goes back to step 1 (chainRestart).
//
// File: song files (.rwf) get a chain block after their "EOF". Older firmware stops reading at
// "EOF", so the block needs no format version: it carries its own marker.
//   "CHN" | 1 (block version) | count | flags (bit 0 active, bit 1 loop) | 16 x (bank, repeats)
#ifndef __SONGCHAIN_H
#define __SONGCHAIN_H

#include <stdint.h>
#include <string.h>

const uint8_t kChainMaxSteps = 16;
const uint8_t kChainMaxRepeats = 16;
const uint8_t kChainBanks = 5;
const uint16_t kChainFileSize = 3 + 1 + 1 + 1 + 2 * kChainMaxSteps;  // 38

struct ChainStep {
    uint8_t bank;     // 0..4
    uint8_t repeats;  // 1..16 loops
};

struct SongChain {
    uint8_t count = 1;  // 1..16 steps in use
    ChainStep steps[kChainMaxSteps] = {{0, 1}};
    bool active = false;  // chain play on
    bool loop = true;     // after the last step: back to step 1 (true) or stop (false)
};

struct ChainPlay {
    uint8_t step = 0;        // step playing now
    uint8_t loopsDone = 0;   // loops of that step already finished
    bool skipCount = false;  // the loop now playing is not the step's own (a jump or a detour is under way)
    int8_t detour = -1;      // a bank to play for one loop from the next loop end, -1 = none
};

const int8_t kChainKeep = -1;  // stay on the current bank
const int8_t kChainStop = -2;  // the song is over: stop

struct SongChain;
struct ChainPlay;
inline int8_t chainCountLoop(const SongChain& c_, ChainPlay& p_);

inline void chainClear(SongChain& c_) { c_ = SongChain(); }

inline void chainRestart(const SongChain& c_, ChainPlay& p_) {
    (void)c_;
    p_.step = 0;
    p_.loopsDone = 0;
    p_.skipCount = false;
    p_.detour = -1;
}

inline uint8_t chainCurrentBank(const SongChain& c_, const ChainPlay& p_) {
    return c_.steps[(p_.step < c_.count) ? p_.step : 0].bank;
}

// One loop finished: returns the bank to switch to, kChainKeep, or kChainStop. After a jump or a
// detour it returns the bank to play even when it is the step's own (the caller compares it with
// the bank playing).
inline int8_t chainLoopEnd(const SongChain& c_, ChainPlay& p_) {
    if (c_.count == 0) return kChainKeep;
    if (p_.step >= c_.count) p_.step = 0;
    int8_t next;
    if (p_.skipCount) {
        // The loop that ended was a detour, or the old bank before a jump: don't count it.
        p_.skipCount = false;
        next = static_cast<int8_t>(c_.steps[p_.step].bank);
    } else {
        next = chainCountLoop(c_, p_);
        if (next == kChainStop) {
            p_.detour = -1;
            return kChainStop;
        }
        if ((next == kChainKeep) && (p_.detour >= 0)) next = static_cast<int8_t>(c_.steps[p_.step].bank);
    }
    if (p_.detour >= 0) {
        // A bank the chain doesn't use: one loop of it, then back to the chain without counting.
        next = p_.detour;
        p_.detour = -1;
        p_.skipCount = true;
    }
    return next;
}

// One loop of the current step counted (the chain's own rule).
inline int8_t chainCountLoop(const SongChain& c_, ChainPlay& p_) {
    if (p_.step >= c_.count) p_.step = 0;
    const uint8_t before = c_.steps[p_.step].bank;
    p_.loopsDone++;
    if (p_.loopsDone < c_.steps[p_.step].repeats) return kChainKeep;
    p_.loopsDone = 0;
    if (p_.step + 1 < c_.count) {
        p_.step++;
    } else if (c_.loop) {
        p_.step = 0;
    } else {
        p_.step = 0;  // ready to play again from the top
        return kChainStop;
    }
    const uint8_t next = c_.steps[p_.step].bank;
    return (next == before) ? kChainKeep : static_cast<int8_t>(next);
}

// Manual bank change: the first step using that bank, from the current step on (wrapping), so a
// bank used twice continues forward. -1 if the chain doesn't use it.
inline int8_t chainJumpToBank(const SongChain& c_, ChainPlay& p_, uint8_t bank_) {
    for (uint8_t k = 0; k < c_.count; k++) {
        const uint8_t s = static_cast<uint8_t>((p_.step + k) % c_.count);
        if (c_.steps[s].bank == bank_) {
            p_.step = s;
            p_.loopsDone = 0;
            return static_cast<int8_t>(s);
        }
    }
    return -1;
}

// A bank picked by hand (SONG + pads). Returns the step jumped to, or -1 for a detour.
// - A bank the chain uses: the chain goes to that step. Playing, the bank switches at the loop
//   end and that loop doesn't count; stopped, the caller switches at once and counting is normal.
// - A bank it doesn't use: playing, it plays for one loop from the loop end (the loop now playing
//   counts as usual, unless it is itself a detour or a jump), then the chain goes on; stopped, the
//   caller switches at once, it plays first, then the chain's current step.
inline int8_t chainManualBank(const SongChain& c_, ChainPlay& p_, uint8_t bank_, bool playing_) {
    const int8_t step = chainJumpToBank(c_, p_, bank_);
    if (step >= 0) {
        p_.detour = -1;
        p_.skipCount = playing_;
        return step;
    }
    if (playing_) {
        p_.detour = static_cast<int8_t>(bank_);
    } else {
        p_.detour = -1;
        p_.skipCount = true;
    }
    return -1;
}

// Editing. setCount grows by copying the last step; the play position stays inside the chain.
inline void chainSetCount(SongChain& c_, ChainPlay& p_, uint8_t count_) {
    if (count_ < 1) count_ = 1;
    if (count_ > kChainMaxSteps) count_ = kChainMaxSteps;
    for (uint8_t i = c_.count; i < count_; i++) c_.steps[i] = c_.steps[c_.count - 1];
    c_.count = count_;
    if (p_.step >= c_.count) chainRestart(c_, p_);
}

inline void chainSetStep(SongChain& c_, uint8_t step_, uint8_t bank_, uint8_t repeats_) {
    if (step_ >= c_.count) return;
    if (bank_ >= kChainBanks) bank_ = kChainBanks - 1;
    if (repeats_ < 1) repeats_ = 1;
    if (repeats_ > kChainMaxRepeats) repeats_ = kChainMaxRepeats;
    c_.steps[step_].bank = bank_;
    c_.steps[step_].repeats = repeats_;
}

// Total loops of the whole chain (for the screen).
inline uint16_t chainTotalLoops(const SongChain& c_) {
    uint16_t n = 0;
    for (uint8_t i = 0; i < c_.count; i++) n += c_.steps[i].repeats;
    return n;
}

inline void chainWrite(const SongChain& c_, uint8_t* out_) {
    memcpy(out_, "CHN", 3);
    out_[3] = 1;
    out_[4] = c_.count;
    out_[5] = static_cast<uint8_t>((c_.active ? 1 : 0) | (c_.loop ? 2 : 0));
    for (uint8_t i = 0; i < kChainMaxSteps; i++) {
        out_[6 + 2 * i] = c_.steps[i].bank;
        out_[7 + 2 * i] = c_.steps[i].repeats;
    }
}

// Returns false (and leaves the default chain) when the block is missing or not valid.
inline bool chainRead(SongChain& c_, const uint8_t* in_, uint16_t size_) {
    chainClear(c_);
    if ((size_ < kChainFileSize) || (memcmp(in_, "CHN", 3) != 0) || (in_[3] != 1)) return false;
    const uint8_t count = in_[4];
    if ((count < 1) || (count > kChainMaxSteps)) return false;
    SongChain c;
    c.count = count;
    c.active = (in_[5] & 1) != 0;
    c.loop = (in_[5] & 2) != 0;
    for (uint8_t i = 0; i < kChainMaxSteps; i++) {
        const uint8_t bank = in_[6 + 2 * i], repeats = in_[7 + 2 * i];
        if ((i < count) && ((bank >= kChainBanks) || (repeats < 1) || (repeats > kChainMaxRepeats))) return false;
        c.steps[i].bank = (bank < kChainBanks) ? bank : 0;
        c.steps[i].repeats = ((repeats >= 1) && (repeats <= kChainMaxRepeats)) ? repeats : 1;
    }
    c_ = c;
    return true;
}

#endif
