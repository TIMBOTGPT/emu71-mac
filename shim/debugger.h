/* Stub debugger interface: the original debugger UI is not part of the Mac port. */
#ifndef EMU71_SHIM_DEBUGGER_H
#define EMU71_SHIM_DEBUGGER_H
#define BP_EXEC		0x01
#define BP_READ		0x02
#define BP_WRITE	0x04
#define BP_ACCESS	(BP_READ|BP_WRITE)
#define BN_ASM		0
#define BN_RPL		1
#define BN_ASM_BT	2
#define DBG_SUSPEND		-1
#define DBG_OFF			0
#define DBG_RUN			1
#define DBG_STEPINTO	2
#define DBG_STEPOVER	3
#define DBG_STEPOUT		4
static inline VOID UpdateDbgCycleCounter(VOID) {}
static inline BOOL CheckBreakpoint(DWORD a, DWORD r, UINT t) { (void)a; (void)r; (void)t; return FALSE; }
static inline VOID NotifyDebugger(INT t) { (void)t; }
static inline VOID DisableDebugger(VOID) {}
static inline VOID LoadBreakpointList(HANDLE h) { (void)h; }
#endif
