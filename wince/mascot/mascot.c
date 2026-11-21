/* P56 WinCE desktop mascot (self-built CE app, additional-app route).
 * Plays BMAS v3 baked animation data. ALL heavy work is done at
 * conversion time ("distillation"): frames are pre-scaled RGB565
 * XOR-deltas (LZ4), window regions are pre-built rect lists, changed
 * bboxes are precomputed, region updates are pre-thinned. The runtime
 * only: reads a small block, LZ4-decodes it, XORs the baked bbox into
 * a DIB, applies a stored region when the frame carries one, and
 * BitBlts the baked bbox. No image decode, no scaling, no region
 * construction, no scans at runtime.
 * Transparency: window-region shaping (ExtCreateRegion). This CE
 * build's coredll has no AlphaBlend/TransparentBlt (runtime-probed).
 * Input: tap = reaction, drag = move, long-press = menu
 * (react / walk on-off / hide 10 s / exit). Diagnostics: P56LOG.TXT. */
#include "lz4mini.h"

typedef void *HWND, *HDC, *HBRUSH, *HINSTANCE, *HMENU, *HICON, *HCURSOR,
             *HANDLE, *HBITMAP, *HRGN, *HMODULE;
typedef unsigned int UINT, DWORD, WPARAM;
typedef int BOOL;
typedef long LPARAM, LRESULT;
typedef unsigned short WCHAR;
typedef unsigned char BYTE;

typedef struct { int left, top, right, bottom; } RECT;
typedef struct { long x, y; } POINT;
typedef struct {
    UINT style; LRESULT (*lpfnWndProc)(HWND, UINT, WPARAM, LPARAM);
    int cbClsExtra, cbWndExtra; HINSTANCE hInstance; HICON hIcon;
    HCURSOR hCursor; HBRUSH hbrBackground;
    const WCHAR *lpszMenuName, *lpszClassName;
} WNDCLASSW;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam;
                 DWORD time; int x, y; } MSGW;
typedef struct { HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore;
                 BOOL fIncUpdate; unsigned char rgbReserved[32]; } PAINTSTRUCT;
typedef struct {
    DWORD biSize; long biWidth, biHeight; unsigned short biPlanes, biBitCount;
    DWORD biCompression, biSizeImage; long biXPels, biYPels;
    DWORD biClrUsed, biClrImportant;
} BIH;
typedef struct { BIH hdr; DWORD colors[3]; } BMI;
typedef struct { DWORD dwSize, iType, nCount, nRgnSize; RECT rcBound; } RGNDATAHDR;
typedef struct { UINT cbSize; RECT rcExclude; } TPMPARAMS;

