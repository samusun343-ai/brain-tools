# Mascot — a desktop mascot player for the SHARP Brain (WinCE)

A windowless, always-on-top character player for the stock WinCE
shell: it plays back a pre-baked animation pack (`.bdm`) with
per-pixel alpha, blended in software against a snapshot of whatever
is behind it. No `AlphaBlend`, no window regions — the player
restores the background strip, blends the frame into a memory DC and
`BitBlt`s it out, which is what makes clean edges possible on this
GWES at all.

`mascot64.c` is the current player (the "HQ" pipeline). `mascot.c`
is the original region-based player, kept for reference: its
region union exposed near-transparent pixels whose premultiplied
colour noise blew up into saturated speckles and black dashes, and
its RGB565 + tolerance snapping posterised gradients. The HQ
pipeline roughly halves the in-silhouette error (MAE @480:
Mythra 2.51 → 1.25, Miku 3.66 → 1.64) and is much faster
(see Measurements).

## Files

| file | role |
|---|---|
| `mascot64.c` | the current player: `.bdm` v3 reader, software alpha-blend presenter, background snapshot/restore, tap/drag handling |
| `mascot.c` | the original player (region-based presentation; superseded, reference only) |
| `pe-link56.py` | minimal PE linker (ELF32-ARM object → WinCE PE, coredll ordinal imports) |
| `build64.py` | SD-card builder for the verified guest runs; assembles an additional-app folder per character/size (paths point at the research workspace it was run from) |
| `prebuilt/AppMain64.exe` | prebuilt binary of `mascot64.c` (1,891,328 B, 50 coredll ordinal imports) |
| `prebuilt/launch/` | additional-app launch set — see Install. `AppMain.blr` is regenerated with `../blr_build.py` and byte-matches the guest-verified file |

## The `.bdm` pack

The pack is `.bdm` v3: a premultiplied-ARGB core of per-frame
sparse tiles. HQ packs are baked with a low tolerance (tol 1–2) and
**no** RGB565 profile. The format is shared with the Brainux shell;
its spec lives with that project, not in this repo.

**Character packs are not distributed here.** `mascot.bdm` is baked
locally from your own rendered frames and never committed; the
launch set below ships without one on purpose.

## Build

Requirements: clang (ARM backend) and python3. No SDK is needed.

- Compile **at `-O0`**. The recovered clang-18 in this environment
  drops initialisers of certain statics at `-O1` (proven with a
  faithful reproduction + readelf), which made layouts and flags
  silently wrong. If clang-18 is unavailable, `zig cc` (clang 22)
  works with:
  `-target arm-freestanding-eabi -mcpu=arm926ej_s -marm -fshort-wchar -ffreestanding -nostdlib -fno-builtin -fno-sanitize=undefined -fno-unwind-tables -fno-asynchronous-unwind-tables -O0`
- Link with `pe-link56.py`, then generate the launch companion with
  `../blr_build.py`.

## Install

Copy `prebuilt/launch/` as one app folder under `アプリ\` on an SD
card, add your own `mascot.bdm` next to it, and adjust
`mascot.cfg`:

```text
HQ=1
SZ=320
```

(`SZ` selects the baked size, 320 or 480.) The folder then appears
in the shell's additional-apps list and launches like any other
additional app.

## Measurements (QEMU guest, PW-SH6 model, 128 MB)

- Mythra @320: decode 1 ms / blend 7 ms / worst frame 21 ms inside
  a 33 ms frame interval.
- Miku @480: decode 3 ms / blend 17 ms / worst frame 63 ms at a
  50 ms interval. (The old region pipeline needed 50–93 ms of
  region work per frame at @480, ≈14 fps.)
- Memory, worst case (Miku @480, 30.06 MB pack): the player holds
  ≈33 MB in total and the guest still had 10,172 KB free of
  91,548 KB.

## Known remainders

- Tap / long-press menu / drag are implemented and the tap reaches
  the window (diagnostic log), but they are not yet confirmed on
  screen in the emulator.
- After some shell operations the screen presentation can freeze
  again in the emulator; the same freeze class affects ordinary
  windowed apps, so it is tracked as an environment issue, not a
  player bug.
- One character's source frames carry render noise from the
  original low-sample render; the fix is a high-sample re-render on
  the content side, not in the player.
