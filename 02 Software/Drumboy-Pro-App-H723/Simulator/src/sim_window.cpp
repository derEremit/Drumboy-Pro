// Visual front panel (SDL3).
//
// The official panel drawing (01 Hardware/Drawing/Drumboy Pro Graphic.ai, a
// PDF) is rendered at the window's pixel size, with CoreGraphics on macOS and
// poppler/cairo on Linux; the LCD, pressed keys, pad and EQ LEDs and knob
// positions are drawn over it from the simulated hardware every frame, so
// presses from scripts and simctl show up as well. Keys respond to the
// keyboard (shortcut printed in each key's corner) and the mouse; knobs turn
// by dragging, scrolling, or holding Option (Alt on Linux) over a knob and
// moving the pointer. Audio plays through the default output.
//
// Layout coordinates are PDF points (page 841.9 x 595.3, origin top-left),
// measured from the drawing.

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#else
#include <cairo.h>
#include <poppler.h>
#endif
#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "sim.h"

namespace sim {

/* Layout -------------------------------------------------------- */

struct Rect {
    float x, y, w, h;
};

// Window shows this part of the page (the panel plus a margin for the help line)
static const Rect kCrop = {30.0f, 66.0f, 782.0f, 454.0f};
static const float kUnits = 1.6f;                               // Logical render units per point

struct Key {
    const char* name;                                           // sim key name (sim_panel.cpp)
    Rect rect;
    SDL_Scancode scancode;
    const char* label;                                          // Shortcut shown in the corner
    int keypad, bit;                                            // For the pressed state
};

static const float kCol[] = {84.5f, 132.5f, 180.5f, 614.5f, 662.5f, 711.0f};
static const float kRow[] = {110.5f, 187.0f, 235.0f, 283.5f, 360.0f, 436.5f};
static const float kKeySize = 47.5f;

#define KEY(name, col, row, scancode, label, keypad, bit) \
    {name, {kCol[col], kRow[row], kKeySize, kKeySize}, scancode, label, keypad, bit}

// Left keypad (0): printed SELECT = ADD, GLOBAL = CENTER. Right keypad (1).
// SONG and KNOB are the two bottom corner keys (by keypad wiring; check on the device).
static const Key kKeys[] = {
    KEY("RESET", 0, 0, SDL_SCANCODE_BACKSPACE, "DEL", 0, 0),
    KEY("PLAYSTOP", 1, 0, SDL_SCANCODE_SPACE, "SPC", 0, 1),
    KEY("REC", 2, 0, SDL_SCANCODE_R, "R", 0, 2),
    KEY("ADD", 0, 1, SDL_SCANCODE_RETURN, "RET", 0, 3),
    KEY("UP", 1, 1, SDL_SCANCODE_UP, "^", 0, 4),
    KEY("CLEAR", 2, 1, SDL_SCANCODE_X, "X", 0, 5),
    KEY("LEFT", 0, 2, SDL_SCANCODE_LEFT, "<", 0, 6),
    KEY("CENTER", 1, 2, SDL_SCANCODE_ESCAPE, "ESC", 0, 7),
    KEY("RIGHT", 2, 2, SDL_SCANCODE_RIGHT, ">", 0, 8),
    KEY("COPY", 0, 3, SDL_SCANCODE_C, "C", 0, 9),
    KEY("DOWN", 1, 3, SDL_SCANCODE_DOWN, "v", 0, 10),
    KEY("PASTE", 2, 3, SDL_SCANCODE_V, "V", 0, 11),
    KEY("MUTE", 0, 4, SDL_SCANCODE_Q, "Q", 0, 12),
    KEY("FILL", 1, 4, SDL_SCANCODE_W, "W", 0, 13),
    KEY("STYLE", 2, 4, SDL_SCANCODE_E, "E", 0, 14),
    KEY("SONG", 0, 5, SDL_SCANCODE_TAB, "TAB", 0, 15),
    KEY("FILE", 3, 0, SDL_SCANCODE_F, "F", 1, 0),
    KEY("DRUMKIT", 4, 0, SDL_SCANCODE_D, "D", 1, 1),
    KEY("MIXER", 5, 0, SDL_SCANCODE_M, "M", 1, 2),
    KEY("RHYTHM", 3, 1, SDL_SCANCODE_T, "T", 1, 3),
    KEY("METRONOME", 4, 1, SDL_SCANCODE_N, "N", 1, 4),
    KEY("IO", 5, 1, SDL_SCANCODE_O, "O", 1, 5),
    KEY("FILTER_0", 3, 2, SDL_SCANCODE_H, "H", 1, 6),
    KEY("FILTER_1", 4, 2, SDL_SCANCODE_J, "J", 1, 7),
    KEY("EQ", 5, 2, SDL_SCANCODE_G, "G", 1, 8),
    KEY("EFFECT_0", 3, 3, SDL_SCANCODE_Y, "Y", 1, 9),
    KEY("EFFECT_1", 4, 3, SDL_SCANCODE_U, "U", 1, 10),
    KEY("REVERB", 5, 3, SDL_SCANCODE_B, "B", 1, 11),
    KEY("LFO", 3, 4, SDL_SCANCODE_L, "L", 1, 12),
    KEY("LAYER_INST", 4, 4, SDL_SCANCODE_I, "I", 1, 13),
    KEY("LAYER_SONG", 5, 4, SDL_SCANCODE_S, "S", 1, 14),
    KEY("KNOB", 5, 5, SDL_SCANCODE_K, "K", 1, 15),
#define PAD(n, scancode, label) {"LAYER_" #n, {181.0f + n * 48.35f, 436.5f, 46.5f, 47.5f}, scancode, label, 2, n}
    PAD(0, SDL_SCANCODE_1, "1"), PAD(1, SDL_SCANCODE_2, "2"), PAD(2, SDL_SCANCODE_3, "3"), PAD(3, SDL_SCANCODE_4, "4"),
    PAD(4, SDL_SCANCODE_5, "5"), PAD(5, SDL_SCANCODE_6, "6"), PAD(6, SDL_SCANCODE_7, "7"), PAD(7, SDL_SCANCODE_8, "8"),
    PAD(8, SDL_SCANCODE_9, "9"), PAD(9, SDL_SCANCODE_0, "0"),
#undef PAD
};
static const int kKeyCount = sizeof(kKeys) / sizeof(kKeys[0]);
static const int kFirstPad = kKeyCount - 10;

// Encoders 0-3 top row, 4-7 bottom row, left to right
static const float kKnobX[] = {306.5f, 383.0f, 459.5f, 536.0f};
static const float kKnobY[] = {122.75f, 395.0f};
static const float kKnobRadius = 22.5f;

static const Rect kScreen = {266.0f, 172.0f, 310.5f, 173.5f};   // Inside the screen outline

// Window shortcuts use the platform's command key: Cmd on macOS, Ctrl elsewhere
#ifdef __APPLE__
static const SDL_Keymod kCommandMod = SDL_KMOD_GUI;
static const char* kHelpLine = "Keys: shortcut in each key's corner, or click.  Knobs: drag, scroll, or hold Option and move.  Cmd-R: power cycle.";
#else
static const SDL_Keymod kCommandMod = SDL_KMOD_CTRL;
static const char* kHelpLine = "Keys: shortcut in each key's corner, or click.  Knobs: drag, scroll, or hold Alt and move.  Ctrl-R: power cycle.";
#endif

// EQ meters: 12 LEDs per side, bit 0 at the bottom
static const float kEqX[] = {51.0f, 790.9f};
static const float kEqTop = 229.3f, kEqStep = 5.255f;
static const SDL_Color kEqColors[4] = {{237, 54, 56, 255}, {247, 152, 54, 255}, {251, 237, 55, 255}, {146, 199, 76, 255}};

/* Coordinates --------------------------------------------------- */

static SDL_FRect toUnits(const Rect& r) {
    return {(r.x - kCrop.x) * kUnits, (r.y - kCrop.y) * kUnits, r.w * kUnits, r.h * kUnits};
}

static float unitsX(float x) { return (x - kCrop.x) * kUnits; }
static float unitsY(float y) { return (y - kCrop.y) * kUnits; }

static bool inside(const Rect& r, float ux, float uy) {
    SDL_FRect f = toUnits(r);
    return ux >= f.x && ux < f.x + f.w && uy >= f.y && uy < f.y + f.h;
}

static int keyAt(float ux, float uy) {
    for (int i = 0; i < kKeyCount; i++) {
        if (inside(kKeys[i].rect, ux, uy)) return i;
    }
    return -1;
}

static int knobAt(float ux, float uy) {
    for (int i = 0; i < 8; i++) {
        float dx = ux - unitsX(kKnobX[i % 4]), dy = uy - unitsY(kKnobY[i / 4]);
        float r = (kKnobRadius + 6.0f) * kUnits;
        if (dx * dx + dy * dy <= r * r) return i;
    }
    return -1;
}

/* Panel art ----------------------------------------------------- */

// Renders the crop rectangle of the drawing at `scale` pixels per point
#ifdef __APPLE__
static SDL_Texture* renderArt(SDL_Renderer* renderer, float scale) {
    const char* path = DRUMBOY_PANEL_ART;
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, reinterpret_cast<const UInt8*>(path), strlen(path), false);
    CGPDFDocumentRef doc = CGPDFDocumentCreateWithURL(url);
    CFRelease(url);
    if (!doc) {
        fprintf(stderr, "sim: cannot open panel drawing %s\n", path);
        return nullptr;
    }
    CGPDFPageRef page = CGPDFDocumentGetPage(doc, 1);
    CGRect box = CGPDFPageGetBoxRect(page, kCGPDFMediaBox);

