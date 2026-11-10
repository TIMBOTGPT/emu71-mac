#include <signal.h>
/*
 * Emu71 for macOS (arm64) - SDL2 front end
 *
 * Replaces the Win32 user interface of Emu71 v1.19 (Christoph Giesselink, GPL)
 * and keeps the original Saturn CPU / memory / timer / keyboard core unchanged.
 */
#include "pch.h"
#include "Emu71.h"
#include "io.h"
#include <SDL.h>
#include "video.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"
#include <libgen.h>
#include <mach-o/dyld.h>
#include <sys/stat.h>

#define I 0xFFFFFFFFu
#define DISPLAY_MS 16
#define LCD_COLS 132
#define LCD_ROWS 8

/* ---------- globals the core expects the front end to provide ---------- */
CRITICAL_SECTION csGDILock, csLcdLock, csKeyLock, csTLock, csBitLock, csSlowLock, csDbgLock;
HANDLE   hEventShutdn = NULL;
HANDLE   hThread = NULL;
LPTSTR   szTitle = "Emu71";
LPBYTE   pbyRom = NULL;
DWORD    dwRomSize = 0;
PPORTDATA psExtPortData[NOEXTPORTS];

VOID    KeyMacroRecord(BOOL bPress, UINT out, UINT in) { (void)bPress; (void)out; (void)in; }
LRESULT OnToolMacroStop(VOID) { return 0; }
VOID    SoundOut(CHIPSET *w, WORD wOut) { (void)w; (void)wOut; }

/* ---------- KML skin ---------- */
typedef struct { int cx, cy, ox, oy, dx, dy; } ANN;
typedef struct { int id, cx, cy, ox, oy, dx, dy; int out, in; } BTN;
static ANN  ann[64];
static BTN  btn[128];
static int  nbtn;
static int  bgW = 660, bgH = 333;
static int  lcdZoom = 1, lcdX, lcdY;
static char bmpName[256];
static char kmlName[256] = "REAL71P.KML", imgOverride[512];
static double winScale = 1.0;
extern void lock_aspect(SDL_Window *, int, int);
static int  refW = 0;          /* width of the reference sheet the KML coordinates refer to */
static double skScale = 1.0;   /* texture width / refW */
#define SC(v) ((int)((v) * skScale + 0.5))
static DWORD dwKMLColor[32];

/* KMI keyboard scripts: per Windows virtual key a small op list, interpreted at run time */
enum { OP_SETFLAG, OP_RESETFLAG, OP_IFFLAG, OP_IFPRESSED, OP_ELSE, OP_END, OP_MAP, OP_PRESS, OP_RELEASE };
typedef struct { int type, a, b; } KOP;
static KOP *kops[256]; static int nkops[256];
static int kflags[32];
static int vkActive[256][4];
static int heldCnt[128];
static char *strip(char *s)
{
	int inq = 0; char *p;
	for (p = s; *p; ++p) {
		if (*p == '"') inq = !inq;
		if (*p == '#' && !inq) { *p = 0; break; }
		if (*p == '\r' || *p == '\n') { *p = 0; break; }
	}
	while (*s == ' ' || *s == '\t') ++s;
	for (p = s + strlen(s); p > s && (p[-1] == ' ' || p[-1] == '\t'); --p) p[-1] = 0;
	return s;
}

static void kop_add(int vk, int type, int a, int b)
{
	kops[vk] = (KOP *)realloc(kops[vk], sizeof(KOP) * (nkops[vk] + 1));
	kops[vk][nkops[vk]].type = type; kops[vk][nkops[vk]].a = a; kops[vk][nkops[vk]].b = b;
	++nkops[vk];
}

static void parse_kmi(const char *path)
{
	FILE *f = fopen(path, "r"); char line[512]; int depth = 0, vk = -1;
	if (!f) { fprintf(stderr, "no keymap %s\n", path); return; }
	while (fgets(line, sizeof line, f)) {
		char *s = strip(line); int a, b;
		if (!*s) continue;
		if (sscanf(s, "Scancode %d", &a) == 1) { vk = (a >= 0 && a < 256) ? a : -1; depth = 1; continue; }
		if (vk < 0) { if (!strcmp(s, "End") && depth > 0) --depth; continue; }
		if (sscanf(s, "SetFlag %d", &a) == 1) kop_add(vk, OP_SETFLAG, a, 0);
		else if (sscanf(s, "ResetFlag %d", &a) == 1) kop_add(vk, OP_RESETFLAG, a, 0);
		else if (sscanf(s, "IfFlag %d", &a) == 1) { kop_add(vk, OP_IFFLAG, a, 0); ++depth; }
		else if (sscanf(s, "IfPressed %d", &a) == 1) { kop_add(vk, OP_IFPRESSED, a, 0); ++depth; }
		else if (!strcmp(s, "Else")) kop_add(vk, OP_ELSE, 0, 0);
		else if (!strcmp(s, "End")) { if (depth > 1) kop_add(vk, OP_END, 0, 0); if (depth > 0) --depth; if (depth == 0) vk = -1; }
		else if (sscanf(s, "Map %d %d", &a, &b) == 2) kop_add(vk, OP_MAP, a, b);
		else if (sscanf(s, "Press %d", &a) == 1) kop_add(vk, OP_PRESS, a, 0);
		else if (sscanf(s, "Release %d", &a) == 1) kop_add(vk, OP_RELEASE, a, 0);
	}
	fclose(f);
}

