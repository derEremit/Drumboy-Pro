// Host-side tests for MIDI input (MidiParser.h) and the MIDI clock slave lock (MidiClock.h).

#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

#include "MidiClock.h"
#include "MidiParser.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        ++checks;                                                           \
        if (!(cond)) {                                                      \
            ++failures;                                                     \
            std::printf("FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond);     \
            std::printf(__VA_ARGS__);                                       \
            std::printf("\n");                                              \
        }                                                                   \
    } while (0)

static std::vector<MidiEvent> feed(MidiParser& p, std::initializer_list<int> bytes) {
    std::vector<MidiEvent> out;
    for (int b : bytes) {
        MidiEvent e;
        if (midiParse(p, static_cast<uint8_t>(b), e)) out.push_back(e);
    }
    return out;
}

static bool isNote(const MidiEvent& e, uint8_t type, uint8_t ch, uint8_t key, uint8_t vel) {
    return e.type == type && e.channel == ch && e.data1 == key && e.data2 == vel;
}

static void testParser() {
    MidiParser p;
    auto ev = feed(p, {0x90, 36, 100});
    CHECK(ev.size() == 1 && isNote(ev[0], MIDI_EVENT_NOTE_ON, 0, 36, 100), "plain note on");

    // Running status: one status byte, then key/velocity pairs.
    p = MidiParser();
    ev = feed(p, {0x92, 36, 100, 38, 64, 40, 1});
    CHECK(ev.size() == 3 && isNote(ev[1], MIDI_EVENT_NOTE_ON, 2, 38, 64) && isNote(ev[2], MIDI_EVENT_NOTE_ON, 2, 40, 1), "running status: %zu", ev.size());

    // Velocity 0 is a note off, also under running status.
    ev = feed(p, {36, 0});
    CHECK(ev.size() == 1 && isNote(ev[0], MIDI_EVENT_NOTE_OFF, 2, 36, 0), "velocity 0 = note off");

    // Clock bytes in the middle of a note don't break it or the running status.
    p = MidiParser();
    ev = feed(p, {0x90, 0xF8, 36, 0xF8, 100, 0xF8, 38, 0xFA, 90});
    int clocks = 0, notes = 0;
    for (auto& e : ev) {
        if (e.type == MIDI_EVENT_REALTIME && e.data1 == 0xF8) clocks++;
        if (e.type == MIDI_EVENT_NOTE_ON) notes++;
    }
    CHECK(clocks == 3 && notes == 2 && ev.back().type == MIDI_EVENT_NOTE_ON && ev.back().data1 == 38, "interleaved clock: %d clocks %d notes", clocks, notes);
    CHECK(ev[3].type == MIDI_EVENT_REALTIME && ev[3].data1 == 0xF8, "real-time first, at once");

    // A chord: three notes in a row all arrive (the old parser kept only one).
    p = MidiParser();
    ev = feed(p, {0x90, 36, 100, 0x90, 38, 100, 0x90, 42, 100});
    CHECK(ev.size() == 3, "chord");

    // Note off with its own status, and CC.
    ev = feed(p, {0x80, 36, 64, 0xB1, 7, 90});
    CHECK(ev.size() == 2 && isNote(ev[0], MIDI_EVENT_NOTE_OFF, 0, 36, 64) && ev[1].type == MIDI_EVENT_CC && ev[1].channel == 1 && ev[1].data2 == 90, "note off and CC");

    // Program change has one data byte; running status continues with single bytes.
    ev = feed(p, {0xC0, 5, 6});
    CHECK(ev.size() == 2 && ev[0].type == MIDI_EVENT_OTHER && ev[1].data1 == 6, "program change");

    // System common cancels running status: its data bytes are not read as notes.
    p = MidiParser();
    ev = feed(p, {0x90, 36, 100, 0xF2, 0, 0, 38, 100});
    CHECK(ev.size() == 1, "song position pointer cancels running status: %zu", ev.size());

    // Data bytes before any status are ignored.
    p = MidiParser();
    ev = feed(p, {36, 100, 0x90, 36, 100});
    CHECK(ev.size() == 1, "leading data ignored");

    // The Drumboy's SysEx transport commands; foreign SysEx is skipped, notes after it work.
    p = MidiParser();
    ev = feed(p, {0xF0, 0x7D, 0x52, 0x12, 0xF7});
    CHECK(ev.size() == 1 && ev[0].type == MIDI_EVENT_SYSEX_CMD && ev[0].data1 == 0x12, "sysex command");
    ev = feed(p, {0xF0, 0x41, 0x10, 0x42, 0x12, 0x00, 0xF7, 0x90, 36, 100});
    CHECK(ev.size() == 1 && ev[0].type == MIDI_EVENT_NOTE_ON, "foreign sysex skipped");
    ev = feed(p, {0xF0, 0x7D, 0x52, 0x12, 0x00, 0xF7});
    CHECK(ev.empty(), "long sysex with our header is not a command");
    ev = feed(p, {0xF0, 0x7D, 0xF8, 0x52, 0x11, 0xF8, 0xF7});
    CHECK(ev.size() == 3 && ev[2].type == MIDI_EVENT_SYSEX_CMD && ev[2].data1 == 0x11, "clock inside sysex");

    // Every channel.
    for (int ch = 0; ch < 16; ch++) {
        p = MidiParser();
        ev = feed(p, {0x90 + ch, 40, 77});
        CHECK(ev.size() == 1 && ev[0].channel == ch, "channel %d", ch);
    }
}

static void testQueue() {
    MidiQueue<16> q;
    MidiEvent e{MIDI_EVENT_NOTE_ON, 0, 0, 100};
    for (int i = 0; i < 20; i++) {
        e.data1 = static_cast<uint8_t>(i);
        q.push(e);
    }
    CHECK(q.dropped == 4, "full queue drops new events: %u", q.dropped);
    MidiEvent out;
    int n = 0;
    bool order = true;
    while (q.pop(out)) order = order && (out.data1 == n++);
    CHECK(n == 16 && order, "fifo order");
    // Wraparound of the 8-bit indices.
    for (int i = 0; i < 1000; i++) {
        e.data1 = static_cast<uint8_t>(i & 0x7F);
        q.push(e);
        CHECK(q.pop(out) && out.data1 == (i & 0x7F), "wrap %d", i);
        if (failures) break;
    }
}

static void testVelocity() {
    CHECK(midiVelocityToLevel(127) == 100 && midiVelocityToLevel(0) == 0 && midiVelocityToLevel(1) == 1, "velocity to level ends");
    for (int v = 1; v <= 127; v++) {
        const uint8_t level = midiVelocityToLevel(static_cast<uint8_t>(v));
        CHECK(level >= 1 && level <= 100, "level %d", v);
        CHECK(std::abs(midiLevelToVelocity(level) - v) <= 1, "round trip %d -> %u -> %u", v, level, midiLevelToVelocity(level));
    }
    CHECK(midiLevelToVelocity(100) == 127 && midiLevelToVelocity(50) == 64 && midiLevelToVelocity(0) == 0, "FLAT 127, GHOST 64");
    for (int l = 0; l <= 100; l++) {
        const float mult = std::pow(l / 100.0f, 3.0f);
        CHECK(midiMultiplierToVelocity(mult) == midiLevelToVelocity(static_cast<uint8_t>(l)), "multiplier of level %d", l);
    }
}

// --- Clock lock simulation -------------------------------------------------------------------
// The sequencer timer: fires every `period` us; a new period (ARR with preload) takes effect at
// the next update; a kick restarts it at once.
struct Sim {
    MidiClockLock lock;
    double nextFire = 0, period = 625, pending = 625;
    std::vector<double> tickTimes;  // time of each sequencer tick played
    void runUntil(double t) {
        while (nextFire <= t) fire(nextFire);
    }
    void fire(double t) {
        period = pending;
        if (clockLockMayAdvance(lock)) tickTimes.push_back(t);
        nextFire = t + period;
    }
    // Returns the result; times in microseconds (unitsPerUs = 1).
    ClockTickResult clock(double t) {
        runUntil(t - 1e-9);
        const ClockTickResult r = clockLockTick(lock, static_cast<uint32_t>(static_cast<uint64_t>(t) & 0xFFFFFFFFu));
        if (r != CLOCK_REJECTED) {
            const uint32_t p = clockLockPeriodUs(lock, 40, 20000);
            if (p) pending = p;
            if (r == CLOCK_KICK) fire(t);
        }
        return r;
    }
};

// Worst early/late of tick k against its ideal time (clock 1 = tick 0).
struct Err {
    double early = 0, late = 0;
};
static Err tickError(const Sim& s, double t0, double clockUs, size_t from = 0) {
    Err e;
    const double tickUs = clockUs * 24.0 / 800.0;
    for (size_t k = from; k < s.tickTimes.size(); k++) {
        const double d = s.tickTimes[k] - (t0 + k * tickUs);
        if (-d > e.early) e.early = -d;
        if (d > e.late) e.late = d;
    }
    return e;
}

static void testClockSteady() {
    // 97.43 BPM (Loopy Pro style fractional tempo), 10 minutes, +-1 ms jitter.
    const double bpm = 97.43, clockUs = 60e6 / (24 * bpm);
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> jit(-1000, 1000);
    Sim s;
    const int clocks = static_cast<int>(600e6 / clockUs);
    double t0 = 0;
    for (int n = 0; n < clocks; n++) {
        const double t = 1000 + n * clockUs + jit(rng);
        if (n == 0) t0 = t;
        s.clock(t);
    }
    // From the second clock on (the first interval runs at the Drumboy's own tempo: unknown yet).
    const double tickUs = clockUs / (800.0 / 24.0);
    const Err e = tickError(s, t0, clockUs, 34);
    CHECK(e.early < tickUs + 0.04 * clockUs + 2000, "never runs ahead: %.0f us early", e.early);
    CHECK(e.late < clockUs + 2000, "never more than one clock behind: %.0f us late", e.late);
    const double expected = (clocks - 1) * 800.0 / 24.0;
    CHECK(std::fabs(s.tickTimes.size() - expected) <= 34, "no drift after 10 min: %zu ticks vs %.0f", s.tickTimes.size(), expected);
    CHECK(std::fabs(clockLockBpm(s.lock) - bpm) < 0.2, "tempo %.2f", clockLockBpm(s.lock));
    CHECK(s.lock.rejected == 0, "jitter is not rejected: %u", s.lock.rejected);
}

static void testClockEcho() {
    // Every clock comes twice (a MIDI loop): the duplicates 3 ms later are ignored.
    const double clockUs = 60e6 / (24 * 120.0);
    Sim s;
    double t0 = 1000;
    for (int n = 0; n < 24 * 200; n++) {
        const double t = 1000 + n * clockUs;
        s.clock(t);
        if (n > 0) s.clock(t + 3000);
    }
    CHECK(std::fabs(clockLockBpm(s.lock) - 120.0) < 0.5, "echo does not double the tempo: %.1f", clockLockBpm(s.lock));
    CHECK(s.lock.clocks == 24 * 200u, "echoed clocks not counted: %u", s.lock.clocks);
    const Err e = tickError(s, t0, clockUs, 34);
    CHECK(e.late < 100 && e.early < clockUs / (800.0 / 24.0) + 0.04 * clockUs, "in time with an echo: %.0f/%.0f", e.early, e.late);
}

static void testClockLost() {
    // Every 500th clock byte is lost: the gap is counted, so the position does not slip.
    const double clockUs = 60e6 / (24 * 128.0);
    Sim s;
    for (int n = 0; n < 24 * 300; n++) {
        if (n % 500 == 499) continue;
        s.clock(1000 + n * clockUs);
    }
    CHECK(s.lock.clocks == 24 * 300u, "lost clocks counted: %u", s.lock.clocks);
    const Err e = tickError(s, 1000, clockUs, 34);
    CHECK(e.late < 2 * clockUs + 100, "lost clock: at most briefly behind: %.0f", e.late);
    CHECK(std::fabs(s.tickTimes.size() - (24 * 300 - 1) * 800.0 / 24.0) <= 34, "no slip: %zu", s.tickTimes.size());
}

static void testClockTempoChanges() {
    // 120 -> 140 -> 70 -> 140 (a real halving and doubling) -> 100: position follows the count.
    Sim s;
    double t = 1000;
    const double tempos[] = {120, 140, 70, 140, 100};
    int total = 0;
    for (double bpm : tempos) {
        const double clockUs = 60e6 / (24 * bpm);
        for (int n = 0; n < 24 * 32; n++, total++) {
            s.clock(t);
            t += clockUs;
        }
    }
    CHECK(s.lock.clocks == static_cast<uint32_t>(total), "every clock counted across tempo changes: %u of %d", s.lock.clocks, total);
    CHECK(std::fabs(clockLockBpm(s.lock) - 100.0) < 0.5, "follows the last tempo: %.1f", clockLockBpm(s.lock));
    s.runUntil(t + 1e6);
    CHECK(s.tickTimes.size() == clockAllowedFor(s.lock.clocks), "caught up: %zu of %u", s.tickTimes.size(), clockAllowedFor(s.lock.clocks));
}

static void testClockStartAndGap() {
    MidiClockLock l;
    CHECK(clockLockWaiting(l) && !clockLockMayAdvance(l), "nothing plays before the first clock");
    CHECK(clockLockTick(l, 0) == CLOCK_KICK && l.allowed == 34, "first clock allows ticks 0-33");
    for (int i = 0; i < 34; i++) clockLockMayAdvance(l);
    CHECK(!clockLockMayAdvance(l), "waits at the next clock's position");
    CHECK(clockLockTick(l, 20833) == CLOCK_KICK && l.allowed == 67, "second clock");
    CHECK(clockAllowedFor(24) == 800 && clockAllowedFor(3) == 100, "one beat = 800 ticks");
    // A pause longer than 0.5 s: timing measured afresh, no false lost-clock guesses.
    const uint32_t c = l.clocks;
    clockLockTick(l, 20833 + 2000000);
    CHECK(l.clocks == c + 1, "a pause is not lost clocks");
    // Restart.
    clockLockRestart(l);
    CHECK(l.clocks == 0 && l.allowed == 0 && l.done == 0, "restart");
    // Wrapping timestamps (32-bit counter).
    MidiClockLock w;
    uint32_t now = 0xFFFFFFFFu - 50000;
    for (int i = 0; i < 100; i++, now += 20833) clockLockTick(w, now);
    CHECK(std::fabs(clockLockBpm(w) - 120.0) < 0.1 && w.rejected == 0, "timestamps wrap: %.2f", clockLockBpm(w));
}

int main() {
    // MIDI thru of clock and transport: both sides must have the matching flag.
    CHECK(midiThruForwards(0xF8, true, true, true, true), "clock passes with SYNC on both sides");
    CHECK(!midiThruForwards(0xF8, true, true, false, false), "no clock out, no clock thru");
    CHECK(!midiThruForwards(0xF8, false, false, true, true), "no clock in, nothing to pass");
    for (uint8_t b : {0xFA, 0xFB, 0xFC, 0xFF}) {
        CHECK(midiThruForwards(b, true, true, true, true), "transport byte %02X passes", b);
        CHECK(!midiThruForwards(b, true, false, true, true), "transport byte %02X: transport in off", b);
        CHECK(!midiThruForwards(b, true, true, true, false), "transport byte %02X: transport out off", b);
    }
    CHECK(!midiThruForwards(0xFE, true, true, true, true) && !midiThruForwards(0x90, true, true, true, true), "only clock and transport are passed");

    testParser();
    testQueue();
    testVelocity();
    testClockSteady();
    testClockEcho();
    testClockLost();
    testClockTempoChanges();
    testClockStartAndGap();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
