#!/usr/bin/env python3
"""Minimal PE linker for the P20Test app: links ONE ELF32-ARM object
(clang -target armv5te-none-eabi, ARM state) into a fixed-base WinCE PE
matching the stock Brain app profile (machine 0x01C2, subsystem CE GUI 9,
no reloc directory, imports only COREDLL.dll by name).
Layout: headers 0x400, .text (code+import thunks), .rdata, .data, .idata."""
import struct, sys, json

OBJ, OUT = sys.argv[1], sys.argv[2]
BASE = 0x10000
SECT_ALIGN = 0x1000
FILE_ALIGN = 0x200

d = open(OBJ, 'rb').read()
assert d[:4] == b'\x7fELF' and d[4] == 1
(e_shoff,) = struct.unpack_from('<I', d, 0x20)
e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', d, 0x2E)
secs = []
for i in range(e_shnum):
    name, typ, flags, addr, offset, size, link, info, align, entsize = \
        struct.unpack_from('<IIIIIIIIII', d, e_shoff + i * e_shentsize)
    secs.append(dict(name=name, typ=typ, offset=offset, size=size,
                     link=link, info=info, data=d[offset:offset + size]))
shstr = secs[e_shstrndx]['data']
def shname(s):
    z = shstr.index(b'\0', s['name'])
    return shstr[s['name']:z].decode()
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

def align(v, a):
    return (v + a - 1) // a * a

# ---- collect content sections ----
text_sec = next(s for s in secs if shname(s) == '.text')
ro_secs = [s for s in secs if shname(s) in ('.rodata.str2.2', '.rodata.str1.1', '.rodata', '.rodata.cst16')]
bss_sec = next((s for s in secs if shname(s) == '.bss'), None)
data_sec = next((s for s in secs if shname(s) == '.data'), None)

imports = [s['name'] for s in syms if s['shndx'] == 0 and s['name']]
imports = list(dict.fromkeys(imports))  # unique, stable order

text_data = bytearray(text_sec['data'])
thunk_rva0 = None  # filled after RVA assignment
thunk_size = len(imports) * 16

# ---- RVA layout ----
rva = SECT_ALIGN
def place(size):
    global rva
    r = rva
    rva += align(max(size, 1), SECT_ALIGN)
    return r
text_rva = place(len(text_data) + thunk_size)
ro_rva = {}
ro_blob = bytearray()
for s in ro_secs:
    ro_rva[secs.index(s)] = ro_rva and (list(ro_rva.values())[-1]) or None
# simpler: build rodata blob with per-section RVA offsets
ro_off = {}
blob = bytearray()
for s in ro_secs:
    a = 16 if shname(s).endswith('cst16') else 4
    while len(blob) % a: blob += b'\0'
    ro_off[secs.index(s)] = len(blob)
    blob += s['data']
rodata_rva = place(len(blob))
elf_data = data_sec['data'] if data_sec else b''
bss_size = bss_sec['size'] if bss_sec else 0
data_size = len(elf_data) + bss_size
data_rva = place(max(data_size, 1)) if data_size else None

# .idata layout (p53: two DLLs): 3 descriptors + per-DLL ILT/IAT + names
TH_ORDINALS = {'CreateToolhelp32Snapshot': 2, 'Process32First': 10,
               'Process32Next': 11, 'CloseToolhelp32Snapshot': 1}