static void parse_kml(const char *dir, const char *name)
{
	char path[1024], line[512], blk[32] = ""; int id = 0; FILE *f;
	BTN *cb = NULL; ANN *ca = NULL;
	snprintf(path, sizeof path, "%s/%s", dir, name);
	f = fopen(path, "r");
	if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(1); }
	while (fgets(line, sizeof line, f)) {
		char *s = strip(line); int a, b, c, d, e, g;
		if (!*s) continue;
		if (!strcmp(s, "End")) { blk[0] = 0; cb = NULL; ca = NULL; continue; }
		if (sscanf(s, "Annunciator %d", &id) == 1) { strcpy(blk, "ann"); if (id >= 1 && id <= 64) ca = &ann[id - 1]; continue; }
		if (sscanf(s, "Button %d", &id) == 1) { strcpy(blk, "btn"); if (nbtn < 128) { cb = &btn[nbtn++]; cb->id = id; } continue; }
		if (!strcmp(s, "Global")) { strcpy(blk, "global"); continue; }
		if (!strcmp(s, "Background")) { strcpy(blk, "bg"); continue; }
		if (!strcmp(s, "Lcd")) { strcpy(blk, "lcd"); continue; }
		if (!strncmp(s, "Locale", 6)) { strcpy(blk, "skip"); continue; }
		if (!strcmp(blk, "global") && !strncmp(s, "Bitmap", 6)) { sscanf(s, "Bitmap \"%255[^\"]\"", bmpName); }
		else if (!strcmp(blk, "bg") && sscanf(s, "Size %d %d", &a, &b) == 2) { bgW = a; bgH = b; }
		else if (!strcmp(blk, "lcd")) {
			if (sscanf(s, "Zoom %d", &a) == 1) lcdZoom = a;
			else if (sscanf(s, "Offset %d %d", &a, &b) == 2) { lcdX = a; lcdY = b; }
			else if (sscanf(s, "Color %d %d %d %d", &a, &b, &c, &d) == 4) dwKMLColor[a & 31] = (b << 16) | (c << 8) | d;
		} else if (!strcmp(blk, "ann") && ca) {
			if (sscanf(s, "Size %d %d", &a, &b) == 2) { ca->cx = a; ca->cy = b; }
			else if (sscanf(s, "Offset %d %d", &a, &b) == 2) { ca->ox = a; ca->oy = b; }
			else if (sscanf(s, "Down %d %d", &a, &b) == 2) { ca->dx = a; ca->dy = b; }
		} else if (!strcmp(blk, "btn") && cb) {
			if (sscanf(s, "Size %d %d", &a, &b) == 2) { cb->cx = a; cb->cy = b; }
			else if (sscanf(s, "Offset %d %d", &a, &b) == 2) { cb->ox = a; cb->oy = b; }
			else if (sscanf(s, "Down %d %d", &a, &b) == 2) { cb->dx = a; cb->dy = b; }
			else if (sscanf(s, "OutIn %d %d", &e, &g) == 2) { cb->out = e; cb->in = g; }
		}
	}
	fclose(f);
}

/* ---------- display state shared between emulation and render threads ---------- */
static UINT nBlinkCnt = 0;
static BYTE lcdOn[LCD_ROWS][LCD_COLS];
static BYTE lcdBlank = 0;
static BYTE annOn[64];
static DWORD annColor[64];
static volatile int displayRunning = 0;
static pthread_t displayThread;
static int displayThreadValid = 0;

static const BYTE byRowTable[] = { 0x5C, 0x58, 0x54, 0x50, 0x52, 0x56, 0x5A, 0x5E };

VOID UpdateAnnunciators(DWORD dwUpdateMask)
{
	UINT i, nAnnId; DWORD d; BYTE byContrast; BOOL bDispOn;
	EnterCriticalSection(&csGDILock);
	if ((Chipset.dd[MASTER].IORam[DD1CTL & 0xFF] & DON) != 0) {
		byContrast = Chipset.dd[MASTER].IORam[DCONTR & 0xFF];
		bDispOn = (nBlinkCnt < 32);
		nAnnId = 1;
		for (d = (ANNAD1 & 0xFF); d <= (ANN2_5 & 0xFF); d += 2) {
			WORD wCol = *(WORD *)&Chipset.dd[SLAVE1].IORam[d];
			for (i = 0; i < sizeof byRowTable; ++i) {
				if ((dwUpdateMask & 1) != 0) {
					DWORD c = dwKMLColor[((wCol & *(WORD *)&Chipset.dd[MASTER].IORam[byRowTable[i]]) != 0) ? byContrast : byContrast + 16];
					if (nAnnId < 64) { annOn[nAnnId - 1] = (bDispOn && c != I); annColor[nAnnId - 1] = c; }
				}
				dwUpdateMask >>= 1; ++nAnnId;
			}
		}
		for (d = (ANNAD3 & 0xFF); d <= (ANN4_5 & 0xFF); d += 2) {
			WORD wCol = *(WORD *)&Chipset.dd[MASTER].IORam[d];
			for (i = 0; i < sizeof byRowTable; ++i) {
				if ((dwUpdateMask & 1) != 0) {
					DWORD c = dwKMLColor[((wCol & *(WORD *)&Chipset.dd[MASTER].IORam[byRowTable[i]]) != 0) ? byContrast : byContrast + 16];
					if (nAnnId < 64) { annOn[nAnnId - 1] = (bDispOn && c != I); annColor[nAnnId - 1] = c; }
				}
				dwUpdateMask >>= 1; ++nAnnId;
			}
		}
	} else {
		memset(annOn, 0, sizeof annOn);
	}
	LeaveCriticalSection(&csGDILock);
}

VOID UpdateContrast(VOID) { UpdateAnnunciators(0xFFFFFFFF); }

