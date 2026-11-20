// BrainTerm -- a self-made command-line shell for the SHARP Brain (WinCE).
// Full-screen terminal window + built-in command interpreter over COREDLL.
// Built with the p20-B/p44 toolchain: clang armv5te ARM-state, freestanding,
// hand-written PE linker, COREDLL ordinal imports; toolhelp.dll is resolved
// at runtime by name (its exports are named, like SharpLib's).
typedef unsigned short WCHAR;
typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef int BOOL;
typedef long LONG;
typedef void *HANDLE, *HMODULE, *FARPROC;
typedef unsigned char BYTE;
#define NULL ((void *)0)
#define TRUE 1
#define FALSE 0

#define GENERIC_READ 0x80000000UL
#define GENERIC_WRITE 0x40000000UL
#define OPEN_EXISTING 3
#define CREATE_ALWAYS 2
#define FILE_ATTRIBUTE_NORMAL 0x80
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFFUL
#define SW_SHOWNORMAL 1
#define WS_POPUP 0x80000000UL
#define WS_VISIBLE 0x10000000UL
#define WS_CAPTION 0x00C00000UL
#define WS_SYSMENU 0x00080000UL
#define WS_THICKFRAME 0x00040000UL
#define WS_MINIMIZEBOX 0x00020000UL
#define WS_MAXIMIZEBOX 0x00010000UL
#define WM_SIZE 0x0005
#define WM_CLOSE 0x0010
#define WM_PAINT 0x000F
#define WM_DESTROY 0x0002
#define WM_CHAR 0x0102
#define WM_KEYDOWN 0x0100
#define WM_TIMER 0x0113
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_MOUSEMOVE 0x0200
#define PM_REMOVE 1
#define TRANSPARENT_BK 1
#define ETO_OPAQUE 2
#define ETO_CLIPPED 4

#define VK_BACK 0x08
#define VK_RETURN 0x0D
#define VK_ESCAPE 0x1B
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_DELETE 0x2E
#define VK_CONTROL 0x11

typedef struct { DWORD cb; WCHAR *lpReserved; WCHAR *lpDesktop; WCHAR *lpTitle;
  DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute,
  dwFlags, wShowWindow, cbReserved2; BYTE *lpReserved2; HANDLE hStdInput,
  hStdOutput, hStdError; } STARTUPINFO;
typedef struct { HANDLE hProcess, hThread; DWORD dwProcessId, dwThreadId; } PROCESS_INFORMATION;
typedef struct { DWORD left, top, right, bottom; } RECT;
typedef struct { HANDLE hdc; BOOL fErase; RECT rcPaint; BOOL fRestore, fIncUpdate; BYTE rgbReserved[32]; } PAINTSTRUCT;
typedef struct { HANDLE hwnd; DWORD message; DWORD wParam; long lParam; DWORD time; long pt_x, pt_y; } MSG;
typedef struct { DWORD style; void *lpfnWndProc; int cbClsExtra, cbWndExtra; HANDLE hInstance;
  HANDLE hIcon, hCursor, hbrBackground; const WCHAR *lpszMenuName, *lpszClassName; } WNDCLASS;
typedef struct { LONG cx, cy; } SIZE;
typedef struct { DWORD dwFileAttributes; DWORD ftCreationTime[2]; DWORD ftLastAccessTime[2];
  DWORD ftLastWriteTime[2]; DWORD nFileSizeHigh, nFileSizeLow; DWORD dwReserved0;
  WCHAR cFileName[260]; WCHAR cAlternateFileName[14]; } WIN32_FIND_DATA;
  // NOTE: on this CE build cFileName sits at offset 40 (only dwReserved0
  // precedes it; offset 36 carries a per-entry FSD id). Verified in-guest
  // by dumping the raw struct (p48): attr@0, sizes@28/32, name@40.
typedef struct { DWORD dwOSVersionInfoSize, dwMajorVersion, dwMinorVersion,
  dwBuildNumber, dwPlatformId; WCHAR szCSDVersion[128]; } OSVERSIONINFO;
typedef struct { DWORD dwLength, dwMemoryLoad, dwTotalPhys, dwAvailPhys,
  dwTotalPageFile, dwAvailPageFile, dwTotalVirtual, dwAvailVirtual; } MEMORYSTATUS;
typedef struct { unsigned short wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; } SYSTEMTIME;
typedef struct { LONG lfHeight, lfWidth, lfEscapement, lfOrientation, lfWeight;
  BYTE lfItalic, lfUnderline, lfStrikeOut, lfCharSet, lfOutPrecision,
  lfClipPrecision, lfQuality, lfPitchAndFamily; WCHAR lfFaceName[32]; } LOGFONT;

