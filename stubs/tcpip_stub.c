/* HP-IL over TCP/IP is not part of the Mac port (yet): no remote loop partners. */
#include "pch.h"
#include "tcpip.h"
VOID TcpInit(PTCPIP p) { ZeroMemory(p, sizeof(*p)); }
BOOL TcpCreateSvr(PTCPIP p) { (void)p; return FALSE; }
VOID TcpCloseSvr(PTCPIP p) { (void)p; }
WORD HpilController(PTCPIP p, WORD wFrame) { (void)p; return wFrame; }
VOID HpilDevice(PTCPIP p, WORD wFrame) { (void)p; (void)wFrame; }
