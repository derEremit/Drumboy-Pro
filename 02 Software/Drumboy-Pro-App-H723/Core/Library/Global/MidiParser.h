// MIDI input: a byte-by-byte parser and a small event queue between the UART interrupt and the
// main loop. Hardware-free so it can be tested on a PC (see 02 Software/tests).
//
// Parser (midiParse), following the MIDI 1.0 rules:
// - Real-time bytes (0xF8-0xFF) are returned at once and may appear anywhere, even between the
//   data bytes of another message; they don't touch the running status.
// - Running status: data bytes without a new status byte reuse the last channel status, so a
//   controller may send 0x90 once and then key/velocity pairs.
// - System common messages (0xF1-0xF6) and SysEx cancel the running status; their data bytes
//   are skipped.
// - Note On with velocity 0 is a Note Off.
// - The Drumboy's own SysEx transport commands (F0 7D 52 cmd F7) come back as MIDI_SYSEX_CMD.
//
// Queue (MidiQueue): one writer (the UART interrupt) and one reader (the main loop), each owning
// its own index, so no lock is needed. A full queue drops the new event and counts it.
#ifndef __MIDIPARSER_H
#define __MIDIPARSER_H

#include <math.h>
#include <stdint.h>

enum MidiEventType : uint8_t {
    MIDI_EVENT_NONE = 0,
    MIDI_EVENT_NOTE_ON = 1,
    MIDI_EVENT_NOTE_OFF = 2,
    MIDI_EVENT_CC = 3,
    MIDI_EVENT_REALTIME = 4,   // data1 = the byte (0xF8 clock, 0xFA start, ...)
    MIDI_EVENT_SYSEX_CMD = 5,  // data1 = the command byte of F0 7D 52 cmd F7
    MIDI_EVENT_OTHER = 6,      // pitch bend, aftertouch, program change: parsed, unused
};

struct MidiEvent {
    uint8_t type;
    uint8_t channel;  // 0-15 for channel messages
    uint8_t data1;    // key, controller or real-time byte
    uint8_t data2;    // velocity or value
};

const uint8_t kMidiSysExManufacturer = 0x7D;  // non-commercial
const uint8_t kMidiSysExFamily = 0x52;        // Drumboy transport

struct MidiParser {
    uint8_t status = 0;  // running status (0x80-0xEF) or 0 = none
    uint8_t data[2] = {0, 0};
    uint8_t count = 0;
    uint8_t sysexStage = 0;  // 0 none, 1 after F0, 2 after manufacturer, 3 after family, 4 after cmd, 5 other SysEx
    uint8_t sysexCmd = 0;
};

inline uint8_t midiDataBytes(uint8_t status_) {
    const uint8_t kind = status_ & 0xF0;
    return ((kind == 0xC0) || (kind == 0xD0)) ? 1 : 2;
}

// Feeds one byte. Returns true and fills event_ when a message is complete.
inline bool midiParse(MidiParser& p_, uint8_t byte_, MidiEvent& event_) {
    if (byte_ >= 0xF8) {
        event_ = {MIDI_EVENT_REALTIME, 0, byte_, 0};
        return true;
    }
    if (byte_ == 0xF0) {
        p_.status = 0;
        p_.count = 0;
        p_.sysexStage = 1;
        p_.sysexCmd = 0;
        return false;
    }
    if (byte_ == 0xF7) {
        const bool cmd = (p_.sysexStage == 4);
        p_.sysexStage = 0;
        p_.status = 0;
        p_.count = 0;
        if (cmd) {
            event_ = {MIDI_EVENT_SYSEX_CMD, 0, p_.sysexCmd, 0};
            return true;
        }
        return false;
    }
    if (byte_ >= 0xF1) {  // system common: cancels running status, its data bytes are skipped
        p_.status = 0;
        p_.count = 0;
        p_.sysexStage = 0;
        return false;
    }
    if (byte_ & 0x80) {  // channel status
        p_.status = byte_;
        p_.count = 0;
        p_.sysexStage = 0;
        return false;
    }

    // Data byte.
    if (p_.sysexStage != 0) {
        switch (p_.sysexStage) {
            case 1: p_.sysexStage = (byte_ == kMidiSysExManufacturer) ? 2 : 5; break;
            case 2: p_.sysexStage = (byte_ == kMidiSysExFamily) ? 3 : 5; break;
            case 3: p_.sysexCmd = byte_; p_.sysexStage = 4; break;
            default: p_.sysexStage = 5; break;  // longer or foreign SysEx: wait for F7
        }
        return false;
    }
    if (p_.status == 0) return false;
    p_.data[p_.count++] = byte_;
    if (p_.count < midiDataBytes(p_.status)) return false;
    p_.count = 0;  // keep the status: running status

    const uint8_t kind = p_.status & 0xF0;
    const uint8_t channel = p_.status & 0x0F;
    switch (kind) {
        case 0x90:
            event_ = {(p_.data[1] == 0) ? MIDI_EVENT_NOTE_OFF : MIDI_EVENT_NOTE_ON, channel, p_.data[0], p_.data[1]};
            break;
        case 0x80:
            event_ = {MIDI_EVENT_NOTE_OFF, channel, p_.data[0], p_.data[1]};
            break;
        case 0xB0:
            event_ = {MIDI_EVENT_CC, channel, p_.data[0], p_.data[1]};
            break;
        default:
            event_ = {MIDI_EVENT_OTHER, channel, p_.data[0], (midiDataBytes(p_.status) == 2) ? p_.data[1] : static_cast<uint8_t>(0)};
            break;
    }
    return true;
}