extern HANDLE CreateFileW(const WCHAR *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
extern BOOL ReadFile(HANDLE, void *, DWORD, DWORD *, void *);
extern DWORD SetFilePointer(HANDLE, long, long *, DWORD);
extern BOOL CloseHandle(HANDLE);
extern DWORD GetLastError(void);
extern DWORD GetTickCount(void);
extern BOOL CreateProcessW(const WCHAR *, const WCHAR *, void *, void *, BOOL, DWORD, void *, const WCHAR *, STARTUPINFO *, PROCESS_INFORMATION *);
extern void Sleep(DWORD);
extern BOOL GetExitCodeProcess(HANDLE, DWORD *);
extern HANDLE OpenProcess(DWORD, BOOL, DWORD);
extern BOOL TerminateProcess(HANDLE, UINT);
extern HMODULE LoadLibraryW(const WCHAR *);
extern BOOL FreeLibrary(HMODULE);
extern FARPROC GetProcAddressA(HMODULE, const char *);
extern BOOL RegisterClassW(const WNDCLASS *);
extern HANDLE CreateWindowExW(DWORD, const WCHAR *, const WCHAR *, DWORD, int, int, int, int, HANDLE, HANDLE, HANDLE, void *);
extern BOOL ShowWindow(HANDLE, int);
extern BOOL UpdateWindow(HANDLE);
extern BOOL GetMessageW(MSG *, HANDLE, DWORD, DWORD);
extern BOOL PeekMessageW(MSG *, HANDLE, DWORD, DWORD, DWORD);
extern BOOL TranslateMessage(const MSG *);
extern long DispatchMessageW(const MSG *);
extern void PostQuitMessage(int);
extern BOOL DestroyWindow(HANDLE);
extern HANDLE SetFocus(HANDLE);
extern int SetForegroundWindow(HANDLE);
extern long DefWindowProcW(HANDLE, DWORD, DWORD, long);
extern int GetKeyState(int);
extern HANDLE BeginPaint(HANDLE, PAINTSTRUCT *);
extern BOOL EndPaint(HANDLE, const PAINTSTRUCT *);
extern BOOL GetClientRect(HANDLE, RECT *);
extern HANDLE CreateSolidBrush(DWORD);
extern int FillRect(HANDLE, const RECT *, HANDLE);
extern BOOL DeleteObject(HANDLE);
extern HANDLE CreateFontIndirectW(const LOGFONT *);
extern int GetTextFaceW(HANDLE, int, WCHAR *);
extern BOOL WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
extern int AddFontResourceW(const WCHAR *);
extern HANDLE SelectObject(HANDLE, HANDLE);
extern BOOL ExtTextOutW(HANDLE, int, int, UINT, const RECT *, const WCHAR *, UINT, const int *);
extern BOOL GetTextExtentExPointW(HANDLE, const WCHAR *, int, int, int *, int *, SIZE *);
extern BOOL GetTextMetricsW(HANDLE, void *);
extern DWORD SetTextColor(HANDLE, DWORD);
extern DWORD SetBkColor(HANDLE, DWORD);
extern int SetBkMode(HANDLE, int);
extern BOOL InvalidateRect(HANDLE, const RECT *, BOOL);
extern DWORD SetTimer(HANDLE, UINT, UINT, void *);
extern BOOL KillTimer(HANDLE, UINT);
extern HANDLE FindFirstFileW(const WCHAR *, WIN32_FIND_DATA *);
extern BOOL FindNextFileW(HANDLE, WIN32_FIND_DATA *);
extern BOOL FindClose(HANDLE);
extern DWORD GetFileAttributesW(const WCHAR *);
extern BOOL DeleteFileW(const WCHAR *);
extern BOOL CopyFileW(const WCHAR *, const WCHAR *, BOOL);
extern BOOL MoveFileW(const WCHAR *, const WCHAR *);
extern BOOL CreateDirectoryW(const WCHAR *, void *);
extern BOOL RemoveDirectoryW(const WCHAR *);
extern BOOL GetDiskFreeSpaceExW(const WCHAR *, DWORD *, DWORD *, DWORD *);
extern void GlobalMemoryStatus(MEMORYSTATUS *);
extern BOOL GetVersionExW(OSVERSIONINFO *);
extern void GetLocalTime(SYSTEMTIME *);
extern DWORD GetTickCount(void);
extern HANDLE GetDC(HANDLE);
extern int ReleaseDC(HANDLE, HANDLE);

// ---------------- small runtime helpers ----------------
static int wlen(const WCHAR *s) { int n = 0; while (s[n]) n++; return n; }
static void wcpy(WCHAR *d, const WCHAR *s) { while ((*d++ = *s++)) ; }
static void wcat(WCHAR *d, const WCHAR *s) { while (*d) d++; wcpy(d, s); }
static int wcmp_s(const WCHAR *a, const char *b) { while (*b) { if (*a != (WCHAR)(unsigned char)*b) return 1; a++; b++; } return *a != 0; }
static int wicmp_s(const WCHAR *a, const char *b) {
  while (*b) { WCHAR c = *a; if (c >= L'A' && c <= L'Z') c += 32; if (c != (WCHAR)(unsigned char)*b) return 1; a++; b++; }
  return *a != 0;
}
static int wstarts_i(const WCHAR *a, const char *b) { while (*b) { WCHAR c = *a; if (c >= L'A' && c <= L'Z') c += 32; if (c != (WCHAR)(unsigned char)*b) return 0; a++; b++; } return 1; }
static int weq(const WCHAR *a, const WCHAR *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }

// decimal print of a 32-bit value into buf (returns length)
static int u32_dec(char *buf, DWORD v) {
  char tmp[12]; int t = 0, i = 0;
  if (!v) { buf[0] = '0'; buf[1] = 0; return 1; }
  while (v) { DWORD q = v / 10; tmp[t++] = (char)('0' + (v - q * 10)); v = q; }
  while (t) buf[i++] = tmp[--t];
  buf[i] = 0; return i;
}
// decimal print of a 64-bit value given as (hi, lo) DWORDs, limb-wise (no 64-bit div)
static int u64_dec(char *buf, DWORD hi, DWORD lo) {
  unsigned short limb[4]; char out[24]; int oi = 0, i;
  limb[0] = (unsigned short)(lo & 0xFFFF); limb[1] = (unsigned short)(lo >> 16);
  limb[2] = (unsigned short)(hi & 0xFFFF); limb[3] = (unsigned short)(hi >> 16);
  if (!hi && !lo) { buf[0] = '0'; buf[1] = 0; return 1; }
  while (hi || lo) {
    DWORD rem = 0; int top = hi ? 3 : (lo >> 16 ? 1 : 0);
    (void)top;
    for (i = 3; i >= 0; i--) {
      DWORD cur = (rem << 16) | limb[i];
      DWORD q = cur / 10;
      limb[i] = (unsigned short)q;
      rem = cur - q * 10;
    }
    out[oi++] = (char)('0' + rem);
    lo = limb[0] | ((DWORD)limb[1] << 16); hi = limb[2] | ((DWORD)limb[3] << 16);
  }
  for (i = 0; i < oi; i++) buf[i] = out[oi - 1 - i];
  buf[oi] = 0; return oi;
}

// ---------------- terminal core ----------------
#define MAXCOLS 168
#define MAXLINES 512
static int g_scrw, g_scrh; // set from GetClientRect in WinMain
static int g_padx, g_pady;   // constant text padding inside the client (WinMain)

static WCHAR g_sb[MAXLINES][MAXCOLS];       // scrollback ring buffer
static unsigned char g_sblen[MAXLINES];
static unsigned char g_sbattr[MAXLINES];  // 0 = normal, 2 = error line
static int g_attr_cur;                    // attr for the line being built     // cells used per line (may exceed cols -> wrapped at print time; we pre-wrap)
static int g_sb_head = 0;                   // ring index of oldest line
static int g_sb_count = 0;                  // number of committed lines
static int g_cols, g_rows;                  // defaults set in WinMain (.bss only)
static int g_cellw, g_cellh;
static int g_view_off = 0;                  // lines scrolled back from bottom
static int   g_sym_pending;                 // VK 0x7B prefix seen, pair awaited
static DWORD g_sym_t0;                      // tick of the prefix
static int   g_eat_ch;                      // swallow the decoded pair's CHAR
static int   g_sym_again;                   // a second 0x7B followed the first
static HANDLE g_hwnd = NULL;
static HANDLE g_font = NULL;
static int g_cursor_on;
static int g_busy = 0;                      // executing a command
static int g_abort = 0;                     // Ctrl+C seen during pump

// ---- color themes ---------------------------------------------------
// Values taken from the official theme definitions (no invented colors):
//  onedark: One Dark Pro theme JSON (Binaryify/OneDark-Pro) --
//           terminal.background/foreground/ansiBlue/ansiRed,
//           editorLineNumber.foreground for dim.
//  dracula: official Dracula spec (draculatheme.com/spec) --
//           Background/Foreground/Purple/Comment/Red.
//  tokyo:   Tokyo Night (enkia VS Code theme README palette + folke
//           tokyonight canonical palette) -- bg #1a1b26, fg #a9b1d6,
//           blue #7aa2f7, comment #565f89, red #f7768e.
//  github:  GitHub Dark (Primer dark tokens) -- canvas #0d1117,
//           fg.default #c9d1d9, accent.fg #58a6ff, fg.muted #8b949e,
//           danger.fg #f85149.
// COLORREF is 0x00BBGGRR; C3() takes the #RRGGBB components.
#define C3(r, g, b) ((DWORD)(r) | ((DWORD)(g) << 8) | ((DWORD)(b) << 16))
// Syntax roles use each theme's own canonical syntax palette:
//  onedark: blue #61afef, green #98c379, yellow #e5c07b (One Dark hues)
//  dracula: Cyan #8be9fd, Yellow #f1fa8c, Green #50fa7b (official spec)
//  tokyo:   blue #7aa2f7, green #9ece6a, yellow #e0af68 (tokyonight)
//  github:  blue #79c0ff, string #a5d6ff, orange #ffa657 (GitHub Dark)
typedef struct { char name[12]; DWORD bg, fg, accent, dim, err, verb, str, path; } ThemeDef;
static const ThemeDef THEMES[4] = {
  { "onedark", C3(0x28,0x2C,0x34), C3(0xAB,0xB2,0xBF), C3(0x4A,0xA5,0xF0), C3(0x49,0x51,0x62), C3(0xE0,0x55,0x61), C3(0x61,0xAF,0xEF), C3(0x98,0xC3,0x79), C3(0xE5,0xC0,0x7B) },
  { "dracula", C3(0x28,0x2A,0x36), C3(0xF8,0xF8,0xF2), C3(0xBD,0x93,0xF9), C3(0x62,0x72,0xA4), C3(0xFF,0x55,0x55), C3(0x8B,0xE9,0xFD), C3(0xF1,0xFA,0x8C), C3(0x50,0xFA,0x7B) },
  { "tokyo",   C3(0x1A,0x1B,0x26), C3(0xA9,0xB1,0xD6), C3(0x7A,0xA2,0xF7), C3(0x56,0x5F,0x89), C3(0xF7,0x76,0x8E), C3(0x7A,0xA2,0xF7), C3(0x9E,0xCE,0x6A), C3(0xE0,0xAF,0x68) },
  { "github",  C3(0x0D,0x11,0x17), C3(0xC9,0xD1,0xD9), C3(0x58,0xA6,0xFF), C3(0x8B,0x94,0x9E), C3(0xF8,0x51,0x49), C3(0x79,0xC0,0xFF), C3(0xA5,0xD6,0xFF), C3(0xFF,0xA6,0x57) },
};
static int g_theme;  // index into THEMES; set to 0 (One Dark Pro) in WinMain
#define COL_BG THEMES[g_theme].bg
#define COL_FG THEMES[g_theme].fg
#define COL_ACC THEMES[g_theme].accent
#define COL_DIM THEMES[g_theme].dim
#define COL_ERR THEMES[g_theme].err
#define COL_VERB THEMES[g_theme].verb
#define COL_STR THEMES[g_theme].str
#define COL_PATH THEMES[g_theme].path
// scrollback line attributes: 0 normal, 1 dim, 2 error, 3 path/dir
static DWORD attr_color(int a) { return a == 2 ? COL_ERR : a == 1 ? COL_DIM : a == 3 ? COL_PATH : COL_FG; }

static int sb_line_idx(int logical) { return (g_sb_head + logical) % MAXLINES; }

static void sb_push_line(const WCHAR *cells, int n) {
  int idx;
  if (g_sb_count < MAXLINES) { idx = sb_line_idx(g_sb_count); g_sb_count++; }
  else { idx = g_sb_head; g_sb_head = (g_sb_head + 1) % MAXLINES; }
  int i; for (i = 0; i < n && i < MAXCOLS; i++) g_sb[idx][i] = cells[i];
  g_sblen[idx] = (unsigned char)(n > MAXCOLS ? MAXCOLS : n);
  g_sbattr[idx] = (unsigned char)g_attr_cur;
}

static int is_wide(WCHAR c) {
  if (c < 0x1100) return 0;
  if (c <= 0x115F) return 1;
  if (c >= 0x2E80 && c <= 0xA4CF) return 1;
  if (c >= 0xAC00 && c <= 0xD7A3) return 1;
  if (c >= 0xD800 && c <= 0xDBFF) return 1;
  if (c >= 0xF900 && c <= 0xFAFF) return 1;
  if (c >= 0xFE30 && c <= 0xFE6F) return 1;
  if (c >= 0xFF00 && c <= 0xFF60) return 1;
  if (c >= 0xFFE0 && c <= 0xFFE6) return 1;
  return 0;
}
// width in cells of the first n chars of s
static int str_cells(const WCHAR *s, int n) {
  int w = 0, i; for (i = 0; i < n; i++) w += is_wide(s[i]) ? 2 : 1; return w;
}

// streaming output state: current (uncommitted) output line
static WCHAR g_oline[MAXCOLS];
static int g_olen = 0;      // chars in oline
static int g_ocells = 0;    // cells used in oline

static void term_flush_line(void) {
  sb_push_line(g_oline, g_olen);
  g_olen = 0; g_ocells = 0; g_attr_cur = 0;
}
static void term_repaint(void) {
  if (g_hwnd) { InvalidateRect(g_hwnd, NULL, TRUE); UpdateWindow(g_hwnd); }
}
static void term_putc(WCHAR c) {
  if (c == L'\r') return;
  if (c == L'\n') { term_flush_line(); return; }
  if (c == L'\t') c = L' ';
  int w = is_wide(c) ? 2 : 1;
  if (g_ocells + w > g_cols || g_olen >= MAXCOLS - 1) term_flush_line();
  g_oline[g_olen++] = c; g_ocells += w;
}
static void term_puts(const char *s) { while (*s) term_putc((WCHAR)(unsigned char)*s++); }
static void term_putw(const WCHAR *s) { while (*s) term_putc(*s++); }
static void term_putdec(DWORD v) { char b[16]; u32_dec(b, v); term_puts(b); }
static void term_putu64(DWORD hi, DWORD lo) { char b[24]; u64_dec(b, hi, lo); term_puts(b); }
static void term_nl(void) { term_putc(L'\n'); }
// end the current output: flush partial line if any, repaint
static void term_sync(void) { if (g_olen > 0) term_flush_line(); term_repaint(); }

// Pump messages during long operations; returns 0 if Ctrl+C abort requested.
static int pump_ok(void) {
  MSG msg; int n = 0;
  while (n++ < 64 && PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_CHAR && msg.wParam == 3) { g_abort = 1; continue; }
    if (msg.message == WM_KEYDOWN && msg.wParam == 0x43 && (GetKeyState(VK_CONTROL) & 0x8000)) { g_abort = 1; continue; }
    TranslateMessage(&msg); DispatchMessageW(&msg);
  }
  return !g_abort;
}

