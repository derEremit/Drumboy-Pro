// Host-side tests for the sample preview stream (Core/Library/Global/Preview.h).
//
// The conversion is checked against a direct model of the layer loader; the voice is checked
// for clicks (no step between output samples larger than the signal's own steps plus the fade
// step) at start, stop, underrun and end, and the stream is checked end to end with a writer
// that pauses at random.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <random>
#include <vector>

#include "Preview.h"

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

static std::mt19937 rng(99);

// Loader model: layerInst_setSampleLoaded's loop.
static std::vector<float> loaderModel(const std::vector<uint8_t>& raw, const PreviewFormat& f) {
    std::vector<float> out;
    const uint32_t step = previewFrameBytes(f);
    const uint8_t rep = previewRepeat(f);
    for (uint32_t j = 0; j + step <= raw.size(); j += step) {
        const float v = previewDecode(&raw[j], f.bits);
        for (uint8_t k = 0; k < rep; k++) out.push_back(v);
    }
    return out;
}

static void testDecode() {
    uint8_t b[4];
    b[0] = 0; CHECK(previewDecode(b, 8) == -1.0f, "8-bit min");
    b[0] = 128; CHECK(previewDecode(b, 8) == 0.0f, "8-bit zero");
    b[0] = 0x00; b[1] = 0x80; CHECK(previewDecode(b, 16) == -1.0f, "16-bit min");
    b[0] = 0xFF; b[1] = 0x7F; CHECK(std::fabs(previewDecode(b, 16) - 32767.0f / 32768.0f) < 1e-7f, "16-bit max");
    b[0] = 0x00; b[1] = 0x00; b[2] = 0x80; CHECK(std::fabs(previewDecode(b, 24) + 8388608.0f / 8388607.0f) < 1e-6f, "24-bit min (sign extended)");
    b[0] = 0xFF; b[1] = 0xFF; b[2] = 0x7F; CHECK(previewDecode(b, 24) == 1.0f, "24-bit max");
    const float x = 0.375f; memcpy(b, &x, 4); CHECK(previewDecode(b, 32) == 0.375f, "float");
    CHECK(previewFrameBytes({16, 2, 44100}) == 4 && previewFrameBytes({24, 2, 96000}) == 12 && previewFrameBytes({16, 1, 192000}) == 8, "frame steps");
    CHECK(previewRepeat({16, 1, 8000}) == 6 && previewRepeat({16, 1, 22050}) == 2 && previewRepeat({16, 1, 48000}) == 1, "repeats");
}

static void testConvertMatchesLoader() {
    const PreviewFormat formats[] = {{8, 1, 8000}, {16, 1, 44100}, {16, 2, 44100}, {24, 2, 48000}, {24, 1, 96000}, {32, 2, 22050}, {16, 2, 192000}, {8, 2, 11025}};
    std::vector<float> ringBuf(1 << 12);
    for (const auto& f : formats) {
        std::vector<uint8_t> raw(3000 + rng() % 5000);
        for (auto& c : raw) c = static_cast<uint8_t>(rng());
        if (f.bits == 32) {  // keep floats finite
            for (size_t i = 0; i + 4 <= raw.size(); i += 4) { const float v = (rng() % 2001 - 1000) / 1000.0f; memcpy(&raw[i], &v, 4); }
        }
        const auto want = loaderModel(raw, f);
        PreviewRing r{ringBuf.data(), static_cast<uint32_t>(ringBuf.size() - 1), 0, 0};
        std::vector<float> got;
        // Feed in odd-sized chunks, drain at random, keep the unconsumed bytes like the firmware.
        size_t pos = 0;
        std::vector<uint8_t> pend;
        while (pos < raw.size() || !pend.empty()) {
            const size_t take = std::min<size_t>(raw.size() - pos, 1 + rng() % 700);
            pend.insert(pend.end(), raw.begin() + pos, raw.begin() + pos + take);
            pos += take;
            const uint32_t used = previewConvert(r, pend.data(), static_cast<uint32_t>(pend.size()), f);
            pend.erase(pend.begin(), pend.begin() + used);
            const uint32_t drain = rng() % (previewRingCount(r) + 1);
            for (uint32_t k = 0; k < drain; k++) { got.push_back(r.buf[r.read & r.mask]); r.read = r.read + 1; }
            CHECK(previewRingCount(r) <= r.mask + 1, "ring overfilled");
            if (pos == raw.size() && pend.size() < previewFrameBytes(f)) pend.clear();
        }
        while (r.write != r.read) { got.push_back(r.buf[r.read & r.mask]); r.read = r.read + 1; }
        CHECK(got == want, "bits %u ch %u rate %u: %zu vs %zu samples", f.bits, f.channels, f.rate, got.size(), want.size());
    }
}

