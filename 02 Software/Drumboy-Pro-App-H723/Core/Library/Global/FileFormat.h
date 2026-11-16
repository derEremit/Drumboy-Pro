// Kit and song file format version, hardware-free so it can be tested on a PC.
//
// Kit files and song files both carry the 1500-byte kit section, so one stamp there covers both:
//   496  format version
//   497  check byte: version ^ 0x5A (tells a real stamp from the spaces or zeros older files hold)
// Files without a valid stamp are version 0: everything written by firmware up to V1.10.
//   0  up to V1.10
//   1  V1.11: semitone pitch in layer record byte 24 (Pitch.h)
// A file with a higher version than this firmware knows is refused before anything is changed.
// Song files may also carry a song mode chain after their "EOF" (SongChain.h). It has its own
// marker and older firmware stops reading at "EOF", so it needed no version change.
#ifndef __FILEFORMAT_H
#define __FILEFORMAT_H

#include <stdint.h>

const uint16_t kFormatVersionOffset = 496;
const uint8_t kFormatCheckXor = 0x5A;
const uint8_t kFormatVersion = 1;

inline void formatWriteVersion(uint8_t* kit_) {
    kit_[kFormatVersionOffset] = kFormatVersion;
    kit_[kFormatVersionOffset + 1] = static_cast<uint8_t>(kFormatVersion ^ kFormatCheckXor);
}

// The version from the two stamp bytes (0 when there is no valid stamp).
inline uint8_t formatVersionFrom(uint8_t version_, uint8_t check_) {
    return (static_cast<uint8_t>(version_ ^ kFormatCheckXor) == check_) ? version_ : 0;
}

inline uint8_t formatReadVersion(const uint8_t* kit_) { return formatVersionFrom(kit_[kFormatVersionOffset], kit_[kFormatVersionOffset + 1]); }

inline bool formatSupported(uint8_t version_) { return version_ <= kFormatVersion; }

#endif
