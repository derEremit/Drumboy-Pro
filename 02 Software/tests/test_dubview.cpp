// Host-side tests for the dub desk geometry (DubView.h).

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "DubView.h"

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

int main() {
    // Twelve columns fill the lane area (x 20..940): ten layers and the two returns.
    CHECK(deskX(0) == 24 && deskX(11) + kDeskWidth <= 936 && deskX(11) + kDeskWidth >= 930, "columns end at %u", deskX(11) + kDeskWidth);
    CHECK(kDeskWidth + 3 == kDeskStep && kDeskColumns == 12, "a 3 px gap between columns");
    // Rows from the top, none overlapping, all inside y 131..440.
    CHECK(kDeskPlateY >= 133 && kDeskBusY >= kDeskPlateY + kDeskPlateHeight + 2, "plate then bus");
    CHECK(kDeskKnobY - kDeskKnobRadius - 3 >= kDeskBusY + kDeskBusHeight + 2, "knobs below the bus switch");
    CHECK(kDeskKnobLabelY >= kDeskKnobY + kDeskKnobRadius + 3 && kDeskKnobValueY >= kDeskKnobLabelY + 8 && kDeskPanY >= kDeskKnobValueY + 8, "knob texts, pan bar");
    CHECK(kDeskFaderY >= kDeskPanY + kDeskPanHeight + 3 && kDeskLevelY >= kDeskFaderY + kDeskFaderHeight + 3 && kDeskFootY >= kDeskLevelY + 8, "fader, level, foot");
    CHECK(kDeskFootY + kDeskFootHeight <= 438, "the foot plate fits: %u", kDeskFootY + kDeskFootHeight);
    // The fader is shorter than before (208 px).
    CHECK(kDeskFaderHeight < 208 && kDeskFaderHeight >= 100, "fader height %u", kDeskFaderHeight);
    // Columns: scale, ticks, rail, cap and meter fit and do not overlap.
    CHECK(kDeskScaleX + 14 <= kDeskTickX && kDeskTickX + 6 <= kDeskRailX && kDeskCapX + kDeskCapWidth <= kDeskMeterX && kDeskMeterX + kDeskMeterWidth <= kDeskWidth, "fader columns");
    CHECK(kDeskCapX <= kDeskRailX && kDeskCapX + kDeskCapWidth > kDeskRailX, "the rail runs through the cap");
    // Knobs fit their half of the column.
    for (int k = 0; k < 2; k++) CHECK(kDeskKnobX[k] >= kDeskKnobRadius + 4 && kDeskKnobX[k] + kDeskKnobRadius + 4 <= kDeskWidth, "knob %d fits", k);
    CHECK(kDeskKnobX[1] - kDeskKnobX[0] >= 2 * kDeskKnobRadius + 6, "knobs do not touch");
    CHECK(2 * kDeskBusCell + 1 <= kDeskWidth, "bus cells fit");
    // Fader cap.
    CHECK(deskCapY(100) == kDeskFaderY && deskCapY(0) == kDeskFaderY + kDeskFaderHeight - kDeskCapHeight && deskCapY(250) == kDeskFaderY, "cap ends");
    uint16_t prev = 0xFFFF;
    for (uint8_t v = 0; v <= 100; v++) {
        CHECK(deskCapY(v) <= prev, "cap monotonic at %u", v);
        prev = deskCapY(v);
    }
    // Knob pointer: lower left at 0, straight up at 50, lower right at 100, clockwise in between.
    int16_t x, y;
    deskKnobPoint(100, 100, 10, 50, x, y);
    CHECK(x == 100 && y == 90, "50 %% is straight up: %d %d", x, y);
    deskKnobPoint(100, 100, 10, 0, x, y);
    CHECK(x < 100 && y > 100 && std::abs(x - 93) <= 1 && std::abs(y - 107) <= 1, "0 %% is lower left: %d %d", x, y);
    deskKnobPoint(100, 100, 10, 100, x, y);
    CHECK(x > 100 && y > 100 && std::abs(x - 107) <= 1 && std::abs(y - 107) <= 1, "100 %% is lower right: %d %d", x, y);
    deskKnobPoint(100, 100, 10, 200, x, y);
    CHECK(x == 107 || x == 108 || x == 106, "past the end is the end");
    int16_t lastX = -1000, lastY = 1000;
    for (uint8_t p = 0; p <= 100; p++) {
        deskKnobPoint(100, 100, 10, p, x, y);
        CHECK(std::abs(x - 100) <= 10 && std::abs(y - 100) <= 10, "pointer inside the knob at %u", p);
        if ((p >= 17) && (p <= 50)) CHECK(x >= lastX, "from the leftmost point to the top it moves right at %u", p);
        if (p <= 50) CHECK(y <= lastY, "the first half rises at %u", p);
        lastX = x;
        lastY = y;
    }
    // Pan bar.
    CHECK(deskPanX(0, 41) == 0 && deskPanX(40, 41) == 40 && deskPanX(20, 41) == 20 && deskPanX(99, 41) == 40, "pan bar");
    // LED ladder: segments from the bottom, a quiet signal lights none, a full one all.
    CHECK(deskMeterLit(0, 18) == 0 && deskMeterLit(1, 18) == 1 && deskMeterLit(18, 18) == kDeskMeterSegments && deskMeterLit(99, 18) == kDeskMeterSegments && deskMeterLit(5, 0) == 0, "ladder");
    uint8_t lastLit = 0;
    for (uint16_t px = 0; px <= 18; px++) {
        CHECK(deskMeterLit(px, 18) >= lastLit, "ladder monotonic at %u", px);
        lastLit = deskMeterLit(px, 18);
    }
    CHECK(deskSegmentY(0) + deskSegmentHeight() <= kDeskFaderY + kDeskFaderHeight && deskSegmentY(kDeskMeterSegments - 1) >= kDeskFaderY, "segments inside the fader height");
    for (uint8_t k = 1; k < kDeskMeterSegments; k++) CHECK(deskSegmentY(k) + deskSegmentHeight() < deskSegmentY(k - 1), "segments %u and %u do not touch", k - 1, k);
    CHECK(kDeskMeterGreen < kDeskMeterYellow && kDeskMeterYellow < kDeskMeterSegments, "colour bands");
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
