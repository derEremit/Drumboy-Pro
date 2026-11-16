// Sample preview: a WAV file streamed from the SD card through a ring buffer, so a sample can be
// heard while browsing without loading it into a layer. Hardware-free so it can be tested on a
// PC (see 02 Software/tests).
//
// Two sides share the ring:
// - the main loop reads the file in chunks and converts them (previewConvert), the only side
//   that touches the SD card (FatFs is not reentrant);
// - the audio interrupt only reads samples (previewRender). It fades in at the start, fades out
//   on stop, and when the ring runs dry it holds the last sample and fades it out instead of
//   clicking; when data comes back it fades in again.
// One writer and one reader, each owning its own index, so no lock is needed.
//
// The conversion matches the layer loader (layerInst_setSampleLoaded): the first channel only,
// every 2nd/4th frame at 88.2-96 / 176.4-192 kHz, each frame repeated at rates below 44.1 kHz.
#ifndef __PREVIEW_H
#define __PREVIEW_H

#include <stdint.h>
#include <string.h>

struct PreviewFormat {
    uint8_t bits;      // 8, 16, 24 or 32 (float)
    uint8_t channels;  // 1 or 2
    uint32_t rate;     // Hz
};

// Bytes of input consumed per output frame step (skipping frames at high rates).
inline uint32_t previewFrameBytes(const PreviewFormat& f_) {
    const uint32_t rateSkip = (f_.rate >= 176400) ? 4 : ((f_.rate >= 88200) ? 2 : 1);
    return rateSkip * ((f_.channels == 2) ? 2 : 1) * (f_.bits / 8);
}

// How many times each frame is written (rates below 44.1 kHz).
inline uint8_t previewRepeat(const PreviewFormat& f_) {
    if ((f_.rate == 0) || (f_.rate >= 44100)) return 1;
    switch (f_.rate) {
        case 8000: return 6;
        case 11025: return 4;
        case 16000: return 3;
        case 22050: return 2;
        case 24000: return 2;
        default: return static_cast<uint8_t>(44100 / f_.rate);
    }
}

inline float previewDecode(const uint8_t* p_, uint8_t bits_) {
    switch (bits_) {
        case 8:
            return static_cast<float>(static_cast<int>(p_[0]) - 128) / 128.0f;
        case 16:
            return static_cast<float>(static_cast<int16_t>(p_[0] | (p_[1] << 8))) / 32768.0f;
        case 24: {
            int32_t s = p_[0] | (p_[1] << 8) | (p_[2] << 16);
            if (s & 0x800000) s |= static_cast<int32_t>(0xFF000000);
            return static_cast<float>(s) / 8388607.0f;
        }
        case 32: {
            float v;
            memcpy(&v, p_, 4);
            return v;
        }
        default:
            return 0.0f;
    }
}

// Ring of floats; size must be a power of two. write/read count up forever and wrap with the mask.
struct PreviewRing {
    float* buf;
    uint32_t mask;
    volatile uint32_t write;
    volatile uint32_t read;
};

inline uint32_t previewRingCount(const PreviewRing& r_) { return r_.write - r_.read; }
inline uint32_t previewRingFree(const PreviewRing& r_) { return (r_.mask + 1) - previewRingCount(r_); }

// Main-loop side: converts whole frames from raw_ into the ring while there is room. Returns the
// bytes consumed (a multiple of the frame size); the caller keeps the rest for the next call.
inline uint32_t previewConvert(PreviewRing& r_, const uint8_t* raw_, uint32_t bytes_, const PreviewFormat& f_) {
    const uint32_t step = previewFrameBytes(f_);
    const uint8_t repeat = previewRepeat(f_);
    if ((step == 0) || ((f_.bits != 8) && (f_.bits != 16) && (f_.bits != 24) && (f_.bits != 32))) return bytes_;
    uint32_t used = 0;
    uint32_t w = r_.write;
    while ((used + step <= bytes_) && ((r_.mask + 1) - (w - r_.read) >= repeat)) {
        const float v = previewDecode(raw_ + used, f_.bits);
        for (uint8_t k = 0; k < repeat; k++) r_.buf[(w++) & r_.mask] = v;
        used += step;
    }
    r_.write = w;  // publish after the samples are written
    return used;
}

enum PreviewState : uint8_t { PREVIEW_IDLE = 0, PREVIEW_PLAY = 1, PREVIEW_STOP = 2 };

const float kPreviewRampStep = 1.0f / 64.0f;  // fades take 64 samples (1.5 ms)

struct PreviewVoice {
    volatile uint8_t state;      // PreviewState; the main loop sets PLAY and STOP, the ISR sets IDLE
    volatile bool sourceDone;    // main loop: the whole file is in the ring
    float ramp;                  // 0..1
    float last;                  // last sample read, held while the ring is dry
    volatile uint32_t underruns; // times the ring ran dry before the end (diagnostics)
    bool starved;
};

// Audio side: n_ samples of the preview (gain_ applied) into out_.
inline void previewRender(PreviewRing& r_, PreviewVoice& v_, float* out_, uint16_t n_, float gain_) {
    uint8_t state = v_.state;
    for (uint16_t i = 0; i < n_; i++) {
        if (state == PREVIEW_IDLE) {
            out_[i] = 0.0f;
            continue;
        }
        const bool has = r_.write != r_.read;
        if (has) {
            v_.last = r_.buf[r_.read & r_.mask];
            r_.read = r_.read + 1;
            v_.starved = false;
        } else if (!v_.starved && !v_.sourceDone) {
            v_.starved = true;
            v_.underruns = v_.underruns + 1;
        }
        // Fade in while playing with data; fade out on stop, at the end, or while starved.
        const bool fadeIn = (state == PREVIEW_PLAY) && has;
        if (fadeIn) {
            v_.ramp += kPreviewRampStep;
            if (v_.ramp > 1.0f) v_.ramp = 1.0f;
        } else {
            v_.ramp -= kPreviewRampStep;
            if (v_.ramp <= 0.0f) {
                v_.ramp = 0.0f;
                if ((state == PREVIEW_STOP) || (!has && v_.sourceDone)) {
                    state = PREVIEW_IDLE;
                    v_.state = PREVIEW_IDLE;
                }
            }
        }
        out_[i] = v_.last * v_.ramp * gain_;
    }
}

#endif
