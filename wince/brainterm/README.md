# BrainTerm — a command-line shell app for the SHARP Brain (WinCE)

`BrainTerm.exe` is a self-made terminal + command interpreter that
runs as an ordinary windowed Windows CE application on the SHARP
Brain (PW-AJ2/SH6 class, Windows CE 6.0). The ROM's stock text
shell (`\Windows\shell.exe`, TxtShell) cannot be used interactively
on this image because the OS ships no console device (see
`../braincon/` for the driver route that fixes that); BrainTerm
takes the independent route of implementing the terminal window
and the command interpreter inside one GUI app.

## Files

| file | role |
|---|---|
| `brainterm.c` | the whole app: terminal window, cell-grid renderer, line editor with syntax highlighting, command interpreter |
| `pe-link48.py` | minimal PE linker (ELF32-ARM object → WinCE PE, coredll ordinal imports) |
| `build48.py` | build driver: clang compile + unresolved-symbol check against the linker ordinal table, then link |
| `THEMES.md` | the four colour themes with their sourced values |
| `prebuilt/BrainTerm.exe` | prebuilt binary (271,872 B, 58 COREDLL ordinal imports) |
| `prebuilt/launch/` | **complete additional-app set, ready to launch** — see Install |

## Build

Requirements: clang (ARM backend) and python3. No SDK is needed —
the source is freestanding and declares the coredll surface it
uses itself.

```sh
python3 build48.py   # writes BrainTerm.exe next to the sources
```

## Install (additional-app route)

BrainTerm is launched through the shell's additional-app list.
**`prebuilt/launch/` contains the complete set, ready to use:**
copy its four files, unchanged, into a folder `\アプリ\BrainTerm\`
on a bare FAT16 SD card:

| file | origin |
|---|---|
| `AppMain.exe` | copy of `BrainTerm.exe` |
| `AppMain.blr` | gate record for this exe, generated with `../blr_build.py` |
| `AppMain.cfg`, `index.din` | from a genuine SHARP additional-app set (PW-SH1) |

The entry then appears in the shell's 追加アプリ list and starts
with a normal window (no ATR sidebar).

Provenance and verification of the archived set: the four files are
the exact bytes of the set that was launched and exercised on the
guest during development (extracted back out of the FAT16 SD image
used for those runs; `AppMain.exe` md5 `574fe9e5…` = the prebuilt
above, `AppMain.cfg` md5 `3e6d243e…`, `index.din` md5 `7e852cfd…`,
`AppMain.blr` md5 `c16ce795…`). The archived `AppMain.blr` was
re-checked against this repository's generator —
`python3 ../blr_build.py AppMain.exe` reproduces it **byte for
byte**, and the generator's offline gate re-implementation
(`verify()`) passes. If `BrainTerm.exe` is ever rebuilt, the blr
must be regenerated for the new exe bytes and the whole set
replaced together: the gate checks the blr against the exe in the
same folder, so an exe-only update will not launch.

## Features

- Framed, movable/resizable window; fixed cell grid (9×21 px from
  MS Gothic metrics) with per-cell rendering, wrap at the final
  column, 512-line scrollback (PgUp key / touch drag).
- cwd prompt (`\>`, `\nand2>`, …; 0x5C renders as ¥ in the
  device's Japanese font, as usual on this machine).
- Live syntax highlighting of the input line: known verb, unknown
  verb (error colour), path argument, quoted string; coloured
  output (`dir` directories, dim footers/headers, red errors).
- Commands: help / dir, ls / cd / pwd / type, cat (UTF-8 and
  UTF-16LE with BOM detection, Japanese text) / copy, cp / move,
  mv, ren / del, rm / md, mkdir / rd, rmdir / run, start / ps /
  kill / ver / mem / time, date / echo / cls / theme / exit, quit.
- Line editing: Backspace/Delete/arrows/Home/End, 24-entry
  history, Ctrl+A/E/K/U/L, Ctrl+C aborts long output.
- Four themes (onedark default, dracula, tokyo, github), all
  values taken from the themes' official definitions — see
  THEMES.md.
- True monospace: the app registers `\Nand\msgothic.ttc` with
  `AddFontResourceW` in its own process before creating the
  "MS Gothic" font, so GWES does not substitute the proportional
  dictionary font. No registry or \Windows changes.

## Platform notes (measured on the guest)

- This CE build's `toolhelp.dll` exports `Process32First` /
  `Process32Next` **without** the W suffix, and `PROCESSENTRY32`
  is the 564-byte CE layout (trailing `th32MemoryBase` /
  `th32AccessKey`). `ps` resolves the functions at runtime and
  re-checks them on every call.
- Symbol keys on the Brain keyboard arrive as a chord
  [VK 0x7B prefix][base key] (~271 ms apart); BrainTerm decodes
  pairs within a 500 ms window (Q=1 … P=0, M=., N=comma, H=;,
  '-' key='/', L='\'). A lone prefix flushed by the blink timer
  means the Backslash key itself.
- Host-key remaps measured on this guest: Home→F1, End→PgUp,
  PgUp→PgDn (scrollback), Delete→Esc (forward delete).
