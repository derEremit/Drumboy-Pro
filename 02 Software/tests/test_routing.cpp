// Host-side tests for the signal router (Core/Library/Global/Routing.h).
//
// 1. Default patch == today's fixed chain: a direct model of the summing in
//    processAudioEq/Filter/Effect/Reverb/Send (firmware V1.04) is compared with routeBlock
//    for random sources, entry points and module on/off states. The stand-in modules are
//    nonlinear, so summing before or after the wrong module shows up as a mismatch.
// 2. Parallel paths: each module multiplies by its own prime, so a source's output at the
//    master is the product of the modules on its path.
// 3. Patch validation and entry-point conversion.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>

#include "Routing.h"

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

const uint16_t N = 32;                    // I2S_BLOCK_SIZE
const uint8_t kSources = 12;              // line-in, metronome, 10 layers
const uint8_t MODE_SEND = 6;              // MixerInMode value of SEND

// Stand-in DSP: module m maps x -> a*x + b*x^2 (nonlinear); the reverb also makes R.
static const float kA[6] = {0.9f, 1.1f, 0.8f, 1.2f, 0.7f, 0.6f};
static const float kB[6] = {0.05f, -0.04f, 0.03f, -0.02f, 0.06f, 0.1f};
static float mod(uint8_t m, float x) { return kA[m] * x + kB[m] * x * x; }
static float revRight(float x) { return 0.3f * x - 0.2f * x * x; }

struct State {
    float src[kSources][N];
    uint8_t mode[kSources];  // MixerInMode entry point per source
    bool active[6];          // module on/off
};

// Today's chain, transcribed from the V1.04 stage functions.
static void legacyChain(const State& s, float outL[N], float outR[N]) {
    float eq[N] = {}, f0[N] = {}, f1[N] = {}, e0[N] = {}, e1[N] = {}, rl[N] = {}, rr[N] = {};
    auto add = [&](float* bus, uint8_t mode) {
        for (uint8_t k = 0; k < kSources; k++)
            if (s.mode[k] == mode)
                for (uint16_t i = 0; i < N; i++) bus[i] += s.src[k][i];
    };
    add(eq, 0);
    if (s.active[0]) for (uint16_t i = 0; i < N; i++) eq[i] = mod(0, eq[i]);
    for (uint16_t i = 0; i < N; i++) f0[i] += eq[i];
    add(f0, 1);
    add(f1, 2);
    if (s.active[1]) for (uint16_t i = 0; i < N; i++) f0[i] = mod(1, f0[i]);
    for (uint16_t i = 0; i < N; i++) f1[i] += f0[i];
    if (s.active[2]) for (uint16_t i = 0; i < N; i++) f1[i] = mod(2, f1[i]);
    for (uint16_t i = 0; i < N; i++) e0[i] += f1[i];
    add(e0, 3);
    add(e1, 4);
    if (s.active[3]) for (uint16_t i = 0; i < N; i++) e0[i] = mod(3, e0[i]);
    for (uint16_t i = 0; i < N; i++) e1[i] += e0[i];
    if (s.active[4]) for (uint16_t i = 0; i < N; i++) e1[i] = mod(4, e1[i]);
    for (uint16_t i = 0; i < N; i++) rl[i] += e1[i];
    add(rl, 5);
    for (uint16_t i = 0; i < N; i++) {
        if (s.active[5]) {
            rr[i] = revRight(rl[i]);
            rl[i] = mod(5, rl[i]);
        } else {
            rr[i] = rl[i];
        }
    }
    float send[N] = {};
    add(send, MODE_SEND);
    for (uint16_t i = 0; i < N; i++) {
        outL[i] = rl[i] + send[i];
        outR[i] = rr[i] + send[i];
    }
}

