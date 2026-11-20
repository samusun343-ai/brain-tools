/* p47term.c -- BrainCon terminal + launcher (SHARP Brain, WinCE).
 *
 * One app, three jobs:
 *   1. Install (an embedded copy of BrainCon.dll is written to
 *      \Nand2 if none is there), register and activate the BrainCon
 *      console stream driver (device CON1:) -- see README.md. The
 *      additional-app set on the card is all a user needs.
 *   2. Host the on-screen terminal window: drains the driver's
 *      output ring (IOCTL_CON_GETOUTPUT) into a scrollback grid and
 *      feeds keystrokes back (IOCTL_CON_PUTINPUT).
 *   3. Launch the stock TxtShell (shell.exe -c) with its stdio
 *      paths bound to CON1:, so the shell's stdio lands here.
 *
 * Presentation (redesigned 2026-11-20 per user direction):
 *   - A normal overlapped window (caption, sizable frame), NOT
 *     fullscreen; it coexists with the shell screen and can be
 *     moved/resized/maximized.
 *   - Font: MS Gothic from \Nand\msgothic.ttc, registered for this
 *     process only via AddFontResourceW (no registry/\Windows
 *     changes). Fixed pitch, full ASCII+Japanese coverage. The
 *     console byte stream is decoded as CP932 (the image's ANSI
 *     codepage), so Japanese output renders instead of mojibake.
 *     Root cause of the old "banner became a row of boxes" bug was
 *     an encoding mismatch (ANSI bytes paired up as UTF-16LE ->
 *     unassigned CJK codepoints), NOT a glyph-coverage problem;
 *     the byte-model fix + this font close it for good.
 *   - Colour themes: palette table (THEMES.md has the sourced
 *     values): One Dark Pro (default), Dracula, Tokyo Night,
 *     GitHub Dark. Chosen by \Nand2\P47THEME.TXT (first line:
 *     onedark|dracula|tokyo|github); F2 cycles at runtime and
 *     persists the choice back to that file.
 *
 * Modes (from \Nand2\P47MODE.TXT):
 *   term     -- the full flow above (+ an auto script that runs
 *               `gi proc`, `mi`, `gi thrd` for the proof runs)
 *   termkbd  -- the full flow, no auto script: commands are typed
 *               on the (emulated) physical keyboard instead
 *   selftest -- driver conformance checks without TxtShell:
 *               multi-open, write->terminal drain, input queue read,
 *               read timeout behaviour; results to \Nand2\P47OUT.TXT
 */
typedef unsigned long DWORD;
typedef unsigned short WCHAR;
typedef unsigned char BYTE;
typedef int BOOL;
typedef void *HANDLE;
typedef long LONG;
#define TRUE 1
#define FALSE 0
#define NULL ((void *)0)
#define GENERIC_READ  0x80000000UL
#define GENERIC_WRITE 0x40000000UL
#define OPEN_EXISTING 3
#define CREATE_ALWAYS 2
#define FILE_ATTRIBUTE_NORMAL 0x80
#define STARTF_USESTDHANDLES 0x100
#define HKEY_LOCAL_MACHINE ((HANDLE)0x80000002UL)
#define REG_SZ 1
#define REG_DWORD 4
#define WM_DESTROY 0x0002
#define WM_SIZE 0x0005
#define WM_PAINT 0x000F
#define WM_TIMER 0x0113
#define WM_CHAR 0x0102
#define WM_KEYDOWN 0x0100
#define WS_VISIBLE 0x10000000UL
#define WS_OVERLAPPEDWINDOW 0x00CF0000UL
#define SW_SHOWNORMAL 1
#define VK_F2 0x71
#define STILL_ACTIVE 259

#include "conioctl.h"

typedef struct { DWORD left, top, right, bottom; } RECT;
typedef struct { HANDLE hdc; BOOL fErase; RECT rcPaint; BOOL fRestore; BOOL fIncUpdate; BYTE rgbReserved[32]; } PAINTSTRUCT;
typedef struct { DWORD style; void *lpfnWndProc; int cbClsExtra; int cbWndExtra; HANDLE hInstance; HANDLE hIcon; HANDLE hCursor; HANDLE hbrBackground; const WCHAR *lpszMenuName; const WCHAR *lpszClassName; } WNDCLASS;
typedef struct { HANDLE hwnd; DWORD message; DWORD wParam; long lParam; DWORD time; long pt_x, pt_y; } MSG;
typedef struct { DWORD cb; WCHAR *lpReserved; WCHAR *lpDesktop; WCHAR *lpTitle;
  DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute,
  dwFlags; unsigned short wShowWindow, cbReserved2; BYTE *lpReserved2;
  HANDLE hStdInput, hStdOutput, hStdError; } STARTUPINFO;
typedef struct { HANDLE hProcess, hThread; DWORD dwProcessId, dwThreadId; } PROCESS_INFORMATION;

