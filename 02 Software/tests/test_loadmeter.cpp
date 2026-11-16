// Host-side tests for the CPU and memory monitor (LoadMeter.h).

#include <cstdio>
#include <cstdlib>

#include "LoadMeter.h"

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
    // The block budget of the device: 550 MHz, 32 samples at 44.1 kHz.
    const uint32_t budget = loadBlockBudget(550000000u, 32, 44100);
    CHECK(budget == 399092, "budget %u", budget);
    CHECK(loadBlockBudget(550000000u, 32, 0) == 0, "no sample rate");

    // Shares.
    CHECK(loadPermille(0, budget) == 0, "idle");
    CHECK(loadPermille(budget, budget) == 1000, "full");
    CHECK(loadPermille(budget / 2, budget) == 500, "half %u", loadPermille(budget / 2, budget));
    CHECK(loadPermille(135691, budget) == 340, "34%% of the block: %u", loadPermille(135691, budget));
    CHECK(loadPermille(0xFFFFFFFFu, budget) == 9999, "overload is capped");
    CHECK(loadPermille(5, 0) == 0, "no budget");

    // The meter: sum, count and worst block; a reading restarts it.
    LoadMeter meter;
    LoadReading r = loadMeterTake(meter);
    CHECK(r.blocks == 0 && r.average == 0 && r.peak == 0, "empty meter");
    meter.add(100);
    meter.add(300);
    meter.add(200);
    r = loadMeterTake(meter);
    CHECK(r.blocks == 3 && r.average == 200 && r.peak == 300, "blocks %u avg %u peak %u", r.blocks, r.average, r.peak);
    r = loadMeterTake(meter);
    CHECK(r.blocks == 0 && r.peak == 0, "restarted");
    // A quarter second of blocks at full load does not overflow the sum.
    for (int i = 0; i < 345; i++) meter.add(budget);
    r = loadMeterTake(meter);
    CHECK(r.blocks == 345 && r.average == budget && r.peak == budget, "quarter second avg %u", r.average);

    // The CPU bar rises at once and falls 3% per update.
    CHECK(monitorHold(200, 650) == 650, "rises at once");
    CHECK(monitorHold(650, 200) == 620, "falls slowly: %u", monitorHold(650, 200));
    CHECK(monitorHold(210, 200) == 200, "lands on the value");
    uint32_t shown = 900;
    int updates = 0;
    while (shown != 300 && updates < 100) {
        shown = monitorHold(shown, 300);
        updates++;
    }
    CHECK(updates == 20, "900 to 300 in %d updates", updates);

    // Bar pixels.
    CHECK(monitorBarFill(0, kMonitorBarWidth) == 0, "empty");
    CHECK(monitorBarFill(1, kMonitorBarWidth) == 1, "anything shows");
    CHECK(monitorBarFill(500, kMonitorBarWidth) == 29, "half: %u", monitorBarFill(500, kMonitorBarWidth));
    CHECK(monitorBarFill(999, kMonitorBarWidth) == 57, "nearly full");
    CHECK(monitorBarFill(1000, kMonitorBarWidth) == 57 && monitorBarFill(4000, kMonitorBarWidth) == 57, "full and over");
    uint16_t prev = 0;
    for (uint32_t p = 0; p <= 1200; p++) {
        const uint16_t w = monitorBarFill(p, kMonitorBarWidth);
        CHECK(w >= prev && w <= kMonitorBarWidth, "bar at %u: %u", p, w);
        prev = w;
    }

    // The number and the warning.
    CHECK(monitorPercent(0) == 0 && monitorPercent(344) == 34 && monitorPercent(345) == 35 && monitorPercent(1000) == 100, "percent");
    CHECK(monitorPercent(9999) == 999, "three digits at most");
    CHECK(!monitorWarn(799) && monitorWarn(800) && monitorWarn(1500), "warning from 80%%");

    // The layout: the number ends where the bar ends (three cells of the 5x7 font, 7 px each),
    // and the cleared box covers the texts, the bars and the frame image's four dots.
    CHECK(kMonitorValueX + 3 * 7 == kMonitorX + kMonitorBarWidth, "number and bar end together");
    CHECK(kMonitorClearX <= kMonitorX && kMonitorClearX + kMonitorClearW >= kMonitorX + kMonitorBarWidth, "box covers the bars");
    CHECK(kMonitorClearY <= kMonitorCpuTextY && kMonitorClearY + kMonitorClearH >= kMonitorMemBarY + kMonitorBarHeight, "box covers both rows");
    CHECK(kMonitorClearX <= 879 && kMonitorClearX + kMonitorClearW > 891 && kMonitorClearY <= 53 && kMonitorClearY + kMonitorClearH > 64, "box covers the dots");
    CHECK(kMonitorClearX > 830 && kMonitorClearX + kMonitorClearW < 940 && kMonitorClearY > 29 && kMonitorClearY + kMonitorClearH < 99, "box stays inside its frame");

    // Sample memory: ten slots of 240,000 samples.
    const uint32_t slot = 240000;
    uint32_t sizes[10] = {0};
    CHECK(sampleMemPermille(sizes, 10, slot) == 0, "no samples");
    sizes[0] = slot;
    CHECK(sampleMemPermille(sizes, 10, slot) == 100, "one full slot");
    sizes[1] = slot / 2;
    CHECK(sampleMemPermille(sizes, 10, slot) == 150, "one and a half");
    for (int i = 0; i < 10; i++) sizes[i] = slot;
    CHECK(sampleMemPermille(sizes, 10, slot) == 1000, "all full");
    sizes[3] = slot * 3;
    CHECK(sampleMemPermille(sizes, 10, slot) == 1000, "a size beyond its slot counts as the slot");
    CHECK(sampleMemPermille(sizes, 0, slot) == 0 && sampleMemPermille(sizes, 10, 0) == 0, "nothing to measure");

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
