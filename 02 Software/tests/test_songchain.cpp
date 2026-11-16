// Host-side tests for song mode (Core/Library/Global/SongChain.h).

#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <utility>

#include "SongChain.h"

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

// Plays loops and returns the bank of each loop as digits 1-5, '.' after a stop.
static std::string play(const SongChain& c, ChainPlay& p, int loops) {
    std::string out;
    uint8_t bank = chainCurrentBank(c, p);
    bool stopped = false;
    for (int i = 0; i < loops; i++) {
        out += stopped ? '.' : static_cast<char>('1' + bank);
        if (stopped) continue;
        const int8_t r = chainLoopEnd(c, p);
        if (r == kChainStop) stopped = true;
        else if (r >= 0) bank = static_cast<uint8_t>(r);
    }
    return out;
}

static SongChain make(std::initializer_list<ChainStep> steps, bool loop) {
    SongChain c;
    c.count = 0;
    for (const auto& s : steps) c.steps[c.count++] = s;
    c.active = true;
    c.loop = loop;
    return c;
}

static void testPlayback() {
    ChainPlay p;
    const SongChain a = make({{0, 4}, {1, 2}, {2, 1}}, true);  // 1x4 2x2 3x1, loop
    CHECK(play(a, p, 14) == "11112231111223", "%s", play(a, p = ChainPlay(), 14).c_str());
    p = ChainPlay();
    const SongChain b = make({{0, 2}, {1, 1}}, false);  // 1x2 2x1, end
    CHECK(play(b, p, 6) == "112...", "end stops: %s", play(b, p = ChainPlay(), 6).c_str());
    CHECK(p.step == 0 && p.loopsDone == 0, "after the end it is ready to play from the top");
    p = ChainPlay();
    const SongChain same = make({{2, 1}, {2, 2}, {0, 1}}, true);  // same bank twice: no switch between them
    std::string s;
    uint8_t switches = 0;
    for (int i = 0; i < 8; i++) {
        const int8_t r = chainLoopEnd(same, p);
        if (r >= 0) switches++;
        s += static_cast<char>('0' + r + 2);
    }
    CHECK(switches == 4, "%u switches (%s)", switches, s.c_str());  // 3->1, 1->3 per lap, 2 laps
    p = ChainPlay();
    const SongChain one = make({{3, 1}}, true);
    CHECK(play(one, p, 5) == "44444", "single step loops on its bank");
    // Every repeats value: the step lasts exactly that many loops.
    for (uint8_t r = 1; r <= kChainMaxRepeats; r++) {
        p = ChainPlay();
        const SongChain c = make({{0, r}, {1, 1}}, true);
        const std::string out = play(c, p, r + 1);
        CHECK(out == std::string(r, '1') + "2", "repeats %u: %s", r, out.c_str());
    }
}

static void testJumpAndEdit() {
    SongChain c = make({{0, 4}, {1, 2}, {0, 1}, {2, 1}}, true);
    ChainPlay p;
    p.step = 1;
    p.loopsDone = 1;
    CHECK(chainJumpToBank(c, p, 0) == 2 && p.loopsDone == 0, "a bank used twice continues forward (step 3)");
    CHECK(chainJumpToBank(c, p, 2) == 3, "jump to bank 3");
    CHECK(chainJumpToBank(c, p, 4) == -1 && p.step == 3, "a bank the chain doesn't use: no jump");
    chainSetCount(c, p, 2);
    CHECK(c.count == 2 && p.step == 0, "shrinking past the play position restarts");
    chainSetCount(c, p, 5);
    CHECK(c.count == 5 && c.steps[4].bank == 1 && c.steps[4].repeats == 2, "growing copies the last step");
    chainSetCount(c, p, 40);
    CHECK(c.count == kChainMaxSteps, "max 16 steps");
    chainSetCount(c, p, 0);
    CHECK(c.count == 1, "min 1 step");
    chainSetStep(c, 0, 9, 0);
    CHECK(c.steps[0].bank == 4 && c.steps[0].repeats == 1, "values clamped");
    chainSetStep(c, 0, 2, 99);
    CHECK(c.steps[0].repeats == kChainMaxRepeats, "repeats clamped");
    c = make({{0, 4}, {1, 2}, {2, 1}}, true);
    CHECK(chainTotalLoops(c) == 7, "total loops");
}

// Plays loops like the firmware: the bank playing, switched at each loop end; manual_ = {loop index
// at which SONG + pad is pressed (during that loop), bank}.
static std::string playManual(const SongChain& c, ChainPlay& p, int loops, std::initializer_list<std::pair<int, int>> manual, uint8_t start = 0xFF) {
    std::string out;
    uint8_t bank = (start == 0xFF) ? chainCurrentBank(c, p) : start;
    for (int i = 0; i < loops; i++) {
        for (const auto& m : manual)
            if (m.first == i) chainManualBank(c, p, static_cast<uint8_t>(m.second), true);
        out += static_cast<char>('1' + bank);
        const int8_t r = chainLoopEnd(c, p);
        if (r == kChainStop) {
            out += '|';
            break;
        }
        if (r >= 0) bank = static_cast<uint8_t>(r);
    }
    return out;
}

