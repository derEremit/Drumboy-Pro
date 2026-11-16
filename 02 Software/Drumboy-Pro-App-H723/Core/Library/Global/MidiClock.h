// MIDI clock slave: keeps the sequencer locked to an incoming MIDI clock (24 pulses per quarter).
// Hardware-free so it can be tested on a PC (see 02 Software/tests).
//
// The sequencer runs on its own timer (kClockSeqTicksPerQuarter ticks per quarter note). Locked,
// it may only play as far as the incoming clock allows: after the k-th clock since START it may
// play the ticks before the position of clock k+1 (ceil(k * 800 / 24) ticks in all). Its timer
// is set at every clock so those ticks fit in about 97% of a clock interval; then it waits. So
// it can never run ahead of the master, it is never more than one clock behind, and a fractional
// tempo (e.g. 97.43 BPM) cannot drift. When the sequencer is already waiting as a clock arrives,
// the caller restarts its timer at once, which puts the tick on the clock edge.
//
// Tempo is measured from the clock intervals (smoothed). A clock that arrives far too early is
// treated as a stray duplicate (for example a MIDI loop echoing clock back) and ignored, unless
// the clock keeps coming that fast for a whole beat: then the master really sped up, and the
// clocks ignored meanwhile are counted after all. An interval of about 2 or 3 clocks means clock
// bytes were lost: the missing clocks are counted, so the position stays right; if the long
// intervals go on (the master really slowed down) those guesses are taken back.
//
// Concurrency on the device: clockLockTick runs in the MIDI receive interrupt, clockLockMayAdvance
// in the sequencer timer interrupt. Each 32-bit counter has a single writer.
#ifndef __MIDICLOCK_H
#define __MIDICLOCK_H

#include <stdint.h>

const uint32_t kClockSeqTicksPerQuarter = 800;  // kMeasureInterval
const uint32_t kClockPulsesPerQuarter = 24;
const float kClockFitShare = 0.97f;             // the ticks of one clock fit in 97% of it
const uint32_t kClockGapUs = 500000;            // no clock for 0.5 s: the clock has stopped
const float kClockEarlyShare = 0.6f;           // a clock sooner than 60% of the interval is suspect
const uint8_t kClockDoubleRun = 24;             // a whole beat of fast clocks = a real tempo change

enum ClockTickResult : uint8_t {
    CLOCK_REJECTED = 0,     // ignored (a stray early clock)
    CLOCK_ACCEPTED = 1,     // counted
    CLOCK_KICK = 2,         // counted, and the sequencer was waiting for it: restart its timer now
};

struct MidiClockLock {
    volatile uint32_t clocks = 0;   // clocks counted since the origin (START or engage)
    volatile uint32_t allowed = 0;  // sequencer ticks the clock allows since the origin
    volatile uint32_t done = 0;     // sequencer ticks played since the origin
    float unitsPerUs = 1.0f;        // timestamp units per microsecond (CPU cycles on the device)
    uint32_t last = 0;              // time of the last counted clock
    uint32_t lastRaw = 0;           // time of the last clock received, counted or not
    bool haveLast = false;
    float intervalUs = 0.0f;        // smoothed clock interval, 0 = unknown
    uint8_t fastRun = 0;            // consecutive clocks far too early
    uint8_t offRun = 0;             // consecutive intervals far from the average
    uint8_t guessed = 0;            // clocks counted as lost during the current offRun
    uint32_t rejected = 0;          // diagnostics
};

// Ticks allowed after k clocks: those before the position of clock k+1.
inline uint32_t clockAllowedFor(uint32_t clocks_) {
    return (clocks_ * kClockSeqTicksPerQuarter + kClockPulsesPerQuarter - 1) / kClockPulsesPerQuarter;
}

// A new origin (START, CONTINUE, or engaging while playing): position 0 plays on the next clock.
inline void clockLockRestart(MidiClockLock& l_) {
    l_.clocks = 0;
    l_.allowed = 0;
    l_.done = 0;
}

// Forget the timing history (the clock stopped or was switched off).
inline void clockLockForgetTiming(MidiClockLock& l_) {
    l_.haveLast = false;
    l_.fastRun = 0;
    l_.offRun = 0;
    l_.guessed = 0;
}

inline bool clockLockWaiting(const MidiClockLock& l_) { return l_.done >= l_.allowed; }

