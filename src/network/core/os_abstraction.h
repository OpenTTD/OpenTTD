/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file os_abstraction.h Network stuff has many things that needs to be
 *                        included and/or implemented by default.
 *                        All those things are in this file.
 */

#ifndef NETWORK_CORE_OS_ABSTRACTION_H
#define NETWORK_CORE_OS_ABSTRACTION_H

/* Include standard stuff per OS */

#ifdef ENABLE_NETWORK

/* Windows stuff */
#if defined(WIN32) || defined(WIN64)
#include <errno.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#define GET_LAST_ERROR() WSAGetLastError()
#undef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
/* Windows has some different names for some types */
typedef unsigned long in_addr_t;

#if !(defined(__MINGW32__) || defined(__CYGWIN__))
	/* Windows has some different names for some types */
	typedef SSIZE_T ssize_t;
	typedef int socklen_t;
#	define IPPROTO_IPV6 41
#else
#include "../../os/windows/win32.h"
#include "../../core/alloc_func.hpp"

#define AI_ADDRCONFIG   0x00000400  /* Resolution only if global address configured */
#define IPV6_V6ONLY 27

static inline int OTTDgetnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, DWORD hostlen, char *serv, DWORD servlen, int flags)
{
	static int (WINAPI *getnameinfo)(const struct sockaddr *, socklen_t, char *, DWORD, char *, DWORD, int) = NULL;
	static bool first_time = true;

	if (first_time) {
		LoadLibraryList((Function*)&getnameinfo, "ws2_32.dll\0getnameinfo\0\0");
		first_time = false;
	}

	if (getnameinfo != NULL) return getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);

	strncpy(host, inet_ntoa(((const struct sockaddr_in *)sa)->sin_addr), hostlen);
	return 0;
}
#define getnameinfo OTTDgetnameinfo

static inline int OTTDgetaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res)
{
	static int (WINAPI *getaddrinfo)(const char *, const char *, const struct addrinfo *, struct addrinfo **) = NULL;
	static bool first_time = true;

	if (first_time) {
		LoadLibraryList((Function*)&getaddrinfo, "ws2_32.dll\0getaddrinfo\0\0");
		first_time = false;
	}

	if (getaddrinfo != NULL) return getaddrinfo(nodename, servname, hints, res);

	*res = NULL;

	in_addr_t ip = inet_addr(nodename);
	if (ip == INADDR_NONE) {
		struct hostent *he = gethostbyname(nodename);
		if (he == NULL) return EAI_NONAME;
		ip = (*(struct in_addr *)he->h_addr).s_addr;
	}

	struct sockaddr_in *sin = CallocT<struct sockaddr_in>(1);
	sin->sin_family = AF_INET;
	sin->sin_port = htons(strtoul(servname, NULL, 10));
	sin->sin_addr.s_addr = ip;

	struct addrinfo *ai = CallocT<struct addrinfo>(1);
	ai->ai_family = PF_INET;
	ai->ai_addr = (struct sockaddr*)sin;
	ai->ai_addrlen = sizeof(*sin);
	ai->ai_socktype = hints->ai_socktype;

	*res = ai;
	return 0;
}
#define getaddrinfo OTTDgetaddrinfo

static inline void OTTDfreeaddrinfo(struct addrinfo *ai)
{
	static int (WINAPI *freeaddrinfo)(struct addrinfo *) = NULL;
	static bool first_time = true;

	if (ai == NULL) return;

	if (first_time) {
		LoadLibraryList((Function*)&freeaddrinfo, "ws2_32.dll\0freeaddrinfo\0\0");
		first_time = false;
	}

	if (freeaddrinfo != NULL) {
		freeaddrinfo(ai);
		return;
	}

	do {
		struct addrinfo *next = ai->ai_next;
		free(ai->ai_addr);
		free(ai);
		ai = next;
	} while (ai != NULL);
}
#define freeaddrinfo OTTDfreeaddrinfo
#endif /* __MINGW32__ && __CYGWIN__ */
#endif /* WIN32 */

/* UNIX stuff */
#if defined(UNIX) && !defined(__OS2__)
#	if defined(OPENBSD) || defined(__NetBSD__)
#		define AI_ADDRCONFIG 0
#	endif
#	define SOCKET int
#	define INVALID_SOCKET -1
#	if !defined(__MORPHOS__) && !defined(__AMIGA__)
#		define ioctlsocket ioctl
#	if !defined(BEOS_NET_SERVER)
#		define closesocket close
#	endif
#		define GET_LAST_ERROR() (errno)
#	endif
/* Need this for FIONREAD on solaris */
#	define BSD_COMP

/* Includes needed for UNIX-like systems */
#	include <unistd.h>
#	include <sys/ioctl.h>
#	if defined(__BEOS__) && defined(BEOS_NET_SERVER)
#		include <be/net/socket.h>
#		include <be/kernel/OS.h> /* snooze() */
#		include <be/net/netdb.h>
		typedef unsigned long in_addr_t;
#		define INADDR_NONE INADDR_BROADCAST
#	else
#		include <sys/socket.h>
#		include <netinet/in.h>
#		include <netinet/tcp.h>
#		include <arpa/inet.h>
#		include <net/if.h>
/* According to glibc/NEWS, <ifaddrs.h> appeared in glibc-2.3. */
#		if !defined(__sgi__) && !defined(SUNOS) && !defined(__MORPHOS__) && !defined(__BEOS__) && !defined(__HAIKU__) && !defined(__INNOTEK_LIBC__) \
		   && !(defined(__GLIBC__) && (__GLIBC__ <= 2) && (__GLIBC_MINOR__ <= 2)) && !defined(__dietlibc__) && !defined(HPUX)