/* ---- coredll imports (ordinals fixed by pe-link56.py) ---- */
HANDLE CreateFileW(const WCHAR*, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
BOOL WriteFile(HANDLE, const void*, DWORD, DWORD*, void*);
BOOL ReadFile(HANDLE, void*, DWORD, DWORD*, void*);
BOOL CloseHandle(HANDLE);
BOOL FlushFileBuffers(HANDLE);
DWORD SetFilePointer(HANDLE, long, long*, DWORD);
DWORD GetFileSize(HANDLE, DWORD*);
void *VirtualAlloc(void*, unsigned, DWORD, DWORD);
DWORD GetLastError(void);
DWORD GetModuleFileNameW(HMODULE, WCHAR*, DWORD);
HMODULE GetModuleHandleW(const WCHAR*);
void *GetProcAddressA(HMODULE, const char*);
unsigned short RegisterClassW(const WNDCLASSW*);
HWND CreateWindowExW(DWORD, const WCHAR*, const WCHAR*, DWORD,
    int, int, int, int, HWND, HMENU, HINSTANCE, void*);
BOOL ShowWindow(HWND, int);
BOOL DestroyWindow(HWND);
BOOL GetMessageW(MSGW*, HWND, UINT, UINT);
BOOL TranslateMessage(const MSGW*);
LRESULT DispatchMessageW(const MSGW*);
LRESULT DefWindowProcW(HWND, UINT, WPARAM, LPARAM);
void PostQuitMessage(int);
HDC BeginPaint(HWND, PAINTSTRUCT*);
BOOL EndPaint(HWND, const PAINTSTRUCT*);
BOOL GetClientRect(HWND, RECT*);
BOOL GetWindowRect(HWND, RECT*);
BOOL SetWindowPos(HWND, HWND, int, int, int, int, UINT);
BOOL InvalidateRect(HWND, const RECT*, BOOL);
UINT SetTimer(HWND, UINT, UINT, void*);
BOOL KillTimer(HWND, UINT);
DWORD GetTickCount(void);
void Sleep(DWORD);
HDC GetDC(HWND);
int ReleaseDC(HWND, HDC);
int GetDeviceCaps(HDC, int);
HBITMAP CreateDIBSection(HDC, const BMI*, UINT, void**, HANDLE, DWORD);
HDC CreateCompatibleDC(HDC);
BOOL DeleteDC(HDC);
void *SelectObject(HDC, void*);
BOOL DeleteObject(void*);
BOOL BitBlt(HDC, int, int, int, int, HDC, int, int, DWORD);
HRGN CreateRectRgn(int, int, int, int);
int CombineRgn(HRGN, HRGN, HRGN, int);
HRGN ExtCreateRegion(const void*, DWORD, const void*);
int SetWindowRgn(HWND, HRGN, BOOL);
HMENU CreatePopupMenu(void);
BOOL AppendMenuW(HMENU, UINT, UINT, const WCHAR*);
BOOL DestroyMenu(HMENU);
BOOL TrackPopupMenuEx(HMENU, UINT, int, int, HWND, TPMPARAMS*);
BOOL SetCapture(HWND);
BOOL ReleaseCapture(void);
BOOL ClientToScreen(HWND, POINT*);
BOOL SetForegroundWindow(HWND);
BOOL BringWindowToTop(HWND);

/* ---- tiny libc ---- */
void *memcpy(void *d, const void *s, unsigned n)
{ unsigned char *dd = d; const unsigned char *ss = s;
  while (n--) *dd++ = *ss++; return d; }
void *memset(void *d, int v, unsigned n)
{ unsigned char *dd = d; while (n--) *dd++ = (unsigned char)v; return d; }
static int my_strlen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int name_eq(const char *a, const char *b)
{ while (*a && *a == *b) { a++; b++; } return *a == *b; }
static unsigned udiv(unsigned a, unsigned b)
{ unsigned q = 0, r = 0; int i;
  if (!b) return 0;
  for (i = 31; i >= 0; i--) {
      r = (r << 1) | ((a >> i) & 1);
      if (r >= b) { r -= b; q |= 1u << i; }
  }
  return q; }

/* ---- constants ---- */
#define WS_POPUP        0x80000000u
#define WS_EX_TOPMOST   0x00000008u
#define WS_EX_NOACTIVATE 0x08000000u
#define SWP_NOSIZE 1
#define SWP_NOMOVE 2
#define SWP_NOACTIVATE 0x10
#define SW_SHOWNOACTIVATE 4
#define GENERIC_READ  0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 1
#define OPEN_EXISTING 3
#define CREATE_ALWAYS 2
#define SRCCOPY 0x00CC0020u
#define BI_BITFIELDS 3
#define RGN_OR 2
#define TPM_RETURNCMD 0x0100
#define MF_STRING 0
#define WM_DESTROY 0x0002
#define WM_PAINT 0x000F
#define WM_ERASEBKGND 0x0014
#define WM_TIMER 0x0113
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_MOUSEMOVE 0x0200
#define TIMER_ID 1
#define MAX_ANIMS 16
#define MAX_FRAMES 8192
#define MAX_PIX (320 * 480)
#define COMP_CAP (320 * 1024)
#define RAW_CAP (MAX_PIX * 2 + 64)
#define MAX_RGN_RECTS 1024

typedef struct { char name[12]; unsigned short first, count, loop; short vx; } ANIM;
typedef struct { unsigned short dur; unsigned char flags, res;
                 unsigned short bx, by, bw, bh;
                 unsigned off, len, rgnOff; unsigned short rgnCount, res2; } FRAME;

static ANIM g_anims[MAX_ANIMS];
static FRAME g_frames[MAX_FRAMES];
static int g_nAnims, g_nFrames, g_W, g_H, g_anchorX, g_anchorY;
static unsigned g_rawLen, g_frameTabOff;
static HANDLE g_hFile;
static unsigned char *g_pack;
static unsigned g_packLen;
static HBITMAP g_hBmp;
static HDC g_memDC;
static unsigned char *g_bits;
static int g_stride;
static unsigned char g_comp[COMP_CAP];
static unsigned char g_raw[RAW_CAP];
static unsigned char g_rgnBuf[sizeof(RGNDATAHDR) + MAX_RGN_RECTS * sizeof(RECT) + 16];
static short g_rgnRaw[MAX_RGN_RECTS * 4];
static int g_rgnMethod;

/* ---- .bdm v3 state (see the bdm backend section below) ---- */
static int g_bdm;
static int g_bdmSparse565;
static int g_bdmStandClip;
static unsigned g_bdm565Off, g_bdm565Len, g_bdmRgnOff, g_bdmRgnLen;
static unsigned g_bdm565Clip[MAX_ANIMS], g_bdmRgnClip[MAX_ANIMS];
static unsigned g_bdm565Ptr, g_bdmRgnPtr;
static short g_bdmRgnCur[MAX_RGN_RECTS * 4];
static short g_bdmRgnTmp[MAX_RGN_RECTS * 4];
static int g_bdmRgnN;
static unsigned char g_bdmMark[MAX_RGN_RECTS];
static unsigned char g_uBmp[(MAX_PIX + 7) / 8];
static unsigned char g_uBmpApp[(MAX_PIX + 7) / 8];
static unsigned g_uArea;
static int g_bdmRgnInit;
static unsigned g_msUni;
static int g_cfgRgnRaw;
static int g_cfgRgnMask;
static int g_bdmLastEmit;

static HWND g_hwnd;
static int g_curAnim, g_curFrame;
static DWORD g_nextDue;
static int g_walkOn, g_walkDir;
static DWORD g_walkAcc, g_lastTick;
static int g_winX, g_winY, g_scrW, g_scrH;
static int g_cfgX, g_cfgY, g_cfgWalk;
static DWORD g_hideUntil;
static int g_tapCount;

static int g_captured, g_dragging, g_menuShown;
static int g_timerBusy;
static int g_downX, g_downY, g_downWinX, g_downWinY;
static DWORD g_downTick;

/* stats: split timings for the method comparison */
static unsigned g_statFrames, g_statBytes, g_statLogCtr, g_hbCtr;
static unsigned g_msDec, g_msApply, g_msRgn, g_nRgn, g_maxFrameMs;

/* ---- logging ---- */
static HANDLE g_hLog;
static void log_raw(const char *s, int n)
{ DWORD w; if (g_hLog) { WriteFile(g_hLog, s, n, &w, 0); FlushFileBuffers(g_hLog); } }
static void logs(const char *s) { log_raw(s, my_strlen(s)); }
static void mark(char c) { log_raw(&c, 1); }
static void logn(long v)
{ char buf[16]; int i = 15, neg = 0; unsigned long u;
  if (v < 0) { neg = 1; u = (unsigned long)(-v); } else u = (unsigned long)v;
  buf[i] = 0;
  if (!u) buf[--i] = '0';
  while (u) { buf[--i] = (char)('0' + u % 10); u /= 10; }
  if (neg) buf[--i] = '-';
  log_raw(buf + i, 15 - i); }
static void logline(const char *tag, long v)
{ logs(tag); logn(v); logs("\r\n"); }

static void log_stats(const char *when)
{
    unsigned n = g_statFrames ? g_statFrames : 1;
    logs("stats "); logs(when);
    logs(" frames="); logn(g_statFrames);
    logs(" dec_ms="); logn((long)udiv(g_msDec, n));
    logs(" apply_ms="); logn((long)udiv(g_msApply, n));
    logs(" max_ms="); logn(g_maxFrameMs);
    logs(" rgn_n="); logn(g_nRgn);
    logs(" rgn_ms="); logn(g_nRgn ? (long)udiv(g_msRgn, g_nRgn) : 0);
    logs(" bytes="); logn(g_statBytes);
    logs("\r\n");
}

/* ---- paths ---- */
static WCHAR g_dir[280];
static void make_path(WCHAR *out, const char *name)
{
    int i = 0, j = 0;
    while (g_dir[i]) { out[j++] = g_dir[i++]; }
    while (name[j - i]) { out[j] = (WCHAR)name[j - i]; j++; }
    out[j] = 0;
}

static int read_at(unsigned off, void *buf, unsigned len)
{
    DWORD got = 0;
    if (g_pack) {
        if (off + len > g_packLen) return 0;
        memcpy(buf, g_pack + off, len);
        return 1;
    }
    SetFilePointer(g_hFile, (long)off, 0, 0);
    return ReadFile(g_hFile, buf, len, &got, 0) && got == len;
}

static int find_anim(const char *name)
{
    int i;
    for (i = 0; i < g_nAnims; i++)
        if (name_eq(g_anims[i].name, name)) return i;
    return -1;
}

/* ---- baked region application ---- */
static void apply_region(FRAME *fr)
{
    RGNDATAHDR *hdr = (RGNDATAHDR *)g_rgnBuf;
    RECT *rects = (RECT *)(g_rgnBuf + sizeof(RGNDATAHDR));
    int n = fr->rgnCount, i;
    int x0 = g_W, y0 = g_H, x1 = 0, y1 = 0;
    DWORD t0 = GetTickCount();
    if (n > MAX_RGN_RECTS) { logs("rgn too big\r\n"); return; }
    if (!read_at(fr->rgnOff, g_rgnRaw, (unsigned)n * 8)) {
        logs("rgn read fail\r\n");
        return;
    }
    for (i = 0; i < n; i++) {
        rects[i].left = g_rgnRaw[i * 4];
        rects[i].top = g_rgnRaw[i * 4 + 1];
        rects[i].right = g_rgnRaw[i * 4 + 2];
        rects[i].bottom = g_rgnRaw[i * 4 + 3];
        if (rects[i].left < x0) x0 = rects[i].left;
        if (rects[i].top < y0) y0 = rects[i].top;
        if (rects[i].right > x1) x1 = rects[i].right;
        if (rects[i].bottom > y1) y1 = rects[i].bottom;
    }
    hdr->dwSize = sizeof(RGNDATAHDR);
    hdr->iType = 1;
    hdr->nCount = n;
    hdr->nRgnSize = n * sizeof(RECT);
    hdr->rcBound.left = x0; hdr->rcBound.top = y0;
    hdr->rcBound.right = x1; hdr->rcBound.bottom = y1;
    {
        HRGN rgn = ExtCreateRegion(0, sizeof(RGNDATAHDR) + n * sizeof(RECT), hdr);
        /* redraw=FALSE + self-invalidate only (probe pr5h, -O0 build:
         * redraw=TRUE's synchronous cross-window repaint starves the
         * shell -- shell-alive test failed at 10 applies/s with TRUE,
         * passed with FALSE at the same cadence). */
        if (rgn) {
            g_rgnMethod = 1;
            SetWindowRgn(g_hwnd, rgn, 0);
        } else {
            HRGN acc = CreateRectRgn(0, 0, 0, 0);
            for (i = 0; i < n; i++) {
                HRGN r = CreateRectRgn(rects[i].left, rects[i].top,
                                       rects[i].right, rects[i].bottom);
                CombineRgn(acc, acc, r, RGN_OR);
                DeleteObject(r);
            }
            g_rgnMethod = 2;
            SetWindowRgn(g_hwnd, acc, 0);
        }
        InvalidateRect(g_hwnd, 0, 0);
    }
    g_nRgn++;
    g_msRgn += GetTickCount() - t0;
}

/* ---- frame application ---- */
static void blit_rect(int x, int y, int w, int h)
{
    HDC dc;
    if (w <= 0 || h <= 0) return;
    dc = GetDC(g_hwnd);
    if (dc) {
        BitBlt(dc, x, y, w, h, g_memDC, x, y, SRCCOPY);
        ReleaseDC(g_hwnd, dc);
    }
}

static void apply_frame(int gi)
{
    FRAME *fr = &g_frames[gi];
    DWORD t0 = GetTickCount(), t1;
    int got, x, y;
    g_statFrames++;
    if (fr->flags & 4) return; /* zero-delta: nothing changes */
    if (fr->len > COMP_CAP || !read_at(fr->off, g_comp, fr->len)) {
        logs("frame read fail\r\n");
        return;
    }
    got = lz4_decompress(g_comp, (int)fr->len, g_raw, (int)g_rawLen);
    if (got != (int)g_rawLen) { logs("lz4 fail\r\n"); return; }
    t1 = GetTickCount();
    g_msDec += t1 - t0;
    /* XOR only the baked bbox (delta is zero outside it) */
    {
        const unsigned short *d = (const unsigned short *)g_raw;
        int yEnd = fr->by + fr->bh;
        for (y = fr->by; y < yEnd; y++) {
            unsigned short *row = (unsigned short *)
                (g_bits + (g_H - 1 - y) * g_stride);
            const unsigned short *dr = d + y * g_W;
            for (x = fr->bx; x < fr->bx + fr->bw; x++)
                if (dr[x]) row[x] ^= dr[x];
        }
    }
    if (fr->flags & 2) {
        apply_region(fr);
        blit_rect(0, 0, g_W, g_H);
    } else {
        blit_rect(fr->bx, fr->by, fr->bw, fr->bh);
    }
    {
        DWORD dt = GetTickCount() - t1;
        DWORD tot = GetTickCount() - t0;
        g_msApply += dt;
        if (tot > g_maxFrameMs) g_maxFrameMs = tot;
        g_statBytes += fr->len;
        if (++g_statLogCtr >= 240) { g_statLogCtr = 0; log_stats("periodic"); }
    }
}

/* ---- .bdm v3 (common format) backend ----
 * Reader for the shared .bdm v3 container's WinCE profile: the
 * per-pose RGB565 stream (header flags bit3; bit4 = sparse form)
 * is applied into the DIB (which doubles as the composition
 * buffer), and the delta-encoded region lists are decoded with
 * the FORMAT.md 5b semantics (after a delta the list IS the
 * survivors in previous order + the adds appended). Dimensions
 * come from stored file values; the canvas width is the spec's
 * round-half-even(crop_w * size_h / crop_h), computed once at
 * load with exact integer arithmetic -- never re-derived per
 * frame (cf. the Brainux Mao shear bug).
 *
 * Baked-region history: the producer originally baked the
 * region lists with damage_bands over alpha>0, whose
 * x-expanding rects degenerate toward the bounding box -- the
 * union covered 1.65x (miku) / 2.01x (mythra) of the
 * alpha>=128 silhouette, so used raw the window exposed
 * ~19-27k px of zeroed buffer around the character (p58
 * report). The producer re-baked the region sections on
 * 2026-11-21 from the alpha>=128 silhouette with banded run
 * unions (band height B = clamp(round-half-even(H/40),4,8);
 * measured tightness <= 1.147, region sections ~2.3-2.6x
 * smaller, all other sections byte-identical). Since p59 the
 * DEFAULT path applies the baked lists themselves, thinned
 * by the BMAS rule (apply when the union symmetric difference
 * vs the last applied union exceeds 1.5% of the union area).
 * Two fallbacks remain in mascot.cfg: RGNRAW=1 applies the
 * list verbatim on any change (the defect-evidence mode), and
 * RGNMASK=1 selects the p58 interim strategy (shape the
 * window from the decoded 565 canvas itself -- the nonzero
 * mask, banded into 8-row run unions and intersected with the
 * baked union as an outer bound), kept in case a future pack
 * again ships loose regions. */
static unsigned short bdm_r16(const unsigned char *p)
{ return (unsigned short)(p[0] | (p[1] << 8)); }
static unsigned bdm_r32(const unsigned char *p)
{ return (unsigned)p[0] | ((unsigned)p[1] << 8) |
         ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24); }
