#!/usr/bin/env python3
"""pe-link47 -- minimal PE linker for SHARP Brain WinCE guest binaries.

Links ONE ELF32-ARM object (clang -target armv5te-none-eabi -marm
-fshort-wchar, ARM state) into a fixed-base WinCE PE matching the stock
Brain app profile (machine 0x01C2 ARMV4T, subsystem CE GUI 9, imports
only COREDLL.dll by ordinal).

Derived from pe-link44 (p44) with these extensions:
  * --entry SYMBOL      entry-point symbol (default WinMain; DllMain for DLLs)
  * --dll               emit a DLL (IMAGE_FILE_DLL) instead of an EXE
  * --exports A,B,C     emit an export directory for the named symbols
  * --base 0xNNNN       preferred image base (default 0x10000)
  * real .data section  (initialised writable data, with relocations)
  * .reloc section      base relocations for every ABS32 fixup, so DLLs
                        load correctly at any address (device.exe loads
                        stream drivers into its own address space)
  * ordinal table       the hand-verified seed table below (checked
                        against this image's coredll; it covers every
                        import of braincon.c and p47term.c). If a
                        coredll-exports.json (ordinal -> name map) is
                        present at ../static/, it is merged over the
                        seed for symbols the seed does not know.

Usage: pe-link47.py OBJ OUT [--dll] [--entry SYM] [--exports A,B] [--base N]
Outputs: OUT, OUT.imports.json, OUT.map.json
"""
import struct, sys, json, os

# ---- seed ordinals (hand-verified in-guest by p20/p44) ----
ORDINALS = {
 'GetProcAddressA': 1230, 'GetProcAddressW': 530, 'VirtualAlloc': 524, 'VirtualFree': 525, 'VirtualCopy': 560,'CreateFileW': 168, 'WriteFile': 171, 'CloseHandle': 553,
            'GetModuleHandleW': 1177, 'RegisterClassW': 95, 'GetLastError': 516,
            'GetSystemMetrics': 885, 'CreateWindowExW': 246, 'ShowWindow': 266,
            'UpdateWindow': 267, 'GetMessageW': 861, 'TranslateMessage': 870,
            'DispatchMessageW': 859, 'PostQuitMessage': 866, 'DestroyWindow': 265,
            'DefWindowProcW': 264, 'BeginPaint': 260, 'GetClientRect': 249,
            'CreateSolidBrush': 931, 'FillRect': 935, 'DeleteObject': 912,
            'DrawTextW': 945, 'EndPaint': 261, 'FlushFileBuffers': 175, 'ReadFile': 170,
            'FindFirstFileW': 167, 'FindNextFileW': 181, 'FindClose': 180,
            'GetFileAttributesW': 166, 'CreateProcessW': 493, 'Sleep': 496,
            'GetTickCount': 535, 'SetFilePointer': 173, 'GetFileSize': 172,
            'DeleteFileW': 165, 'CopyFileW': 164, 'CreateDirectoryW': 160,
            'LoadLibraryW': 528, 'FreeLibrary': 529, 'MessageBoxW': 858,
            'WaitForSingleObject': 497, 'TerminateProcess': 544,
            'GetExitCodeProcess': 519, 'GetModuleFileNameW': 537,
            'DeviceIoControl': 179, 'GetCommandLineW': 1231,
            'OpenEventW': 1496, 'CreateEventW': 495,
            'RegOpenKeyExW': 461, 'RegQueryValueExW': 463, 'RegCloseKey': 455,
 'ActivateDeviceEx': 1494, 'AddFontResourceW': 893, 'CreateFontIndirectW': 895,
            'DeleteCriticalSection': 3, 'EnterCriticalSection': 4, 'EventModify': 494,
            'ExtTextOutW': 896, 'GetDC': 262, 'GetStdioPathW': 1149, 'GetTextFaceW': 967,
            'GetTextMetricsW': 898, 'GetWindowRect': 248, 'InitializeCriticalSection': 2,
            'InterlockedDecrement': 11, 'InterlockedIncrement': 10, 'InvalidateRect': 250,
            'KillTimer': 876, 'LeaveCriticalSection': 5, 'LocalAlloc': 33, 'LocalFree': 36,
            'MoveWindow': 272, 'MultiByteToWideChar': 196, 'OutputDebugStringW': 541,
            'RegCreateKeyExW': 456, 'RegSetValueExW': 464, 'RegisterDevice': 235,
            'ReleaseDC': 263, 'RemoveFontResourceW': 900, 'SelectObject': 921,
            'SetBkColor': 922, 'SetFocus': 704, 'SetForegroundWindow': 702,
            'SetLastError': 517, 'SetStdioPathW': 1150, 'SetTextColor': 924,
            'SetTimer': 875, 'WideCharToMultiByte': 197}