VOID UpdateMainDisplay(VOID)
{
	DWORD d; int col = 0, i;
	if ((Chipset.dd[MASTER].IORam[DD1CTL & 0xFF] & (DBLINK | DON)) == (DBLINK | DON)) {
		nBlinkCnt = (nBlinkCnt + 1) & 0x3F;
		if ((nBlinkCnt & 0x1F) == 0) UpdateAnnunciators(0xFFFFFFFF);
		if (nBlinkCnt >= 32) { lcdBlank = 1; return; }
	} else if (nBlinkCnt != 0) {
		nBlinkCnt = 0; UpdateAnnunciators(0xFFFFFFFF);
	}
	EnterCriticalSection(&csGDILock);
	lcdBlank = 0;
	memset(lcdOn, 0, sizeof lcdOn);
	if ((Chipset.dd[MASTER].IORam[DD1CTL & 0xFF] & DON) != 0
	    && dwKMLColor[Chipset.dd[MASTER].IORam[DCONTR & 0xFF] & 15] != I) {
		struct { int chip; DWORD st, en; } seg[3] = {
			{ SLAVE1, DD3ST & 0xFF, DD3END & 0xFF },
			{ SLAVE2, DD2ST & 0xFF, DD2END & 0xFF },
			{ MASTER, DD1ST & 0xFF, DD1END & 0xFF } };
		int s;
		for (s = 0; s < 3; ++s)
			for (d = seg[s].st; d < seg[s].en && col < LCD_COLS; d += 2, ++col) {
				WORD w = *(WORD *)&Chipset.dd[seg[s].chip].IORam[d];
				for (i = 0; i < LCD_ROWS; ++i)
					lcdOn[i][col] = (w & *(WORD *)&Chipset.dd[MASTER].IORam[byRowTable[i]]) != 0;
			}
	}
	LeaveCriticalSection(&csGDILock);
}

static void *display_loop(void *arg)
{
	(void)arg;
	while (displayRunning) {
		EnterCriticalSection(&csLcdLock);
		if (displayRunning) UpdateMainDisplay();
		LeaveCriticalSection(&csLcdLock);
		usleep(DISPLAY_MS * 1000);
	}
	return NULL;
}

VOID StartDisplay(VOID)
{
	if (displayRunning) return;
	nBlinkCnt = 0;
	if (Chipset.dd[MASTER].IORam[DD1CTL & 0xFF] & DON) {
		UpdateAnnunciators(0xFFFFFFFF);
		displayRunning = 1;
		pthread_create(&displayThread, NULL, display_loop, NULL);
		displayThreadValid = 1;
	}
}

VOID StopDisplay(VOID)
{
	if (!displayRunning) return;
	displayRunning = 0;
	if (displayThreadValid) { pthread_join(displayThread, NULL); displayThreadValid = 0; }
	EnterCriticalSection(&csGDILock);
	memset(lcdOn, 0, sizeof lcdOn);
	LeaveCriticalSection(&csGDILock);
}

/* ---------- ROM images ---------- */
static LPBYTE load_nibbles(const char *path, DWORD *pnibbles)
{
	FILE *f = fopen(path, "rb"); long sz; LPBYTE raw, out; DWORD i; int packed = 0;
	if (!f) return NULL;
	fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
	raw = (LPBYTE)malloc(sz);
	if (fread(raw, 1, sz, f) != (size_t)sz) { fclose(f); free(raw); return NULL; }
	fclose(f);
	for (i = 0; i < 8 && i < (DWORD)sz; ++i) if (raw[i] & 0xF0) packed = 1;
	if (!packed) { *pnibbles = (DWORD)sz; return raw; }
	out = (LPBYTE)malloc((size_t)sz * 2);
	for (i = 0; i < (DWORD)sz; ++i) { out[2 * i] = raw[i] & 0xF; out[2 * i + 1] = raw[i] >> 4; }
	free(raw);
	*pnibbles = (DWORD)sz * 2;
	return out;
}

static int add_rom_module(int port, const char *path)
{
	DWORD n; LPBYTE data = load_nibbles(path, &n); PPORTDATA p, *pp; DWORD chip;
	if (!data) { fprintf(stderr, "cannot load module ROM %s\n", path); return 0; }
	chip = (n % (16 * 2048) == 0) ? (16 * 2048) : n;
	p = (PPORTDATA)calloc(1, sizeof *p);
	p->sInfo.dwStructSize = sizeof p->sInfo;
	p->sInfo.nType = TYPE_ROM;
	p->sInfo.dwSize = n;
	p->sInfo.dwChips = n / chip;
	p->sInfo.dwCfgSize = sizeof(SATCFG) * p->sInfo.dwChips;
	p->sInfo.bHybrid = TRUE;
	p->psCfg = (PSATCFG)calloc(1, p->sInfo.dwCfgSize);
	p->pszName = strdup(path);
	p->dwNameSize = (DWORD)strlen(path);
	p->pbyData = data;
	for (pp = &psExtPortData[port]; *pp; pp = &(*pp)->pNext) ;
	*pp = p;
	return 1;
}

/* hard-configured ROM (Forth/Assembler system part): fixed base, ignores bus configuration commands */
static int add_hrd_module(int port, const char *path, DWORD base)
{
	PPORTDATA *pp; DWORD c, chip = 16 * 2048;
	if (!add_rom_module(port, path)) return 0;
	for (pp = &psExtPortData[port]; (*pp)->pNext; pp = &(*pp)->pNext) ;
	(*pp)->sInfo.nType = TYPE_HRD;
	for (c = 0; c < (*pp)->sInfo.dwChips; ++c) { (*pp)->psCfg[c].bCfg = TRUE; (*pp)->psCfg[c].dwBase = base + c * chip; }
	return 1;
}

