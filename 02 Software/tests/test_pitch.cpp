// Host-side tests for layer pitch (Pitch.h) and the file format version (FileFormat.h).

#include <cmath>
#include <cstdio>
#include <cstring>

#include "FileFormat.h"
#include "Pitch.h"

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

static void testFormat() {
    uint8_t kit[1500];
    memset(kit, ' ', sizeof kit);
    CHECK(formatReadVersion(kit) == 0, "template spaces = version 0");
    memset(kit, 0, sizeof kit);
    CHECK(formatReadVersion(kit) == 0, "zeros = version 0");
    formatWriteVersion(kit);
    CHECK(formatReadVersion(kit) == kFormatVersion, "round trip");
    CHECK(formatSupported(0) && formatSupported(kFormatVersion) && !formatSupported(kFormatVersion + 1), "supported versions");
    kit[kFormatVersionOffset] = kFormatVersion + 1;
    kit[kFormatVersionOffset + 1] = static_cast<uint8_t>((kFormatVersion + 1) ^ kFormatCheckXor);
    CHECK(!formatSupported(formatReadVersion(kit)), "newer file refused");
    kit[kFormatVersionOffset + 1] ^= 1;
    CHECK(formatReadVersion(kit) == 0, "broken check byte = no stamp");
    // Every possible pair of old bytes: only a matching check byte counts as a stamp.
    int stamps = 0;
    for (int a = 0; a < 256; a++)
        for (int b = 0; b < 256; b++)
            if (formatVersionFrom(static_cast<uint8_t>(a), static_cast<uint8_t>(b)) != 0) stamps++;
    CHECK(stamps == 255, "%d stamped pairs", stamps);
    CHECK(kFormatVersionOffset + 1 < 499 && kFormatVersionOffset > 495, "stamp sits between the routing bytes 490-495 and the marker 498");
}

static void testFactors() {
    CHECK(pitchFactor(kPitchDefaultCode) == 1.0f, "default is unity");
    CHECK(std::fabs(pitchFactor(pitchCodeFromSemi(12)) - 2.0f) < 1e-6f && std::fabs(pitchFactor(pitchCodeFromSemi(-12)) - 0.5f) < 1e-6f, "octaves");
    CHECK(std::fabs(pitchFactor(pitchCodeFromSemi(24)) - 4.0f) < 1e-6f && std::fabs(pitchFactor(pitchCodeFromSemi(-24)) - 0.25f) < 1e-6f, "two octaves");
    for (int s = kPitchMinSemi; s <= kPitchMaxSemi; s++) {
        const float want = std::pow(2.0f, s / 12.0f);
        CHECK(std::fabs(pitchFactor(pitchCodeFromSemi(s)) / want - 1.0f) < 1e-6f, "semitone %d", s);
        CHECK(pitchSemi(pitchCodeFromSemi(s)) == s, "semitone %d round trip", s);
    }
    // Legacy codes play exactly the old factors.
    for (int i = 0; i <= kPitchLegacyMax; i++) {
        const float old = static_cast<float>(i + 50) / 100.0f;
        CHECK(std::fabs(pitchFactor(kPitchLegacyCodeBase + i) - old) < 1e-6f, "legacy %d", i);
    }
    CHECK(pitchFactor(250) == 1.0f, "invalid code plays at unity");
}

static void testSteps() {
    uint8_t c = kPitchDefaultCode;
    c = pitchStep(c, true, false);
    CHECK(pitchSemi(c) == 1, "one up");
    c = pitchStep(c, true, true);
    CHECK(pitchSemi(c) == 12, "octave up snaps to 12");
    c = pitchStep(c, true, true);
    CHECK(pitchSemi(c) == 24, "to 24");
    c = pitchStep(c, true, true);
    CHECK(pitchSemi(c) == 24, "clamped at +24");
    c = pitchStep(pitchCodeFromSemi(-5), false, true);
    CHECK(pitchSemi(c) == -12, "-5 down an octave = -12");
    c = pitchStep(pitchCodeFromSemi(-5), true, true);
    CHECK(pitchSemi(c) == 0, "-5 up an octave = 0");
    c = pitchStep(pitchCodeFromSemi(-24), false, false);
    CHECK(pitchSemi(c) == -24, "clamped at -24");
    // Every semitone, both octave directions, lands on an octave line or the limit.
    for (int s = kPitchMinSemi; s <= kPitchMaxSemi; s++) {
        const int up = pitchSemi(pitchStep(pitchCodeFromSemi(s), true, true));
        const int dn = pitchSemi(pitchStep(pitchCodeFromSemi(s), false, true));
        CHECK(up % 12 == 0 && (up > s || s == kPitchMaxSemi) && up - s <= 12, "octave up from %d -> %d", s, up);
        CHECK(dn % 12 == 0 && (dn < s || s == kPitchMinSemi) && s - dn <= 12, "octave down from %d -> %d", s, dn);
    }
    // A legacy pitch snaps to its nearest semitone on the first turn.
    const uint8_t legacy137 = kPitchLegacyCodeBase + 87;  // 1.37x = +5.45 semitones
    CHECK(pitchSemi(legacy137) == 5, "1.37x is nearest +5");
    CHECK(pitchSemi(pitchStep(legacy137, true, false)) == 6 && pitchSemi(pitchStep(legacy137, false, false)) == 4, "first turn steps from the nearest semitone");
}

