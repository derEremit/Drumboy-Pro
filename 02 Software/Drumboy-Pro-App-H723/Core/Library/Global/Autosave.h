// Autosave: the song is written now and then to two reserved files, so a power cut, a crash or a
// LOAD over unsaved work loses at most a few minutes. Hardware-free so it can be tested on a PC
// (see 02 Software/tests).
//
// - Change detection: a CRC-32 of the song as it would be saved (kit, the 10 layers' banks, the
//   chain). Different from the CRC of the last save, load or autosave = unsaved work.
// - Two files, A and B, written alternately; each ends with a trailer that holds a sequence
//   number and the CRC of everything before it. A write cut short leaves a file without a valid
//   trailer, and the other file still holds the previous good copy.
// - The autosave files are deleted after a manual SAVE and when the restore is declined, so an
//   autosave file that exists always holds work that was not saved into a slot.
// - When: every AUTOSAVE minutes (OFF, 2, 5, 10, 20), only while it can't disturb playing: not
//   recording, no key for 2 s, and if the sequencer plays, only when no MIDI notes come in (the
//   short SD write would hold them back) and at a loop end.
//
// File: a normal song file (older firmware loads it like any other) followed by the trailer:
//   "ASV" | 1 (trailer version) | sequence (4 bytes LE) | CRC-32 of all bytes before (4 bytes LE)
#ifndef __AUTOSAVE_H
#define __AUTOSAVE_H

#include <stdint.h>
#include <string.h>

const uint8_t kAutosaveTrailerSize = 12;
const uint8_t kAutosaveIntervalCount = 5;
const uint8_t kAutosaveMinutes[kAutosaveIntervalCount] = {0, 2, 5, 10, 20};  // 0 = OFF
const uint8_t kAutosaveDefaultInterval = 3;                                    // 10 minutes
const uint32_t kAutosaveKeyQuietMs = 2000;

// CRC-32 (IEEE 802.3, the zip/PNG one). Start with crc = 0, feed blocks in order.
inline uint32_t autosaveCrc32(uint32_t crc_, const uint8_t* data_, uint32_t size_) {
    crc_ = ~crc_;
    for (uint32_t i = 0; i < size_; i++) {
        crc_ ^= data_[i];
        for (uint8_t k = 0; k < 8; k++) crc_ = (crc_ >> 1) ^ (0xEDB88320u & (0u - (crc_ & 1u)));
    }
    return ~crc_;
}

inline void autosaveWriteTrailer(uint8_t* out_, uint32_t sequence_, uint32_t crc_) {
    memcpy(out_, "ASV", 3);
    out_[3] = 1;
    for (uint8_t i = 0; i < 4; i++) {
        out_[4 + i] = static_cast<uint8_t>(sequence_ >> (8 * i));
        out_[8 + i] = static_cast<uint8_t>(crc_ >> (8 * i));
    }
}

// True when in_ is a trailer; fills the sequence and the CRC it records.
inline bool autosaveReadTrailer(const uint8_t* in_, uint32_t& sequence_, uint32_t& crc_) {
    if ((memcmp(in_, "ASV", 3) != 0) || (in_[3] != 1)) return false;
    sequence_ = 0;
    crc_ = 0;
    for (uint8_t i = 0; i < 4; i++) {
        sequence_ |= static_cast<uint32_t>(in_[4 + i]) << (8 * i);
        crc_ |= static_cast<uint32_t>(in_[8 + i]) << (8 * i);
    }
    return true;
}

// One autosave file as found on the card.
struct AutosaveFile {
    bool valid = false;  // complete: trailer present and its CRC matches the content
    uint32_t sequence = 0;
};

// Sequence numbers count up and may wrap: a is newer when it is ahead by less than half the range.
inline bool autosaveNewer(uint32_t a_, uint32_t b_) { return static_cast<int32_t>(a_ - b_) > 0; }

// The file to restore: 0 (A), 1 (B), or -1 when neither is valid.
inline int8_t autosaveNewest(const AutosaveFile& a_, const AutosaveFile& b_) {
    if (a_.valid && b_.valid) return autosaveNewer(b_.sequence, a_.sequence) ? 1 : 0;
    if (a_.valid) return 0;
    if (b_.valid) return 1;
    return -1;
}

// The file to write next: never the newest valid one, so a cut-short write keeps it.
inline uint8_t autosaveNextSlot(const AutosaveFile& a_, const AutosaveFile& b_) {
    const int8_t newest = autosaveNewest(a_, b_);
    return (newest == 0) ? 1 : 0;
}

// Sequence for the next write: one past the newest found.
inline uint32_t autosaveNextSequence(const AutosaveFile& a_, const AutosaveFile& b_) {
    const int8_t newest = autosaveNewest(a_, b_);
    if (newest < 0) return 1;
    return ((newest == 0) ? a_.sequence : b_.sequence) + 1;
}

struct AutosaveTiming {
    uint8_t interval = kAutosaveDefaultInterval;  // index into kAutosaveMinutes
    uint32_t lastCheckMs = 0;                      // last time the interval started
};

inline uint32_t autosaveIntervalMs(uint8_t interval_) {
    if (interval_ >= kAutosaveIntervalCount) interval_ = kAutosaveDefaultInterval;
    return static_cast<uint32_t>(kAutosaveMinutes[interval_]) * 60000u;
}

// The interval has passed (OFF never is). Wraps of the millisecond counter are fine.
inline bool autosaveDue(const AutosaveTiming& t_, uint32_t nowMs_) {
    const uint32_t interval = autosaveIntervalMs(t_.interval);
    return (interval > 0) && ((nowMs_ - t_.lastCheckMs) >= interval);
}

struct AutosaveContext {
    bool playing;
    bool recording;      // recording, or a precount before it
    bool midiNotesIn;    // MIDI IN plays notes (a write would hold them back)
    bool atLoopEnd;      // the sequencer just wrapped to the start of the loop
    bool busy;           // a load, save or other SD work, or a question on screen
    uint32_t msSinceKey; // time since the last key or encoder
};

// Whether writing now can't disturb the player.
inline bool autosaveSafeNow(const AutosaveContext& c_) {
    if (c_.busy || c_.recording) return false;
    if (c_.msSinceKey < kAutosaveKeyQuietMs) return false;
    if (c_.playing) return !c_.midiNotesIn && c_.atLoopEnd;
    return true;
}

#endif