static int hpilOn = 0; static int hpilIn = 60000, hpilOut = 60001; static char hpilHost[128] = "localhost";
static char ramSpec[64] = "3:64,4:64";	/* extra RAM modules: port:KB,... ("none" for no expansion) */
static int add_ram_module(int port, int kb)
{
	DWORD n = (DWORD)kb * 2048, chip; PPORTDATA p, *pp;
	if (port < 1 || port > 5 || kb < 1 || kb > 192) return 0;
	chip = (n % (32 * 2048) == 0) ? (32 * 2048) : (n < 2048 ? n : 2048);
	p = (PPORTDATA)calloc(1, sizeof *p);
	p->sInfo.dwStructSize = sizeof p->sInfo;
	p->sInfo.nType = TYPE_RAM;
	p->sInfo.dwSize = n;
	p->sInfo.dwChips = n / chip;
	p->sInfo.dwCfgSize = sizeof(SATCFG) * p->sInfo.dwChips;
	p->sInfo.bHybrid = TRUE;
	p->psCfg = (PSATCFG)calloc(1, p->sInfo.dwCfgSize);
	p->pbyData = (LPBYTE)calloc(n, 1);
	for (pp = &psExtPortData[port]; *pp; pp = &(*pp)->pNext) ;
	*pp = p;
	return 1;
}
static int add_hpil_module(int port, const char *path)
{
	PPORTDATA p, *pp;
	p = (PPORTDATA)calloc(1, sizeof *p);
	p->sInfo.dwStructSize = sizeof p->sInfo;
	p->sInfo.nType = TYPE_HPIL;
	p->sInfo.dwSize = 16;
	p->sInfo.dwChips = 1;
	p->sInfo.bHybrid = FALSE;
	p->sInfo.dwCfgSize = sizeof(SATCFG);
	p->psCfg = (PSATCFG)calloc(1, sizeof(SATCFG));
	p->psTcp = (PPORTTCPIP)calloc(1, sizeof *p->psTcp);
	p->psTcp->dwAddrSize = (DWORD)strlen(hpilHost);
	p->psTcp->lpszAddrOut = strdup(hpilHost);
	p->psTcp->wPortOut = (WORD)hpilOut;
	p->psTcp->wPortIn = (WORD)hpilIn;
	p->pbyData = (LPBYTE)calloc(16, 1);
	for (pp = &psExtPortData[port]; *pp; pp = &(*pp)->pNext) ;
	*pp = p;
	return add_rom_module(port, path);
}
static int vidOn = 0, vidW = 800, vidH = 600;
static int add_video_module(int port)
{
	PPORTDATA p, *pp;
	p = (PPORTDATA)calloc(1, sizeof *p);
	p->sInfo.dwStructSize = sizeof p->sInfo;
	p->sInfo.nType = TYPE_VIDEO;
	p->sInfo.dwSize = VID_NIBBLES;
	p->sInfo.dwChips = 1;
	p->sInfo.bHybrid = FALSE;
	p->sInfo.dwCfgSize = sizeof(SATCFG);
	p->psCfg = (PSATCFG)calloc(1, sizeof(SATCFG));
	for (pp = &psExtPortData[port]; *pp; pp = &(*pp)->pNext) ;
	*pp = p;
	return 1;
}
static void add_ram_from_spec(const char *spec)
{
	char buf[64], *tok, *save; strncpy(buf, spec, sizeof buf - 1); buf[sizeof buf - 1] = 0;
	if (!strcmp(buf, "none")) return;
	for (tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
		int port, kb;
		if (sscanf(tok, "%d:%d", &port, &kb) != 2 || !add_ram_module(port, kb)) fprintf(stderr, "bad --ram entry '%s'\n", tok);
	}
}

/* ---------- key handling ---------- */
static int pressedBtn = -1;

static int find_btn_idx(int id) { int i; for (i = 0; i < nbtn; ++i) if (btn[i].id == id) return i; return -1; }
static void btn_event(int idx, int press)
{
	if (idx < 0) return;
	if (press) { if (heldCnt[idx]++ == 0) KeyboardEvent(TRUE, btn[idx].out, btn[idx].in); }
	else if (heldCnt[idx] > 0) { if (--heldCnt[idx] == 0) KeyboardEvent(FALSE, btn[idx].out, btn[idx].in); }
}
static void release_all_btn(int idx) { if (idx >= 0 && heldCnt[idx] > 0) { heldCnt[idx] = 0; KeyboardEvent(FALSE, btn[idx].out, btn[idx].in); } }

/* run the KMI script of one virtual key for a press or release event */
static void vk_event(int vk, int pressed)
{
	int sp = 0, n, i; struct { int parent, cond, active; } st[16];
	if (vk <= 0 || vk >= 256) return;
	if (!pressed) {	/* release buttons bound by Map on press */
		for (i = 3; i >= 0; --i) if (vkActive[vk][i]) { btn_event(find_btn_idx(vkActive[vk][i]), FALSE); vkActive[vk][i] = 0; }
	}
	for (n = 0; n < nkops[vk]; ++n) {
		KOP *o = &kops[vk][n]; int act = sp ? st[sp - 1].active : 1;
		switch (o->type) {
		case OP_IFFLAG: case OP_IFPRESSED:
			if (sp < 16) {
				int c = (o->type == OP_IFFLAG) ? (kflags[o->a & 31] != 0) : (pressed && o->a == vk);
				st[sp].parent = act; st[sp].cond = c; st[sp].active = act && c; ++sp;
			}
			break;
		case OP_ELSE: if (sp) st[sp - 1].active = st[sp - 1].parent && !st[sp - 1].cond; break;
		case OP_END: if (sp) --sp; break;
		case OP_SETFLAG: if (act) kflags[o->a & 31] = 1; break;
		case OP_RESETFLAG: if (act) kflags[o->a & 31] = 0; break;
		case OP_MAP:
			if (act && pressed) {
				int idx = find_btn_idx(o->b);
				btn_event(idx, TRUE);
				for (i = 0; i < 4; ++i) if (!vkActive[vk][i]) { vkActive[vk][i] = o->b; break; }
			}
			break;
		case OP_PRESS: if (act) btn_event(find_btn_idx(o->a), TRUE); break;
		case OP_RELEASE: if (act) release_all_btn(find_btn_idx(o->a)); break;
		}
	}
}

