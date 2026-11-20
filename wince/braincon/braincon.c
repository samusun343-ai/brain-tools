/* braincon.c -- BrainCon: a console stream device driver for Windows CE,
 * built for the SHARP Brain (PW-SH6 / PW-AJ2) stock image, which ships
 * without any console device.
 *
 * =====================================================================
 * STRUCTURE (reusable stream-driver skeleton)
 * =====================================================================
 * This file is deliberately organised in two parts so it can serve as
 * the template for future CE stream drivers on this platform:
 *
 *   PART 1 -- GENERIC SKELETON. Everything a stream driver needs that
 *   does not depend on what the device does: the coredll import surface,
 *   DllMain, the exported entry points (XXX_Init/Open/Close/Read/Write/
 *   Seek/IOControl/PowerUp/PowerDown), device/open context management
 *   with refcounting, and the dispatch conventions (error reporting via
 *   SetLastError, (DWORD)-1 / FALSE returns). To make a new driver:
 *   copy Part 1, rename the CON_ prefix, replace Part 2.
 *
 *   PART 2 -- CONSOLE IMPLEMENTATION. The device-specific state and
 *   behaviour: input/output ring buffers, blocking-read semantics,
 *   and the IOCTL handlers (the terminal channel, see conioctl.h).
 *
 * The driver is loaded by device.exe (ActivateDeviceEx from the
 * launcher, registry key HKLM\Drivers\BuiltIn\BrainCon) and appears
 * as the stream device CON1:. ANY process can then CreateFileW
 * ("CON1:") and ReadFile/WriteFile it. The driver owns no window and
 * no UI; the terminal is a separate app that drains output and feeds
 * input through the documented IOCTLs (conioctl.h), so the driver
 * outlives any single consumer.
 *
 * Text model: the console is a pure byte stream. CE CRT stdio in
 * byte mode (what TxtShell uses via coredll fputws/fgetws) moves
 * ANSI/MBCS bytes over it; the driver is encoding-agnostic.
 *
 * Build: clang -target armv5te-none-eabi -marm -Os -ffreestanding
 *        -fno-builtin -fshort-wchar, linked with pe-link47.py --dll
 *        --entry DllMain --exports CON_Init,..., base 0x10000000
 *        (the PE carries .reloc, so device.exe may load it anywhere).
 */

typedef unsigned long DWORD;
typedef unsigned short WCHAR;
typedef int BOOL;
typedef void *HANDLE;
typedef const WCHAR *LPCWSTR;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef unsigned char BYTE;
typedef BYTE *PBYTE;
typedef DWORD *PDWORD;
#define TRUE 1
#define FALSE 0
#define NULL ((void *)0)
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define GENERIC_READ  0x80000000UL
#define GENERIC_WRITE 0x40000000UL
#define ERROR_INVALID_FUNCTION    1
#define ERROR_ACCESS_DENIED       5
#define ERROR_INVALID_HANDLE      6
#define ERROR_INVALID_PARAMETER   87
#define ERROR_NO_DATA             232
#define ERROR_TIMEOUT             1460
#define INFINITE_TIMEOUT          0xFFFFFFFFUL
#define DLL_PROCESS_ATTACH 1

#include "conioctl.h"

/* ==================== PART 1: generic skeleton ==================== */

/* ---- coredll imports (resolved by ordinal in pe-link47.py) ---- */
extern HANDLE CreateEventW(void *sec, BOOL manualReset, BOOL initState, LPCWSTR name);
extern BOOL EventModify(HANDLE hEvent, DWORD dwFunc); /* 1=pulse 2=reset 3=set */
extern DWORD WaitForSingleObject(HANDLE h, DWORD ms);
extern void Sleep(DWORD ms);
extern void InitializeCriticalSection(void *cs);
extern void EnterCriticalSection(void *cs);
extern void LeaveCriticalSection(void *cs);
extern void DeleteCriticalSection(void *cs);
extern long InterlockedIncrement(long *p);
extern long InterlockedDecrement(long *p);
extern void *LocalAlloc(DWORD flags, DWORD bytes);
extern void *LocalFree(void *p);
extern DWORD GetTickCount(void);
extern void SetLastError(DWORD e);
extern void OutputDebugStringW(LPCWSTR s);
#define EVENT_RESET 2
#define EVENT_SET   3
#define LMEM_FIXED 0

