// Host-side tests for per-note settings (BeatNote.h) and their place in the Beat (Fill.h).

#include <cstdio>
#include <cstring>
#include <random>
#include <set>
#include <string>

#include "BeatList.h"
#include "BeatNote.h"

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

static std::string name(uint8_t code) {
    char t[16];
    noteCondName(code, t, 11);
    std::string s(t);
    return s.substr(s.find_first_not_of(' '));
}

static void testConditions() {
    // Every code decodes and encodes back; names are unique.
    std::set<std::string> names;
    for (uint8_t c = 0; c < kNoteCondCount; c++) {
        CHECK(noteCondEncode(noteCondDecode(c)) == c, "code %u round trip", c);
        names.insert(name(c));
    }
    CHECK(names.size() == kNoteCondCount, "%zu unique names", names.size());
    CHECK(name(0) == "ALWAYS" && name(kNoteCondFill) == "FKEY" && name(kNoteCondNotPre) == "!PRE", "names");
    CHECK(name(noteCondEncode({COND_RATIO, 1, 4})) == "1:4" && name(noteCondEncode({COND_RATIO, 8, 8})) == "8:8", "ratio names");
    CHECK(name(noteCondEncode({COND_NOT_RATIO, 3, 4})) == "!3:4", "negated ratio");
    CHECK(noteCondEncode({COND_NOT_RATIO, 1, 5}) == 0 && noteCondEncode({COND_RATIO, 3, 2}) == 0, "out of range = none");
    // Field order visits every code once and wraps.
    std::set<uint8_t> seen;
    uint8_t c = 0;
    for (uint8_t i = 0; i < kNoteCondCount; i++) {
        seen.insert(c);
        c = noteCondStep(c, true);
    }
    CHECK(seen.size() == kNoteCondCount && c == 0, "stepping up visits all and wraps");
    CHECK(noteCondStep(0, false) == noteCondAtStep(kNoteCondCount - 1), "down from none wraps to the last");
    CHECK(noteCondStep(0, true) == kNoteCondFill, "FILL first");
    char t[16];
    noteCondName(noteCondEncode({COND_RATIO, 1, 2}), t, sizeof t);
    CHECK(strlen(t) == 10 && !strcmp(t, "       1:2"), "[%s] right-aligned in 10", t);

    // A:B over 24 loops: plays exactly loops/B times, on loop A-1 mod B.
    for (uint8_t b = 2; b <= 8; b++) {
        for (uint8_t a = 1; a <= b; a++) {
            NoteState s;
            int plays = 0;
            bool right = true;
            for (uint16_t loop = 0; loop < 8 * b; loop++) {
                s.loop = loop;
                const bool p = noteCondPasses(noteCondEncode({COND_RATIO, a, b}), s);
                plays += p;
                right = right && (p == ((loop % b) == a - 1));
            }
            CHECK(plays == 8 && right, "%u:%u plays %d of %d", a, b, plays, 8 * b);
        }
    }
    NoteState s;
    s.loop = 0;
    CHECK(noteCondPasses(kNoteCond1st, s) && !noteCondPasses(kNoteCondNot1st, s), "1ST on the first loop");
    s.loop = 5;
    CHECK(!noteCondPasses(kNoteCond1st, s) && noteCondPasses(kNoteCondNot1st, s), "!1ST later");
    s.fillHeld = true;
    CHECK(noteCondPasses(kNoteCondFill, s) && !noteCondPasses(kNoteCondNotFill, s), "FILL held");
}