extern HANDLE CreateFileW(const WCHAR *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
extern BOOL ReadFile(HANDLE, void *, DWORD, DWORD *, void *);
extern BOOL WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
extern BOOL CloseHandle(HANDLE);
extern BOOL DeviceIoControl(HANDLE, DWORD, void *, DWORD, void *, DWORD, DWORD *, void *);
extern BOOL CreateProcessW(const WCHAR *, const WCHAR *, void *, void *, BOOL, DWORD, void *, const WCHAR *, STARTUPINFO *, PROCESS_INFORMATION *);
extern BOOL GetExitCodeProcess(HANDLE, DWORD *);
extern void Sleep(DWORD);
extern DWORD GetTickCount(void);
extern DWORD GetLastError(void);
extern long RegCreateKeyExW(HANDLE, const WCHAR *, DWORD, WCHAR *, DWORD, DWORD, void *, HANDLE *, DWORD *);
extern long RegSetValueExW(HANDLE, const WCHAR *, DWORD, DWORD, const BYTE *, DWORD);
extern long RegCloseKey(HANDLE);
extern HANDLE ActivateDeviceEx(const WCHAR *, const void *, DWORD, void *);
extern BOOL RegisterDevice(const WCHAR *, DWORD, const WCHAR *, DWORD);
extern HANDLE LoadLibraryW(const WCHAR *);
extern void *GetProcAddressW(HANDLE, const WCHAR *);
extern BOOL FreeLibrary(HANDLE);
extern BOOL SetStdioPathW(DWORD, const WCHAR *);
extern BOOL GetStdioPathW(DWORD, WCHAR *, DWORD *);
extern DWORD GetFileSize(HANDLE, DWORD *);
extern DWORD GetModuleFileNameW(HANDLE, WCHAR *, DWORD);
extern int RegisterClassW(const WNDCLASS *);
extern HANDLE CreateWindowExW(DWORD, const WCHAR *, const WCHAR *, DWORD, int, int, int, int, HANDLE, HANDLE, HANDLE, void *);
extern BOOL ShowWindow(HANDLE, int);
extern BOOL UpdateWindow(HANDLE);
extern BOOL GetMessageW(MSG *, HANDLE, DWORD, DWORD);
extern BOOL TranslateMessage(const MSG *);
extern long DispatchMessageW(const MSG *);
extern long DefWindowProcW(HANDLE, DWORD, DWORD, long);
extern void PostQuitMessage(int);
extern HANDLE BeginPaint(HANDLE, PAINTSTRUCT *);
extern BOOL EndPaint(HANDLE, const PAINTSTRUCT *);
extern BOOL GetClientRect(HANDLE, RECT *);
extern HANDLE CreateSolidBrush(DWORD);
extern BOOL FillRect(HANDLE, const RECT *, HANDLE);
extern BOOL DeleteObject(HANDLE);
extern int DrawTextW(HANDLE, const WCHAR *, int, RECT *, DWORD);
extern DWORD SetTextColor(HANDLE, DWORD);
extern DWORD SetBkColor(HANDLE, DWORD);
extern DWORD SetTimer(HANDLE, DWORD, DWORD, void *);
extern BOOL KillTimer(HANDLE, DWORD);
extern BOOL InvalidateRect(HANDLE, const RECT *, BOOL);
extern HANDLE SetFocus(HANDLE);
extern BOOL DestroyWindow(HANDLE);
extern BOOL SetForegroundWindow(HANDLE);
extern BOOL GetWindowRect(HANDLE, RECT *);
extern BOOL MoveWindow(HANDLE, int, int, int, int, BOOL);
extern int AddFontResourceW(const WCHAR *);
extern BOOL RemoveFontResourceW(const WCHAR *);
extern HANDLE CreateFontIndirectW(const void *);
extern BOOL GetTextMetricsW(HANDLE, void *);
extern HANDLE SelectObject(HANDLE, HANDLE);
extern HANDLE GetDC(HANDLE);
extern BOOL ReleaseDC(HANDLE, HANDLE);
extern int MultiByteToWideChar(DWORD, DWORD, const char *, int, WCHAR *, int);
extern int WideCharToMultiByte(DWORD, DWORD, const WCHAR *, int, char *, int, const char *, BOOL *);
extern BOOL ExtTextOutW(HANDLE, int, int, DWORD, const RECT *, const WCHAR *, DWORD, const int *);
extern int GetTextFaceW(HANDLE, int, WCHAR *);
#define ETO_OPAQUE 0x0002
#define VK_PRIOR 0x21
#define VK_NEXT 0x22

typedef struct { LONG lfHeight, lfWidth, lfEscapement, lfOrientation, lfWeight;
  BYTE lfItalic, lfUnderline, lfStrikeOut, lfCharSet, lfOutPrecision,
       lfClipPrecision, lfQuality, lfPitchAndFamily; WCHAR lfFaceName[32]; } LOGFONT;
typedef struct { LONG tmHeight, tmAscent, tmDescent, tmInternalLeading, tmExternalLeading,
  tmAveCharWidth, tmMaxCharWidth, tmWeight, tmOverhang, tmDigitizedAspectX, tmDigitizedAspectY;
  WCHAR tmFirstChar, tmLastChar, tmDefaultChar, tmBreakChar;
  BYTE tmItalic, tmUnderlined, tmStruckOut, tmPitchAndFamily, tmCharSet; } TEXTMETRIC;
#define CP_SJIS 932

/* ---------------- log (file + on-screen mirror) ---------------- */
static WCHAR g_log[16384];
static DWORD g_logn;
static void log_ch(WCHAR c) { if (g_logn < 16380) g_log[g_logn++] = c; }
static void log_s(const char *s) { while (*s) log_ch((WCHAR)(*s++)); }
static void log_ws(const WCHAR *s) { while (*s) log_ch(*s++); }
static void log_nl(void) { log_ch(L'\r'); log_ch(L'\n'); }
static void log_dec(DWORD v) {
  char tmp[12]; int n = 0;
  if (!v) { log_ch(L'0'); return; }
  while (v) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
  while (n) log_ch((WCHAR)tmp[--n]);
}
static void log_hex(DWORD v) {
  int i; log_s("0x");
  for (i = 28; i >= 0; i -= 4) {
    DWORD d = (v >> i) & 0xF;
    log_ch((WCHAR)(d < 10 ? '0' + d : 'a' + d - 10));
  }
}
static void write_out(void) {
  HANDLE h = CreateFileW(L"\\Nand2\\P47OUT.TXT", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == (HANDLE)-1) return;
  DWORD wr = 0;
  static const WCHAR bom = 0xFEFF;
  WriteFile(h, &bom, 2, &wr, NULL);
  WriteFile(h, g_log, g_logn * 2, &wr, NULL);
  CloseHandle(h);
}

/* ---------------- mode / theme files ---------------- */
static void read_first_line(const WCHAR *path, WCHAR *out, DWORD cap) {
  HANDLE h = CreateFileW(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  DWORD n = 0, i = 0;
  BYTE buf[128];
  out[0] = 0;
  if (h == (HANDLE)-1) return;
  if (ReadFile(h, buf, 127, &n, NULL)) {
    if (n >= 2 && buf[0] == 0xFF && buf[1] == 0xFE) {
      DWORD k = 2;
      while (k + 1 < n && i + 1 < cap) {
        WCHAR c = (WCHAR)(buf[k] | (buf[k + 1] << 8));
        if (c == L'\r' || c == L'\n') break;
        out[i++] = c; k += 2;
      }
    } else {
      while (i < n && i + 1 < cap && buf[i] != '\r' && buf[i] != '\n') { out[i] = (WCHAR)buf[i]; i++; }
    }
  }
  out[i] = 0;
  CloseHandle(h);
}
static WCHAR g_mode[64];
static void read_mode(void) { read_first_line(L"\\Nand2\\P47MODE.TXT", g_mode, 64); }
static int wcmp(const WCHAR *a, const char *b) {
  while (*b) { if (*a != (WCHAR)(*b)) return 1; a++; b++; }
  return *a != 0;
}

/* ---------------- colour themes (values: see THEMES.md) ---------- */
/* Colours are stored as web hex 0xRRGGBB and converted at paint. */
typedef struct { const char *key; const char *label;
  DWORD bg, fg, panel, panelFg, accent, lineHi; } THEME;
static const THEME g_themes[4] = {
  { "onedark", "One Dark Pro", 0x282C34, 0xABB2BF, 0x21252B, 0x9DA5B4, 0x528BFF, 0x2C313C },
  { "dracula", "Dracula",      0x282A36, 0xF8F8F2, 0x21222C, 0xF8F8F2, 0xFF79C6, 0x353747 },
  { "tokyo",   "Tokyo Night",  0x1A1B26, 0xA9B1D6, 0x16161E, 0x787C99, 0xC0CAF5, 0x1E202E },
  { "github",  "GitHub Dark",  0x0D1117, 0xE6EDF3, 0x161B22, 0x848D97, 0x2F81F7, 0x010409 },
};
static int g_themeIdx;
static DWORD cref(DWORD webhex) {  /* 0xRRGGBB -> COLORREF 0x00BBGGRR */
  return ((webhex >> 16) & 0xFF) | (webhex & 0xFF00) | ((webhex & 0xFF) << 16);
}
static void read_theme(void) {
  WCHAR buf[64]; int i;
  read_first_line(L"\\Nand2\\P47THEME.TXT", buf, 64);
  g_themeIdx = 0;
  for (i = 0; i < 4; i++) if (!wcmp(buf, g_themes[i].key)) { g_themeIdx = i; break; }
}
static void write_theme_file(void) {
  HANDLE h = CreateFileW(L"\\Nand2\\P47THEME.TXT", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == (HANDLE)-1) return;
  DWORD wr = 0;
  static const WCHAR bom = 0xFEFF;
  WriteFile(h, &bom, 2, &wr, NULL);
  const char *k = g_themes[g_themeIdx].key;
  while (*k) { WCHAR c = (WCHAR)(*k++); WriteFile(h, &c, 2, &wr, NULL); }
  { WCHAR e[2]; e[0] = L'\r'; e[1] = L'\n'; WriteFile(h, e, 4, &wr, NULL); }
  CloseHandle(h);
}

/* ---------------- terminal font ----------------
 * MS Gothic (fixed pitch, ASCII + full Japanese) lives in
 * \Nand\msgothic.ttc (stock SHARP font file, verified by parsing
 * its name table). AddFontResourceW registers it for this process
 * only -- no registry writes, no \Windows changes, nothing
 * persistent, so the stock dictionary shell cannot be affected.
 * lfQuality requests ClearType (5) with antialiased (4) fallback
 * semantics: GDI uses the best smoothing it supports. */
static HANDLE g_hFont;
static int g_lineH = 22, g_charW = 9, g_charW2 = 18;
static void setup_font(void) {
  int added = AddFontResourceW(L"\\Nand\\msgothic.ttc");
  log_s("AddFontResourceW(msgothic.ttc) -> "); log_dec((DWORD)added); log_nl();
  LOGFONT lf;
  for (unsigned k = 0; k < sizeof(lf) / 4; k++) ((DWORD *)&lf)[k] = 0;
  lf.lfHeight = -20;
  lf.lfWeight = 700;               /* bold: thin strokes were unreadable */
  lf.lfCharSet = 128;              /* SHIFTJIS_CHARSET */
  lf.lfOutPrecision = 0;
  lf.lfClipPrecision = 0;
  lf.lfQuality = 5;                /* CLEARTYPE_QUALITY */
  lf.lfPitchAndFamily = 0x31;      /* FIXED_PITCH | FF_MODERN */
  { const char *nm = "MS Gothic"; int i = 0;
    while (nm[i]) { lf.lfFaceName[i] = (WCHAR)nm[i]; i++; } lf.lfFaceName[i] = 0; }
  g_hFont = CreateFontIndirectW(&lf);
  log_s("CreateFontIndirectW(MS Gothic -20 bold) -> "); log_hex((DWORD)g_hFont); log_nl();
  if (g_hFont) {
    HANDLE dc = GetDC(NULL);
    if (dc) {
      HANDLE old = SelectObject(dc, g_hFont);
      TEXTMETRIC tm;
      if (GetTextMetricsW(dc, &tm)) {
        log_s("TEXTMETRIC h="); log_dec((DWORD)tm.tmHeight);
        log_s(" aveW="); log_dec((DWORD)tm.tmAveCharWidth);
        log_s(" maxW="); log_dec((DWORD)tm.tmMaxCharWidth);
        log_s(" pitchFam="); log_hex((DWORD)tm.tmPitchAndFamily); log_nl();
        if (tm.tmHeight >= 8 && tm.tmHeight <= 64) {
          g_lineH = tm.tmHeight;   /* line pitch == cell height, no extra leading */
          if (tm.tmAveCharWidth > 0) g_charW = tm.tmAveCharWidth;
          if (tm.tmMaxCharWidth > g_charW) g_charW2 = tm.tmMaxCharWidth;
          else g_charW2 = g_charW * 2;
        }
      }
      { WCHAR face[64]; face[0] = 0;
        if (GetTextFaceW(dc, 64, face)) { log_s("actual face = "); log_ws(face); log_nl(); } }
      SelectObject(dc, old);
      ReleaseDC(NULL, dc);
    }
  }
  write_out();
}

/* ---------------- driver DLL self-install ----------------
 * The terminal carries an embedded copy of BrainCon.dll and
 * installs it before registering the driver, so the additional-app
 * set on the card is all a user needs: nothing has to be copied to
 * \Nand2 by hand. (Originally the DLL had to be placed on \Nand2
 * out of band; with only the card set present, registration pointed
 * at a nonexistent file and activation always failed with
 * "DRIVER ACTIVATION FAILED".) */
#include "braincon_embed.h"

static WCHAR g_dllpath[280];

static void wstr_copy(WCHAR *dst, const WCHAR *src) {
  while ((*dst++ = *src++)) {}
}
static int wstr_len(const WCHAR *s) { int n = 0; while (s[n]) n++; return n; }

static long file_size_of(const WCHAR *path) {
  HANDLE h = CreateFileW(path, GENERIC_READ, 1, NULL, OPEN_EXISTING, 0, NULL);
  if (h == (HANDLE)-1) return -1;
  DWORD sz = GetFileSize(h, NULL);
  CloseHandle(h);
  return (long)sz;
}

static int write_embedded_dll(const WCHAR *path) {
  HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == (HANDLE)-1) {
    log_s("dll install: open failed le="); log_dec(GetLastError()); log_nl();
    write_out();
    return 0;
  }
  DWORD off = 0; int ok = 1;
  while (off < (DWORD)sizeof(g_braincon_dll)) {
    DWORD chunk = (DWORD)sizeof(g_braincon_dll) - off;
    if (chunk > 8192) chunk = 8192;
    DWORD w = 0;
    if (!WriteFile(h, g_braincon_dll + off, chunk, &w, NULL) || w == 0) { ok = 0; break; }
    off += w;
  }
  CloseHandle(h);
  log_s("dll install: wrote "); log_dec(off);
  log_s(ok ? " bytes to " : " bytes (FAILED) to ");
  log_ws(path); log_nl(); write_out();
  return ok && off == (DWORD)sizeof(g_braincon_dll);
}

static void ensure_driver_dll(void) {
  wstr_copy(g_dllpath, L"\\Nand2\\BrainCon.dll");
  if (file_size_of(g_dllpath) == (long)sizeof(g_braincon_dll)) {
    log_s("dll install: already present at "); log_ws(g_dllpath);
    log_nl(); write_out();
    return;
  }
  if (write_embedded_dll(g_dllpath)) return;
  /* fallback: beside the exe (its card folder) */
  {
    WCHAR mod[280];
    DWORD n = GetModuleFileNameW(NULL, mod, 280);
    if (n && n < 260) {
      int i = (int)n - 1;
      while (i >= 0 && mod[i] != L'\\') i--;
      mod[i + 1] = 0;
      wstr_copy(g_dllpath, mod);
      { int j = wstr_len(g_dllpath); const WCHAR *p = L"BrainCon.dll";
        while (*p) g_dllpath[j++] = *p++;
        g_dllpath[j] = 0; }
      if (file_size_of(g_dllpath) == (long)sizeof(g_braincon_dll) ||
          write_embedded_dll(g_dllpath)) return;
    }
  }
  /* last resort: keep the default path; activation reports the error */
  wstr_copy(g_dllpath, L"\\Nand2\\BrainCon.dll");
  log_s("dll install: FAILED everywhere"); log_nl(); write_out();
}

/* ---------------- driver registration ---------------- */
static HANDLE g_hDev;      /* ActivateDeviceEx token */
static HANDLE g_hTerm;     /* our CON1: handle for IOCTLs */
static PROCESS_INFORMATION g_shell;

static void reg_set_sz(HANDLE hk, const WCHAR *name, const WCHAR *val) {
  DWORD n = 0; const WCHAR *p = val; while (*p++) n++;
  RegSetValueExW(hk, name, 0, REG_SZ, (const BYTE *)val, (n + 1) * 2);
}
static void reg_set_dw(HANDLE hk, const WCHAR *name, DWORD val) {
  RegSetValueExW(hk, name, 0, REG_DWORD, (const BYTE *)&val, 4);
}

static int activate_driver(void) {
  HANDLE hk = NULL; DWORD disp = 0;
  ensure_driver_dll();
  long rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Drivers\\BuiltIn\\BrainCon",
                            0, NULL, 0, 0, NULL, &hk, &disp);
  log_s("RegCreateKeyEx BrainCon rc="); log_dec((DWORD)rc); log_nl();
  if (rc == 0 && hk) {
    reg_set_sz(hk, L"Dll", g_dllpath);
    reg_set_sz(hk, L"Prefix", L"CON");
    reg_set_dw(hk, L"Index", 1);
    reg_set_dw(hk, L"Order", 0);
    reg_set_dw(hk, L"Context", 0);
    reg_set_dw(hk, L"Flags", 0);
    RegCloseKey(hk);
  }
  log_s("calling ActivateDeviceEx..."); log_nl(); write_out();
  g_hDev = ActivateDeviceEx(L"Drivers\\BuiltIn\\BrainCon", NULL, 0, NULL);
  log_s("ActivateDeviceEx -> "); log_hex((DWORD)g_hDev);
  log_s(" le="); log_dec(GetLastError()); log_nl(); write_out();
  if (!g_hDev) {
    BOOL ok = RegisterDevice(L"CON", 1, g_dllpath, 0);
    log_s("RegisterDevice fallback -> "); log_dec((DWORD)ok);
    log_s(" le="); log_dec(GetLastError()); log_nl();
    if (!ok) return 0;
  }
  g_hTerm = CreateFileW(L"CON1:", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                        OPEN_EXISTING, 0, NULL);
  log_s("open CON1: (term) -> "); log_hex((DWORD)g_hTerm);
  log_s(" le="); log_dec(GetLastError()); log_nl();
  if (g_hTerm == (HANDLE)-1) { g_hTerm = NULL; return 0; }
  CON_INFO ci; DWORD used = 0;
  if (DeviceIoControl(g_hTerm, IOCTL_CON_GETINFO, NULL, 0, &ci, sizeof(ci), &used, NULL)) {
    log_s("GETINFO ver="); log_hex(ci.version);
    log_s(" opens="); log_dec(ci.openCount);
    log_s(" inCap="); log_dec(ci.inputCap);
    log_s(" outCap="); log_dec(ci.outputCap); log_nl();
  } else {
    log_s("GETINFO failed le="); log_dec(GetLastError()); log_nl();
  }
  write_out();
  return 1;
}

