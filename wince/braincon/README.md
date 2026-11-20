# BrainCon — a reusable console stream driver for the SHARP Brain (WinCE)

`BrainCon.dll` supplies the console device this ROM image lacks, so
the stock text shell (`\Windows\shell.exe`, TxtShell) — and any other
guest program — gets a working console on the on-screen terminal
`P47Term.exe`. Built for the SHARP Brain (PW-AJ2/SH6 class, Windows
CE). The driver is written as a reusable component: its skeleton
(Part 1 of `braincon.c`) is meant to seed future CE stream drivers
on this platform, and any guest app can use the console through the
plain stream interface documented below.

## Files

| file | role |
|---|---|
| `braincon.c` | the stream driver (Part 1 = reusable skeleton, Part 2 = console) |
| `conioctl.h` | the driver↔terminal channel protocol (IOCTL map, CON_INFO) |
| `p47term.c` | terminal window + driver registrar + TxtShell launcher |
| `pe-link47.py` | minimal PE linker (ELF32-ARM object → WinCE PE, coredll ordinal imports) |
| `build.sh` | builds `BrainCon.dll` and `P47Term.exe` |
| `THEMES.md` | terminal colour themes with their sourced values |
| `prebuilt/BrainCon.dll`, `prebuilt/P47Term.exe` | prebuilt binaries |
| `prebuilt/launch/` | **complete additional-app set for the terminal, ready to launch** — see Deployment |

## Build

Requirements: clang (ARM backend) and python3. No SDK is needed —
the sources are freestanding and declare the coredll surface they
use themselves.

```sh
sh build.sh
```

Prebuilt binaries are included under `prebuilt/` for direct use; `build.sh` regenerates them from source.

This compiles both sources
(`clang -target armv5te-none-eabi -marm -Os -ffreestanding
-fno-builtin -fshort-wchar`) and links them with `pe-link47.py`:

- `BrainCon.dll` (~80 KB, mostly zero-filled ring storage) — DLL,
  entry `DllMain`, base `0x10000000`, exports the ten `CON_*`
  stream entry points. The PE carries `.reloc`, so device.exe may
  load it at any address.
- `P47Term.exe` (~28 KB) — GUI executable, entry `WinMain`.

The linker's coredll ordinal seed table covers every import of
these two sources. (It can optionally merge a fuller ordinal map
from `../static/coredll-exports.json` if one is present; not needed
for this build.)

## Deployment

Nothing is baked into the ROM image; everything is installed at
runtime:

1. Copy `BrainCon.dll` to `\Nand2\BrainCon.dll` on the device (the
   terminal registers the driver from that path).