/* clang may emit these even with -fno-builtin; provide them locally. */
void *memcpy(void *d, const void *s, unsigned n) {
  BYTE *dd = (BYTE *)d; const BYTE *ss = (const BYTE *)s;
  while (n--) *dd++ = *ss++;
  return d;
}
void *memset(void *d, int v, unsigned n) {
  BYTE *dd = (BYTE *)d;
  while (n--) *dd++ = (BYTE)v;
  return d;
}

static void dbg(LPCWSTR s) { OutputDebugStringW(s); }

/* Critical section storage (CE CRITICAL_SECTION is 24 bytes; keep room). */
typedef struct { DWORD opaque[8]; } CS;
static void cs_init(CS *cs) { InitializeCriticalSection(cs); }
static void cs_lock(CS *cs) { EnterCriticalSection(cs); }
static void cs_unlock(CS *cs) { LeaveCriticalSection(cs); }
static void cs_del(CS *cs) { DeleteCriticalSection(cs); }

/* ---- device / open contexts -------------------------------------
 * One DEVCTX per CON_Init (per loaded device instance), one OPENCTX
 * per CON_Open. The skeleton owns their lifetime and refcounting;
 * Part 2 hangs device-specific state off DEVCTX. */
typedef struct DEVCTX DEVCTX;
typedef struct OPENCTX OPENCTX;

/* Part 2 hooks the skeleton calls (defined below). */
static BOOL dev_create(DEVCTX *dev, LPCWSTR regPath, DWORD context);
static void dev_destroy(DEVCTX *dev);
static BOOL open_create(DEVCTX *dev, OPENCTX *oc, DWORD access);
static void open_destroy(DEVCTX *dev, OPENCTX *oc);
static DWORD dev_read(DEVCTX *dev, OPENCTX *oc, void *buf, DWORD len);
static DWORD dev_write(DEVCTX *dev, OPENCTX *oc, const void *buf, DWORD len);
static BOOL dev_ioctl(DEVCTX *dev, OPENCTX *oc, DWORD code,
                      PBYTE in, DWORD inLen, PBYTE out, DWORD outLen,
                      PDWORD outUsed);

struct DEVCTX {
  CS lock;              /* guards all device state below */
  long openCount;       /* live opens */
  long totalOpens;      /* opens since Init */
  DWORD regContext;     /* dwContext handed to Init */
  /* ---- device-specific state (Part 2) ---- */
  HANDLE hInEvent;      /* manual-reset: input queued or EOF */
  HANDLE hOutEvent;     /* manual-reset: output pending */
  DWORD defaultReadTimeout; /* ms, INFINITE_TIMEOUT = block forever */
  int inputEOF;         /* IOCTL_CON_CLOSEINPUT seen */
  int echoInput;        /* echo PUTINPUT chars to the output ring */
  DWORD droppedBytes;   /* output bytes dropped (ring full) */
  DWORD droppedInput;   /* input bytes dropped (ring full) */
};

struct OPENCTX {
  DEVCTX *dev;
  DWORD access;         /* GENERIC_READ/GENERIC_WRITE bits from Open */
  DWORD readTimeout;    /* ms for this open */
};

/* ---- DLL entry -------------------------------------------------- */
BOOL __attribute__((stdcall)) DllMain(HANDLE hDll, DWORD reason, void *res) {
  (void)hDll; (void)res;
  if (reason == DLL_PROCESS_ATTACH) dbg(L"BrainCon: DllMain attach\r\n");
  return TRUE;
}

/* ---- stream entry points (exports; prefix CON) ------------------- */
DWORD __attribute__((stdcall)) CON_Init(LPCWSTR lpszContext, DWORD dwContext) {
  DEVCTX *dev = (DEVCTX *)LocalAlloc(LMEM_FIXED, sizeof(DEVCTX));
  if (!dev) return 0;
  memset(dev, 0, sizeof(DEVCTX));
  dev->regContext = dwContext;
  if (!dev_create(dev, lpszContext, dwContext)) {
    LocalFree(dev);
    return 0;
  }
  dbg(L"BrainCon: CON_Init ok\r\n");
  return (DWORD)dev;
}

BOOL __attribute__((stdcall)) CON_Deinit(DWORD dwContext) {
  DEVCTX *dev = (DEVCTX *)dwContext;
  if (!dev) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
  dev_destroy(dev);
  LocalFree(dev);
  dbg(L"BrainCon: CON_Deinit\r\n");
  return TRUE;
}