    int w = static_cast<int>(kCrop.w * scale), h = static_cast<int>(kCrop.h * scale);
    std::vector<uint8_t> pixels(static_cast<size_t>(w) * h * 4);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(pixels.data(), w, h, 8, w * 4, space, kCGImageAlphaPremultipliedLast);
    CGContextSetRGBFillColor(ctx, 0.09, 0.09, 0.10, 1.0);       // Window background around the panel
    CGContextFillRect(ctx, CGRectMake(0, 0, w, h));
    // PDF space is bottom-up: map the crop's top-left to the bitmap's top-left
    CGContextScaleCTM(ctx, scale, scale);
    CGContextTranslateCTM(ctx, -kCrop.x - box.origin.x, -(box.size.height - kCrop.y - kCrop.h) - box.origin.y);
    CGContextDrawPDFPage(ctx, page);
    CGContextRelease(ctx);
    CGColorSpaceRelease(space);
    CGPDFDocumentRelease(doc);

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    SDL_UpdateTexture(texture, nullptr, pixels.data(), w * 4);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    return texture;
}
#else
static SDL_Texture* renderArt(SDL_Renderer* renderer, float scale) {
    const char* path = DRUMBOY_PANEL_ART;
    GError* error = nullptr;
    gchar* uri = g_filename_to_uri(path, nullptr, nullptr);
    PopplerDocument* doc = uri ? poppler_document_new_from_file(uri, nullptr, &error) : nullptr;
    g_free(uri);
    if (!doc) {
        fprintf(stderr, "sim: cannot open panel drawing %s%s%s\n", path, error ? ": " : "", error ? error->message : "");
        if (error) g_error_free(error);
        return nullptr;
    }
    PopplerPage* page = poppler_document_get_page(doc, 0);
    if (!page) {
        fprintf(stderr, "sim: panel drawing %s has no page\n", path);
        g_object_unref(doc);
        return nullptr;
    }

    // Cairo and poppler use the same top-left origin and point units as kCrop
    int w = static_cast<int>(kCrop.w * scale), h = static_cast<int>(kCrop.h * scale);
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t* cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 0.09, 0.09, 0.10);                 // Window background around the panel
    cairo_paint(cr);
    cairo_scale(cr, scale, scale);
    cairo_translate(cr, -kCrop.x, -kCrop.y);
    poppler_page_render(page, cr);
    cairo_destroy(cr);
    cairo_surface_flush(surface);
    g_object_unref(page);
    g_object_unref(doc);

    // CAIRO_FORMAT_ARGB32 is native-endian 0xAARRGGBB, which is SDL's ARGB8888
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
    SDL_UpdateTexture(texture, nullptr, cairo_image_surface_get_data(surface), cairo_image_surface_get_stride(surface));
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    cairo_surface_destroy(surface);
    return texture;
}
#endif