// Old-style call: plain masks, no fades, fresh module state (every route snaps at once).
template <typename Process>
static void runBlock(const RoutePatch& patch, const RouteBuffers& buf, const float* const* src, const uint8_t* masks, uint8_t count, Process process) {
    RouteSource sources[16];
    for (uint8_t k = 0; k < count; k++) sources[k] = {src[k], masks[k], nullptr, nullptr, nullptr, 1.0f, 1.0f};
    RouteState state;
    routeBlock<N>(patch, state, buf, sources, count, process);
}

static void routedChain(const State& s, const RoutePatch& patch, float outL[N], float outR[N]) {
    static float nodeL[6][N], nodeR[6][N];
    RouteBuffers buf = {{nodeL[0], nodeL[1], nodeL[2], nodeL[3], nodeL[4], nodeL[5]}, {nodeR[0], nodeR[1], nodeR[2], nodeR[3], nodeR[4], nodeR[5]}, outL, outR};
    const float* src[kSources];
    uint8_t masks[kSources];
    for (uint8_t k = 0; k < kSources; k++) {
        src[k] = s.src[k];
        masks[k] = routeMaskFromEntry(s.mode[k]);
    }
    runBlock(patch, buf, src, masks, kSources, [&](uint8_t m) {
        if (m == NODE_REV) {
            // The reverb makes its stereo field from the mono sum of its pair (equal channels here).
            for (uint16_t i = 0; i < N; i++) {
                const float in = 0.5f * (nodeL[m][i] + nodeR[m][i]);
                nodeR[m][i] = s.active[m] ? revRight(in) : in;
                nodeL[m][i] = s.active[m] ? mod(m, in) : in;
            }
        } else if (s.active[m]) {
            for (uint16_t i = 0; i < N; i++) {
                nodeL[m][i] = mod(m, nodeL[m][i]);
                nodeR[m][i] = mod(m, nodeR[m][i]);
            }
        }
    });
}

static void testDefaultPatchMatchesLegacy() {
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> sample(-0.5f, 0.5f);
    std::uniform_int_distribution<int> mode(0, 6), coin(0, 1);
    float worst = 0;
    for (int trial = 0; trial < 20000; trial++) {
        State s;
        for (uint8_t k = 0; k < kSources; k++) {
            s.mode[k] = (uint8_t)mode(rng);
            for (uint16_t i = 0; i < N; i++) s.src[k][i] = (coin(rng) || trial % 3) ? sample(rng) : 0.0f;
        }
        for (uint8_t m = 0; m < 6; m++) s.active[m] = trial % 7 == 0 ? true : coin(rng);
        float aL[N], aR[N], bL[N], bR[N];
        legacyChain(s, aL, aR);
        routedChain(s, kDefaultRoutePatch, bL, bR);
        for (uint16_t i = 0; i < N; i++) {
            float d = std::fmax(std::fabs(aL[i] - bL[i]), std::fabs(aR[i] - bR[i]));
            worst = std::fmax(worst, d);
        }
    }
    // Only float rounding from a different summation order may differ.
    CHECK(worst < 1e-5f, "default patch differs from the V1.04 chain by %g", (double)worst);
    std::printf("default patch vs V1.04 chain: 20000 random blocks, worst difference %.3g\n", (double)worst);
}

