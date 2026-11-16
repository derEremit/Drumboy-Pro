// Host-side tests for the pan law (Pan.h).

#include <cmath>
#include <cstdio>
#include <cstring>

#include "Pan.h"

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
    float l, r;
    // The centre is exactly 1 on both sides: nothing changes without panning.
    panGains(kPanCentre, l, r);
    CHECK(l == 1.0f && r == 1.0f, "centre %f %f", l, r);
    // The ends: all on one side, +3 dB there.
    panGains(0, l, r);
    CHECK(std::fabs(l - 1.41421356f) < 1e-5f && std::fabs(r) < 1e-6f, "hard left %f %f", l, r);
    panGains(kPanMax, l, r);
    CHECK(std::fabs(r - 1.41421356f) < 1e-5f && std::fabs(l) < 1e-5f, "hard right %f %f", l, r);
    panGains(200, l, r);
    CHECK(std::fabs(r - 1.41421356f) < 1e-5f, "a position past the end is the end");
    // Constant power, mirror symmetry, monotonic.
    float prevL = 2.0f, prevR = -1.0f;
    for (uint8_t p = 0; p <= kPanMax; p++) {
        panGains(p, l, r);
        CHECK(std::fabs(l * l + r * r - 2.0f) < 1e-5f, "power at %u: %f", p, l * l + r * r);
        CHECK(l <= prevL + 1e-6f && r >= prevR - 1e-6f, "monotonic at %u", p);
        prevL = l;
        prevR = r;
        float ml, mr;
        panGains(static_cast<uint8_t>(kPanMax - p), ml, mr);
        CHECK(std::fabs(ml - r) < 1e-5f && std::fabs(mr - l) < 1e-5f, "mirror at %u", p);
    }
    // A step to either side keeps the other side close to full (a gentle law near the centre).
    panGains(kPanCentre - 1, l, r);
    CHECK(l > 1.0f && l < 1.1f && r < 1.0f && r > 0.9f, "one step left: %f %f", l, r);
    // The sum of both sides (a mono mix-down) never exceeds sqrt(2) times a centred signal.
    for (uint8_t p = 0; p <= kPanMax; p++) {
        panGains(p, l, r);
        CHECK(l + r <= 2.0f + 1e-5f, "mono sum at %u: %f", p, l + r);
    }
    // Labels.
    char t[8];
    panLabel(t, sizeof t, 20);
    CHECK(std::strcmp(t, "C") == 0, "%s", t);
    panLabel(t, sizeof t, 8);
    CHECK(std::strcmp(t, "L12") == 0, "%s", t);
    panLabel(t, sizeof t, 23);
    CHECK(std::strcmp(t, "R 3") == 0, "%s", t);
    panLabel(t, sizeof t, 0);
    CHECK(std::strcmp(t, "L20") == 0, "%s", t);
    panLabel(t, sizeof t, 40);
    CHECK(std::strcmp(t, "R20") == 0, "%s", t);
    panLabel(t, sizeof t, 99);
    CHECK(std::strcmp(t, "R20") == 0, "past the end %s", t);
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