def main():
    args = sys.argv[1:]
    OBJ, OUT = args[0], args[1]
    DLL = '--dll' in args
    entry_sym = 'WinMain'
    export_names = []
    BASE = 0x10000
    i = 2
    while i < len(args):
        if args[i] == '--entry': entry_sym = args[i + 1]; i += 2
        elif args[i] == '--exports': export_names = args[i + 1].split(','); i += 2
        elif args[i] == '--base': BASE = int(args[i + 1], 0); i += 2
        else: i += 1
    # merge the extracted coredll ordinal map (authoritative for this image)
    ext = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       '..', 'static', 'coredll-exports.json')
    if os.path.exists(ext):
        for ords, name in json.load(open(ext)).items():
            ORDINALS.setdefault(name, int(ords))

    SECT_ALIGN = 0x1000
    FILE_ALIGN = 0x200

    d = open(OBJ, 'rb').read()
    assert d[:4] == b'\x7fELF' and d[4] == 1
    (e_shoff,) = struct.unpack_from('<I', d, 0x20)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', d, 0x2E)
    secs = []
    for i in range(e_shnum):
        name, typ, flags, addr, offset, size, link, info, align_, entsize = \
            struct.unpack_from('<IIIIIIIIII', d, e_shoff + i * e_shentsize)
        secs.append(dict(name=name, typ=typ, flags=flags, offset=offset, size=size,
                         link=link, info=info, data=d[offset:offset + size]))
    shstr = secs[e_shstrndx]['data']
    def shname(s):
        z = shstr.index(b'\0', s['name'])
        return shstr[s['name']:z].decode()
    for s in secs: s['sname'] = shname(s)
    symsec = next(s for s in secs if s['typ'] == 2)
    strsec = secs[symsec['link']]
    def strget(off):
        z = strsec['data'].index(b'\0', off)
        return strsec['data'][off:z].decode()
    syms = []
    for i in range(symsec['size'] // 16):
        name, value, size, info, other, shndx = struct.unpack_from('<IIIBBH', symsec['data'], i * 16)
        syms.append(dict(name=strget(name) if name else '', value=value,
                         bind=info >> 4, typ=info & 0xF, shndx=shndx))
    sym_by_name = {s['name']: s for s in syms if s['name']}

    def align(v, a): return (v + a - 1) // a * a

    # ---- collect content sections ----
    text_sec = next(s for s in secs if s['sname'] == '.text')
    ro_secs = [s for s in secs if s['sname'].startswith('.rodata')]
    data_sec = next((s for s in secs if s['sname'] == '.data' and s['typ'] == 1), None)
    bss_sec = next((s for s in secs if s['sname'] == '.bss'), None)

    imports = [s['name'] for s in syms if s['shndx'] == 0 and s['name']]
    imports = list(dict.fromkeys(imports))
    for f in imports:
        assert f in ORDINALS, f'no coredll ordinal known for import {f}'

    text_data = bytearray(text_sec['data'])
    thunk_size = len(imports) * 16

    # ---- RVA layout ----
    rva = SECT_ALIGN
    def place(size):
        nonlocal rva
        r = rva
        rva += align(max(size, 1), SECT_ALIGN)
        return r
    text_rva = place(len(text_data) + thunk_size)
    # rodata blob
    ro_off = {}
    blob = bytearray()
    for s in ro_secs:
        while len(blob) % 4: blob += b'\0'
        ro_off[secs.index(s)] = len(blob)
        blob += s['data']
    rodata_rva = place(len(blob)) if ro_secs else None
    # .data (initialised) followed virtually by .bss. The RVA span
    # reserved MUST cover data+bss, or later sections land inside the
    # .data virtual range and the loader maps them over each other.
    data_blob = bytearray(data_sec['data']) if data_sec else bytearray()
    bss_size = bss_sec['size'] if bss_sec else 0
    data_vsize = len(data_blob) + bss_size
    data_rva = place(data_vsize) if (data_sec or bss_sec) else None
    # .idata
    n = len(imports)
    idata_rva = rva
    desc_rva = idata_rva
    ilt_rva = desc_rva + 40
    iat_rva = ilt_rva + 4 * (n + 1)
    p = iat_rva + 4 * (n + 1)
    dllname_rva = p
    p += len(b'COREDLL.dll\0')
    if p % 2: p += 1
    idata_size = p - idata_rva
    rva += align(idata_size, SECT_ALIGN)
    # .edata (DLL exports)
    edata_rva = edata_size = 0
    exp_syms = []
    if export_names:
        for nm in export_names:
            assert nm in sym_by_name and sym_by_name[nm]['shndx'] != 0, f'export {nm} not defined'
            exp_syms.append(nm)
        ne = len(exp_syms)
        edata_rva = rva
        # dir(40) + EAT(4*ne) + ENPT(4*ne) + EOT(2*ne) + dll name + fn names
        q = edata_rva + 40 + 8 * ne + 2 * ne
        dllnm = os.path.basename(OUT).encode() + b'\0'
        q += len(dllnm)
        for nm in exp_syms: q += len(nm.encode()) + 1
        edata_size = q - edata_rva
        rva += align(edata_size, SECT_ALIGN)
    size_of_image = rva

    sec_rva = {secs.index(text_sec): text_rva}
    for idx, off in ro_off.items(): sec_rva[idx] = rodata_rva + off
    if data_sec is not None: sec_rva[secs.index(data_sec)] = data_rva
    if bss_sec is not None:
        sec_rva[secs.index(bss_sec)] = (data_rva + len(data_blob)) if data_rva else None

    # ---- symbol resolution ----
    thunk_va = {}
    tp = text_rva + len(text_data)
    if tp % 4: tp += 4 - (tp % 4)
    for f in imports:
        thunk_va[f] = tp
        tp += 16
    iat_va = {f: iat_rva + 4 * i for i, f in enumerate(imports)}

    def sym_va(s):
        if s['shndx'] == 0:
            return BASE + thunk_va[s['name']]
        if s['typ'] == 3:
            return BASE + sec_rva[s['shndx']]
        return BASE + sec_rva[s['shndx']] + s['value']

    # ---- relocations; record ABS32 sites for the .reloc section ----
    reloc_sites = []  # output RVAs of 32-bit absolute fixups
    def apply_relocs(sec_indices, buf, buf_rva, is_text):
        for s in secs:
            if s['typ'] != 9: continue
            tgt_idx = s['info']
            if tgt_idx not in sec_indices: continue
            base_off = 0 if is_text or tgt_idx not in ro_off else ro_off[tgt_idx]
            if not is_text and tgt_idx in ro_off: base_off = ro_off[tgt_idx]
            if not is_text and tgt_idx not in ro_off: base_off = 0
            for j in range(s['size'] // 8):
                off, inf = struct.unpack_from('<II', s['data'], j * 8)
                sym, typ = syms[inf >> 8], inf & 0xFF
                S = sym_va(sym)
                site_rva = buf_rva + base_off + off
                if typ == 2:  # R_ARM_ABS32
                    (a,) = struct.unpack_from('<I', buf, base_off + off)
                    struct.pack_into('<I', buf, base_off + off, (S + a) & 0xFFFFFFFF)
                    reloc_sites.append(site_rva)
                elif typ in (28, 29) and is_text:  # R_ARM_CALL / R_ARM_JUMP24
                    (w,) = struct.unpack_from('<I', buf, base_off + off)
                    imm = w & 0xFFFFFF
                    if imm & 0x800000: imm -= 1 << 24
                    A = (imm << 2) + 8
                    P = BASE + site_rva
                    newimm = ((S + A - P) - 8) >> 2
                    assert ((S + A - P) - 8) % 4 == 0 and -(1 << 23) <= newimm < (1 << 23), \
                        (sym['name'], hex(S), hex(P))
                    struct.pack_into('<I', buf, base_off + off,
                                     (w & 0xFF000000) | (newimm & 0xFFFFFF))
                else:
                    raise SystemExit(f'unhandled reloc type {typ} at {off:#x}')

    text_sec_idx = secs.index(text_sec)
    apply_relocs({text_sec_idx}, text_data, text_rva, True)
    if ro_secs:
        apply_relocs(set(ro_off.keys()), blob, rodata_rva, False)
    if data_sec is not None:
        apply_relocs({secs.index(data_sec)}, data_blob, data_rva, False)

    # thunks appended to .text. Each thunk's 4th word is an absolute
    # IAT address: it MUST be recorded as a base-relocation site, or a
    # DLL loaded away from its preferred base calls through garbage.
    if len(text_data) % 4:
        text_data += b'\0' * (4 - len(text_data) % 4)
    thunk_bytes = bytearray()
    for f in imports:
        reloc_sites.append(thunk_va[f] + 12)
        thunk_bytes += struct.pack('<IIII', 0xE59FC004, 0xE59CC000, 0xE12FFF1C, BASE + iat_va[f])
    pad_to = thunk_va[imports[0]] - text_rva if imports else len(text_data)
    text_full = bytes(text_data).ljust(pad_to, b'\0') + bytes(thunk_bytes)

    # ---- .idata ----
    idata = bytearray()
    idata += struct.pack('<IIIII', ilt_rva, 0, 0, dllname_rva, iat_rva)
    idata += b'\0' * 20
    ilt = b''.join(struct.pack('<I', 0x80000000 | ORDINALS[f]) for f in imports) + b'\0\0\0\0'
    idata += ilt
    idata += ilt
    idata += b'COREDLL.dll\0'
    if len(idata) % 2: idata += b'\0'
    assert len(idata) == idata_size

    # ---- .edata ----
    edata = bytearray()
    if export_names:
        ne = len(exp_syms)
        eat_rva = edata_rva + 40
        enpt_rva = eat_rva + 4 * ne
        eot_rva = enpt_rva + 4 * ne
        names_rva = eot_rva + 2 * ne
        dllnm = os.path.basename(OUT).encode() + b'\0'
        edata += struct.pack('<IIHHIIIIIII', 0, 0, 0, 0, names_rva, 1, ne, ne,
                             eat_rva, enpt_rva, eot_rva)
        name_ptrs = []
        cur = names_rva + len(dllnm)
        for nm in exp_syms:
            name_ptrs.append(cur)
            cur += len(nm.encode()) + 1
        for nm in exp_syms:
            s = sym_by_name[nm]
            edata += struct.pack('<I', sec_rva[s['shndx']] + s['value'])
        for ptr in name_ptrs: edata += struct.pack('<I', ptr)
        for k in range(ne): edata += struct.pack('<H', k)
        edata += dllnm
        for nm in exp_syms: edata += nm.encode() + b'\0'
        assert len(edata) == edata_size, (len(edata), edata_size)

    # ---- .reloc ----
    reloc = bytearray()
    reloc_rva = 0
    if reloc_sites:
        reloc_rva = rva
        sites = sorted(set(reloc_sites))
        pages = {}
        for s_ in sites: pages.setdefault(s_ & ~0xFFF, []).append(s_ & 0xFFF)
        for page, offs in sorted(pages.items()):
            entries = sorted(offs)
            pad = len(entries) % 2  # pad entry must be type 0 (ABSOLUTE)
            reloc += struct.pack('<II', page, 8 + 2 * (len(entries) + pad))
            for o_ in entries:
                reloc += struct.pack('<H', (3 << 12) | o_)  # HIGHLOW
            if pad:
                reloc += struct.pack('<H', 0)  # ABSOLUTE: no-op padding
        rva += align(len(reloc), SECT_ALIGN)
        size_of_image = rva

    # ---- headers ----
    esym = sym_by_name[entry_sym]
    entry_rva = sec_rva[esym['shndx']] + esym['value']
    assert entry_rva % 2 == 0
    out_secs = [(b'.text', text_rva, len(text_full), 0x60500020)]
    if ro_secs: out_secs.append((b'.rdata', rodata_rva, len(blob), 0x40300040))
    if data_rva is not None:
        out_secs.append((b'.data', data_rva, data_vsize, 0xC0300080))
    out_secs.append((b'.idata', idata_rva, idata_size, 0xC0300040))
    if export_names: out_secs.append((b'.edata', edata_rva, edata_size, 0x40300040))
    if reloc_rva: out_secs.append((b'.reloc', reloc_rva, len(reloc), 0x42000040))

    chars = 0x2103 if DLL else 0x103
    hdr = bytearray(0x400)
    hdr[0:2] = b'MZ'
    struct.pack_into('<I', hdr, 0x3C, 0x80)
    hdr[0x80:0x84] = b'PE\0\0'
    coff = struct.pack('<HHIIIHH', 0x01C2, len(out_secs), 0, 0, 0, 224, chars)
    hdr[0x84:0x84 + 20] = coff
    o = 0x84 + 20
    code_size = len(text_full)
    init_data = (len(blob) if ro_secs else 0) + idata_size + edata_size + len(data_blob)
    opt = struct.pack('<HBB' + 'I' * 9 + 'H' * 6 + 'I' * 4 + 'H' * 2 + 'I' * 6,
                      0x10B, 11, 1, code_size, init_data, data_vsize,
                      entry_rva, text_rva, rodata_rva or idata_rva, BASE, SECT_ALIGN, FILE_ALIGN,
                      4, 0, 0, 0, 6, 0, 0, size_of_image, 0x400, 0, 9, 0,
                      0x10000, 0x1000, 0x100000, 0x1000, 0, 16)
    assert len(opt) == 96, len(opt)
    hdr[o:o + 96] = opt
    dd = o + 96
    if export_names:
        struct.pack_into('<II', hdr, dd + 0, edata_rva, edata_size)   # export dir
    struct.pack_into('<II', hdr, dd + 8, idata_rva, 40)               # import dir
    if reloc_rva:
        struct.pack_into('<II', hdr, dd + 40, reloc_rva, len(reloc))  # base reloc dir
    sp = dd + 128
    file_off = 0x400
    rawmap = {text_rva: text_full, idata_rva: bytes(idata)}
    if ro_secs: rawmap[rodata_rva] = bytes(blob)
    if data_rva is not None:
        # BSS-only .data: emit real zero bytes like pe-link44 did -- a
        # zero raw size with a nonzero PointerToRawData that aliases the
        # next section's raw data confuses the CE loader.
        rawmap[data_rva] = bytes(data_blob) if data_blob else bytes(data_vsize)
    if export_names: rawmap[edata_rva] = bytes(edata)
    if reloc_rva: rawmap[reloc_rva] = bytes(reloc)
    sec_blobs = []
    for nm, vrva, vsz, ch in out_secs:
        raw = rawmap[vrva]
        rawsz = align(len(raw), FILE_ALIGN)
        hdr[sp:sp + 40] = struct.pack('<8sIIIIIIHHI', nm.ljust(8, b'\0'), vsz, vrva,
                                      rawsz, file_off, 0, 0, 0, 0, ch)
        sec_blobs.append((file_off, raw, rawsz))
        file_off += rawsz
        sp += 40
    out = bytearray(hdr)
    for foff, raw, rawsz in sec_blobs:
        assert len(out) <= foff
        out += b'\0' * (foff - len(out))
        out += raw
        out += b'\0' * (rawsz - len(raw))
    open(OUT, 'wb').write(bytes(out))
    json.dump({f: hex(BASE + thunk_va[f]) for f in imports}, open(OUT + '.imports.json', 'w'), indent=1)
    json.dump({'entry': hex(BASE + entry_rva), 'entry_sym': entry_sym,
               'base': hex(BASE), 'dll': DLL, 'exports': export_names,
               'text_rva': hex(text_rva), 'size_of_image': hex(size_of_image),
               'reloc_sites': len(set(reloc_sites))},
              open(OUT + '.map.json', 'w'), indent=1)
    print(f'wrote {OUT}: {len(out)} bytes, entry={hex(BASE + entry_rva)}, '
          f'imports={len(imports)}, exports={len(export_names)}, relocs={len(set(reloc_sites))}')

if __name__ == '__main__':
    main()