// Linear modules (x * prime): an impulse's value at the master is the product of its path.
static void testParallelPaths() {
    static const float prime[6] = {2, 3, 5, 7, 11, 13};
    // Example from the design: kick -> Filter 1 -> master, snare -> Filter 2 -> FX 1 -> Reverb -> master,
    // hat -> master directly, clap -> EQ -> FX 2 -> master.
    RoutePatch p = {{routeBit(NODE_FX2), routeBit(NODE_MASTER), routeBit(NODE_FX1), routeBit(NODE_REV), routeBit(NODE_MASTER), routeBit(NODE_MASTER)}};
    CHECK(routePatchValid(p), "example patch should be valid");
    static float nodeL[6][N], nodeR[6][N], L[N], R[N];
    static float src[4][N];
    memset(src, 0, sizeof(src));
    const uint8_t at[4] = {1, 5, 9, 13};  // impulse positions, one per source
    for (uint8_t k = 0; k < 4; k++) src[k][at[k]] = 1.0f;
    const float* srcs[4] = {src[0], src[1], src[2], src[3]};
    const uint8_t masks[4] = {routeBit(NODE_F1), routeBit(NODE_F2), routeBit(NODE_MASTER), routeBit(NODE_EQ)};
    RouteBuffers buf = {{nodeL[0], nodeL[1], nodeL[2], nodeL[3], nodeL[4], nodeL[5]}, {nodeR[0], nodeR[1], nodeR[2], nodeR[3], nodeR[4], nodeR[5]}, L, R};
    runBlock(p, buf, srcs, masks, 4, [&](uint8_t m) {
        for (uint16_t i = 0; i < N; i++) {
            nodeL[m][i] *= prime[m];
            nodeR[m][i] *= prime[m];
        }
    });
    const float want[4] = {3, 5 * 7 * 13, 1, 2 * 11};
    const char* name[4] = {"kick", "snare", "hat", "clap"};
    for (uint8_t k = 0; k < 4; k++) {
        CHECK(L[at[k]] == want[k] && R[at[k]] == want[k], "%s: got L=%g R=%g, want %g", name[k], (double)L[at[k]], (double)R[at[k]], (double)want[k]);
    }
    float other = 0;
    for (uint16_t i = 0; i < N; i++)
        if (i != at[0] && i != at[1] && i != at[2] && i != at[3]) other += std::fabs(L[i]) + std::fabs(R[i]);
    CHECK(other == 0, "signal leaked to other samples");

    // One source feeding two places at once: master + reverb.
    const uint8_t both[1] = {static_cast<uint8_t>(routeBit(NODE_MASTER) | routeBit(NODE_REV))};
    const float* one[1] = {src[0]};
    runBlock(kDefaultRoutePatch, buf, one, both, 1, [&](uint8_t m) {
        for (uint16_t i = 0; i < N; i++) {
            nodeL[m][i] *= prime[m];
            nodeR[m][i] *= prime[m];
        }
    });
    CHECK(L[at[0]] == 1 + 13, "dry + reverb copy: got %g", (double)L[at[0]]);

    // Empty mask mutes.
    const uint8_t mute[1] = {0};
    runBlock(kDefaultRoutePatch, buf, one, mute, 1, [&](uint8_t) {});
    float sum = 0;
    for (uint16_t i = 0; i < N; i++) sum += std::fabs(L[i]) + std::fabs(R[i]);
    CHECK(sum == 0, "muted source reached the master");
}

static void testValidation() {
    CHECK(routePatchValid(kDefaultRoutePatch), "default patch must be valid");
    for (uint8_t m = 0; m < kRouteModuleCount; m++) {
        for (unsigned mask = 0; mask < 128; mask++) {
            bool expect;
            if (m == NODE_REV) expect = (mask == routeBit(NODE_MASTER));
            else expect = (mask & ((routeBit(m) << 1) - 1)) == 0;  // nothing at or below itself
            CHECK(routeModuleDestValid(m, (uint8_t)mask) == expect, "module %u mask 0x%02x", m, mask);
        }
    }
    CHECK(!routeModuleDestValid(kRouteModuleCount, routeBit(NODE_MASTER)), "out-of-range module");
    for (uint8_t e = 0; e < 7; e++) CHECK(routeMaskFromEntry(e) == routeBit(e), "entry %u", e);
    CHECK(routeMaskFromEntry(7) == 0 && routeMaskFromEntry(255) == 0, "out-of-range entry must mute");
}