static void testPlays() {
    // PRE follows the last conditioned beat; plain beats and PRE beats don't change it.
    NoteState s;
    const uint16_t half = noteMake(10, 0, 0);  // 50%
    const uint16_t pre = noteMake(0, kNoteCondPre, 0), notPre = noteMake(0, kNoteCondNotPre, 0);
    std::mt19937 rng(9);
    int agree = 0;
    for (int i = 0; i < 1000; i++) {
        const bool first = notePlays(half, s, static_cast<uint16_t>(rng() % 10000));
        notePlays(0, s, 0);  // a plain beat in between
        const bool p = notePlays(pre, s, 0);
        const bool np = notePlays(notPre, s, 0);
        agree += (p == first) && (np == !first);
    }
    CHECK(agree == 1000, "PRE / !PRE follow the 50%% beat: %d", agree);
    // Chance: 0% never, 100% always, 50% about half, 5% steps.
    int n50 = 0, n0 = 0, n100 = 0;
    for (int i = 0; i < 20000; i++) {
        NoteState t;
        const uint16_t r = static_cast<uint16_t>(rng() % 10000);
        n50 += notePlays(noteWithChancePercent(0, 50), t, r);
        n0 += notePlays(noteWithChancePercent(0, 0), t, r);
        n100 += notePlays(0, t, r);
    }
    CHECK(n0 == 0 && n100 == 20000 && n50 > 9500 && n50 < 10500, "chance %d %d %d", n0, n50, n100);
    for (int p = 0; p <= 100; p += 5) CHECK(noteChancePercent(noteWithChancePercent(0, static_cast<int16_t>(p))) == p, "chance %d%%", p);
    CHECK(noteChancePercent(noteWithChancePercent(0, 97)) == 95 && noteChancePercent(noteWithChancePercent(0, 150)) == 100, "rounding and clamp");
    // A condition and a chance together.
    NoteState t;
    t.loop = 1;
    CHECK(!notePlays(noteMake(0, noteCondEncode({COND_RATIO, 1, 2}), 0), t, 0), "1:2 off on loop 1");
}

static void testWord() {
    // Every field combination survives the 16-bit word; all zero = default.
    for (uint8_t ch = 0; ch <= kNoteChanceSteps; ch++)
        for (uint8_t c = 0; c < kNoteCondCount; c++)
            for (int8_t n = -kNoteNudgeMax; n <= kNoteNudgeMax; n++) {
                const uint16_t w = noteMake(ch, c, n);
                if (noteChanceStep(w) != ch || noteCondCode(w) != c || noteNudge(w) != n) {
                    CHECK(false, "word %u %u %d", ch, c, n);
                    return;
                }
                if (noteSanitize(w) != w) {
                    CHECK(false, "sanitize keeps valid %u %u %d", ch, c, n);
                    return;
                }
            }
    CHECK(noteMake(0, 0, 0) == 0 && noteIsDefault(0, 0) && !noteIsDefault(1, 0), "zero is default");
    CHECK(noteNudge(noteWithNudge(0, 40)) == kNoteNudgeMax && noteNudge(noteWithNudge(0, -40)) == -kNoteNudgeMax, "nudge clamped");
    // Garbage from a file: out-of-range fields become defaults.
    const uint16_t bad = static_cast<uint16_t>(31 | (63 << 5) | (15 << 11));  // chance 31, cond 63, nudge +15
    CHECK(noteSanitize(bad) == 0, "sanitized garbage %04x", noteSanitize(bad));
    // Due interval: nudge moves, clamped inside the loop.
    const uint16_t song = 3200;
    CHECK(noteDueInterval(400, noteWithNudge(0, 5), song) == 410 && noteDueInterval(400, noteWithNudge(0, -12), song) == 376, "nudge ticks");
    CHECK(noteDueInterval(0, noteWithNudge(0, -3), song) == 0 && noteDueInterval(3190, noteWithNudge(0, 12), song) == 3199, "clamped to the loop");
    // Neighbours on the finest grid (50 ticks) never swap, whatever their nudges.
    for (int a = -kNoteNudgeMax; a <= kNoteNudgeMax; a++)
        for (int b = -kNoteNudgeMax; b <= kNoteNudgeMax; b++)
            if (noteDueInterval(1000, noteWithNudge(0, static_cast<int16_t>(a)), song) >= noteDueInterval(1050, noteWithNudge(0, static_cast<int16_t>(b)), song)) {
                CHECK(false, "swap %d %d", a, b);
                return;
            }
    // Velocity to level.
    CHECK(noteLevel(0, 100) == 100 && noteLevel(0, 50) == 50 && noteLevel(127, 50) == 100 && noteLevel(64, 100) == 50 && noteLevel(1, 100) == 1, "levels");
}