/* Audio --------------------------------------------------------- */

static SDL_AudioStream* audioStream = nullptr;
static std::mutex audioMutex;                                   // The audio thread feeds the stream while the window may close it

static void playBlock(const int32_t* frames, uint32_t count) {
    float out[64 * 2];
    std::lock_guard<std::mutex> lock(audioMutex);
    if (!audioStream || count > 64) return;
    // Keep at most ~60 ms queued: the simulated clock and the sound card drift apart
    if (SDL_GetAudioStreamQueued(audioStream) > static_cast<int>(44100 * 0.06 * sizeof(out[0]) * 2)) return;
    for (uint32_t i = 0; i < count * 2; i++) out[i] = frames[i] / 2147483648.0f;
    SDL_PutAudioStreamData(audioStream, out, static_cast<int>(count * 2 * sizeof(float)));
}

/* Screenshots from other threads -------------------------------- */

static std::atomic<bool> active(false);
static std::mutex shotMutex;
static std::condition_variable shotDone;
static std::string shotPath;
static bool shotPending = false, shotOk = false;

bool windowActive() { return active; }

bool windowScreenshot(const std::string& path) {
    if (!active) return false;
    std::unique_lock<std::mutex> lock(shotMutex);
    shotPath = path;
    shotPending = true;
    if (!shotDone.wait_for(lock, std::chrono::seconds(5), [] { return !shotPending; })) return false;
    return shotOk;
}