// The OUT field writes the old four settings; reading them back must give the same entry point,
// and the priority must match the old layerInst_calculateMixMode for every combination.
static void testLegacyConversion() {
    for (uint8_t e = 0; e < kRouteNodeCount; e++) {
        const uint8_t* l = kRouteLegacyFromEntry[e];
        CHECK(routeEntryFromLegacy(l[0], l[1], l[2], l[3]) == e, "entry %u does not round-trip", e);
    }
    for (uint8_t eq = 0; eq <= 1; eq++)
        for (uint8_t f = 0; f <= 2; f++)
            for (uint8_t fx = 0; fx <= 2; fx++)
                for (uint8_t rv = 0; rv <= 1; rv++) {
                    // V1.04 layerInst_calculateMixMode, transcribed
                    uint8_t old = eq ? 0 : f == 2 ? 1 : f == 1 ? 2 : fx == 2 ? 3 : fx == 1 ? 4 : rv ? 5 : 6;
                    CHECK(routeEntryFromLegacy(eq, f, fx, rv) == old, "eq %u filter %u effect %u reverb %u", eq, f, fx, rv);
                }
}

// Crossfades: with pass-through modules the master must equal the input on every sample,
// whatever the route does, so a route change can never jump in level.
static void testCrossfades() {
    static float nodeL[6][N], nodeR[6][N], L[N], R[N];
    float dc[N];
    for (uint16_t i = 0; i < N; i++) dc[i] = 1.0f;
    RouteBuffers buf = {{nodeL[0], nodeL[1], nodeL[2], nodeL[3], nodeL[4], nodeL[5]}, {nodeR[0], nodeR[1], nodeR[2], nodeR[3], nodeR[4], nodeR[5]}, L, R};
    auto passThrough = [&](uint8_t) {};

    // A source: MASTER -> REVERB. First block snaps, then a 64-sample fade over two blocks.
    RouteState state;
    RouteFade fade;
    RouteSource src = {dc, routeBit(NODE_MASTER), &fade};
    routeBlock<N>(kDefaultRoutePatch, state, buf, &src, 1, passThrough);
    CHECK(!routeFading(fade) && fade.mask == routeBit(NODE_MASTER), "first route must snap");
    float reverbShare[3 * N];
    src.mask = routeBit(NODE_REV);
    for (int b = 0; b < 3; b++) {
        routeBlock<N>(kDefaultRoutePatch, state, buf, &src, 1, [&](uint8_t m) {
            if (m == NODE_REV) {
                for (uint16_t i = 0; i < N; i++) reverbShare[b * N + i] = nodeL[m][i];
            }
        });
        for (uint16_t i = 0; i < N; i++) CHECK(std::fabs(L[i] - 1.0f) < 1e-6f && std::fabs(R[i] - 1.0f) < 1e-6f, "block %d sample %u: level %g", b, i, (double)L[i]);
    }
    for (uint16_t i = 0; i < 3 * N; i++) {
        float want = i < kRouteFadeLength ? (float)(i + 1) / kRouteFadeLength : 1.0f;
        CHECK(std::fabs(reverbShare[i] - want) < 1e-6f, "reverb share at %u: %g want %g", i, (double)reverbShare[i], (double)want);
    }

    // A module output: EQ -> FX 2 instead of Filter 1, signal entering at EQ. Level stays 1.
    RouteState st2;
    RouteSource eqIn = {dc, routeBit(NODE_EQ), nullptr};
    RoutePatch p = kDefaultRoutePatch;
    routeBlock<N>(p, st2, buf, &eqIn, 1, passThrough);
    p.moduleDest[NODE_EQ] = routeBit(NODE_FX2);
    for (int b = 0; b < 3; b++) {
        routeBlock<N>(p, st2, buf, &eqIn, 1, passThrough);
        for (uint16_t i = 0; i < N; i++) CHECK(std::fabs(L[i] - 1.0f) < 1e-6f, "module route change: block %d sample %u level %g", b, i, (double)L[i]);
    }
    CHECK(!routeFading(st2.module[NODE_EQ]) && st2.module[NODE_EQ].mask == routeBit(NODE_FX2), "module fade must finish on FX 2");

    // Turning the encoder fast: a new destination every block. With every module sending
    // straight to the master, node m's input is exactly the source's share in m, so we can check
    // that no share ever moves by more than one fade step per sample, across block boundaries
    // too, and that the total at the master stays 1.
    RoutePatch flat = {{routeBit(NODE_MASTER), routeBit(NODE_MASTER), routeBit(NODE_MASTER), routeBit(NODE_MASTER), routeBit(NODE_MASTER), routeBit(NODE_MASTER)}};
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> dest(0, 6);
    RouteState st3;
    RouteFade f3;
    RouteSource s3 = {dc, routeBit(NODE_MASTER), &f3};
    routeBlock<N>(flat, st3, buf, &s3, 1, passThrough);  // first route snaps (by design); start from here
    float prev[7] = {0, 0, 0, 0, 0, 0, 1};
    float worstStep = 0;
    for (int b = 0; b < 400; b++) {
        s3.mask = routeBit((uint8_t)dest(rng));
        float share[N][7] = {};
        routeBlock<N>(flat, st3, buf, &s3, 1, [&](uint8_t m) {
            for (uint16_t i = 0; i < N; i++) share[i][m] = nodeL[m][i];
        });
        for (uint16_t i = 0; i < N; i++) {
            float modules = 0;
            for (uint8_t m = 0; m < 6; m++) modules += share[i][m];
            share[i][6] = L[i] - modules;  // direct-to-master share
            CHECK(std::fabs(L[i] - 1.0f) < 1e-5f, "fast changes: block %d sample %u level %g", b, i, (double)L[i]);
            for (uint8_t m = 0; m < 7; m++) {
                worstStep = std::fmax(worstStep, std::fabs(share[i][m] - prev[m]));
                prev[m] = share[i][m];
            }
        }
    }
    CHECK(worstStep <= 1.0f / kRouteFadeLength + 1e-5f, "a share jumped by %g in one sample", (double)worstStep);
}