/* ---------------- screen model ---------------- */
#define COLS 120
#define ROWS 400
static WCHAR g_rows[ROWS][COLS];
static int g_curRow, g_curCol, g_topRow;
static int g_curX;           /* cursor visual column (fullwidth = 2) */
static int g_cols = 76;      /* wrap width: visible columns (from WM_SIZE) */
static int g_viewOff;        /* scrollback: lines the view is above the tail */
static int udiv(int a, int b) {
  /* shift-based long division (this toolchain has no runtime
   * divider, and a plain subtraction loop gets idiom-recognised
   * back into one). Operands here are tiny (< 2^18). */
  int q = 0, shift = 0, bb = b;
  if (b <= 0 || a < b) return 0;
  while ((bb << 1) <= a && shift < 24) { bb <<= 1; shift++; }
  for (;;) {
    if (a >= bb) { a -= bb; q += (1 << shift); }
    if (shift == 0) break;
    bb >>= 1; shift--;
  }
  return q;
}
static int is_wide(WCHAR c) {  /* fullwidth cell (2 units) */
  return !(c < 0x80 || (c >= 0xFF61 && c <= 0xFF9F));
}
static void scr_newline(void) {
  g_curRow++; g_curCol = 0; g_curX = 0;
  if (g_curRow >= ROWS) { /* scroll by one line within the ring area */
    int i, j;
    for (i = 0; i < ROWS - 1; i++)
      for (j = 0; j < COLS; j++) g_rows[i][j] = g_rows[i + 1][j];
    g_curRow = ROWS - 1;
  }
  {
    int j; for (j = 0; j < COLS; j++) g_rows[g_curRow][j] = 0;
  }
}
static void scr_putc(WCHAR c) {
  if (c == L'\r') { g_curCol = 0; g_curX = 0; return; }
  if (c == L'\n') { scr_newline(); return; }
  if (c == L'\b') {
    if (g_curCol > 0) { g_curCol--; g_curX -= is_wide(g_rows[g_curRow][g_curCol]) ? 2 : 1; if (g_curX < 0) g_curX = 0; }
    return;
  }
  if (c == L'\t') { do { scr_putc(L' '); } while (g_curX % 8); return; }
  if (c < 32) return;
  { int w = is_wide(c) ? 2 : 1;   /* wrap at the last visible column */
    if (g_curX + w > g_cols || g_curCol >= COLS) scr_newline();
    g_rows[g_curRow][g_curCol++] = c; g_curX += w; }
}
/* The console stream is bytes in the image's ANSI codepage (CP932):
 * CRT stdio consumers convert through it, so decode it properly --
 * ASCII passes through, SJIS pairs and halfwidth katakana become
 * real WCHARs. A lead byte split across two drains is held over. */
