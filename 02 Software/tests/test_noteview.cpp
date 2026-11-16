// Host-side tests for the step edit screen geometry (NoteView.h).

#include <cstdio>
#include <vector>

#include "NoteView.h"

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
    // x positions
    CHECK(noteViewX(0, 3200) == kNoteViewX && noteViewX(3199, 3200) == kNoteViewX + 850, "x ends: %u", noteViewX(3199, 3200));
    CHECK(noteViewX(1600, 3200) == kNoteViewX + 425 && noteViewX(99999, 3200) == noteViewX(3199, 3200) && noteViewX(5, 0) == kNoteViewX, "x middle");
    uint16_t prev = 0;
    for (uint32_t i = 0; i < 3200; i += 7) {
        CHECK(noteViewX(i, 3200) >= prev && noteViewX(i, 3200) < kNoteViewX + kNoteViewWidth, "monotonic at %u", i);
        prev = noteViewX(i, 3200);
    }
    // 64 beats on a loop: no two beats of a 1/32 grid share a pixel column of their bars.
    const uint32_t song = 6400;  // two bars
    for (uint32_t k = 1; k < 64; k++) {
        const uint16_t a = noteViewX((k - 1) * song / 64, song), b = noteViewX(k * song / 64, song);
        CHECK(b - a >= 13, "step %u is %u px", k, b - a);
    }
    CHECK(kNoteViewBarWidth < 13 && kNoteViewBlockWidth < 13, "bars fit a 1/32 step");
    // Blocks
    CHECK(noteViewBlockWidth(false, 0, 0, 3200) == kNoteViewBlockWidth, "plain beat");
    CHECK(noteViewBlockWidth(true, 0, 1600, 3200) == noteViewX(1600, 3200) - noteViewX(0, 3200), "a fill lasts");
    CHECK(noteViewBlockWidth(true, 100, 100, 3200) == kNoteViewBlockWidth && noteViewBlockWidth(true, 100, 105, 3200) == kNoteViewBlockWidth, "a tiny fill is not narrower than a beat");
    uint16_t from, to;
    noteViewSpan(kNoteViewX, kNoteViewBlockWidth, from, to);
    CHECK(from == kNoteViewX - 2 && to == kNoteViewX + kNoteViewBlockWidth + 1, "span at the left edge: %u..%u", from, to);
    noteViewSpan(kNoteViewX + kNoteViewWidth - 1, kNoteViewBlockWidth, from, to);
    CHECK(to == kNoteViewX + kNoteViewWidth + 3 && from < to, "span at the right edge: %u..%u", from, to);
    // Heights
    CHECK(noteViewVelHeight(0) == 0 && noteViewVelHeight(1) == 1 && noteViewVelHeight(127) == 100 && noteViewVelHeight(200) == 100 && noteViewVelHeight(64) == 50, "velocity bars: %u %u", noteViewVelHeight(1), noteViewVelHeight(64));
    CHECK(noteViewChanceHeight(0) == 0 && noteViewChanceHeight(100) == 60 && noteViewChanceHeight(50) == 30 && noteViewChanceHeight(255) == 60, "chance bars");
    uint16_t last = 0;
    for (int v = 0; v <= 127; v++) {
        CHECK(noteViewVelHeight(static_cast<uint8_t>(v)) >= last, "velocity monotonic at %d", v);
        last = noteViewVelHeight(static_cast<uint8_t>(v));
    }
    // Rows fit the lane area and do not overlap.
    CHECK(kNoteViewTitleY >= 131 && kNoteViewRulerY > kNoteViewTitleY + 9 && kNoteViewTickY > kNoteViewRulerY + 7 && kNoteViewBlockY > kNoteViewTickY + kNoteViewTickHeight, "top rows");
    CHECK(kNoteViewDotY > kNoteViewBlockY + kNoteViewBlockHeight && kNoteViewVelBase - kNoteViewVelHeight > kNoteViewDotY + 3, "dots above the velocity lane");
    CHECK(kNoteViewChanceBase - kNoteViewChanceHeight > kNoteViewVelBase + 2 && kNoteViewChanceBase < 440 && kNoteViewX + kNoteViewWidth < 936, "lanes fit");
    // Ruler
    std::vector<int> ticks, bars;
    const uint16_t n = noteViewRuler(6400, 3200, 800, [&](uint16_t x, bool bar, uint16_t number) {
        ticks.push_back(x);
        if (bar) bars.push_back(number);
    });
    CHECK(n == 8 && ticks.size() == 8 && bars.size() == 2 && bars[0] == 1 && bars[1] == 2, "ruler: %u ticks, %zu bars", n, bars.size());
    CHECK(noteViewRuler(0, 3200, 800, [](uint16_t, bool, uint16_t) {}) == 0 && noteViewRuler(3200, 3200, 0, [](uint16_t, bool, uint16_t) {}) == 0, "empty ruler");
    CHECK(noteViewRuler(3200, 0, 400, [](uint16_t, bool, uint16_t) {}) == 8, "no bar length still ticks");
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
