// Host-side tests for beat list editing (Core/Library/Global/BeatList.h).
//
// Every operation is checked against the bank invariants the sequencer relies on (active
// beats contiguous and sorted with unique starts, end = next start, each complex fill in its
// own slot with micro hits inside its beat) and against the beats' fill settings, which
// must survive moves, rotations and inserts.

#include <cstdio>
#include <cstring>
#include <map>
#include <random>
#include <vector>

#include "BeatList.h"

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

static const uint16_t kQuarter = 800;                                   // kMeasureInterval
static const uint16_t kSteps[] = {50, 100, 200, 400, 800, 1600, 3200};  // kQuantizeInterval
static std::mt19937 rng(1234);

static uint32_t rnd(uint32_t n) { return std::uniform_int_distribution<uint32_t>(0, n - 1)(rng); }

struct Setting {
    uint8_t type, pattern, time, level;
    bool operator==(const Setting& o) const { return type == o.type && pattern == o.pattern && time == o.time && level == o.level; }
};

static Setting settingOf(const Beat& b) { return {b.getFillType(), b.getFillPattern(), b.getFillTime(), b.getFillLevel()}; }

static bool isComplex(const Setting& s) { return !((s.type == 0) && (s.pattern <= 2)); }

static std::map<uint16_t, Setting> snapshot(const Bank& bank) {
    std::map<uint16_t, Setting> m;
    for (int16_t i = 0; i <= bank.lastActiveBeatNum; i++) m[bank.beatLibrary[i].getStartInterval()] = settingOf(bank.beatLibrary[i]);
    return m;
}

// The invariants playback depends on, plus: every fill equals a fresh computation for its beat.
static void checkBank(const Bank& bank, uint16_t song, const char* where) {
    const int16_t last = bank.lastActiveBeatNum;
    CHECK(last >= -1 && last < static_cast<int16_t>(kBeatLibrarySize), "%s: last %d", where, last);
    bool owned[kBeatFillLibrarySize] = {};
    for (int16_t i = 0; i < static_cast<int16_t>(kBeatLibrarySize); i++) {
        const Beat& b = bank.beatLibrary[i];
        if (i > last) {
            CHECK(!b.getActive() && b.getFillTarget() == NULL_FILL_TARGET, "%s: slot %d past last active", where, i);
            continue;
        }
        CHECK(b.getActive(), "%s: beat %d inactive", where, i);
        CHECK(b.getStartInterval() < song, "%s: beat %d start %u >= song %u", where, i, b.getStartInterval(), song);
        if (i > 0) CHECK(bank.beatLibrary[i - 1].getStartInterval() < b.getStartInterval(), "%s: beat %d not sorted", where, i);
        const uint16_t end = (i < last) ? bank.beatLibrary[i + 1].getStartInterval() : song;
        CHECK(b.getEndInterval() == end, "%s: beat %d end %u != %u", where, i, b.getEndInterval(), end);
        const Setting s = settingOf(b);
        const uint8_t t = b.getFillTarget();
        if (!isComplex(s)) {
            CHECK(t == NULL_FILL_TARGET, "%s: plain beat %d owns slot %u", where, i, t);
            continue;
        }
        CHECK(t < kBeatFillLibrarySize, "%s: complex beat %d has no slot", where, i);
        if (t >= kBeatFillLibrarySize) continue;
        CHECK(!owned[t], "%s: slot %u shared", where, t);
        owned[t] = true;
        const BeatFill& f = bank.beatFillLibrary[t];
        BeatFill fresh;
        fresh.set(b.getStartInterval(), b.getEndInterval(), s.type, s.pattern, s.time, s.level);
        CHECK(f.getActive() && f.getStepSize() == fresh.getStepSize(), "%s: beat %d fill header", where, i);
        for (uint8_t k = 0; k < f.getStepSize() && k < kBeatMicroLibrarySize; k++) {
            const BeatMicro& m = f.beatMicroLibrary[k];
            const BeatMicro& r = fresh.beatMicroLibrary[k];
            CHECK(m.getActive() == r.getActive() && m.interval == r.interval, "%s: beat %d micro %u stale", where, i, k);
            if (s.level == kFillLevelRandom) {  // drawn with rand(): only the range is known
                const FillPatternData& p = kFillPatternDataLibrary[s.type][s.pattern];
                CHECK(m.getLevel() >= p.levelLo && m.getLevel() <= p.levelHi, "%s: beat %d micro %u level %u", where, i, k, m.getLevel());
            } else {
                CHECK(m.getLevel() == r.getLevel(), "%s: beat %d micro %u level %u != %u", where, i, k, m.getLevel(), r.getLevel());
            }
            if (m.getActive()) CHECK(m.interval >= b.getStartInterval() && m.interval <= end, "%s: beat %d micro %u outside beat", where, i, k);
        }
    }
    for (uint8_t t = 0; t < kBeatFillLibrarySize; t++) {
        if (!owned[t]) CHECK(!bank.beatFillLibrary[t].getActive(), "%s: orphan fill slot %u", where, t);
    }
}