// The largest step between neighbouring output samples allowed for a signal x in [-1, 1]:
// the signal's own step plus what one fade step can add.
static void testVoice() {
    std::vector<float> ringBuf(1 << 10);
    PreviewRing r{ringBuf.data(), static_cast<uint32_t>(ringBuf.size() - 1), 0, 0};
    PreviewVoice v{};
    float out[32];
    // A full-scale DC signal: any jump other than a fade step would be a click.
    auto push = [&](int n) { for (int i = 0; i < n; i++) { r.buf[r.write & r.mask] = 1.0f; r.write = r.write + 1; } };
    std::vector<float> all;
    push(200);
    v.state = PREVIEW_PLAY;
    for (int b = 0; b < 4; b++) { previewRender(r, v, out, 32, 1.0f); all.insert(all.end(), out, out + 32); }
    CHECK(all[0] <= kPreviewRampStep + 1e-6f, "starts with a fade-in: %f", all[0]);
    CHECK(std::fabs(all[100] - 1.0f) < 1e-6f, "full level after the fade");
    // Underrun: ring runs dry after 200 samples (block 7); output fades instead of dropping.
    for (int b = 0; b < 6; b++) { previewRender(r, v, out, 32, 1.0f); all.insert(all.end(), out, out + 32); }
    CHECK(v.underruns == 1 && v.state == PREVIEW_PLAY, "one underrun, still playing");
    CHECK(all.back() == 0.0f, "faded to silence while starved");
    // Data comes back: fades in again.
    push(300);
    for (int b = 0; b < 6; b++) { previewRender(r, v, out, 32, 1.0f); all.insert(all.end(), out, out + 32); }
    // Stop: fades out and goes idle.
    v.state = PREVIEW_STOP;
    for (int b = 0; b < 4; b++) { previewRender(r, v, out, 32, 1.0f); all.insert(all.end(), out, out + 32); }
    CHECK(v.state == PREVIEW_IDLE && all.back() == 0.0f, "stop fades out and goes idle");
    float maxStep = 0.0f;
    for (size_t i = 1; i < all.size(); i++) maxStep = std::max(maxStep, std::fabs(all[i] - all[i - 1]));
    CHECK(maxStep <= kPreviewRampStep + 1e-6f, "no clicks: largest step %f", maxStep);

    // End of file: drains the ring, then fades out and goes idle by itself.
    r.read = r.write = 0;
    v = PreviewVoice{};
    push(100);
    v.sourceDone = true;
    v.state = PREVIEW_PLAY;
    int blocks = 0;
    while (v.state != PREVIEW_IDLE && blocks < 20) { previewRender(r, v, out, 32, 1.0f); blocks++; }
    CHECK(v.state == PREVIEW_IDLE && v.underruns == 0 && r.read == r.write, "ends by itself after %d blocks", blocks);

    // Idle outputs silence and reads nothing.
    push(10);
    const uint32_t before = r.read;
    previewRender(r, v, out, 32, 1.0f);
    CHECK(r.read == before && out[0] == 0.0f && out[31] == 0.0f, "idle is silent");
}