// ---------------- input line editing ----------------
#define IBUF_MAX 512
static WCHAR g_ibuf[IBUF_MAX];
static int g_ilen = 0, g_icur = 0;
#define HIST_MAX 24
static WCHAR g_hist[HIST_MAX][IBUF_MAX];
static int g_hcount = 0, g_hpos;           // hpos: -1 = current line (set in WinMain)
static WCHAR g_hsaved[IBUF_MAX];

static WCHAR g_cwd[260]; // initialised to "\" in WinMain (linker carries .bss only)

static void prompt_str(WCHAR *out) { wcpy(out, g_cwd); wcat(out, L"> "); }

// number of display rows the edit line (prompt + input) wraps into
static int edit_rows(void) {
  WCHAR p[280]; prompt_str(p);
  int cells = str_cells(p, wlen(p)) + str_cells(g_ibuf, g_ilen);
  int r = (cells + g_cols - 1) / g_cols;
  return r < 1 ? 1 : r;
}
// cell offset (row, col) of the cursor within the edit area
static void cursor_pos(int *row, int *col) {
  WCHAR p[280]; prompt_str(p);
  int cells = str_cells(p, wlen(p)) + str_cells(g_ibuf, g_icur);
  *row = cells / g_cols; *col = cells - (*row) * g_cols;
}

