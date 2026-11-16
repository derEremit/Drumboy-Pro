// Host-side tests for the fill engine (Core/Library/Global/Fill.h).
// Build and run: make -C "02 Software/tests"
//
// Checks the fill tables, then runs every type x pattern x time x level combination
// through Bank::setBeatFill and checks the resulting micro-hits, plus a few cases worked
// out by hand from the firmware code.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Fill.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...)                                       \
    do {                                                       \
        ++checks;                                              \
        if (!(cond)) {                                         \
            ++failures;                                        \
            std::printf("FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond); \
            std::printf(__VA_ARGS__);                          \
            std::printf("\n");                                 \
        }                                                      \
    } while (0)

static const uint8_t kLevelHi = 100, kLevelLo = 40, kLevelStep = FillPatternData::levelStep;

struct Hit {
    uint16_t interval;
    uint8_t level;
};

// Put one beat [start, end) in a fresh bank, apply a fill, and return its micro-hits.
static std::vector<Hit> runFill(Bank& bank, uint16_t start, uint16_t end, uint8_t type, uint8_t pattern, uint8_t time, uint8_t level, uint8_t* targetOut = nullptr) {
    bank.setBeat(0, start, end);
    uint8_t target = bank.setBeatFill(0, type, pattern, time, level);
    if (targetOut) *targetOut = target;
    std::vector<Hit> hits;
    if (target == NULL_FILL_TARGET) return hits;
    const BeatFill& f = bank.beatFillLibrary[target];
    for (uint8_t i = 0; i < f.getStepSize(); i++) hits.push_back({f.beatMicroLibrary[i].getInterval(), f.beatMicroLibrary[i].getLevel()});
    return hits;
}

static bool isBasic(uint8_t type, uint8_t pattern) { return type == 0 && pattern <= 2; }

static void testTables() {
    for (uint8_t t = 0; t < kFillTypeLibrarySize; t++) {
        CHECK(std::strlen(kFillTypeDataLibrary[t].nameLong) == 10, "type %u name '%s' must be 10 chars", t, kFillTypeDataLibrary[t].nameLong);
        for (uint8_t p = 0; p < kFillPatternLibrarySize; p++) {
            const FillPatternData& d = kFillPatternDataLibrary[t][p];
            CHECK(std::strlen(d.nameLong) == 10, "pattern %u/%u name '%s' must be 10 chars", t, p, d.nameLong);
            CHECK(d.levelLo <= d.levelHi, "pattern %s levels", d.nameLong);
            if (isBasic(t, p)) continue;
            CHECK(d.stepSize >= 2 && d.stepSize + 1 <= kBeatMicroLibrarySize, "pattern %s stepSize %u out of range", d.nameLong, d.stepSize);
            CHECK(d.timeLibrary[0] == 0.0f, "pattern %s must start at 0", d.nameLong);
            CHECK(d.timeLibrary[d.stepSize] == 1.0f, "pattern %s must end with 1.0 at index stepSize", d.nameLong);
            for (uint8_t i = 1; i <= d.stepSize; i++) CHECK(d.timeLibrary[i] > d.timeLibrary[i - 1], "pattern %s times must increase (index %u)", d.nameLong, i);
            for (uint8_t i = d.stepSize + 1; i < kBeatMicroLibrarySize; i++) CHECK(d.timeLibrary[i] == 0.0f, "pattern %s unused time slots must be 0", d.nameLong);
        }
    }
}

static void testAllCombinations() {
    const uint16_t starts[] = {0, 800, 3000};
    const uint16_t spans[] = {200, 800, 3200};
    for (uint8_t t = 0; t < kFillTypeLibrarySize; t++)
        for (uint8_t p = 0; p < kFillPatternLibrarySize; p++)
            for (uint8_t tm = kMinFillTime; tm <= kMaxFillTime; tm++)
                for (uint8_t lv = kMinFillLevel; lv <= kMaxFillLevel; lv++)
                    for (uint16_t start : starts)
                        for (uint16_t span : spans) {
                            static Bank bank;
                            bank = Bank();
                            uint8_t target;
                            std::vector<Hit> hits = runFill(bank, start, start + span, t, p, tm, lv, &target);
                            const Beat& beat = bank.beatLibrary[0];
                            const char* name = kFillPatternDataLibrary[t][p].nameLong;
                            if (isBasic(t, p)) {
                                CHECK(target == NULL_FILL_TARGET, "%s should not use a fill slot", name);
                                continue;
                            }
                            CHECK(target != NULL_FILL_TARGET, "%s should get a fill slot", name);
                            CHECK(beat.getFillType() == t && beat.getFillPattern() == p && beat.getFillTime() == tm && beat.getFillLevel() == lv, "%s settings not stored", name);
                            uint8_t ss = kFillPatternDataLibrary[t][p].stepSize;
                            CHECK(hits.size() == (tm == FILL_TIME_STD ? ss : ss + 1u), "%s time %u: %zu hits", name, tm, hits.size());
                            for (size_t i = 0; i < hits.size(); i++) {
                                CHECK(hits[i].interval >= start && hits[i].interval < start + span, "%s hit %zu at %u outside [%u,%u)", name, i, hits[i].interval, start, start + span);
                                if (i) CHECK(hits[i].interval >= hits[i - 1].interval, "%s hits out of order", name);
                                bool stepOk = (lv == 1) && (hits[i].level == kLevelStep);
                                CHECK(stepOk || (hits[i].level >= kLevelLo && hits[i].level <= kLevelHi), "%s level %u", name, hits[i].level);
                            }
                            CHECK(hits[0].interval == start, "%s first hit must be on the beat", name);
                            if (tm == FILL_TIME_R_ALIGNED || lv <= 2) CHECK(hits[0].level == kLevelHi, "%s first hit level %u", name, hits[0].level);
                        }
}

static void expectHits(const char* label, const std::vector<Hit>& got, const std::vector<Hit>& want) {
    CHECK(got.size() == want.size(), "%s: %zu hits, expected %zu", label, got.size(), want.size());
    for (size_t i = 0; i < got.size() && i < want.size(); i++)
        CHECK(got[i].interval == want[i].interval && got[i].level == want[i].level, "%s hit %zu: got (%u,%u) want (%u,%u)", label, i, got[i].interval, got[i].level, want[i].interval, want[i].level);
}

// Cases worked out by hand from BeatFill::set (one beat = 800 ticks).
static void testKnownCases() {
    Bank bank;
    // BASIC / BASIC_2 (stepSize 2, times 0, 0.5, 1.0)
    expectHits("BASIC_2 STD FLAT", runFill(bank, 0, 800, 0, 3, FILL_TIME_STD, 0), {{0, 100}, {400, 100}});
    expectHits("BASIC_2 LEFT FLAT", runFill(bank, 0, 800, 0, 3, FILL_TIME_L_ALIGNED, 0), {{0, 100}, {200, 100}, {400, 100}});
    expectHits("BASIC_2 RIGHT DESCEND", runFill(bank, 0, 800, 0, 3, FILL_TIME_R_ALIGNED, 2), {{0, 100}, {400, 100}, {600, 40}});
    // BASIC_4 STEP alternates high and the quieter step level
    expectHits("BASIC_4 STD STEP", runFill(bank, 1600, 2400, 0, 5, FILL_TIME_STD, 1), {{1600, 100}, {1800, 75}, {2000, 100}, {2200, 75}});
    // BASIC_4 ASCEND: 40 + 20*i
    expectHits("BASIC_4 STD ASCEND", runFill(bank, 0, 800, 0, 5, FILL_TIME_STD, 3), {{0, 40}, {200, 60}, {400, 80}, {600, 100}});

    // ROLL / ROLL_DBL (type 7, pattern 0): 15 hits, DESCEND from 100 to 40
    std::vector<Hit> roll = runFill(bank, 0, 800, 7, 0, FILL_TIME_STD, 2);
    const uint8_t rollLevels[] = {100, 95, 91, 87, 82, 78, 74, 70, 65, 61, 57, 52, 48, 44, 40};
    CHECK(roll.size() == 15, "ROLL_DBL has %zu hits", roll.size());
    for (size_t i = 0; i < roll.size() && i < 15; i++) CHECK(roll[i].level == rollLevels[i], "ROLL_DBL DESCEND hit %zu level %u want %u", i, roll[i].level, rollLevels[i]);
}

static void testRobustness() {
    // Out-of-range values (e.g. from a song saved by newer firmware) fall back to a plain beat.
    Bank bank;
    uint8_t target;
    runFill(bank, 0, 800, kFillTypeLibrarySize, 0, 0, 0, &target);
    CHECK(target == NULL_FILL_TARGET && bank.beatLibrary[0].getFillType() == 0 && bank.beatLibrary[0].getFillPattern() == 0, "unknown fill type must fall back to FLAT");
    runFill(bank, 0, 800, 1, 0, kMaxFillTime + 1, 0, &target);
    CHECK(target == NULL_FILL_TARGET, "unknown fill time must fall back to a plain beat");
    runFill(bank, 0, 800, 1, 0, 0, kMaxFillLevel + 1, &target);
    CHECK(target == NULL_FILL_TARGET, "unknown fill level must fall back to a plain beat");

    // Every complex fill needs one of kBeatFillLibrarySize slots; beyond that, beats stay plain.
    Bank full;
    for (uint8_t b = 0; b <= kBeatFillLibrarySize; b++) {
        full.setBeat(b, b * 100, b * 100 + 100);
        uint8_t t = full.setBeatFill(b, 1, 0, 0, 0);
        if (b < kBeatFillLibrarySize) CHECK(t != NULL_FILL_TARGET, "beat %u should get a fill slot", b);
        else CHECK(t == NULL_FILL_TARGET && full.beatLibrary[b].getFillType() == 0, "beat %u should fall back when slots run out", b);
    }

    // Switching a beat back to a basic pattern frees its slot for reuse.
    Bank reuse;
    reuse.setBeat(0, 0, 800);
    uint8_t first = reuse.setBeatFill(0, 1, 0, 0, 0);
    reuse.setBeatFill(0, 0, 0, 0, 0);
    reuse.setBeat(1, 800, 1600);
    CHECK(reuse.setBeatFill(1, 2, 0, 0, 0) == first, "freed fill slot should be reused");
}

int main() {
    testTables();
    testAllCombinations();
    testKnownCases();
    testRobustness();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
