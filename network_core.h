#ifndef NETWORK_CORE_H
#define NETWORK_CORE_H

// Network stuff has many things that needs to be included
//  by default. All those things are in this file.

// =============================
// Include standard stuff per OS

// Windows stuff
#if defined(WIN32)
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#ifdef _MSC_VER
	#pragma comment (lib, "ws2_32.lib")
#endif //_MSC_VER

#if ! (defined(__MINGW32__) || defined(__CYGWIN__))
	#define ENABLE_NETWORK // On windows, the network is always enabled
	// Windows has some different names for some types..
	typedef SSIZE_T ssize_t;
#endif

#define GET_LAST_ERROR() WSAGetLastError()
#define EWOULDBLOCK WSAEWOULDBLOCK
// Windows has some different names for some types..
typedef unsigned long in_addr_t;
typedef INTERFACE_INFO IFREQ;
#endif // WIN32

// UNIX stuff
#if defined(UNIX)
#	define SOCKET int
#	define INVALID_SOCKET -1
typedef struct ifreq IFREQ;
#	if !defined(__MORPHOS__) && !defined(__AMIGA__)
#		define ioctlsocket ioctl
#	if !defined(BEOS_NET_SERVER)
#		define closesocket close
#	endif
#		define GET_LAST_ERROR() (errno)
#	endif
// Need this for FIONREAD on solaris
#	define BSD_COMP

// Includes needed for UNIX-like systems
#	include <unistd.h>
#	include <sys/ioctl.h>
#	if defined(__BEOS__) && defined(BEOS_NET_SERVER)
#		include <be/net/socket.h>
#		include <be/kernel/OS.h> // snooze()
#		include <be/net/netdb.h>
		typedef unsigned long in_addr_t;
#		define INADDR_NONE INADDR_BROADCAST
#	else
#		include <sys/socket.h>
#		include <netinet/in.h>
#		include <netinet/tcp.h>
#		include <arpa/inet.h>
#		include <net/if.h>
#		if !defined(SUNOS) && !defined(__MORPHOS__) && !defined(__BEOS__)
#			include <ifaddrs.h>
// If for any reason ifaddrs.h does not exist on a system, remove define below
//   and an other system will be used to fetch ips from the system
#			define HAVE_GETIFADDRS
#		else
#			define INADDR_NONE 0xffffffff
#		endif // SUNOS
#		if defined(__BEOS__) && !defined(BEOS_NET_SERVER)
			// needed on Zeta
#			include <sys/sockio.h>
#		endif
#	endif // BEOS_NET_SERVER

/* GLibc 2.1 does not support GetIfAddr() */
#	if defined(__GLIBC__) && (__GLIBC__ == 2) && (__GLIBC_MINOR__ == 1)
#		undef HAVE_GETIFADDRS
		typedef uint32_t in_addr_t;
#	endif /* __GLIBC__ && (__GLIBC__ == 2) && (__GLIBC_MINOR__ == 1) */

#	include <errno.h>
#	include <sys/time.h>
#	include <netdb.h>
#endif // UNIX

// OS/2 stuff
#if defined(__OS2__)
#	define SOCKET int
#	define INVALID_SOCKET -1
typedef struct ifreq IFREQ;
#	define ioctlsocket ioctl
#	define closesocket close
#	define GET_LAST_ERROR() (sock_errno())

// Includes needed for OS/2 systems
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

typedef unsigned long in_addr_t;
#endif // OS/2

// MorphOS and Amiga stuff
#if defined(__MORPHOS__) || defined(__AMIGA__)
#	include <exec/types.h>
#	include <proto/exec.h>		// required for Open/CloseLibrary()
#	if defined(__MORPHOS__)
#		include <sys/filio.h> 	// FIO* defines
#		include <sys/sockio.h>  // SIO* defines
#	else // __AMIGA__
#		include	<proto/socket.h>
#	endif

// Make the names compatible
#	define closesocket(s) CloseSocket(s)
#	define GET_LAST_ERROR() Errno()
#	define ioctlsocket(s,request,status) IoctlSocket((LONG)s,(ULONG)request,(char*)status)
#	define ioctl ioctlsocket

	typedef unsigned int in_addr_t;
	extern struct Library *SocketBase;

#	ifdef __AMIGA__
	// for usleep() implementation
	extern struct Device      *TimerBase;
	extern struct MsgPort     *TimerPort;
	extern struct timerequest *TimerRequest;
#	endif
#endif // __MORPHOS__ || __AMIGA__

static inline bool SetNonBlocking(int d)
{
	int nonblocking = 1;
	#if defined(__BEOS__) && defined(BEOS_NET_SERVER)
	return setsockopt(d, SOL_SOCKET, SO_NONBLOCK, &nonblocking, sizeof(nonblocking)) == 0;
	#else
	return ioctlsocket(d, FIONBIO, &nonblocking) == 0;
	#endif
}

static inline bool SetNoDelay(int d)
{
	// XXX should this be done at all?
	#if !defined(BEOS_NET_SERVER) // not implemented on BeOS net_server
	int b = 1;
	// The (const char*) cast is needed for windows
	return setsockopt(d, IPPROTO_TCP, TCP_NODELAY, (const char*)&b, sizeof(b)) == 0;
	#else
	return true;
	#endif
}

#endif // NETWORK_CORE_H
