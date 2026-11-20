# Terminal colour themes — sourced values (fetched 2026-11-20)

All values were taken from the themes' official definitions (never from
memory), as required. The terminal uses: bg (client background), fg
(text), panel (status-strip background), panelFg (status-strip text),
accent (cursor / highlights), lineHi (current-line band).

## One Dark Pro (DEFAULT)
Source: Binaryify/OneDark-Pro, `themes/OneDark-Pro.json`
(commit 75c80ab18d90655e05ad837bb8d89f5e68d5ce53), "colors" section,
fetched via the GitHub connector.
- editor.background / terminal.background  #282C34
- editor.foreground / terminal.foreground  #ABB2BF
- sideBar.background / statusBar.background #21252B
- statusBar.foreground                      #9DA5B4
- editorCursor.foreground (accent)          #528BFF
- editor.lineHighlightBackground            #2C313C
- focusBorder                               #3E4452

## Dracula
Source: official specification, https://draculatheme.com/spec
("Dracula Classic" palette + UI palette), fetched 2026-11-20.
- Background            #282A36
- Foreground            #F8F8F2
- Selection             #44475A
- Comment               #6272A4
- Current-line fallback #353747 (spec's opaque fallback)
- Background Dark       #21222C  (panel)
- Background Darker     #191A21
- Accents: Pink #FF79C6, Purple #BD93F9, Green #50FA7B, Cyan #8BE9FD,
  Red #FF5555, Orange #FFB86C, Yellow #F1FA8C
- Terminal accent used: Pink #FF79C6

## Tokyo Night
Source: tokyo-night/tokyo-night-vscode-theme (formerly enkia/…),
`themes/tokyo-night-color-theme.json`, fetched via the GitHub
connector; cross-checked against the repo README palette table
(Editor Foreground #A9B1D6, Editor Background (Night) #1A1B26).
- editor.background            #1A1B26
- editor.foreground            #A9B1D6
- sideBar/statusBar/titleBar   #16161E
- statusBar.foreground         #787C99
- editorCursor.foreground      #C0CAF5  (accent)
- editor.lineHighlightBackground #1E202E

## GitHub Dark ("GitHub Dark Default")
Source: the theme is generated from Primer Primitives tokens
(primer/github-vscode-theme builds from @primer/primitives).
Token values (dark): canvas.default #0D1117, fg.default #E6EDF3,
fg.muted #848D97, canvas.overlay #161B22, border.default #30363D,
canvas.inset #010409, accent.fg #2F81F7, success.fg #3FB950,
danger.fg #F85149. Corroborated by the Primer DESIGN.md token
listing and independent theme listings (bg #0D1117 / surface
#161B22 / fg #E6EDF3).
- bg #0D1117, fg #E6EDF3, panel #161B22, panelFg #848D97,
  accent #2F81F7, lineHi #010409

## Terminal font (sourced from the guest image itself)
- ROM font files (default.fdf census, images A/B): only
  `tahoma.ttf` (312,684 B) — proportional, Latin-centric; all GWE
  system fonts (SYSFNT / Menu / OOM) are "Tahoma", Ht=-12.
- `\Nand\msgothic.ttc` (4,204,524 B, in the \Nand census) was
  extracted and its name table parsed on the host: subfont 0 =
  "MS Gothic" / "ＭＳ ゴシック", subfont 1 = "MS PGothic" /
  "ＭＳ Ｐゴシック" (TTC, 2 faces, v2.00 Ricoh/Ryobi).
  MS Gothic is fixed-pitch with full Japanese coverage — the
  terminal registers it per-process with AddFontResourceW and
  creates it via CreateFontIndirectW. No registry or \Windows
  changes; nothing persistent; wceprj is untouched.
