#!/usr/bin/env python3
"""Wrap an application binary into a Drumboy Pro SD-card update package.

Same format as Firmware/Drumboy-Pro-Firmware-Builder.cpp and as checked by the
bootloader (Drumboy-Pro-Boot-H723, Controller.cpp):

    24 bytes  "RW_DRUMBOY_PRO_FIRMWARE "
     2 bytes  version major (uint16 LE, < 10)
     2 bytes  version minor (uint16 LE, < 100)
     4 bytes  payload size  (uint32 LE, < 917504)
     4 bytes  CRC-32 of payload (uint32 LE, zlib/IEEE polynomial)
     N bytes  application binary

Usage:
    package_firmware.py app.bin Drumboy_Pro_Fw_01_03.bin --version 1.03
    package_firmware.py --check Drumboy_Pro_Fw_01_03.bin
"""

import argparse
import os
import struct
import sys
import zlib

MAGIC = b"RW_DRUMBOY_PRO_FIRMWARE "
HEADER = struct.Struct("<HHII")
MAX_SIZE = 917504          # bootloader limit (sectors 1-7)
MAX_NAME = 28              # the bootloader (FatFs _MAX_LFN 32) should accept 31, but a 31-character name was ignored on the owner's device (2026-11-14): stay at 28
NAME_PREFIX = "Drumboy_Pro_Fw"   # bootloader looks for Drumboy_Pro_Fw?*.bin in the card's root
APP_BASE = 0x08020000      # application flash start


def parse_version(text):
    major, _, minor = text.partition(".")
    major, minor = int(major), int(minor or 0)
    if not (0 <= major < 10 and 0 <= minor < 100):
        raise ValueError("version must be <major 0-9>.<minor 0-99>")
    return major, minor


def check_payload(payload):
    if not 0 < len(payload) < MAX_SIZE:
        raise ValueError(f"payload size {len(payload)} outside 1..{MAX_SIZE - 1}")
    if len(payload) % 4:
        raise ValueError("payload size must be a multiple of 4 (bootloader writes 32-bit words)")
    sp, reset = struct.unpack_from("<II", payload)
    if not (APP_BASE <= reset < APP_BASE + len(payload)) or not reset & 1:
        raise ValueError(f"reset vector 0x{reset:08x} is not a Thumb address inside the application")


def check_name(path):
    name = os.path.basename(path)
    if not (name.startswith(NAME_PREFIX) and len(name) > len(NAME_PREFIX) and name.lower().endswith(".bin")):
        raise ValueError(f"{name}: the bootloader only flashes files named {NAME_PREFIX}?*.bin")
    if len(name) > MAX_NAME:
        raise ValueError(f"{name}: {len(name)} characters; the bootloader ignores names longer than {MAX_NAME}")


def check(path):
    check_name(path)
    data = open(path, "rb").read()
    if data[:24] != MAGIC:
        raise ValueError("bad magic")
    major, minor, size, crc = HEADER.unpack_from(data, 24)
    payload = data[36:]
    if size != len(payload):
        raise ValueError(f"header size {size} != payload {len(payload)}")
    if zlib.crc32(payload) != crc:
        raise ValueError("CRC mismatch")
    check_payload(payload)
    print(f"{path}: OK  V{major}.{minor:02d}  {size} bytes  crc {crc:08x}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input")
    ap.add_argument("output", nargs="?")
    ap.add_argument("--version")
    ap.add_argument("--check", action="store_true", help="validate an existing package")
    args = ap.parse_args()

    try:
        if args.check:
            check(args.input)
            return
        if not (args.output and args.version):
            ap.error("output and --version are required when packaging")
        check_name(args.output)
        major, minor = parse_version(args.version)
        payload = open(args.input, "rb").read()
        check_payload(payload)
        with open(args.output, "wb") as f:
            f.write(MAGIC + HEADER.pack(major, minor, len(payload), zlib.crc32(payload)) + payload)
        check(args.output)
    except ValueError as e:
        sys.exit(f"error: {e}")


if __name__ == "__main__":
    main()
