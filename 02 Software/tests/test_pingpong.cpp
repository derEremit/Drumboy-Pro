// Host-side tests for the ping-pong echo (PingPong.h): a delay run the way the firmware runs it.

#include <cstdio>
#include <cmath>
#include <vector>

#include "PingPong.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...)                                                \
    do {                                                                \
        ++checks;                                                       \
        if (!(cond)) {                                                  \
            ++failures;                                                 \
            std::printf("FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond); \
            std::printf(__VA_ARGS__);                                   \
            std::printf("\n");                                          \
        }                                                               \
    } while (0)

// One delay as in processAudioEffectHelper: write at r, first tap at r - lag, feedback from the right tap.
struct Run {
    std::vector<float> buf;
    uint32_t p, r, lag, size;
    float width, fb, level;
    Run(uint32_t size_, uint32_t lag_, float width_, float fb_) : buf(size_, 0.0f), p(0), r(lag_), lag(lag_), size(size_), width(width_), fb(fb_), level(1.0f) {}
    void step(float in, float& l, float& rr) {
        const float t1 = buf[p];
        const float t2 = (width > 0.0f) ? buf[pingPongSecondIndex(p, lag, size)] : 0.0f;
        const float right = pingPongRight(t1, t2, width, fb);
        buf[r] = in * level + right * fb;
        l = t1;
        rr = right;
        if (++p >= size) p = 0;
        if (++r >= size) r = 0;
    }
};

int main() {
    // Index and fit.
    CHECK(pingPongSecondIndex(10, 4, 100) == 6 && pingPongSecondIndex(2, 4, 100) == 98 && pingPongSecondIndex(4, 4, 100) == 0, "second index");
    CHECK(pingPongFits(49, 100) && !pingPongFits(50, 100) && !pingPongFits(0, 100), "fit");
    CHECK(pingPongWidth(0, 10, 100) == 0.0f && pingPongWidth(100, 10, 100) == 1.0f && pingPongWidth(50, 10, 100) == 0.5f, "width gain");
    CHECK(pingPongWidth(100, 60, 100) == 0.0f, "two echo times do not fit: centred");
    CHECK(pingPongWidth(250, 10, 100) == 1.0f, "over 100 is 100");

    const uint32_t size = 1000, lag = 100;
    // Width 0: both channels carry the same echo, equal to a plain delay.
    {
        Run a(size, lag, 0.0f, 0.5f);
        std::vector<float> plain(size, 0.0f);
        uint32_t pp = 0, rr = lag;
        float worst = 0.0f;
        for (uint32_t n = 0; n < 1000; n++) {
            const float in = (n == 0) ? 1.0f : 0.0f;
            float l, r;
            a.step(in, l, r);
            const float t = plain[pp];
            plain[rr] = in + t * 0.5f;
            if (++pp >= size) pp = 0;
            if (++rr >= size) rr = 0;
            worst = fmaxf(worst, fmaxf(fabsf(l - t), fabsf(r - t)));
        }
        CHECK(worst == 0.0f, "width 0 is exactly the plain delay (%g)", worst);
    }
    // Full width: the first repeat on the left only, the second on the right only, and so on.
    {
        Run a(size, lag, 1.0f, 0.5f);
        float L[1000], R[1000];
        for (uint32_t n = 0; n < 1000; n++) a.step((n == 0) ? 1.0f : 0.0f, L[n], R[n]);
        CHECK(L[lag] == 1.0f && R[lag] == 0.0f, "first repeat on the left: %g %g", L[lag], R[lag]);
        CHECK(R[2 * lag] == 0.5f && L[2 * lag] == 0.0f, "second repeat on the right at half level: %g %g", R[2 * lag], L[2 * lag]);
        CHECK(L[3 * lag] == 0.25f && R[3 * lag] == 0.0f, "third on the left: %g %g", L[3 * lag], R[3 * lag]);
        CHECK(R[4 * lag] == 0.125f, "fourth on the right: %g", R[4 * lag]);
        float off = 0.0f;
        for (uint32_t n = 0; n < 1000; n++) {
            if (n % lag != 0 || n == 0) off += fabsf(L[n]) + fabsf(R[n]);
        }
        CHECK(off == 0.0f, "nothing between the repeats (%g)", off);
    }
    // Half width: both sides hear every repeat, the right later ones increasingly on the right.
    {
        Run a(size, lag, 0.5f, 0.5f);
        float L[1000], R[1000];
        for (uint32_t n = 0; n < 1000; n++) a.step((n == 0) ? 1.0f : 0.0f, L[n], R[n]);
        CHECK(L[lag] == 1.0f && R[lag] == 0.5f, "half width, first repeat %g %g", L[lag], R[lag]);
    }
    // It stays bounded with feedback below 1.
    {
        Run a(size, lag, 1.0f, 0.9f);
        float peak = 0.0f, l, r;
        for (uint32_t n = 0; n < 100000; n++) {
            a.step((n % 50 == 0) ? 0.5f : 0.0f, l, r);
            peak = fmaxf(peak, fmaxf(fabsf(l), fabsf(r)));
        }
        CHECK(peak < 20.0f && std::isfinite(peak), "bounded %g", peak);
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
