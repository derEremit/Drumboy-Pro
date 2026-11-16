// Host-side tests for DUB mode (DubMix.h and the send gains of Routing.h).

#include <cmath>
#include <cstdio>
#include <cstring>

#include "DubMix.h"

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

const uint16_t N = 32;

// Each module multiplies by its own prime-ish factor, so what reaches the master tells the path.
static const float kGain[6] = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f};  // EQ, F1, F2, FX1, FX2, REV

struct Rig {
    float eqL[N], f1L[N], f2L[N], fx1L[N], fx2L[N], revL[N];
    float eqR[N], f1R[N], f2R[N], fx1R[N], fx2R[N], revR[N];
    float outL[N], outR[N];
    RouteBuffers bufs() { return {{eqL, f1L, f2L, fx1L, fx2L, revL}, {eqR, f1R, f2R, fx1R, fx2R, revR}, outL, outR}; }
};

static void run(Rig& r, RouteSource* sources, uint8_t count, const RoutePatch& patch, RouteState& st) {
    routeBlock<N>(patch, st, r.bufs(), sources, count, [&](uint8_t m) {
        float* l = (m == NODE_EQ) ? r.eqL : (m == NODE_F1) ? r.f1L : (m == NODE_F2) ? r.f2L : (m == NODE_FX1) ? r.fx1L : (m == NODE_FX2) ? r.fx2L : r.revL;
        float* rr = (m == NODE_EQ) ? r.eqR : (m == NODE_F1) ? r.f1R : (m == NODE_F2) ? r.f2R : (m == NODE_FX1) ? r.fx1R : (m == NODE_FX2) ? r.fx2R : r.revR;
        for (uint16_t i = 0; i < N; i++) {
            l[i] *= kGain[m];
            rr[i] *= kGain[m];
        }
    });
}

