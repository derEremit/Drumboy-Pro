#!/bin/bash
# Creates a raw SD card image (MBR + FAT32, volume DRUMBOY) for the simulator
# and copies a folder onto it, e.g. the SD card backup.
#
#   make_card_image.sh <source folder> <image file> [size, default 1g]
#
# The image is sparse, so it only takes the space of the copied files.
# macOS metadata (._ files, .fseventsd, Spotlight) is kept off the card.
# On Linux the image is a FAT32 volume without a partition table (FatFs mounts
# both), made with mkfs.vfat and filled with mtools, so no root is needed.

set -euo pipefail

if [ $# -lt 2 ]; then
    echo "Usage: $0 <source folder> <image file> [size, default 1g]" >&2
    exit 2
fi
SOURCE="$1"
IMAGE="$2"
SIZE="${3:-1g}"

if [ ! -d "$SOURCE" ]; then
    echo "Source folder not found: $SOURCE" >&2
    exit 1
fi
if [ -e "$IMAGE" ]; then
    echo "Image already exists: $IMAGE" >&2
    exit 1
fi

mkdir -p "$(dirname "$IMAGE")"

if [ "$(uname -s)" != "Darwin" ]; then
    for tool in mkfs.vfat mcopy; do
        command -v "$tool" >/dev/null || { echo "$tool not found (install dosfstools and mtools)" >&2; exit 1; }
    done
    truncate -s "${SIZE^^}" "$IMAGE"                              # Sparse; mkfs.vfat wants the size in upper case units
    mkfs.vfat -F 32 -n DRUMBOY "$IMAGE" >/dev/null
    # mtools copies directories recursively; skip macOS metadata files
    export MTOOLS_SKIP_CHECK=1
    for entry in "$SOURCE"/* "$SOURCE"/.[!.]*; do
        [ -e "$entry" ] || continue
        case "$(basename "$entry")" in ._*|.fseventsd|.Spotlight-V100|.Trashes|.DS_Store) continue ;; esac
        mcopy -i "$IMAGE" -s -Q "$entry" ::/
    done
    # The firmware refuses to start without System/Firmware (the bootloader's
    # package archive). Git keeps no empty folders, so the repo's card lacks it.
    mdir -i "$IMAGE" ::/System/Firmware >/dev/null 2>&1 || mmd -i "$IMAGE" ::/System/Firmware
    echo "Created $IMAGE from $SOURCE"
    exit 0
fi

mkfile -n "$SIZE" "$IMAGE"
DISK=$(hdiutil attach -nomount -imagekey diskimage-class=CRawDiskImage "$IMAGE" | head -1 | awk '{print $1}')
cleanup() { hdiutil detach "$DISK" -quiet || true; }
trap cleanup EXIT

diskutil eraseDisk FAT32 DRUMBOY MBRFormat "$DISK" >/dev/null
MOUNT=$(diskutil info "${DISK}s1" | awk -F': *' '/Mount Point/ {print $2}')
if [ -z "$MOUNT" ]; then
    echo "Card image did not mount" >&2
    exit 1
fi

mdutil -i off "$MOUNT" >/dev/null 2>&1 || true
touch "$MOUNT/.metadata_never_index"
cp -RX "$SOURCE"/ "$MOUNT"/
find "$MOUNT" -path "$MOUNT/.Spotlight-V100" -prune -o -name '._*' -print0 | xargs -0 rm -f
rm -rf "$MOUNT/.fseventsd" "$MOUNT/.Spotlight-V100" "$MOUNT/.Trashes" "$MOUNT/.metadata_never_index" 2>/dev/null || true
mkdir -p "$MOUNT/System/Firmware"                               # Required at boot; git keeps no empty folders

diskutil unmount "$MOUNT" >/dev/null
echo "Created $IMAGE from $SOURCE"
