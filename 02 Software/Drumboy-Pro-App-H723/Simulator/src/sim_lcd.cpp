// LCD controller (NT35516, 540 x 960, RGB565 over a 16-bit 8080 bus).
//
// sim_gpio.cpp passes every WR strobe here. The decoder follows the commands
// Lcd.cpp uses: column/row address set (one byte per command, 0x2A00..03 and
// 0x2B00..03), memory write (0x2C00/0x3C00, then a pixel stream that wraps
// inside the window), address mode (0x3600, MV swaps to landscape) and
// display on/off. The framebuffer is kept in the firmware's logical
// coordinates, which is how the screen reads on the device.

#include <zlib.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <vector>

#include "sim.h"

namespace sim {

static const uint16_t kNativeWidth = 540;
static const uint16_t kNativeHeight = 960;
static const uint16_t kMaxSide = 960;
static const uint8_t kMadctlMv = 0x20;

static uint16_t framebuffer[kMaxSide * kMaxSide];

static struct {
    uint16_t command = 0;
    bool memoryWrite = false;
    uint16_t xs = 0, xe = kNativeWidth - 1, ys = 0, ye = kNativeHeight - 1;
    uint16_t x = 0, y = 0;
    uint8_t madctl = 0;
    bool displayOn = false;
} lcd;

static std::atomic<bool> backlight(false);
static std::atomic<uint64_t> busWrites(0), pixels(0);

static uint16_t width() { return (lcd.madctl & kMadctlMv) ? kNativeHeight : kNativeWidth; }
static uint16_t height() { return (lcd.madctl & kMadctlMv) ? kNativeWidth : kNativeHeight; }

static void setByte(uint16_t& value, bool high, uint16_t data) {
    value = high ? static_cast<uint16_t>((value & 0x00FF) | ((data & 0xFF) << 8)) : static_cast<uint16_t>((value & 0xFF00) | (data & 0xFF));
}

void lcdBusWrite(bool command, uint16_t value) {
    busWrites++;
    if (command) {
        lcd.command = value;
        lcd.memoryWrite = false;
        switch (value) {
            case 0x2C00:                                        // Write memory start
                lcd.x = lcd.xs;
                lcd.y = lcd.ys;
                lcd.memoryWrite = true;
                break;
            case 0x3C00:                                        // Write memory continue
                lcd.memoryWrite = true;
                break;
            case 0x2900:
                lcd.displayOn = true;
                break;
            case 0x2800:
                lcd.displayOn = false;
                break;
        }
        return;
    }

    if (lcd.memoryWrite) {
        if (lcd.x < kMaxSide && lcd.y < kMaxSide) framebuffer[lcd.y * kMaxSide + lcd.x] = value;
        pixels++;
        if (++lcd.x > lcd.xe) {
            lcd.x = lcd.xs;
            if (++lcd.y > lcd.ye) lcd.y = lcd.ys;
        }
        return;
    }

    switch (lcd.command) {
        case 0x2A00: setByte(lcd.xs, true, value); break;
        case 0x2A01: setByte(lcd.xs, false, value); break;
        case 0x2A02: setByte(lcd.xe, true, value); break;
        case 0x2A03: setByte(lcd.xe, false, value); break;
        case 0x2B00: setByte(lcd.ys, true, value); break;
        case 0x2B01: setByte(lcd.ys, false, value); break;
        case 0x2B02: setByte(lcd.ye, true, value); break;
        case 0x2B03: setByte(lcd.ye, false, value); break;
        case 0x3600: lcd.madctl = value & 0xFF; break;
    }
}

void lcdSetBacklight(bool on) { backlight = on; }

LcdInfo lcdInfo() { return {width(), height(), lcd.displayOn, backlight, busWrites, pixels}; }

uint32_t lcdChecksum() {
    uint32_t hash = 2166136261u;
    for (uint16_t y = 0; y < height(); y++) {
        for (uint16_t x = 0; x < width(); x++) {
            hash = (hash ^ framebuffer[y * kMaxSide + x]) * 16777619u;
        }
    }
    return hash;
}

/* PNG ----------------------------------------------------------- */

static void putBe32(std::vector<uint8_t>& out, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<uint8_t>(value >> shift));
}

static void putChunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    putBe32(out, static_cast<uint32_t>(data.size()));
    size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    putBe32(out, static_cast<uint32_t>(crc32(0, out.data() + start, static_cast<uInt>(out.size() - start))));
}

bool lcdCopyFrame(uint16_t* rgb565, uint16_t& w, uint16_t& h) {
    w = width();
    h = height();
    for (uint16_t y = 0; y < h; y++) memcpy(rgb565 + y * w, framebuffer + y * kMaxSide, w * sizeof(uint16_t));
    return lcd.displayOn && backlight;
}

bool lcdScreenshot(const std::string& path) {
    const uint16_t w = width(), h = height();
    bool lit = lcd.displayOn && backlight;
    std::vector<uint8_t> rgb;
    rgb.reserve(w * h * 3);
    for (uint16_t y = 0; y < h; y++) {
        for (uint16_t x = 0; x < w; x++) {
            uint16_t c = lit ? framebuffer[y * kMaxSide + x] : 0;
            uint8_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
            rgb.push_back(static_cast<uint8_t>((r << 3) | (r >> 2)));
            rgb.push_back(static_cast<uint8_t>((g << 2) | (g >> 4)));
            rgb.push_back(static_cast<uint8_t>((b << 3) | (b >> 2)));
        }
    }
    return writePng(path, w, h, rgb.data());
}

bool writePng(const std::string& path, uint32_t w, uint32_t h, const uint8_t* rgb) {
    // Rows of RGB8 pixels, each with filter byte 0
    std::vector<uint8_t> raw;
    raw.reserve((w * 3 + 1) * h);
    for (uint32_t y = 0; y < h; y++) {
        raw.push_back(0);
        raw.insert(raw.end(), rgb + y * w * 3, rgb + (y + 1) * w * 3);
    }

    uLongf zsize = compressBound(static_cast<uLong>(raw.size()));
    std::vector<uint8_t> z(zsize);
    if (compress2(z.data(), &zsize, raw.data(), static_cast<uLong>(raw.size()), 6) != Z_OK) return false;
    z.resize(zsize);

    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<uint8_t> ihdr;
    putBe32(ihdr, w);
    putBe32(ihdr, h);
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});                   // 8-bit RGB
    putChunk(png, "IHDR", ihdr);
    putChunk(png, "IDAT", z);
    putChunk(png, "IEND", {});

    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(png.data(), 1, png.size(), f) == png.size();
    return (fclose(f) == 0) && ok;
}

}  // namespace sim
