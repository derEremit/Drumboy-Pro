// Host-side tests for autosave (Autosave.h): CRC, trailer, which file to restore and to write
// (including a write cut short by a power cut), timing and when writing is safe.

#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#include "Autosave.h"

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

static void testCrc() {
    const char* s = "123456789";
    CHECK(autosaveCrc32(0, reinterpret_cast<const uint8_t*>(s), 9) == 0xCBF43926u, "standard check value");
    // Feeding in blocks = feeding at once.
    std::mt19937 rng(3);
    std::vector<uint8_t> d(14500);
    for (auto& b : d) b = static_cast<uint8_t>(rng());
    const uint32_t whole = autosaveCrc32(0, d.data(), d.size());
    uint32_t parts = 0;
    for (size_t off = 0; off < d.size(); off += 512) parts = autosaveCrc32(parts, d.data() + off, std::min<size_t>(512, d.size() - off));
    CHECK(whole == parts, "blockwise");
    // Any single changed byte changes it (e.g. one beat moved, one level step).
    int same = 0;
    for (int i = 0; i < 2000; i++) {
        const size_t at = rng() % d.size();
        const uint8_t old = d[at];
        d[at] = static_cast<uint8_t>(old ^ (1u << (rng() % 8)));
        if (autosaveCrc32(0, d.data(), d.size()) == whole) same++;
        d[at] = old;
    }
    CHECK(same == 0, "single-bit changes always detected (%d missed)", same);
}

// A simulated file on the card: content + trailer; `cut` bytes missing at the end.
static AutosaveFile scan(const std::vector<uint8_t>& file) {
    AutosaveFile f;
    if (file.size() < kAutosaveTrailerSize) return f;
    uint32_t seq, crc;
    if (!autosaveReadTrailer(file.data() + file.size() - kAutosaveTrailerSize, seq, crc)) return f;
    if (autosaveCrc32(0, file.data(), file.size() - kAutosaveTrailerSize) != crc) return f;
    f.valid = true;
    f.sequence = seq;
    return f;
}

static std::vector<uint8_t> makeFile(uint32_t seq, uint8_t fill, size_t size = 14422) {
    std::vector<uint8_t> f(size, fill);
    f.resize(size + kAutosaveTrailerSize);
    autosaveWriteTrailer(f.data() + size, seq, autosaveCrc32(0, f.data(), size));
    return f;
}

static void testFiles() {
    std::vector<uint8_t> a = makeFile(7, 0x11), b = makeFile(8, 0x22);
    AutosaveFile fa = scan(a), fb = scan(b);
    CHECK(fa.valid && fb.valid && fa.sequence == 7 && fb.sequence == 8, "trailer round trip");
    CHECK(autosaveNewest(fa, fb) == 1 && autosaveNextSlot(fa, fb) == 0 && autosaveNextSequence(fa, fb) == 9, "newest B, write A next");

    // Power cut while writing A: A is truncated -> invalid; B is restored and A is written again.
    std::vector<uint8_t> cut(a.begin(), a.begin() + 9000);
    AutosaveFile fc = scan(cut);
    CHECK(!fc.valid && autosaveNewest(fc, fb) == 1 && autosaveNextSlot(fc, fb) == 0, "cut-short write: the other file survives");
    // A corrupted byte in the middle is caught by the CRC.
    std::vector<uint8_t> bad = b;
    bad[5000] ^= 0x40;
    CHECK(!scan(bad).valid, "corruption detected");
    // No files at all.
    AutosaveFile none;
    CHECK(autosaveNewest(none, none) == -1 && autosaveNextSlot(none, none) == 0 && autosaveNextSequence(none, none) == 1, "nothing yet");

    // Alternation over many saves, with random power cuts: the restore is always the last
    // completed save.
    std::mt19937 rng(11);
    std::vector<uint8_t> files[2];
    uint32_t lastGood = 0;
    for (int n = 0; n < 500; n++) {
        AutosaveFile s0 = scan(files[0]), s1 = scan(files[1]);
        const uint8_t slot = autosaveNextSlot(s0, s1);
        const uint32_t seq = autosaveNextSequence(s0, s1);
        std::vector<uint8_t> f = makeFile(seq, static_cast<uint8_t>(n), 3000 + rng() % 2000);
        const bool cutShort = (rng() % 5) == 0;
        if (cutShort) f.resize(rng() % f.size());
        files[slot] = f;
        if (!cutShort) lastGood = seq;
        AutosaveFile r0 = scan(files[0]), r1 = scan(files[1]);
        const int8_t newest = autosaveNewest(r0, r1);
        const uint32_t restored = (newest < 0) ? 0 : (newest == 0 ? r0.sequence : r1.sequence);
        CHECK(restored == lastGood, "save %d: restore %u, last good %u", n, restored, lastGood);
        if (failures) break;
    }

    // Sequence wraparound.
    AutosaveFile w0, w1;
    w0.valid = w1.valid = true;
    w0.sequence = 0xFFFFFFFFu;
    w1.sequence = 0;
    CHECK(autosaveNewest(w0, w1) == 1, "0 after 0xFFFFFFFF is newer");
    CHECK(!autosaveReadTrailer(reinterpret_cast<const uint8_t*>("XYZ\1abcdefgh"), w0.sequence, w0.sequence), "not a trailer");
}

static void testTiming() {
    AutosaveTiming t;
    t.lastCheckMs = 1000;
    CHECK(!autosaveDue(t, 1000 + 599999) && autosaveDue(t, 1000 + 600000), "10 minutes by default");
    t.interval = 0;
    CHECK(!autosaveDue(t, 0xFFFFFFF0u), "OFF never");
    t.interval = 1;
    t.lastCheckMs = 0xFFFFFF00u;
    CHECK(autosaveDue(t, 0xFFFFFF00u + 120000u), "2 minutes across the millisecond wrap");
    CHECK(autosaveIntervalMs(9) == 600000, "bad setting = default");
    CHECK(kAutosaveMinutes[4] == 20, "20 minutes");
}

static void testSafety() {
    AutosaveContext c{false, false, false, false, false, 5000};
    CHECK(autosaveSafeNow(c), "stopped and quiet");
    c.msSinceKey = 500;
    CHECK(!autosaveSafeNow(c), "not while editing");
    c.msSinceKey = 5000;
    c.recording = true;
    CHECK(!autosaveSafeNow(c), "not while recording");
    c.recording = false;
    c.busy = true;
    CHECK(!autosaveSafeNow(c), "not during other SD work or a question");
    c.busy = false;
    c.playing = true;
    CHECK(!autosaveSafeNow(c), "playing: only at a loop end");
    c.atLoopEnd = true;
    CHECK(autosaveSafeNow(c), "playing, loop end");
    c.midiNotesIn = true;
    CHECK(!autosaveSafeNow(c), "playing with MIDI notes coming in: wait for STOP");
    c.playing = false;
    CHECK(autosaveSafeNow(c), "stopped with MIDI notes in: fine");
}

int main() {
    testCrc();
    testFiles();
    testTiming();
    testSafety();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