static uint16_t randomSong() { return static_cast<uint16_t>(kQuarter * (1 + rnd(8)) * (1 + rnd(8))); }

static Setting randomSetting() {
    if (rnd(3) == 0) return {0, static_cast<uint8_t>(rnd(3)), 0, 0};
    return {static_cast<uint8_t>(rnd(kFillTypeLibrarySize)), static_cast<uint8_t>(rnd(kFillPatternLibrarySize)), static_cast<uint8_t>(rnd(kMaxFillTime + 1)), static_cast<uint8_t>(rnd(kMaxFillLevel + 1))};
}

// A valid bank with up to `count` beats on a random grid and random fills.
static void randomBank(Bank& bank, uint16_t song, uint8_t count) {
    bank = Bank();
    const uint16_t grid = kSteps[rnd(4)];
    for (uint8_t i = 0; i < count; i++) beatListAdd(bank, static_cast<uint16_t>(rnd(song / grid) * grid), song);
    beatListRebuild(bank, song);
    for (int16_t i = 0; i <= bank.lastActiveBeatNum; i++) {
        const Setting s = randomSetting();
        bank.setBeatFill(static_cast<uint8_t>(i), s.type, s.pattern, s.time, s.level);
    }
    beatListRebuild(bank, song);
}

static bool sameBank(const Bank& a, const Bank& b) {
    if (a.lastActiveBeatNum != b.lastActiveBeatNum) return false;
    for (uint8_t i = 0; i < kBeatLibrarySize; i++) {
        const Beat &x = a.beatLibrary[i], &y = b.beatLibrary[i];
        if (x.data != y.data || x.fillTarget != y.fillTarget || x.startInterval != y.startInterval || x.endInterval != y.endInterval) return false;
    }
    for (uint8_t t = 0; t < kBeatFillLibrarySize; t++) {
        const BeatFill &x = a.beatFillLibrary[t], &y = b.beatFillLibrary[t];
        if (x.data != y.data) return false;
        for (uint8_t k = 0; k < kBeatMicroLibrarySize; k++) {
            if (x.beatMicroLibrary[k].data != y.beatMicroLibrary[k].data || x.beatMicroLibrary[k].interval != y.beatMicroLibrary[k].interval) return false;
        }
    }
    return true;
}

