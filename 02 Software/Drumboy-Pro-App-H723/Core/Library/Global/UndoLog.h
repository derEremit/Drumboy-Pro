// Undo / redo for beat and bank edits (backlog 28). Hardware-free so it can be tested on a PC
// (see 02 Software/tests).
//
// The log only keeps the bookkeeping; the caller stores one Bank per entry (in SDRAM). Before an
// edit the caller copies the bank it is about to change into the entry's slot. Undo and redo
// SWAP each slot with the live bank: after an undo the slot holds the "after" state, so redo is
// the same swap again. An edit that changes several banks (whole-bank paste, a recording take)
// is one group: undo and redo always take a whole group.
//
// Repeated small edits (turning a knob, moving a beat step by step) of the same kind on the same
// layer and bank join the previous group instead of making a new one, so one undo takes back the
// whole gesture: within kUndoJoinMs of the last edit, or with no time limit for kinds that are one
// session (EUCLID page, a recording take).
//
// A new edit drops whatever could still be redone. When the log is full, the oldest group goes.
#ifndef __UNDOLOG_H
#define __UNDOLOG_H

#include <stdint.h>

const uint32_t kUndoJoinMs = 2000;

enum UndoKind : uint8_t {
    UNDO_NONE = 0,
    UNDO_FILL,      // fill type / pattern / time / level of the selected beat
    UNDO_MOVE,      // MOVE / SHIFT ALL / insert
    UNDO_DELETE,    // CLEAR a beat
    UNDO_CLEAR,     // long CLEAR: every beat of the layer
    UNDO_PASTE,     // paste a layer's beats or a fill
    UNDO_BANK,      // paste a whole bank (all layers)
    UNDO_EUCLID,    // the EUCLID page (one session)
    UNDO_NOTE,      // the NOTE page
    UNDO_GENERATE,  // beats generated with the long ADD press
    UNDO_RECORD,    // a recording take (one session)
};

inline bool undoJoinsByTime(UndoKind k_) { return (k_ == UNDO_FILL) || (k_ == UNDO_MOVE) || (k_ == UNDO_NOTE) || (k_ == UNDO_GENERATE); }
inline bool undoJoinsBySession(UndoKind k_) { return (k_ == UNDO_EUCLID) || (k_ == UNDO_RECORD); }

struct UndoEntry {
    uint8_t layer;
    uint8_t bank;
    uint16_t group;
};

template <uint16_t N>
struct UndoLog {
    UndoEntry entry[N];
    uint16_t start = 0;   // oldest entry (ring)
    uint16_t count = 0;   // entries kept
    uint16_t cursor = 0;  // entries applied: [0, cursor) can be undone, [cursor, count) redone
    uint16_t nextGroup = 1;
    UndoKind lastKind = UNDO_NONE;
    uint8_t lastLayer = 0xFF, lastBank = 0xFF;
    uint32_t lastMs = 0;
    uint16_t session = 0;     // current session id (EUCLID page visit, recording take)
    uint16_t lastSession = 0;

    uint16_t slot(uint16_t i_) const { return static_cast<uint16_t>((start + i_) % N); }

    void clear() {
        start = 0;
        count = 0;
        cursor = 0;
        lastKind = UNDO_NONE;
    }

    // A new session for the session kinds (call when the EUCLID page opens or a take starts).
    void newSession() { session = static_cast<uint16_t>(session + 1); }

    bool canUndo() const { return cursor > 0; }
    bool canRedo() const { return cursor < count; }

    // Before an edit of (layer, bank): true when the caller must store the bank's current state in
    // slot_ (a new group, or a new bank in the open group); false when the edit joins the last group
    // and that bank is already in it.
    bool begin(UndoKind kind_, uint8_t layer_, uint8_t bank_, uint32_t nowMs_, uint16_t& slot_) {
        const bool atTop = (cursor == count) && (count > 0);
        bool join = false;
        if (atTop && (kind_ == lastKind) && (layer_ == lastLayer) && (bank_ == lastBank)) {
            if (undoJoinsByTime(kind_) && ((nowMs_ - lastMs) < kUndoJoinMs)) join = true;
            if (undoJoinsBySession(kind_) && (session == lastSession)) join = true;
        }
        lastKind = kind_;
        lastLayer = layer_;
        lastBank = bank_;
        lastMs = nowMs_;
        lastSession = session;
        if (join) return false;
        startGroup();
        slot_ = add(layer_, bank_);
        return true;
    }

    // A group of several banks (whole-bank paste, a take that spans banks): start it, then add()
    // each bank before changing it.
    void startGroup() {
        count = cursor;  // a new edit drops what could be redone
        nextGroup = static_cast<uint16_t>(nextGroup + 1);
        if (nextGroup == 0) nextGroup = 1;
    }

    // Adds (layer, bank) to the open group and returns the slot to store it in. Already in the open
    // group: returns that slot again (nothing new to store; check with contains()).
    uint16_t add(uint8_t layer_, uint8_t bank_) {
        for (uint16_t i = cursor; i > 0; i--) {
            const UndoEntry& e = entry[slot(i - 1)];
            if (e.group != nextGroup) break;
            if ((e.layer == layer_) && (e.bank == bank_)) return slot(i - 1);
        }
        if (count == N) dropOldestGroup();
        const uint16_t s = slot(count);
        entry[s] = UndoEntry{layer_, bank_, nextGroup};
        count = static_cast<uint16_t>(count + 1);
        cursor = count;
        return s;
    }

    // Whether (layer, bank) is already stored in the open group.
    bool contains(uint8_t layer_, uint8_t bank_) const {
        for (uint16_t i = cursor; i > 0; i--) {
            const UndoEntry& e = entry[slot(i - 1)];
            if (e.group != nextGroup) break;
            if ((e.layer == layer_) && (e.bank == bank_)) return true;
        }
        return false;
    }

    // The last applied group, for undo: fills slots_ (up to max_) and returns how many; the caller
    // swaps each slot with its live bank.
    uint16_t undo(uint16_t* slots_, uint16_t max_) {
        if (!canUndo()) return 0;
        const uint16_t g = entry[slot(cursor - 1)].group;
        uint16_t n = 0;
        while ((cursor > 0) && (entry[slot(cursor - 1)].group == g)) {
            if (n < max_) slots_[n++] = slot(cursor - 1);
            cursor--;
        }
        lastKind = UNDO_NONE;  // nothing joins across an undo
        return n;
    }

    // The next undone group, for redo (same swap).
    uint16_t redo(uint16_t* slots_, uint16_t max_) {
        if (!canRedo()) return 0;
        const uint16_t g = entry[slot(cursor)].group;
        uint16_t n = 0;
        while ((cursor < count) && (entry[slot(cursor)].group == g)) {
            if (n < max_) slots_[n++] = slot(cursor);
            cursor++;
        }
        lastKind = UNDO_NONE;
        return n;
    }

  private:
    void dropOldestGroup() {
        const uint16_t g = entry[start].group;
        while ((count > 0) && (entry[start].group == g)) {
            start = static_cast<uint16_t>((start + 1) % N);
            count--;
            if (cursor > 0) cursor--;
        }
    }
};

#endif
