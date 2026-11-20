#!/usr/bin/env python3
"""p48 build: clang brainterm.c -> ELF32-ARM object, check undefined symbols
against the runtime helpers + pe-link48 ORDINALS, then link BrainTerm.exe."""
import struct, subprocess, sys, os, json, re

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "brainterm.c")
OBJ = os.path.join(HERE, "brainterm.o")
OUT = os.path.join(HERE, "BrainTerm.exe")

RUNTIME = {"memset", "memcpy", "memmove", "__aeabi_memset", "__aeabi_memclr",
           "__aeabi_memcpy", "__aeabi_memmove", "__aeabi_uidiv", "__aeabi_idiv"}

def undef_syms(objpath):
    d = open(objpath, "rb").read()
    assert d[:4] == b"\x7fELF" and d[4] == 1
    (e_shoff,) = struct.unpack_from("<I", d, 0x20)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", d, 0x2E)
    secs = []
    for i in range(e_shnum):
        name, typ, flags, addr, offset, size, link, info, align, entsize = \
            struct.unpack_from("<IIIIIIIIII", d, e_shoff + i * e_shentsize)
        secs.append(dict(typ=typ, offset=offset, size=size, link=link,
                         data=d[offset:offset + size]))
    symsec = next(s for s in secs if s["typ"] == 2)
    strsec = secs[symsec["link"]]
    def strget(off):
        z = strsec["data"].index(b"\0", off)
        return strsec["data"][off:z].decode()
    undef, defined = [], set()
    for i in range(symsec["size"] // 16):
        name, value, size, info, other, shndx = struct.unpack_from(
            "<IIIBBH", symsec["data"], i * 16)
        nm = strget(name) if name else ""
        if not nm:
            continue
        if shndx == 0:
            undef.append(nm)
        else:
            defined.add(nm)
    return undef, defined

def main():
    r = subprocess.run(["clang", "-target", "armv5te-none-eabi", "-marm",
                        "-O2", "-ffreestanding", "-fno-builtin", "-fshort-wchar",
                        "-Wall", "-c", SRC, "-o", OBJ],
                       capture_output=True, text=True)
    print(r.stdout)
    print(r.stderr, file=sys.stderr)
    if r.returncode != 0:
        sys.exit("compile failed")
    undef, defined = undef_syms(OBJ)
    link_src = open(os.path.join(HERE, "pe-link48.py")).read()
    ords = set(re.findall(r"'([A-Za-z0-9_]+)': \d+", link_src))
    bad = [u for u in undef if u not in RUNTIME and u not in ords and u not in defined]
    print("undefined symbols:", sorted(set(undef)))
    if bad:
        sys.exit(f"FATAL: unresolved symbols with no ordinal/runtime: {bad}")
    r = subprocess.run([sys.executable, os.path.join(HERE, "pe-link48.py"), OBJ, OUT],
                       capture_output=True, text=True)
    print(r.stdout)
    print(r.stderr, file=sys.stderr)
    if r.returncode != 0:
        sys.exit("link failed")
    print("built", OUT, os.path.getsize(OUT), "bytes")

if __name__ == "__main__":
    main()
