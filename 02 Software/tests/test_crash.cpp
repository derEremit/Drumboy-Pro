// Host-side tests for the crash record (CrashRecord.h).

#include <cstdio>
#include <string>
#include <cstring>
#include <random>

#include "CrashRecord.h"

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
    CrashRecord r;
    memset(&r, 0, sizeof r);
    CHECK(!crashValid(&r), "zeros are not a record");
    r.kind = CRASH_HARDFAULT;
    r.pc = 0x0801A2B4;
    r.lr = 0x0801C0DF;
    r.psr = 0x61000000;
    r.sp = 0x2404F000;
    r.cfsr = 0x00008200;
    r.hfsr = 0x40000000;
    r.bfar = 0xC1000000;
    r.version = (1 << 8) | 14;
    CHECK(!crashValid(&r), "not sealed yet");
    crashSeal(&r);
    CHECK(crashValid(&r), "sealed record");

    char text[32];
    crashAlertText(&r, text, sizeof text);
    CHECK(!strcmp(text, "CRASH 1 AT 0801A2B4") && strlen(text) == 19, "[%s]", text);
    char line[200];
    const int len = crashLogLine(&r, line, sizeof line);
    CHECK(len > 0 && len < static_cast<int>(sizeof line) && strstr(line, "V1.14 kind=1 pc=0801A2B4 lr=0801C0DF") && strstr(line, "bfar=C1000000") && line[len - 1] == '\n', "[%s]", line);

    // Any changed field breaks the seal.
    for (size_t w = 1; w < sizeof(CrashRecord) / 4 - 1; w++) {
        CrashRecord c = r;
        reinterpret_cast<uint32_t*>(&c)[w] ^= 0x10;
        CHECK(!crashValid(&c), "field %zu changed", w);
    }
    CrashRecord c = r;
    c.kind = 9;
    c.check = crashCheck(&c);
    CHECK(!crashValid(&c), "unknown kind");
    crashClear(&r);
    CHECK(!crashValid(&r), "cleared");

    // Power-on noise: random RAM is never taken for a record, even with the magic in place.
    std::mt19937 rng(21);
    int taken = 0;
    for (int i = 0; i < 200000; i++) {
        CrashRecord n;
        for (size_t w = 0; w < sizeof n / 4; w++) reinterpret_cast<uint32_t*>(&n)[w] = rng();
        n.magic = CRASH_MAGIC;
        n.kind = 1 + rng() % 6;
        taken += crashValid(&n);
    }
    CHECK(taken == 0, "%d noise records accepted", taken);
    CHECK(sizeof(CrashRecord) <= 64, "fits the reserved 64 bytes: %zu", sizeof(CrashRecord));

    // Watchdog restart: the record carries the stage and the page, and is valid.
    {
        CrashRecord w;
        WdStage st = {WD_STAGE_SD, 0x16, 123456u, 0};
        crashFromWatchdog(&w, &st, (1u << 8) | 14u);
        CHECK(crashValid(&w) && w.kind == CRASH_WATCHDOG, "watchdog record valid");
        CHECK(w.pc == ((4u << 8) | 0x16u) && w.lr == 123456u, "stage and page: %08X", w.pc);
        char text[32];
        crashAlertText(&w, text, sizeof text);
        CHECK(std::string(text) == "CRASH 7 S04 P16 L00", "alert %s", text);
        st.spare = 995;
        crashFromWatchdog(&w, &st, (1u << 8) | 14u);
        crashAlertText(&w, text, sizeof text);
        CHECK(std::string(text) == "CRASH 7 S04 P16 LOV" && strlen(text) <= 19, "overload alert %s", text);
        st.spare = 412;
        crashFromWatchdog(&w, &st, (1u << 8) | 14u);
        crashAlertText(&w, text, sizeof text);
        CHECK(std::string(text) == "CRASH 7 S04 P16 L41", "load alert %s", text);
        st.spare = 0;
        crashFromWatchdog(&w, &st, (1u << 8) | 14u);
        st.spare = 987;
        crashFromWatchdog(&w, &st, (1u << 8) | 15u);
        CHECK(w.sp == 987 && crashValid(&w), "the audio load goes into the record");
        CHECK(watchdogReload(6) == 750 && watchdogReload(1000) == 4095, "reload %u", watchdogReload(6));
        CHECK(sizeof(WdStage) == 16 && WATCHDOG_STAGE_ADDRESS + sizeof(WdStage) <= CRASH_RECORD_ADDRESS, "stage block sits below the record");
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
