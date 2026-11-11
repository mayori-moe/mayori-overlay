#!/usr/bin/env python3
"""Rewrite R_X86_64_IRELATIVE relocations as R_X86_64_RELATIVE.

musl 1.2.6 does not implement R_X86_64_IRELATIVE (type 37).  NVIDIA's
libnvidia-glcore has exactly one such relocation, an IFUNC resolver that
returns either a baseline or an AVX implementation.  Pointing it at the
baseline implementation is functionally correct on every x86_64 CPU.

Usage: patch-irelative.py ELF [TARGET_VADDR]
"""
import struct
import sys


def main() -> int:
    if len(sys.argv) not in (2, 3):
        print(__doc__, file=sys.stderr)
        return 2

    path = sys.argv[1]
    target = int(sys.argv[2], 0) if len(sys.argv) == 3 else 0xB8C5B0

    with open(path, "rb") as f:
        data = bytearray(f.read())

    if data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
        print(f"{path}: not a little-endian ELF64 file", file=sys.stderr)
        return 1

    e_shoff = struct.unpack_from("<Q", data, 0x28)[0]
    e_shentsize = struct.unpack_from("<H", data, 0x3A)[0]
    e_shnum = struct.unpack_from("<H", data, 0x3C)[0]

    patched = 0
    for i in range(e_shnum):
        sh = e_shoff + i * e_shentsize
        sh_type = struct.unpack_from("<I", data, sh + 4)[0]
        if sh_type != 4:  # SHT_RELA
            continue
        sh_offset = struct.unpack_from("<Q", data, sh + 0x18)[0]
        sh_size = struct.unpack_from("<Q", data, sh + 0x20)[0]
        sh_entsize = struct.unpack_from("<Q", data, sh + 0x38)[0]
        if sh_entsize != 24:
            continue

        for j in range(0, sh_size, 24):
            p = sh_offset + j
            r_offset, r_info, _r_addend = struct.unpack_from("<QQq", data, p)
            if (r_info & 0xFFFFFFFF) != 37:  # R_X86_64_IRELATIVE
                continue
            # Keep the symbol index, replace type with R_X86_64_RELATIVE.
            struct.pack_into("<Q", data, p + 8, (r_info & 0xFFFFFFFF00000000) | 8)
            struct.pack_into("<q", data, p + 16, target)
            patched += 1
            print(f"patch {path}: IRELATIVE at {r_offset:#x} -> "
                  f"RELATIVE {target:#x}")

    if not patched:
        print(f"{path}: no R_X86_64_IRELATIVE relocation found", file=sys.stderr)
        return 1

    with open(path, "wb") as f:
        f.write(data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