static void testRebuild() {
    for (int n = 0; n < 2000; n++) {
        const uint16_t song = randomSong();
        Bank bank;
        // Beats scattered over random slots, unsorted, some sharing a start.
        std::vector<uint16_t> starts;
        for (uint8_t i = 0; i < kBeatLibrarySize; i++) {
            if (rnd(2)) continue;
            Beat& b = bank.beatLibrary[i];
            const uint16_t start = static_cast<uint16_t>(rnd(song / 50) * 50);
            b.setActive(true);
            b.setStartInterval(start);
            starts.push_back(start);
        }
        bank.lastActiveBeatNum = kBeatLibrarySize - 1;
        beatListRebuild(bank, song);
        checkBank(bank, song, "rebuild");
        std::map<uint16_t, int> unique;
        for (uint16_t s : starts) unique[s]++;
        CHECK(bank.lastActiveBeatNum + 1 == static_cast<int>(unique.size()), "rebuild kept %d of %zu starts", bank.lastActiveBeatNum + 1, unique.size());
    }
    Bank empty;
    beatListRebuild(empty, 3200);
    CHECK(empty.lastActiveBeatNum == -1, "empty bank stays empty");
}

// Reference for a move: the next free grid position in that direction, wrapping.
static int32_t expectedMove(const std::map<uint16_t, Setting>& before, uint16_t start, bool forward, uint16_t step, uint16_t song) {
    uint16_t t = start;
    for (uint32_t k = 0; k <= song / step + 1u; k++) {
        if (forward) {
            t = (static_cast<uint32_t>(t / step + 1) * step < song) ? static_cast<uint16_t>((t / step + 1) * step) : 0;
        } else if (t == 0) {
            t = static_cast<uint16_t>(((song - 1) / step) * step);
        } else {
            t = (t % step) ? static_cast<uint16_t>((t / step) * step) : static_cast<uint16_t>(t - step);
        }
        if (t == start) return -1;
        if (!before.count(t)) return t;
    }
    return -1;
}

static void testMove() {
    for (int n = 0; n < 20000; n++) {
        const uint16_t song = randomSong();
        Bank bank;
        randomBank(bank, song, static_cast<uint8_t>(1 + rnd(kBeatLibrarySize)));
        const auto before = snapshot(bank);
        const uint8_t beatNum = static_cast<uint8_t>(rnd(bank.lastActiveBeatNum + 1));
        const uint16_t start = bank.beatLibrary[beatNum].getStartInterval();
        const Setting moved = settingOf(bank.beatLibrary[beatNum]);
        const bool forward = rnd(2);
        const uint16_t step = kSteps[rnd(7)];
        const int32_t expect = expectedMove(before, start, forward, step, song);
        const int16_t idx = beatListMove(bank, beatNum, forward, step, song);
        checkBank(bank, song, "move");
        if (expect < 0) {
            CHECK(idx == -1, "move should be refused");
            CHECK(snapshot(bank) == before, "refused move changed the bank");
            continue;
        }
        CHECK(idx >= 0 && bank.beatLibrary[idx].getStartInterval() == expect, "moved to %u, expected %d", idx >= 0 ? bank.beatLibrary[idx].getStartInterval() : 0, expect);
        // Same beats with the same settings, except the moved one changed position. A complex
        // fill only falls back to plain if no slot was free, which a move can't cause.
        auto after = snapshot(bank);
        auto want = before;
        want.erase(start);
        want[static_cast<uint16_t>(expect)] = moved;
        CHECK(after == want, "settings changed by a move");
    }

    // Jumps over neighbours: beats at 0, 200, 400; moving 0 forward by 200 lands on 600.
    Bank bank;
    for (uint16_t s : {0, 200, 400}) beatListAdd(bank, s, 3200);
    beatListRebuild(bank, 3200);
    bank.setBeatFill(0, 1, 0, 0, 0);  // WAVE_A
    int16_t idx = beatListMove(bank, 0, true, 200, 3200);
    CHECK(idx == 2 && bank.beatLibrary[2].getStartInterval() == 600 && bank.beatLibrary[2].getFillType() == 1, "jump over neighbours keeps fill");
    checkBank(bank, 3200, "jump");

    // Wraps at the song end, both ways.
    idx = beatListMove(bank, 2, true, 3200, 3200);  // 600 -> 3200 = song end -> 0, taken -> 200, ... -> refused?
    checkBank(bank, 3200, "wrap");
    Bank w;
    beatListAdd(w, 3000, 3200);
    beatListRebuild(w, 3200);
    CHECK(beatListMove(w, 0, true, 200, 3200) == 0 && w.beatLibrary[0].getStartInterval() == 0, "forward wrap to 0");
    CHECK(beatListMove(w, 0, false, 200, 3200) == 0 && w.beatLibrary[0].getStartInterval() == 3000, "back wrap to 3000");

    // Off-grid beats snap to the grid line in the direction of travel.
    Bank o;
    beatListAdd(o, 130, 3200);
    beatListRebuild(o, 3200);
    CHECK(beatListMove(o, 0, true, 200, 3200) == 0 && o.beatLibrary[0].getStartInterval() == 200, "off-grid forward");
    beatListRebuild(o, 3200);
    o.beatLibrary[0].setStartInterval(130);
    CHECK(beatListMove(o, 0, false, 200, 3200) == 0 && o.beatLibrary[0].getStartInterval() == 0, "off-grid back");

    // Every grid position taken: refused, bank unchanged.
    Bank full;
    for (uint16_t s = 0; s < 800; s += 200) beatListAdd(full, s, 800);
    beatListRebuild(full, 800);
    const auto fullBefore = snapshot(full);
    CHECK(beatListMove(full, 1, true, 200, 800) == -1 && snapshot(full) == fullBefore, "no free position");
}

