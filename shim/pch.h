/* macOS/arm64 compatibility shim replacing the Win32 PCH.H used by Emu71.
 * Provides just enough of the Win32 API surface for the Saturn CPU core. */
#ifndef EMU71_SHIM_PCH_H
#define EMU71_SHIM_PCH_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <time.h>
#include <mach/mach_time.h>

typedef void            VOID;
typedef int             BOOL;
typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef uint32_t        DWORD;
typedef unsigned int    UINT;
typedef int             INT;
typedef int32_t         LONG;
typedef short           SHORT;
typedef unsigned long long ULONGLONG;
typedef long long       LONGLONG;
typedef BYTE           *LPBYTE;
typedef WORD           *LPWORD;
typedef DWORD          *LPDWORD;
typedef void           *LPVOID;
typedef char            TCHAR;
typedef char           *LPTSTR;
typedef const char     *LPCTSTR;
typedef char           *LPSTR;
typedef const char     *LPCSTR;
typedef void           *HANDLE;
typedef intptr_t        LRESULT;

#define TRUE   1
#define FALSE  0
#define CALLBACK
#define WINAPI
#define MAX_PATH 1024
#define INFINITE 0xFFFFFFFFu
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define _T(x) x
#define lstrcpy strcpy
#define lstrlen strlen
#define lstrcmpi strcasecmp
#define ZeroMemory(p,n) memset((p),0,(n))
#define CopyMemory(d,s,n) memcpy((d),(s),(n))
#define LOBYTE(w) ((BYTE)((w)&0xFF))
#define HIBYTE(w) ((BYTE)(((w)>>8)&0xFF))
#define LOWORD(l) ((WORD)((l)&0xFFFF))
#define HIWORD(l) ((WORD)(((l)>>16)&0xFFFF))
#define _ASSERT(x) ((void)0)
#define VERIFY(f) ((void)(f))
#define CLL(x) x##LL
#define OutputDebugString(s) ((void)0)

typedef union _LARGE_INTEGER {
	struct { DWORD LowPart; LONG HighPart; };
	LONGLONG QuadPart;
} LARGE_INTEGER;

/* ---- critical sections (recursive mutex, like Win32) ---- */
typedef pthread_mutex_t CRITICAL_SECTION;
static inline void InitializeCriticalSection(CRITICAL_SECTION *cs)
{
	pthread_mutexattr_t a; pthread_mutexattr_init(&a);
	pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(cs, &a); pthread_mutexattr_destroy(&a);
}
static inline void DeleteCriticalSection(CRITICAL_SECTION *cs) { pthread_mutex_destroy(cs); }
static inline void EnterCriticalSection(CRITICAL_SECTION *cs) { pthread_mutex_lock(cs); }
static inline void LeaveCriticalSection(CRITICAL_SECTION *cs) { pthread_mutex_unlock(cs); }

/* ---- time: QPC counts microseconds so 32-bit LowPart arithmetic matches 1 MHz ---- */
static inline DWORD timeGetTime(void)
{
	static mach_timebase_info_data_t tb;
	if (!tb.denom) mach_timebase_info(&tb);
	return (DWORD)((mach_absolute_time() * tb.numer / tb.denom) / 1000000ull);
}
static inline BOOL QueryPerformanceFrequency(LARGE_INTEGER *f) { f->QuadPart = 1000000; return TRUE; }
static inline BOOL QueryPerformanceCounter(LARGE_INTEGER *c)
{
	static mach_timebase_info_data_t tb;
	if (!tb.denom) mach_timebase_info(&tb);
	c->QuadPart = (LONGLONG)((mach_absolute_time() * tb.numer / tb.denom) / 1000ull);
	return TRUE;
}
static inline void Sleep(DWORD ms)
{
	if (ms == 0) { sched_yield(); return; }
	usleep(ms * 1000u);
}

/* ---- events and threads share the HANDLE type, distinguished by a tag ---- */
#define TAG_EVENT  0x45564e54
#define TAG_THREAD 0x54485244
typedef struct { int tag; pthread_mutex_t m; pthread_cond_t c; int set; int manual; } EMU_EVENT;
typedef struct { int tag; pthread_t t; int joined; } EMU_THREAD;

static inline HANDLE CreateEvent(void *attr, BOOL manual, BOOL init, const char *name)
{
	(void)attr; (void)name;
	EMU_EVENT *e = (EMU_EVENT *)calloc(1, sizeof(*e));
	e->tag = TAG_EVENT;
	pthread_mutex_init(&e->m, NULL); pthread_cond_init(&e->c, NULL);
	e->manual = manual; e->set = init; return e;
}
static inline BOOL SetEvent(HANDLE h)
{
	EMU_EVENT *e = (EMU_EVENT *)h;
	pthread_mutex_lock(&e->m); e->set = 1; pthread_cond_broadcast(&e->c); pthread_mutex_unlock(&e->m);
	return TRUE;
}
static inline BOOL ResetEvent(HANDLE h)
{
	EMU_EVENT *e = (EMU_EVENT *)h;
	pthread_mutex_lock(&e->m); e->set = 0; pthread_mutex_unlock(&e->m);
	return TRUE;
}
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT  0x102
static inline DWORD WaitForSingleObject(HANDLE h, DWORD ms)
{
	if (((EMU_THREAD *)h)->tag == TAG_THREAD) {
		EMU_THREAD *t = (EMU_THREAD *)h;
		if (!t->joined) { pthread_join(t->t, NULL); t->joined = 1; }
		return WAIT_OBJECT_0;
	}
	EMU_EVENT *e = (EMU_EVENT *)h; DWORD r = WAIT_OBJECT_0;
	pthread_mutex_lock(&e->m);
	if (ms == INFINITE) {
		while (!e->set) pthread_cond_wait(&e->c, &e->m);
	} else {
		struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += ms / 1000; ts.tv_nsec += (long)(ms % 1000) * 1000000L;
		if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
		while (!e->set) if (pthread_cond_timedwait(&e->c, &e->m, &ts)) { r = WAIT_TIMEOUT; break; }
	}
	if (r == WAIT_OBJECT_0 && !e->manual) e->set = 0;
	pthread_mutex_unlock(&e->m);
	return r;
}