DWORD __attribute__((stdcall)) CON_Open(DWORD dwContext, DWORD dwAccess, DWORD dwShareMode) {
  DEVCTX *dev = (DEVCTX *)dwContext;
  OPENCTX *oc;
  (void)dwShareMode; /* console is shareable; access checked per call */
  if (!dev) { SetLastError(ERROR_INVALID_HANDLE); return 0; }
  oc = (OPENCTX *)LocalAlloc(LMEM_FIXED, sizeof(OPENCTX));
  if (!oc) return 0;
  memset(oc, 0, sizeof(OPENCTX));
  oc->dev = dev;
  oc->access = dwAccess;
  oc->readTimeout = dev->defaultReadTimeout;
  if (!open_create(dev, oc, dwAccess)) { LocalFree(oc); return 0; }
  InterlockedIncrement(&dev->openCount);
  InterlockedIncrement(&dev->totalOpens);
  return (DWORD)oc;
}

BOOL __attribute__((stdcall)) CON_Close(DWORD dwOpenContext) {
  OPENCTX *oc = (OPENCTX *)dwOpenContext;
  if (!oc) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
  open_destroy(oc->dev, oc);
  InterlockedDecrement(&oc->dev->openCount);
  LocalFree(oc);
  return TRUE;
}

DWORD __attribute__((stdcall)) CON_Read(DWORD dwOpenContext, LPVOID pBuffer, DWORD dwCount) {
  OPENCTX *oc = (OPENCTX *)dwOpenContext;
  if (!oc) { SetLastError(ERROR_INVALID_HANDLE); return (DWORD)-1; }
  if (!(oc->access & GENERIC_READ)) { SetLastError(ERROR_ACCESS_DENIED); return (DWORD)-1; }
  return dev_read(oc->dev, oc, pBuffer, dwCount);
}

DWORD __attribute__((stdcall)) CON_Write(DWORD dwOpenContext, LPCVOID pBuffer, DWORD dwCount) {
  OPENCTX *oc = (OPENCTX *)dwOpenContext;
  if (!oc) { SetLastError(ERROR_INVALID_HANDLE); return (DWORD)-1; }
  if (!(oc->access & GENERIC_WRITE)) { SetLastError(ERROR_ACCESS_DENIED); return (DWORD)-1; }
  return dev_write(oc->dev, oc, pBuffer, dwCount);
}

DWORD __attribute__((stdcall)) CON_Seek(DWORD dwOpenContext, long lOffset, DWORD dwWhence) {
  (void)dwOpenContext; (void)lOffset; (void)dwWhence;
  /* A console is a pure stream: seeking is meaningless and must fail. */
  SetLastError(ERROR_INVALID_FUNCTION);
  return (DWORD)-1;
}

BOOL __attribute__((stdcall)) CON_IOControl(DWORD dwOpenContext, DWORD dwIoControlCode,
                                            PBYTE pInBuf, DWORD dwInLen,
                                            PBYTE pOutBuf, DWORD dwOutLen, PDWORD pdwOutLen) {
  OPENCTX *oc = (OPENCTX *)dwOpenContext;
  if (!oc) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
  if (pdwOutLen) *pdwOutLen = 0;
  return dev_ioctl(oc->dev, oc, dwIoControlCode, pInBuf, dwInLen,
                   pOutBuf, dwOutLen, pdwOutLen);
}

void __attribute__((stdcall)) CON_PowerUp(DWORD dwContext) { (void)dwContext; }
void __attribute__((stdcall)) CON_PowerDown(DWORD dwContext) { (void)dwContext; }

/* ==================== PART 2: console device ======================
 * Two ring buffers, both guarded by dev->lock:
 *   input  -- keystroke bytes queued by the terminal (PUTINPUT),
 *             consumed by ReadFile on any open.
 *   output -- raw bytes written by WriteFile on any open, drained
 *             by the terminal (GETOUTPUT).
 * A console must never deadlock a writer: a full output ring drops
 * the OLDEST bytes (scrollback semantics) and counts them. A full
 * input ring drops the newest excess bytes and counts them.
 * Reads block on hInEvent until input is queued or EOF is signalled
 * (IOCTL_CON_CLOSEINPUT). */
#define IN_CAP  8192     /* bytes */
#define OUT_CAP 65536    /* bytes */
static BYTE inq[IN_CAP];
static DWORD inHead, inTail, inCount;      /* head=read pos, tail=write pos */
static BYTE outq[OUT_CAP];
static DWORD outHead, outTail, outCount;

static BOOL dev_create(DEVCTX *dev, LPCWSTR regPath, DWORD context) {
  (void)regPath; (void)context;
  cs_init(&dev->lock);
  dev->defaultReadTimeout = INFINITE_TIMEOUT;
  dev->inputEOF = 0;
  dev->echoInput = 1;
  dev->hInEvent = CreateEventW(NULL, TRUE, FALSE, NULL);   /* manual-reset */
  dev->hOutEvent = CreateEventW(NULL, TRUE, FALSE, NULL);  /* manual-reset */
  if (!dev->hInEvent || !dev->hOutEvent) return FALSE;
  inHead = inTail = inCount = 0;
  outHead = outTail = outCount = 0;
  return TRUE;
}

