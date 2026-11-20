/* conioctl.h -- BrainCon console stream driver: public interface.
 *
 * Shared by the driver (braincon.c) and its consumers (the terminal
 * app, and ANY other guest app that wants a console). This header is
 * the channel protocol definition; see README.md next to the sources
 * for the full documentation.
 *
 * Device:   stream device "CON1:" (prefix CON, index 1), implemented
 *           by BrainCon.dll, loaded by device.exe via ActivateDeviceEx.
 * Data:     the console is a BYTE stream. Text on it is whatever the
 *           writer's runtime produces -- for CE CRT stdio consumers
 *           (TxtShell) that is ANSI/MBCS bytes, because coredll's
 *           fputws/fgetws convert through the stream's byte mode.
 *           The driver itself is encoding-agnostic.
 * Channel:  apps use plain ReadFile/WriteFile on CON1: for console
 *           I/O. The TERMINAL side of the channel (the window that
 *           renders output and sources keystrokes) talks to the
 *           driver through the IOCTLs below on its own CON1: handle.
 *           The driver is independent of any terminal: output written
 *           with no terminal attached is buffered (ring, drop-oldest)
 *           and delivered to the next terminal that drains it.
 */
#ifndef CONIOCTL_H
#define CONIOCTL_H

/* CTL_CODE(FILE_DEVICE_UNKNOWN=0x22, fn, METHOD_BUFFERED=0, FILE_ANY_ACCESS=0) */
#define CON_CTL(fn)  ((0x22 << 16) | ((fn) << 2))

/* Drain pending console output into the caller's buffer (non-blocking).
 * In: none. Out: raw bytes as written by console writers.
 * *lpBytesReturned = bytes copied (0 if none pending). */
#define IOCTL_CON_GETOUTPUT    CON_CTL(0x800)

/* Block until output is pending or the timeout expires.
 * In: DWORD timeout_ms. Returns TRUE (output pending) or FALSE
 * (timeout / bad parameter). */
#define IOCTL_CON_WAITOUTPUT   CON_CTL(0x801)

/* Queue bytes as console input (keystrokes). In: byte array (not
 * NUL-terminated; length from dwInBufferSize). Readers of CON1:
 * receive these bytes from ReadFile. */
#define IOCTL_CON_PUTINPUT     CON_CTL(0x802)

/* Query how much input is queued. Out: DWORD = byte count. */
#define IOCTL_CON_PEEKINPUT    CON_CTL(0x803)

/* Query how much output is pending. Out: DWORD = byte count. */
#define IOCTL_CON_PEEKOUTPUT   CON_CTL(0x804)

/* Discard queued data. In: DWORD flags: 1 = input, 2 = output. */
#define IOCTL_CON_FLUSH        CON_CTL(0x805)

/* Signal end-of-input: pending and future ReadFile calls on CON1:
 * return 0 (EOF) once the input queue is empty, instead of blocking.
 * Sticky until the driver is deactivated. */
#define IOCTL_CON_CLOSEINPUT   CON_CTL(0x806)

/* Driver information. Out: CON_INFO below. */
#define IOCTL_CON_GETINFO      CON_CTL(0x807)

/* Set the default read timeout for NEW opens. In: DWORD ms.
 * 0xFFFFFFFF (default) = reads block indefinitely. A per-open timeout
 * can also be set by IOCTL_CON_SETREADTIMEOUT on an open handle. */
#define IOCTL_CON_SETDEFAULTTIMEOUT CON_CTL(0x808)

/* Set the read timeout of THIS open. In: DWORD ms (0 = poll,
 * 0xFFFFFFFF = block indefinitely). */
#define IOCTL_CON_SETREADTIMEOUT    CON_CTL(0x809)

/* Enable/disable input echo. In: DWORD 0/1 (default 1). When on,
 * bytes queued via IOCTL_CON_PUTINPUT are also appended to the
 * output stream, like a real console in cooked mode (CR is echoed
 * as CRLF; a following LF is not echoed). */
#define IOCTL_CON_SETECHO           CON_CTL(0x80A)

typedef struct {
  unsigned long version;      /* driver version, 0x00010000 = 1.0 */
  unsigned long openCount;    /* currently open handles */
  unsigned long totalOpens;   /* opens since Init */
  unsigned long droppedBytes; /* output bytes dropped (ring full) */
  unsigned long inputCap;     /* input ring capacity, in bytes */
  unsigned long outputCap;    /* output ring capacity, in bytes */
  unsigned long inputQueued;  /* bytes currently queued */
  unsigned long outputQueued; /* bytes currently pending */
} CON_INFO;

#endif /* CONIOCTL_H */