static void testLfo() {
    // Legacy layers: the old rule exactly.
    for (int i = 0; i <= kPitchLegacyMax; i += 7) {
        for (float lfo = 0.0f; lfo <= 1.0f; lfo += 0.125f) {
            int n = i + static_cast<int>(round(100 * (lfo - 0.5) * 0.8f));
            n = n < 0 ? 0 : (n > 150 ? 150 : n);
            CHECK(pitchWithLfo(kPitchLegacyCodeBase + i, lfo, 0.8f) == kPitchLegacyCodeBase + n, "legacy LFO %d %f", i, lfo);
        }
    }
    // Semitone layers: +-12 at full depth, nothing at the centre, clamped.
    CHECK(pitchSemi(pitchWithLfo(kPitchDefaultCode, 1.0f, 1.0f)) == 12 && pitchSemi(pitchWithLfo(kPitchDefaultCode, 0.0f, 1.0f)) == -12, "full depth");
    CHECK(pitchWithLfo(pitchCodeFromSemi(7), 0.5f, 1.0f) == pitchCodeFromSemi(7), "centre");
    CHECK(pitchSemi(pitchWithLfo(pitchCodeFromSemi(20), 1.0f, 1.0f)) == 24, "clamped");
    CHECK(pitchSemi(pitchWithLfo(kPitchDefaultCode, 1.0f, 0.5f)) == 6, "half depth");
}

static void testNamesAndFiles() {
    char t[32];
    pitchName(pitchCodeFromSemi(7), t, sizeof t);
    CHECK(!strcmp(t, "        +7"), "[%s]", t);
    pitchName(pitchCodeFromSemi(-12), t, sizeof t);
    CHECK(!strcmp(t, "       -12"), "[%s]", t);
    pitchName(kPitchDefaultCode, t, sizeof t);
    CHECK(!strcmp(t, "         0"), "[%s]", t);
    pitchName(kPitchLegacyCodeBase + 87, t, sizeof t);
    CHECK(!strcmp(t, "      1.37"), "[%s]", t);
    pitchName(kPitchLegacyCodeBase, t, sizeof t);
    CHECK(!strcmp(t, "      0.50") && strlen(t) == 10, "[%s]", t);

    // Old files (version 0): byte 24 is a space and ignored; byte 1 is the old index.
    for (int i = 0; i <= kPitchLegacyMax; i++) {
        CHECK(pitchFromFile(static_cast<uint8_t>(i), ' ', 0) == kPitchLegacyCodeBase + i, "old file index %d", i);
    }
    CHECK(pitchFromFile(200, ' ', 0) == kPitchDefaultCode, "out-of-range old byte");
    // New files round-trip every code; older firmware reads byte 1 close to the real pitch.
    for (int c = 0; c <= kPitchCodeMax; c++) {
        const uint8_t code = static_cast<uint8_t>(c);
        const uint8_t back = pitchFromFile(pitchFileOldByte(code), pitchFileSemiByte(code), kFormatVersion);
        CHECK(back == code, "code %d round trip -> %u", c, back);
        const float oldFactor = (pitchFileOldByte(code) + 50) / 100.0f;
        const float f = pitchFactor(code);
        if (f >= 0.5f && f <= 2.0f) CHECK(std::fabs(oldFactor - f) <= 0.0051f, "code %d old byte %.3f vs %.3f", c, oldFactor, f);
    }
}

int main() {
    testFormat();
    testFactors();
    testSteps();
    testLfo();
    testNamesAndFiles();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
