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

/**
 * Abstraction of a network error where all implementation details of the
 * error codes are encapsulated in this class and the abstraction layer.
 */
class NetworkError {
private:
	int error;                   ///< The underlying error number from errno or WSAGetLastError.
	mutable std::string message; ///< The string representation of the error (set on first call to #AsString).
public:
	NetworkError(int error);

	bool HasError() const;
	bool WouldBlock() const;
	bool IsConnectionReset() const;
	bool IsConnectInProgress() const;
	const std::string &AsString() const;

	static NetworkError GetLast();
};

/* Include standard stuff per OS */

/* Windows stuff */
#if defined(_WIN32)
#include <errno.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

/* Windows has some different names for some types */
typedef unsigned long in_addr_t;

/* Handle cross-compilation with --build=*-*-cygwin --host=*-*-mingw32 */
#if defined(__MINGW32__) && !defined(AI_ADDRCONFIG)
#	define AI_ADDRCONFIG               0x00000400
#endif

#if !(defined(__MINGW32__) || defined(__CYGWIN__))
	/* Windows has some different names for some types */
	typedef SSIZE_T ssize_t;
	typedef int socklen_t;
#	define IPPROTO_IPV6 41
#endif /* !(__MINGW32__ && __CYGWIN__) */
#endif /* _WIN32 */

/* UNIX stuff */
#if defined(UNIX)
#	if defined(OPENBSD) || defined(__NetBSD__)
#		define AI_ADDRCONFIG 0
#	endif
#	define SOCKET int
#	define INVALID_SOCKET -1
#	define closesocket close
/* Need this for FIONREAD on solaris */
#	define BSD_COMP

/* Includes needed for UNIX-like systems */
#	include <unistd.h>
#	include <sys/ioctl.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <netinet/tcp.h>
#	include <arpa/inet.h>
#	include <net/if.h>
#	include <ifaddrs.h>
#	if !defined(INADDR_NONE)
#		define INADDR_NONE 0xffffffff
#	endif

#	if defined(__GLIBC__) && (__GLIBC__ <= 2) && (__GLIBC_MINOR__ <= 1)
		typedef uint32_t in_addr_t;
#	endif

#	include <errno.h>
#	include <sys/time.h>
#	include <netdb.h>

#   if defined(__EMSCRIPTEN__)
/* Emscripten doesn't support AI_ADDRCONFIG and errors out on it. */
#		undef AI_ADDRCONFIG
#		define AI_ADDRCONFIG 0
/* Emscripten says it supports FD_SETSIZE fds, but it really only supports 64.
 * https://github.com/emscripten-core/emscripten/issues/1711 */
#		undef FD_SETSIZE
#		define FD_SETSIZE 64
#   endif

/* Haiku says it supports FD_SETSIZE fds, but it really only supports 512. */
#   if defined(__HAIKU__)
#		undef FD_SETSIZE
#		define FD_SETSIZE 512
#   endif

#endif /* UNIX */

#ifdef __EMSCRIPTEN__
/**
 * Emscripten doesn't set 'addrlen' for accept(), getsockname(), getpeername()
 * and recvfrom(), which confuses other functions and causes them to crash.
 * This function needs to be called after these four functions to make sure
 * 'addrlen' is patched up.
 *
 * https://github.com/emscripten-core/emscripten/issues/12996
 *
 * @param address The address returned by those four functions.
 * @return The correct value for addrlen.
 */
inline socklen_t FixAddrLenForEmscripten(struct sockaddr_storage &address)
{
	switch (address.ss_family) {
		case AF_INET6: return sizeof(struct sockaddr_in6);
		case AF_INET: return sizeof(struct sockaddr_in);
		default: NOT_REACHED();
	}
}
#endif


bool SetNonBlocking(SOCKET d);
bool SetNoDelay(SOCKET d);
bool SetReusePort(SOCKET d);
NetworkError GetSocketError(SOCKET d);

/* Make sure these structures have the size we expect them to be */
static_assert(sizeof(in_addr)  ==  4); ///< IPv4 addresses should be 4 bytes.
static_assert(sizeof(in6_addr) == 16); ///< IPv6 addresses should be 16 bytes.

#endif /* NETWORK_CORE_OS_ABSTRACTION_H */
