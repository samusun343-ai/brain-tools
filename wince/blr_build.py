#!/usr/bin/env python3
"""Synthesise AppMain.blr for a given AppMain.exe, per wceprj gate func 0x12fe5c.

Gate recap (all offsets verified in the disassembly):
  inputs from exe: size, head_sum = sum16(exe[0:min(size,0x1000)]),
    entry_off (via PE section mapping of AddressOfEntryPoint),
    sb = sum32(exe[entry_off-0x200 : entry_off+0x200])  (zero-padded buffer),
    r8 = sum32(exe[pdata_raw : pdata_raw+0x400])        (pdata_raw=0 if no .pdata)
  blr checks (0x400-byte file):
    [0x3FE]=0xAA [0x3FF]=0x55
    u16LE[0x3FA] = sum16(blr) ; u16LE[0x3FC] = 0xFFFF - sum16(blr)
    u16LE[0x000] = (size ^ head_sum) & 0xFFFF
    interleave at 0x100: even bytes = size BIG-ENDIAN ([0x100]=size>>24, [0x102]=size>>16,
      [0x104]=size>>8, [0x106]=size&0xFF); odd bytes hold v=(head_sum^(size>>1))&0xFFFF
      [0x101]=v.lo [0x105]=v.hi [0x103]=(0xFFFF-v).lo [0x107]=(0xFFFF-v).hi
    [0x200:0x20B] = "APPMAIN.EXE", zero pad to 0x210
    echoes of base bytes [0x10..0x13]: [0x3C]=b0+0x1F, [0x6B]=b1+0x33, [0x3F0]=b2+0x47, [0x3F1]=b3+0x5B (mod 256)
    scattered sb (BIG-ENDIAN): [0x249]=sb>>24 [0x256]=sb>>16 [0x258]=sb>>8 [0x265]=sb&0xFF
    scattered r8 (BIG-ENDIAN): [0x295]=r8>>24 [0x2A2]=r8>>16 [0x2B1]=r8>>8 [0x2B4]=r8&0xFF
    sum pair self-consistency: pair bytes always sum to 0x1FE, so
      S = (sum16(all other bytes) + 0x1FE) & 0xFFFF
"""
import struct, sys

def gate_inputs(exe: bytes):
    size = len(exe)
    head_sum = sum(exe[:min(size, 0x1000)]) & 0xFFFF
    assert exe[:2] == b'MZ'
    lf = struct.unpack_from('<I', exe, 0x3C)[0]
    assert exe[lf:lf+4] == b'PE\0\0'
    nsec = struct.unpack_from('<H', exe, lf+6)[0]
    optsz = struct.unpack_from('<H', exe, lf+20)[0]
    opt = lf + 24
    assert optsz == 0xE0 and struct.unpack_from('<H', exe, opt)[0] == 0x10B
    assert struct.unpack_from('<I', exe, opt+92)[0] == 0x10
    entry_rva = struct.unpack_from('<I', exe, opt+16)[0]
    entry_off = None
    pdata_raw = 0
    for i in range(nsec):
        o = opt + optsz + i*40
        name = exe[o:o+8].rstrip(b'\0').decode('ascii', 'replace')
        vsz, va, rsz, rptr = struct.unpack_from('<IIII', exe, o+8)
        if name == '.pdata':
            pdata_raw = rptr
        if va <= entry_rva < va + vsz:
            entry_off = rptr + (entry_rva - va)
    assert entry_off is not None
    def sum32(off):
        # Gate semantics: a zero-filled 0x400 buffer covering file offsets
        # [off, off+0x400); bytes before the start of the file stay zero.
        # Clamp the source start so a negative off (entry_off < 0x200)
        # does not wrap the Python slice around to the end of the file.
        buf = bytearray(0x400)
        start = max(off, 0)
        src = exe[start:off+0x400]
        buf[start-off:start-off+len(src)] = src
        return sum(buf) & 0xFFFFFFFF
    sb = sum32(entry_off - 0x200)
    r8 = sum32(pdata_raw)
    return dict(size=size, head_sum=head_sum, entry_off=entry_off,
                pdata_raw=pdata_raw, sb=sb, r8=r8)