static int rhe_i(unsigned n, unsigned d)
{ unsigned q = udiv(n, d), r = n - q * d;
  if (2 * r > d || (2 * r == d && (q & 1))) q++;
  return (int)q; }
static int bytes_eq(const unsigned char *a, const unsigned char *b,
                    unsigned n)
{ while (n--) if (*a++ != *b++) return 0; return 1; }

/* Apply one RGB565 record at g_bdm565Ptr into the DIB; returns
 * bytes consumed. Mirrors the converter's CE simulation: FULL
 * zeroes the canvas then writes its rect; SPARSE writes runs
 * over the flat canvas stream; SAME does nothing. The DIB is
 * bottom-up: canvas row y lives at DIB row H-1-y. */
static void flat_adv(int *x, int *y, int n)
{
    int t = *x + n;
    unsigned q = udiv((unsigned)t, (unsigned)g_W);
    *y += (int)q;
    *x = t - (int)(q * (unsigned)g_W);
}

static unsigned bdm_apply565(void)
{
    const unsigned char *p = g_pack + g_bdm565Ptr;
    const unsigned char *start = p;
    if (!g_bdmSparse565) {
        /* full form: 12B record (0,0,w,h,bx,by) + w*h u16 */
        int w = bdm_r16(p + 4), h = bdm_r16(p + 6);
        int bx = (short)bdm_r16(p + 8), by = (short)bdm_r16(p + 10);
        int yy;
        p += 12;
        memset(g_bits, 0, g_stride * g_H);
        for (yy = 0; yy < h; yy++) {
            memcpy(g_bits + (g_H - 1 - (by + yy)) * g_stride + bx * 2,
                   p, (unsigned)w * 2);
            p += (unsigned)w * 2;
        }
        g_bdm565Ptr += (unsigned)(p - start);
        return (unsigned)(p - start);
    }
    {
        unsigned short kind = bdm_r16(p);
        p += 2;
        if (kind == 0) { /* FULL */
            int w = bdm_r16(p), h = bdm_r16(p + 2);
            int bx = (short)bdm_r16(p + 4), by = (short)bdm_r16(p + 6);
            int yy;
            p += 8;
            memset(g_bits, 0, g_stride * g_H);
            for (yy = 0; yy < h; yy++) {
                memcpy(g_bits + (g_H - 1 - (by + yy)) * g_stride +
                       bx * 2, p, (unsigned)w * 2);
                p += (unsigned)w * 2;
            }
        } else if (kind == 1) { /* SPARSE */
            unsigned plen = bdm_r32(p);
            const unsigned char *end;
            int x = 0, y = 0;
            p += 4;
            end = p + plen;
            while (p < end) {
                unsigned skip = bdm_r16(p), len = bdm_r16(p + 2);
                p += 4;
                if (!len && !skip) break;
                if (!len) { flat_adv(&x, &y, (int)skip); continue; }
                flat_adv(&x, &y, (int)skip);
                {
                    unsigned short *dst = (unsigned short *)
                        (g_bits + (g_H - 1 - y) * g_stride) + x;
                    unsigned k;
                    for (k = 0; k < len; k++) {
                        *dst++ = bdm_r16(p);
                        p += 2;
                        if (++x == g_W) {
                            x = 0;
                            y++;
                            dst = (unsigned short *)
                                (g_bits + (g_H - 1 - y) * g_stride);
                        }
                    }
                }
            }
            p = end;
        }
        /* kind == 2: SAME, nothing to do */
    }
    g_bdm565Ptr += (unsigned)(p - start);
    return (unsigned)(p - start);
}

/* Decode the next region record at g_bdmRgnPtr into
 * g_bdmRgnCur. Returns 1 when the list changed. */
static int bdm_rgn_decode(void)
{
    const unsigned char *p = g_pack + g_bdmRgnPtr;
    unsigned ndel = bdm_r16(p), nadd = bdm_r16(p + 2);
    unsigned k;
    p += 4;
    if (ndel == 0xFFFF) {
        unsigned n = nadd;
        if (n > MAX_RGN_RECTS) n = MAX_RGN_RECTS;
        for (k = 0; k < n; k++, p += 8) {
            g_bdmRgnCur[k * 4] = (short)bdm_r16(p);
            g_bdmRgnCur[k * 4 + 1] = (short)bdm_r16(p + 2);
            g_bdmRgnCur[k * 4 + 2] = (short)bdm_r16(p + 4);
            g_bdmRgnCur[k * 4 + 3] = (short)bdm_r16(p + 6);
        }
        p += (nadd - n) * 8;
        g_bdmRgnN = (int)n;
        g_bdmRgnPtr = (unsigned)(p - g_pack);
        return 1;
    }
    {
        int i, w = 0, consumed = 0;
        memset(g_bdmMark, 0, (unsigned)g_bdmRgnN);
        for (k = 0; k < ndel; k++) {
            unsigned idx = bdm_r16(p);
            p += 2;
            if (idx < (unsigned)g_bdmRgnN) g_bdmMark[idx] = 1;
        }
        for (i = 0; i < g_bdmRgnN; i++)
            if (!g_bdmMark[i]) {
                memcpy(g_bdmRgnTmp + w * 4, g_bdmRgnCur + i * 4, 8);
                w++;
            }
        for (k = 0; k < nadd; k++, p += 8) {
            if (w < MAX_RGN_RECTS) {
                g_bdmRgnTmp[w * 4] = (short)bdm_r16(p);
                g_bdmRgnTmp[w * 4 + 1] = (short)bdm_r16(p + 2);
                g_bdmRgnTmp[w * 4 + 2] = (short)bdm_r16(p + 4);
                g_bdmRgnTmp[w * 4 + 3] = (short)bdm_r16(p + 6);
                w++;
                consumed++;
            }
        }
        (void)consumed;
        memcpy(g_bdmRgnCur, g_bdmRgnTmp, (unsigned)w * 8);
        g_bdmRgnN = w;
        g_bdmRgnPtr = (unsigned)(p - g_pack);
        return (ndel || nadd) ? 1 : 0;
    }
}

static void bmp_span(unsigned char *row, int x0, int x1)
{
    int b0 = x0 >> 3, b1 = (x1 - 1) >> 3, i;
    if (b0 == b1) {
        row[b0] |= (unsigned char)
            ((0xFF << (x0 & 7)) & (0xFF >> (7 - ((x1 - 1) & 7))));
        return;
    }
    row[b0] |= (unsigned char)(0xFF << (x0 & 7));
    for (i = b0 + 1; i < b1; i++) row[i] = 0xFF;
    row[b1] |= (unsigned char)(0xFF >> (7 - ((x1 - 1) & 7)));
}