/* If for any reason ifaddrs.h does not exist on your system, comment out
 *   the following two lines and an alternative way will be used to fetch
 *   the list of IPs from the system. */
#			include <ifaddrs.h>
#			define HAVE_GETIFADDRS
#		endif
#		if !defined(INADDR_NONE)
#			define INADDR_NONE 0xffffffff
#		endif
#		if defined(__BEOS__) && !defined(BEOS_NET_SERVER)
			/* needed on Zeta */
#			include <sys/sockio.h>
#		endif
#	endif /* BEOS_NET_SERVER */

#	if !defined(__BEOS__) && defined(__GLIBC__) && (__GLIBC__ <= 2) && (__GLIBC_MINOR__ <= 1)
		typedef uint32_t in_addr_t;
#	endif

#	include <errno.h>
#	include <sys/time.h>
#	include <netdb.h>
#endif /* UNIX */

#ifdef __BEOS__
	typedef int socklen_t;
#endif

#ifdef __HAIKU__
	#define IPV6_V6ONLY 27
#endif

/* OS/2 stuff */
#if defined(__OS2__)
#	define SOCKET int
#	define INVALID_SOCKET -1
#	define ioctlsocket ioctl
#	define closesocket close
#	define GET_LAST_ERROR() (sock_errno())

/* Includes needed for OS/2 systems */
#	include <types.h>
#	include <unistd.h>
#	include <sys/ioctl.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <netinet/tcp.h>
#	include <arpa/inet.h>
#	include <net/if.h>
#	include <errno.h>
#	include <sys/time.h>
#	include <netdb.h>
#	include <nerrno.h>
#	define INADDR_NONE 0xffffffff
#	include "../../3rdparty/os2/getaddrinfo.h"
#	include "../../3rdparty/os2/getnameinfo.h"

#define IPV6_V6ONLY 27

/*
 * IPv6 address
 */
struct in6_addr {
	union {
		uint8_t  __u6_addr8[16];
		uint16_t __u6_addr16[8];
		uint32_t __u6_addr32[4];
	} __u6_addr; /* 128-bit IP6 address */
};

#define s6_addr   __u6_addr.__u6_addr8

struct sockaddr_in6 {
	uint8_t         sin6_len;       /* length of this struct */
	sa_family_t     sin6_family;    /* AF_INET6 */
	in_port_t       sin6_port;      /* Transport layer port # */
	uint32_t        sin6_flowinfo;  /* IP6 flow information */
	struct in6_addr sin6_addr;      /* IP6 address */
	uint32_t        sin6_scope_id;  /* scope zone index */
};

typedef int socklen_t;
#if !defined(__INNOTEK_LIBC__)
typedef unsigned long in_addr_t;
#endif /* __INNOTEK_LIBC__ */

#endif /* OS/2 */

/* MorphOS and Amiga stuff */
#if defined(__MORPHOS__) || defined(__AMIGA__)
#	include <exec/types.h>
#	include <proto/exec.h>   /* required for Open/CloseLibrary() */
	/* MorphOS defines his network functions with UBYTE arrays while we
	 *  use char arrays. This gives tons of unneeded warnings */
#	define UBYTE char
#	if defined(__MORPHOS__)
#		include <sys/filio.h>  /* FIO* defines */
#		include <sys/sockio.h> /* SIO* defines */
#		include <netinet/in.h>
#	else /* __AMIGA__ */
#		include	<proto/socket.h>
#	endif

/* Make the names compatible */
#	define closesocket(s) CloseSocket(s)
#	define GET_LAST_ERROR() Errno()
#	define ioctlsocket(s, request, status) IoctlSocket((LONG)s, (ULONG)request, (char*)status)
#	define ioctl ioctlsocket

	typedef unsigned int in_addr_t;
	typedef long         socklen_t;
	extern struct Library *SocketBase;

#	ifdef __AMIGA__
	/* for usleep() implementation */
	extern struct Device      *TimerBase;
	extern struct MsgPort     *TimerPort;
	extern struct timerequest *TimerRequest;
#	endif
#endif /* __MORPHOS__ || __AMIGA__ */

/**
 * Try to set the socket into non-blocking mode.
 * @param d The socket to set the non-blocking more for.
 * @return True if setting the non-blocking mode succeeded, otherwise false.
 */
static inline bool SetNonBlocking(SOCKET d)
{
#ifdef WIN32
	u_long nonblocking = 1;
#else
	int nonblocking = 1;
#endif
#if (defined(__BEOS__) && defined(BEOS_NET_SERVER))
	return setsockopt(d, SOL_SOCKET, SO_NONBLOCK, &nonblocking, sizeof(nonblocking)) == 0;
#else
	return ioctlsocket(d, FIONBIO, &nonblocking) == 0;
#endif
}

/**
 * Try to set the socket to not delay sending.
 * @param d The socket to disable the delaying for.
 * @return True if disabling the delaying succeeded, otherwise false.
 */
static inline bool SetNoDelay(SOCKET d)
{
	/* XXX should this be done at all? */
#if !defined(BEOS_NET_SERVER) /* not implemented on BeOS net_server */
	int b = 1;
	/* The (const char*) cast is needed for windows */
	return setsockopt(d, IPPROTO_TCP, TCP_NODELAY, (const char*)&b, sizeof(b)) == 0;
#else
	return true;
#endif
}

/* Make sure these structures have the size we expect them to be */
assert_compile(sizeof(in_addr)  ==  4); ///< IPv4 addresses should be 4 bytes.
assert_compile(sizeof(in6_addr) == 16); ///< IPv6 addresses should be 16 bytes.

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_OS_ABSTRACTION_H */