int main() {
    // --- Patch ---
    CHECK(routePatchValid(kDubRoutePatch), "the DUB patch is valid");
    CHECK(kDubRoutePatch.moduleDest[NODE_EQ] == routeBit(NODE_F1) && kDubRoutePatch.moduleDest[NODE_F2] == routeBit(NODE_FX1), "bus and echo paths");
    for (uint8_t m = NODE_F1; m < kRouteModuleCount; m++) {
        if (m == NODE_F2) continue;
        CHECK(kDubRoutePatch.moduleDest[m] == routeBit(NODE_MASTER), "module %u goes to the master", m);
    }

    // --- Gains ---
    float g[kRouteNodeCount];
    DubStrip s;
    dubNodeGains(s, 0, g);
    CHECK(g[NODE_EQ] == 1.0f && g[NODE_MASTER] == 0.0f && g[NODE_F2] == 0.0f && g[NODE_REV] == 0.0f && g[NODE_FX2] == 0.0f && g[NODE_F1] == 0.0f && g[NODE_FX1] == 0.0f, "a default strip is the dry FILT path");
    s.bus = DUB_BUS_DIR;
    s.echo = 35;
    s.verb = 62;
    s.fx = 100;
    dubNodeGains(s, 0, g);
    CHECK(g[NODE_EQ] == 0.0f && g[NODE_MASTER] == 1.0f && std::fabs(g[NODE_F2] - 0.35f) < 1e-6f && std::fabs(g[NODE_REV] - 0.62f) < 1e-6f && g[NODE_FX2] == 1.0f, "DIR strip with sends");
    dubNodeGains(s, THROW_ECHO, g);
    CHECK(g[NODE_F2] == 1.0f && std::fabs(g[NODE_REV] - 0.62f) < 1e-6f, "an echo throw opens the echo send only");
    dubNodeGains(s, THROW_ECHO | THROW_VERB, g);
    CHECK(g[NODE_F2] == 1.0f && g[NODE_REV] == 1.0f, "both throws");
    s.echo = 200;
    dubNodeGains(s, 0, g);
    CHECK(g[NODE_F2] == 1.0f, "a send above 100 is capped");
    CHECK(dubDryScale(true) == 0.0f && dubDryScale(false) == 1.0f, "dry scale");

    // --- Routing with gains: the master gets the product of the modules on each path ---
    float src[N];
    for (uint16_t i = 0; i < N; i++) src[i] = 0.5f + 0.01f * i;
    RouteState st;
    float state[kRouteGainStateSize] = {0};
    DubStrip strip;
    strip.bus = DUB_BUS_FILT;
    strip.echo = 50;
    strip.verb = 20;
    strip.fx = 10;
    float target[kRouteNodeCount];
    dubNodeGains(strip, 0, target);
    Rig r;
    RouteSource sources[1] = {{src, 0, nullptr, target, state, 1.0f, 1.0f}};
    for (int block = 0; block < 3; block++) run(r, sources, 1, kDubRoutePatch, st);  // settle the ramp
    bool ok = true;
    for (uint16_t i = 0; i < N; i++) {
        // FILT: EQ * F1; echo: 0.5 * F2 * FX1; verb: 0.2 * REV; fx: 0.1 * FX2
        const float expected = src[i] * (kGain[0] * kGain[1] + 0.5f * kGain[2] * kGain[3] + 0.2f * kGain[5] + 0.1f * kGain[4]);
        ok = ok && std::fabs(r.outL[i] - expected) < 1e-5f && std::fabs(r.outR[i] - r.outL[i]) < 1e-5f;
    }
    CHECK(ok, "master sum of the four paths");
    // A silent strip adds nothing.
    for (uint8_t n = 0; n < kRouteNodeCount; n++) target[n] = 0.0f;
    for (int block = 0; block < 2; block++) run(r, sources, 1, kDubRoutePatch, st);
    float peak = 0;
    for (uint16_t i = 0; i < N; i++) peak = std::fmax(peak, std::fabs(r.outL[i]));
    CHECK(peak == 0.0f, "all gains 0: silence");

    // --- The ramp: from the old gain to the new one over the block, linear, then steady ---
    {
        float one[N];
        for (uint16_t i = 0; i < N; i++) one[i] = 1.0f;
        float eqL2[N], f1L2[N], f2L2[N], x1L2[N], x2L2[N], rvL2[N], eqR2[N], f1R2[N], f2R2[N], x1R2[N], x2R2[N], rvR2[N], mL[N], mR[N];
        RouteBuffers b2 = {{eqL2, f1L2, f2L2, x1L2, x2L2, rvL2}, {eqR2, f1R2, f2R2, x1R2, x2R2, rvR2}, mL, mR};
        auto clear = [&]() {
            memset(mL, 0, sizeof mL);
            memset(mR, 0, sizeof mR);
            memset(f2L2, 0, sizeof f2L2);
            memset(f2R2, 0, sizeof f2R2);
            memset(eqL2, 0, sizeof eqL2);
            memset(eqR2, 0, sizeof eqR2);
        };
        float st2[kRouteGainStateSize] = {0};
        float tl[kRouteNodeCount] = {0}, tr[kRouteNodeCount] = {0};
        tl[NODE_MASTER] = tr[NODE_MASTER] = 1.0f;
        clear();
        routeScatterLR<N>(b2, one, tl, tr, st2);
        CHECK(std::fabs(mL[0] - 1.0f / N) < 1e-6f && std::fabs(mL[N - 1] - 1.0f) < 1e-6f && std::fabs(mL[15] - 16.0f / N) < 1e-6f && std::fabs(mR[15] - 16.0f / N) < 1e-6f && st2[NODE_MASTER] == 1.0f,
              "ramp up: %f %f", mL[0], mL[N - 1]);
        clear();
        routeScatterLR<N>(b2, one, tl, tr, st2);
        CHECK(mL[0] == 1.0f && mL[N - 1] == 1.0f && mR[7] == 1.0f, "steady");
        tl[NODE_MASTER] = tr[NODE_MASTER] = 0.0f;
        clear();
        routeScatterLR<N>(b2, one, tl, tr, st2);
        CHECK(std::fabs(mL[0] - (1.0f - 1.0f / N)) < 1e-6f && std::fabs(mL[N - 1]) < 1e-6f, "ramp down");
        // Another node receives its own gain in both channels.
        tl[NODE_F2] = tr[NODE_F2] = 0.25f;
        clear();
        routeScatterLR<N>(b2, one, tl, tr, st2);  // the ramp
        clear();
        routeScatterLR<N>(b2, one, tl, tr, st2);  // steady
        CHECK(std::fabs(f2L2[N - 1] - 0.25f) < 1e-6f && std::fabs(f2R2[N - 1] - 0.25f) < 1e-6f && eqL2[5] == 0.0f, "echo node");

        // --- Pan: the left and right gains of a source split every node it goes to ---
        float pl, pr;
        panGains(8, pl, pr);
        float st4[kRouteGainStateSize] = {0};
        float gains[kRouteNodeCount] = {0};
        gains[NODE_MASTER] = 1.0f;
        gains[NODE_F2] = 0.5f;
        RouteState rs;
        Rig r3;
        RouteSource panned[1] = {{one, 0, nullptr, gains, st4, pl, pr}};
        run(r3, panned, 1, kDubRoutePatch, rs);
        run(r3, panned, 1, kDubRoutePatch, rs);
        // The master gets the dry path panned; the echo path goes F2 -> FX1 -> master, panned the same way.
        const float echoPath = 0.5f * kGain[2] * kGain[3];
        CHECK(std::fabs(r3.outL[N - 1] - (pl + echoPath * pl)) < 1e-5f && std::fabs(r3.outR[N - 1] - (pr + echoPath * pr)) < 1e-5f, "panned DIR strip with an echo send: %f %f", r3.outL[N - 1], r3.outR[N - 1]);
        // A centred pan is exactly the unpanned signal, in both channels.
        float st5[kRouteGainStateSize] = {0};
        RouteSource centred[1] = {{one, 0, nullptr, gains, st5, 1.0f, 1.0f}};
        RouteState rs2;
        run(r3, centred, 1, kDubRoutePatch, rs2);
        run(r3, centred, 1, kDubRoutePatch, rs2);
        CHECK(r3.outL[7] == r3.outR[7], "a centred source is the same on both sides: %f %f", r3.outL[7], r3.outR[7]);
        // The pan moves with a ramp: the first block goes from centre to the new gains.
        float st6[kRouteGainStateSize] = {0};
        RouteSource moving[1] = {{one, 0, nullptr, gains, st6, 1.0f, 1.0f}};
        RouteState rs3;
        run(r3, moving, 1, kDubRoutePatch, rs3);
        run(r3, moving, 1, kDubRoutePatch, rs3);
        const float centreL = r3.outL[0];
        moving[0].panLeft = pl;
        moving[0].panRight = pr;
        run(r3, moving, 1, kDubRoutePatch, rs3);
        CHECK(std::fabs(r3.outL[N - 1] - (pl + echoPath * pl)) < 1e-4f && r3.outL[0] != centreL, "pan ramp: %f then %f", r3.outL[0], r3.outL[N - 1]);

        // The mask path (CHAIN): a source to the master and the reverb, panned: both are panned, the stereo pair stays a pair.
        float st7[kRouteGainStateSize] = {0};
        const uint8_t mask = routeBit(NODE_MASTER) | routeBit(NODE_REV);
        RouteSource maskSrc[1] = {{one, mask, nullptr, nullptr, st7, pl, pr}};
        RouteState rs4;
        run(r3, maskSrc, 1, kDefaultRoutePatch, rs4);
        run(r3, maskSrc, 1, kDefaultRoutePatch, rs4);
        const float rev = kGain[NODE_REV];
        CHECK(std::fabs(r3.outL[N - 1] - (pl + pl * rev)) < 1e-5f && std::fabs(r3.outR[N - 1] - (pr + pr * rev)) < 1e-5f, "mask path panned: %f %f", r3.outL[N - 1], r3.outR[N - 1]);
        // Through the chain: a panned source entering at the EQ comes out of the master panned (all stand-in modules are linear and identical on both sides).
        float st8[kRouteGainStateSize] = {0};
        RouteSource eqSrc[1] = {{one, routeBit(NODE_EQ), nullptr, nullptr, st8, pl, pr}};
        RouteState rs5;
        run(r3, eqSrc, 1, kDefaultRoutePatch, rs5);
        run(r3, eqSrc, 1, kDefaultRoutePatch, rs5);
        float chain = 1.0f;
        for (uint8_t m = 0; m < 6; m++) chain *= kGain[m];
        CHECK(std::fabs(r3.outL[N - 1] - chain * pl) < 1e-5f && std::fabs(r3.outR[N - 1] - chain * pr) < 1e-5f, "panned through the whole chain: %f %f want %f %f", r3.outL[N - 1], r3.outR[N - 1], chain * pl, chain * pr);
    }

    // --- File block ---
    DubMix d;
    d.on = true;
    for (uint8_t i = 0; i < kDubStrips; i++) {
        d.strip[i].bus = i % 2;
        d.strip[i].echo = static_cast<uint8_t>(i * 10);
        d.strip[i].verb = static_cast<uint8_t>(100 - i * 10);
        d.strip[i].fx = static_cast<uint8_t>(i);
    }
    uint8_t blob[kDubFileSize];
    dubWrite(d, blob);
    DubMix back;
    CHECK(dubRead(back, blob, kDubFileSize) && back.on && back.strip[3].bus == 1 && back.strip[3].echo == 30 && back.strip[3].verb == 70 && back.strip[9].fx == 9, "round trip");
    CHECK(!dubRead(back, blob, kDubFileSizeV2 - 1) && !back.on, "too short: default");
    // Pan: kept, defaults to the centre, validated; a version 1 block (no pan) still reads.
    d.pan[4] = 7;
    d.pan[9] = 40;
    dubWrite(d, blob);
    CHECK(dubRead(back, blob, kDubFileSize) && back.pan[4] == 7 && back.pan[9] == 40 && back.pan[0] == kPanCentre, "pan round trip");
    uint8_t v1[kDubFileSizeV1];
    memcpy(v1, blob, kDubFileSizeV1);
    v1[3] = 1;
    CHECK(dubRead(back, v1, kDubFileSizeV1) && back.on && back.strip[3].echo == 30 && back.pan[4] == kPanCentre && back.pan[9] == kPanCentre, "version 1 is read, centred");
    CHECK(!dubRead(back, blob, kDubFileSizeV1), "a version 2 block that is too short is refused");
    uint8_t badPan[kDubFileSize];
    memcpy(badPan, blob, kDubFileSize);
    badPan[kDubFileSizeV1 + 2] = 41;
    CHECK(!dubRead(back, badPan, kDubFileSize), "a pan past the end is refused");
    DubMix fresh;
    CHECK(fresh.pan[0] == kPanCentre && fresh.pan[9] == kPanCentre, "a new mix is centred");
    uint8_t bad[kDubFileSize];
    memcpy(bad, blob, kDubFileSize);
    bad[3] = 3;
    CHECK(!dubRead(back, bad, kDubFileSize), "unknown version");
    memcpy(bad, blob, kDubFileSize);
    bad[6 + 4 * 2] = 101;
    CHECK(!dubRead(back, bad, kDubFileSize) && !back.on && back.strip[2].echo == 0, "a send over 100 rejects the block");
    memcpy(bad, blob, kDubFileSize);
    bad[5] = 2;
    CHECK(!dubRead(back, bad, kDubFileSize), "unknown bus");
    memcpy(bad, blob, kDubFileSize);
    bad[0] = 'X';
    CHECK(!dubRead(back, bad, kDubFileSize), "wrong marker");
    CHECK(kDubFileSize == 56 && kDubFileSizeV2 == 55 && kDubFileSizeV1 == 45, "block sizes %u %u %u", kDubFileSize, kDubFileSizeV2, kDubFileSizeV1);
    // The echo width: stored in the last byte; a block of 55 bytes (before the width) reads as 0.
    DubMix w;
    w.echoWidth = 70;
    uint8_t wb[kDubFileSize];
    dubWrite(w, wb);
    DubMix wr;
    CHECK(dubRead(wr, wb, kDubFileSize) && wr.echoWidth == 70, "echo width round trip");
    CHECK(dubRead(wr, wb, kDubFileSizeV2) && wr.echoWidth == 0, "a 55-byte block has no width");
    wb[kDubFileSizeV2] = 101;
    CHECK(!dubRead(wr, wb, kDubFileSize), "a width over 100 rejects the block");

    // --- A stereo source (a layer playing a stereo sample) ---
    {
        Rig r;
        RouteState st;
        float left[N], right[N];
        for (uint16_t i = 0; i < N; i++) {
            left[i] = 0.5f;
            right[i] = -0.25f;
        }
        float state[kRouteGainStateSize] = {0};
        RouteSource src[1];
        src[0] = {left, routeBit(NODE_MASTER), nullptr, nullptr, state, 1.0f, 1.0f, right};
        RoutePatch patch = kDefaultRoutePatch;
        run(r, src, 1, patch, st);  // the ramp
        run(r, src, 1, patch, st);  // steady
        CHECK(r.outL[0] == 0.5f && r.outR[0] == -0.25f && r.outL[N - 1] == 0.5f && r.outR[N - 1] == -0.25f, "centred stereo source: left stays left, right stays right (%f %f)", r.outL[0], r.outR[0]);
        // Panned hard left: the right channel is silent, the left one is at its gain.
        float pl, pr;
        panGains(0, pl, pr);
        src[0].panLeft = pl;
        src[0].panRight = pr;
        run(r, src, 1, patch, st);
        run(r, src, 1, patch, st);
        CHECK(std::fabs(r.outL[5] - 0.5f * pl) < 1e-6f && r.outR[5] == 0.0f, "stereo source panned left is a balance: %f %f", r.outL[5], r.outR[5]);
        // A mono source next to it is unchanged: both channels from one buffer.
        float mono[N];
        for (uint16_t i = 0; i < N; i++) mono[i] = 0.5f;
        float state2[kRouteGainStateSize] = {0};
        RouteSource s2[1] = {{mono, routeBit(NODE_MASTER), nullptr, nullptr, state2, 1.0f, 1.0f}};
        run(r, s2, 1, patch, st);
        run(r, s2, 1, patch, st);
        CHECK(r.outL[3] == 0.5f && r.outR[3] == 0.5f, "mono source: the same on both sides");
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