static BYTE g_lead; static int g_hasLead;
static void feed_mb(const char *mb, int n) {
  WCHAR w = 0;
  if (MultiByteToWideChar(CP_SJIS, 0, mb, n, &w, 1) == 1) scr_putc(w);
}
static void scr_feed(const BYTE *p, DWORD n) {
  DWORD i;
  for (i = 0; i < n; i++) {
    BYTE b = p[i];
    if (g_hasLead) {
      char pair[2]; pair[0] = (char)g_lead; pair[1] = (char)b;
      g_hasLead = 0; feed_mb(pair, 2); continue;
    }
    if ((b >= 0x81 && b <= 0x9F) || (b >= 0xE0 && b <= 0xFC)) { g_lead = b; g_hasLead = 1; continue; }
    if (b < 0x80) scr_putc((WCHAR)b);
    else { char s = (char)b; feed_mb(&s, 1); }
  }
}

/* ---------------- terminal machinery ---------------- */
static HANDLE g_hwnd;
static int g_auto;             /* auto-feed the proof script (mode term) */
static DWORD g_t0;             /* window-up tick */
static DWORD g_shellTick;      /* shell launch tick (0 = not launched) */
static int g_fed1, g_fed2, g_fed3, g_exitLogged;
static int g_blink;
static int g_focusLogged, g_charCount, g_rectAsserted;
static char g_status[128];