// End to end: a sine streamed by a writer that pauses at random comes out as the same sine,
// apart from fades.
static void testStream() {
    std::vector<float> ringBuf(1 << 11);
    PreviewRing r{ringBuf.data(), static_cast<uint32_t>(ringBuf.size() - 1), 0, 0};
    PreviewVoice v{};
    const PreviewFormat f{16, 1, 44100};
    std::vector<uint8_t> raw(2 * 44100);
    for (size_t i = 0; i < raw.size() / 2; i++) {
        const int16_t s = static_cast<int16_t>(20000 * std::sin(i * 0.05));
        raw[2 * i] = static_cast<uint8_t>(s & 0xFF), raw[2 * i + 1] = static_cast<uint8_t>((s >> 8) & 0xFF);
    }
    size_t pos = 0;
    v.state = PREVIEW_PLAY;
    float out[32], maxStep = 0.0f, prev = 0.0f;
    int blocks = 0;
    while (v.state != PREVIEW_IDLE && blocks < 10000) {
        if (rng() % 4) {  // the main loop gets to run three times out of four
            const uint32_t chunk = std::min<uint32_t>(static_cast<uint32_t>(raw.size() - pos), 512);
            pos += previewConvert(r, raw.data() + pos, chunk, f);
            if (pos >= raw.size()) v.sourceDone = true;
        }
        previewRender(r, v, out, 32, 1.0f);
        for (float s : out) { maxStep = std::max(maxStep, std::fabs(s - prev)); prev = s; }
        blocks++;
    }
    CHECK(v.state == PREVIEW_IDLE && r.read == r.write && pos == raw.size(), "whole file played (%d blocks)", blocks);
    CHECK(maxStep < 0.05f, "smooth output, largest step %f", maxStep);  // sine step ~0.03 + fade
}

// Throughput: a 48 kHz 24-bit stereo file (6 bytes per frame) with a main loop that only gets to
// run once every ~20 ms (28 audio blocks). One 4 KB read per pass (V1.11 first build) runs the
// ring dry again and again; up to 4 x 12 KB per pass while the ring is under half full keeps up.
static uint32_t streamUnderruns(uint32_t chunkBytes, int passes, int blocksPerLoop) {
    std::vector<float> ringBuf(65536);
    PreviewRing r{ringBuf.data(), static_cast<uint32_t>(ringBuf.size() - 1), 0, 0};
    PreviewVoice v{};
    const PreviewFormat f{24, 2, 48000};
    const uint32_t step = previewFrameBytes(f);
    std::vector<uint8_t> raw(6 * 48000 * 4);  // 4 s
    for (auto& c : raw) c = static_cast<uint8_t>(rng());
    size_t pos = 0;
    auto service = [&]() {
        for (int k = 0; k < passes && pos < raw.size(); k++) {
            if (k > 0 && previewRingCount(r) >= (r.mask + 1) / 2) break;
            uint32_t chunk = (chunkBytes / step) * step;
            if (chunk > raw.size() - pos) chunk = static_cast<uint32_t>(raw.size() - pos);
            if (previewRingFree(r) < chunk / step) break;
            pos += previewConvert(r, raw.data() + pos, chunk, f);
        }
        if (pos >= raw.size()) v.sourceDone = true;
    };
    service();
    v.state = PREVIEW_PLAY;
    float out[32];
    for (int b = 0; b < 8000 && v.state != PREVIEW_IDLE; b++) {
        if (b % blocksPerLoop == 0) service();
        previewRender(r, v, out, 32, 1.0f);
    }
    return v.underruns;
}

static void testThroughput() {
    const uint32_t before = streamUnderruns(4096, 1, 28);
    const uint32_t after = streamUnderruns(12288, 4, 28);
    CHECK(before > 0, "4 KB per pass should starve a 24-bit stereo stream (%u underruns)", before);
    CHECK(after == 0, "4 x 12 KB per pass keeps up (%u underruns)", after);
    std::printf("48k/24-bit stereo, main loop every 20 ms: %u underruns with 4 KB/pass, %u with 4 x 12 KB\n", before, after);
}

int main() {
    testThroughput();
    testDecode();
    testConvertMatchesLoader();
    testVoice();
    testStream();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