static void serviceScreenshot(SDL_Renderer* renderer) {
    std::lock_guard<std::mutex> lock(shotMutex);
    if (!shotPending) return;
    shotOk = false;
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
    if (surface) {
        SDL_Surface* rgb = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGB24);
        if (rgb) {
            std::vector<uint8_t> packed(static_cast<size_t>(rgb->w) * rgb->h * 3);
            for (int y = 0; y < rgb->h; y++) {
                memcpy(&packed[static_cast<size_t>(y) * rgb->w * 3], static_cast<uint8_t*>(rgb->pixels) + y * rgb->pitch, rgb->w * 3);
            }
            shotOk = writePng(shotPath, rgb->w, rgb->h, packed.data());
            SDL_DestroySurface(rgb);
        }
        SDL_DestroySurface(surface);
    }
    shotPending = false;
    shotDone.notify_all();
}

/* Drawing ------------------------------------------------------- */

static void fillRect(SDL_Renderer* r, const Rect& rect, SDL_Color c, float inset = 0) {
    SDL_FRect f = toUnits({rect.x + inset, rect.y + inset, rect.w - 2 * inset, rect.h - 2 * inset});
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &f);
}

static void outlineRect(SDL_Renderer* r, const Rect& rect, SDL_Color c, float inset = 0) {
    SDL_FRect f = toUnits({rect.x + inset, rect.y + inset, rect.w - 2 * inset, rect.h - 2 * inset});
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderRect(r, &f);
    SDL_FRect g = {f.x + 1, f.y + 1, f.w - 2, f.h - 2};
    SDL_RenderRect(r, &g);
}

