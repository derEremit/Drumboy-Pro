// Host-side tests for the lane marks and the ring geometry (LaneView.h).

#include <cstdio>
#include <cstdlib>

#include "LaneView.h"

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
    // A beat without note settings looks as before: FLAT full height; GHOST is now shorter.
    CHECK(laneMarkHeight(0, 100) == 9, "FLAT %u", laneMarkHeight(0, 100));
    CHECK(laneMarkHeight(0, 50) == 6, "GHOST %u", laneMarkHeight(0, 50));
    CHECK(laneMarkHeight(127, 50) == 9 && laneMarkHeight(1, 100) == 2 && laneMarkHeight(64, 100) == 6, "velocity ends");
    uint8_t prev = 0;
    for (int v = 1; v <= 127; v++) {
        const uint8_t h = laneMarkHeight(static_cast<uint8_t>(v), 100);
        CHECK(h >= kLaneMarkMinHeight && h <= kLaneMarkMaxHeight && h >= prev, "velocity %d height %u", v, h);
        prev = h;
    }
    CHECK(!laneMarkHollow(0) && !laneMarkDot(0), "plain beat: solid, no dot");
    CHECK(laneMarkHollow(noteWithChancePercent(0, 95)) && !laneMarkHollow(noteWithChancePercent(0, 100)), "hollow below 100%%");
    CHECK(laneMarkDot(noteWithCond(0, kNoteCondFill)) && !laneMarkDot(noteWithNudge(0, 5)), "dot for a condition only");

    // Rings: layer 1 outside, 12 px apart, all inside the lane area (y 132-440) with the playhead orbit.
    CHECK(ringRadius(0) == 140 && ringRadius(9) == 32, "radii");
    CHECK(kRingCenterY - kRingPlayRadius - 2 >= 132 && kRingCenterY + kRingPlayRadius + 2 <= 440, "orbit inside the lane area");
    CHECK(kRingCenterX - kRingPlayRadius - 2 >= 21, "orbit inside the left edge");
    const uint32_t song = 6400;
    RingPoint p = ringPoint(0, song, 100);
    CHECK(p.x == 200 && p.y == 186, "loop start at the top: %d,%d", p.x, p.y);
    p = ringPoint(song / 4, song, 100);
    CHECK(p.x == 300 && p.y == 286, "a quarter: right: %d,%d", p.x, p.y);
    p = ringPoint(song / 2, song, 100);
    CHECK(p.x == 200 && p.y == 386, "half: bottom: %d,%d", p.x, p.y);
    p = ringPoint(3 * song / 4, song, 100);
    CHECK(p.x == 100 && p.y == 286, "three quarters: left: %d,%d", p.x, p.y);
    p = ringPoint(song, song, 100);
    CHECK(p.x == 200 && p.y == 186, "the end wraps to the start");
    p = ringPoint(5, 0, 100);
    CHECK(p.x == 200 && p.y == 186, "no song length: the top");
    // Every point lies on its ring (within rounding), for every layer.
    for (uint8_t l = 0; l < 10; l++) {
        for (uint32_t t = 0; t < song; t += 50) {
            const RingPoint q = ringPoint(t, song, ringRadius(l));
            const long dx = q.x - kRingCenterX, dy = q.y - kRingCenterY, r = ringRadius(l);
            if (labs(dx * dx + dy * dy - r * r) > 2 * r + 2) {
                CHECK(false, "layer %u t %u off the ring", l, t);
                break;
            }
        }
    }
    CHECK(ringDotRadius(false, 0, 100) == 2 && ringDotRadius(true, 0, 100) == 4 && ringDotRadius(true, 0, 50) == 3 && ringDotRadius(true, 100, 50) == 4, "dot sizes");

    // --- Song timeline ---
    {
        SongChain c;
        ChainPlay p;
        // The default chain (one step, one loop) gets the widest unit.
        CHECK(songViewUnit(c) == kSongViewMaxUnit, "default unit %u", songViewUnit(c));
        // The mockup's chain: 1x4 2x2 1x4 3x2 = 12 loops.
        chainSetCount(c, p, 4);
        chainSetStep(c, 0, 0, 4);
        chainSetStep(c, 1, 1, 2);
        chainSetStep(c, 2, 0, 4);
        chainSetStep(c, 3, 2, 2);
        const uint16_t unit = songViewUnit(c);
        CHECK(unit == 72, "12 loops: unit %u", unit);
        CHECK(songViewStepX(c, 0, unit) == kSongViewX && songViewStepX(c, 1, unit) == kSongViewX + 4 * unit && songViewStepX(c, 4, unit) == kSongViewX + 12 * unit, "step positions");
        CHECK(chainUsedBanks(c) == 0x07, "used banks %02x", chainUsedBanks(c));
        // What comes next.
        ChainNext n = chainNext(c, p);
        CHECK(n.bank == 1 && n.loops == 4, "next from the start: bank %d in %u", n.bank, n.loops);
        p.step = 1;
        p.loopsDone = 1;
        n = chainNext(c, p);
        CHECK(n.bank == 0 && n.loops == 1, "last loop of step 2: bank %d in %u", n.bank, n.loops);
        p.step = 3;
        p.loopsDone = 0;
        n = chainNext(c, p);
        CHECK(n.bank == 0 && n.loops == 2, "last step loops back: bank %d in %u", n.bank, n.loops);
        c.loop = false;
        n = chainNext(c, p);
        CHECK(n.bank == -1 && n.loops == 2, "last step, STOP: %d in %u", n.bank, n.loops);

        // Every chain fits the width, each box inside its span, each progress bar inside its box.
        for (uint8_t count = 1; count <= kChainMaxSteps; count++) {
            for (uint8_t repeats = 1; repeats <= kChainMaxRepeats; repeats++) {
                SongChain d;
                ChainPlay q;
                chainSetCount(d, q, count);
                for (uint8_t i = 0; i < count; i++) chainSetStep(d, i, i % kChainBanks, repeats);
                const uint16_t u = songViewUnit(d);
                const uint16_t end = songViewStepX(d, count, u);
                CHECK(u >= kSongViewMinUnit && u <= kSongViewMaxUnit && end <= kSongViewX + kSongViewWidth, "%u x %u: unit %u end %u", count, repeats, u, end);
                const uint16_t w = songViewBoxWidth(repeats, u);
                CHECK(w >= 2 && w <= repeats * u, "%u x %u: box %u", count, repeats, w);
                const uint16_t barEnd = static_cast<uint16_t>((repeats - 1) * u + songViewCellInset(u) + songViewCellWidth(u));
                CHECK(songViewCellWidth(u) >= 1 && barEnd <= repeats * u, "%u x %u: bar ends at %u of %u", count, repeats, barEnd, repeats * u);
                CHECK(songViewProgress(0, 3200, u) == 1 && songViewProgress(3199, 3200, u) == songViewCellWidth(u) && songViewProgress(99999, 3200, u) == songViewCellWidth(u),
                      "%u x %u: progress ends", count, repeats);
            }
        }
        CHECK(songViewProgress(100, 0, 72) == 0, "no loop length");

        // Thumbnails: five across, inside the area; beats inside their frame.
        CHECK(songThumbX(0) == 40 && songThumbX(4) + kSongThumbWidth <= 939, "thumbnails fit: %u", songThumbX(4) + kSongThumbWidth);
        CHECK(kSongThumbRowY + 9 * kSongThumbRowStep + kSongThumbRowHeight < kSongThumbY + kSongThumbHeight, "ten rows fit the frame");
        CHECK(kSongThumbY + kSongThumbHeight <= 440, "frames inside the lane area");
        for (uint8_t b = 0; b < kChainBanks; b++) {
            const uint16_t first = songThumbBeatX(b, 0, 3200), last = songThumbBeatX(b, 3199, 3200);
            CHECK(first == songThumbX(b) + kSongThumbInset && last + 2 < songThumbX(b) + kSongThumbWidth, "bank %u beats %u..%u", b, first, last);
        }
        CHECK(songThumbBeatX(1, 50, 0) == songThumbX(1) + kSongThumbInset, "no loop length");
    }

    // --- Routing view ---
    CHECK(routeViewModuleX(0) == 300 && routeViewModuleX(NODE_MASTER) + kRouteViewBoxWidth / 2 < 939, "modules fit: %u", routeViewModuleX(NODE_MASTER) + kRouteViewBoxWidth / 2);
    CHECK(kRouteViewNameX + (3 + 12) * 9 < kRouteViewGuideX && kRouteViewGuideX < routeViewModuleX(0) - kRouteViewBoxWidth / 2, "names end before the guides and boxes");
    CHECK(routeViewRowY(kRouteViewSources - 1) + kRouteViewMark < 440 && routeViewRowY(0) > kRouteViewBoxY + kRouteViewBoxHeight, "twelve rows fit below the boxes");
    for (uint8_t m = 0; m < kRouteModuleCount; m++) {
        CHECK(routeViewArcY(m) > 133 && routeViewArcY(m) < kRouteViewBoxY, "module %u arc height %u", m, routeViewArcY(m));
        CHECK(routeViewArcOffset(m) < kRouteViewBoxWidth / 2, "module %u arc offset %u", m, routeViewArcOffset(m));
        for (uint8_t k = 0; k < m; k++) CHECK(routeViewArcY(k) != routeViewArcY(m) && routeViewArcOffset(k) != routeViewArcOffset(m), "arcs %u and %u apart", k, m);
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