static void log_window_rects(HANDLE hwnd, const char *tag) {
  RECT wr, cr;
  if (GetWindowRect(hwnd, &wr)) {
    log_s(tag); log_s(" winRect=");
    log_dec((DWORD)wr.left); log_ch(L','); log_dec((DWORD)wr.top); log_ch(L'-');
    log_dec((DWORD)wr.right); log_ch(L','); log_dec((DWORD)wr.bottom);
  }
  if (GetClientRect(hwnd, &cr)) {
    log_s(" client=");
    log_dec((DWORD)(cr.right - cr.left)); log_ch(L'x'); log_dec((DWORD)(cr.bottom - cr.top));
  }
  log_nl(); write_out();
}

static void status_set(const char *s) {
  int i = 0; while (s[i] && i < 120) { g_status[i] = s[i]; i++; } g_status[i] = 0;
}

static void put_input(const BYTE *s, DWORD nbytes) {
  DWORD used = 0;
  DeviceIoControl(g_hTerm, IOCTL_CON_PUTINPUT, (void *)s, nbytes,
                  NULL, 0, &used, NULL);
}
static void put_line(const char *ascii) {
  BYTE buf[64]; int i = 0;
  while (ascii[i] && i < 60) { buf[i] = (BYTE)ascii[i]; i++; }
  buf[i++] = '\r'; buf[i++] = '\n';
  put_input(buf, (DWORD)i);
}

static void log_stdio_path(DWORD id, const char *tag) {
  WCHAR buf[128]; DWORD n = 128;
  buf[0] = 0;
  BOOL ok = GetStdioPathW(id, buf, &n);
  log_s(tag); log_s(" id="); log_hex(id);
  log_s(" ok="); log_dec((DWORD)ok); log_s(" path=");
  if (ok) log_ws(buf); else { log_s("(le="); log_dec(GetLastError()); log_ch(L')'); }
  log_nl();
}

/* CE stdio redirection: a process's stdin/out/err are bound to PATHS
 * (SetStdioPathW), and children inherit the paths -- STARTUPINFO std
 * handles are ignored by this kernel (proven in the p47 probes).
 * Point all three streams at the console device, then launch. */
static void launch_shell(void) {
  log_stdio_path(0, "stdio before");
  log_stdio_path(0xFFFFFFF6UL, "stdio before"); /* STD_INPUT_HANDLE */
  DWORD ids[3];
  ids[0] = 0xFFFFFFF6UL; ids[1] = 0xFFFFFFF7UL; ids[2] = 0xFFFFFFF8UL;
  int i;
  for (i = 0; i < 3; i++) {
    BOOL ok = SetStdioPathW(ids[i], L"\\CON1:");
    log_s("SetStdioPathW("); log_hex(ids[i]); log_s(", \\CON1:) ok=");
    log_dec((DWORD)ok); log_s(" le="); log_dec(GetLastError()); log_nl();
  }
  for (i = 0; i < 3; i++) {
    BOOL ok = SetStdioPathW((DWORD)i, L"\\CON1:");
    log_s("SetStdioPathW("); log_dec((DWORD)i); log_s(", \\CON1:) ok=");
    log_dec((DWORD)ok); log_s(" le="); log_dec(GetLastError()); log_nl();
  }
  log_stdio_path(0, "stdio after");
  log_stdio_path(1, "stdio after");
  log_stdio_path(2, "stdio after");
  write_out();
  STARTUPINFO si;
  for (unsigned k = 0; k < sizeof(si) / 4; k++) ((DWORD *)&si)[k] = 0;
  si.cb = sizeof(si);
  BOOL ok = CreateProcessW(L"\\Windows\\shell.exe", L"-c", NULL, NULL, FALSE,
                           0, NULL, NULL, &si, &g_shell);
  log_s("CreateProcessW(shell.exe -c, stdio paths) ok="); log_dec((DWORD)ok);
  log_s(" le="); log_dec(GetLastError());
  if (ok) { log_s(" pid="); log_dec(g_shell.dwProcessId); }
  log_nl(); write_out();
  g_shellTick = GetTickCount();
}

static void pump(void) {
  if (!g_hTerm) return;
  DWORD timeout = 0, used = 0;
  BOOL any = DeviceIoControl(g_hTerm, IOCTL_CON_WAITOUTPUT, &timeout, 4,
                              NULL, 0, &used, NULL);
  if (any) {
    static BYTE buf[8192];
    DWORD got = 0;
    if (DeviceIoControl(g_hTerm, IOCTL_CON_GETOUTPUT, NULL, 0,
                        buf, sizeof(buf), &got, NULL) && got) {
      static int dumped, nchunks;
      if (nchunks < 48) {
        DWORD i;
        log_s("chunk["); log_dec((DWORD)nchunks); log_s("] bytes="); log_dec(got); log_s(" hex:");
        for (i = 0; i < got && i < 24; i++) {
          DWORD b = buf[i];
          log_ch((WCHAR)(' '));
          log_ch((WCHAR)(b >> 4 < 10 ? '0' + (b >> 4) : 'a' + (b >> 4) - 10));
          log_ch((WCHAR)((b & 15) < 10 ? '0' + (b & 15) : 'a' + (b & 15) - 10));
        }
        log_nl(); write_out();
      }
      nchunks++;
      if (!dumped) {
        dumped = 1;
        DWORD i;
        log_s("first output chunk bytes="); log_dec(got); log_s(" hex:");
        for (i = 0; i < got && i < 96; i++) {
          DWORD b = buf[i];
          log_ch((WCHAR)(' '));
          log_ch((WCHAR)(b >> 4 < 10 ? '0' + (b >> 4) : 'a' + (b >> 4) - 10));
          log_ch((WCHAR)((b & 15) < 10 ? '0' + (b & 15) : 'a' + (b & 15) - 10));
        }
        log_nl(); write_out();
      }
      scr_feed(buf, got);
      g_viewOff = 0;   /* new output: follow the tail */
      InvalidateRect(g_hwnd, NULL, FALSE);
    }
  }
  if (g_shellTick && g_auto) {
    DWORD now = GetTickCount();
    if (!g_fed1 && now - g_shellTick > 6000) {
      g_fed1 = 1; put_line("gi proc");
      log_s("fed: gi proc"); log_nl(); write_out();
    }
    if (!g_fed2 && now - g_shellTick > 20000) {
      g_fed2 = 1; put_line("mi");
      log_s("fed: mi"); log_nl(); write_out();
    }
    if (!g_fed3 && now - g_shellTick > 34000) {
      g_fed3 = 1; put_line("gi thrd");
      log_s("fed: gi thrd"); log_nl(); write_out();
    }
  }
  if (g_shellTick) {
    if (!g_exitLogged) {
      DWORD ec = 0;
      if (GetExitCodeProcess(g_shell.hProcess, &ec) && ec != STILL_ACTIVE) {
        g_exitLogged = 1;
        log_s("shell EXITED code="); log_dec(ec); log_nl(); write_out();
        status_set("shell exited");
      }
    }
  }
  { /* cursor blink: repaint about twice a second */
    static int tick;
    if ((++tick & 3) == 0) { g_blink = !g_blink; InvalidateRect(g_hwnd, NULL, FALSE); }
  }
  if (!g_rectAsserted && g_shellTick) {  /* the shell frame may have
    repositioned us at activation: assert the fitting rect once more
    (only if we were actually moved -- an unnecessary MoveWindow
    would leave an unrepainted ghost of the old position, p65) */
    g_rectAsserted = 1;
    RECT wr;
    if (GetWindowRect(g_hwnd, &wr) &&
        (wr.left != 96 || wr.top != 12 || wr.right != 784 || wr.bottom != 464)) {
      MoveWindow(g_hwnd, 96, 12, 688, 452, TRUE);
      log_window_rects(g_hwnd, "after-assert");
    }
  }
}

