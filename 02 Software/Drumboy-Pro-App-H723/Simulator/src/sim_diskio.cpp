// FatFs disk driver on a raw SD card image (replaces FATFS/Target/sd_diskio.c).
//
// The firmware's own FatFs (ff.c, ffconf.h) runs unchanged on top of it, so
// long file names, the folder checks and file formats behave as on the device.
// The image is a byte-for-byte copy of a card: `dd` of the real card or one
// made by Simulator/tools/make_card_image.sh.

#include <fcntl.h>

#include <cstdio>
#include <mutex>
#include <string>

#include "fatfs.h"
#include "sim.h"

namespace sim {

static const uint32_t kSectorSize = 512;

static FILE* card = nullptr;
static uint32_t cardSectors = 0;
static std::mutex cardMutex;

bool cardOpen(const std::string& path) {
    std::lock_guard<std::mutex> lock(cardMutex);
    card = fopen(path.c_str(), "r+b");
    if (!card) return false;
    fcntl(fileno(card), F_SETFD, FD_CLOEXEC);
    fseeko(card, 0, SEEK_END);
    cardSectors = static_cast<uint32_t>(ftello(card) / kSectorSize);
    // SD detect is active low
    GPIOC->IDR &= ~static_cast<uint32_t>(SDMMC2_DETECT_Pin);
    return true;
}

bool cardInserted() { return card != nullptr; }

}  // namespace sim

using namespace sim;

static DSTATUS SD_initialize(BYTE lun) { return card ? 0 : STA_NOINIT; }

static DSTATUS SD_status(BYTE lun) { return card ? 0 : STA_NOINIT; }

static DRESULT SD_read(BYTE lun, BYTE* buff, DWORD sector, UINT count) {
    std::lock_guard<std::mutex> lock(cardMutex);
    if (!card) return RES_NOTRDY;
    if (fseeko(card, static_cast<off_t>(sector) * kSectorSize, SEEK_SET) != 0) return RES_ERROR;
    if (fread(buff, kSectorSize, count, card) != count) return RES_ERROR;
    return RES_OK;
}

static DRESULT SD_write(BYTE lun, const BYTE* buff, DWORD sector, UINT count) {
    std::lock_guard<std::mutex> lock(cardMutex);
    if (!card) return RES_NOTRDY;
    if (fseeko(card, static_cast<off_t>(sector) * kSectorSize, SEEK_SET) != 0) return RES_ERROR;
    if (fwrite(buff, kSectorSize, count, card) != count) return RES_ERROR;
    return RES_OK;
}

static DRESULT SD_ioctl(BYTE lun, BYTE cmd, void* buff) {
    std::lock_guard<std::mutex> lock(cardMutex);
    if (!card) return RES_NOTRDY;
    switch (cmd) {
        case CTRL_SYNC:
            fflush(card);
            return RES_OK;
        case GET_SECTOR_COUNT:
            *static_cast<DWORD*>(buff) = cardSectors;
            return RES_OK;
        case GET_SECTOR_SIZE:
            *static_cast<WORD*>(buff) = kSectorSize;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *static_cast<DWORD*>(buff) = 1;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}

extern "C" const Diskio_drvTypeDef SD_Driver = {
    SD_initialize, SD_status, SD_read, SD_write, SD_ioctl,
};