2. Put the terminal on an SD card as an **additional app** (the
   stock shell's normal route for card apps). **`prebuilt/launch/`
   in this repository contains the complete set, ready to use:**
   copy its four files, unchanged, into `\アプリ\P47TERM\` on the
   card:

   ```text
   \アプリ\P47TERM\{ AppMain.exe = P47Term.exe,
                      AppMain.cfg, index.din, AppMain.blr }
   ```

   `AppMain.cfg` and `index.din` are taken from an official
   additional-app package (PW-SH1; the din only supplies the list
   title); `AppMain.blr` is generated for this exe with the
   generator in this repository:

   ```sh
   python3 ../blr_build.py AppMain.exe AppMain.blr
   ```

   Provenance and verification of the archived set: the four files
   are the exact bytes of the set that was launched and exercised
   on the guest during development (extracted back out of the
   FAT16 SD image used for those runs; `AppMain.exe` md5
   `90d6252b…` = `prebuilt/P47Term.exe`, `AppMain.cfg` md5
   `3e6d243e…`, `index.din` md5 `7e852cfd…`, `AppMain.blr` md5
   `17e2906d…`). The archived `AppMain.blr` was re-checked against
   the generator: `python3 ../blr_build.py AppMain.exe` reproduces
   it **byte for byte**, and the generator's offline gate
   re-implementation (`verify()`) passes. If `P47Term.exe` is ever
   rebuilt, regenerate the blr and replace the whole set together.

   `BrainCon.dll` itself needs **no** blr and is not part of the
   launch folder: the shell's gate validates only the
   `AppMain.blr` sitting next to an additional app's
   `AppMain.exe`, while the DLL is loaded by `device.exe` from
   `\Nand2` through the registry entry the terminal writes (step 1
   above and "The device" below), a path the gate never inspects.

3. Optionally write `\Nand2\P47MODE.TXT` (UTF-16LE, first line
   `termkbd` for plain interactive use; see Modes below) and
   `\Nand2\P47THEME.TXT` (`onedark|dracula|tokyo|github`).
4. Launch it from 追加コンテンツ → 追加アプリ/動画 like any
   downloaded app. The terminal registers and activates the driver,
   opens the terminal window, and starts TxtShell with its stdio
   bound to `CON1:`.

(The terminal is deliberately *not* installed in a stock app's
slot: that route leaves the stock app's sidebar frame composited
around the window.)

## The device

- Name: **`CON1:`** (prefix `CON`, index 1), a standard CE stream
  device served by `device.exe`.
- Registered at runtime by the terminal app (nothing is baked into
  the image): it writes

  ```
  HKLM\Drivers\BuiltIn\BrainCon
      Dll     (REG_SZ)    = \Nand2\BrainCon.dll
      Prefix  (REG_SZ)    = CON
      Index   (REG_DWORD) = 1
      Order   (REG_DWORD) = 0
      Context (REG_DWORD) = 0
      Flags   (REG_DWORD) = 0
  ```

  then calls `ActivateDeviceEx(L"Drivers\\BuiltIn\\BrainCon", …)`
  (fallback: `RegisterDevice(L"CON", 1, dllpath, 0)`).
- Any process can then `CreateFileW(L"CON1:", …)` and ReadFile /
  WriteFile it like a console. Multiple concurrent opens are
  supported; each open has its own context and read timeout.

## Driver semantics (braincon.c, Part 2)

- **Byte stream.** Text encoding is whatever the writer's runtime
  produces — for CE CRT stdio consumers (TxtShell) that is ANSI/MBCS
  bytes (CP932 on this image), because coredll's `fputws`/`fgetws`
  run the stream in byte mode. Verified on the wire: the shell's
  first write is the 58 ANSI bytes of its banner. The driver itself
  is encoding-agnostic.
- **Input ring** 8 KiB (terminal → readers), **output ring** 64 KiB
  (writers → terminal), both under one critical section.
- **Reads block** on an event until input is queued: per-open
  timeout (default INFINITE; `IOCTL_CON_SETREADTIMEOUT`), timeout 0
  → `ERROR_NO_DATA` (232) if empty, finite expiry →
  `ERROR_TIMEOUT` (1460). EOF (a 0-length return) happens only after
  `IOCTL_CON_CLOSEINPUT`. Access mode is enforced (reading a
  write-only open → `ERROR_ACCESS_DENIED`).
- **Writes never block** and are atomic per call; a full output ring
  drops the OLDEST bytes (scrollback semantics) and counts them in
  `CON_INFO.droppedBytes`. A full input ring drops the newest excess
  and counts it too.
- **Seek** fails with `ERROR_INVALID_FUNCTION` (a console is not
  seekable). **PowerUp/PowerDown** are no-ops returning success.
- **Echo** (default on): bytes queued with `IOCTL_CON_PUTINPUT` are
  also appended to the output stream, cooked-mode style — CR is
  echoed as CRLF, a following LF is not echoed. Toggle with
  `IOCTL_CON_SETECHO`.

## Channel protocol (driver ↔ terminal)

The terminal is *just another client* of the driver, over the same
handle any app would use — the driver has no window and no knowledge
of the terminal, so it outlives any consumer. The terminal:

- drains output with `IOCTL_CON_WAITOUTPUT` (poll with timeout 0)
  then `IOCTL_CON_GETOUTPUT` (up to 8 KiB per call),
- feeds keystrokes with `IOCTL_CON_PUTINPUT`,
- reads counters with `IOCTL_CON_GETINFO`.

IOCTL codes are `CTL_CODE(0x22, fn)` = `(0x22<<16)|(fn<<2)`:

| fn | name | direction / payload |
|---|---|---|
| 0x800 | GETOUTPUT | out: bytes drained from the output ring (non-blocking) |
| 0x801 | WAITOUTPUT | in: DWORD timeout ms → whether output is pending |
| 0x802 | PUTINPUT | in: bytes queued as console input |
| 0x803 | PEEKINPUT | out: DWORD bytes queued |
| 0x804 | PEEKOUTPUT | out: DWORD bytes pending |
| 0x805 | FLUSH | in: DWORD flags (1=input, 2=output) |
| 0x806 | CLOSEINPUT | readers see EOF after the queue drains |
| 0x807 | GETINFO | out: `CON_INFO` (version, opens, totals, drops, queue sizes) |
| 0x808 | SETDEFAULTTIMEOUT | in: DWORD ms, default read timeout for new opens |
| 0x809 | SETREADTIMEOUT | in: DWORD ms, this open's read timeout |
| 0x80A | SETECHO | in: DWORD 0/1 |

## Making a process's stdio use the console

This kernel has **no `GetStdHandle`/`SetStdHandle` in coredll**, and
**STARTUPINFO std handles are ignored** (proven by probes: file- and
device-backed handles both leave TxtShell exiting with code 1).
CE stdio is bound by **paths**:

```c
SetStdioPathW(0, L"\\CON1:");   // stdin   (ids are 0/1/2;
SetStdioPathW(1, L"\\CON1:");   // stdout   the -10/-11/-12
SetStdioPathW(2, L"\\CON1:");   // stderr   forms fail with 87)
CreateProcessW(L"\\Windows\\shell.exe", L"-c", …);
```

Children inherit the caller's stdio paths. TxtShell's switches
(parsed from its command line): **`-c`** use coredll CRT stdio
(the paths above); **`-d`** like `-c` but output also goes to
`OutputDebugStringW`; **`-r <n>`** main-thread priority. With no
switch it tries to open `\Release\con` (the devkit ReleaseFSD host
directory) forever — that is why a plain launch "hangs" silently.

## Reusing the skeleton for another driver

`braincon.c` is split so the boilerplate can seed future drivers:

- **Part 1 (generic)**: coredll ordinal imports, `DllMain`, the ten
  stream entry points (`XXX_Init/Deinit/Open/Close/Read/Write/Seek/
  IOControl/PowerUp/PowerDown`), `DEVCTX`/`OPENCTX` context
  management with refcounting, and the `SetLastError` conventions.
  Copy it, rename the `CON_` prefix (and the exports list at link
  time), keep the context plumbing.
- **Part 2 (device-specific)**: the ring buffers, the device
  semantics (`dev_read`/`dev_write`), and the IOCTL handlers.

Three linker lessons are baked into `pe-link47.py` (don't
regress them): the export-directory struct is `'<IIHHIIIIIII'`;
`.data`'s RVA reservation must cover data+BSS or the next section is
mapped over it; and the absolute IAT address inside each import
thunk must be emitted as a `.reloc` site, or a DLL loaded away from
its preferred base calls through garbage (in device.exe this made
`ActivateDeviceEx` hang).

## The terminal (p47term.c)

- A normal **overlapped window** (caption + sizable frame), rect
  (96,12)–(784,464) — fully inside the 854×480 panel — coexisting
  with the shell screens; movable/resizable/maximizable. A status
  strip at the bottom shows the theme, the shell state, and the
  scrollback offset (`[-N]`) when scrolled.
- **Cell-grid rendering**: every row is drawn with `ExtTextOutW`
  plus an explicit per-character advance array (`lpDx`), so glyph
  origins sit exactly on a fixed cell grid (cell = font's average
  advance × font height; fullwidth = 2 cells). Line pitch equals
  the cell height — no extra leading. Text **wraps at the last
  visible column** (column count is recomputed from the client
  width on resize). The cursor is a **blinking block** in the
  theme's accent colour (the character under it is drawn reversed).
- **Scrollback**: 400-line model; **PageUp / PageDown** move the
  view (new output snaps back to the tail), with a scrollbar drawn
  on the right edge.
- **Font**: MS Gothic from `\Nand\msgothic.ttc` (a stock SHARP font
  file; face names verified by parsing the TTC), registered with
  `AddFontResourceW` for this process only — no registry writes, no
  `\Windows` changes, nothing persistent, so the stock dictionary
  shell (wceprj) cannot be affected. `CreateFontIndirectW`,
  height −20 **bold**, `lfQuality = CLEARTYPE_QUALITY`. Measured
  in-guest: tmHeight 20, halfwidth advance 10, fullwidth 20 (true
  fixed pitch), and `GetTextFaceW` confirms the selected face is
  really "MS Gothic". If the font file is missing the app falls
  back to the system font.
- **Decoding**: the byte stream is decoded as CP932 with SJIS
  lead-byte carry-over between drains; keystrokes are encoded back
  with `WideCharToMultiByte(932)` (ASCII fast path), so Japanese
  text works in both directions.
- **Themes**: palette table in the source; values and sources in
  `THEMES.md`. Default **One Dark Pro**; select with
  `\Nand2\P47THEME.TXT` (`onedark|dracula|tokyo|github`) or cycle
  with **F2** (persisted back to the file).
- Modes (`\Nand2\P47MODE.TXT`): `term` (auto-runs `gi proc`, `mi`,
  `gi thrd` for proof runs), `termkbd` (no auto script — type on the
  keyboard), `selftest` (driver conformance checks, log to
  `\Nand2\P47OUT.TXT`).

## Known limits

- Input is a single shared queue: if two processes read `CON1:` at
  once they compete for keystrokes (same as a real console).
- No line editing in the driver: Backspace is passed through as a
  byte (cooked-mode editing is the consumer's job, as with TxtShell
  reading whole lines).
- Output written while no terminal is attached stays in the ring
  (drop-oldest past 64 KiB); a terminal started later sees the tail.
- The driver's DLL file is mostly zero-filled ring storage
  (~80 KB); the terminal EXE is ~28 KB.
- **Prompt latency is the shell's own CRT behaviour** (byte-level
  proven): the shell prints the prompt with `fputws` before every
  read, but coredll's CRT fully buffers stdio output to the
  device — coredll has no `GetFileType` export at all, so the CRT
  cannot classify the target as a character device. On the wire:
  the banner is one solo 58-byte write at startup; keystroke echoes
  follow immediately (they are the driver's); everything else —
  prompts and command output — accumulates in an ~8 KiB CRT
  buffer and arrives in one burst when it fills (a captured burst
  starts `gi thrd\r\n` (echo) then `Windows CE>PROC…`, i.e. the
  first prompt followed by `gi proc`'s output, then the later
  prompts and outputs). Nothing is dropped by the driver or the
  terminal; the prompt is visible in the scrollback after each
  burst. There is no legitimate way to flush another process's
  CRT buffer, so this is also exactly how TxtShell behaves on a
  stock CE console on this image.
- The SHARP shell composites a faint few-pixel offset sliver of an
  additional app's window at its left edge (observed with this
  app; a shell-frame presentation quirk, not under the app's
  control). The window itself reports and uses rect
  (96,12)–(784,464).
