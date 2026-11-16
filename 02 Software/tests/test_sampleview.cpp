// Host-side tests for the sample view (SampleView.h).

#include <cmath>
#include <cstdio>
#include <vector>

#include "SampleView.h"

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
    uint8_t out[kSampleCols];
    // Silence and bad input.
    std::vector<float> zero(1000, 0.0f);
    samplePeakColumns(zero.data(), zero.size(), 1.0f, out, kSampleCols);
    bool allZero = true;
    for (uint16_t c = 0; c < kSampleCols; c++) allZero = allZero && (out[c] == 0);
    CHECK(allZero, "silence shows nothing");
    samplePeakColumns(nullptr, 100, 1.0f, out, kSampleCols);
    CHECK(out[0] == 0 && out[kSampleCols - 1] == 0, "no data");
    samplePeakColumns(zero.data(), 0, 1.0f, out, kSampleCols);
    CHECK(out[10] == 0, "no samples");

    // A burst in the second half only: the first columns are 0, the last are loud.
    std::vector<float> burst(240000, 0.0f);
    for (uint32_t i = 120000; i < 240000; i++) burst[i] = 0.5f * ((i & 1) ? 1.0f : -1.0f);
    samplePeakColumns(burst.data(), burst.size(), 1.0f, out, kSampleCols);
    CHECK(out[0] == 0 && out[400] == 0 && out[430] == 50 && out[kSampleCols - 1] == 50, "burst: %u %u %u %u", out[0], out[400], out[430], out[kSampleCols - 1]);
    // Normalisation scales it, and caps at the plot height.
    samplePeakColumns(burst.data(), burst.size(), 1.6f, out, kSampleCols);
    CHECK(out[600] == 80, "scaled: %u", out[600]);
    samplePeakColumns(burst.data(), burst.size(), 4.0f, out, kSampleCols);
    CHECK(out[600] == kSampleHalf, "capped: %u", out[600]);
    // A single click is not lost to the stride (a column's stride is at most 64 reads, so a click
    // narrower than the stride can be missed; a click of the column's own width is not).
    std::vector<float> click(840 * 100, 0.0f);
    for (uint32_t i = 100 * 300; i < 100 * 301; i++) click[i] = 0.25f;
    samplePeakColumns(click.data(), click.size(), 1.0f, out, kSampleCols);
    CHECK(out[300] == 25 && out[299] == 0 && out[301] == 0, "click: %u %u %u", out[299], out[300], out[301]);
    // Very short samples: fewer samples than columns still fill the columns, a quiet sample shows 1.
    std::vector<float> tiny(10, 0.0f);
    tiny[3] = 0.001f;
    samplePeakColumns(tiny.data(), tiny.size(), 1.0f, out, kSampleCols);
    uint32_t lit = 0;
    for (uint16_t c = 0; c < kSampleCols; c++) lit += (out[c] == 1);
    CHECK(lit > 0 && lit <= 100, "tiny sample lights %u columns", lit);

    // Positions.
    CHECK(sampleXOfPercent(0) == kSampleX && sampleXOfPercent(100) == kSampleX + kSampleCols - 1 && sampleXOfPercent(150) == sampleXOfPercent(100), "x ends");
    CHECK(sampleXOfPercent(50) == kSampleX + 419, "middle %u", sampleXOfPercent(50));
    uint16_t prev = 0;
    for (uint8_t p = 0; p <= 100; p++) {
        CHECK(sampleXOfPercent(p) >= prev && sampleXOfPercent(p) < kSampleX + kSampleCols, "x at %u", p);
        prev = sampleXOfPercent(p);
    }
    CHECK(sampleColumnActive(0, 0, 100) && sampleColumnActive(kSampleCols - 1, 0, 100), "whole sample active");
    CHECK(!sampleColumnActive(10, 20, 80) && sampleColumnActive(sampleXOfPercent(50) - kSampleX, 20, 80) && !sampleColumnActive(kSampleCols - 5, 20, 80), "part active");
    CHECK(sampleColumnActive(sampleXOfPercent(20) - kSampleX, 20, 80) && sampleColumnActive(sampleXOfPercent(80) - kSampleX, 20, 80), "handles are inside");

    // Dirty range.
    uint16_t a = 0, b = 0;
    CHECK(!sampleDirtyColumns(10, 90, 10, 90, a, b), "nothing moved");
    CHECK(sampleDirtyColumns(10, 90, 20, 90, a, b) && a == sampleXOfPercent(10) - kSampleX - 3 && b == sampleXOfPercent(90) - kSampleX + 3, "start moved: %u..%u", a, b);
    CHECK(sampleDirtyColumns(0, 100, 0, 99, a, b) && a == 0 && b == kSampleCols - 1, "ends clamp: %u..%u", a, b);
    CHECK(kSampleCenterY + kSampleHalf < kSampleScaleY && kSampleCenterY - kSampleHalf > kSampleHandleTop && kSampleHandleBottom + 8 <= 440 && kSampleX + kSampleCols < 940, "plot fits the lane area");

    // Time.
    CHECK(sampleSecondsCenti(2.0f, 0, 100, 1.0f) == 200 && sampleSecondsCenti(2.0f, 25, 75, 1.0f) == 100 && sampleSecondsCenti(2.0f, 0, 100, 2.0f) == 100, "time");
    CHECK(sampleSecondsCenti(2.0f, 50, 50, 1.0f) == 0 && sampleSecondsCenti(2.0f, 0, 100, 0.0f) == 0, "time edge cases");

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