static void fillCircle(SDL_Renderer* r, float cx, float cy, float radius, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (float dy = -radius; dy <= radius; dy += 1.0f) {
        float dx = std::sqrt(std::max(0.0f, radius * radius - dy * dy));
        SDL_RenderLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void ring(SDL_Renderer* r, float cx, float cy, float radius, SDL_Color c, float width) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (float w = 0; w < width; w += 0.5f) {
        const int steps = 96;
        for (int i = 0; i < steps; i++) {
            float a0 = i * 2 * SDL_PI_F / steps, a1 = (i + 1) * 2 * SDL_PI_F / steps;
            float rr = radius - w;
            SDL_RenderLine(r, cx + rr * std::cos(a0), cy + rr * std::sin(a0), cx + rr * std::cos(a1), cy + rr * std::sin(a1));
        }
    }
}

static void text(SDL_Renderer* r, float x, float y, const char* s, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderDebugText(r, x, y, s);
}

static void drawPanel(SDL_Renderer* r, SDL_Texture* art, SDL_Texture* lcd, int activeKnob, int mouseKey) {
    const float fullW = kCrop.w * kUnits, fullH = kCrop.h * kUnits;
    SDL_FRect all = {0, 0, fullW, fullH};
    SDL_RenderTexture(r, art, nullptr, &all);

    // LCD
    static std::vector<uint16_t> frame(960 * 540);
    uint16_t w = 0, h = 0;
    bool lit = lcdCopyFrame(frame.data(), w, h);
    fillRect(r, kScreen, {0, 0, 0, 255});
    if (lit && w == 960 && h == 540) {
        SDL_UpdateTexture(lcd, nullptr, frame.data(), 960 * 2);
        float sh = kScreen.w * 540.0f / 960.0f;
        SDL_FRect dst = toUnits({kScreen.x, kScreen.y + (kScreen.h - sh) / 2, kScreen.w, sh});
        SDL_RenderTexture(r, lcd, nullptr, &dst);
    }

    // Keys: pressed highlight, pad LEDs, shortcut labels
    uint16_t inputs[3] = {panelKeypadInputs(0), panelKeypadInputs(1), panelKeypadInputs(2)};
    uint16_t padLeds = panelLedOutputs(0x22);
    for (int i = 0; i < kKeyCount; i++) {
        const Key& k = kKeys[i];
        bool pad = i >= kFirstPad;
        if (pad && (padLeds & (1u << k.bit))) {
            fillRect(r, k.rect, {255, 196, 110, 175}, 1.0f);    // Pad LED shining through the translucent cap
        }
        bool pressed = !(inputs[k.keypad] & (1u << k.bit));
        if (pressed) {
            fillRect(r, k.rect, {255, 255, 255, 70}, 1.5f);
            outlineRect(r, k.rect, {120, 230, 255, 255}, 0.5f);
        } else if (i == mouseKey) {
            outlineRect(r, k.rect, {255, 255, 255, 60}, 0.5f);
        }
        SDL_FRect f = toUnits(k.rect);
        float tx = f.x + f.w - 6.0f - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * static_cast<float>(strlen(k.label));
        float ty = f.y + f.h - 6.0f - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
        text(r, tx, ty, k.label, pad ? SDL_Color{30, 30, 30, 255} : SDL_Color{150, 150, 150, 255});
    }

    // EQ meters: cover the printed dots, then draw each LED lit or dim
    uint16_t eq[2] = {panelLedOutputs(0x23), panelLedOutputs(0x24)};
    for (int side = 0; side < 2; side++) {
        fillRect(r, {kEqX[side] - 3.0f, kEqTop - 3.0f, 6.0f, 11 * kEqStep + 6.0f}, {41, 41, 41, 255});
        for (int led = 0; led < 12; led++) {
            int bit = 11 - led;
            SDL_Color c = kEqColors[led / 3];
            if (!(eq[side] & (1u << bit))) c = {static_cast<Uint8>(c.r / 5), static_cast<Uint8>(c.g / 5), static_cast<Uint8>(c.b / 5), 255};
            fillCircle(r, unitsX(kEqX[side]), unitsY(kEqTop + led * kEqStep), 1.6f * kUnits, c);
        }
    }

    // Knobs: pointer line by position (24 detents per turn), highlight while turning
    for (int i = 0; i < 8; i++) {
        float cx = unitsX(kKnobX[i % 4]), cy = unitsY(kKnobY[i / 4]);
        float a = -SDL_PI_F / 2 + panelKnobPosition(i) * (2 * SDL_PI_F / 24);
        float r0 = 4.0f * kUnits, r1 = 10.0f * kUnits;
        SDL_SetRenderDrawColor(r, 235, 235, 235, 255);
        for (float o = -1.0f; o <= 1.0f; o += 0.5f) {
            SDL_RenderLine(r, cx + r0 * std::cos(a) - o * std::sin(a), cy + r0 * std::sin(a) + o * std::cos(a), cx + r1 * std::cos(a) - o * std::sin(a),
                           cy + r1 * std::sin(a) + o * std::cos(a));
        }
        if (i == activeKnob) ring(r, cx, cy, (kKnobRadius + 1.5f) * kUnits, {120, 230, 255, 255}, 3.0f);
        char label[4];
        snprintf(label, sizeof(label), "%d", i + 1);
        text(r, cx + (kKnobRadius - 2) * kUnits, cy + (kKnobRadius - 6) * kUnits, label, {150, 150, 150, 255});
    }

    text(r, 12, 10, kHelpLine, {170, 170, 170, 255});
}

/* Main loop ----------------------------------------------------- */

int windowRun() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "sim: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    const int logicalW = static_cast<int>(kCrop.w * kUnits), logicalH = static_cast<int>(kCrop.h * kUnits);
    SDL_Window* window = SDL_CreateWindow("Drumboy Pro Simulator", logicalW, logicalH, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        fprintf(stderr, "sim: cannot create window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderLogicalPresentation(renderer, logicalW, logicalH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_Texture* lcd = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, 960, 540);
    SDL_SetTextureScaleMode(lcd, SDL_SCALEMODE_LINEAR);
    SDL_Texture* art = nullptr;
    int artWidth = 0;

    SDL_AudioSpec spec = {SDL_AUDIO_F32, 2, 44100};
    audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (audioStream) {
        SDL_ResumeAudioStreamDevice(audioStream);
        audioSetOutput(playBlock);
    } else {
        fprintf(stderr, "sim: no audio output: %s\n", SDL_GetError());
    }
    active = true;

    int mouseKey = -1;                                          // Key held with the mouse
    int dragKnob = -1;                                          // Knob being dragged / grabbed with Option
    bool optionGrab = false;
    float dragAccum = 0;
    float scrollAccum[8] = {};
    const float kUnitsPerDetent = 7.0f * kUnits;                // Drag distance for one detent

    bool running = true;
    while (running) {
        // Re-render the drawing when the window's pixel size changes (Retina, resize)
        int outW = 0, outH = 0;
        SDL_GetCurrentRenderOutputSize(renderer, &outW, &outH);
        int wantW = std::min(outW, static_cast<int>(outH * kCrop.w / kCrop.h));
        if (!art || std::abs(wantW - artWidth) > 8) {
            if (art) SDL_DestroyTexture(art);
            artWidth = wantW;
            art = renderArt(renderer, std::max(1.0f, wantW / kCrop.w));
        }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            SDL_ConvertEventToRenderCoordinates(renderer, &e);
            switch (e.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;

                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP: {
                    bool down = e.type == SDL_EVENT_KEY_DOWN;
                    if (e.key.repeat) break;
                    if ((e.key.mod & kCommandMod) && e.key.scancode == SDL_SCANCODE_Q) {
                        running = false;
                        break;
                    }
                    if ((e.key.mod & kCommandMod) && e.key.scancode == SDL_SCANCODE_R && down) {
                        restart();                              // Power cycle (e.g. after switching plugins); exec releases SDL
                    }
                    if (e.key.mod & kCommandMod) break;          // Other command shortcuts are not panel keys
                    if (e.key.scancode == SDL_SCANCODE_LALT || e.key.scancode == SDL_SCANCODE_RALT) {
                        if (down && dragKnob < 0) {
                            float mx, my, ux, uy;
                            SDL_GetMouseState(&mx, &my);
                            SDL_RenderCoordinatesFromWindow(renderer, mx, my, &ux, &uy);
                            dragKnob = knobAt(ux, uy);
                            if (dragKnob >= 0) {
                                optionGrab = true;
                                dragAccum = 0;
                                SDL_SetWindowRelativeMouseMode(window, true);
                            }
                        } else if (!down && optionGrab) {
                            optionGrab = false;
                            dragKnob = -1;
                            SDL_SetWindowRelativeMouseMode(window, false);
                        }
                        break;
                    }
                    for (const Key& k : kKeys) {
                        if (k.scancode == e.key.scancode) panelKey(k.name, down);
                    }
                    break;
                }

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (e.button.button != SDL_BUTTON_LEFT) break;
                    mouseKey = keyAt(e.button.x, e.button.y);
                    if (mouseKey >= 0) {
                        panelKey(kKeys[mouseKey].name, true);
                    } else {
                        dragKnob = knobAt(e.button.x, e.button.y);
                        dragAccum = 0;
                    }
                    break;

                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (e.button.button != SDL_BUTTON_LEFT) break;
                    if (mouseKey >= 0) panelKey(kKeys[mouseKey].name, false);
                    mouseKey = -1;
                    if (!optionGrab) dragKnob = -1;
                    break;

                case SDL_EVENT_MOUSE_MOTION:
                    if (dragKnob >= 0) {
                        dragAccum += e.motion.xrel - e.motion.yrel;    // Right or up = clockwise
                        int detents = static_cast<int>(dragAccum / kUnitsPerDetent);
                        if (detents) {
                            panelTurnAsync(dragKnob, detents);
                            dragAccum -= detents * kUnitsPerDetent;
                        }
                    }
                    break;

                case SDL_EVENT_MOUSE_WHEEL: {
                    int knob = knobAt(e.wheel.mouse_x, e.wheel.mouse_y);
                    if (knob < 0) break;
                    float dir = (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) ? -1.0f : 1.0f;
                    scrollAccum[knob] += (e.wheel.y + e.wheel.x) * dir;
                    int detents = static_cast<int>(scrollAccum[knob]);
                    if (detents) {
                        panelTurnAsync(knob, detents);
                        scrollAccum[knob] -= detents;
                    }
                    break;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 23, 23, 26, 255);
        SDL_RenderClear(renderer);
        if (art) drawPanel(renderer, art, lcd, dragKnob, mouseKey);
        serviceScreenshot(renderer);
        SDL_RenderPresent(renderer);
    }

    active = false;
    audioSetOutput(nullptr);
    {
        std::lock_guard<std::mutex> lock(audioMutex);
        if (audioStream) SDL_DestroyAudioStream(audioStream);
        audioStream = nullptr;
    }
    SDL_Quit();
    return 0;
}

}  // namespace sim