static void bdm_union_build(void)
{
    int i, y, rb = (g_W + 7) / 8;
    unsigned j, nb = (unsigned)((g_W * g_H + 7) / 8);
    memset(g_uBmp, 0, nb);
    for (i = 0; i < g_bdmRgnN; i++) {
        int rx = g_bdmRgnCur[i * 4], ry = g_bdmRgnCur[i * 4 + 1];
        int rw = g_bdmRgnCur[i * 4 + 2], rh = g_bdmRgnCur[i * 4 + 3];
        if (rw <= 0 || rh <= 0) continue;
        if (rx < 0) { rw += rx; rx = 0; }
        if (ry < 0) { rh += ry; ry = 0; }
        if (rx + rw > g_W) rw = g_W - rx;
        if (ry + rh > g_H) rh = g_H - ry;
        if (rw <= 0 || rh <= 0) continue;
        for (y = ry; y < ry + rh; y++)
            bmp_span(g_uBmp + (unsigned)y * rb, rx, rx + rw);
    }
    g_uArea = 0;
    for (j = 0; j < nb; j++) {
        unsigned char b = g_uBmp[j];
        while (b) { g_uArea += b & 1; b >>= 1; }
    }
}

/* Emit a bitmap as rects: consecutive rows with identical
 * bitmaps merge vertically. Returns rect count. */
static int bdm_union_emit(const unsigned char *bmp, RECT *out, int max)
{
    int rb = (g_W + 7) / 8, y = 0, n = 0;
    while (y < g_H) {
        int y2 = y + 1, x = 0;
        const unsigned char *row = bmp + (unsigned)y * rb;
        while (y2 < g_H &&
               bytes_eq(bmp + (unsigned)y2 * rb, row, (unsigned)rb))
            y2++;
        while (x < g_W) {
            int x0;
            while (x < g_W &&
                   !(row[x >> 3] & (1u << (x & 7)))) x++;
            if (x >= g_W) break;
            x0 = x;
            while (x < g_W && (row[x >> 3] & (1u << (x & 7)))) x++;
            if (n < max) {
                out[n].left = x0; out[n].top = y;
                out[n].right = x; out[n].bottom = y2;
            }
            n++;
        }
        y = y2;
    }
    return n;
}

/* Commit the rects already placed in g_rgnBuf as the window
 * region (redraw=FALSE + self-invalidate: the pr5h lesson). */
static void commit_region(int n, int x0, int y0, int x1, int y1)
{
    RGNDATAHDR *hdr = (RGNDATAHDR *)g_rgnBuf;
    RECT *out = (RECT *)(g_rgnBuf + sizeof(RGNDATAHDR));
    int i;
    hdr->dwSize = sizeof(RGNDATAHDR);
    hdr->iType = 1;
    hdr->nCount = n;
    hdr->nRgnSize = n * sizeof(RECT);
    hdr->rcBound.left = x0; hdr->rcBound.top = y0;
    hdr->rcBound.right = x1; hdr->rcBound.bottom = y1;
    {
        HRGN rgn = ExtCreateRegion(0,
            sizeof(RGNDATAHDR) + n * sizeof(RECT), hdr);
        if (rgn) {
            g_rgnMethod = 1;
            SetWindowRgn(g_hwnd, rgn, 0);
        } else {
            HRGN acc = CreateRectRgn(0, 0, 0, 0);
            for (i = 0; i < n; i++) {
                HRGN r = CreateRectRgn(out[i].left, out[i].top,
                                       out[i].right, out[i].bottom);
                CombineRgn(acc, acc, r, RGN_OR);
                DeleteObject(r);
            }
            g_rgnMethod = 2;
            SetWindowRgn(g_hwnd, acc, 0);
        }
        InvalidateRect(g_hwnd, 0, 0);
    }
}

/* Commit the decoded baked list (g_bdmRgnCur) verbatim as
 * the window region. Returns the rect count committed. */
static int bdm_commit_list(void)
{
    RECT *rects = (RECT *)(g_rgnBuf + sizeof(RGNDATAHDR));
    int i, n = g_bdmRgnN;
    int x0 = g_W, y0 = g_H, x1 = 0, y1 = 0;
    if (n > MAX_RGN_RECTS) n = MAX_RGN_RECTS;
    for (i = 0; i < n; i++) {
        rects[i].left = g_bdmRgnCur[i * 4];
        rects[i].top = g_bdmRgnCur[i * 4 + 1];
        rects[i].right = rects[i].left + g_bdmRgnCur[i * 4 + 2];
        rects[i].bottom = rects[i].top + g_bdmRgnCur[i * 4 + 3];
        if (rects[i].left < x0) x0 = rects[i].left;
        if (rects[i].top < y0) y0 = rects[i].top;
        if (rects[i].right > x1) x1 = rects[i].right;
        if (rects[i].bottom > y1) y1 = rects[i].bottom;
    }
    if (n) commit_region(n, x0, y0, x1, y1);
    return n;
}

/* Apply the decoded region list. Default (p59): the baked
 * list itself, thinned by the BMAS rule -- apply only when
 * the union symmetric difference vs the last applied union
 * exceeds 1.5% of the union area (clip starts always apply).
 * RGNRAW=1: the list verbatim on any change (evidence mode).
 * RGNMASK=1: the p58 interim canvas-mask shaping below. */
static void bdm_apply_region(int force, int changed)
{
    DWORD t0 = GetTickCount();
    if (g_cfgRgnRaw) {
        if (!force && !changed) return;
        g_bdmLastEmit = bdm_commit_list();
        g_nRgn++;
        g_msRgn += GetTickCount() - t0;
        return;
    }
    if (!g_cfgRgnMask) {
        /* baked list + 1.5% thinning on the exact union */
        unsigned nb = (unsigned)((g_W * g_H + 7) / 8);
        if (!force && !changed) return;
        bdm_union_build();
        if (!force && g_bdmRgnInit) {
            unsigned j, diff = 0;
            for (j = 0; j < nb; j++) {
                unsigned char bb = g_uBmp[j] ^ g_uBmpApp[j];
                while (bb) { diff += bb & 1; bb >>= 1; }
            }
            g_msUni += GetTickCount() - t0;
            if (diff * 1000 <= g_uArea * 15) return; /* <=1.5% */
        }
        g_bdmLastEmit = bdm_commit_list();
        memcpy(g_uBmpApp, g_uBmp, nb);
        g_bdmRgnInit = 1;
        g_nRgn++;
        g_msRgn += GetTickCount() - t0;
        return;
    }
    /* v2 shaping (RGNMASK=1): banded nonzero mask of the decoded canvas,
     * intersected with the baked union as an outer bound. The
     * thinning decision compares 2x2-downsampled nonzero masks
     * (current vs at-last-apply) at 4%: the full-res 565 mask
     * flickers at the fringe (quantization toggles hundreds of
     * edge pixels per pose -- 122 applies/240 frames measured),
     * and banding amplifies motion ~8x; the downsampled
     * criterion lands at ~14% applies with a worst unapplied
     * lag of ~700 px, both host-simulated on the real stream
     * and on par with BMAS's bake-time thinning (16/204). The
     * baked union (outer bound) is only built on apply frames. */
    {
        unsigned char *msk = (unsigned char *)g_raw; /* banded */
        unsigned char *bmpD = (unsigned char *)g_comp;
        unsigned char *prevD = (unsigned char *)g_comp + 8192;
        int rb = (g_W + 7) / 8, y, x, b;
        int W2 = (g_W + 1) / 2, H2 = (g_H + 1) / 2;
        int rbD = (W2 + 7) / 8;
        unsigned j, nb = (unsigned)((g_W * g_H + 7) / 8);
        unsigned nbD = (unsigned)((W2 * H2 + 7) / 8);
        unsigned area = 0, diff = 0;
        memset(bmpD, 0, nbD);
        for (y = 0; y < H2; y++) {
            int yy = y * 2;
            const unsigned short *s0 = (const unsigned short *)
                (g_bits + (g_H - 1 - yy) * g_stride);
            const unsigned short *s1 = yy + 1 < g_H ?
                (const unsigned short *)
                (g_bits + (g_H - 1 - yy - 1) * g_stride) : s0;
            unsigned char *row = bmpD + (unsigned)y * rbD;
            for (x = 0; x < W2; x++) {
                int xx = x * 2;
                if (s0[xx] || (xx + 1 < g_W && s0[xx + 1]) ||
                    s1[xx] || (xx + 1 < g_W && s1[xx + 1]))
                    row[x >> 3] |= (1u << (x & 7));
            }
        }
        for (j = 0; j < nbD; j++) {
            unsigned char bb = bmpD[j];
            while (bb) { area += bb & 1; bb >>= 1; }
        }
        if (!force && g_bdmRgnInit) {
            for (j = 0; j < nbD; j++) {
                unsigned char bb = bmpD[j] ^ prevD[j];
                while (bb) { diff += bb & 1; bb >>= 1; }
            }
            g_msUni += GetTickCount() - t0;
            if (diff * 100 <= area * 4) return; /* <=4% of area */
        }
        /* applying: baked union bound + full-res nonzero mask,
         * banded by 8 rows */
        bdm_union_build();
        memset(msk, 0, nb);
        for (y = 0; y < g_H; y++) {
            const unsigned short *src = (const unsigned short *)
                (g_bits + (g_H - 1 - y) * g_stride);
            unsigned char *row = msk + (unsigned)y * rb;
            for (x = 0; x < g_W; x++)
                if (src[x]) row[x >> 3] |= (1u << (x & 7));
        }
        if (rb <= 64) {
            for (b = 0; b < g_H; b += 8) {
                unsigned char brow[64];
                int y2, i2, rows = g_H - b < 8 ? g_H - b : 8;
                memset(brow, 0, (unsigned)rb);
                for (y2 = b; y2 < b + rows; y2++)
                    for (i2 = 0; i2 < rb; i2++)
                        brow[i2] |= msk[(unsigned)y2 * rb + i2];
                for (y2 = b; y2 < b + rows; y2++)
                    memcpy(msk + (unsigned)y2 * rb, brow, (unsigned)rb);
            }
        } else {
            memcpy(msk, g_uBmp, nb); /* degenerate width */
        }
        for (j = 0; j < nb; j++) msk[j] &= g_uBmp[j];
        g_msUni += GetTickCount() - t0;
        {
            RECT *out = (RECT *)(g_rgnBuf + sizeof(RGNDATAHDR));
            int n = bdm_union_emit(msk, out, MAX_RGN_RECTS);
            int i, x0 = g_W, y0 = g_H, x1 = 0, y1 = 0;
            if (!n) return;
            for (i = 0; i < n; i++) {
                if (out[i].left < x0) x0 = out[i].left;
                if (out[i].top < y0) y0 = out[i].top;
                if (out[i].right > x1) x1 = out[i].right;
                if (out[i].bottom > y1) y1 = out[i].bottom;
            }
            g_bdmLastEmit = n;
            commit_region(n, x0, y0, x1, y1);
            memcpy(prevD, bmpD, nbD);
            g_bdmRgnInit = 1;
        }
    }
    g_nRgn++;
    g_msRgn += GetTickCount() - t0;
}

