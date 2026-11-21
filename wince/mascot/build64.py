#!/usr/bin/env python3
"""p64 build: HQ-profile packs (bdm CORE premult ARGB, low tol,
no 565 profile) + AppMain64.exe (software alpha blend backend,
mascot.cfg HQ=1). One size per pack; SZ= in mascot.cfg selects
it explicitly.
  アプリ\\P64A\\  mythra HQ tol2 @320
  アプリ\\P64B\\  miku   HQ tol1 @320
  アプリ\\P64C\\  mythra HQ tol2 @480 (Y=0)
  アプリ\\P64D\\  miku   HQ tol1 @480 (Y=0)
Character data is local-verification only; nothing here is
committed to any repo.
"""
import os, sys
HID = os.path.dirname(os.path.abspath(__file__))
P64 = os.path.abspath(os.path.join(HID, '..'))
HF = os.path.abspath(os.path.join(HID, '..', '..'))
SRC56 = os.path.join(HF, 'p56work', 'src')
GUEST56 = os.path.join(HF, 'p56work', 'guest')
sys.path.insert(0, SRC56)
from mksd import Fat16Builder

APP = open(os.path.join(P64, 'AppMain64.exe'), 'rb').read()
BLR = open(os.path.join(P64, 'AppMain64.blr'), 'rb').read()
CFG = open(os.path.join(HF, 'p46work', 'static', 'AppMain.cfg'), 'rb').read()
DIN0 = open(os.path.join(HF, 'p46work', 'static', 'index.din'), 'rb').read()

_OLD = '電子辞書アップデートツール(V7.7) for PW-SH1'.encode('utf-8')
assert len(_OLD) == 56 and DIN0.count(_OLD) == 1


def din_named(title):
    nb = title.encode('utf-8')
    assert len(nb) <= 56
    return DIN0.replace(_OLD, nb + b' ' * (56 - len(nb)))


def build(sdname, folder, title, packpath, cfgtext):
    pack = open(packpath, 'rb').read()
    files = [('AppMain.exe', False, APP), ('AppMain.cfg', False, CFG),
             ('index.din', False, din_named(title)),
             ('AppMain.blr', False, BLR),
             ('mascot.cfg', False, cfgtext.encode('ascii')),
             ('mascot.bdm', False, pack)]
    sd = os.path.join(GUEST56, sdname)
    b = Fat16Builder(sd)
    b.add_root('アプリ', True, [(folder, True, files)])
    b.save()
    open(sd, 'wb').write(b.img)
    print(f'wrote {sd} img={len(b.img)} pack={len(pack)} ({packpath})')


BAKES = os.path.join(P64, 'bakes')
if __name__ == '__main__':
    which = sys.argv[1] if len(sys.argv) > 1 else 'all'
    if which in ('all', 'my320'):
        build('sd64a.img', 'P64A', 'P64A MythraHQ320 マスコット',
              os.path.join(BAKES, 'mythra-hq-t2.bdm'), 'HQ=1\nSZ=320\n')
    if which in ('all', 'mk320'):
        build('sd64b.img', 'P64B', 'P64B MikuHQ320 マスコット',
              os.path.join(BAKES, 'miku-hq-t1.bdm'), 'HQ=1\nSZ=320\n')
    if which in ('all', 'my480'):
        build('sd64c.img', 'P64C', 'P64C MythraHQ480 マスコット',
              os.path.join(BAKES, 'mythra-hq480-t2.bdm'),
              'HQ=1\nSZ=480\nX=247\nY=0\n')
    if which in ('all', 'mk480'):
        build('sd64d.img', 'P64D', 'P64D MikuHQ480 マスコット',
              os.path.join(BAKES, 'miku-hq480-t1.bdm'),
              'HQ=1\nSZ=480\nX=270\nY=0\n')
    print('done')