static void dev_destroy(DEVCTX *dev) {
  /* device.exe deactivates only after handles close in practice;
   * wake any straggler readers so nothing blocks on a dead device. */
  if (dev->hInEvent) EventModify(dev->hInEvent, EVENT_SET);
  if (dev->hOutEvent) EventModify(dev->hOutEvent, EVENT_SET);
  cs_del(&dev->lock);
}

static BOOL open_create(DEVCTX *dev, OPENCTX *oc, DWORD access) {
  (void)dev; (void)oc; (void)access;
  return TRUE;
}

static void open_destroy(DEVCTX *dev, OPENCTX *oc) { (void)dev; (void)oc; }

/* Copy up to `want` bytes out of the input ring. Caller holds lock.
 * Returns bytes copied. */
static DWORD in_pop(BYTE *dst, DWORD want) {
  DWORD n = 0;
  while (n < want && inCount > 0) {
    dst[n++] = inq[inHead];
    inHead++; if (inHead >= IN_CAP) inHead = 0;
    inCount--;
  }
  return n;
}

static DWORD dev_read(DEVCTX *dev, OPENCTX *oc, void *buf, DWORD len) {
  DWORD start = GetTickCount();
  if (len == 0) return 0;
  for (;;) {
    DWORD got;
    cs_lock(&dev->lock);
    got = in_pop((BYTE *)buf, len);
    if (got == 0 && inCount == 0 && !dev->inputEOF)
      EventModify(dev->hInEvent, EVENT_RESET);
    cs_unlock(&dev->lock);
    if (got > 0) return got;
    if (dev->inputEOF) return 0;      /* EOF: only 0-length return */
    if (oc->readTimeout == 0) { SetLastError(ERROR_NO_DATA); return (DWORD)-1; }
    if (oc->readTimeout == INFINITE_TIMEOUT) {
      WaitForSingleObject(dev->hInEvent, INFINITE_TIMEOUT);
    } else {
      DWORD elapsed = GetTickCount() - start;
      if (elapsed >= oc->readTimeout) { SetLastError(ERROR_TIMEOUT); return (DWORD)-1; }
      WaitForSingleObject(dev->hInEvent, oc->readTimeout - elapsed);
    }
  }
}

static DWORD dev_write(DEVCTX *dev, OPENCTX *oc, const void *buf, DWORD len) {
  const BYTE *p = (const BYTE *)buf;
  DWORD i;
  (void)oc;
  cs_lock(&dev->lock);
  for (i = 0; i < len; i++) {
    if (outCount == OUT_CAP) {        /* full: drop oldest */
      outHead++; if (outHead >= OUT_CAP) outHead = 0;
      outCount--;
      dev->droppedBytes++;
    }
    outq[outTail] = p[i];
    outTail++; if (outTail >= OUT_CAP) outTail = 0;
    outCount++;
  }
  if (outCount > 0) EventModify(dev->hOutEvent, EVENT_SET);
  cs_unlock(&dev->lock);
  return len;                          /* writes are atomic per call */
}

static BOOL ioctl_fail(DWORD e) { SetLastError(e); return FALSE; }

