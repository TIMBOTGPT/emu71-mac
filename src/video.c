/*
 * video.c - memory-mapped-I/O mailbox video device (prototype)
 *
 * The Saturn side sees 128 nibbles of MMIO (register map in docs/video-architecture.md).
 * Commands execute synchronously inside the write callback; pixels live in host buffers.
 * New code only, the Saturn core in orig/ is untouched.
 */
#include "pch.h"
#include "Emu71.h"
#include "ops.h"
#include "io.h"
#include "portmem.h"
#include <pthread.h>
#include <math.h>
#include "video.h"

/* mailbox offsets */
enum { R0 = 0x00, R1 = 0x10, R2 = 0x20, R3 = 0x30, I0 = 0x40, I1 = 0x44, I2 = 0x48, I3 = 0x4C, DATA = 0x50,
       STAT = 0x60, ERRC = 0x61, VER = 0x62, TGT = 0x63, LEN = 0x64, LPTR = 0x66, COL = 0x6B, PAL = 0x6D, FLAGS = 0x6F, CMD = 0x7E };
enum { E_OK = 0, E_CMD = 1, E_LAYER = 2, E_RANGE = 3, E_SCALE = 4 };

#define NLAYERS 16

typedef struct {
	BOOL   cfg;
	DWORD  base;
	BYTE   mb[VID_NIBBLES];
	int    W, H;
	BYTE  *layer[NLAYERS];
	int    shown;
	double xmin, xmax, ymin, ymax;
	int    lx0, ly0, lx1, ly1;		/* pixel limit rectangle, inclusive, origin top-left */
	double penx, peny;
	int    pendown, color;
	uint32_t pal[256];
	int    visible, dirty;
	pthread_mutex_t mu;
} VID;

static VID *V;
static PPORTACC vidAcc;
static int reqW = 800, reqH = 600;
static int vidDev = VID_DEV_ID;		/* MMIO class nibble, EMU71_VIDDEV overrides for experiments */

void VideoSetSize(int w, int h) { if (w >= 16 && w <= 2048 && h >= 16 && h <= 2048) { reqW = w; reqH = h; } }

/* ---------- mailbox field access ---------- */
static int get_u(int off, int n) { int v = 0, i; for (i = n - 1; i >= 0; --i) v = (v << 4) | (V->mb[off + i] & 0xF); return v; }
static int get_s16(int off) { int v = get_u(off, 4); return v >= 0x8000 ? v - 0x10000 : v; }
static void put_u(int off, int n, int v) { int i; for (i = 0; i < n; ++i) { V->mb[off + i] = v & 0xF; v >>= 4; } }
static void put_s16(int off, int v) { put_u(off, 4, v & 0xFFFF); }

/*
 * HP-71 REAL, 16 nibbles. Assumed layout (UNVERIFIED against a real variable):
 * nibbles 0-2 exponent (BCD, 10's complement when negative), 3-14 mantissa with nibble 14 most
 * significant, nibble 15 sign (0 = +, non-zero = -).
 */
static double real_dec(const BYTE *n)
{
	int e = (n[0] & 0xF) + 10 * (n[1] & 0xF) + 100 * (n[2] & 0xF), k;
	double m = 0, v;
	if (e >= 500) e -= 1000;
	for (k = 14; k >= 3; --k) m = m * 10 + (n[k] & 0xF);
	v = m / 1e11 * pow(10.0, e);
	return (n[15] & 0xF) ? -v : v;
}
static void real_enc(BYTE *n, double v)
{
	int neg = v < 0, e, k; double m; long long q;
	memset(n, 0, 16);
	if (v == 0 || v != v) return;
	if (neg) v = -v;
	e = (int)floor(log10(v));
	m = v / pow(10.0, e);
	q = (long long)floor(m * 1e11 + 0.5);
	if (q >= 1000000000000LL) { q /= 10; ++e; }
	if (q < 100000000000LL) { q *= 10; --e; }
	for (k = 3; k <= 14; ++k) { n[k] = (BYTE)(q % 10); q /= 10; }
	if (e < 0) e += 1000;
	n[0] = e % 10; n[1] = (e / 10) % 10; n[2] = (e / 100) % 10;
	n[15] = neg ? 9 : 0;
}

