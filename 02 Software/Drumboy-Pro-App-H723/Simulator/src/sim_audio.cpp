// Audio: a 44.1 kHz clock that drives the I2S DMA callbacks.
//
// Once the firmware starts HAL_I2SEx_TransmitReceive_DMA, a thread calls
// HAL_I2SEx_TxRxHalfCpltCallback and HAL_I2SEx_TxRxCpltCallback in turn every
// 32 frames, as interrupts, exactly like the circular DMA. After each call the
// half the firmware just rendered (int32 stereo) goes into a 60 s history used
// for levels and WAV recordings. The input (line in) stays silent.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

#include "Global.h"
#include "i2s.h"
#include "sim.h"

extern uint32_t i2s_txBuffer[];

namespace sim {

static const uint32_t kSampleRate = 44100;
static const uint32_t kHistoryFrames = kSampleRate * 60;

static std::mutex historyMutex;
static std::vector<int32_t> history(kHistoryFrames * 2);        // Interleaved L/R ring
static uint64_t framesWritten = 0;
static std::atomic<uint64_t> blocks(0);

uint64_t audioBlocks() { return blocks; }

static std::atomic<void (*)(const int32_t*, uint32_t)> outputSink(nullptr);

void audioSetOutput(void (*output)(const int32_t* frames, uint32_t count)) { outputSink = output; }

static void storeBlock(const uint32_t* half) {
    std::lock_guard<std::mutex> lock(historyMutex);
    for (uint32_t i = 0; i < I2S_BLOCK_SIZE; i++) {
        uint32_t pos = static_cast<uint32_t>(framesWritten % kHistoryFrames);
        history[pos * 2] = static_cast<int32_t>(half[i * 2]);
        history[pos * 2 + 1] = static_cast<int32_t>(half[i * 2 + 1]);
        framesWritten++;
    }
    auto sink = outputSink.load();
    if (sink) sink(reinterpret_cast<const int32_t*>(half), I2S_BLOCK_SIZE);
}

void audioStart() {
    static std::atomic<bool> started(false);
    if (started.exchange(true)) return;
    std::thread([] {
        using clock = std::chrono::steady_clock;
        const auto period = std::chrono::nanoseconds(1000000000ULL * I2S_BLOCK_SIZE / kSampleRate);
        auto next = clock::now();
        bool firstHalf = true;
        while (true) {
            next += period;
            std::this_thread::sleep_until(next);
            if (clock::now() - next > std::chrono::milliseconds(100)) next = clock::now();  // Resync after a stall

            if (hi2s3.State != HAL_I2S_STATE_BUSY_TX_RX) continue;
            if (firstHalf) {
                runIsr([] { HAL_I2SEx_TxRxHalfCpltCallback(&hi2s3); });
                storeBlock(&i2s_txBuffer[0]);
            } else {
                runIsr([] { HAL_I2SEx_TxRxCpltCallback(&hi2s3); });
                storeBlock(&i2s_txBuffer[I2S_BLOCK_SIZE * 2]);
            }
            firstHalf = !firstHalf;
            blocks++;
        }
    }).detach();
}

// Copies the most recent frames (oldest first)
static std::vector<int32_t> recentFrames(uint32_t frames) {
    std::lock_guard<std::mutex> lock(historyMutex);
    frames = static_cast<uint32_t>(std::min<uint64_t>({frames, framesWritten, kHistoryFrames}));
    std::vector<int32_t> out(frames * 2);
    uint64_t start = framesWritten - frames;
    for (uint32_t i = 0; i < frames; i++) {
        uint32_t pos = static_cast<uint32_t>((start + i) % kHistoryFrames);
        out[i * 2] = history[pos * 2];
        out[i * 2 + 1] = history[pos * 2 + 1];
    }
    return out;
}

static float toDb(double value) { return value > 0 ? static_cast<float>(20.0 * log10(value)) : -200.0f; }

AudioLevel audioLevel(uint32_t ms) {
    std::vector<int32_t> frames = recentFrames(kSampleRate / 1000 * ms);
    AudioLevel level{};
    for (int ch = 0; ch < 2; ch++) {
        double peak = 0, sum = 0;
        size_t n = frames.size() / 2;
        for (size_t i = 0; i < n; i++) {
            double v = frames[i * 2 + ch] / 2147483648.0;
            peak = std::max(peak, std::fabs(v));
            sum += v * v;
        }
        level.peakDb[ch] = toDb(peak);
        level.rmsDb[ch] = toDb(n ? std::sqrt(sum / n) : 0);
    }
    return level;
}

static void putLe(FILE* f, uint32_t value, int bytes) {
    for (int i = 0; i < bytes; i++) fputc((value >> (8 * i)) & 0xFF, f);
}

bool audioRecord(const std::string& path, uint32_t ms) {
    ms = std::min<uint32_t>(ms, 60000);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    std::vector<int32_t> frames = recentFrames(kSampleRate / 1000 * ms);

    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    uint32_t dataBytes = static_cast<uint32_t>(frames.size() * 4);
    fwrite("RIFF", 1, 4, f);
    putLe(f, 36 + dataBytes, 4);
    fwrite("WAVEfmt ", 1, 8, f);
    putLe(f, 16, 4);
    putLe(f, 1, 2);                                             // PCM
    putLe(f, 2, 2);                                             // Stereo
    putLe(f, kSampleRate, 4);
    putLe(f, kSampleRate * 8, 4);
    putLe(f, 8, 2);
    putLe(f, 32, 2);
    fwrite("data", 1, 4, f);
    putLe(f, dataBytes, 4);
    for (int32_t sample : frames) putLe(f, static_cast<uint32_t>(sample), 4);
    return fclose(f) == 0;
}

}  // namespace sim