template <uint8_t N>
struct MidiQueue {
    static_assert((N & (N - 1)) == 0, "size must be a power of two");
    MidiEvent buf[N];
    volatile uint8_t head = 0;  // written by the producer
    volatile uint8_t tail = 0;  // written by the consumer
    volatile uint16_t dropped = 0;

    bool push(const MidiEvent& e_) {
        const uint8_t h = head;
        if (static_cast<uint8_t>(h - tail) >= N) {
            dropped = static_cast<uint16_t>(dropped + 1);
            return false;
        }
        buf[h & (N - 1)] = e_;
        head = static_cast<uint8_t>(h + 1);  // publish after the event is written
        return true;
    }
    bool pop(MidiEvent& e_) {
        const uint8_t t = tail;
        if (t == head) return false;
        e_ = buf[t & (N - 1)];
        tail = static_cast<uint8_t>(t + 1);
        return true;
    }
    void clear() { tail = head; }
};

// Velocity 1-127 -> the 0-100 level scale the layers and fills use (1 stays audible).
inline uint8_t midiVelocityToLevel(uint8_t velocity_) {
    if (velocity_ == 0) return 0;
    if (velocity_ > 127) velocity_ = 127;
    const uint8_t level = static_cast<uint8_t>((velocity_ * 100 + 63) / 127);
    return (level == 0) ? 1 : level;
}

// A hit's volume multiplier (level^3, as kNumberDataLibrary[level].powMultiplier) -> velocity.
// Exact for the 0-100 levels: FLAT -> 127, GHOST (50) -> 64.
inline uint8_t midiLevelToVelocity(uint8_t level_) {
    if (level_ > 100) level_ = 100;
    const uint8_t v = static_cast<uint8_t>((level_ * 127 + 50) / 100);
    return (level_ > 0 && v == 0) ? 1 : v;
}

// The same from the multiplier layer_playBeat receives.
inline uint8_t midiMultiplierToVelocity(float multiplier_) {
    if (multiplier_ <= 0.0f) return 0;
    if (multiplier_ >= 1.0f) return 127;
    const int level = static_cast<int>(cbrtf(multiplier_) * 100.0f + 0.5f);
    return midiLevelToVelocity(static_cast<uint8_t>(level));
}

// MIDI thru of the real-time bytes: with SYNC on both MIDI IN and MIDI OUT, the Drumboy passes the
// incoming clock and transport on to MIDI OUT. While it follows an incoming clock it sends none of its
// own (see midi_txTransportOn and interruptMidiTxClock), so the receiver sees exactly one clock.
// Clock (0xF8) needs the clock flags on both sides, start, continue, stop and reset (0xFA, 0xFB,
// 0xFC, 0xFF) the transport flags.
inline bool midiThruForwards(uint8_t byte_, bool rxClock_, bool rxTransport_, bool txClock_, bool txTransport_) {
    switch (byte_) {
        case 0xF8:
            return rxClock_ && txClock_;
        case 0xFA:
        case 0xFB:
        case 0xFC:
        case 0xFF:
            return rxTransport_ && txTransport_;
        default:
            return false;
    }
}

#endif