static BOOL dev_ioctl(DEVCTX *dev, OPENCTX *oc, DWORD code,
                      PBYTE in, DWORD inLen, PBYTE out, DWORD outLen,
                      PDWORD outUsed) {
  switch (code) {
  case IOCTL_CON_GETOUTPUT: {
    DWORD n = 0;
    if (!out && outLen) return ioctl_fail(ERROR_INVALID_PARAMETER);
    cs_lock(&dev->lock);
    while (n < outLen && outCount > 0) {
      out[n++] = outq[outHead];
      outHead++; if (outHead >= OUT_CAP) outHead = 0;
      outCount--;
    }
    if (outCount == 0) EventModify(dev->hOutEvent, EVENT_RESET);
    cs_unlock(&dev->lock);
    if (outUsed) *outUsed = n;
    return TRUE;
  }
  case IOCTL_CON_WAITOUTPUT: {
    DWORD timeout;
    if (!in || inLen < 4) return ioctl_fail(ERROR_INVALID_PARAMETER);
    timeout = *(DWORD *)in;
    if (outCount > 0) return TRUE;
    WaitForSingleObject(dev->hOutEvent, timeout);
    return outCount > 0 ? TRUE : FALSE;
  }
  case IOCTL_CON_PUTINPUT: {
    const BYTE *w = (const BYTE *)in;
    DWORD want = inLen, put = 0;
    if (!in && inLen) return ioctl_fail(ERROR_INVALID_PARAMETER);
    cs_lock(&dev->lock);
    while (put < want) {
      BYTE c;
      if (inCount == IN_CAP) { dev->droppedInput += (want - put); break; }
      c = w[put++];
      inq[inTail] = c;
      inTail++; if (inTail >= IN_CAP) inTail = 0;
      inCount++;
      if (dev->echoInput && c != '\n') {
        /* cooked-mode echo: CR is echoed as CRLF; LF is not echoed
         * (it is expected to follow the CR of the same keystroke) */
        DWORD k, nch = (c == '\r') ? 2 : 1;
        for (k = 0; k < nch; k++) {
          BYTE ec = (k == 0) ? c : (BYTE)'\n';
          if (outCount == OUT_CAP) {
            outHead++; if (outHead >= OUT_CAP) outHead = 0;
            outCount--; dev->droppedBytes++;
          }
          outq[outTail] = ec; outTail++; if (outTail >= OUT_CAP) outTail = 0; outCount++;
        }
        EventModify(dev->hOutEvent, EVENT_SET);
      }
    }
    if (inCount > 0) EventModify(dev->hInEvent, EVENT_SET);
    cs_unlock(&dev->lock);
    if (outUsed) *outUsed = put;
    return TRUE;
  }
  case IOCTL_CON_SETECHO:
    if (!in || inLen < 4) return ioctl_fail(ERROR_INVALID_PARAMETER);
    dev->echoInput = *(DWORD *)in ? 1 : 0;
    return TRUE;
  case IOCTL_CON_PEEKINPUT:
    if (!out || outLen < 4) return ioctl_fail(ERROR_INVALID_PARAMETER);
    *(DWORD *)out = inCount;
    if (outUsed) *outUsed = 4;
    return TRUE;
  case IOCTL_CON_PEEKOUTPUT:
    if (!out || outLen < 4) return ioctl_fail(ERROR_INVALID_PARAMETER);
    *(DWORD *)out = outCount;
    if (outUsed) *outUsed = 4;
    return TRUE;
  case IOCTL_CON_FLUSH: {
    DWORD flags;
    if (!in || inLen < 4) return ioctl_fail(ERROR_INVALID_PARAMETER);
    flags = *(DWORD *)in;
    cs_lock(&dev->lock);
    if (flags & 1) {
      inHead = inTail = inCount = 0;
      if (!dev->inputEOF) EventModify(dev->hInEvent, EVENT_RESET);
    }
    if (flags & 2) {
      outHead = outTail = outCount = 0;
      EventModify(dev->hOutEvent, EVENT_RESET);
    }
    cs_unlock(&dev->lock);
    return TRUE;
  }
  case IOCTL_CON_CLOSEINPUT:
    cs_lock(&dev->lock);
    dev->inputEOF = 1;
    EventModify(dev->hInEvent, EVENT_SET);   /* wake readers -> EOF */
    cs_unlock(&dev->lock);
    return TRUE;
  case IOCTL_CON_GETINFO: {
    CON_INFO *ci = (CON_INFO *)out;
    if (!out || outLen < sizeof(CON_INFO)) return ioctl_fail(ERROR_INVALID_PARAMETER);
    ci->version = 0x00010000;
    ci->openCount = (DWORD)dev->openCount;
    ci->totalOpens = (DWORD)dev->totalOpens;
    ci->droppedBytes = dev->droppedBytes;
    ci->inputCap = IN_CAP;
    ci->outputCap = OUT_CAP;
    ci->inputQueued = inCount;
    ci->outputQueued = outCount;
    if (outUsed) *outUsed = sizeof(CON_INFO);
    return TRUE;
  }
  case IOCTL_CON_SETDEFAULTTIMEOUT:
    if (!in || inLen < 4) return ioctl_fail(ERROR_INVALID_PARAMETER);
    dev->defaultReadTimeout = *(DWORD *)in;
    return TRUE;
  case IOCTL_CON_SETREADTIMEOUT:
    if (!in || inLen < 4) return ioctl_fail(ERROR_INVALID_PARAMETER);
    oc->readTimeout = *(DWORD *)in;
    return TRUE;
  default:
    return ioctl_fail(ERROR_INVALID_FUNCTION);
  }
}
