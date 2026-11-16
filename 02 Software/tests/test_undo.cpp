// Host-side tests for undo / redo (UndoLog.h): a simulated song (one number per layer and bank)
// is edited, undone and redone at random and checked against a full history of states.

#include <cstdio>
#include <map>
#include <random>
#include <vector>

#include "UndoLog.h"

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

const int kLayers = 10, kBanks = 5;
struct Song {
    int v[kLayers][kBanks];
    bool operator==(const Song& o) const {
        for (int l = 0; l < kLayers; l++)
            for (int b = 0; b < kBanks; b++)
                if (v[l][b] != o.v[l][b]) return false;
        return true;
    }
};

template <uint16_t N>
struct Device {
    Song song{};
    UndoLog<N> log;
    int store[N];
    uint32_t now = 0;
    // One edit of (layer, bank) the way the firmware does it: snapshot first, then change.
    bool edit(UndoKind kind, int layer, int bank, int value) {  // true: a new undo step
        uint16_t s;
        const bool fresh = log.begin(kind, static_cast<uint8_t>(layer), static_cast<uint8_t>(bank), now, s);
        if (fresh) store[s] = song.v[layer][bank];
        song.v[layer][bank] = value;
        return fresh;
    }
    // A whole-bank edit: one group, every layer.
    void editBank(int bank, int value) {
        log.startGroup();
        for (int l = 0; l < kLayers; l++) {
            const uint16_t s = log.add(static_cast<uint8_t>(l), static_cast<uint8_t>(bank));
            store[s] = song.v[l][bank];
            song.v[l][bank] = value + l;
        }
        log.lastKind = UNDO_BANK;
    }
    void swap(uint16_t* slots, uint16_t n) {
        for (uint16_t i = 0; i < n; i++) {
            const UndoEntry& e = log.entry[slots[i]];
            const int t = song.v[e.layer][e.bank];
            song.v[e.layer][e.bank] = store[slots[i]];
            store[slots[i]] = t;
        }
    }
    bool undo() {
        uint16_t slots[N];
        const uint16_t n = log.undo(slots, N);
        swap(slots, n);
        return n > 0;
    }
    bool redo() {
        uint16_t slots[N];
        const uint16_t n = log.redo(slots, N);
        swap(slots, n);
        return n > 0;
    }
};

