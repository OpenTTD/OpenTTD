#ifndef NETWORK_CORE_H
#define NETWORK_CORE_H

// Network stuff has many things that needs to be included
//  by default. All those things are in this file.

// =============================
// Include standard stuff per OS

// Windows stuff
#if defined(WIN32)
#	include <windows.h>
#	include <winsock2.h>
#	include <ws2tcpip.h>
#	pragma comment (lib, "ws2_32.lib")
//#	define ENABLE_NETWORK // On windows, the network is always enabled
#	define GET_LAST_ERROR() WSAGetLastError()
#	define EWOULDBLOCK WSAEWOULDBLOCK
// Windows has some different names for some types..
typedef SSIZE_T ssize_t;
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
#		if !defined(SUNOS) && !defined(__MORPHOS__)
#			include <ifaddrs.h>
// If for any reason ifaddrs.h does not exist on a system, remove define below
//   and an other system will be used to fetch ips from the system
#			define HAVE_GETIFADDRS
#		else
#			define INADDR_NONE 0xffffffff
#		endif // SUNOS
#	endif // BEOS_NET_SERVER
#	include <errno.h>
#	include <sys/time.h>
#	include <netdb.h>
#endif // UNIX

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

#endif // NETWORK_CORE_H