static int sdl_to_vk(SDL_Keycode k)
{
	if (k >= 'a' && k <= 'z') return k - 'a' + 'A';
	if (k >= '0' && k <= '9') return k;
	switch (k) {
	case SDLK_LSHIFT: case SDLK_RSHIFT: return 16;
	case SDLK_LCTRL: case SDLK_RCTRL: return 17;
	case SDLK_LALT: case SDLK_RALT: return 18;
	case SDLK_RETURN: case SDLK_KP_ENTER: return 13;
	case SDLK_BACKSPACE: return 8;
	case SDLK_TAB: return 9;
	case SDLK_ESCAPE: return 27;
	case SDLK_SPACE: return 32;
	case SDLK_PAGEUP: return 33; case SDLK_PAGEDOWN: return 34;
	case SDLK_END: return 35; case SDLK_HOME: return 36;
	case SDLK_LEFT: return 37; case SDLK_UP: return 38;
	case SDLK_RIGHT: return 39; case SDLK_DOWN: return 40;
	case SDLK_INSERT: return 45; case SDLK_DELETE: return 46;
	case SDLK_F1: return 112; case SDLK_F2: return 113; case SDLK_F3: return 114;
	case SDLK_F4: return 115; case SDLK_F5: return 116; case SDLK_F6: return 117;
	case SDLK_F7: return 118; case SDLK_F8: return 119; case SDLK_F9: return 120;
	case SDLK_F10: return 121; case SDLK_F11: return 122; case SDLK_F12: return 123;
	case SDLK_SEMICOLON: return 186; case SDLK_EQUALS: return 187;
	case SDLK_COMMA: return 188; case SDLK_MINUS: return 189;
	case SDLK_PERIOD: return 190; case SDLK_SLASH: return 191;
	case SDLK_BACKQUOTE: return 192; case SDLK_LEFTBRACKET: return 219;
	case SDLK_BACKSLASH: return 220; case SDLK_RIGHTBRACKET: return 221;
	case SDLK_QUOTE: return 222;
	case SDLK_KP_0: return 96; case SDLK_KP_1: return 97; case SDLK_KP_2: return 98;
	case SDLK_KP_3: return 99; case SDLK_KP_4: return 100; case SDLK_KP_5: return 101;
	case SDLK_KP_6: return 102; case SDLK_KP_7: return 103; case SDLK_KP_8: return 104;
	case SDLK_KP_9: return 105; case SDLK_KP_MULTIPLY: return 106; case SDLK_KP_PLUS: return 107;
	case SDLK_KP_MINUS: return 109; case SDLK_KP_PERIOD: return 110; case SDLK_KP_DIVIDE: return 111;
	}
	return 0;
}

/* ---------- rendering ---------- */
static void draw(SDL_Renderer *r, SDL_Texture *skin, SDL_Texture *lcdTex, uint32_t *lcdPix)
{
	int x, y, i, zw = LCD_COLS * lcdZoom, zh = LCD_ROWS * lcdZoom;
	BYTE contrast = Chipset.dd[MASTER].IORam[DCONTR & 0xFF] & 15;
	BOOL on = (Chipset.dd[MASTER].IORam[DD1CTL & 0xFF] & DON) != 0;
	DWORD fg = dwKMLColor[contrast], bg = dwKMLColor[contrast + 16];
	SDL_Rect src = { 0, 0, SC(bgW), SC(bgH) }, dst = { 0, 0, bgW, bgH };
	SDL_RenderClear(r);
	SDL_RenderCopy(r, skin, &src, &dst);

	if (on && !lcdBlank && fg != I) {
		uint32_t cfg = 0xFF000000u | fg, cbg = (bg != I) ? (0xFF000000u | bg) : 0;
		EnterCriticalSection(&csGDILock);
		for (y = 0; y < zh; ++y)
			for (x = 0; x < zw; ++x)
				lcdPix[y * zw + x] = lcdOn[y / lcdZoom][x / lcdZoom] ? cfg : cbg;
		LeaveCriticalSection(&csGDILock);
		SDL_UpdateTexture(lcdTex, NULL, lcdPix, zw * 4);
		{ SDL_Rect d = { lcdX, lcdY, zw, zh }; SDL_RenderCopy(r, lcdTex, NULL, &d); }
	}
	for (i = 0; i < 64; ++i) {
		if (annOn[i] && ann[i].cx) {
			SDL_Rect s = { SC(ann[i].dx), SC(ann[i].dy), SC(ann[i].cx), SC(ann[i].cy) };
			SDL_Rect d = { ann[i].ox, ann[i].oy, ann[i].cx, ann[i].cy };
			SDL_RenderCopy(r, skin, &s, &d);
		}
	}
	for (i = 0; i < nbtn; ++i) {
		int down = heldCnt[i] > 0;
		if (down) {
			SDL_Rect s = { SC(btn[i].dx), SC(btn[i].dy), SC(btn[i].cx), SC(btn[i].cy) };
			SDL_Rect d = { btn[i].ox, btn[i].oy, btn[i].cx, btn[i].cy };
			SDL_RenderCopy(r, skin, &s, &d);
		}
	}
	SDL_RenderPresent(r);
}

