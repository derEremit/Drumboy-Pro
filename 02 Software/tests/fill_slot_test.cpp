// Host check for the fill-slot leak. The ownership rule and reset order match
// Bank in Core/Library/Global/Global.h. This file is not part of the CubeIDE build.

#include <cstdio>
#include <cstdint>

static const uint8_t kBeatFillLibrarySize = 32;
static const uint8_t kBeatLibrarySize = 64;
static const uint8_t NULL_FILL_TARGET = 255;

struct Beat {
    bool active = false;
    uint8_t fillTarget = NULL_FILL_TARGET;
    uint8_t fillPattern = 0;
};

struct BeatFill {
    bool active = false;
    void reset() { active = false; }
};

struct Bank {
    Beat beatLibrary[kBeatLibrarySize];
    BeatFill beatFillLibrary[kBeatFillLibrarySize];
    int8_t lastActiveBeatNum = -1;

    int16_t ownerOfFillTarget(uint8_t fillIdx) const {
        for (uint8_t i = 0; i < kBeatLibrarySize; i++) {
            if (beatLibrary[i].active && beatLibrary[i].fillTarget == fillIdx) return i;
        }
        return -1;
    }

    uint8_t findFreeFillSlot() const {
        for (uint8_t i = 0; i < kBeatFillLibrarySize; i++) {
            if (!beatFillLibrary[i].active && ownerOfFillTarget(i) == -1) return i;
        }
        return NULL_FILL_TARGET;
    }

    // Stock V1.01 order: the beat is inactive before the owner check, so the slot stays taken.
    void resetBeatLeaky(uint8_t beatNum) {
        Beat& beat = beatLibrary[beatNum];
        uint8_t oldTarget = beat.fillTarget;
        beat.active = false;
        if (oldTarget != NULL_FILL_TARGET && ownerOfFillTarget(oldTarget) == beatNum) {
            beatFillLibrary[oldTarget].reset();
        }
        beat.fillTarget = NULL_FILL_TARGET;
        beat.fillPattern = 0;
    }

    // Fixed order: free the slot while the beat is still active.
    void resetBeat(uint8_t beatNum) {
        Beat& beat = beatLibrary[beatNum];
        uint8_t oldTarget = beat.fillTarget;
        if (oldTarget != NULL_FILL_TARGET && ownerOfFillTarget(oldTarget) == beatNum) {
            beatFillLibrary[oldTarget].reset();
        }
        beat.active = false;
        beat.fillTarget = NULL_FILL_TARGET;
        beat.fillPattern = 0;
    }

    uint8_t setBeatFill(uint8_t beatNum, uint8_t pattern) {
        Beat& beat = beatLibrary[beatNum];
        if (pattern <= 2) {
            uint8_t oldTarget = beat.fillTarget;
            if (oldTarget != NULL_FILL_TARGET && ownerOfFillTarget(oldTarget) == beatNum) {
                beatFillLibrary[oldTarget].reset();
            }
            beat.fillPattern = pattern;
            beat.fillTarget = NULL_FILL_TARGET;
            return NULL_FILL_TARGET;
        }
        uint8_t fillTarget = beat.fillTarget;
        if (fillTarget == NULL_FILL_TARGET) fillTarget = findFreeFillSlot();
        if (fillTarget == NULL_FILL_TARGET) {
            beat.fillPattern = 0;
            beat.fillTarget = NULL_FILL_TARGET;
            return NULL_FILL_TARGET;
        }
        beat.fillPattern = pattern;
        beat.fillTarget = fillTarget;
        beatFillLibrary[fillTarget].active = true;
        return fillTarget;
    }

    void resetAll() {
        for (uint8_t i = 0; i < kBeatLibrarySize; i++) resetBeat(i);
        for (uint8_t fill = 0; fill < kBeatFillLibrarySize; fill++) beatFillLibrary[fill].reset();
        lastActiveBeatNum = -1;
    }
};

static int failures = 0;

static void expect(bool ok, const char* what) {
    if (ok) return;
    std::printf("FAIL %s\n", what);
    failures += 1;
}

static void fillAll(Bank& bank) {
    for (uint8_t i = 0; i < kBeatFillLibrarySize; i++) {
        bank.beatLibrary[i].active = true;
        bank.setBeatFill(i, 5);
        bank.lastActiveBeatNum = static_cast<int8_t>(i);
    }
}

int main() {
    Bank leaked;
    fillAll(leaked);
    expect(leaked.setBeatFill(32, 5) == NULL_FILL_TARGET, "33rd complex fill is refused");
    expect(leaked.beatLibrary[32].fillPattern == 0, "refused fill is stored as FLAT");
    for (uint8_t i = 0; i < kBeatFillLibrarySize; i++) leaked.resetBeatLeaky(i);
    leaked.lastActiveBeatNum = -1;
    uint8_t stillTaken = 0;
    for (uint8_t i = 0; i < kBeatFillLibrarySize; i++) {
        if (leaked.beatFillLibrary[i].active) stillTaken += 1;
    }
    expect(stillTaken == kBeatFillLibrarySize, "stock clear leaves every slot taken");
    leaked.beatLibrary[0].active = true;
    expect(leaked.setBeatFill(0, 3) == NULL_FILL_TARGET, "BASIC_2 still fails after a stock clear");

    Bank fixed;
    fillAll(fixed);
    for (uint8_t i = 0; i < kBeatFillLibrarySize; i++) fixed.resetBeat(i);
    fixed.beatLibrary[0].active = true;
    expect(fixed.setBeatFill(0, 3) == 0, "short clear returns the slot");
    expect(fixed.beatLibrary[0].fillPattern == 3, "BASIC_2 stays stored");

    Bank orphan;
    fillAll(orphan);
    for (uint8_t i = 0; i < kBeatFillLibrarySize; i++) orphan.resetBeatLeaky(i);
    orphan.lastActiveBeatNum = -1;
    orphan.resetAll();
    orphan.beatLibrary[0].active = true;
    expect(orphan.setBeatFill(0, 5) == 0, "full clear frees slots orphaned by the old order");

    Bank neighbor;
    neighbor.beatLibrary[0].active = true;
    neighbor.beatLibrary[1].active = true;
    neighbor.setBeatFill(0, 5);
    neighbor.setBeatFill(1, 5);
    neighbor.resetBeat(0);
    expect(!neighbor.beatFillLibrary[0].active, "clearing one beat frees only its slot");
    expect(neighbor.beatFillLibrary[1].active, "the other beat keeps its slot");

    if (failures == 0) {
        std::printf("ok\n");
        return 0;
    }
    std::printf("%d failed\n", failures);
    return 1;
}