static void testFileStorage() {
    uint8_t kit[1500];
    RoutePatch p;
    // Old file (zeros) and the blank-kit template (spaces): default chain.
    memset(kit, 0, sizeof(kit));
    CHECK(!routeReadPatch(kit, p) && memcmp(&p, &kDefaultRoutePatch, sizeof(p)) == 0, "zeros must load the default chain");
    memset(kit, ' ', sizeof(kit));
    CHECK(!routeReadPatch(kit, p) && memcmp(&p, &kDefaultRoutePatch, sizeof(p)) == 0, "template spaces must load the default chain");
    // Round trip of a parallel patch.
    RoutePatch par = {{routeBit(NODE_FX2), routeBit(NODE_MASTER), static_cast<uint8_t>(routeBit(NODE_FX1) | routeBit(NODE_MASTER)), routeBit(NODE_REV), routeBit(NODE_MASTER), routeBit(NODE_MASTER)}};
    memset(kit, 0, sizeof(kit));
    routeWritePatch(kit, par);
    CHECK(routeReadPatch(kit, p) && memcmp(&p, &par, sizeof(p)) == 0, "patch must round-trip");
    // Only bytes 490-495 and 498 are touched.
    for (uint16_t i = 0; i < sizeof(kit); i++)
        if (!((i >= 490 && i <= 495) || i == 498)) CHECK(kit[i] == 0, "byte %u written", i);
    // Invalid masks (backwards, reverb not to master) fall back per module.
    kit[490 + NODE_F2] = routeBit(NODE_F1);   // backwards
    kit[490 + NODE_REV] = routeBit(NODE_FX1); // reverb must feed the master
    routeReadPatch(kit, p);
    CHECK(p.moduleDest[NODE_F2] == kDefaultRoutePatch.moduleDest[NODE_F2] && p.moduleDest[NODE_REV] == routeBit(NODE_MASTER), "invalid masks must fall back");
    CHECK(p.moduleDest[NODE_EQ] == par.moduleDest[NODE_EQ], "valid masks must survive next to invalid ones");
    // Metronome destination in the system preset byte 11.
    for (uint8_t mute = 0; mute <= 1; mute++)
        for (uint8_t e = 0; e < kRouteNodeCount; e++) {
            uint8_t b = routePackMetro(mute, e);
            CHECK(routeUnpackMetro(b) == e && (b & 1) == mute, "metro %u mute %u", e, mute);
        }
    CHECK(routeUnpackMetro(0) == NODE_REV && routeUnpackMetro(1) == NODE_REV, "unset metronome route must be the reverb");
}