/* Apply pose (clipStart=1 resets the region list from the
 * sentinel record and forces a region apply). Streams are
 * sequential: the caller keeps them in sync via
 * start/advance. */
static void bdm_apply_pose(int gi, int clipStart)
{
    DWORD t0 = GetTickCount(), t1;
    unsigned consumed;
    int changed;
    (void)gi;
    g_statFrames++;
    consumed = bdm_apply565();
    t1 = GetTickCount();
    g_msDec += t1 - t0;
    g_statBytes += consumed;
    changed = bdm_rgn_decode();
    bdm_apply_region(clipStart, changed);
    blit_rect(0, 0, g_W, g_H);
    {
        DWORD dt = GetTickCount() - t1;
        DWORD tot = GetTickCount() - t0;
        g_msApply += dt;
        if (tot > g_maxFrameMs) g_maxFrameMs = tot;
        if (++g_statLogCtr >= 240) {
            g_statLogCtr = 0;
            log_stats("periodic");
            logs("bdm uni_ms_tot="); logn((long)g_msUni);
            logs(" emit_rects="); logn(g_bdmLastEmit);
            logs(" list_rects="); logn(g_bdmRgnN);
            logs("\r\n");
        }
    }
}

/* Parse the RAM-resident pack: header, clips, size choice,
 * stream scan (per-clip section offsets + validation). */
static int bdm_parse(void)
{
    const unsigned char *d = g_pack;
    unsigned clipsOff = bdm_r32(d + 56), sizesOff = bdm_r32(d + 64);
    unsigned strOff = bdm_r32(d + 68);
    int cropW = bdm_r16(d + 36), cropH = bdm_r16(d + 38);
    int nclips = bdm_r16(d + 40), nsizes = bdm_r16(d + 44);
    int i, chosen = -1, chosenH = 0;
    unsigned posesOff = 0, totalPoses = 0;
    g_bdmStandClip = (short)bdm_r16(d + 54);
    if (nclips <= 0 || nclips > MAX_ANIMS || nsizes <= 0) {
        logs("bdm clips/sizes bad\r\n");
        return -1;
    }
    /* size choice: exact 320, else largest below, else smallest */
    for (i = 0; i < nsizes; i++) {
        int h = bdm_r16(d + sizesOff + (unsigned)i * 48);
        if (h == 320) { chosen = i; chosenH = h; break; }
        if (h < 320 && h > chosenH) { chosen = i; chosenH = h; }
    }
    if (chosen < 0) {
        chosen = 0;
        chosenH = bdm_r16(d + sizesOff);
        for (i = 1; i < nsizes; i++) {
            int h = bdm_r16(d + sizesOff + (unsigned)i * 48);
            if (h < chosenH) { chosen = i; chosenH = h; }
        }
    }
    {
        const unsigned char *sr = d + sizesOff + (unsigned)chosen * 48;
        posesOff = bdm_r32(sr + 20);
        g_bdmRgnOff = bdm_r32(sr + 32);
        g_bdmRgnLen = bdm_r32(sr + 36);
        g_bdm565Off = bdm_r32(sr + 40);
        g_bdm565Len = bdm_r32(sr + 44);
        g_H = chosenH;
        g_W = rhe_i((unsigned)cropW * (unsigned)chosenH,
                    (unsigned)cropH);
    }
    logline("bdm size h=", g_H);
    logline("bdm canvas w=", g_W);
    if (g_W <= 0 || g_H <= 0 || g_W * g_H > MAX_PIX ||
        !g_bdm565Len || !g_bdmRgnLen) {
        logs("bdm size/profile bad\r\n");
        return -1;
    }
    if (g_bdmStandClip < 0 || g_bdmStandClip >= nclips)
        g_bdmStandClip = 0;
    g_nAnims = nclips;
    for (i = 0; i < nclips; i++) {
        const unsigned char *cr = d + clipsOff + (unsigned)i * 16;
        const char *nm = (const char *)(d + strOff + bdm_r16(cr));
        int j = 0, type = cr[12];
        while (nm[j] && j < 11) {
            g_anims[i].name[j] = nm[j];
            j++;
        }
        g_anims[i].name[j] = 0;
        g_anims[i].first = (unsigned short)totalPoses;
        g_anims[i].count = bdm_r16(cr + 2);
        g_anims[i].loop = cr[14] & 1;
        g_anims[i].vx = 0;
        {
            unsigned k, start = totalPoses;
            int vxq = 0;
            for (k = 0; k < g_anims[i].count; k++) {
                const unsigned char *pr;
                if (start + k >= MAX_FRAMES) {
                    logs("bdm too many poses\r\n");
                    return -1;
                }
                pr = d + posesOff + (start + k) * 12;
                unsigned dur = bdm_r16(pr + 2);
                int v = (short)bdm_r16(pr + 8);
                g_frames[start + k].dur =
                    (unsigned short)(dur ? dur : 50);
                if (!vxq && v) vxq = v;
            }
            if (vxq) {
                int v = (int)udiv((unsigned)(vxq < 0 ? -vxq : vxq),
                                  256);
                g_anims[i].vx = (short)(vxq < 0 ? -(v ? v : 1)
                                                : (v ? v : 1));
            }
            if (type == 1 && !g_anims[i].vx) g_anims[i].vx = 1;
        }
        totalPoses += g_anims[i].count;
        logs("bdm clip "); logs(g_anims[i].name);
        logs(" count="); logn(g_anims[i].count);
        logs(" loop="); logn(g_anims[i].loop);
        logs(" vx="); logn(g_anims[i].vx); logs("\r\n");
    }
    if (totalPoses > MAX_FRAMES) {
        logs("bdm too many poses\r\n");
        return -1;
    }
    g_nFrames = (int)totalPoses;
    {
        const unsigned char *pr = d + posesOff;
        g_anchorX = (short)bdm_r16(pr + 4);
        g_anchorY = (short)bdm_r16(pr + 6);
    }
    /* stream scan: per-clip start offsets + exact consumption */
    {
        unsigned rp = 0, xp = 0, maxRgn = 0;
        for (i = 0; i < nclips; i++) {
            const unsigned char *p;
            unsigned n = 0, k;
            g_bdmRgnClip[i] = rp;
            g_bdm565Clip[i] = xp;
            p = d + g_bdmRgnOff + rp;
            for (k = 0; k < g_anims[i].count; k++) {
                unsigned ndel = bdm_r16(p), nadd = bdm_r16(p + 2);
                p += 4;
                if (ndel == 0xFFFF) {
                    if (k) { logs("bdm rgn sentinel mid-clip\r\n");
                             return -1; }
                    n = nadd;
                    p += nadd * 8;
                } else {
                    if (ndel > n) { logs("bdm rgn del oob\r\n");
                                    return -1; }
                    n = n - ndel + nadd;
                    p += ndel * 2 + nadd * 8;
                }
                if (n > maxRgn) maxRgn = n;
            }
            rp = (unsigned)(p - (d + g_bdmRgnOff));
            p = d + g_bdm565Off + xp;
            for (k = 0; k < g_anims[i].count; k++) {
                if (!g_bdmSparse565) {
                    unsigned w = bdm_r16(p + 4), hh = bdm_r16(p + 6);
                    p += 12 + w * hh * 2;
                } else {
                    unsigned kind = bdm_r16(p);
                    p += 2;
                    if (kind == 0) {
                        unsigned w = bdm_r16(p), hh = bdm_r16(p + 2);
                        p += 8 + w * hh * 2;
                    } else if (kind == 1) {
                        unsigned plen = bdm_r32(p);
                        p += 4 + plen;
                    } else if (kind != 2) {
                        logs("bdm 565 kind bad\r\n");
                        return -1;
                    }
                }
            }
            xp = (unsigned)(p - (d + g_bdm565Off));
        }
        if (rp != g_bdmRgnLen || xp != g_bdm565Len) {
            logs("bdm stream length mismatch rgn=");
            logn((long)rp); logs("/"); logn((long)g_bdmRgnLen);
            logs(" 565="); logn((long)xp); logs("/");
            logn((long)g_bdm565Len); logs("\r\n");
            return -1;
        }
        logline("bdm rgn max rects=", (long)maxRgn);
        logline("bdm rgn bytes=", (long)g_bdmRgnLen);
        logline("bdm 565 bytes=", (long)g_bdm565Len);
    }
    return 1;
}