def build_blr(exe: bytes) -> bytes:
    g = gate_inputs(exe)
    size, head_sum, sb, r8 = g['size'], g['head_sum'], g['sb'], g['r8']
    blr = bytearray(0x400)
    struct.pack_into('<H', blr, 0x000, (size ^ head_sum) & 0xFFFF)
    base = bytes([0x00, 0x00, 0x00, 0x00])          # [0x10..0x13] free choice
    blr[0x10:0x14] = base
    blr[0x3C] = (base[0] + 0x1F) & 0xFF
    blr[0x6B] = (base[1] + 0x33) & 0xFF
    blr[0x3F0] = (base[2] + 0x47) & 0xFF
    blr[0x3F1] = (base[3] + 0x5B) & 0xFF
    v = (head_sum ^ (size >> 1)) & 0xFFFF
    nv = 0xFFFF - v
    szb = struct.pack('>I', size)   # gate's orr-chain reads the even bytes big-endian
    blr[0x100] = szb[0]; blr[0x102] = szb[1]; blr[0x104] = szb[2]; blr[0x106] = szb[3]
    blr[0x101] = v & 0xFF;  blr[0x105] = (v >> 8) & 0xFF
    blr[0x103] = nv & 0xFF; blr[0x107] = (nv >> 8) & 0xFF
    blr[0x200:0x200+11] = b'APPMAIN.EXE'
    for off, val in ((0x249, sb >> 24), (0x256, sb >> 16), (0x258, sb >> 8), (0x265, sb)):
        blr[off] = val & 0xFF
    for off, val in ((0x295, r8 >> 24), (0x2A2, r8 >> 16), (0x2B1, r8 >> 8), (0x2B4, r8)):
        blr[off] = val & 0xFF
    blr[0x3FE] = 0xAA; blr[0x3FF] = 0x55
    t = sum(blr) & 0xFFFF          # pair bytes are still zero here
    s = (t + 0x1FE) & 0xFFFF
    struct.pack_into('<H', blr, 0x3FA, s)
    struct.pack_into('<H', blr, 0x3FC, 0xFFFF - s)
    return bytes(blr)

def verify(exe: bytes, blr: bytes) -> bool:
    """Offline re-implementation of every blr check in gate 0x12fe5c."""
    g = gate_inputs(exe)
    size, head_sum, sb, r8 = g['size'], g['head_sum'], g['sb'], g['r8']
    assert len(blr) == 0x400
    assert blr[0x3FE] == 0xAA and blr[0x3FF] == 0x55
    s = sum(blr) & 0xFFFF
    assert struct.unpack_from('<H', blr, 0x3FA)[0] == s, 'sum'
    assert struct.unpack_from('<H', blr, 0x3FC)[0] == (0xFFFF - s) & 0xFFFF, '~sum'
    assert struct.unpack_from('<H', blr, 0x000)[0] == (size ^ head_sum) & 0xFFFF, 'check0'
    v = (head_sum ^ (size >> 1)) & 0xFFFF
    assert blr[0x101] | (blr[0x105] << 8) == v, 'v'
    assert blr[0x103] | (blr[0x107] << 8) == (0xFFFF - v) & 0xFFFF, '~v'
    assert (blr[0x106] | (blr[0x104] << 8) | (blr[0x102] << 16) | (blr[0x100] << 24)) == size, 'size'
    assert blr[0x200:0x200+16] == b'APPMAIN.EXE' + b'\0'*5, 'name'
    assert blr[0x3C] == (blr[0x10] + 0x1F) & 0xFF
    assert blr[0x6B] == (blr[0x11] + 0x33) & 0xFF
    assert blr[0x3F0] == (blr[0x12] + 0x47) & 0xFF
    assert blr[0x3F1] == (blr[0x13] + 0x5B) & 0xFF
    assert (blr[0x265] | (blr[0x258] << 8) | (blr[0x256] << 16) | (blr[0x249] << 24)) == sb, 'sb'
    assert (blr[0x2B4] | (blr[0x2B1] << 8) | (blr[0x2A2] << 16) | (blr[0x295] << 24)) == r8, 'r8'
    return True

if __name__ == '__main__':
    exe = open(sys.argv[1], 'rb').read()
    blr = build_blr(exe)
    print('gate inputs:', gate_inputs(exe))
    print('verify:', verify(exe, blr))
    if len(sys.argv) > 2:
        open(sys.argv[2], 'wb').write(blr)
        print('wrote', sys.argv[2], len(blr))