// Encoder stepping on the ROUTING page: every value offered must be a valid destination,
// all valid destinations must be reachable, and stepping must wrap both ways.
static void testModuleOutStepping() {
    for (uint8_t m = 0; m < NODE_REV; m++) {
        uint8_t cur = routeFirstNode(kDefaultRoutePatch.moduleDest[m]);
        bool seen[kRouteNodeCount] = {};
        for (int i = 0; i < 20; i++) {
            cur = routeNextModuleOut(m, cur, true);
            CHECK(routeModuleDestValid(m, routeBit(cur)), "module %u stepped up to invalid %u", m, cur);
            seen[cur] = true;
        }
        for (uint8_t n = 0; n < kRouteNodeCount; n++) CHECK(seen[n] == (n > m), "module %u: node %u reachable=%d", m, n, (int)seen[n]);
        for (int i = 0; i < 20; i++) {
            uint8_t down = routeNextModuleOut(m, cur, false);
            CHECK(routeModuleDestValid(m, routeBit(down)), "module %u stepped down to invalid %u", m, down);
            CHECK(routeNextModuleOut(m, down, true) == cur, "module %u: down then up must return", m);
            cur = down;
        }
        CHECK(routeNextModuleOut(m, NODE_MASTER, true) == m + 1 && routeNextModuleOut(m, m + 1, false) == NODE_MASTER, "module %u must wrap", m);
        CHECK(routeNextModuleOut(m, m, true) == m + 1, "module %u: invalid current must snap to first", m);
    }
    CHECK(routeFirstNode(0) == NODE_MASTER && routeFirstNode(routeBit(NODE_FX1) | routeBit(NODE_MASTER)) == NODE_FX1, "routeFirstNode");
}