static void testRotate() {
    for (int n = 0; n < 5000; n++) {
        const uint16_t song = randomSong();
        Bank bank;
        randomBank(bank, song, static_cast<uint8_t>(rnd(kBeatLibrarySize + 1)));
        const Bank original = bank;
        const auto before = snapshot(bank);
        const int32_t delta = (rnd(2) ? 1 : -1) * static_cast<int32_t>(kSteps[rnd(7)] * (1 + rnd(4)));
        beatListRotate(bank, delta, song);
        checkBank(bank, song, "rotate");
        std::map<uint16_t, Setting> want;
        for (const auto& kv : before) want[static_cast<uint16_t>(((kv.first + delta) % song + song) % song)] = kv.second;
        CHECK(snapshot(bank) == want, "rotation moved settings wrongly");
        beatListRotate(bank, -delta, song);
        CHECK(sameBank(bank, original), "rotate %d then back is not identical (song %u)", delta, song);
    }
}

static void testInsert() {
    Bank bank;
    CHECK(beatListInsertAfter(bank, -1, 200, 3200) == 0 && bank.beatLibrary[0].getStartInterval() == 0, "empty bank inserts at 0");
    CHECK(beatListInsertAfter(bank, 0, 200, 3200) == 1 && bank.beatLibrary[1].getStartInterval() == 1600, "middle of a whole loop");
    CHECK(beatListInsertAfter(bank, 0, 200, 3200) == 1 && bank.beatLibrary[1].getStartInterval() == 800, "middle of first half");
    CHECK(beatListInsertAfter(bank, 2, 200, 3200) == 3 && bank.beatLibrary[3].getStartInterval() == 2400, "gap after last runs to the next loop");
    checkBank(bank, 3200, "insert");

    // Gap after the last beat wraps past the song end.
    Bank wrap;
    for (uint16_t s : {400, 2800}) beatListAdd(wrap, s, 3200);
    beatListRebuild(wrap, 3200);
    CHECK(beatListInsertAfter(wrap, 1, 200, 3200) == 0 && wrap.beatLibrary[0].getStartInterval() == 0, "wrapped gap 2800..3600 -> 0");
    checkBank(wrap, 3200, "insert wrap");

    // No grid position in the gap.
    Bank tight;
    for (uint16_t s : {0, 50}) beatListAdd(tight, s, 3200);
    beatListRebuild(tight, 3200);
    CHECK(beatListInsertAfter(tight, 0, 50, 3200) == -1, "gap of one step");
    CHECK(beatListInsertAfter(tight, 0, 200, 3200) == -1, "gap shorter than a step");

    // Full bank.
    Bank full;
    for (uint16_t i = 0; i < kBeatLibrarySize; i++) beatListAdd(full, static_cast<uint16_t>(i * 50), 6400);
    beatListRebuild(full, 6400);
    CHECK(full.lastActiveBeatNum == kBeatLibrarySize - 1, "bank filled");
    CHECK(beatListInsertAfter(full, kBeatLibrarySize - 1, 50, 6400) == -1, "full bank refuses");
    CHECK(!beatListAdd(full, 6000, 6400), "full bank refuses add");
    checkBank(full, 6400, "full");

    // Random inserts keep everyone's fill, and the neighbour's fill is recomputed.
    for (int n = 0; n < 10000; n++) {
        const uint16_t song = randomSong();
        Bank b;
        randomBank(b, song, static_cast<uint8_t>(rnd(kBeatLibrarySize)));
        const auto before = snapshot(b);
        const int16_t sel = (b.lastActiveBeatNum < 0) ? -1 : static_cast<int16_t>(rnd(b.lastActiveBeatNum + 1));
        const int16_t idx = beatListInsertAfter(b, sel, kSteps[rnd(7)], song);
        checkBank(b, song, "random insert");
        auto after = snapshot(b);
        if (idx < 0) {
            CHECK(after == before, "refused insert changed the bank");
            continue;
        }
        const uint16_t at = b.beatLibrary[idx].getStartInterval();
        CHECK(!before.count(at) && after.count(at) && !isComplex(after[at]), "new beat is plain and new");
        after.erase(at);
        CHECK(after == before, "insert changed other beats' settings");
    }
}