/* In-process driver test: load BrainCon.dll into THIS process and
 * call its stream entry points directly. Isolates DLL soundness
 * (exports, relocations, logic) from device.exe loading issues. */
typedef DWORD (*PFN_INIT)(const WCHAR *, DWORD);
typedef BOOL (*PFN_DEINIT)(DWORD);
typedef DWORD (*PFN_OPEN)(DWORD, DWORD, DWORD);
typedef BOOL (*PFN_CLOSE)(DWORD);
typedef DWORD (*PFN_RW)(DWORD, void *, DWORD);
typedef BOOL (*PFN_IOCTL)(DWORD, DWORD, BYTE *, DWORD, BYTE *, DWORD, DWORD *);
static void dlltest(void) {
  ensure_driver_dll();
  log_s("dlltest: LoadLibrary BrainCon.dll"); log_nl(); write_out();
  HANDLE h = LoadLibraryW(g_dllpath);
  log_s("LoadLibrary -> "); log_hex((DWORD)h); log_s(" le="); log_dec(GetLastError()); log_nl(); write_out();
  if (!h) return;
  PFN_INIT pInit = (PFN_INIT)GetProcAddressW(h, L"CON_Init");
  PFN_OPEN pOpen = (PFN_OPEN)GetProcAddressW(h, L"CON_Open");
  PFN_RW pWrite = (PFN_RW)GetProcAddressW(h, L"CON_Write");
  PFN_RW pRead = (PFN_RW)GetProcAddressW(h, L"CON_Read");
  PFN_IOCTL pIoctl = (PFN_IOCTL)GetProcAddressW(h, L"CON_IOControl");
  PFN_CLOSE pClose = (PFN_CLOSE)GetProcAddressW(h, L"CON_Close");
  PFN_DEINIT pDeinit = (PFN_DEINIT)GetProcAddressW(h, L"CON_Deinit");
  log_s("GPA: Init="); log_hex((DWORD)pInit); log_s(" Open="); log_hex((DWORD)pOpen);
  log_s(" Write="); log_hex((DWORD)pWrite); log_s(" Ioctl="); log_hex((DWORD)pIoctl); log_nl(); write_out();
  if (!pInit || !pOpen || !pWrite || !pIoctl) { log_s("dlltest: missing exports"); log_nl(); write_out(); return; }
  DWORD dcx = pInit(L"Drivers\\BuiltIn\\BrainCon", 0);
  log_s("CON_Init -> "); log_hex(dcx); log_nl(); write_out();
  if (!dcx) return;
  DWORD ocx = pOpen(dcx, GENERIC_READ | GENERIC_WRITE, 0);
  log_s("CON_Open -> "); log_hex(ocx); log_nl();
  static const char m[] = "INPROC-TEST 6789\r\n";
  DWORD wr = pWrite(ocx, (void *)m, sizeof(m) - 1);
  log_s("CON_Write -> "); log_dec(wr); log_nl();
  CON_INFO ci; DWORD used = 0;
  BOOL ok = pIoctl(ocx, IOCTL_CON_GETINFO, NULL, 0, (BYTE *)&ci, sizeof(ci), &used);
  log_s("GETINFO ok="); log_dec((DWORD)ok);
  if (ok) { log_s(" outQ="); log_dec(ci.outputQueued); log_s(" opens="); log_dec(ci.openCount); }
  log_nl();
  static BYTE drain[128]; DWORD got = 0;
  ok = pIoctl(ocx, IOCTL_CON_GETOUTPUT, NULL, 0, drain, sizeof(drain), &got);
  log_s("GETOUTPUT ok="); log_dec((DWORD)ok); log_s(" bytes="); log_dec(got); log_s(" text=");
  { DWORD i; for (i = 0; i < got; i++) {
      BYTE c = drain[i]; if (c >= 32 && c < 127) log_ch((WCHAR)c); } }
  log_nl();
  /* input roundtrip in-process */
  static const char inp[] = "ping\r\n";
  DWORD u2 = 0;
  pIoctl(ocx, IOCTL_CON_PUTINPUT, (BYTE *)inp, sizeof(inp) - 1, NULL, 0, &u2);
  static BYTE rb[32]; DWORD rd = pRead(ocx, rb, sizeof(rb));
  log_s("CON_Read after PUTINPUT -> "); log_dec(rd); log_s(" text=");
  { DWORD i; for (i = 0; i < rd && i < 30; i++) {
      BYTE c = rb[i]; if (c >= 32 && c < 127) log_ch((WCHAR)c); } }
  log_nl(); write_out();
  if (pClose) pClose(ocx);
  if (pDeinit) pDeinit(dcx);
  FreeLibrary(h);
  log_s("dlltest done"); log_nl(); write_out();
}