// ---------------- paths ----------------
// normalize an absolute path in place: collapse "." and "..", single backslashes
static void normalize(WCHAR *path) {
  WCHAR parts[32][128]; int np = 0;
  WCHAR tmp[512]; int ti = 0;
  const WCHAR *s = path;
  while (*s) {
    while (*s == L'\\' || *s == L'/') s++;
    if (!*s) break;
    ti = 0;
    while (*s && *s != L'\\' && *s != L'/' && ti < 127) tmp[ti++] = *s++;
    tmp[ti] = 0;
    if (ti == 1 && tmp[0] == L'.') continue;
    if (ti == 2 && tmp[0] == L'.' && tmp[1] == L'.') { if (np > 0) np--; continue; }
    if (np < 32) { wcpy(parts[np], tmp); np++; }
  }
  WCHAR *d = path; *d++ = L'\\';
  for (int i = 0; i < np; i++) { if (i) *d++ = L'\\'; wcpy(d, parts[i]); d += wlen(parts[i]); }
  *d = 0;
}
// resolve arg against cwd into out (absolute, normalized)
static void resolve(const WCHAR *arg, WCHAR *out) {
  WCHAR tmp[512];
  if (!arg[0]) { wcpy(out, g_cwd); return; }
  if (arg[0] == L'\\' || arg[0] == L'/') { wcpy(tmp, arg); }
  else {
    wcpy(tmp, g_cwd);
    if (wlen(tmp) > 1) wcat(tmp, L"\\");
    wcat(tmp, arg);
  }
  normalize(tmp);
  wcpy(out, tmp);
}
static int has_wild(const WCHAR *s) { while (*s) { if (*s == L'*' || *s == L'?') return 1; s++; } return 0; }
static int is_dir(const WCHAR *path) {
  DWORD a = GetFileAttributesW(path);
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
static void print_err(const char *what) {
  g_attr_cur = 2;
  term_puts(what); term_puts(" failed, error "); term_putdec(GetLastError()); term_nl();
}

// ---------------- tokenizer ----------------
#define TOK_MAX 8
static WCHAR g_tok[TOK_MAX][260];
static int g_ntok = 0;
static void tokenize(const WCHAR *line) {
  g_ntok = 0;
  const WCHAR *s = line;
  while (*s && g_ntok < TOK_MAX) {
    while (*s == L' ' || *s == L'\t') s++;
    if (!*s) break;
    int n = 0, inq = 0;
    while (*s && (inq || (*s != L' ' && *s != L'\t'))) {
      if (*s == L'"') { inq = !inq; s++; continue; }
      if (n < 259) g_tok[g_ntok][n++] = *s;
      s++;
    }
    g_tok[g_ntok][n] = 0;
    g_ntok++;
  }
}

// ---------------- commands ----------------
static void cmd_help(void) {
  term_puts("BrainTerm commands:"); term_nl();
  term_puts("  help              this list"); term_nl();
  term_puts("  dir [path|pat]    list directory (ls)"); term_nl();
  term_puts("  cd <path>         change directory"); term_nl();
  term_puts("  pwd               print current directory"); term_nl();
  term_puts("  type <file>       show text file (Ctrl+C stops)"); term_nl();
  term_puts("  copy <s> <d>      copy file"); term_nl();
  term_puts("  move <s> <d>      move / rename"); term_nl();
  term_puts("  del <file>        delete file"); term_nl();
  term_puts("  md <dir> / rd <dir>  make / remove directory"); term_nl();
  term_puts("  run <prog> [args] launch a program"); term_nl();
  term_puts("                    (or just type the program path)"); term_nl();
  term_puts("  ps                process list"); term_nl();
  term_puts("  kill <pid>        terminate a process"); term_nl();
  term_puts("  ver               OS version"); term_nl();
  term_puts("  mem               memory status"); term_nl();
  term_puts("  time              current time"); term_nl();
  term_puts("  echo <text>       print text"); term_nl();
  term_puts("  cls               clear screen"); term_nl();
  term_puts("  theme [name]      color theme: onedark (default),"); term_nl();
  term_puts("                    dracula, tokyo, github"); term_nl();
  term_puts("  exit              leave BrainTerm"); term_nl();
  term_puts("Keys: Left/Right/Home/End edit, Up/Down history,"); term_nl();
  term_puts("      PgUp/PgDn scroll back, touch-drag scrolls too."); term_nl();
  term_puts("      Ctrl+C abort output, Ctrl+L clear, Ctrl+U clear line."); term_nl();
  term_puts("Paths: \\Nand \\Nand2 \\Nand3 \\Nand4"); term_nl();
  term_puts("       \\Storage Card \\Windows"); term_nl();
}

static void fmt_size(WIN32_FIND_DATA *fd) {
  if (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { term_puts("<DIR>     "); return; }
  char b[24]; int n = u64_dec(b, fd->nFileSizeHigh, fd->nFileSizeLow);
  for (int i = n; i < 10; i++) term_putc(L' ');
  term_puts(b); term_putc(L' ');
}
static void print_entry(WIN32_FIND_DATA *fd) {
  g_attr_cur = (fd->dwFileAttributes & 0x10) ? 3 : 0;  // directories in path colour
  fmt_size(fd);
  term_putw(fd->cFileName);
  term_nl();
}
static void cmd_dir(const WCHAR *arg) {
  WCHAR path[512]; resolve(arg, path);
  WIN32_FIND_DATA fd; HANDLE h;
  int count = 0; DWORD tlo = 0, thi = 0;
  if (has_wild(path)) {
    h = FindFirstFileW(path, &fd);
    if (h == INVALID_HANDLE_VALUE) { print_err("dir"); return; }
    do { print_entry(&fd); count++; thi += 0; tlo += fd.nFileSizeLow; if (fd.nFileSizeHigh) thi += fd.nFileSizeHigh;
      if ((count & 31) == 0) { term_sync(); if (!pump_ok()) { FindClose(h); term_puts("^C"); term_nl(); return; } }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
  } else if (is_dir(path)) {
    WCHAR pat[512]; wcpy(pat, path); if (wlen(pat) > 1) wcat(pat, L"\\"); wcat(pat, L"*");
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) { print_err("dir"); return; }
    do {
      if (weq(fd.cFileName, L".") || weq(fd.cFileName, L"..")) continue;
      print_entry(&fd); count++;
      tlo += fd.nFileSizeLow; thi += fd.nFileSizeHigh;
      if ((count & 31) == 0) { term_sync(); if (!pump_ok()) { FindClose(h); term_puts("^C"); term_nl(); return; } }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
  } else {
    h = FindFirstFileW(path, &fd);
    if (h == INVALID_HANDLE_VALUE) { print_err("dir"); return; }
    print_entry(&fd); count = 1; FindClose(h);
  }
  g_attr_cur = 1; term_putdec((DWORD)count); term_puts(" item(s)"); term_nl();
  DWORD fb[2] = {0, 0}, tt[2] = {0, 0}, tf[2] = {0, 0};
  WCHAR root[512]; wcpy(root, path);
  if (GetDiskFreeSpaceExW(root, fb, tt, tf)) {
    // outputs are 64-bit: MB = bytes >> 20 across the (lo,hi) pair
    DWORD fb_mb = (fb[0] >> 20) | (fb[1] << 12);
    DWORD tt_mb = (tt[0] >> 20) | (tt[1] << 12);
    g_attr_cur = 1; term_puts("free "); term_putdec(fb_mb); term_puts(" MB of ");
    term_putdec(tt_mb); term_puts(" MB"); term_nl();
  }
  (void)thi;
}

static void cmd_cd(const WCHAR *arg) {
  if (!arg[0]) { term_putw(g_cwd); term_nl(); return; }
  WCHAR path[512]; resolve(arg, path);
  if (!is_dir(path)) { term_puts("not a directory: "); term_putw(path); term_nl(); return; }
  wcpy(g_cwd, path);
}
static void cmd_type(const WCHAR *arg) {
  WCHAR path[512]; resolve(arg, path);
  HANDLE h = CreateFileW(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) { print_err("type"); return; }
  static BYTE raw[4096];
  DWORD rd = 0;
  ReadFile(h, raw, 3, &rd, NULL);
  int mode = 0; // 0=utf8/ascii, 1=utf16le
  DWORD pos = 0;
  if (rd >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) { mode = 1; pos = 2; }
  else if (rd >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF) { pos = 3; }
  SetFilePointer(h, (long)pos, NULL, 0);
  int lines = 0;
  DWORD carry = 0; int ncarry = 0; BYTE cbuf[2];
  for (;;) {
    if (!ReadFile(h, raw, sizeof(raw), &rd, NULL) || rd == 0) break;
    DWORD i = 0;
    if (mode == 1) {
      // byte stream -> WCHAR stream, tolerant of odd chunk splits
      DWORD total = 0; static BYTE buf2[8192];
      if (ncarry) { buf2[0] = cbuf[0]; total = 1; }
      for (DWORD k = 0; k < rd; k++) buf2[total++] = raw[k];
      DWORD pairs = total / 2;
      for (DWORD k = 0; k < pairs; k++) {
        WCHAR c = (WCHAR)(buf2[2 * k] | (buf2[2 * k + 1] << 8));
        if (c == L'\n') { lines++; if ((lines & 63) == 0) { term_sync(); if (!pump_ok()) { CloseHandle(h); term_puts("^C"); term_nl(); return; } } }
        term_putc(c);
      }
      ncarry = (total & 1) ? 1 : 0;
      if (ncarry) cbuf[0] = buf2[total - 1];
      carry = 0; (void)carry;
    } else {
      // UTF-8 decode, invalid bytes pass through as Latin-1
      while (i < rd) {
        BYTE b = raw[i];
        if (b < 0x80) { if (b == '\n') { lines++; if ((lines & 63) == 0) { term_sync(); if (!pump_ok()) { CloseHandle(h); term_puts("^C"); term_nl(); return; } } } term_putc((WCHAR)b); i++; }
        else if ((b & 0xE0) == 0xC0 && i + 1 < rd && (raw[i + 1] & 0xC0) == 0x80) {
          term_putc((WCHAR)(((b & 0x1F) << 6) | (raw[i + 1] & 0x3F))); i += 2;
        } else if ((b & 0xF0) == 0xE0 && i + 2 < rd && (raw[i + 1] & 0xC0) == 0x80 && (raw[i + 2] & 0xC0) == 0x80) {
          term_putc((WCHAR)(((b & 0x0F) << 12) | ((raw[i + 1] & 0x3F) << 6) | (raw[i + 2] & 0x3F))); i += 3;
        } else { term_putc((WCHAR)b); i++; }
      }
    }
    if (lines > 6000) { term_puts("... (truncated)"); term_nl(); break; }
    term_sync();
    if (!pump_ok()) { CloseHandle(h); term_puts("^C"); term_nl(); return; }
  }
  CloseHandle(h);
  term_nl();
}

// ---- ps / kill via toolhelp.dll (named exports, resolved at runtime) ----
typedef HANDLE (*pfnSnap)(DWORD, DWORD);
typedef BOOL (*pfnProc)(HANDLE, void *);
// CE layout (differs from desktop): two extra fields after szExeFile.
typedef struct { DWORD dwSize, cntUsage, th32ProcessID, th32DefaultHeapID,
  th32ModuleID, cntThreads, th32ParentProcessID; long pcPriClassBase;
  DWORD dwFlags; WCHAR szExeFile[260]; DWORD th32MemoryBase, th32AccessKey; } PROCESSENTRY32;
static pfnSnap g_thSnap = NULL; static pfnProc g_thFirst = NULL, g_thNext = NULL;
static int th_load(void) {
  if (g_thSnap) return 1;
  HMODULE h = LoadLibraryW(L"toolhelp.dll");
  if (!h) h = LoadLibraryW(L"\\windows\\toolhelp.dll");
  if (!h) return 0;
  g_thSnap = (pfnSnap)GetProcAddressA(h, "CreateToolhelp32Snapshot");
  // This CE build exports the ANSI-style names without the W suffix
  // (verified in the ROM toolhelp.dll export table).
  g_thFirst = (pfnProc)GetProcAddressA(h, "Process32First");
  if (!g_thFirst) g_thFirst = (pfnProc)GetProcAddressA(h, "Process32FirstW");
  g_thNext = (pfnProc)GetProcAddressA(h, "Process32Next");
  if (!g_thNext) g_thNext = (pfnProc)GetProcAddressA(h, "Process32NextW");
  if (!(g_thSnap && g_thFirst && g_thNext)) { g_thSnap = NULL; g_thFirst = NULL; g_thNext = NULL; return 0; }
  return 1;
}
static void cmd_ps(void) {
  if (!th_load()) { term_puts("toolhelp.dll unavailable"); term_nl(); return; }
  HANDLE snap = g_thSnap(0x00000002UL /*TH32CS_SNAPPROCESS*/, 0);
  if (snap == INVALID_HANDLE_VALUE) { print_err("ps"); return; }
  PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
  g_attr_cur = 1; term_puts("  PID   THR  NAME"); term_nl();
  pe.dwSize = sizeof(pe);
  if (g_thFirst(snap, &pe)) {
    int n = 0;
    do {
      term_puts("  "); term_putdec(pe.th32ProcessID);
      term_puts("   "); term_putdec(pe.cntThreads);
      term_puts("  "); term_putw(pe.szExeFile); term_nl();
      if ((++n & 15) == 0) { term_sync(); if (!pump_ok()) break; }
      if (n >= 512) break;  // paranoia: never loop forever
      pe.dwSize = sizeof(pe);
    } while (g_thNext(snap, &pe));
  } else print_err("ps(first)");
  CloseHandle(snap);
}
static DWORD parse_dec(const WCHAR *s) {
  DWORD v = 0; while (*s >= L'0' && *s <= L'9') { v = v * 10 + (DWORD)(*s - L'0'); s++; } return v;
}
static void cmd_kill(const WCHAR *arg) {
  DWORD pid = parse_dec(arg);
  if (!pid) { term_puts("usage: kill <pid>"); term_nl(); return; }
  HANDLE h = OpenProcess(0x0001 /*PROCESS_TERMINATE*/, FALSE, pid);
  if (!h) { print_err("kill(open)"); return; }
  if (TerminateProcess(h, 1)) { term_puts("killed "); term_putdec(pid); term_nl(); }
  else print_err("kill");
  CloseHandle(h);
}

static int do_run(const WCHAR *path, const WCHAR *args) {
  STARTUPINFO si; PROCESS_INFORMATION pi;
  for (unsigned i = 0; i < sizeof(si) / 4; i++) ((DWORD *)&si)[i] = 0;
  si.cb = sizeof(si);
  BOOL ok = CreateProcessW(path, (args && args[0]) ? args : NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
  if (!ok) { print_err("run"); return 0; }
  term_puts("started pid "); term_putdec(pi.dwProcessId); term_puts(": "); term_putw(path); term_nl();
  CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
  return 1;
}
static void cmd_ver(void) {
  OSVERSIONINFO vi; for (unsigned i = 0; i < sizeof(vi) / 4; i++) ((DWORD *)&vi)[i] = 0;
  vi.dwOSVersionInfoSize = sizeof(vi);
  if (GetVersionExW(&vi)) {
    term_puts("Windows CE "); term_putdec(vi.dwMajorVersion); term_putc(L'.');
    term_putdec(vi.dwMinorVersion); term_puts(" build "); term_putdec(vi.dwBuildNumber); term_nl();
  } else print_err("ver");
  term_puts("BrainTerm 1.0 (p48) on SHARP Brain"); term_nl();
}
static void mb1(DWORD bytes) { // print as x.y MB
  DWORD whole = bytes / 1048576, rem = bytes - whole * 1048576;
  term_putdec(whole); term_putc(L'.'); term_putdec(rem * 10 / 1048576); term_puts(" MB");
}
static void cmd_mem(void) {
  MEMORYSTATUS ms; for (unsigned i = 0; i < sizeof(ms) / 4; i++) ((DWORD *)&ms)[i] = 0;
  ms.dwLength = sizeof(ms);
  GlobalMemoryStatus(&ms);
  term_puts("memory load "); term_putdec(ms.dwMemoryLoad); term_puts("%"); term_nl();
  term_puts("RAM  total "); mb1(ms.dwTotalPhys); term_puts("  avail "); mb1(ms.dwAvailPhys); term_nl();
  term_puts("virt total "); mb1(ms.dwTotalVirtual); term_puts("  avail "); mb1(ms.dwAvailVirtual); term_nl();
}
static void p2(int v) { if (v < 10) term_putc(L'0'); term_putdec((DWORD)v); }
static void cmd_time(void) {
  SYSTEMTIME st; GetLocalTime(&st);
  term_putdec(st.wYear); term_putc(L'-'); p2(st.wMonth); term_putc(L'-'); p2(st.wDay);
  term_putc(L' '); p2(st.wHour); term_putc(L':'); p2(st.wMinute); term_putc(L':'); p2(st.wSecond); term_nl();
}

static int g_exit_req = 0;

static void cmd_theme(const WCHAR *a1) {
  if (!a1[0]) {
    term_puts("theme: "); term_puts(THEMES[g_theme].name); term_puts("  (available: onedark dracula tokyo github)"); term_nl();
    return;
  }
  for (int i = 0; i < 4; i++) {
    if (!wicmp_s(a1, THEMES[i].name)) {
      g_theme = i;
      term_puts("theme set: "); term_puts(THEMES[i].name); term_nl();
      return;
    }
  }
  g_attr_cur = 2;
  term_puts("unknown theme: "); term_putw(a1); term_puts("  (onedark dracula tokyo github)"); term_nl();
}

static void execute(const WCHAR *line) {
  tokenize(line);
  if (g_ntok == 0) return;
  const WCHAR *c = g_tok[0];
  const WCHAR *a1 = g_ntok > 1 ? g_tok[1] : (const WCHAR *)L"";
  const WCHAR *a2 = g_ntok > 2 ? g_tok[2] : (const WCHAR *)L"";
  if (!wicmp_s(c, "help") || !wcmp_s(c, "?")) cmd_help();
  else if (!wicmp_s(c, "dir") || !wicmp_s(c, "ls")) cmd_dir(a1);
  else if (!wicmp_s(c, "cd")) cmd_cd(a1);
  else if (!wicmp_s(c, "pwd")) { term_putw(g_cwd); term_nl(); }
  else if (!wicmp_s(c, "type") || !wicmp_s(c, "cat")) { if (a1[0]) cmd_type(a1); else { term_puts("usage: type <file>"); term_nl(); } }
  else if (!wicmp_s(c, "copy") || !wicmp_s(c, "cp")) {
    if (!a1[0] || !a2[0]) { term_puts("usage: copy <src> <dst>"); term_nl(); }
    else { WCHAR s[512], d[512]; resolve(a1, s); resolve(a2, d); if (CopyFileW(s, d, FALSE)) { term_puts("copied"); term_nl(); } else print_err("copy"); }
  }
  else if (!wicmp_s(c, "move") || !wicmp_s(c, "mv") || !wicmp_s(c, "ren")) {
    if (!a1[0] || !a2[0]) { term_puts("usage: move <src> <dst>"); term_nl(); }
    else { WCHAR s[512], d[512]; resolve(a1, s); resolve(a2, d); if (MoveFileW(s, d)) { term_puts("moved"); term_nl(); } else print_err("move"); }
  }
  else if (!wicmp_s(c, "del") || !wicmp_s(c, "rm")) {
    if (!a1[0]) { term_puts("usage: del <file>"); term_nl(); }
    else { WCHAR s[512]; resolve(a1, s); if (DeleteFileW(s)) { term_puts("deleted"); term_nl(); } else print_err("del"); }
  }
  else if (!wicmp_s(c, "md") || !wicmp_s(c, "mkdir")) {
    if (!a1[0]) { term_puts("usage: md <dir>"); term_nl(); }
    else { WCHAR s[512]; resolve(a1, s); if (CreateDirectoryW(s, NULL)) { term_puts("created"); term_nl(); } else print_err("md"); }
  }
  else if (!wicmp_s(c, "rd") || !wicmp_s(c, "rmdir")) {
    if (!a1[0]) { term_puts("usage: rd <dir>"); term_nl(); }
    else { WCHAR s[512]; resolve(a1, s); if (RemoveDirectoryW(s)) { term_puts("removed"); term_nl(); } else print_err("rd"); }
  }
  else if (!wicmp_s(c, "run") || !wicmp_s(c, "start")) {
    if (!a1[0]) { term_puts("usage: run <prog> [args]"); term_nl(); }
    else {
      WCHAR s[512]; resolve(a1, s);
      // join remaining tokens as the argument string
      WCHAR args[512]; args[0] = 0;
      for (int i = 2; i < g_ntok; i++) { if (i > 2) wcat(args, L" "); wcat(args, g_tok[i]); }
      do_run(s, args);
    }
  }
  else if (!wicmp_s(c, "ps")) cmd_ps();
  else if (!wicmp_s(c, "kill")) cmd_kill(a1);
  else if (!wicmp_s(c, "ver")) cmd_ver();
  else if (!wicmp_s(c, "mem")) cmd_mem();
  else if (!wicmp_s(c, "time") || !wicmp_s(c, "date")) cmd_time();
  else if (!wicmp_s(c, "echo")) {
    // echo the raw line after "echo"
    const WCHAR *s = line; while (*s && *s != L' ' && *s != L'\t') s++;
    while (*s == L' ' || *s == L'\t') s++;
    term_putw(s); term_nl();
  }
  else if (!wicmp_s(c, "cls")) { g_sb_count = 0; g_sb_head = 0; g_view_off = 0; }
  else if (!wicmp_s(c, "theme")) cmd_theme(a1);
  else if (!wicmp_s(c, "exit") || !wicmp_s(c, "quit")) { g_exit_req = 1; }
  else {
    // bare path? if it names an existing file, launch it
    WCHAR s[512]; resolve(c, s);
    DWORD at = GetFileAttributesW(s);
    if (at != INVALID_FILE_ATTRIBUTES && !(at & FILE_ATTRIBUTE_DIRECTORY)) {
      WCHAR args[512]; args[0] = 0;
      for (int i = 1; i < g_ntok; i++) { if (i > 1) wcat(args, L" "); wcat(args, g_tok[i]); }
      do_run(s, args);
    } else {
      g_attr_cur = 2;
      term_puts("unknown command: "); term_putw(c); term_puts("  (try 'help')"); term_nl();
    }
  }
}

// commit the edit line (prompt + input) to scrollback, run it, reset editor
static void edit_insert(WCHAR c) {
  if (g_ilen >= IBUF_MAX - 1) return;
  for (int i = g_ilen; i > g_icur; i--) g_ibuf[i] = g_ibuf[i - 1];
  g_ibuf[g_icur] = c; g_ilen++; g_icur++; g_ibuf[g_ilen] = 0; g_hpos = -1;
}

// Symbol-layer decode (measured p48 probe3/probe4/probe5): on this
// guest, host symbol keys arrive as a chord [VK 0x7B prefix][base key]
// and the plain text path yields the base key's letter. The model's
// chord stagger puts the base ~271 ms after the prefix, so the pair
// window is 600 ms. A prefix whose pair never resolves (flushed by the
// blink timer) means a lone Backslash intent.
static WCHAR sym_decode(DWORD vk) {
  switch (vk) {
    case 0x51: return L'1'; case 0x57: return L'2'; case 0x45: return L'3';
    case 0x52: return L'4'; case 0x54: return L'5'; case 0x59: return L'6';
    case 0x55: return L'7'; case 0x49: return L'8'; case 0x4F: return L'9';
    case 0x50: return L'0'; case 0x4D: return L'.'; case 0x4E: return L',';
    case 0x48: return L';'; case 0xBD: return L'/';
    case 0x4C: return L'\\';  // the host Backslash key itself is (0x7B, L)
    default: return 0;
  }
}

static void accept_line(void) {
  WCHAR p[280]; prompt_str(p);
  term_putw(p); term_putw(g_ibuf); term_nl(); term_flush_line();
  // history
  if (g_ilen > 0) {
    if (g_hcount < HIST_MAX) { wcpy(g_hist[g_hcount++], g_ibuf); }
    else { for (int i = 1; i < HIST_MAX; i++) wcpy(g_hist[i - 1], g_hist[i]); wcpy(g_hist[HIST_MAX - 1], g_ibuf); }
  }
  g_hpos = -1;
  static WCHAR line[IBUF_MAX];
  wcpy(line, g_ibuf);
  g_ilen = 0; g_icur = 0; g_ibuf[0] = 0;
  g_busy = 1; g_abort = 0;
  execute(line);
  g_busy = 0;
  if (g_olen > 0) term_flush_line();
  g_view_off = 0;
  term_repaint();
  if (g_exit_req) PostQuitMessage(0);
}

static void hist_move(int dir) {
  if (g_hcount == 0) return;
  if (dir < 0) { // older
    if (g_hpos == -1) { wcpy(g_hsaved, g_ibuf); g_hpos = g_hcount - 1; }
    else if (g_hpos > 0) g_hpos--;
    wcpy(g_ibuf, g_hist[g_hpos]);
  } else {
    if (g_hpos == -1) return;
    if (g_hpos < g_hcount - 1) { g_hpos++; wcpy(g_ibuf, g_hist[g_hpos]); }
    else { g_hpos = -1; wcpy(g_ibuf, g_hsaved); }
  }
  g_ilen = wlen(g_ibuf); g_icur = g_ilen;
  term_repaint();
}

int g_drag_y, g_drag_off, g_dragging;

static void measure_metrics(HANDLE hw);

// ---------------- painting ----------------
static void draw_row_text(HANDLE hdc, int row_y, const WCHAR *cells, int ncells_chars,
                          DWORD fgcol, int inv_from, int inv_to) {
  int x = 0;
  for (int i = 0; i < ncells_chars; i++) {
    WCHAR c = cells[i];
    int w = is_wide(c) ? 2 : 1;
    RECT rc; rc.left = g_padx + x * g_cellw; rc.top = row_y; rc.right = rc.left + w * g_cellw; rc.bottom = row_y + g_cellh;
    int invert = (inv_from >= 0 && x >= inv_from && x < inv_to);
    SetTextColor(hdc, invert ? COL_BG : fgcol);
    SetBkColor(hdc, invert ? fgcol : COL_BG);
    ExtTextOutW(hdc, rc.left, row_y + 1, ETO_OPAQUE | ETO_CLIPPED, &rc, &c, 1, NULL);
    x += w;
  }
  // clear the rest of the row
  RECT rc; rc.left = g_padx + x * g_cellw; rc.top = row_y; rc.right = g_scrw; rc.bottom = row_y + g_cellh;
  HANDLE br = CreateSolidBrush(COL_BG); FillRect(hdc, &rc, br); DeleteObject(br);
}

// ---- edit-line syntax highlighting (classes per input char) ----
// 0 fg, 1 verb (known command), 2 quoted string, 3 path arg, 4 unknown verb
static const char *const VERBS[] = { "help","dir","ls","cd","pwd","type","cat",
  "copy","cp","move","mv","ren","del","rm","md","mkdir","rd","rmdir","run",
  "start","ps","kill","ver","mem","time","date","echo","cls","theme","exit",
  "quit", 0 };
static int verb_known_range(int t0, int t1) {
  for (int v = 0; VERBS[v]; v++) {
    const char *sp = VERBS[v]; int k = t0;
    while (k < t1 && *sp) { WCHAR c = g_ibuf[k]; if (c >= L'A' && c <= L'Z') c = (WCHAR)(c + 32); if (c != (WCHAR)*sp) break; k++; sp++; }
    if (k == t1 && *sp == 0) return 1;
  }
  return 0;
}
static int range_has(int t0, int t1, WCHAR ch) { for (int k = t0; k < t1; k++) if (g_ibuf[k] == ch) return 1; return 0; }
static void compute_ecl(unsigned char *ecl) {
  int n = g_ilen; int inq = 0;
  for (int i = 0; i < n; i++) { WCHAR c = g_ibuf[i]; if (c == L'"') { ecl[i] = 2; inq = !inq; } else ecl[i] = inq ? 2 : 0; }
  int i = 0;
  while (i < n && g_ibuf[i] == L' ') i++;
  int t0 = i; while (i < n && g_ibuf[i] != L' ') i++; int t1 = i;
  if (t1 > t0) { int cls = (verb_known_range(t0, t1) || range_has(t0, t1, L'\\') || range_has(t0, t1, L'/')) ? 1 : 4;
    for (int k = t0; k < t1; k++) if (ecl[k] != 2) ecl[k] = (unsigned char)cls; }
  while (i < n) {
    while (i < n && g_ibuf[i] == L' ') i++;
    if (i >= n) break;
    t0 = i; while (i < n && g_ibuf[i] != L' ') i++; t1 = i;
    int cls = (range_has(t0, t1, L'\\') || range_has(t0, t1, L'/')) ? 3 : 0;
    for (int k = t0; k < t1; k++) if (ecl[k] != 2) ecl[k] = (unsigned char)cls;
  }
}
static DWORD ecl_color(int cls) { return cls == 1 ? COL_VERB : cls == 2 ? COL_STR : cls == 3 ? COL_PATH : cls == 4 ? COL_ERR : COL_FG; }

static long __attribute__((stdcall)) wndproc(HANDLE hwnd, DWORD msg, DWORD wp, long lp) {
  switch (msg) {
  case WM_SIZE: {
    if (wp != 1) {  // not SIZE_MINIMIZED
      int nw = (int)(lp & 0xFFFF), nh = (int)((lp >> 16) & 0xFFFF);
      if (nw > 40 && nh > 40 && (nw != g_scrw || nh != g_scrh)) {
        g_scrw = nw; g_scrh = nh;
        if (g_font) measure_metrics(hwnd);
      }
    }
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
  }
  case WM_CLOSE: DestroyWindow(hwnd); return 0;
  case WM_PAINT: {
    PAINTSTRUCT ps; HANDLE hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    HANDLE br = CreateSolidBrush(COL_BG); FillRect(hdc, &rc, br); DeleteObject(br);
    if (g_font) SelectObject(hdc, g_font);
    SetBkMode(hdc, 2 /*OPAQUE*/);
    int er = edit_rows();
    int content_rows = g_rows - er;
    if (content_rows < 0) content_rows = 0;
    int end = g_sb_count - g_view_off;
    if (end < 0) end = 0;
    int start = end - content_rows;
    if (start < 0) start = 0;
    int y = g_pady;
    for (int li = start; li < end; li++, y += g_cellh) {
      int idx = sb_line_idx(li);
      draw_row_text(hdc, y, g_sb[idx], g_sblen[idx], attr_color(g_sbattr[idx]), -1, -1);
    }
    // edit line: prompt + input, wrapped
    WCHAR p[280]; prompt_str(p);
    int plen = wlen(p);
    static WCHAR eline[800];
    int ei = 0; for (int i = 0; i < plen && ei < 790; i++) eline[ei++] = p[i];
    for (int i = 0; i < g_ilen && ei < 790; i++) eline[ei++] = g_ibuf[i];
    eline[ei] = 0;
    static unsigned char ecl[800];
    compute_ecl(ecl);
    int crow, ccol; cursor_pos(&crow, &ccol);
    int total_cells = str_cells(eline, ei);
    int rows_e = (total_cells + g_cols - 1) / g_cols; if (rows_e < 1) rows_e = 1;
    // render wrapped segments
    int pos = 0; // char index
    for (int r = 0; r < rows_e; r++) {
      int yy = g_pady + (content_rows + r) * g_cellh;
      if (yy >= g_scrh) break;
      int seg_start = pos, cells_in = 0;
      while (pos < ei && cells_in + (is_wide(eline[pos]) ? 2 : 1) <= g_cols) { cells_in += is_wide(eline[pos]) ? 2 : 1; pos++; }
      int inv_from = -1, inv_to = -1;
      if (g_cursor_on && r == crow) { inv_from = ccol; inv_to = ccol + 1; }
      // draw segment chars
      int x = 0;
      for (int i = seg_start; i < pos; i++) {
        WCHAR c = eline[i];
        int w = is_wide(c) ? 2 : 1;
        RECT crc; crc.left = g_padx + x * g_cellw; crc.top = yy; crc.right = crc.left + w * g_cellw; crc.bottom = yy + g_cellh;
        int invert = (inv_from >= 0 && x >= inv_from && x < inv_to);
        DWORD base = (i < plen) ? COL_ACC : ecl_color(ecl[i - plen]);  // prompt accent, input syntax colours
        SetTextColor(hdc, invert ? COL_BG : base);
        SetBkColor(hdc, invert ? base : COL_BG);
        ExtTextOutW(hdc, crc.left, yy + 1, ETO_OPAQUE | ETO_CLIPPED, &crc, &c, 1, NULL);
        x += w;
      }
      if (inv_from >= 0 && ccol >= x) {
        // cursor past end of text (on the empty cell)
        RECT crc; crc.left = g_padx + ccol * g_cellw; crc.top = yy; crc.right = crc.left + g_cellw; crc.bottom = yy + g_cellh;
        HANDLE cbr = CreateSolidBrush(COL_ACC); FillRect(hdc, &crc, cbr); DeleteObject(cbr);
      }
      RECT rc2; rc2.left = g_padx + x * g_cellw; rc2.top = yy; rc2.right = g_scrw; rc2.bottom = yy + g_cellh;
      HANDLE br2 = CreateSolidBrush(COL_BG); FillRect(hdc, &rc2, br2); DeleteObject(br2);
    }
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_TIMER: {
    if (g_sym_pending && (GetTickCount() - g_sym_t0) > 500) {
      // A prefix that never paired: it was the lone Backslash key.
      g_sym_pending = 0; g_sym_again = 0;
      if (!g_busy) { edit_insert(L'\\'); term_repaint(); }
    }
    g_cursor_on = !g_cursor_on;
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }
  case WM_LBUTTONDOWN: {
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    g_drag_y = (int)(short)((lp >> 16) & 0xFFFF);
    g_drag_off = g_view_off; g_dragging = 1;
    return 0;
  }
  case WM_MOUSEMOVE: {
    if (g_dragging && (wp & 1 /*MK_LBUTTON*/)) {
      int y = (int)(short)((lp >> 16) & 0xFFFF);
      int drows = (y - g_drag_y) / g_cellh;
      int noff = g_drag_off + drows;
      if (noff < 0) noff = 0;
      int maxoff = g_sb_count - (g_rows - edit_rows());
      if (maxoff < 0) maxoff = 0;
      if (noff > maxoff) noff = maxoff;
      if (noff != g_view_off) { g_view_off = noff; InvalidateRect(hwnd, NULL, FALSE); }
    }
    return 0;
  }
  case WM_LBUTTONUP: {
    g_dragging = 0;
    return 0;
  }
  case WM_KEYDOWN: {
    if (g_busy) return 0;
    g_eat_ch = 0;  // a stale eat flag must not swallow a fresh key's char
    // Symbol-layer prefix VK 0x7B (see sym_decode for the measured model).
    if (wp == 0x7B) {
      if (g_sym_pending) g_sym_again = 1;  // 0x7B,0x7B: host '=' route
      g_sym_pending = 1; g_sym_t0 = GetTickCount();
      return 0;
    }
    if (g_sym_pending) {
      DWORD dt = GetTickCount() - g_sym_t0;
      int shift = (GetKeyState(0x10) & 0x8000) != 0;
      WCHAR sc = (!shift && dt <= 500) ? sym_decode(wp) : 0;
      g_sym_pending = 0; g_sym_again = 0;
      if (sc) { edit_insert(sc); g_eat_ch = 1; g_view_off = 0; term_repaint(); return 0; }
      if (wp == 20) return 0;  // prefix + caps toggle (brackets): no glyph
      edit_insert(L'\\');      // lone backslash; wp is then handled normally
    }
    g_view_off = (wp == VK_NEXT) ? g_view_off : 0;
    switch (wp) {
    case VK_RETURN: accept_line(); return 0;
    case VK_BACK:
      if (g_icur > 0) { for (int i = g_icur - 1; i < g_ilen - 1; i++) g_ibuf[i] = g_ibuf[i + 1]; g_ilen--; g_icur--; g_ibuf[g_ilen] = 0; }
      term_repaint(); return 0;
    case VK_DELETE:
      if (g_icur < g_ilen) { for (int i = g_icur; i < g_ilen - 1; i++) g_ibuf[i] = g_ibuf[i + 1]; g_ilen--; g_ibuf[g_ilen] = 0; }
      term_repaint(); return 0;
    case VK_LEFT: if (g_icur > 0) g_icur--; term_repaint(); return 0;
    case VK_RIGHT: if (g_icur < g_ilen) g_icur++; term_repaint(); return 0;
    case VK_HOME: g_icur = 0; term_repaint(); return 0;
    case VK_END: g_icur = g_ilen; term_repaint(); return 0;
    case VK_UP: hist_move(-1); return 0;
    case VK_DOWN: hist_move(1); return 0;
    // Measured host-key map on this guest (p48 probe4): host Home arrives
    // as VK_F1 (112), host End as VK_PRIOR (33), host PgUp as VK_NEXT (34),
    // host Delete as VK_ESCAPE (27), host PgDn as VK_BACK (8). The cases
    // below follow the HOST intent, not the nominal VK names.
    case 112: g_icur = 0; term_repaint(); return 0;              // host Home
    case VK_PRIOR: g_icur = g_ilen; term_repaint(); return 0;    // host End
    case VK_NEXT: { int m = g_sb_count - (g_rows - edit_rows()); if (m < 0) m = 0; g_view_off += g_rows - 1; if (g_view_off > m) g_view_off = m; term_repaint(); return 0; }  // host PgUp: scroll back
    case VK_ESCAPE:  // host Delete: forward-delete one char
      if (g_icur < g_ilen) { for (int i = g_icur; i < g_ilen - 1; i++) g_ibuf[i] = g_ibuf[i + 1]; g_ilen--; g_ibuf[g_ilen] = 0; }
      term_repaint(); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
  }
  case WM_CHAR: {
    if (g_eat_ch) { g_eat_ch = 0; return 0; }  // decoded pair's base char
    if (g_busy) return 0;
    WCHAR c = (WCHAR)wp;
    if (c == L'\r' || c == L'\n' || c == 0x08) return 0; // handled in KEYDOWN
    g_view_off = 0;
    if (c == 0x03) return 0;                       // Ctrl+C at idle: ignore
    if (c == 0x0C) { g_sb_count = 0; g_sb_head = 0; term_repaint(); return 0; }  // Ctrl+L
    if (c == 0x15) { g_ilen = 0; g_icur = 0; g_ibuf[0] = 0; term_repaint(); return 0; } // Ctrl+U
    if (c == 0x01) { g_icur = 0; term_repaint(); return 0; }   // Ctrl+A
    if (c == 0x05) { g_icur = g_ilen; term_repaint(); return 0; } // Ctrl+E
    if (c == 0x0B) { // Ctrl+K: delete to end of line
      g_ilen = g_icur; g_ibuf[g_ilen] = 0; term_repaint(); return 0;
    }
    if (c < 0x20 && c != L'\t') return 0;
    if (g_ilen < IBUF_MAX - 1) {
      for (int i = g_ilen; i > g_icur; i--) g_ibuf[i] = g_ibuf[i - 1];
      g_ibuf[g_icur] = (c == L'\t') ? L' ' : c;
      g_ilen++; g_icur++; g_ibuf[g_ilen] = 0;
      g_hpos = -1;
    }
    term_repaint();
    return 0;
  }
  case WM_DESTROY: PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}
static void measure_metrics(HANDLE hw) {
  HANDLE hdc = GetDC(hw);
  if (!hdc) return;
  if (g_font) SelectObject(hdc, g_font);
  SIZE sz; sz.cx = 0; sz.cy = 0;
  if (GetTextExtentExPointW(hdc, L"M", 1, 0, NULL, NULL, &sz) && sz.cx > 0) g_cellw = (int)sz.cx;
  static BYTE tm[128];
  if (GetTextMetricsW(hdc, tm)) { LONG hh = *(LONG *)tm; if (hh > 4) g_cellh = (int)hh + 3; }
  if (g_cellw < 4) g_cellw = 8;
  if (g_cellh < 8) g_cellh = 16;
  g_cols = (g_scrw - 2 * g_padx) / g_cellw; if (g_cols > MAXCOLS) g_cols = MAXCOLS; if (g_cols < 20) g_cols = 20;
  g_rows = (g_scrh - 2 * g_pady) / g_cellh; if (g_rows > 40) g_rows = 40; if (g_rows < 5) g_rows = 5;
  ReleaseDC(hw, hdc);
}

int __attribute__((stdcall)) WinMain(HANDLE hInst, HANDLE hPrev, WCHAR *cmd, int show) {
  (void)hPrev; (void)cmd; (void)show;
  g_cwd[0] = L'\\'; g_cwd[1] = 0;
  g_theme = 0;  // One Dark Pro
  g_padx = 6; g_pady = 4;
  g_cols = 80; g_rows = 25; g_cellw = 10; g_cellh = 18; g_cursor_on = 1; g_hpos = -1;
  LOGFONT lf;
  for (unsigned i = 0; i < sizeof(lf) / 4; i++) ((DWORD *)&lf)[i] = 0;
  // Readable, full-coverage font: the device's own MS Gothic (complete
  // JIS coverage, so Japanese file names render as glyphs, not boxes).
  // This only SELECTS an existing ROM font -- no system font settings,
  // registry font links or \Windows contents are ever touched. If the
  // face cannot be created, register the on-device font file with this
  // process only (AddFontResource, read-only) and retry; last resort is
  // the previous Courier New request.
  // Register the on-device Gothic TTC with this process first
  // (app-local, read-only, no system changes) so the "MS Gothic"
  // request below binds the genuine fixed-pitch Gothic face rather
  // than a proportional substitute.
  AddFontResourceW(L"\\Nand\\msgothic.ttc");
  lf.lfHeight = -18; lf.lfWeight = 400;
  lf.lfCharSet = 128;      // SHIFTJIS_CHARSET
  lf.lfQuality = 5;        // CLEARTYPE_QUALITY (ignored if unsupported)
  lf.lfPitchAndFamily = 0; // DEFAULT_PITCH
  wcpy(lf.lfFaceName, L"MS Gothic");
  g_font = CreateFontIndirectW(&lf);
  if (!g_font) { wcpy(lf.lfFaceName, L"Courier New"); lf.lfCharSet = 0; g_font = CreateFontIndirectW(&lf); }

  WNDCLASS wc;
  for (unsigned i = 0; i < sizeof(wc) / 4; i++) ((DWORD *)&wc)[i] = 0;
  wc.lpfnWndProc = (void *)wndproc;
  wc.hInstance = hInst;
  wc.lpszClassName = L"BrainTermClass";
  RegisterClassW(&wc);
  // A normal framed window (caption, system menu, sizing frame,
  // min/max buttons): movable and resizable like any CE app. Launched
  // as an additional app it gets the full 854x480 panel; the default
  // geometry keeps the whole window, frame included, inside it.
  HANDLE hw = CreateWindowExW(0, L"BrainTermClass", L"BrainTerm",
                              WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
                              WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_VISIBLE,
                              60, 30, 734, 420, NULL, NULL, hInst, NULL);
  g_hwnd = hw;
  ShowWindow(hw, SW_SHOWNORMAL);
  SetForegroundWindow(hw);
  SetFocus(hw);
  { RECT cr; GetClientRect(hw, &cr);
    // Layout follows the window's real client size (WM_SIZE keeps it
    // current when the user resizes or maximises the window).
    g_scrw = (int)(cr.right - cr.left); g_scrh = (int)(cr.bottom - cr.top);
    if (g_scrw < 100) g_scrw = 718;
    if (g_scrh < 100) g_scrh = 388; }
  measure_metrics(hw);
  term_puts("BrainTerm 1.0 -- self-made shell for SHARP Brain. Type 'help'."); term_nl();
  term_sync();
  SetTimer(hw, 1, 530, NULL);
  MSG msg;
  while (GetMessageW(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
  KillTimer(hw, 1);
  return 0;
}

// ---------------- compiler runtime (freestanding) ----------------
void *memset(void *d, int v, unsigned n) { unsigned char *p = (unsigned char *)d; while (n--) *p++ = (unsigned char)v; return d; }
void *memcpy(void *d, const void *s, unsigned n) { unsigned char *p = (unsigned char *)d; const unsigned char *q = (const unsigned char *)s; while (n--) *p++ = *q++; return d; }
void *memmove(void *d, const void *s, unsigned n) {
  unsigned char *p = (unsigned char *)d; const unsigned char *q = (const unsigned char *)s;
  if (p < q) { while (n--) *p++ = *q++; }
  else if (p > q) { p += n; q += n; while (n--) *--p = *--q; }
  return d;
}
void *__aeabi_memset(void *d, unsigned n, int v) { return memset(d, v, n); }
void *__aeabi_memclr(void *d, unsigned n) { return memset(d, 0, n); }
void *__aeabi_memcpy(void *d, const void *s, unsigned n) { return memcpy(d, s, n); }
void *__aeabi_memmove(void *d, const void *s, unsigned n) { return memmove(d, s, n); }
unsigned __aeabi_uidiv(unsigned n, unsigned d) {
  if (!d) return 0xFFFFFFFF;
  unsigned q = 0, r = 0;
  for (int i = 31; i >= 0; i--) { r = (r << 1) | ((n >> i) & 1); if (r >= d) { r -= d; q |= (1u << i); } }
  return q;
}
int __aeabi_idiv(int n, int d) {
  if (!d) return n < 0 ? 0x80000000 : 0x7FFFFFFF;
  int neg = 0; unsigned un, ud;
  if (n < 0) { neg = !neg; un = (unsigned)(-(n + 1)) + 1; } else un = (unsigned)n;
  if (d < 0) { neg = !neg; ud = (unsigned)(-(d + 1)) + 1; } else ud = (unsigned)d;
  unsigned q = __aeabi_uidiv(un, ud);
  return neg ? -(int)q : (int)q;
}