/* ---------- layers and drawing ---------- */
static BYTE *layer_get(int n)
{
	if (n < 0 || n >= NLAYERS) return NULL;
	if (!V->layer[n]) V->layer[n] = (BYTE *)calloc((size_t)V->W * V->H, 1);
	return V->layer[n];
}
static void setpix(BYTE *L, int x, int y, int c)
{
	if (x < V->lx0 || x > V->lx1 || y < V->ly0 || y > V->ly1) return;
	L[(size_t)y * V->W + x] = (BYTE)c;
	if (L == V->layer[V->shown]) V->dirty = 1;
}
static int clampi(double v) { return v > 1e6 ? 1000000 : v < -1e6 ? -1000000 : (int)floor(v + 0.5); }
static int wx2px(double x) { return clampi(V->lx0 + (x - V->xmin) / (V->xmax - V->xmin) * (V->lx1 - V->lx0)); }
static int wy2py(double y) { return clampi(V->ly1 - (y - V->ymin) / (V->ymax - V->ymin) * (V->ly1 - V->ly0)); }
static void line(BYTE *L, int x0, int y0, int x1, int y1, int c)
{
	int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy, e2;
	for (;;) {
		setpix(L, x0, y0, c);
		if (x0 == x1 && y0 == y1) break;
		e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
}

static void reset_state(void)
{
	int i;
	V->xmin = 0; V->xmax = V->W - 1; V->ymin = 0; V->ymax = V->H - 1;
	V->lx0 = 0; V->ly0 = 0; V->lx1 = V->W - 1; V->ly1 = V->H - 1;
	V->penx = V->peny = 0; V->pendown = 1; V->color = 1; V->shown = 0;
	for (i = 0; i < NLAYERS; ++i) if (V->layer[i]) memset(V->layer[i], 0, (size_t)V->W * V->H);
	for (i = 0; i < 256; ++i) {
		static const uint32_t base[8] = { 0x000000, 0xFFFFFF, 0xFF3030, 0x30FF30, 0x3050FF, 0xFFFF30, 0x30FFFF, 0xFF30FF };
		uint32_t g = (uint32_t)(i * 255 / 255);
		V->pal[i] = 0xFF000000u | (i < 8 ? base[i] : (g << 16) | (g << 8) | g);
	}
	V->dirty = 1;
}

static void move_to(double x, double y, int draw)
{
	BYTE *L = layer_get(get_u(TGT, 1));
	if (draw && V->pendown && L) line(L, wx2px(V->penx), wy2py(V->peny), wx2px(x), wy2py(y), V->color);
	V->penx = x; V->peny = y;
}

static int exec_cmd(int cmd)
{
	BYTE *L = layer_get(get_u(TGT, 1));
	int x, y;
	double a, b;
	switch (cmd) {
	case 0x00: return E_OK;
	case 0x01: reset_state(); return E_OK;
	case 0x02: V->visible = 1; V->dirty = 1; return E_OK;			/* GRAPH */
	case 0x03: V->visible = 0; return E_OK;							/* ALPHA */
	case 0x04: if (!L) return E_LAYER; memset(L, get_u(COL, 2), (size_t)V->W * V->H); V->dirty = 1; return E_OK;	/* GCLEAR */
	case 0x05: a = real_dec(V->mb + R0); if (a < 0 || a > 255) return E_RANGE; V->color = (int)a; V->pendown = 1; return E_OK;	/* PEN n (R0) */
	case 0x06: V->pendown = 0; return E_OK;							/* PENUP */
	case 0x10:													/* SCALE */
		a = real_dec(V->mb + R1) - real_dec(V->mb + R0); b = real_dec(V->mb + R3) - real_dec(V->mb + R2);
		if (a == 0 || b == 0) return E_SCALE;
		V->xmin = real_dec(V->mb + R0); V->xmax = real_dec(V->mb + R1);
		V->ymin = real_dec(V->mb + R2); V->ymax = real_dec(V->mb + R3);
		return E_OK;
	case 0x11:													/* LIMIT: x0,y0,x1,y1 pixels */
		x = get_s16(I0); y = get_s16(I1);
		if (x < 0 || y < 0 || get_s16(I2) >= V->W || get_s16(I3) >= V->H || get_s16(I2) < x || get_s16(I3) < y) return E_RANGE;
		V->lx0 = x; V->ly0 = y; V->lx1 = get_s16(I2); V->ly1 = get_s16(I3);
		return E_OK;
	case 0x13: move_to(real_dec(V->mb + R0), real_dec(V->mb + R1), 0); return E_OK;			/* MOVE */
	case 0x14: move_to(real_dec(V->mb + R0), real_dec(V->mb + R1), 1); return E_OK;			/* DRAW */
	case 0x15: move_to(V->penx + real_dec(V->mb + R0), V->peny + real_dec(V->mb + R1), 0); return E_OK;	/* IMOVE */
	case 0x16: move_to(V->penx + real_dec(V->mb + R0), V->peny + real_dec(V->mb + R1), 1); return E_OK;	/* IDRAW */
	case 0x17: {													/* PLOT x,y,mode: 1 = line to, 2 = dot, else move */
		int mode = (int)real_dec(V->mb + R2); a = real_dec(V->mb + R0); b = real_dec(V->mb + R1);
		if (mode == 1) move_to(a, b, 1);
		else { move_to(a, b, 0); if (mode == 2 && L) setpix(L, wx2px(a), wy2py(b), V->color); }
		return E_OK;
	}
	case 0x40: case 0x41: {										/* XAXIS y,tick,x0,x1 / YAXIS x,tick,y0,y1 */
		double at = real_dec(V->mb + R0), tk = real_dec(V->mb + R1), lo = real_dec(V->mb + R2), hi = real_dec(V->mb + R3), t;
		int horiz = (cmd == 0x40);
		if (!L) return E_LAYER;
		if (horiz) line(L, wx2px(lo), wy2py(at), wx2px(hi), wy2py(at), V->color);
		else line(L, wx2px(at), wy2py(lo), wx2px(at), wy2py(hi), V->color);
		if (tk > 0 && (hi - lo) / tk < 2000) {
			for (t = ceil(lo / tk) * tk; t <= hi + tk * 1e-9; t += tk) {
				if (horiz) line(L, wx2px(t), wy2py(at) - 3, wx2px(t), wy2py(at) + 3, V->color);
				else line(L, wx2px(at) - 3, wy2py(t), wx2px(at) + 3, wy2py(t), V->color);
			}
		}
		V->dirty = 1; return E_OK;
	}
	case 0x18: real_enc(V->mb + R0, V->penx); real_enc(V->mb + R1, V->peny); return E_OK;	/* WHERE */
	case 0x50: if (!L) return E_LAYER; x = get_s16(I0); y = get_s16(I1);							/* PGET */
		if (x < 0 || y < 0 || x >= V->W || y >= V->H) return E_RANGE;
		put_u(COL, 2, L[(size_t)y * V->W + x]); return E_OK;
	case 0x51: if (!L) return E_LAYER; x = get_s16(I0); y = get_s16(I1);							/* PSET */
		if (x < 0 || y < 0 || x >= V->W || y >= V->H) return E_RANGE;
		L[(size_t)y * V->W + x] = (BYTE)get_u(COL, 2); if (L == V->layer[V->shown]) V->dirty = 1; return E_OK;
	case 0x60: x = get_u(PAL, 2);																	/* COLORMAP: DATA = RRGGBB */
		V->pal[x] = 0xFF000000u | (get_u(DATA, 2) << 16) | (get_u(DATA + 2, 2) << 8) | get_u(DATA + 4, 2);
		V->dirty = 1; return E_OK;
	case 0x72: a = real_dec(V->mb + R0); if (a < 0 || a >= NLAYERS) return E_RANGE;					/* GTARGET n (R0) */
		x = (int)a; if (!layer_get(x)) return E_LAYER; put_u(TGT, 1, x); return E_OK;
	case 0x77: a = real_dec(V->mb + R0); if (a < 0 || a >= NLAYERS) return E_RANGE;					/* GSHOW n (R0) */
		x = (int)a; if (!layer_get(x)) return E_LAYER; V->shown = x; V->dirty = 1; return E_OK;
	case 0x75: {																					/* GSWAP target <-> I0 */
		int o = get_u(I0, 1), t = get_u(TGT, 1);
		BYTE *A = layer_get(t), *B = layer_get(o);
		if (!A || !B) return E_LAYER;
		V->layer[t] = B; V->layer[o] = A; V->dirty = 1; return E_OK;
	}
	case 0x80: return E_OK;																			/* LSEEK: LPTR written directly */
	case 0x81: case 0x82: {																			/* VPOKE / VPEEK with auto-increment */
		int p = get_u(LPTR, 5);
		if (!L) return E_LAYER;
		if (p >= V->W * V->H) return E_RANGE;
		if (cmd == 0x81) { L[p] = (BYTE)get_u(COL, 2); if (L == V->layer[V->shown]) V->dirty = 1; } else put_u(COL, 2, L[p]);
		if (!(get_u(FLAGS, 1) & 2)) put_u(LPTR, 5, p + 1);
		return E_OK;
	}
	}
	return E_CMD;
}

/* ---------- port interface (same shape as the HP-IL module) ---------- */
static BOOL IsModuleVid(HANDLE h, DWORD d)
{
	return V->cfg && ((V->base ^ d) & ~(DWORD)(VID_NIBBLES - 1)) == 0;
}
static BOOL ConfigVid(HANDLE h)
{
	if (V->cfg) return FALSE;
	V->cfg = TRUE;
	V->base = Npack(Chipset.C, 5) & ~(DWORD)(VID_NIBBLES - 1);
	fprintf(stderr, "VIDEO: configured at %05X\n", (unsigned)V->base);
	return TRUE;
}
static VOID UncnfgVid(HANDLE h)
{
	DWORD p = Npack(Chipset.C, 5);
	if (V->cfg && (p & ~(DWORD)(VID_NIBBLES - 1)) == V->base) V->cfg = FALSE;
}
static VOID ResetVid(HANDLE h) { V->cfg = FALSE; V->base = 0; }
static DWORD C_Eq_IdVid(HANDLE h)
{
	/* ID nibbles high to low: last-chip(8) class MMIO(F) reserved(0) size. HP-IL is 0x80F0F, class 0. Ours: class vidDev, 128 nibbles = C */
	return V->cfg ? 0 : (0x80F0Cu | ((DWORD)vidDev << 12));
}
static VOID WriteIOVid(HANDLE h, BYTE *a, DWORD d, LPDWORD ps)
{
	UINT i; int trigger = 0;
	d &= VID_NIBBLES - 1;
	*ps = MIN(*ps, VID_NIBBLES - d);
	pthread_mutex_lock(&V->mu);
	for (i = 0; i < *ps; ++i, ++d) {
		if ((d >= STAT && d <= VER) || (d >= 0x70 && d < CMD)) continue;	/* read-only registers and signature */
		V->mb[d] = a[i] & 0xF;
		if (d == CMD + 1) trigger = 1;
	}
	if (trigger) {
		int err = exec_cmd(get_u(CMD, 2));
		V->mb[ERRC] = err;
		if (getenv("EMU71_VIDLOG")) fprintf(stderr, "VIDEO: cmd %02X err %d R0..R3 = %g %g %g %g\n", get_u(CMD, 2), err, real_dec(V->mb + R0), real_dec(V->mb + R1), real_dec(V->mb + R2), real_dec(V->mb + R3));
		V->mb[STAT] = (err ? 2 : 0) | (V->visible ? 4 : 0) | (V->dirty ? 8 : 0);
	}
	pthread_mutex_unlock(&V->mu);
}
static VOID ReadIOVid(HANDLE h, BYTE *a, DWORD d, LPDWORD ps, BOOL bUpdate)
{
	d &= VID_NIBBLES - 1;
	*ps = MIN(*ps, VID_NIBBLES - d);
	pthread_mutex_lock(&V->mu);
	memcpy(a, V->mb + d, *ps);
	pthread_mutex_unlock(&V->mu);
}
static VOID DetachVid(PPORTACC *ppsPort)
{
	PPORTACC pAct = *ppsPort;
	*ppsPort = (*ppsPort)->pNext;
	VideoShutdown();
	free(pAct);
}

HANDLE AllocVideoMem(UINT nType)
{
	int i;
	if (V) return (HANDLE)V;
	V = (VID *)calloc(1, sizeof *V);
	V->W = reqW; V->H = reqH;
	pthread_mutex_init(&V->mu, NULL);
	reset_state();
	layer_get(0);
	V->mb[VER] = 1;
	{ const char *sg = "VIDEO71"; int k; for (k = 0; k < 7; ++k) { V->mb[0x70 + 2*k] = sg[k] & 0xF; V->mb[0x71 + 2*k] = (sg[k] >> 4) & 0xF; } }
	if (getenv("EMU71_VIDDEV")) vidDev = atoi(getenv("EMU71_VIDDEV")) & 0xF;
	(void)i;
	return (HANDLE)V;
}
BOOL AttachVideoMem(PPORTACC *ppsPort, HANDLE h)
{
	while (*ppsPort != NULL) ppsPort = &(*ppsPort)->pNext;
	if (!h) return FALSE;
	if ((*ppsPort = AllocPortMem()) == NULL) return FALSE;
	(*ppsPort)->h = h;
	(*ppsPort)->pfnIsModule = IsModuleVid;
	(*ppsPort)->pfnConfig = ConfigVid;
	(*ppsPort)->pfnUncnfg = UncnfgVid;
	(*ppsPort)->pfnReset = ResetVid;
	(*ppsPort)->pfnC_Eq_Id = C_Eq_IdVid;
	(*ppsPort)->pfnWriteIO = WriteIOVid;
	(*ppsPort)->pfnReadIO = ReadIOVid;
	(*ppsPort)->pfnDetachMem = DetachVid;
	vidAcc = *ppsPort;
	return TRUE;
}

/* ---------- SDL window (main thread only) ---------- */
static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static uint32_t *frame;

void VideoPump(void)
{
	int vis, dirty;
	if (!V) return;
	pthread_mutex_lock(&V->mu);
	vis = V->visible; dirty = V->dirty;
	if (vis && !win) {
		win = SDL_CreateWindow("Emu71 - Video", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, V->W, V->H, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
		ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
		SDL_RenderSetLogicalSize(ren, V->W, V->H);
		tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, V->W, V->H);
		frame = (uint32_t *)malloc((size_t)V->W * V->H * 4);
		dirty = 1;
	}
	if (win) {
		if (vis && !(SDL_GetWindowFlags(win) & SDL_WINDOW_SHOWN)) { SDL_ShowWindow(win); dirty = 1; }
		if (!vis && (SDL_GetWindowFlags(win) & SDL_WINDOW_SHOWN)) SDL_HideWindow(win);
		if (vis && dirty) {
			BYTE *L = V->layer[V->shown];
			size_t i, n = (size_t)V->W * V->H;
			for (i = 0; i < n; ++i) frame[i] = V->pal[L ? L[i] : 0];
			V->dirty = 0;
			pthread_mutex_unlock(&V->mu);
			SDL_UpdateTexture(tex, NULL, frame, V->W * 4);
			SDL_RenderClear(ren);
			SDL_RenderCopy(ren, tex, NULL, NULL);
			SDL_RenderPresent(ren);
			return;
		}
	}
	pthread_mutex_unlock(&V->mu);
}

int VideoEvent(SDL_Event *e)
{
	Uint32 id;
	if (!V || !win) return 0;
	id = SDL_GetWindowID(win);
	if (e->type == SDL_WINDOWEVENT && e->window.windowID == id) {
		if (e->window.event == SDL_WINDOWEVENT_CLOSE) { pthread_mutex_lock(&V->mu); V->visible = 0; pthread_mutex_unlock(&V->mu); }
		if (e->window.event == SDL_WINDOWEVENT_EXPOSED) { pthread_mutex_lock(&V->mu); V->dirty = 1; pthread_mutex_unlock(&V->mu); }
		return 1;
	}
	/* mouse clicks in the video window must not hit the calculator keys, keyboard goes through */
	if ((e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) && e->button.windowID == id) return 1;
	return 0;
}

void VideoShutdown(void)
{
	if (tex) { SDL_DestroyTexture(tex); tex = NULL; }
	if (ren) { SDL_DestroyRenderer(ren); ren = NULL; }
	if (win) { SDL_DestroyWindow(win); win = NULL; }
	free(frame); frame = NULL;
}

/* ---------- self test: uses the port entry points exactly as the CPU does ---------- */
static void vt_write(int off, const BYTE *n, int cnt)
{
	DWORD c = cnt;
	vidAcc->pfnWriteIO(vidAcc->h, (BYTE *)n, V->base + off, &c);
}
static void vt_read(int off, BYTE *n, int cnt)
{
	DWORD c = cnt;
	vidAcc->pfnReadIO(vidAcc->h, n, V->base + off, &c, FALSE);
}
static void vt_real(int slot, double v) { BYTE n[16]; real_enc(n, v); vt_write(slot, n, 16); }
static void vt_int(int off, int v) { BYTE n[4]; int i; for (i = 0; i < 4; ++i) { n[i] = (v >> (4 * i)) & 0xF; } vt_write(off, n, 4); }
static void vt_byte(int off, int v) { BYTE n[2] = { (BYTE)(v & 0xF), (BYTE)((v >> 4) & 0xF) }; vt_write(off, n, 2); }
static int vt_cmd(int cmd)
{
	BYTE n[2] = { (BYTE)(cmd & 0xF), (BYTE)((cmd >> 4) & 0xF) }, e;
	vt_write(CMD, n, 2);
	vt_read(ERRC, &e, 1);
	return e;
}

void VideoSnapshot(const char *ppmPath)
{
	if (!V) { fprintf(stderr, "VIDEO: no video module, snapshot skipped\n"); return; }
	FILE *f = fopen(ppmPath, "wb");
	if (f) {
		size_t n = (size_t)V->W * V->H, k; BYTE *L = V->layer[V->shown];
		fprintf(f, "P6\n%d %d\n255\n", V->W, V->H);
		for (k = 0; k < n; ++k) { uint32_t c = V->pal[L[k]]; fputc((c >> 16) & 255, f); fputc((c >> 8) & 255, f); fputc(c & 255, f); }
		fclose(f);
		fprintf(stderr, "VIDEO: wrote %s\n", ppmPath);
	}
}

void VideoSelfTest(const char *ppmPath)
{
	int fails = 0, i, e;
	BYTE rd[16];
	double testv[] = { 0, 1, -1, 3.14, -3.14, 1.5e-7, 123456789012.0, 9.99999999999e99, -2.5e-45 };

	if (!V || !vidAcc) { fprintf(stderr, "VIDTEST: no video module attached\n"); return; }
	if (!V->cfg) { fprintf(stderr, "VIDTEST: FAIL module not configured by the OS (no CONFIG seen)\n"); return; }
	fprintf(stderr, "VIDTEST: mailbox base %05X\n", (unsigned)V->base);

	/* 1. REAL encode/decode through the mailbox */
	for (i = 0; i < (int)(sizeof testv / sizeof *testv); ++i) {
		double back, rel;
		vt_real(R0, testv[i]); vt_read(R0, rd, 16); back = real_dec(rd);
		rel = testv[i] == 0 ? fabs(back) : fabs((back - testv[i]) / testv[i]);
		if (rel > 1e-11) { fprintf(stderr, "VIDTEST: FAIL real %.12g -> %.12g\n", testv[i], back); ++fails; }
	}
	/* 2. bad command reports an error, read-only regs ignore writes */
	e = vt_cmd(0xEE); if (e != E_CMD) { fprintf(stderr, "VIDTEST: FAIL bad cmd err=%d\n", e); ++fails; }
	{ BYTE z = 7, v; vt_write(VER, &z, 1); vt_read(VER, &v, 1); if (v != 1) { fprintf(stderr, "VIDTEST: FAIL VER writable\n"); ++fails; } }
	/* 3. sine demo: GRAPH, GCLEAR, PEN 1, SCALE -3.14,3.14,-1,1, axes, curve */
	vt_cmd(0x01); vt_cmd(0x02);
	vt_byte(COL, 0); vt_cmd(0x04);
	vt_real(R0, 1); vt_cmd(0x05);
	vt_real(R0, -3.14); vt_real(R1, 3.14); vt_real(R2, -1); vt_real(R3, 1);
	if ((e = vt_cmd(0x10)) != E_OK) { fprintf(stderr, "VIDTEST: FAIL SCALE err=%d\n", e); ++fails; }
	vt_real(R0, 4); vt_cmd(0x05);
	vt_real(R0, -3.14); vt_real(R1, 0); vt_cmd(0x13); vt_real(R0, 3.14); vt_real(R1, 0); vt_cmd(0x14);	/* x axis */
	vt_real(R0, 0); vt_real(R1, -1); vt_cmd(0x13); vt_real(R0, 0); vt_real(R1, 1); vt_cmd(0x14);			/* y axis */
	vt_real(R0, 3); vt_cmd(0x05);
	for (i = 0; i <= 200; ++i) {
		double x = -3.14 + 6.28 * i / 200.0;
		vt_real(R0, x); vt_real(R1, sin(x)); vt_real(R2, i == 0 ? 0 : 1); vt_cmd(0x17);
	}
	/* 4. WHERE returns the last point */
	vt_cmd(0x18); vt_read(R0, rd, 16);
	if (fabs(real_dec(rd) - 3.14) > 1e-9) { fprintf(stderr, "VIDTEST: FAIL WHERE x=%g\n", real_dec(rd)); ++fails; }
	/* 5. pixel readback: sine at x=0 sits on the axis crossing in the middle of the window */
	vt_int(I0, wx2px(0)); vt_int(I1, wy2py(0)); vt_cmd(0x50); vt_read(COL, rd, 2);
	fprintf(stderr, "VIDTEST: pixel at origin = %d (expect 3 = curve over axis)\n", rd[0] | (rd[1] << 4));
	if ((rd[0] | (rd[1] << 4)) != 3) ++fails;
	/* 6. linear VPOKE/VPEEK on a hidden layer 5 */
	{
		BYTE t = 5, p[5] = { 0, 0, 0, 0, 0 }, c; int k, ok = 1;
		vt_write(TGT, &t, 1); vt_write(LPTR, p, 5);
		for (k = 0; k < 4; ++k) { vt_byte(COL, 10 + k); vt_cmd(0x81); }
		vt_write(LPTR, p, 5);
		for (k = 0; k < 4; ++k) { vt_cmd(0x82); vt_read(COL, rd, 2); c = rd[0] | (rd[1] << 4); if (c != 10 + k) ok = 0; }
		if (!ok) { fprintf(stderr, "VIDTEST: FAIL VPOKE/VPEEK\n"); ++fails; }
		t = 0; vt_write(TGT, &t, 1);
	}
	/* 7. out-of-range pixel read must error */
	vt_int(I0, 5000); vt_int(I1, 1); e = vt_cmd(0x50); if (e != E_RANGE) { fprintf(stderr, "VIDTEST: FAIL range err=%d\n", e); ++fails; }

	if (ppmPath) VideoSnapshot(ppmPath);
	fprintf(stderr, "VIDTEST: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
}