static void testBeat() {
    // The settings live in the Beat: copies, sorts and moves carry them; reset clears them.
    CHECK(sizeof(Beat) == 10, "Beat is %zu bytes (RAM budget)", sizeof(Beat));
    Bank bank;
    beatListClear(bank);
    const uint16_t song = 3200;
    for (uint16_t s : {800, 0, 2400, 1600}) beatListAdd(bank, s, song);
    beatListRebuild(bank, song);  // sorted: 0, 800, 1600, 2400
    for (uint8_t i = 0; i <= static_cast<uint8_t>(bank.lastActiveBeatNum); i++) {
        CHECK(bank.beatLibrary[i].getVelocity() == 0 && bank.beatLibrary[i].getNote() == 0, "new beats default");
        bank.beatLibrary[i].setVelocity(static_cast<uint8_t>(10 + bank.beatLibrary[i].getStartInterval() / 100));
        bank.beatLibrary[i].setNote(noteMake(2, kNoteCondFill, static_cast<int8_t>(bank.beatLibrary[i].getStartInterval() / 400)));
    }
    beatListMove(bank, 0, true, 1000, song);  // 0 -> 1000: sorted after 800
    beatListRotate(bank, 400, song);
    bool kept = true;
    for (uint8_t i = 0; i <= static_cast<uint8_t>(bank.lastActiveBeatNum); i++) {
        const Beat& b = bank.beatLibrary[i];
        // each beat's velocity encodes its original start (+1000 for the moved one), rotated by 400
        const uint16_t orig = static_cast<uint16_t>((b.getStartInterval() + song - 400) % song);
        const uint16_t was = (orig == 1000) ? 0 : orig;
        kept = kept && (b.getVelocity() == 10 + was / 100) && (noteNudge(b.getNote()) == was / 400) && (noteCondCode(b.getNote()) == kNoteCondFill);
    }
    CHECK(kept, "move and rotate keep each beat's settings");
    Bank copy = bank;
    CHECK(copy.beatLibrary[2].getVelocity() == bank.beatLibrary[2].getVelocity() && copy.beatLibrary[2].getNote() == bank.beatLibrary[2].getNote(), "copied with the bank");
    bank.resetBeat(1);
    CHECK(bank.beatLibrary[1].getVelocity() == 0 && bank.beatLibrary[1].getNote() == 0, "resetBeat clears");
    bank.beatLibrary[2].reset();
    CHECK(bank.beatLibrary[2].getVelocity() == 0 && bank.beatLibrary[2].getNote() == 0, "reset clears");
    // A new beat in a slot that held another one starts clean (setBeat).
    bank.beatLibrary[3].setVelocity(99);
    bank.setBeat(3, 100, 200);
    CHECK(bank.beatLibrary[3].getVelocity() == 0 && bank.beatLibrary[3].getNote() == 0, "setBeat starts clean");
}

static void testFile() {
    uint8_t h[kNoteHeaderSize];
    noteWriteHeader(h, 1234);
    CHECK(noteReadHeader(h, sizeof h) == 1234, "header");
    CHECK(noteReadHeader(h, 5) == -1, "short");
    h[3] = 2;
    CHECK(noteReadHeader(h, sizeof h) == -1, "unknown version");
    const uint8_t zeros[kNoteHeaderSize] = {0};
    CHECK(noteReadHeader(zeros, sizeof zeros) == -1, "no block");
    std::mt19937 rng(4);
    for (int i = 0; i < 2000; i++) {
        NoteEntry e{static_cast<uint8_t>(rng() % 10), static_cast<uint8_t>(rng() % 5), static_cast<uint16_t>(rng() % 12800),
                    static_cast<uint8_t>(rng() % 128), noteMake(static_cast<uint8_t>(rng() % 21), static_cast<uint8_t>(rng() % kNoteCondCount), static_cast<int8_t>(static_cast<int>(rng() % 25) - 12))};
        uint8_t buf[kNoteEntrySize];
        noteWriteEntry(buf, e);
        NoteEntry d;
        const bool ok = noteReadEntry(buf, d, 10, 5);
        CHECK(ok && d.layer == e.layer && d.bank == e.bank && d.start == e.start && d.velocity == e.velocity && d.note == e.note, "entry %d", i);
        if (failures) break;
    }
    uint8_t bad[kNoteEntrySize] = {12, 0, 0, 0, 200, 0xFF, 0xFF};
    NoteEntry d;
    CHECK(!noteReadEntry(bad, d, 10, 5) && d.velocity == 127, "bad layer refused, velocity clamped");
}

int main() {
    testConditions();
    testPlays();
    testWord();
    testBeat();
    testFile();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