/* Try to load mascot.bdm from the app folder. Returns 1 when a
 * .bdm pack is active, 0 when there is none (caller falls back
 * to BMAS), -1 on a broken .bdm (fatal). The whole pack is read
 * into RAM sequentially at startup (the T2 wedge lesson:
 * sustained seek+read streaming from SD can stall the reader
 * thread after ~32-36 MB cumulative). */
static int bdm_try_load(void)
{
    WCHAR path[300];
    HANDLE h;
    unsigned char hdr[80];
    DWORD got = 0, fsz;
    make_path(path, "mascot.bdm");
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0,
                    OPEN_EXISTING, 0, 0);
    if (h == (HANDLE)(long)-1) return 0;
    g_bdm = 1;
    if (!ReadFile(h, hdr, 80, &got, 0) || got != 80 ||
        hdr[0] != 'B' || hdr[1] != 'D' || hdr[2] != 'M' ||
        hdr[3] != '1') {
        logs("bdm header bad\r\n");
        CloseHandle(h);
        return -1;
    }
    {
        int ver = hdr[4] | (hdr[5] << 8);
        int flags = hdr[6] | (hdr[7] << 8);
        if (ver != 3 || !(flags & 8)) {
            logs("bdm not v3 / no rgb565 profile\r\n");
            CloseHandle(h);
            return -1;
        }
        g_bdmSparse565 = (flags & 16) != 0;
        logline("bdm flags=", flags);
    }
    fsz = GetFileSize(h, 0);
    logline("bdm pack bytes=", (long)fsz);
    if (!fsz || fsz == 0xFFFFFFFF || fsz > 64u * 1024 * 1024) {
        CloseHandle(h);
        return -1;
    }
    logs("bdm alloc begin\r\n");
    g_pack = (unsigned char *)VirtualAlloc(0, fsz, 0x3000, 0x04);
    if (!g_pack) {
        logs("bdm VirtualAlloc failed\r\n");
        CloseHandle(h);
        return -1;
    }
    logs("bdm alloc ok\r\n");
    {
        unsigned done = 0;
        SetFilePointer(h, 0, 0, 0);
        while (done < fsz) {
            DWORD want = fsz - done > 262144 ? 262144 : fsz - done;
            DWORD g2 = 0;
            if (!ReadFile(h, g_pack + done, want, &g2, 0) || !g2)
                break;
            done += g2;
            if ((done & 0x1FFFFF) == 0)
                logline("bdm load MB=", (long)(done >> 20));
        }
        if (done != fsz) {
            logs("bdm RAM load short\r\n");
            CloseHandle(h);
            return -1;
        }
        g_packLen = fsz;
    }
    CloseHandle(h);
    logs("bdm pack loaded to RAM\r\n");
    return bdm_parse();
}

/* ---- anim control ---- */
static void start_anim(int ai)
{
    if (ai < 0) return;
    g_curAnim = ai;
    g_curFrame = 0;
    memset(g_bits, 0, g_stride * g_H);
    if (g_bdm) {
        /* both .bdm streams are sequential per clip: rewind to
         * the clip's first pose records and apply pose 0 (its
         * 565 record is a FULL by construction, and its region
         * record is the sentinel full list) */
        g_bdm565Ptr = g_bdm565Off + g_bdm565Clip[ai];
        g_bdmRgnPtr = g_bdmRgnOff + g_bdmRgnClip[ai];
        g_bdmRgnN = 0;
        bdm_apply_pose(g_anims[ai].first, 1);
    } else {
        apply_frame(g_anims[ai].first);
    }
    g_nextDue = GetTickCount() + g_frames[g_anims[ai].first].dur;
}

static void play_named(const char *name)
{
    int ai = find_anim(name);
    if (ai >= 0) start_anim(ai);
}

static void base_anim(void)
{
    if (g_walkOn) {
        int ai = find_anim(g_walkDir ? "WalkR" : "Walk");
        if (ai < 0) ai = find_anim("Walk");
        if (ai < 0) ai = find_anim("WalkR");
        if (ai >= 0) { start_anim(ai); return; }
    }
    {
        int ai = find_anim("Idle");
        if (ai < 0) ai = find_anim("Stand");
        if (ai < 0 && g_bdm) ai = g_bdmStandClip;
        if (ai >= 0) start_anim(ai);
    }
}

static void advance(void)
{
    ANIM *a = &g_anims[g_curAnim];
    g_curFrame++;
    if (g_curFrame >= a->count) {
        if (a->loop) {
            start_anim(g_curAnim);
            return;
        }
        base_anim();
        return;
    }
    if (g_bdm) bdm_apply_pose(a->first + g_curFrame, 0);
    else apply_frame(a->first + g_curFrame);
    g_nextDue = GetTickCount() + g_frames[a->first + g_curFrame].dur;
}

/* ---- popup menu ---- */
#define MID_EXIT 101
#define MID_TAP 102
#define MID_WALK 103
#define MID_HIDE 104
static void show_menu(int sx, int sy)
{
    HMENU m = CreatePopupMenu();
    TPMPARAMS tp;
    UINT cmd;
    logs("menu open try\r\n");
    if (!m) { logs("menu create failed\r\n"); return; }
    SetForegroundWindow(g_hwnd);
    BringWindowToTop(g_hwnd);
    AppendMenuW(m, MF_STRING, MID_TAP, L"反応する");
    AppendMenuW(m, MF_STRING, MID_WALK,
                g_walkOn ? L"歩行: オン" : L"歩行: オフ");
    AppendMenuW(m, MF_STRING, MID_HIDE, L"10秒かくす");
    AppendMenuW(m, MF_STRING, MID_EXIT, L"終了");
    tp.cbSize = sizeof(tp);
    tp.rcExclude.left = g_winX; tp.rcExclude.top = g_winY;
    tp.rcExclude.right = g_winX + g_W; tp.rcExclude.bottom = g_winY + g_H;
    cmd = (UINT)(DWORD)TrackPopupMenuEx(m, TPM_RETURNCMD, sx, sy, g_hwnd, &tp);
    logs("menu cmd="); logn((long)cmd);
    logs(" err="); logn((long)GetLastError()); logs("\r\n");
    DestroyMenu(m);
    if (cmd == MID_EXIT) {
        DestroyWindow(g_hwnd);
    } else if (cmd == MID_TAP) {
        g_tapCount++;
        if ((g_tapCount & 1) || find_anim("Flick") < 0) play_named("Tap");
        else play_named("Flick");
    } else if (cmd == MID_WALK) {
        g_walkOn = !g_walkOn;
        base_anim();
    } else if (cmd == MID_HIDE) {
        ShowWindow(g_hwnd, 0);
        g_hideUntil = GetTickCount() + 10000;
    }
}

