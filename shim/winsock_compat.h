/* Winsock -> BSD sockets compatibility for orig/tcpip.c */
#ifndef EMU71_WINSOCK_COMPAT_H
#define EMU71_WINSOCK_COMPAT_H
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <stddef.h>
typedef char CHAR;
typedef struct addrinfo ADDRINFO;
typedef struct timeval TIMEVAL;
typedef struct sockaddr *LPSOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct hostent HOSTENT;
typedef struct { int dummy; } WSADATA;
#define SOCKET_ERROR (-1)
#define WSAEWOULDBLOCK EINPROGRESS
#define WSAEINTR EINTR
#define WSAGetLastError() errno
#define MAKEWORD(a,b) ((WORD)(((a)&0xff)|(((b)&0xff)<<8)))
#define WSAStartup(v,d) 0
#define WSACleanup() 0
#define closesocket(s) close(s)
#define sprintf_s(b,n,...) snprintf((b),(n),__VA_ARGS__)
static inline int emu_ioctl_fionbio(int s, unsigned long *f)
{
	int fl = fcntl(s, F_GETFL, 0);
	return fcntl(s, F_SETFL, *f ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK));
}
/* BSD select() reports a refused non-blocking connect as writable: check SO_ERROR */
static inline int emu_connected(int s)
{
	int err = 0; socklen_t l = sizeof err;
	return getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &l) == 0 && err == 0;
}
#define ioctlsocket(s,c,a) emu_ioctl_fionbio((s),(a))
#define FIONBIO 0
typedef struct in6_addr *PIN6_ADDR;
typedef struct sockaddr_in6 *PSOCKADDR_IN6;
typedef void *LPTHREAD_START_ROUTINE;
#endif