/* ---------------- selftest ---------------- */
static void selftest(void) {
  log_s("P47 selftest"); log_nl();
  dlltest();
  if (!activate_driver()) { log_s("driver activation FAILED"); log_nl(); write_out(); return; }
  HANDLE hApp = CreateFileW(L"CON1:", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
  HANDLE hApp2 = CreateFileW(L"CON1:", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
  log_s("multi-open: hApp="); log_hex((DWORD)hApp); log_s(" hApp2="); log_hex((DWORD)hApp2); log_nl();
  /* 1. write via an app handle, drain via the terminal channel */
  static const char msg[] = "SELFTEST-OUTPUT 12345\r\n";
  DWORD wr = 0;
  BOOL okw = WriteFile(hApp, msg, sizeof(msg) - 1, &wr, NULL);
  log_s("WriteFile(app) ok="); log_dec((DWORD)okw); log_s(" bytes="); log_dec(wr); log_nl();
  CON_INFO ci; DWORD used = 0;
  DeviceIoControl(g_hTerm, IOCTL_CON_GETINFO, NULL, 0, &ci, sizeof(ci), &used, NULL);
  log_s("after write: opens="); log_dec(ci.openCount);
  log_s(" outQueued="); log_dec(ci.outputQueued); log_nl();
  static BYTE drain[256]; DWORD got = 0;
  DeviceIoControl(g_hTerm, IOCTL_CON_GETOUTPUT, NULL, 0, drain, sizeof(drain), &got, NULL);
  log_s("GETOUTPUT bytes="); log_dec(got); log_s(" text=");
  { DWORD i; for (i = 0; i < got; i++) {
      BYTE c = drain[i]; if (c >= 32 && c < 127) log_ch((WCHAR)c); } }
  log_nl();
  /* 2. input: PUTINPUT then ReadFile on the app handle */
  DWORD to = 300, u2 = 0;
  DeviceIoControl(hApp, IOCTL_CON_SETREADTIMEOUT, &to, 4, NULL, 0, &u2, NULL);
  static BYTE rbuf[64]; DWORD rd = 0;
  BOOL okr = ReadFile(hApp, rbuf, sizeof(rbuf), &rd, NULL);
  log_s("ReadFile(empty,300ms) ok="); log_dec((DWORD)okr);
  log_s(" rd="); log_dec(rd); log_s(" le="); log_dec(GetLastError()); log_nl();
  put_input((const BYTE *)"hello\r\n", 7);
  okr = ReadFile(hApp, rbuf, sizeof(rbuf), &rd, NULL);
  log_s("ReadFile(after PUTINPUT) ok="); log_dec((DWORD)okr);
  log_s(" rd="); log_dec(rd); log_s(" text=");
  { DWORD i; for (i = 0; i < rd; i++) {
      BYTE c = rbuf[i]; if (c >= 32 && c < 127) log_ch((WCHAR)c); } }
  log_nl();
  /* drain the echo of "hello" from the output ring */
  DeviceIoControl(g_hTerm, IOCTL_CON_GETOUTPUT, NULL, 0, drain, sizeof(drain), &got, NULL);
  log_s("echo drained bytes="); log_dec(got); log_nl();
  /* 3. seek must fail */
  extern DWORD SetFilePointer(HANDLE, long, long *, DWORD);
  DWORD sp = SetFilePointer(hApp, 0, NULL, 0);
  log_s("SetFilePointer -> "); log_hex(sp); log_s(" le="); log_dec(GetLastError()); log_nl();
  CloseHandle(hApp); CloseHandle(hApp2);
  DeviceIoControl(g_hTerm, IOCTL_CON_GETINFO, NULL, 0, &ci, sizeof(ci), &used, NULL);
  log_s("final: opens="); log_dec(ci.openCount);
  log_s(" totalOpens="); log_dec(ci.totalOpens);
  log_s(" dropped="); log_dec(ci.droppedBytes); log_nl();
  log_s("selftest done"); log_nl(); write_out();
}

/* ---------------- window ----------------
 * A normal overlapped window: the system draws the caption and the
 * sizable frame, we paint only the client area -- themed background,
 * the scrollback grid in MS Gothic, a blinking block cursor, and a
 * status strip along the bottom edge (theme name + shell state). */
#define PAD_X 8
#define PAD_Y 6
#define STATUS_H 20
static int g_visRows = 10;   /* recomputed on WM_SIZE / paint */
static void recompute_grid(int clientW, int clientH) {
  int c = udiv(clientW - PAD_X * 2, g_charW);
  if (c < 16) c = 16; if (c > COLS) c = COLS;
  g_cols = c;
  g_visRows = udiv(clientH - STATUS_H - PAD_Y, g_lineH);
  if (g_visRows < 1) g_visRows = 1;
}
static long __attribute__((stdcall)) wndproc(HANDLE hwnd, DWORD msg, DWORD wp, long lp) {
  if (msg == WM_PAINT) {
    const THEME *th = &g_themes[g_themeIdx];
    PAINTSTRUCT ps;
    HANDLE hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    HANDLE br = CreateSolidBrush(cref(th->bg));
    FillRect(hdc, &rc, br); DeleteObject(br);
    HANDLE oldFont = NULL;
    if (g_hFont) oldFont = SelectObject(hdc, g_hFont);
    /* text grid: fixed cells. Each row is drawn with ExtTextOutW
     * plus an explicit per-character advance array (lpDx), so every
     * glyph origin sits exactly on the cell grid regardless of the
     * font's natural advances; ETO_OPAQUE fills the row band with
     * the background colour. Line pitch == cell height. */
    SetTextColor(hdc, cref(th->fg)); SetBkColor(hdc, cref(th->bg));
    {
      recompute_grid(rc.right, rc.bottom);
      int endRow = g_curRow - g_viewOff; if (endRow < 0) endRow = 0;
      int first = endRow - (g_visRows - 1); if (first < 0) first = 0;
      int r, y = PAD_Y;
      static int dx[COLS];
      for (r = first; r <= endRow; r++, y += g_lineH) {
        RECT tr; tr.left = 0; tr.top = y; tr.right = rc.right; tr.bottom = y + g_lineH;
        int len = 0; while (len < COLS && g_rows[r][len]) len++;
        int i; for (i = 0; i < len; i++) dx[i] = is_wide(g_rows[r][i]) ? g_charW2 : g_charW;
        if (len) ExtTextOutW(hdc, PAD_X, y, ETO_OPAQUE, &tr, g_rows[r], (DWORD)len, dx);
        else { HANDLE eb = CreateSolidBrush(cref(th->bg)); FillRect(hdc, &tr, eb); DeleteObject(eb); }
      }
      /* block cursor on the cursor cell (only at the tail view) */
      if (g_viewOff == 0 && g_blink) {
        int cx = PAD_X + g_curX * g_charW;
        int cy = PAD_Y + (g_curRow - first) * g_lineH;
        int cw = (g_curCol < COLS && is_wide(g_rows[g_curRow][g_curCol])) ? g_charW2 : g_charW;
        RECT cr; cr.left = cx; cr.top = cy; cr.right = cx + cw; cr.bottom = cy + g_lineH;
        HANDLE cb = CreateSolidBrush(cref(th->accent));
        FillRect(hdc, &cr, cb); DeleteObject(cb);
        WCHAR under = (g_curCol < COLS && g_rows[g_curRow][g_curCol]) ? g_rows[g_curRow][g_curCol] : L' ';
        SetTextColor(hdc, cref(th->bg)); SetBkColor(hdc, cref(th->accent));
        { int d1 = cw; ExtTextOutW(hdc, cx, cy, ETO_OPAQUE, &cr, &under, 1, &d1); }
        SetTextColor(hdc, cref(th->fg)); SetBkColor(hdc, cref(th->bg));
      }
      /* scrollbar: track + thumb on the right edge when scrolled
       * or when the scrollback exceeds the view */
      {
        int total = g_curRow + 1;
        if (total > g_visRows) {
          int trackTop = PAD_Y, trackBot = rc.bottom - STATUS_H - 2;
          int trackH = trackBot - trackTop;
          RECT tk; tk.left = rc.right - 7; tk.top = trackTop; tk.right = rc.right - 2; tk.bottom = trackBot;
          HANDLE tb = CreateSolidBrush(cref(th->panel));
          FillRect(hdc, &tk, tb); DeleteObject(tb);
          int thumbH = udiv(trackH * g_visRows, total); if (thumbH < 14) thumbH = 14;
          int range = total - g_visRows; if (range < 1) range = 1;
          int thumbY = trackTop + udiv((trackH - thumbH) * first, range);
          RECT tm; tm.left = tk.left; tm.top = thumbY; tm.right = tk.right; tm.bottom = thumbY + thumbH;
          HANDLE mb = CreateSolidBrush(cref(th->accent));
          FillRect(hdc, &tm, mb); DeleteObject(mb);
        }
      }
    }
    if (oldFont) SelectObject(hdc, oldFont);
    /* status strip (system font, ASCII only) */
    {
      RECT sr; sr.left = 0; sr.top = rc.bottom - STATUS_H; sr.right = rc.right; sr.bottom = rc.bottom;
      HANDLE br2 = CreateSolidBrush(cref(th->panel));
      FillRect(hdc, &sr, br2); DeleteObject(br2);
      SetTextColor(hdc, cref(th->panelFg)); SetBkColor(hdc, cref(th->panel));
      WCHAR st[192]; int i = 0;
      const char *pfx = "BrainCon terminal  [";
      while (pfx[i]) { st[i] = (WCHAR)pfx[i]; i++; }
      const char *tn = th->label;
      { int j = 0; while (tn[j] && i < 60) st[i++] = (WCHAR)tn[j++]; }
      st[i++] = L']'; st[i++] = L' '; st[i++] = L' ';
      { int j = 0; while (g_status[j] && i < 180) st[i++] = (WCHAR)g_status[j++]; }
      if (g_viewOff > 0 && i < 170) {  /* scrollback indicator */
        const char *pfx = "  [-";
        int j = 0; while (pfx[j]) st[i++] = (WCHAR)pfx[j++];
        char num[8]; int n = 0, v = g_viewOff;
        if (!v) num[n++] = '0';
        while (v) { num[n++] = (char)('0' + (v % 10)); v /= 10; }
        while (n) st[i++] = (WCHAR)num[--n];
        st[i++] = L']';
      }
      st[i] = 0;
      RECT tr; tr.left = 6; tr.top = rc.bottom - STATUS_H + 3; tr.right = rc.right - 6; tr.bottom = rc.bottom;
      DrawTextW(hdc, st, -1, &tr, 0);
    }
    EndPaint(hwnd, &ps);
    return 0;
  }
  if (msg == WM_SIZE) {
    recompute_grid((int)(lp & 0xFFFF), (int)((lp >> 16) & 0xFFFF));
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }
  if (msg == 0x0007) {  /* WM_SETFOCUS */
    if (!g_focusLogged) { g_focusLogged = 1; log_s("WM_SETFOCUS received"); log_nl(); write_out(); }
    return 0;
  }
  if (msg == 0x0201) {  /* WM_LBUTTONDOWN: click-to-focus */
    SetFocus(hwnd);
    return 0;
  }
  if (msg == WM_TIMER) { pump(); return 0; }
  if (msg == WM_KEYDOWN) {
    if (wp == VK_F2) {   /* cycle colour theme, persist the choice */
      g_themeIdx = (g_themeIdx + 1) & 3;
      write_theme_file();
      log_s("theme -> "); log_s(g_themes[g_themeIdx].label); log_nl(); write_out();
      InvalidateRect(hwnd, NULL, FALSE);
    } else if (wp == VK_PRIOR) {          /* PageUp: scroll back */
      g_viewOff += g_visRows - 1;
      if (g_viewOff > g_curRow) g_viewOff = g_curRow;
      log_s("PGUP viewOff="); log_dec((DWORD)g_viewOff);
      log_s(" curRow="); log_dec((DWORD)g_curRow);
      log_s(" visRows="); log_dec((DWORD)g_visRows); log_nl(); write_out();
      InvalidateRect(hwnd, NULL, FALSE);
    } else if (wp == VK_NEXT) {           /* PageDown: toward the tail */
      g_viewOff -= g_visRows - 1;
      if (g_viewOff < 0) g_viewOff = 0;
      log_s("PGDN viewOff="); log_dec((DWORD)g_viewOff);
      log_s(" curRow="); log_dec((DWORD)g_curRow); log_nl(); write_out();
      InvalidateRect(hwnd, NULL, FALSE);
    }
    return 0;
  }
  if (msg == WM_CHAR) {
    WCHAR wc = (WCHAR)wp;
    if (g_charCount < 6) {
      log_s("WM_CHAR["); log_dec((DWORD)g_charCount); log_s("] wc="); log_hex((DWORD)wc); log_nl(); write_out();
    }
    g_charCount++;
    if (g_hTerm) {
      if (wc == L'\r') put_input((const BYTE *)"\r\n", 2);
      else if (wc < 0x80) { BYTE b = (BYTE)wc; put_input(&b, 1); }
      else {  /* Japanese etc.: encode to the console's CP932 stream */
        char mb[4]; int n = WideCharToMultiByte(CP_SJIS, 0, &wc, 1, mb, 4, NULL, NULL);
        if (n > 0) put_input((const BYTE *)mb, (DWORD)n);
      }
    }
    return 0;
  }
  if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

int __attribute__((stdcall)) WinMain(HANDLE hInst, HANDLE hPrev, WCHAR *cmd, int show) {
  (void)hPrev; (void)cmd; (void)show;
  int i, j;
  for (i = 0; i < ROWS; i++) for (j = 0; j < COLS; j++) g_rows[i][j] = 0;
  read_mode();
  read_theme();
  int self = !wcmp(g_mode, "selftest");
  g_auto = !wcmp(g_mode, "term");
  log_s("P47 term app, mode="); log_ws(g_mode);
  log_s(" theme="); log_s(g_themes[g_themeIdx].label); log_nl();
  write_out();
  setup_font();
  log_s("step: window create"); log_nl(); write_out();

  WNDCLASS wc;
  for (unsigned k = 0; k < sizeof(wc) / 4; k++) ((DWORD *)&wc)[k] = 0;
  wc.lpfnWndProc = (void *)wndproc;
  wc.hInstance = hInst;
  wc.lpszClassName = L"P47TermClass";
  RegisterClassW(&wc);
  /* Create the window directly at its final rect. (It used to be
   * created at (80,24) and then MoveWindow'd into place; on this
   * shell the area a window vacates is never repainted, so that one
   * move left a permanent ghost slice of the window on screen --
   * see the p65 report. Creating at the final position leaves no
   * vacated area.) */
  g_hwnd = CreateWindowExW(0, L"P47TermClass", L"BrainCon Terminal",
                           WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           96, 12, 688, 452, NULL, NULL, hInst, NULL);
  ShowWindow(g_hwnd, SW_SHOWNORMAL);
  UpdateWindow(g_hwnd);
  SetForegroundWindow(g_hwnd);
  SetFocus(g_hwnd);
  log_window_rects(g_hwnd, "after-create");
  g_t0 = GetTickCount();
  log_s("step: window up, hwnd="); log_hex((DWORD)g_hwnd); log_nl(); write_out();

  if (self) {
    selftest();
    status_set("selftest done (see P47OUT)");
  } else {
    if (activate_driver()) {
      status_set("driver active, launching shell...");
      launch_shell();
      status_set("shell running on CON1:");
    } else {
      status_set("DRIVER ACTIVATION FAILED");
    }
  }
  write_out();
  SetTimer(g_hwnd, 1, 120, NULL);
  MSG msg;
  while (GetMessageW(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  KillTimer(g_hwnd, 1);
  if (g_hFont) DeleteObject(g_hFont);
  RemoveFontResourceW(L"\\Nand\\msgothic.ttc");
  return 0;
}