/* ---------- persistent state (RAM, registers) ---------- */
#define STATE_MAGIC 0x31374D45u	/* "EM71" */
static void state_path(char *out, size_t n)
{
	const char *h = getenv("HOME"); char dir[1024];
	snprintf(dir, sizeof dir, "%s/Library/Application Support/Emu71", h ? h : ".");
	mkdir(dir, 0755);
	snprintf(out, n, "%s/state.bin", dir);
}
#define STATE_MAGIC2 0x32374D45u
typedef struct { uint32_t magic, chipsetSize; char ram[64]; } STATEHDR;
static int state_load(void)
{
	char path[1100]; FILE *f; STATEHDR hdr; static CHIPSET tmp; int pt; PPORTDATA m;
	if (getenv("EMU71_NOSTATE")) return 0;
	state_path(path, sizeof path);
	f = fopen(path, "rb"); if (!f) return 0;
	if (fread(&hdr, sizeof hdr, 1, f) != 1 || hdr.magic != STATE_MAGIC2 || hdr.chipsetSize != sizeof(CHIPSET)
	    || strncmp(hdr.ram, ramSpec, sizeof hdr.ram) != 0 || fread(&tmp, sizeof tmp, 1, f) != 1 || tmp.type != 'T') { fclose(f); return 0; }
	/* RAM module contents follow, in port order */
	for (pt = 0; pt < NOEXTPORTS; ++pt)
		for (m = psExtPortData[pt]; m; m = m->pNext)
			if (m->sInfo.nType == TYPE_RAM && fread(m->pbyData, 1, m->sInfo.dwSize, f) != m->sInfo.dwSize) { fclose(f); return 0; }
	fclose(f);
	Chipset = tmp;
	memset(Chipset.Keyboard_Row, 0, sizeof Chipset.Keyboard_Row);	/* a key held at save time must not stay 'pressed' */
	Chipset.in = 0; Chipset.IR15X = 0;
	return 1;
}
static void state_save(void)
{
	char path[1100], tmpPath[1110]; FILE *f; STATEHDR hdr; int pt; PPORTDATA m;
	if (getenv("EMU71_NOSTATE")) return;
	memset(&hdr, 0, sizeof hdr); hdr.magic = STATE_MAGIC2; hdr.chipsetSize = sizeof(CHIPSET); strncpy(hdr.ram, ramSpec, sizeof hdr.ram - 1);
	state_path(path, sizeof path);
	snprintf(tmpPath, sizeof tmpPath, "%s.tmp", path);
	f = fopen(tmpPath, "wb"); if (!f) return;
	fwrite(&hdr, sizeof hdr, 1, f); fwrite(&Chipset, sizeof Chipset, 1, f);
	for (pt = 0; pt < NOEXTPORTS; ++pt)
		for (m = psExtPortData[pt]; m; m = m->pNext)
			if (m->sInfo.nType == TYPE_RAM) fwrite(m->pbyData, 1, m->sInfo.dwSize, f);
	fclose(f); rename(tmpPath, path);
}

/* ---------- start-up ---------- */
static void exe_dir(char *out, size_t n)
{
	char buf[1024]; uint32_t sz = sizeof buf;
	if (_NSGetExecutablePath(buf, &sz) == 0) { char *d = dirname(buf); strncpy(out, d, n - 1); out[n - 1] = 0; }
	else strcpy(out, ".");
}

