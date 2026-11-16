// Firmware version: the single place to change it.
// `make package` names the SD-card update file from these values and writes them into its
// header (the bootloader shows them while updating), and the release workflow checks that a
// pushed tag vMAJOR.MINOR matches them. The bootloader accepts MAJOR 0-9 and MINOR 0-99.
#ifndef __VERSION_H
#define __VERSION_H

#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 16

#endif