static void testManual() {
    // Chain 1x4 2x2 (loop). Banks 3-5 are not in it.
    const SongChain c = make({{0, 4}, {1, 2}}, true);
    ChainPlay p;
    CHECK(playManual(c, p, 8, {}) == "11112211", "no manual: %s", "");

    // A detour while playing: bank 3 plays one loop after the current loop, then the chain goes on
    // where it was; the loop before the detour counts, the detour doesn't.
    p = ChainPlay();
    std::string s = playManual(c, p, 9, {{1, 2}});
    CHECK(s == "113112211", "detour in step 1: %s", s.c_str());
    p = ChainPlay();
    s = playManual(c, p, 9, {{3, 4}});  // pressed in the 4th loop of bank 1: the step ends, then 5, then bank 2
    CHECK(s == "111152211", "detour at the step's end: %s", s.c_str());
    // The chain stays on, and its position is kept.
    CHECK(c.active, "chain still on");

    // Two detours in a row: each plays one loop, neither counts.
    p = ChainPlay();
    s = playManual(c, p, 9, {{0, 2}, {1, 3}});
    CHECK(s == "134111221", "two detours: %s", s.c_str());

    // A jump while playing: bank 2 from the next loop end, the old bank's loop doesn't count,
    // then bank 2 plays its full 2 repeats.
    p = ChainPlay();
    s = playManual(c, p, 8, {{1, 1}});
    CHECK(s == "11221111", "jump: %s", s.c_str());
    // Jumping to the bank that is playing restarts its repeats.
    p = ChainPlay();
    s = playManual(c, p, 9, {{2, 0}});
    CHECK(s == "111111122", "jump to the same bank: %s", s.c_str());

    // Stopped: a detour bank is switched to at once by the caller, plays first, then the chain's step.
    p = ChainPlay();
    CHECK(chainManualBank(c, p, 4, false) == -1, "stopped detour");
    s = playManual(c, p, 7, {}, 4);
    CHECK(s == "5111122", "stopped detour plays first: %s", s.c_str());
    // Stopped jump: the step plays its full repeats.
    p = ChainPlay();
    CHECK(chainManualBank(c, p, 1, false) == 1, "stopped jump");
    s = playManual(c, p, 5, {}, 1);
    CHECK(s == "22111", "stopped jump: %s", s.c_str());

    // END = STOP: a detour requested in the last loop doesn't play; the song stops.
    const SongChain e = make({{0, 1}, {1, 1}}, false);
    p = ChainPlay();
    s = playManual(e, p, 5, {{1, 3}});
    CHECK(s == "12|", "stop wins: %s", s.c_str());
    CHECK(p.detour == -1 && !p.skipCount && p.step == 0, "clean after the stop");

    // RESET clears a pending detour or jump.
    p = ChainPlay();
    chainManualBank(c, p, 3, true);
    chainRestart(c, p);
    CHECK(p.detour == -1 && !p.skipCount, "restart clears");
}

static void testFile() {
    std::mt19937 rng(5);
    uint8_t buf[kChainFileSize];
    for (int n = 0; n < 2000; n++) {
        SongChain c;
        c.count = static_cast<uint8_t>(1 + rng() % kChainMaxSteps);
        for (uint8_t i = 0; i < kChainMaxSteps; i++) c.steps[i] = {static_cast<uint8_t>(rng() % kChainBanks), static_cast<uint8_t>(1 + rng() % kChainMaxRepeats)};
        c.active = rng() % 2;
        c.loop = rng() % 2;
        chainWrite(c, buf);
        SongChain d;
        const bool ok = chainRead(d, buf, sizeof buf);
        bool same = ok && d.count == c.count && d.active == c.active && d.loop == c.loop;
        for (uint8_t i = 0; same && i < c.count; i++) same = d.steps[i].bank == c.steps[i].bank && d.steps[i].repeats == c.steps[i].repeats;
        CHECK(same, "round trip %d", n);
    }
    SongChain d;
    memset(buf, 0, sizeof buf);
    CHECK(!chainRead(d, buf, sizeof buf) && !d.active && d.count == 1, "no block: chain off, one step");
    SongChain c = make({{0, 1}}, true);
    chainWrite(c, buf);
    CHECK(!chainRead(d, buf, kChainFileSize - 1), "short read refused");
    buf[6] = 7;  // bank out of range
    CHECK(!chainRead(d, buf, sizeof buf) && !d.active, "bad bank refused");
    chainWrite(c, buf);
    buf[4] = 0;
    CHECK(!chainRead(d, buf, sizeof buf), "zero steps refused");
    CHECK(kChainFileSize == 38, "block size");
}

int main() {
    testPlayback();
    testJumpAndEdit();
    testManual();
    testFile();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