typedef struct { void *(*fn)(void *); void *arg; } EMU_THREAD_START;
static inline void *emu_thread_tramp(void *p)
{
	EMU_THREAD_START s = *(EMU_THREAD_START *)p; free(p);
	return s.fn(s.arg);
}
static inline HANDLE emu_CreateThread(void *(*fn)(void *), void *arg)
{
	EMU_THREAD *t = (EMU_THREAD *)calloc(1, sizeof(*t));
	EMU_THREAD_START *s = (EMU_THREAD_START *)malloc(sizeof(*s));
	t->tag = TAG_THREAD; s->fn = fn; s->arg = arg;
	pthread_create(&t->t, NULL, emu_thread_tramp, s);
	return t;
}
#define CreateThread(attr,stack,fn,arg,flags,id) emu_CreateThread((void *(*)(void *))(fn),(arg))
static inline BOOL CloseHandle(HANDLE h) { (void)h; return TRUE; }


/* ---- opaque GUI types referenced only by extern declarations ---- */
typedef const void *LPCVOID;
typedef void *HPALETTE, *HINSTANCE, *HWND, *HDC, *HRGN, *HCURSOR, *HMENU, *HBITMAP, *HFONT, *HICON;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef DWORD COLORREF;
typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { LONG x, y; } POINT;


#define MB_APPLMODAL 0
#define MB_OK 0
#define MB_ICONINFORMATION 0
#define MB_ICONSTOP 0
#define MB_SETFOREGROUND 0
static inline int MessageBox(void *w, const char *t, const char *c, unsigned f) { (void)w; (void)f; fprintf(stderr, "[%s] %s\n", c ? c : "", t); return 1; }


#define MB_YESNO 0
#define MB_ICONEXCLAMATION 0
#define MB_YESNOCANCEL 0
#define CONST const
#define UNREFERENCED_PARAMETER(p) ((void)(p))
typedef uintptr_t DWORD_PTR;

/* ---- system time ---- */
#include <sys/time.h>
typedef struct { WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; } SYSTEMTIME;
static inline void GetLocalTime(SYSTEMTIME *st)
{
	struct timeval tv; gettimeofday(&tv, NULL);
	struct tm tm; time_t t = tv.tv_sec; localtime_r(&t, &tm);
	st->wYear = tm.tm_year + 1900; st->wMonth = tm.tm_mon + 1; st->wDayOfWeek = tm.tm_wday;
	st->wDay = tm.tm_mday; st->wHour = tm.tm_hour; st->wMinute = tm.tm_min;
	st->wSecond = tm.tm_sec; st->wMilliseconds = tv.tv_usec / 1000;
}

/* ---- multimedia timers: one detached thread per one-shot event ---- */
typedef struct { UINT wPeriodMin, wPeriodMax; } TIMECAPS;
#define TIME_ONESHOT 0
#define TIMERR_NOERROR 0
typedef void (*LPTIMECALLBACK)(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
static inline UINT timeGetDevCaps(TIMECAPS *tc, UINT sz) { (void)sz; tc->wPeriodMin = 1; tc->wPeriodMax = 1000; return 0; }
static inline UINT timeBeginPeriod(UINT p) { (void)p; return 0; }
static inline UINT timeEndPeriod(UINT p) { (void)p; return 0; }
typedef struct { UINT delay; LPTIMECALLBACK cb; DWORD_PTR user; } EMU_TIMER_ARG;
static inline void *emu_timer_thread(void *p)
{
	EMU_TIMER_ARG a = *(EMU_TIMER_ARG *)p; free(p);
	usleep(a.delay * 1000u);
	a.cb(1, 0, a.user, 0, 0);
	return NULL;
}
static inline UINT timeSetEvent(UINT delay, UINT res, LPTIMECALLBACK cb, DWORD_PTR user, UINT flags)
{
	(void)res; (void)flags;
	EMU_TIMER_ARG *a = (EMU_TIMER_ARG *)malloc(sizeof(*a));
	a->delay = delay; a->cb = cb; a->user = user;
	pthread_t t; pthread_attr_t at; pthread_attr_init(&at);
	pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
	pthread_create(&t, &at, emu_timer_thread, a); pthread_attr_destroy(&at);
	return 1;
}


#define __forceinline __attribute__((always_inline))
typedef struct { BYTE ACLineStatus, BatteryFlag; } SYSTEM_POWER_STATUS;
#define AC_LINE_OFFLINE 0
#define BATTERY_FLAG_LOW 2
#define BATTERY_FLAG_CRITICAL 4
static inline BOOL GetSystemPowerStatus(SYSTEM_POWER_STATUS *s) { s->ACLineStatus = 1; s->BatteryFlag = 0; return TRUE; }

#define wsprintf sprintf
typedef int SOCKET;
#define INVALID_SOCKET (-1)

#endif /* EMU71_SHIM_PCH_H */