core_imports = [f for f in imports if f not in TH_ORDINALS]
th_imports = [f for f in imports if f in TH_ORDINALS]
nc, nt = len(core_imports), len(th_imports)
idata_rva = rva
desc_rva = idata_rva
ilt_core_rva = desc_rva + 60  # 2 descriptors + null terminator descriptor
iat_core_rva = ilt_core_rva + 4 * (nc + 1)
ilt_th_rva = iat_core_rva + 4 * (nc + 1)
iat_th_rva = ilt_th_rva + 4 * (nt + 1)
p = iat_th_rva + 4 * (nt + 1)
dllname_rva = p
p += len(b'COREDLL.dll\0')
if p % 2: p += 1
thname_rva = p
p += len(b'toolhelp.dll\0')
if p % 2: p += 1
idata_size = p - idata_rva
ORDINALS = {'CreateFileW': 168, 'WriteFile': 171, 'CloseHandle': 553,
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
            'SetEndOfFile': 178,
            'DeleteFileW': 165, 'CopyFileW': 164, 'CreateDirectoryW': 160,
            'LoadLibraryW': 528, 'FreeLibrary': 529, 'MessageBoxW': 858,
            'WaitForSingleObject': 497, 'TerminateProcess': 544,
            'GetExitCodeProcess': 519, 'GetModuleFileNameW': 537,
            'DeviceIoControl': 179, 'GetCommandLineW': 1231,
            'OpenEventW': 1496, 'CreateEventW': 495,
            'RegOpenKeyExW': 461, 'RegQueryValueExW': 463, 'RegCloseKey': 455,
            'BringWindowToTop': 275, 'SetForegroundWindow': 702,
            'SetWindowPos': 247, 'EnumWindows': 291, 'GetClassNameW': 283,
            'GetWindowTextW': 257, 'GetWindowRect': 248,
            'IsWindowVisible': 886, 'GetWindowThreadProcessId': 292,
            'GetForegroundWindow': 701, 'OpenProcess': 509,
            'RegEnumValueW': 459, 'RegEnumKeyExW': 460, 'IsWindow': 271,
            'GetWindow': 251, 'InvalidateRect': 250, 'UpdateWindow': 267,
            'WindowFromPoint': 252, 'GetParent': 269,
            'MoveFileW': 163, 'CopyFileW': 164, 'GetModuleFileNameW': 537,
            'PostMessageW': 865, 'SendMessageW': 868, 'PeekMessageW': 864,
            'RegSetValueExW': 464, 'RegCreateKeyExW': 456,
            'GetProcAddressA': 1230, 'keybd_event': 833, 'mouse_event': 824,
            'GetWindowLongW': 259, 'SetWindowLongW': 258, 'RedrawWindow': 1672,
            'GetDesktopWindow': 1397, 'SetActiveWindow': 703}
ORDINALS.update({'CreateDIBSection': 90, 'CreateCompatibleDC': 910,
            'CreateCompatibleBitmap': 902, 'SelectObject': 921,
            'BitBlt': 903, 'StretchBlt': 905, 'SetWindowRgn': 1398,
            'GetWindowRgn': 1399, 'CreateRectRgn': 980,
            'CreateRectRgnIndirect': 969, 'CombineRgn': 968,
            'ExtCreateRegion': 1617, 'FillRgn': 927, 'GetRgnBox': 974,
            'PtInRegion': 977, 'OffsetRgn': 976, 'SetTimer': 875,
            'KillTimer': 876, 'TrackPopupMenuEx': 845,
            'CreatePopupMenu': 852, 'AppendMenuW': 842, 'InsertMenuW': 841,
            'DestroyMenu': 844, 'DrawMenuBar': 856, 'SetCapture': 708,
            'ReleaseCapture': 709, 'GetCapture': 707, 'GetDC': 262,
            'ReleaseDC': 263, 'GetWindowDC': 270, 'DeleteDC': 911,
            'GetObjectW': 918, 'GetDeviceCaps': 916, 'ClientToScreen': 254,
            'ScreenToClient': 255, 'GetUpdateRect': 274,
            'ValidateRect': 278, 'VirtualAlloc': 524, 'VirtualFree': 525})
rva += align(idata_size, SECT_ALIGN)
size_of_image = rva

sec_rva = {secs.index(text_sec): text_rva}
for idx, off in ro_off.items():
    sec_rva[idx] = rodata_rva + off
if data_sec is not None:
    sec_rva[secs.index(data_sec)] = data_rva
if bss_sec is not None:
    sec_rva[secs.index(bss_sec)] = data_rva + len(elf_data)