/* ---- window proc ---- */
static LRESULT WndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hw, &ps);
        if (dc) BitBlt(dc, 0, 0, g_W, g_H, g_memDC, 0, 0, SRCCOPY);
        EndPaint(hw, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER: {
        DWORD now = GetTickCount();
        /* Reentrancy guard: CE pumps this thread's queue inside
         * blocking waits (SD reads, GWES calls) in this handler, so
         * the next WM_TIMER can be dispatched reentrantly. Drop
         * reentered ticks -- advance() is due-based, so a dropped
         * tick costs nothing. */
        if (g_timerBusy) return 0;
        g_timerBusy = 1;
        if ((g_hbCtr & 7) == 0) mark('t');
        if (++g_hbCtr >= 400) {
            g_hbCtr = 0;
            logs("hb frames="); logn(g_statFrames);
            logs(" anim="); logn(g_curAnim);
            logs(" fr="); logn(g_curFrame); logs("\r\n");
        }
        if (g_hideUntil && now >= g_hideUntil) {
            g_hideUntil = 0;
            ShowWindow(hw, SW_SHOWNOACTIVATE);
            SetWindowPos(hw, (HWND)(long)-1, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        if (!g_hideUntil) {
            if (now >= g_nextDue) { advance(); mark('a'); }
            if (g_captured && !g_dragging && !g_menuShown &&
                now - g_downTick > 800) {
                POINT pt;
                g_menuShown = 1;
                ReleaseCapture();
                g_captured = 0;
                pt.x = g_downX; pt.y = g_downY;
                ClientToScreen(hw, &pt);
                show_menu((int)pt.x, (int)pt.y);
            }
            if (g_walkOn) {
                DWORD el = now - g_lastTick;
                int moved = 0;
                g_walkAcc += el;
                while (g_walkAcc >= 40) {
                    ANIM *a = &g_anims[g_curAnim];
                    g_walkAcc -= 40;
                    if (name_eq(a->name, "Walk") || name_eq(a->name, "WalkR")) {
                        /* BMAS bakes both directions as separate
                         * clips with signed vx; .bdm packs may
                         * carry a single Walk clip, so direction
                         * comes from g_walkDir there (the baked
                         * facing then mismatches on one leg --
                         * noted in the p58 report). */
                        g_winX += g_bdm ? (g_walkDir ? a->vx : -a->vx)
                                        : a->vx;
                        moved = 1;
                        if (g_winX <= 0) {
                            g_winX = 0;
                            if (!g_walkDir) { g_walkDir = 1; base_anim(); }
                        } else if (g_winX + g_W >= g_scrW) {
                            g_winX = g_scrW - g_W;
                            if (g_walkDir) { g_walkDir = 0; base_anim(); }
                        }
                    } else break;
                }
                if (moved)
                    SetWindowPos(hw, 0, g_winX, g_winY, 0, 0,
                                 SWP_NOSIZE | SWP_NOACTIVATE);
            }
            if ((now & 0x7FF) < 25)
                SetWindowPos(hw, (HWND)(long)-1, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        g_lastTick = now;
        if ((g_hbCtr & 7) == 0) mark('w');
        g_timerBusy = 0;
        return 0;
    }
    case WM_LBUTTONDOWN: {
        g_downX = (short)(lp & 0xFFFF);
        g_downY = (short)((lp >> 16) & 0xFFFF);
        g_downTick = GetTickCount();
        g_downWinX = g_winX; g_downWinY = g_winY;
        g_dragging = 0; g_menuShown = 0;
        SetCapture(hw);
        g_captured = 1;
        return 0;
    }
    case WM_MOUSEMOVE:
        if (g_captured) {
            int cx = (short)(lp & 0xFFFF), cy = (short)((lp >> 16) & 0xFFFF);
            int dx = cx - g_downX, dy = cy - g_downY;
            if (!g_dragging && (dx > 6 || dx < -6 || dy > 6 || dy < -6)) {
                g_dragging = 1;
                if (find_anim("Drag") >= 0) play_named("Drag");
            }
            if (g_dragging) {
                g_winX = g_downWinX + dx;
                g_winY = g_downWinY + dy;
                SetWindowPos(hw, 0, g_winX, g_winY, 0, 0,
                             SWP_NOSIZE | SWP_NOACTIVATE);
            }
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_captured) {
            ReleaseCapture();
            g_captured = 0;
            if (g_dragging) {
                g_dragging = 0;
                base_anim();
            } else if (!g_menuShown &&
                       GetTickCount() - g_downTick < 600) {
                g_tapCount++;
                if ((g_tapCount & 1) || find_anim("Flick") < 0)
                    play_named("Tap");
                else
                    play_named("Flick");
            }
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hw, TIMER_ID);
        log_stats("final");
        logs("mascot exit\r\n");
        if (g_hLog) { CloseHandle(g_hLog); g_hLog = 0; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hw, msg, wp, lp);
}

/* ---- config ---- */
static void load_config(void)
{
    WCHAR path[300];
    char buf[1024];
    DWORD got = 0;
    HANDLE h;
    int i;
    g_cfgX = g_cfgY = -1; g_cfgWalk = 0;
    make_path(path, "mascot.cfg");
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (h == (HANDLE)(long)-1) return;
    ReadFile(h, buf, sizeof(buf) - 1, &got, 0);
    CloseHandle(h);
    buf[got] = 0;
    for (i = 0; buf[i]; i++) {
        if ((i == 0 || buf[i - 1] == '\n') && buf[i] == 'X' && buf[i + 1] == '=') {
            int v = 0; i += 2; while (buf[i] >= '0' && buf[i] <= '9') v = v * 10 + buf[i++] - '0';
            g_cfgX = v;
        }
        if ((i == 0 || buf[i - 1] == '\n') && buf[i] == 'Y' && buf[i + 1] == '=') {
            int v = 0; i += 2; while (buf[i] >= '0' && buf[i] <= '9') v = v * 10 + buf[i++] - '0';
            g_cfgY = v;
        }
        if ((i == 0 || buf[i - 1] == '\n') && buf[i] == 'W' && buf[i + 1] == 'A') {
            g_cfgWalk = 1;
        }
        if ((i == 0 || buf[i - 1] == '\n') && buf[i] == 'R' && buf[i + 1] == 'G'
            && buf[i + 2] == 'N' && buf[i + 3] == 'R') {
            g_cfgRgnRaw = 1;
        }
        if ((i == 0 || buf[i - 1] == '\n') && buf[i] == 'R' && buf[i + 1] == 'G'
            && buf[i + 2] == 'N' && buf[i + 3] == 'M') {
            g_cfgRgnMask = 1;
        }
    }
}

/* ---- main ---- */
int WinMain(HINSTANCE hInst, HINSTANCE hPrev, WCHAR *cmd, int show)
{
    WCHAR path[300];
    unsigned char hdr[32];
    DWORD got = 0;
    (void)hPrev; (void)cmd; (void)show;

    {
        WCHAR mod[280];
        int n = (int)GetModuleFileNameW(0, mod, 280), i, last = -1;
        for (i = 0; i < n; i++) if (mod[i] == '\\') last = i;
        for (i = 0; i <= last && i < 279; i++) g_dir[i] = mod[i];
        g_dir[i] = 0;
    }
    make_path(path, "P56LOG.TXT");
    g_hLog = CreateFileW(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (g_hLog == (HANDLE)(long)-1) g_hLog = 0;
    if (g_hLog) {
        /* Preallocate the log file: on this guest's SD/FAT stack, a
         * write that must ALLOCATE a new cluster can block forever
         * (probe modes 12/13/14 all wedged at ~4-5 KB of log growth
         * inside the timer handler; the identical workload with a
         * preallocated log ran 2800+ ticks clean -- mode 16). Write
         * 512 KB of filler, then seek back; all later log writes
         * land in allocated clusters. The filler tail beyond the
         * final log content is ignored by readers. */
        static char filler[4096];
        int i;
        for (i = 0; i < (int)sizeof(filler); i++) filler[i] = '.';
        for (i = 0; i < 128; i++) {
            DWORD w;
            WriteFile(g_hLog, filler, sizeof(filler), &w, 0);
        }
        FlushFileBuffers(g_hLog);
        SetFilePointer(g_hLog, 0, 0, 0);
    }
    logs("P56 mascot start\r\n");

    {
        HMODULE cd = GetModuleHandleW(L"coredll.dll");
        void *ab = GetProcAddressA(cd, "AlphaBlend");
        void *tb = GetProcAddressA(cd, "TransparentBlt");
        HDC sdc = GetDC(0);
        logs("coredll AlphaBlend="); logn((long)(ab != 0));
        logs(" TransparentBlt="); logn((long)(tb != 0)); logs("\r\n");
        if (sdc) {
            logline("caps BITSPIXEL=", GetDeviceCaps(sdc, 12));
            g_scrW = GetDeviceCaps(sdc, 8);
            g_scrH = GetDeviceCaps(sdc, 10);
            ReleaseDC(0, sdc);
        }
        logline("screen w=", g_scrW);
        logline("screen h=", g_scrH);
    }

    /* ---- pack: the common .bdm v3 wins when mascot.bdm is
     * present next to the exe; otherwise the BMAS v3 path below
     * runs unchanged. (load_config runs after the pack parse, so
     * the rgn-mode log line is emitted there; the mode itself is
     * only consulted at apply time.) ---- */
    {
        int br = bdm_try_load();
        if (br < 0) return 1;
    }
    if (!g_bdm) {
    make_path(path, "mascot.bmas");
    g_hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0,
                          OPEN_EXISTING, 0, 0);
    if (g_hFile == (HANDLE)(long)-1) {
        logs("mascot.bmas open FAILED\r\n");
        return 1;
    }
    SetFilePointer(g_hFile, 0, 0, 0);
    if (!ReadFile(g_hFile, hdr, 32, &got, 0) || got != 32 ||
        hdr[0] != 'B' || hdr[1] != 'M' || hdr[2] != 'A' || hdr[3] != 'S' ||
        (hdr[4] | (hdr[5] << 8)) != 3) {
        logs("bmas header bad / not v3\r\n");
        return 1;
    }
    g_W = hdr[8] | (hdr[9] << 8);
    g_H = hdr[10] | (hdr[11] << 8);
    g_anchorX = hdr[12] | (hdr[13] << 8);
    g_anchorY = hdr[14] | (hdr[15] << 8);
    g_nAnims = hdr[16] | (hdr[17] << 8);
    g_nFrames = hdr[18] | (hdr[19] << 8);
    g_frameTabOff = hdr[20] | (hdr[21] << 8) | (hdr[22] << 16) | ((unsigned)hdr[23] << 24);
    g_rawLen = hdr[28] | (hdr[29] << 8) | (hdr[30] << 16) | ((unsigned)hdr[31] << 24);
    logline("canvas w=", g_W);
    logline("canvas h=", g_H);
    logline("anims=", g_nAnims);
    logline("frames=", g_nFrames);
    logline("rawLen=", (long)g_rawLen);
    if (g_nAnims > MAX_ANIMS || g_nFrames > MAX_FRAMES ||
        g_W * g_H > MAX_PIX || g_rawLen > RAW_CAP) {
        logs("bmas too big for buffers\r\n");
        return 1;
    }
    {
        /* Load the whole pack into RAM and animate from memory.
         * Sustained seek+read streaming from the SD wedges the
         * reading thread after ~32-36 MB cumulative (mascot soaks
         * 4/6 and probe mode 18, all with persistent handles over a
         * multi-MB file; narrow-range reads survive 70+ MB). The
         * pack is a few MB -- far inside the 128 MB budget -- so
         * one sequential load at startup removes the SD read path
         * from the animation loop entirely. */
        DWORD fsz = GetFileSize(g_hFile, 0);
        logline("pack bytes=", (long)fsz);
        if (fsz && fsz != 0xFFFFFFFF && fsz <= 64u * 1024 * 1024) {
            g_pack = (unsigned char *)VirtualAlloc(0, fsz, 0x3000, 0x04);
            if (g_pack) {
                unsigned done = 0;
                SetFilePointer(g_hFile, 0, 0, 0);
                while (done < fsz) {
                    DWORD want = fsz - done > 262144 ? 262144 : fsz - done;
                    DWORD g2 = 0;
                    if (!ReadFile(g_hFile, g_pack + done, want, &g2, 0) || !g2)
                        break;
                    done += g2;
                }
                if (done == fsz) {
                    g_packLen = fsz;
                    CloseHandle(g_hFile);
                    g_hFile = 0;
                    logs("pack loaded to RAM\r\n");
                } else {
                    logs("pack RAM load short; streaming fallback\r\n");
                }
            }
        }
    }
    {
        int i;
        unsigned char rec[24];
        for (i = 0; i < g_nAnims; i++) {
            if (!read_at(32 + i * 24, rec, 24)) return 1;
            memcpy(g_anims[i].name, rec, 12);
            g_anims[i].name[11] = 0;
            g_anims[i].first = rec[12] | (rec[13] << 8);
            g_anims[i].count = rec[14] | (rec[15] << 8);
            g_anims[i].loop = rec[16] | (rec[17] << 8);
            g_anims[i].vx = (short)(rec[18] | (rec[19] << 8));
            logs("anim "); logs(g_anims[i].name);
            logs(" count="); logn(g_anims[i].count);
            logs(" loop="); logn(g_anims[i].loop);
            logs(" vx="); logn(g_anims[i].vx); logs("\r\n");
        }
        {
            static unsigned char ft[MAX_FRAMES * 28];
            int i2;
            if (!read_at(g_frameTabOff, ft, (unsigned)g_nFrames * 28)) {
                logs("frame table read fail\r\n");
                return 1;
            }
            for (i2 = 0; i2 < g_nFrames; i2++) {
                const unsigned char *r = ft + i2 * 28;
                g_frames[i2].dur = r[0] | (r[1] << 8);
                g_frames[i2].flags = r[2];
                g_frames[i2].bx = r[4] | (r[5] << 8);
                g_frames[i2].by = r[6] | (r[7] << 8);
                g_frames[i2].bw = r[8] | (r[9] << 8);
                g_frames[i2].bh = r[10] | (r[11] << 8);
                g_frames[i2].off = r[12] | (r[13] << 8) | (r[14] << 16) | ((unsigned)r[15] << 24);
                g_frames[i2].len = r[16] | (r[17] << 8) | (r[18] << 16) | ((unsigned)r[19] << 24);
                g_frames[i2].rgnOff = r[20] | (r[21] << 8) | (r[22] << 16) | ((unsigned)r[23] << 24);
                g_frames[i2].rgnCount = r[24] | (r[25] << 8);
            }
        }
    }
    } /* !g_bdm */

    load_config();
    logline("cfg x=", g_cfgX);
    logline("cfg y=", g_cfgY);
    logline("cfg walk=", g_cfgWalk);
    logline("cfg rgnraw=", g_cfgRgnRaw);
    logline("cfg rgnmask=", g_cfgRgnMask);
    if (g_bdm)
        logs(g_cfgRgnRaw ? "bdm rgn mode=raw\r\n"
             : g_cfgRgnMask ? "bdm rgn mode=bandmask\r\n"
                            : "bdm rgn mode=baked\r\n");

    {
        BMI bi;
        memset(&bi, 0, sizeof(bi));
        bi.hdr.biSize = sizeof(BIH);
        bi.hdr.biWidth = g_W;
        bi.hdr.biHeight = g_H;
        bi.hdr.biPlanes = 1;
        bi.hdr.biBitCount = 16;
        bi.hdr.biCompression = BI_BITFIELDS;
        bi.colors[0] = 0xF800; bi.colors[1] = 0x07E0; bi.colors[2] = 0x001F;
        g_stride = ((g_W * 2) + 3) & ~3;
        g_hBmp = CreateDIBSection(0, &bi, 0, (void **)&g_bits, 0, 0);
        if (!g_hBmp || !g_bits) {
            logs("DIBSection 565 failed, retry BI_RGB\r\n");
            bi.hdr.biCompression = 0;
            g_hBmp = CreateDIBSection(0, &bi, 0, (void **)&g_bits, 0, 0);
            if (!g_hBmp || !g_bits) { logs("DIBSection failed hard\r\n"); return 1; }
            logs("DIB is BI_RGB (555) fallback\r\n");
        } else {
            logs("DIB RGB565 bitfields ok\r\n");
        }
        memset(g_bits, 0, g_stride * g_H);
        g_memDC = CreateCompatibleDC(0);
        SelectObject(g_memDC, g_hBmp);
    }

    {
        WNDCLASSW wc;
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"P56Mascot";
        RegisterClassW(&wc);
    }
    g_winX = (g_cfgX >= 0) ? g_cfgX : (g_scrW - g_W - 6);
    g_winY = (g_cfgY >= 0) ? g_cfgY : (g_scrH - g_H);
    if (g_winX < 0) g_winX = 0;
    if (g_winY < 0) g_winY = 0;
    g_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE, L"P56Mascot",
                             L"", WS_POPUP, g_winX, g_winY, g_W, g_H,
                             0, 0, hInst, 0);
    if (!g_hwnd) { logs("CreateWindowEx failed\r\n"); return 1; }
    logs("window created\r\n");

    g_walkOn = g_cfgWalk;
    g_walkDir = 0;
    /* Launch-race mitigation: two of four fast launches wedged the
     * GUI right after startup (app alive, no timer/paint progress,
     * shell frozen too — a GWES-level stall, suspected collision
     * between our first region/paint calls and the shell's own
     * app-launch transition). Let the shell settle before the first
     * frame/region work. */
    logs("settle wait\r\n");
    Sleep(4000);
    base_anim();
    logs("first frame applied\r\n");
    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    SetWindowPos(g_hwnd, (HWND)(long)-1, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetTimer(g_hwnd, TIMER_ID, 25, 0);
    g_lastTick = GetTickCount();
    if (g_walkOn) base_anim();
    logline("rgn method=", g_rgnMethod);

    {
        MSGW msg;
        while (GetMessageW(&msg, 0, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return 0;
}