static void testOverview() {
    char line[64];
    // Default chain from EQ.
    routeOverviewLine(line, sizeof(line), 0x3 | (1u << kRouteSourceMetro), NODE_EQ, kDefaultRoutePatch);
    CHECK(strcmp(line, "L1 L2 MT > EQ > F1 > F2 > FX1 > FX2 > REV > OUT") == 0, "got '%s'", line);
    // Runs of three or more are shortened; line-in last.
    routeOverviewLine(line, sizeof(line), 0x1 | 0x4 | 0x8 | 0x10 | 0x200 | (1u << kRouteSourceLineIn), NODE_F2, kDefaultRoutePatch);
    CHECK(strcmp(line, "L1 L3-5 L10 LI > F2 > FX1 > FX2 > REV > OUT") == 0, "got '%s'", line);
    // Parallel patch from the design.
    RoutePatch p = {{routeBit(NODE_FX2), routeBit(NODE_MASTER), routeBit(NODE_FX1), routeBit(NODE_REV), routeBit(NODE_MASTER), routeBit(NODE_MASTER)}};
    routeOverviewLine(line, sizeof(line), 0x1, NODE_F1, p);
    CHECK(strcmp(line, "L1 > F1 > OUT") == 0, "got '%s'", line);
    routeOverviewLine(line, sizeof(line), 0x2, NODE_F2, p);
    CHECK(strcmp(line, "L2 > F2 > FX1 > REV > OUT") == 0, "got '%s'", line);
    routeOverviewLine(line, sizeof(line), 0x4, NODE_MASTER, p);
    CHECK(strcmp(line, "L3 > OUT") == 0, "got '%s'", line);
    // The default kit's line (all layers and line-in at EQ) is 47 characters: on the 46-character
    // screen it drops the spaces around '>' instead of being cut.
    routeOverviewLine(line, 47, 0x3FF | (1u << kRouteSourceLineIn), NODE_EQ, kDefaultRoutePatch);
    CHECK(strcmp(line, "L1-10 LI>EQ>F1>F2>FX1>FX2>REV>OUT") == 0, "default kit line: '%s'", line);
    // The ROUTING page shows 36 characters (7x9 font): the default line and the worst case,
    // every source at EQ, both fit once compacted.
    routeOverviewLine(line, 37, 0x3FF | (1u << kRouteSourceLineIn), NODE_EQ, kDefaultRoutePatch);
    CHECK(strcmp(line, "L1-10 LI>EQ>F1>F2>FX1>FX2>REV>OUT") == 0, "default kit line at 36: '%s'", line);
    routeOverviewLine(line, 37, 0x3FF | (1u << kRouteSourceMetro) | (1u << kRouteSourceLineIn), NODE_EQ, kDefaultRoutePatch);
    CHECK(strcmp(line, "L1-10 MT LI>EQ>F1>F2>FX1>FX2>REV>OUT") == 0, "all sources at EQ: '%s'", line);
    routeOverviewLine(line, 37, 0x2, NODE_F2, kDefaultRoutePatch);
    CHECK(strcmp(line, "L2 > F2 > FX1 > FX2 > REV > OUT") == 0, "short line keeps spaces at 36: '%s'", line);
    routeOverviewLine(line, 47, 0x3, NODE_EQ, kDefaultRoutePatch);
    CHECK(strcmp(line, "L1 L2 > EQ > F1 > F2 > FX1 > FX2 > REV > OUT") == 0, "short lines keep spaces: '%s'", line);
    // Truncation keeps the buffer bounded and marks the cut.
    uint8_t len = routeOverviewLine(line, 20, 0x3FF, NODE_EQ, kDefaultRoutePatch);
    CHECK(len == 19 && strlen(line) == 19 && line[17] == '.' && line[18] == '.', "truncated: '%s' (%u)", line, len);
    // A muted module ends the chain.
    RoutePatch muted = kDefaultRoutePatch;
    muted.moduleDest[NODE_F1] = 0;
    routeOverviewLine(line, sizeof(line), 0x1, NODE_EQ, muted);
    CHECK(strcmp(line, "L1 > EQ > F1") == 0, "got '%s'", line);
    // Fed modules: only F1 path and FX1 chain are fed in the parallel patch with L1->F1, L2->F2.
    const uint8_t masks[2] = {routeBit(NODE_F1), routeBit(NODE_F2)};
    uint8_t fed = routeFedModules(p, masks, 2);
    CHECK(fed == (routeBit(NODE_F1) | routeBit(NODE_F2) | routeBit(NODE_FX1) | routeBit(NODE_REV)), "fed 0x%02x", fed);
    const uint8_t one[1] = {routeBit(NODE_EQ)};
    CHECK(routeFedModules(kDefaultRoutePatch, one, 1) == 0x3F, "default chain from EQ feeds everything");
    const uint8_t none[1] = {routeBit(NODE_MASTER)};
    CHECK(routeFedModules(kDefaultRoutePatch, none, 1) == 0, "dry sources feed no module");
}

int main() {
    testOverview();
    testModuleOutStepping();
    testFileStorage();
    testLegacyConversion();
    testDefaultPatchMatchesLegacy();
    testParallelPaths();
    testValidation();
    testCrossfades();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