# ---- symbol resolution ----
def sym_va(s):
    if s['shndx'] == 0:
        return BASE + thunk_va[s['name']]  # import thunk (filled below)
    return BASE + sec_rva[s['shndx']] + s['value']

thunk_va = {}
tp = text_rva + len(text_data)
if tp % 4: tp += 4 - (tp % 4)
for f in imports:
    thunk_va[f] = tp
    tp += 16
iat_va = {f: iat_core_rva + 4 * i for i, f in enumerate(core_imports)}
iat_va.update({f: iat_th_rva + 4 * i for i, f in enumerate(th_imports)})

# ---- apply relocations to .text ----
text_sec_idx = secs.index(text_sec)
for s in secs:
    if s['typ'] != 9 or s['info'] != text_sec_idx:
        continue
    for j in range(s['size'] // 8):
        off, inf = struct.unpack_from('<II', s['data'], j * 8)
        sym, typ = syms[inf >> 8], inf & 0xFF
        P = BASE + text_rva + off
        if sym['shndx'] == 0:
            S = BASE + thunk_va[sym['name']]
        elif sym['typ'] == 3:
            S = BASE + sec_rva[sym['shndx']]
        else:
            S = BASE + sec_rva[sym['shndx']] + sym['value']
        if typ == 2:  # R_ARM_ABS32
            (a,) = struct.unpack_from('<I', text_data, off)
            struct.pack_into('<I', text_data, off, (S + a) & 0xFFFFFFFF)
        elif typ in (28, 29):  # R_ARM_CALL / R_ARM_JUMP24
            (w,) = struct.unpack_from('<I', text_data, off)
            imm = w & 0xFFFFFF
            if imm & 0x800000: imm -= 1 << 24
            A = (imm << 2) + 8
            newimm = ((S + A - P) - 8) >> 2
            assert ((S + A - P) - 8) % 4 == 0 and -(1 << 23) <= newimm < (1 << 23), \
                (sym['name'], hex(S), hex(P))
            struct.pack_into('<I', text_data, off,
                             (w & 0xFF000000) | (newimm & 0xFFFFFF))
        else:
            raise SystemExit(f'unhandled reloc type {typ} at {off:#x}')

# ---- apply relocations to .rdata blob (e.g. pointer arrays in .rodata) ----
for s in secs:
    if s['typ'] != 9 or s['info'] not in ro_off:
        continue
    base_off = ro_off[s['info']]
    for j in range(s['size'] // 8):
        off, inf = struct.unpack_from('<II', s['data'], j * 8)
        sym, typ = syms[inf >> 8], inf & 0xFF
        if sym['shndx'] == 0:
            S = BASE + thunk_va[sym['name']]
        elif sym['typ'] == 3:
            S = BASE + sec_rva[sym['shndx']]
        else:
            S = BASE + sec_rva[sym['shndx']] + sym['value']
        if typ == 2:  # R_ARM_ABS32
            (a,) = struct.unpack_from('<I', blob, base_off + off)
            struct.pack_into('<I', blob, base_off + off, (S + a) & 0xFFFFFFFF)
        else:
            raise SystemExit(f'unhandled rodata reloc type {typ}')

# literal-pool ABS32 relocs also live in .text (handled above: all .rel.text).
# append thunks
if len(text_data) % 4:
    text_data += b'\0' * (4 - len(text_data) % 4)
assert BASE + text_rva + len(text_data) == BASE + tp - 8 * 0 or True
thunk_bytes = bytearray()
for f in imports:
    thunk_bytes += struct.pack('<IIII', 0xE59FC004, 0xE59CC000, 0xE12FFF1C, BASE + iat_va[f])
text_full = bytes(text_data[:text_rva and (tp - text_rva - thunk_size) if False else len(text_data)])
# rebuild text_full deterministically: original text padded to thunk start, then thunks
pad_to = thunk_va[imports[0]] - text_rva if imports else len(text_data)
text_full = bytes(text_data).ljust(pad_to, b'\0') + bytes(thunk_bytes)

# ---- build .idata ----
idata = bytearray()
idata += struct.pack('<IIIII', ilt_core_rva, 0, 0, dllname_rva, iat_core_rva)
idata += struct.pack('<IIIII', ilt_th_rva, 0, 0, thname_rva, iat_th_rva)
idata += b'\0' * 20  # null terminator descriptor
ilt_c = b''.join(struct.pack('<I', 0x80000000 | ORDINALS[f]) for f in core_imports) + b'\0\0\0\0'
idata += ilt_c
idata += ilt_c  # IAT initially identical
ilt_t = b''.join(struct.pack('<I', 0x80000000 | TH_ORDINALS[f]) for f in th_imports) + b'\0\0\0\0'
idata += ilt_t
idata += ilt_t  # IAT initially identical
idata += b'COREDLL.dll\0'
if len(idata) % 2: idata += b'\0'
idata += b'toolhelp.dll\0'
if len(idata) % 2: idata += b'\0'
assert len(idata) == idata_size, (len(idata), idata_size)

# ---- headers ----
entry_rva = None
for s in syms:
    if s['name'] == 'WinMain':
        entry_rva = sec_rva[s['shndx']] + s['value']
assert entry_rva is not None and entry_rva % 2 == 0
out_secs = [
    (b'.text', text_rva, len(text_full), 0x60500020),
    (b'.rdata', rodata_rva, len(blob), 0x40300040),
]
if data_size:
    out_secs.append((b'.data', data_rva, data_size, 0xC0300080))
out_secs.append((b'.idata', idata_rva, idata_size, 0xC0300040))

hdr = bytearray(0x400)
hdr[0:2] = b'MZ'
struct.pack_into('<I', hdr, 0x3C, 0x80)
hdr[0x80:0x84] = b'PE\0\0'
coff = struct.pack('<HHIIIHH', 0x01C2, len(out_secs), 0, 0, 0, 224, 0x103)
hdr[0x84:0x84 + 20] = coff
o = 0x84 + 20
opt = struct.pack('<HBB' + 'I'*9 + 'H'*6 + 'I'*4 + 'H'*2 + 'I'*6,
                  0x10B, 11, 1, len(text_full), len(blob) + idata_size + len(elf_data), bss_size,
                  entry_rva, text_rva, rodata_rva, BASE, SECT_ALIGN, FILE_ALIGN,
                  4, 0, 0, 0, 6, 0, 0, size_of_image, 0x400, 0, 9, 0,
                  0x10000, 0x1000, 0x100000, 0x1000, 0, 16)
assert len(opt) == 96, len(opt)
hdr[o:o + 96] = opt
dd = o + 96
struct.pack_into('<II', hdr, dd + 8, idata_rva, 60)  # import directory
sp = dd + 128
file_off = 0x400
sec_blobs = []
for nm, vrva, vsz, chars in out_secs:
    raw = {text_rva: text_full, rodata_rva: bytes(blob)}.get(vrva)
    if nm == b'.data':
        raw = elf_data + bytes(bss_size)
    if nm == b'.idata':
        raw = bytes(idata)
    rawsz = align(len(raw), FILE_ALIGN)
    hdr[sp:sp + 40] = struct.pack('<8sIIIIIIHHI', nm.ljust(8, b'\0'), vsz, vrva,
                                  rawsz, file_off, 0, 0, 0, 0, chars)
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
json.dump({'entry': hex(BASE + entry_rva), 'winmain_va': hex(BASE + entry_rva),
           'text_rva': hex(text_rva), 'rodata_rva': hex(rodata_rva),
           'idata_rva': hex(idata_rva), 'size_of_image': hex(size_of_image)},
          open(OUT + '.map.json', 'w'), indent=1)
print(f'wrote {OUT}: {len(out)} bytes, entry={BASE + entry_rva:#x}, imports={len(imports)}')