int main(int argc, char **argv)
{
	char base[1024], skinDir[1024], romDir[1024], path[1100];
	SDL_Window *win; SDL_Renderer *ren; SDL_Texture *skin, *lcdTex; SDL_Surface *surf;
	uint32_t *lcdPix; int running = 1, i; const char *script = getenv("EMU71_KEYS"); Uint32 nextKey = 3000; Uint32 vidTestAt = getenv("EMU71_VIDTEST") ? (Uint32)(atof(getenv("EMU71_VIDTEST")) * 1000) : 0; Uint32 dumpAfter = getenv("EMU71_DUMP") ? (Uint32)(atof(getenv("EMU71_DUMP")) * 1000) : 0;

	exe_dir(base, sizeof base);
	/* layout: <base>/../Resources/{skin,roms} in a bundle, or <base>/{skin,roms}; argv overrides */
	snprintf(skinDir, sizeof skinDir, "%s/../Resources/skin", base);
	snprintf(romDir, sizeof romDir, "%s/../Resources/roms", base);
	if (access(skinDir, R_OK)) { snprintf(skinDir, sizeof skinDir, "%s/skin", base); snprintf(romDir, sizeof romDir, "%s/roms", base); }
	for (i = 1; i < argc; ++i) if (!strcmp(argv[i], "--vid")) { vidOn = 1; if (i + 1 < argc && argv[i + 1][0] != '-') { int w = 0, h = 0; if (sscanf(argv[i + 1], "%dx%d", &w, &h) == 2) { vidW = w; vidH = h; } } }
	for (i = 1; i + 1 < argc; i += 2) {
		if (!strcmp(argv[i], "--skin")) strncpy(skinDir, argv[i + 1], sizeof skinDir - 1);
		if (!strcmp(argv[i], "--kml")) strncpy(kmlName, argv[i + 1], sizeof kmlName - 1);
		if (!strcmp(argv[i], "--image")) strncpy(imgOverride, argv[i + 1], sizeof imgOverride - 1);
		if (!strcmp(argv[i], "--scale")) winScale = atof(argv[i + 1]);
		if (!strcmp(argv[i], "--vid")) { vidOn = 1; if (i + 1 < argc && argv[i + 1][0] != '-') { int w = 0, h = 0; if (sscanf(argv[i + 1], "%dx%d", &w, &h) == 2) { vidW = w; vidH = h; } } }
		if (!strcmp(argv[i], "--ram")) strncpy(ramSpec, argv[i + 1], sizeof ramSpec - 1);
		if (!strcmp(argv[i], "--hpil")) { hpilOn = 1; if (i + 1 < argc && argv[i + 1][0] != '-') { /* host:out:in */ char *c = strdup(argv[i + 1]); char *a = strtok(c, ":"), *b = strtok(NULL, ":"), *d = strtok(NULL, ":"); if (a) strncpy(hpilHost, a, sizeof hpilHost - 1); if (b) hpilOut = atoi(b); if (d) hpilIn = atoi(d); free(c); } }
		if (!strcmp(argv[i], "--refw")) refW = atoi(argv[i + 1]);
		if (!strcmp(argv[i], "--roms")) strncpy(romDir, argv[i + 1], sizeof romDir - 1);
	}

	InitializeCriticalSection(&csGDILock); InitializeCriticalSection(&csLcdLock);
	InitializeCriticalSection(&csKeyLock); InitializeCriticalSection(&csTLock);
	InitializeCriticalSection(&csBitLock); InitializeCriticalSection(&csSlowLock);
	InitializeCriticalSection(&csDbgLock);
	for (i = 0; i < 32; ++i) dwKMLColor[i] = I;

	parse_kml(skinDir, kmlName);
	snprintf(path, sizeof path, "%s/KBD-US.KMI", skinDir);
	parse_kmi(path);

	/* emulation thread starts in the INVALID state, exactly like the Win32 version */
	hEventShutdn = CreateEvent(NULL, FALSE, FALSE, NULL);
	nState = SM_RUN; nNextState = SM_INVALID;
	SetSpeed(TRUE);
	hThread = CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
	while (nState != nNextState) Sleep(0);

	snprintf(path, sizeof path, "%s/ROM71.BIN", romDir);
	pbyRom = load_nibbles(path, &dwRomSize);
	if (!pbyRom) { fprintf(stderr, "cannot load system ROM %s\n", path); return 1; }

	ZeroMemory(psExtPortData, sizeof psExtPortData);
	snprintf(path, sizeof path, "%s/MATHROM.BIN", romDir);  add_rom_module(1, path);
	snprintf(path, sizeof path, "%s/FORTHROM.BIN", romDir); add_rom_module(2, path);
	snprintf(path, sizeof path, "%s/HRDFORTH.BIN", romDir); if (!getenv("EMU71_NOHRD")) add_hrd_module(2, path, 0xE0000);
	add_ram_from_spec(ramSpec);
	if (hpilOn) { snprintf(path, sizeof path, "%s/Hpilrom.bin", romDir); add_hpil_module(getenv("HPILPORT") ? atoi(getenv("HPILPORT")) : 5, path); signal(SIGPIPE, SIG_IGN); strncat(ramSpec, "+hpil", sizeof ramSpec - strlen(ramSpec) - 1); }

	if (vidOn) { char sp[24]; VideoSetSize(vidW, vidH); add_video_module(getenv("VIDPORT") ? atoi(getenv("VIDPORT")) : 5); snprintf(sp, sizeof sp, "+vid%dx%d", vidW, vidH); strncat(ramSpec, sp, sizeof ramSpec - strlen(ramSpec) - 1); }

	if (getenv("EMU71_LEXROM")) { if (add_rom_module(3, getenv("EMU71_LEXROM"))) strncat(ramSpec, "+lex", sizeof ramSpec - strlen(ramSpec) - 1); }

	ZeroMemory(&Chipset, sizeof Chipset);
	if (state_load()) {
		Chipset.bExtModulesPlugged = TRUE;
		MountPorts();
		Chipset.Shutdn = FALSE;
		if (!Chipset.bOD) Chipset.pc = 0x010A6;		/* =WARM1X, like Emu71's CalcWarmstart() */
		else { CpuReset(); Chipset.Shutdn = FALSE; }
	} else {
		Chipset.type = 'T';
		Chipset.bExtModulesPlugged = TRUE;
		MountPorts();
		CpuReset();
		Chipset.Shutdn = FALSE;
	}

	if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	{
		SDL_Rect ub; double sc = winScale;
		if (SDL_GetDisplayUsableBounds(0, &ub) == 0) {	/* shrink to fit the screen, keep aspect */
			double fx = (ub.w * 0.96) / bgW, fy = (ub.h * 0.92) / bgH;
			if (sc > fx) sc = fx;
			if (sc > fy) sc = fy;
		}
		win = SDL_CreateWindow("Emu71 - HP-71B", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		                       (int)(bgW * sc), (int)(bgH * sc), SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
		SDL_SetWindowMinimumSize(win, bgW / 2, bgH / 2);
		lock_aspect(win, bgW, bgH);
	}
	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	SDL_RenderSetLogicalSize(ren, bgW, bgH);
	if (imgOverride[0]) snprintf(path, sizeof path, "%s", imgOverride);
	else snprintf(path, sizeof path, "%s/%s", skinDir, bmpName);
	{
		const char *ext = strrchr(path, '.');
		if (ext && (!strcasecmp(ext, ".png") || !strcasecmp(ext, ".jpg") || !strcasecmp(ext, ".jpeg"))) {
			int w, h, n;
			unsigned char *px = stbi_load(path, &w, &h, &n, 4);
			if (!px) { fprintf(stderr, "cannot load skin %s\n", path); return 1; }
			surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
			memcpy(surf->pixels, px, (size_t)w * h * 4);
			stbi_image_free(px);
		} else surf = SDL_LoadBMP(path);
	}
	if (!surf) { fprintf(stderr, "cannot load skin %s: %s\n", path, SDL_GetError()); return 1; }
	if (!refW) refW = (surf->w % 992 == 0) ? 992 : (surf->w % 800 == 0 ? 800 : surf->w);
	skScale = (double)surf->w / refW;
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
	skin = SDL_CreateTextureFromSurface(ren, surf);
	SDL_FreeSurface(surf);
	lcdTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, LCD_COLS * lcdZoom, LCD_ROWS * lcdZoom);
	SDL_SetTextureBlendMode(lcdTex, SDL_BLENDMODE_BLEND);
	lcdPix = (uint32_t *)malloc(LCD_COLS * lcdZoom * LCD_ROWS * lcdZoom * 4);

	SwitchToState(SM_RUN);

	while (running) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (VideoEvent(&e)) continue;
			if (e.type == SDL_QUIT) running = 0;
			else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
				for (i = 0; i < nbtn; ++i)
					if (e.button.x >= btn[i].ox && e.button.x < btn[i].ox + btn[i].cx &&
					    e.button.y >= btn[i].oy && e.button.y < btn[i].oy + btn[i].cy) {
						pressedBtn = i; btn_event(i, TRUE); break;
					}
			} else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
				if (pressedBtn >= 0) { btn_event(pressedBtn, FALSE); pressedBtn = -1; }
			} else if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && !e.key.repeat) {
				vk_event(sdl_to_vk(e.key.keysym.sym), e.type == SDL_KEYDOWN);
			}
		}
		{ static int fastDone = 0; if (!fastDone && getenv("EMU71_FASTAT") && SDL_GetTicks() > (Uint32)(atof(getenv("EMU71_FASTAT")) * 1000)) { fastDone = 1; SetSpeed(FALSE); } }
		if (script && SDL_GetTicks() > nextKey) {
			static int phase = 0, cur = 0, sh = 0;	/* "s186" = shifted key */
			if (!phase) { sh = (*script == 's'); cur = atoi(script + sh); if (sh) vk_event(16, 1); vk_event(cur, 1); phase = 1; }
			else { vk_event(cur, 0); if (sh) vk_event(16, 0); phase = 0; while (*script && *script != ',') ++script; if (*script == ',') ++script; if (!*script) { script = NULL; if (getenv("EMU71_FAST")) SetSpeed(FALSE); } }
			nextKey = SDL_GetTicks() + (getenv("EMU71_KEYMS") ? (Uint32)atoi(getenv("EMU71_KEYMS")) : 250);
		}
		draw(ren, skin, lcdTex, lcdPix);
		VideoPump();
		if (vidTestAt && SDL_GetTicks() > vidTestAt) { vidTestAt = 0; VideoSelfTest(getenv("EMU71_VIDPPM")); if (getenv("EMU71_VIDEXIT")) running = 0; }
		if (dumpAfter && SDL_GetTicks() > dumpAfter) {
			int rr, cc;
			EnterCriticalSection(&csGDILock);
			for (rr = 0; rr < LCD_ROWS; ++rr) { for (cc = 0; cc < LCD_COLS; ++cc) fputc(lcdOn[rr][cc] ? '#' : '.', stderr); fputc('\n', stderr); }
			LeaveCriticalSection(&csGDILock);
			fprintf(stderr, "pc=%05X shutdn=%d state=%u dispOn=%d annOn:", Chipset.pc, Chipset.Shutdn, nState, Chipset.dd[MASTER].IORam[DD1CTL & 0xFF] & DON);
			for (rr = 0; rr < 64; ++rr) if (annOn[rr]) fprintf(stderr, " %d", rr + 1);
			fputc('\n', stderr);
			if (getenv("EMU71_DUMPMEM")) {	/* nibble-per-byte dump of 20000-EFFFF through the bus (system RAM + mapped RAM) */
				FILE *mf = fopen(getenv("EMU71_DUMPMEM"), "wb"); DWORD ad;
				if (mf) { for (ad = 0x2E000; ad < 0xF0000; ++ad) fputc(Read2(ad) & 0xF, mf); fclose(mf); }
			}
			if (getenv("EMU71_FIND")) {	/* EMU71_FIND=hexdigits[,hexdigits]: ascending-address nibble pattern search, 30000-EFFFF */
				char buf[256], *tok, *sv; strncpy(buf, getenv("EMU71_FIND"), sizeof buf - 1); buf[sizeof buf - 1] = 0;
				for (tok = strtok_r(buf, ",", &sv); tok; tok = strtok_r(NULL, ",", &sv)) {
					int n = (int)strlen(tok), pat[64], k, hits = 0; DWORD ad;
					for (k = 0; k < n && k < 64; ++k) pat[k] = (tok[k] <= '9') ? tok[k] - '0' : (tok[k] & 7) + 9;
					fprintf(stderr, "FIND %s:\n", tok);
					for (ad = 0x30000; ad < 0xF0000 && hits < 6; ++ad) {
						for (k = 0; k < n && (Read2(ad + k) & 0xF) == pat[k]; ++k) ;
						if (k == n) {
							int j; ++hits; fprintf(stderr, "  @%05X: ", (unsigned)ad);
							for (j = -6; j < n + 10; ++j) fprintf(stderr, "%X%s", Read2(ad + j) & 0xF, j == -1 || j == n - 1 ? "|" : "");
							fputc('\n', stderr);
						}
					}
					{	/* internal RAM chips, searched directly (no bus reads) */
						int c, o, base = 0; hits = 0;
						for (c = 0; c < 4; ++c, base += 8192)
							for (o = 0; o + n <= 8192 && hits < 8; ++o) {
								for (k = 0; k < n && Chipset.ir[c].Ram[o + k] == pat[k]; ++k) ;
								if (k == n) { int j; ++hits; fprintf(stderr, "  IR%d@%04X (ir offset %05X): ", c, o, base + o);
									for (j = -6; j < n + 10; ++j) if (o + j >= 0 && o + j < 8192) fprintf(stderr, "%X%s", Chipset.ir[c].Ram[o + j] & 0xF, j == -1 || j == n - 1 ? "|" : "");
									fputc('\n', stderr); }
							}
					}
					if (!hits) fprintf(stderr, "  not found\n");
				}
			}
			if (getenv("EMU71_TRACE")) { DWORD a = Chipset.pc; int k; fprintf(stderr, "nibbles@pc:"); for (k = 0; k < 40; ++k) fprintf(stderr, "%X", Read2(a+k) & 0xF); fprintf(stderr, "\nA=%05X C=%05X D0=%05X D1=%05X ST=%02X%02X\n", (unsigned)(Chipset.A[0]|Chipset.A[1]<<4|Chipset.A[2]<<8|Chipset.A[3]<<12|Chipset.A[4]<<16),(unsigned)(Chipset.C[0]|Chipset.C[1]<<4|Chipset.C[2]<<8|Chipset.C[3]<<12|Chipset.C[4]<<16),Chipset.d0,Chipset.d1,Chipset.ST[1],Chipset.ST[0]); }
			dumpAfter = getenv("EMU71_EVERY") ? SDL_GetTicks() + (Uint32)(atof(getenv("EMU71_EVERY")) * 1000) : 0; if (!dumpAfter && getenv("EMU71_EXIT")) running = 0; if (dumpAfter && SDL_GetTicks() > (Uint32)(atof(getenv("EMU71_STOP") ? getenv("EMU71_STOP") : "1e6") * 1000)) running = 0;
		}
	}

	SwitchToState(SM_INVALID);
	state_save();
	SwitchToState(SM_RETURN);
	if (getenv("EMU71_VIDSHOT")) VideoSnapshot(getenv("EMU71_VIDSHOT"));
	VideoShutdown();
	SDL_Quit();
	return 0;
}