static void testBasics() {
    Device<64> d;
    CHECK(!d.undo() && !d.redo(), "nothing to undo or redo");
    d.edit(UNDO_DELETE, 2, 0, 5);
    d.now += 5000;
    d.edit(UNDO_DELETE, 2, 0, 7);
    CHECK(d.undo() && d.song.v[2][0] == 5, "undo one");
    CHECK(d.undo() && d.song.v[2][0] == 0, "undo two");
    CHECK(!d.undo(), "history start");
    CHECK(d.redo() && d.song.v[2][0] == 5 && d.redo() && d.song.v[2][0] == 7 && !d.redo(), "redo both");
    // A new edit after an undo drops the redo.
    d.undo();
    d.now += 5000;
    d.edit(UNDO_DELETE, 3, 1, 9);
    CHECK(!d.redo() && d.song.v[2][0] == 5 && d.song.v[3][1] == 9, "redo dropped");

    // Knob turns of the same kind within 2 s join: one undo takes back the whole gesture.
    Device<64> k;
    for (int i = 1; i <= 20; i++) {
        k.now += 100;
        k.edit(UNDO_NOTE, 1, 2, i);
    }
    CHECK(k.log.count == 1, "joined: %u", k.log.count);
    CHECK(k.undo() && k.song.v[1][2] == 0, "one undo for the gesture");
    // A pause of 2 s or another layer starts a new step.
    Device<64> p;
    p.edit(UNDO_NOTE, 1, 2, 1);
    p.now += 2500;
    p.edit(UNDO_NOTE, 1, 2, 2);
    p.edit(UNDO_NOTE, 4, 2, 3);
    CHECK(p.log.count == 3, "%u steps", p.log.count);
    // Session kinds join with no time limit until a new session.
    Device<64> s;
    s.log.newSession();
    s.edit(UNDO_EUCLID, 0, 0, 1);
    s.now += 60000;
    s.edit(UNDO_EUCLID, 0, 0, 2);
    CHECK(s.log.count == 1, "euclid session joined");
    s.log.newSession();
    s.edit(UNDO_EUCLID, 0, 0, 3);
    CHECK(s.log.count == 2 && s.undo() && s.song.v[0][0] == 2 && s.undo() && s.song.v[0][0] == 0, "sessions");
    // Nothing joins across an undo.
    Device<64> u;
    u.edit(UNDO_MOVE, 0, 0, 1);
    u.edit(UNDO_MOVE, 0, 0, 2);
    u.undo();
    u.edit(UNDO_MOVE, 0, 0, 3);
    CHECK(u.undo() && u.song.v[0][0] == 0, "fresh step after undo");
    // A whole-bank paste is one step.
    Device<64> b;
    b.edit(UNDO_DELETE, 0, 3, 1);
    b.editBank(3, 100);
    CHECK(b.undo() && b.song.v[0][3] == 1 && b.song.v[9][3] == 0, "bank paste undone at once");
    CHECK(b.redo() && b.song.v[5][3] == 105, "and redone");
    // Recording take across two banks (add() of the same bank twice stores once).
    Device<64> r;
    r.log.startGroup();
    uint16_t s0 = r.log.add(1, 0);
    r.store[s0] = r.song.v[1][0];
    r.song.v[1][0] = 11;
    CHECK(r.log.contains(1, 0) && !r.log.contains(2, 0), "contains");
    CHECK(r.log.add(1, 0) == s0 && r.log.count == 1, "same bank once");
    uint16_t s1 = r.log.add(2, 1);
    r.store[s1] = r.song.v[2][1];
    r.song.v[2][1] = 21;
    CHECK(r.undo() && r.song.v[1][0] == 0 && r.song.v[2][1] == 0, "take undone");
    d.log.clear();
    CHECK(!d.log.canUndo() && !d.log.canRedo(), "clear");
}

// Random edits, undos and redos against a full history of states.
template <uint16_t N>
static void testRandom(uint32_t seed) {
    std::mt19937 rng(seed);
    Device<N> d;
    std::vector<Song> history{d.song};  // states after each applied group
    size_t pos = 0;
    int value = 1;
    for (int step = 0; step < 4000; step++) {
        const int op = rng() % 10;
        d.now += (rng() % 2) ? 3000 : 100;
        if (op < 6) {
            const UndoKind kind = static_cast<UndoKind>(1 + rng() % 4);
            const int layer = rng() % kLayers, bank = rng() % kBanks;
            const bool joined = !d.edit(kind, layer, bank, value++);
            history.resize(pos + 1);
            if (joined) {
                history[pos] = d.song;
            } else {
                history.push_back(d.song);
                pos++;
            }
        } else if (op < 7) {
            d.editBank(rng() % kBanks, value);
            value += 20;
            history.resize(pos + 1);
            history.push_back(d.song);
            pos++;
        } else if (op < 9) {
            if (d.undo()) pos--;
        } else {
            if (d.redo()) pos++;
        }
        // The log may forget old groups: then the history keeps only what is still undoable.
        CHECK(d.song == history[pos], "seed %u step %d", seed, step);
        if (failures) return;
        // Undo all the way back as far as the log goes, then redo all: same states.
    }
    Song end = d.song;
    int undone = 0;
    while (d.undo()) undone++;
    for (int i = 0; i < undone; i++) d.redo();
    CHECK(d.song == end, "undo all then redo all returns to the end state (seed %u)", seed);
}

int main() {
    testBasics();
    testRandom<64>(1);
    testRandom<64>(2);
    testRandom<16>(3);  // small log: old groups are forgotten, the rest stays right
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