static void testPosition() {
    char s[16];
    beatListPosition(0, 3200, kQuarter, s, sizeof s);
    CHECK(!strcmp(s, "1.1.1"), "%s", s);
    beatListPosition(3200 + 2 * 800 + 3 * 200, 3200, kQuarter, s, sizeof s);
    CHECK(!strcmp(s, "2.3.4"), "%s", s);
    beatListPosition(3200 + 2 * 800 + 3 * 200 + 50, 3200, kQuarter, s, sizeof s);
    CHECK(!strcmp(s, "2.3.4+1"), "%s", s);
    beatListPosition(2400 + 800, 2400, kQuarter, s, sizeof s);  // 3/4
    CHECK(!strcmp(s, "2.2.1"), "%s", s);

    beatListOffset(0, 3200, kQuarter, s, sizeof s);
    CHECK(!strcmp(s, "0.0.0"), "%s", s);
    beatListOffset(800 + 400, 3200, kQuarter, s, sizeof s);
    CHECK(!strcmp(s, "+0.1.2"), "%s", s);
    beatListOffset(-(3200 + 50), 3200, kQuarter, s, sizeof s);
    CHECK(!strcmp(s, "-1.0.0+1"), "%s", s);
    beatListOffset(-51200, 3200, kQuarter, s, sizeof s);
    CHECK(!strcmp(s, "-16.0.0") && strlen(s) <= 10, "%s fits a 10-char field", s);

    CHECK(beatListWrapShift(200, 3200) == 200, "small shift");
    CHECK(beatListWrapShift(3000, 3200) == -200, "short way round");
    CHECK(beatListWrapShift(-3000, 3200) == 200, "short way round, negative");
    CHECK(beatListWrapShift(3200, 3200) == 0, "full turn");
    CHECK(beatListWrapShift(1600, 3200) == 1600 && beatListWrapShift(-1600, 3200) == 1600, "half turn is positive");
}

int main() {
    testRebuild();
    testMove();
    testRotate();
    testInsert();
    testPosition();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