// A clock byte arrived at time now_ (free-running counter in unitsPerUs; wraps are fine).
inline ClockTickResult clockLockTick(MidiClockLock& l_, uint32_t now_) {
    if (l_.haveLast) {
        const float rawUs = static_cast<float>(now_ - l_.lastRaw) / l_.unitsPerUs;
        const float sinceUs = static_cast<float>(now_ - l_.last) / l_.unitsPerUs;
        if (sinceUs > static_cast<float>(kClockGapUs)) {
            clockLockForgetTiming(l_);  // a long pause: measure afresh
        } else if ((l_.intervalUs > 0.0f) && (rawUs < kClockEarlyShare * l_.intervalUs)) {
            l_.lastRaw = now_;
            if (++l_.fastRun < kClockDoubleRun) {
                l_.rejected++;
                return CLOCK_REJECTED;
            }
            l_.intervalUs = rawUs;  // the master really is that fast now: count the ignored clocks too
            l_.rejected -= static_cast<uint32_t>(l_.fastRun - 1);
            const uint32_t missed = static_cast<uint32_t>(l_.fastRun - 1);
            l_.fastRun = 0;
            l_.offRun = 0;
            l_.guessed = 0;
            l_.last = now_;
            const bool waiting = clockLockWaiting(l_);
            l_.clocks = l_.clocks + 1 + missed;
            l_.allowed = clockAllowedFor(l_.clocks);
            return waiting ? CLOCK_KICK : CLOCK_ACCEPTED;
        } else {
            l_.fastRun = 0;
            if (l_.intervalUs <= 0.0f) {
                l_.intervalUs = sinceUs;
            } else {
                const float ratio = sinceUs / l_.intervalUs;
                if ((ratio > 0.75f) && (ratio < 1.25f)) {
                    l_.offRun = 0;
                    l_.guessed = 0;
                    l_.intervalUs += (sinceUs - l_.intervalUs) * 0.125f;
                } else if (++l_.offRun >= 3) {
                    // A real tempo change: follow it, and take back the clocks guessed as lost.
                    l_.intervalUs = sinceUs;
                    l_.clocks = (l_.clocks > l_.guessed) ? (l_.clocks - l_.guessed) : 0;
                    l_.offRun = 0;
                    l_.guessed = 0;
                } else if ((ratio > 1.75f) && (ratio < 3.25f)) {
                    // One or two clock bytes lost: count them, so the position stays right.
                    const uint8_t lost = (ratio < 2.5f) ? 1 : 2;
                    l_.clocks = l_.clocks + lost;
                    l_.guessed = static_cast<uint8_t>(l_.guessed + lost);
                }
            }
        }
    }
    const bool waiting = clockLockWaiting(l_);
    l_.haveLast = true;
    l_.last = now_;
    l_.lastRaw = now_;
    l_.clocks = l_.clocks + 1;
    l_.allowed = clockAllowedFor(l_.clocks);
    return waiting ? CLOCK_KICK : CLOCK_ACCEPTED;
}

// Sequencer timer period (microseconds) that fits the allowed ticks into ~97% of a clock
// interval, catching up when behind. 0 = no tempo yet (keep the current period).
inline uint32_t clockLockPeriodUs(const MidiClockLock& l_, uint32_t minUs_, uint32_t maxUs_) {
    if (l_.intervalUs <= 0.0f) return 0;
    const uint32_t pending = (l_.allowed > l_.done) ? (l_.allowed - l_.done) : 1;
    float period = l_.intervalUs * kClockFitShare / static_cast<float>(pending);
    if (period < static_cast<float>(minUs_)) period = static_cast<float>(minUs_);
    if (period > static_cast<float>(maxUs_)) period = static_cast<float>(maxUs_);
    return static_cast<uint32_t>(period);
}

// Sequencer side: true if one more tick may play now (and counts it).
inline bool clockLockMayAdvance(MidiClockLock& l_) {
    if (l_.done >= l_.allowed) return false;
    l_.done = l_.done + 1;
    return true;
}

inline float clockLockBpm(const MidiClockLock& l_) {
    return (l_.intervalUs > 0.0f) ? 60000000.0f / (static_cast<float>(kClockPulsesPerQuarter) * l_.intervalUs) : 0.0f;
}

// MIDI clock interval (microseconds, rounded) for the clock/sync output timer.
inline uint32_t clockLockIntervalUs(const MidiClockLock& l_) {
    return static_cast<uint32_t>(l_.intervalUs + 0.5f);
}

#endif
