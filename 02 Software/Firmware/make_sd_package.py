#!/usr/bin/env python3
"""Build or check a Drumboy Pro SD-card firmware package.

Package layout, as read by the factory bootloader (Drumboy-Pro-Boot-H723
Controller.cpp) and written by Drumboy-Pro-Firmware-Builder.cpp:

    "RW_DRUMBOY_PRO_FIRMWARE "   24 bytes
    versionMajor                 u16 LE, must be < 10
    versionMinor                 u16 LE, must be < 100
    payload size                 u32 LE, must equal file size - 36 and be < 917504
    payload CRC-32               u32 LE, zlib / ISO 3309, same as Python zlib.crc32
    payload                      raw application image for 0x08020000

The bootloader installs any file named Drumboy_Pro_Fw?*.bin from the SD card
root on power-up. It does not compare the version with the installed build.

    make_sd_package.py pack  build/Drumboy-Pro-App-H723.bin [-o DIR] [--major N --minor N]
    make_sd_package.py check Drumboy_Pro_Fw_01_11.bin

Without --major/--minor, pack reads kVersionMajor/kVersionMinor from
Drumboy-Pro-App-H723/Core/Library/Global/Global.h next to this script.
"""
import argparse
import pathlib
import re
import struct
import sys
import zlib

TITLE = b"RW_DRUMBOY_PRO_FIRMWARE "
HEADER_LEN = 36
MAX_PAYLOAD = 917504
GLOBAL_H = pathlib.Path(__file__).resolve().parent.parent / "Drumboy-Pro-App-H723" / "Core" / "Library" / "Global" / "Global.h"


def version_from_global_h(path=GLOBAL_H):
    text = path.read_text(errors="replace")
    found = {}
    for name in ("kVersionMajor", "kVersionMinor"):
        m = re.search(r"const\s+uint8_t\s+%s\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*;" % name, text)
        if not m:
            raise SystemExit(f"{path}: {name} not found")
        found[name] = int(m.group(1), 0)
    return found["kVersionMajor"], found["kVersionMinor"]


def parse(data):
    if len(data) < HEADER_LEN:
        raise ValueError("file shorter than the 36-byte header")
    if data[:24] != TITLE:
        raise ValueError("title mismatch: %r" % data[:24])
    major, minor, size, crc = struct.unpack("<HHII", data[24:36])
    return major, minor, size, crc, data[36:]


def check(path):
    data = pathlib.Path(path).read_bytes()
    major, minor, size, crc, payload = parse(data)
    problems = []
    if not (major < 10 and minor < 100):
        problems.append("version out of bootloader range (VERSION-CHECK)")
    if size != len(payload) or size == 0 or size >= MAX_PAYLOAD:
        problems.append(f"size field {size} vs payload {len(payload)} (SIZE-CHECK)")
    actual = zlib.crc32(payload) & 0xFFFFFFFF
    if actual != crc:
        problems.append(f"crc field {crc:08x} vs payload {actual:08x} (CRC-CHECK)")
    print(f"{path}: V{major}.{minor:02d}, payload {len(payload)} bytes, crc32 {crc:08x}")
    for p in problems:
        print("  FAIL:", p)
    return not problems


def pack(source, out_dir, major, minor):
    payload = pathlib.Path(source).read_bytes()
    if not (0 <= major < 10 and 0 <= minor < 100):
        raise SystemExit("bootloader requires major < 10 and minor < 100")
    if not (0 < len(payload) < MAX_PAYLOAD):
        raise SystemExit(f"payload {len(payload)} bytes outside 1..{MAX_PAYLOAD - 1}")
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    target = pathlib.Path(out_dir) / f"Drumboy_Pro_Fw_{major:02d}_{minor:02d}.bin"
    target.write_bytes(TITLE + struct.pack("<HHII", major, minor, len(payload), crc) + payload)
    print(f"{target}: V{major}.{minor:02d}, payload {len(payload)} bytes, crc32 {crc:08x}")
    return target


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("pack", help="wrap a raw application image")
    p.add_argument("source")
    p.add_argument("-o", "--out-dir", default=".")
    p.add_argument("--major", type=int)
    p.add_argument("--minor", type=int)
    c = sub.add_parser("check", help="validate a package the way the bootloader does")
    c.add_argument("package", nargs="+")
    args = ap.parse_args(argv)
    if args.cmd == "pack":
        major, minor = args.major, args.minor
        if major is None or minor is None:
            g_major, g_minor = version_from_global_h()
            major = g_major if major is None else major
            minor = g_minor if minor is None else minor
        target = pack(args.source, args.out_dir, major, minor)
        return 0 if check(target) else 1
    ok = all(check(pkg) for pkg in args.package)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
