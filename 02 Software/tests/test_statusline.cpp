// Host-side tests for the status line and the layer meters (StatusLine.h).

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "StatusLine.h"

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

// Characters the 5x7 font has (everything else would draw as a blank box): upper case, digits and these.
static bool drawable(const char* s) {
    for (; *s; s++) {
        const char c = *s;
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ') continue;
        if (strchr("!#%+,-./:<>?[]_|", c)) continue;
        return false;
    }
    return true;
}

int main() {
    // --- Hints ---
    CHECK(statusHint(0) == nullptr, "nothing held");
    const uint8_t keys[] = {HELD_MUTE, HELD_FILL, HELD_SOLO, HELD_INST, HELD_SONGP, HELD_SONG};
    for (uint8_t k : keys) {
        const char* h = statusHint(k);
        CHECK(h != nullptr && strlen(h) <= kStatusMaxChars && drawable(h), "hint for %u: %s", k, h ? h : "(none)");
    }
    CHECK(statusHint(HELD_MUTE | HELD_SONG) == statusHint(HELD_MUTE), "the lowest bit wins");
    CHECK(strstr(statusHint(HELD_SOLO), "SOLO") && strstr(statusHint(HELD_MUTE), "UNDO") && strstr(statusHint(HELD_FILL), "FKEY"), "hints name their keys");

    // --- Idle text ---
    char t[128];
    int n = statusIdleText(t, sizeof t, 9743, 4);
    CHECK(std::string(t) == "CLK 97.43 LOCKED   AUTOSAVE 4 MIN AGO" && n == (int)strlen(t), "idle: %s", t);
    statusIdleText(t, sizeof t, 0, -1);
    CHECK(std::string(t) == "AUTOSAVE OFF", "no clock: %s", t);
    statusIdleText(t, sizeof t, 12000, -2);
    CHECK(std::string(t) == "CLK 120.00 LOCKED   AUTOSAVE ON", "%s", t);
    statusIdleText(t, sizeof t, 0, 0);
    CHECK(std::string(t) == "AUTOSAVE JUST NOW", "%s", t);
    statusIdleText(t, sizeof t, 99999999u, 5000);
    CHECK(strlen(t) <= kStatusMaxChars && drawable(t), "extremes fit: %s", t);
    for (int age : {-2, -1, 0, 1, 59, 600}) {
        statusIdleText(t, sizeof t, 20000, age);
        CHECK(strlen(t) <= kStatusMaxChars && drawable(t), "age %d: %s", age, t);
    }
    CHECK(statusAgeMinutes(120000, 0) == 2 && statusAgeMinutes(5, 0xFFFFFFFFu - 59994u) == 1 && statusAgeMinutes(60005, 5) == 1, "age minutes");
    CHECK(kStatusClearX + kStatusClearWidth > kStatusRightX && kStatusRightX < 939, "line fits the strip");

    // --- Clock tempo ---
    CHECK(statusCentiBpm(0) == 0, "no clock");
    CHECK(statusCentiBpm(20833.3f) == 12000, "120 BPM: %u", statusCentiBpm(20833.3f));
    CHECK(statusCentiBpm(25707.0f) == 9725, "97.25 BPM: %u", statusCentiBpm(25707.0f));
    CHECK(statusCentiBpm(1.0f) == 99999, "capped");

    // --- Meters ---
    CHECK(meterPixels(0.0f) == 0 && meterPixels(-1.0f) == 0 && meterPixels(0.001f) == 0, "silence");
    CHECK(meterPixels(1.0f) == kMeterBarWidth && meterPixels(2.0f) == kMeterBarWidth, "full scale");
    CHECK(meterPixels(0.0042f) == 1 || meterPixels(0.0042f) == 2, "just above the floor: %u", meterPixels(0.0042f));
    CHECK(meterPixels(0.5f) > 15 && meterPixels(0.5f) < kMeterBarWidth, "-6 dB: %u", meterPixels(0.5f));
    uint16_t prev = 0;
    for (int i = 0; i <= 2000; i++) {
        const float p = std::pow(10.0f, -60.0f / 20.0f + i * (63.0f / 20.0f) / 2000.0f);
        const uint16_t px = meterPixels(p);
        CHECK(px >= prev && px <= kMeterBarWidth, "monotonic at %d: %u", i, px);
        prev = px;
    }
    CHECK(meterClip(1.0f) && meterClip(1.7f) && !meterClip(0.99f), "clip");
    CHECK(meterHold(10, 14) == 14 && meterHold(14, 10) == 12 && meterHold(11, 10) == 10 && meterHold(0, 0) == 0, "hold");
    uint16_t shown = kMeterBarWidth;
    int updates = 0;
    while (shown > 0 && updates < 100) {
        shown = meterHold(shown, 0);
        updates++;
    }
    CHECK(updates == 9, "full to empty in %d updates", updates);
    CHECK(kMeterX + kMeterWidth <= 305 && kMeterX >= 269 && kMeterBarWidth + 4 == kMeterWidth, "meter sits between the boxes and the song row");
    CHECK(kMeterY + kMeterHeight < 31, "inside the lane");

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
