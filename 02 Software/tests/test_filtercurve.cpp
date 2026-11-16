// Host-side tests for the filter curve view (FilterCurve.h).

#include <cmath>
#include <initializer_list>
#include <cstdio>
#include <cstdlib>

#include "FilterCurve.h"

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

static const float kRate = 44100.0f;

// A filter as the firmware designs it (filterDesign), with the page's DRY and WET.
// slope: 0 = 6 dB, 1 = 12 dB, 2 = 24 dB per octave.
static FilterShape shape(int type, float hz, float q, int slope = 1, float wet = 1.0f, float dry = 0.0f) {
    FilterShape s;
    s.sections = filterDesign(static_cast<uint8_t>(type), static_cast<uint8_t>(slope), hz, q, kRate, s.section[0], s.section[1]);
    s.wet = wet;
    s.dry = dry;
    s.on = true;
    return s;
}

int main() {
    // --- Axes ---
    CHECK(curvePointX(0) == kCurveX && curvePointX(kCurvePoints - 1) == kCurveX + kCurveWidth, "x ends");
    CHECK(std::fabs(curvePointHz(0) - 20.0f) < 0.01f && std::fabs(curvePointHz(kCurvePoints - 1) - 20000.0f) < 5.0f, "20 Hz to 20 kHz: %f %f", curvePointHz(0),
          curvePointHz(kCurvePoints - 1));
    CHECK(std::fabs(curvePointHz(140) - 200.0f) < 0.5f && std::fabs(curvePointHz(280) - 2000.0f) < 2.0f, "a decade every 140 points");
    CHECK(curveXOfHz(20) == 80 && curveXOfHz(200) == 360 && curveXOfHz(2000) == 640 && curveXOfHz(20000) == 920, "decades at %u %u %u %u", curveXOfHz(20), curveXOfHz(200),
          curveXOfHz(2000), curveXOfHz(20000));
    CHECK(curveXOfHz(1) == 80 && curveXOfHz(90000) == 920, "frequencies outside are clamped");
    for (uint16_t p = 0; p < kCurvePoints; p++) {
        const int dx = static_cast<int>(curveXOfHz(curvePointHz(p))) - curvePointX(p);
        CHECK(dx >= -1 && dx <= 1, "point %u: x %u vs %u", p, curveXOfHz(curvePointHz(p)), curvePointX(p));
    }
    CHECK(curveYOfDb(18) == kCurveY && curveYOfDb(-48) == kCurveY + kCurveHeight, "y ends");
    CHECK(curveYOfDb(0) == kCurveY + 63 && curveYOfDb(-12) == kCurveY + 105, "0 dB at %u, -12 dB at %u", curveYOfDb(0), curveYOfDb(-12));
    CHECK(curveYOfDb(60) == kCurveY && curveYOfDb(-120) == kCurveY + kCurveHeight, "levels outside are clamped");
    CHECK(kCurveX + kCurveWidth < 939 && kCurveY + kCurveHeight < kCurveLabelY && kCurveLabelY + 7 < 440 && kCurveLegendY + 9 < kCurveY, "the plot fits the lane area");

    // --- Response ---
    // Off: flat.
    FilterShape off = shape(1, 1000, 0.7f);
    off.on = false;
    CHECK(filterShapeDb(off, 5000, kRate) == 0.0f, "an off filter is flat");

    // Low-pass 1 kHz, Q 0.70: flat below, about -3 dB at the cutoff, 12 dB per octave above.
    const FilterShape lp = shape(1, 1000, 0.7071f);
    CHECK(std::fabs(filterShapeDb(lp, 50, kRate)) < 0.1f, "low-pass passband %f", filterShapeDb(lp, 50, kRate));
    CHECK(std::fabs(filterShapeDb(lp, 1000, kRate) + 3.01f) < 0.1f, "low-pass at cutoff %f", filterShapeDb(lp, 1000, kRate));
    const float lpOct = filterShapeDb(lp, 4000, kRate) - filterShapeDb(lp, 8000, kRate);
    CHECK(lpOct > 11.5f && lpOct < 16.0f, "low-pass slope %f dB per octave", lpOct);

    // High-pass: the mirror image.
    const FilterShape hp = shape(2, 1000, 0.7071f);
    CHECK(std::fabs(filterShapeDb(hp, 15000, kRate)) < 0.2f, "high-pass passband %f", filterShapeDb(hp, 15000, kRate));
    CHECK(std::fabs(filterShapeDb(hp, 1000, kRate) + 3.01f) < 0.1f, "high-pass at cutoff %f", filterShapeDb(hp, 1000, kRate));
    const float hpOct = filterShapeDb(hp, 250, kRate) - filterShapeDb(hp, 125, kRate);
    CHECK(std::fabs(hpOct - 12.0f) < 0.5f, "high-pass slope %f dB per octave", hpOct);

    // Resonance: a peak of 20 log10(Q) at the cutoff.
    const FilterShape res = shape(1, 1000, 5.5f);
    CHECK(std::fabs(filterShapeDb(res, 1000, kRate) - 20.0f * std::log10(5.5f)) < 0.1f, "peak %f dB", filterShapeDb(res, 1000, kRate));

    // Band-pass: 0 dB at the centre, lower on both sides. Band-stop: a notch there.
    const FilterShape bp = shape(3, 1000, 2.0f);
    CHECK(std::fabs(filterShapeDb(bp, 1000, kRate)) < 0.05f && filterShapeDb(bp, 100, kRate) < -20.0f && filterShapeDb(bp, 10000, kRate) < -20.0f, "band-pass");
    const FilterShape bs = shape(4, 1000, 2.0f);
    CHECK(filterShapeDb(bs, 1000, kRate) < -60.0f && std::fabs(filterShapeDb(bs, 50, kRate)) < 0.1f && std::fabs(filterShapeDb(bs, 15000, kRate)) < 0.1f, "band-stop");

    // 6 dB: a real first-order filter. -3 dB at the cutoff, 6 dB per octave, RESONANCE ignored.
    const FilterShape lp6 = shape(1, 500, 0.7071f, 0), hp6 = shape(2, 500, 0.7071f, 0);
    CHECK(lp6.sections == 1 && std::fabs(filterShapeDb(lp6, 500, kRate) + 3.01f) < 0.1f && std::fabs(filterShapeDb(lp6, 20, kRate)) < 0.05f, "6 dB low-pass at cutoff %f",
          filterShapeDb(lp6, 500, kRate));
    const float lp6Oct = filterShapeDb(lp6, 4000, kRate) - filterShapeDb(lp6, 8000, kRate);
    CHECK(lp6Oct > 5.5f && lp6Oct < 9.0f, "6 dB low-pass slope %f", lp6Oct);
    CHECK(std::fabs(filterShapeDb(hp6, 500, kRate) + 3.01f) < 0.1f && std::fabs((filterShapeDb(hp6, 60, kRate) - filterShapeDb(hp6, 30, kRate)) - 6.0f) < 0.3f, "6 dB high-pass");
    CHECK(std::fabs(filterShapeDb(shape(1, 500, 5.5f, 0), 500, kRate) - filterShapeDb(lp6, 500, kRate)) < 0.001f, "6 dB ignores resonance");
    // 6 dB band-pass: wider than the 12 dB one.
    CHECK(filterShapeDb(shape(3, 1000, 2.0f, 0), 250, kRate) > filterShapeDb(shape(3, 1000, 2.0f, 1), 250, kRate) + 3.0f, "6 dB band-pass is wider");

    // 24 dB: two sections. At the lowest RESONANCE a Butterworth: -3 dB at the cutoff, no peak,
    // 24 dB per octave. Older settings are untouched: 12 dB is the same single section as before.
    const FilterShape lp24 = shape(1, 1000, 0.70f, 2), hp24 = shape(2, 1000, 0.70f, 2);
    CHECK(lp24.sections == 2 && lp.sections == 1, "sections");
    CHECK(std::fabs(filterShapeDb(lp24, 1000, kRate) + 3.01f) < 0.25f, "24 dB low-pass at cutoff %f", filterShapeDb(lp24, 1000, kRate));
    float top = -200.0f;
    for (uint16_t p = 0; p < kCurvePoints; p++) top = std::fmax(top, filterShapeDb(lp24, curvePointHz(p), kRate));
    CHECK(top < 0.1f, "24 dB low-pass has no peak at the lowest resonance: %f", top);
    const float lp24Oct = filterShapeDb(lp24, 3000, kRate) - filterShapeDb(lp24, 6000, kRate);
    CHECK(lp24Oct > 23.0f && lp24Oct < 28.0f, "24 dB low-pass slope %f", lp24Oct);
    const float hp24Oct = filterShapeDb(hp24, 250, kRate) - filterShapeDb(hp24, 125, kRate);
    CHECK(std::fabs(hp24Oct - 24.0f) < 0.6f, "24 dB high-pass slope %f", hp24Oct);
    // With resonance: one peak near the cutoff, growing with Q, and still stable (finite everywhere).
    const float peakLo = filterShapeDb(shape(1, 1000, 2.0f, 2), 1000, kRate), peakHi = filterShapeDb(shape(1, 1000, 5.5f, 2), 1000, kRate);
    CHECK(peakLo > 3.0f && peakHi > peakLo + 6.0f && peakHi < 20.0f, "24 dB resonance peaks %f %f", peakLo, peakHi);
    // 24 dB band-pass: narrower than the 12 dB one, still 0 dB at the centre.
    CHECK(std::fabs(filterShapeDb(shape(3, 1000, 2.0f, 2), 1000, kRate)) < 0.05f && filterShapeDb(shape(3, 1000, 2.0f, 2), 250, kRate) < filterShapeDb(bp, 250, kRate) - 6.0f, "24 dB band-pass");
    // Every design is stable: poles inside the unit circle (|a2| < 1, |a1| < 1 + a2).
    for (int type = 1; type <= 4; type++)
        for (int slope = 0; slope <= 2; slope++)
            for (float hz : {20.0f, 440.0f, 5000.0f, 20000.0f})
                for (float q : {0.70f, 1.5f, 5.5f}) {
                    const FilterShape s = shape(type, hz, q, slope);
                    for (uint8_t k = 0; k < s.sections; k++)
                        CHECK(std::fabs(s.section[k].a2) < 1.0f && std::fabs(s.section[k].a1) < 1.0f + s.section[k].a2, "type %d slope %d %g Hz Q %g section %u", type, slope, hz, q, k);
                }
    // An unknown type passes the signal through.
    CHECK(std::fabs(filterShapeDb(shape(0, 1000, 0.7f, 2), 3000, kRate)) < 0.001f, "type 0 is flat");

    // DRY and WET: wet 0 is silence unless dry is up; wet 0.5 is 6 dB lower; dry alone is flat.
    CHECK(filterShapeDb(shape(1, 1000, 0.7f, 1, 0.0f, 0.0f), 100, kRate) == -120.0f, "no wet, no dry");
    CHECK(std::fabs(filterShapeDb(shape(1, 1000, 0.7071f, 1, 0.5f, 0.0f), 50, kRate) + 6.02f) < 0.1f, "half wet");
    CHECK(std::fabs(filterShapeDb(shape(1, 1000, 0.7071f, 1, 0.0f, 1.0f), 9000, kRate)) < 0.01f, "dry only is flat");
    // A high-pass with its dry added keeps the lows: a shelf, not a cut.
    const FilterShape shelf = shape(2, 1000, 0.7071f, 1, 1.0f, 0.5f);
    CHECK(std::fabs(filterShapeDb(shelf, 30, kRate) + 6.02f) < 0.3f && std::fabs(filterShapeDb(shelf, 15000, kRate) - 3.52f) < 0.3f, "high-pass plus dry: %f %f",
          filterShapeDb(shelf, 30, kRate), filterShapeDb(shelf, 15000, kRate));

    // Every point of every curve lands inside the plot.
    for (int type = 1; type <= 4; type++) {
        const float kCutoffs[] = {20.0f, 150.0f, 1000.0f, 8000.0f, 20000.0f};
        for (float hz : kCutoffs) {
            const FilterShape s = shape(type, hz, 5.5f);
            for (uint16_t p = 0; p < kCurvePoints; p++) {
                const float db = filterShapeDb(s, curvePointHz(p), kRate);
                const uint16_t y = curveYOfDb(db);
                CHECK(std::isfinite(db) && y >= kCurveY && y <= kCurveY + kCurveHeight, "type %d %g Hz point %u: %f dB y %u", type, hz, p, db, y);
            }
        }
    }

    // --- EQ: sections in series add their gains; the EQ scale ---
    {
        BiquadCoef flat[6];
        CHECK(std::fabs(eqShapeDb(flat, 6, 1000, kRate)) < 0.001f, "six flat bands are flat");
        // Two peaks of +6 dB at 1 kHz (RBJ peaking, Q 1) give +12 dB there.
        const float A = std::pow(10.0f, 6.0f / 40.0f), w = 6.2831853f * 1000.0f / kRate, al = std::sin(w) / 2.0f, a0 = 1 + al / A;
        BiquadCoef pk;
        pk.b0 = (1 + al * A) / a0, pk.b1 = -2 * std::cos(w) / a0, pk.b2 = (1 - al * A) / a0, pk.a1 = -2 * std::cos(w) / a0, pk.a2 = (1 - al / A) / a0;
        flat[1] = pk;
        flat[4] = pk;
        CHECK(std::fabs(eqShapeDb(flat, 6, 1000, kRate) - 12.0f) < 0.05f && std::fabs(eqShapeDb(flat, 6, 30, kRate)) < 0.2f, "two +6 dB peaks: %f", eqShapeDb(flat, 6, 1000, kRate));
        CHECK(curveYOfDbRange(24, 24, -36) == kCurveY && curveYOfDbRange(-36, 24, -36) == kCurveY + kCurveHeight && curveYOfDbRange(99, 24, -36) == kCurveY, "EQ scale ends");
        CHECK(curveYOfDbRange(0, 24, -36) == kCurveY + 92 && curveYOfDbRange(-5, 18, -48) == curveYOfDb(-5), "EQ scale 0 dB at %u", curveYOfDbRange(0, 24, -36));
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
