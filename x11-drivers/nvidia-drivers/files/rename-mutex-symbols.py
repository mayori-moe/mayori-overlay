#!/usr/bin/env python3
"""Rename glibc pthread mutex entry points to shim-private same-length names.

musl's loader adds its own libc to the global symbol table before any
DT_NEEDED dependency, so a NEEDED shim cannot interpose symbols that musl
already exports.  Rewriting the three mutex entry points in .dynstr makes
the prebuilt NVIDIA ELFs call names that only the shim provides.

Only the .dynstr section is touched; the replacement names have exactly
the same length, so no offsets change.

Usage: rename-mutex-symbols.py ELF [ELF...]
"""
import struct
import sys

RENAMES = {
    b"pthread_mutex_lock": b"nvidia_mutex_lock_",
    b"pthread_mutex_trylock": b"nvidia_mutex_trylock_",
    b"pthread_mutex_timedlock": b"nvidia_mutex_timedlock_",
}

for _old, _new in RENAMES.items():
    assert len(_old) == len(_new)


def dynstr_range(data: bytearray):
    if data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
        return None
    e_shoff = struct.unpack_from("<Q", data, 0x28)[0]
    e_shentsize = struct.unpack_from("<H", data, 0x3A)[0]
    e_shnum = struct.unpack_from("<H", data, 0x3C)[0]
    e_shstrndx = struct.unpack_from("<H", data, 0x3E)[0]

    shstr = e_shoff + e_shstrndx * e_shentsize
    shstr_off = struct.unpack_from("<Q", data, shstr + 0x18)[0]

    for i in range(e_shnum):
        sh = e_shoff + i * e_shentsize
        name_off = struct.unpack_from("<I", data, sh)[0]
        start = shstr_off + name_off
        end = data.index(b"\0", start)
        if data[start:end] != b".dynstr":
            continue
        off = struct.unpack_from("<Q", data, sh + 0x18)[0]
        size = struct.unpack_from("<Q", data, sh + 0x20)[0]
        return off, size
    return None


def patch(path: str) -> int:
    with open(path, "rb") as f:
        data = bytearray(f.read())

    rng = dynstr_range(data)
    if not rng:
        return 0
    off, size = rng
    table = bytes(data[off:off + size])

    changed = 0
    for old, new in RENAMES.items():
        needle = b"\0" + old + b"\0"
        replacement = b"\0" + new + b"\0"
        count = table.count(needle)
        if count:
            table = table.replace(needle, replacement)
            changed += count

    if changed:
        data[off:off + size] = table
        with open(path, "wb") as f:
            f.write(data)
        print(f"{path}: renamed {changed} mutex symbol(s)")
    return changed


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    for path in sys.argv[1:]:
        patch(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
