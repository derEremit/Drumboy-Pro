// CPU and memory monitor: two small bars in the top-right box of the screen, on every page.
// Hardware-free so it can be tested on a PC (see 02 Software/tests).
//
// CPU is the audio engine's load: the processor cycles one audio block took, as a share of the
// cycles a block lasts (32 samples at 44.1 kHz = 726 us, about 399,000 cycles at 550 MHz). The bar
// shows the worst block of the last quarter second, because one block that takes too long is what
// makes a dropout; it falls back slowly so a short peak can be read. From 80% on it turns magenta.
// The rest of the firmware (screen, SD card, keys) runs in the time the audio leaves over and is
// not part of the figure.
//
// MEM is the sample memory in use: the ten layers' loaded samples as a share of their ten slots
// (5.44 s each).
#ifndef __LOADMETER_H
#define __LOADMETER_H

#include <stdint.h>

// Where it is drawn (the frame image has four dots there, at x 879-891, y 53-64: cleared first).
const uint16_t kMonitorClearX = 845, kMonitorClearY = 36, kMonitorClearW = 88, kMonitorClearH = 56;
const uint16_t kMonitorX = 856, kMonitorBarWidth = 57, kMonitorBarHeight = 2;
const uint16_t kMonitorValueX = 892;  // three digits, ending where the bar ends
const uint16_t kMonitorCpuTextY = 40, kMonitorCpuBarY = 54, kMonitorMemTextY = 66, kMonitorMemBarY = 80;

const uint32_t kMonitorPeriodMs = 250;
const uint32_t kMonitorWarnPermille = 800;  // the load gate of the backlog: under 80% of the block
const uint32_t kMonitorFallPermille = 30;   // the CPU bar falls at most 3% per update

// Filled by the audio interrupt, one call per block.
struct LoadMeter {
    volatile uint32_t sum = 0;     // cycles since the last reading
    volatile uint32_t blocks = 0;  // blocks since the last reading
    volatile uint32_t peak = 0;    // worst block since the last reading

    void add(uint32_t cycles_) {
        sum = sum + cycles_;
        blocks = blocks + 1;
        if (cycles_ > peak) peak = cycles_;
    }
};

struct LoadReading {
    uint32_t average;  // cycles per block
    uint32_t peak;     // cycles of the worst block
    uint32_t blocks;   // 0: nothing measured, average and peak are 0
};

// Reads and restarts the meter. Call with the audio interrupt held off.
inline LoadReading loadMeterTake(LoadMeter& meter_) {
    LoadReading r;
    r.blocks = meter_.blocks;
    r.peak = meter_.peak;
    r.average = (r.blocks > 0) ? (meter_.sum / r.blocks) : 0;
    meter_.sum = 0;
    meter_.blocks = 0;
    meter_.peak = 0;
    return r;
}

// Processor cycles one audio block lasts.
inline uint32_t loadBlockBudget(uint32_t coreClockHz_, uint32_t blockSamples_, uint32_t sampleRateHz_) {
    if (sampleRateHz_ == 0) return 0;
    return static_cast<uint32_t>((static_cast<uint64_t>(coreClockHz_) * blockSamples_) / sampleRateHz_);
}

// A share in tenths of a percent, rounded; more than the whole stays readable up to 999.9%.
inline uint32_t loadPermille(uint32_t part_, uint32_t whole_) {
    if (whole_ == 0) return 0;
    const uint64_t p = (static_cast<uint64_t>(part_) * 1000u + whole_ / 2) / whole_;
    return (p > 9999u) ? 9999u : static_cast<uint32_t>(p);
}

// The CPU bar: rises at once, falls slowly.
inline uint32_t monitorHold(uint32_t shown_, uint32_t now_) {
    if (now_ >= shown_) return now_;
    return ((shown_ - now_) > kMonitorFallPermille) ? (shown_ - kMonitorFallPermille) : now_;
}

// Filled pixels of a bar: anything above zero shows at least 1 px, 100% and more fill it.
inline uint16_t monitorBarFill(uint32_t permille_, uint16_t width_) {
    if (permille_ == 0) return 0;
    if (permille_ >= 1000) return width_;
    const uint16_t w = static_cast<uint16_t>((permille_ * width_ + 500) / 1000);
    return (w == 0) ? 1 : w;
}

// The number beside the label: whole percent, at most three digits.
inline uint16_t monitorPercent(uint32_t permille_) {
    const uint32_t p = (permille_ + 5) / 10;
    return static_cast<uint16_t>((p > 999) ? 999 : p);
}

inline bool monitorWarn(uint32_t permille_) { return permille_ >= kMonitorWarnPermille; }

// Sample memory in use: the loaded samples as a share of count_ slots of slotSize_ samples.
inline uint32_t sampleMemPermille(const uint32_t* sizes_, uint8_t count_, uint32_t slotSize_) {
    uint64_t used = 0;
    for (uint8_t i = 0; i < count_; i++) used += (sizes_[i] > slotSize_) ? slotSize_ : sizes_[i];
    const uint64_t total = static_cast<uint64_t>(count_) * slotSize_;
    if (total == 0) return 0;
    return static_cast<uint32_t>((used * 1000u + total / 2) / total);
}

#endif
