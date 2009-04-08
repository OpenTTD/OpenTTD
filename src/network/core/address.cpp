/* $Id$ */

/** @file core/address.cpp Implementation of the address. */

#include "../../stdafx.h"

#ifdef ENABLE_NETWORK

#include "address.h"
#include "config.h"
#include "host.h"
#include "../../string_func.h"
#include "../../debug.h"

const char *NetworkAddress::GetHostname()
{
	if (StrEmpty(this->hostname)) {
		assert(this->address_length != 0);
		char *buf = this->hostname;
		if (this->address.ss_family == AF_INET6) buf = strecpy(buf, "[", lastof(this->hostname));
		getnameinfo((struct sockaddr *)&this->address, this->address_length, buf, lastof(this->hostname) - buf, NULL, 0, NI_NUMERICHOST);
		if (this->address.ss_family == AF_INET6) strecat(buf, "]", lastof(this->hostname));
	}
	return this->hostname;
}

uint16 NetworkAddress::GetPort() const
{
	switch (this->address.ss_family) {
		case AF_UNSPEC:
		case AF_INET:
			return ntohs(((struct sockaddr_in *)&this->address)->sin_port);

		case AF_INET6:
			return ntohs(((struct sockaddr_in6 *)&this->address)->sin6_port);

		default:
			NOT_REACHED();
	}
}

void NetworkAddress::SetPort(uint16 port)
{
	switch (this->address.ss_family) {
		case AF_UNSPEC:
		case AF_INET:
			((struct sockaddr_in*)&this->address)->sin_port = htons(port);
			break;

		case AF_INET6:
			((struct sockaddr_in6*)&this->address)->sin6_port = htons(port);
			break;

		default:
			NOT_REACHED();
	}
}

const char *NetworkAddress::GetAddressAsString()
{
	/* 6 = for the : and 5 for the decimal port number */
	static char buf[NETWORK_HOSTNAME_LENGTH + 6 + 7];

	char family;
	switch (this->address.ss_family) {
		case AF_INET:  family = '4'; break;
		case AF_INET6: family = '6'; break;
		default:       family = '?'; break;
	}
	seprintf(buf, lastof(buf), "%s:%d (IPv%c)", this->GetHostname(), this->GetPort(), family);
	return buf;
}

/**
 * Helper function to resolve without opening a socket.
 * @param runp information about the socket to try not
 * @return the opened socket or INVALID_SOCKET
 */
static SOCKET ResolveLoopProc(addrinfo *runp)
{
	/* We just want the first 'entry', so return a valid socket. */
	return !INVALID_SOCKET;
}

const sockaddr_storage *NetworkAddress::GetAddress()
{
	if (!this->IsResolved()) {
		/* Here we try to resolve a network address. We use SOCK_STREAM as
		 * socket type because some stupid OSes, like Solaris, cannot be
		 * bothered to implement the specifications and allow '0' as value
		 * that means "don't care whether it is SOCK_STREAM or SOCK_DGRAM".
		 */
		this->Resolve(this->address.ss_family, SOCK_STREAM, AI_ADDRCONFIG, NULL, ResolveLoopProc);
	}
	return &this->address;
}

bool NetworkAddress::IsFamily(int family)
{
	if (!this->IsResolved()) {
		this->Resolve(family, SOCK_STREAM, AI_ADDRCONFIG, NULL, ResolveLoopProc);
	}
	return this->address.ss_family == family;
}

bool NetworkAddress::IsInNetmask(char *netmask)
{
	/* Resolve it if we didn't do it already */
	if (!this->IsResolved()) this->GetAddress();

	int cidr = this->address.ss_family == AF_INET ? 32 : 128;

	NetworkAddress mask_address;

	/* Check for CIDR separator */
	char *chr_cidr = strchr(netmask, '/');
	if (chr_cidr != NULL) {
		int tmp_cidr = atoi(chr_cidr + 1);

		/* Invalid CIDR, treat as single host */
		if (tmp_cidr > 0 || tmp_cidr < cidr) cidr = tmp_cidr;

		/* Remove and then replace the / so that NetworkAddress works on the IP portion */
		*chr_cidr = '\0';
		mask_address = NetworkAddress(netmask, 0, this->address.ss_family);
		*chr_cidr = '/';
	} else {
		mask_address = NetworkAddress(netmask, 0, this->address.ss_family);
	}

	if (mask_address.GetAddressLength() == 0) return false;

	uint32 *ip;
	uint32 *mask;
	switch (this->address.ss_family) {
		case AF_INET:
			ip = (uint32*)&((struct sockaddr_in*)&this->address)->sin_addr.s_addr;
			mask = (uint32*)&((struct sockaddr_in*)&mask_address.address)->sin_addr.s_addr;
			break;

		case AF_INET6:
			ip = (uint32*)&((struct sockaddr_in6*)&this->address)->sin6_addr;
			mask = (uint32*)&((struct sockaddr_in6*)&mask_address.address)->sin6_addr;
			break;

		default:
			NOT_REACHED();
	}

	while (cidr > 0) {
		uint32 msk = cidr >= 32 ? (uint32)-1 : htonl(-(1 << (32 - cidr)));
		if ((*mask & msk) != (*ip & msk)) return false;

		cidr -= 32;
	}

	return true;
}

SOCKET NetworkAddress::Resolve(int family, int socktype, int flags, SocketList *sockets, LoopProc func)
{
	struct addrinfo *ai;
	struct addrinfo hints;
	memset(&hints, 0, sizeof (hints));
	hints.ai_family   = family;
	hints.ai_flags    = flags;
	hints.ai_socktype = socktype;

	/* The port needs to be a string. Six is enough to contain all characters + '\0'. */
	char port_name[6];
	seprintf(port_name, lastof(port_name), "%u", this->GetPort());

	/* Setting both hostname to NULL and port to 0 is not allowed.
	 * As port 0 means bind to any port, the other must mean that
	 * we want to bind to 'all' IPs. */
	if (this->address_length == 0 && this->GetPort() == 0) {
		strecpy(this->hostname, this->address.ss_family == AF_INET ? "0.0.0.0" : "::", lastof(this->hostname));
	}

	int e = getaddrinfo(StrEmpty(this->hostname) ? NULL : this->hostname, port_name, &hints, &ai);
	if (e != 0) {
		if (func != ResolveLoopProc) {
			DEBUG(net, 0, "getaddrinfo(%s, %s) failed: %s", this->hostname, port_name, FS2OTTD(gai_strerror(e)));
		}
		return INVALID_SOCKET;
	}

	SOCKET sock = INVALID_SOCKET;
	for (struct addrinfo *runp = ai; runp != NULL; runp = runp->ai_next) {
		/* When we are binding to multiple sockets, make sure we do not
		 * connect to one with exactly the same address twice. That's
		 * ofcourse totally unneeded ;) */
		if (sockets != NULL) {
			NetworkAddress address(runp->ai_addr, runp->ai_addrlen);
			if (sockets->Find(address) != sockets->End()) continue;
		}
		sock = func(runp);
		if (sock == INVALID_SOCKET) continue;

		if (sockets == NULL) {
			this->address_length = runp->ai_addrlen;
			assert(sizeof(this->address) >= runp->ai_addrlen);
			memcpy(&this->address, runp->ai_addr, runp->ai_addrlen);
			break;
		}

		NetworkAddress addr(runp->ai_addr, runp->ai_addrlen);
		(*sockets)[addr] = sock;
		sock = INVALID_SOCKET;
	}
	freeaddrinfo (ai);

	return sock;
}

/**
 * Helper function to resolve a connected socket.
 * @param runp information about the socket to try not
 * @return the opened socket or INVALID_SOCKET
 */
static SOCKET ConnectLoopProc(addrinfo *runp)
{
	SOCKET sock = socket(runp->ai_family, runp->ai_socktype, runp->ai_protocol);
	if (sock == INVALID_SOCKET) {
		DEBUG(net, 1, "Could not create socket: %s", strerror(errno));
		return INVALID_SOCKET;
	}

	if (!SetNoDelay(sock)) DEBUG(net, 1, "Setting TCP_NODELAY failed");

	if (connect(sock, runp->ai_addr, runp->ai_addrlen) != 0) {
		DEBUG(net, 1, "Could not connect socket: %s", strerror(errno));
		closesocket(sock);
		return INVALID_SOCKET;
	}

	/* Connection succeeded */
	if (!SetNonBlocking(sock)) DEBUG(net, 0, "Setting non-blocking mode failed");

	return sock;
}

SOCKET NetworkAddress::Connect()
{
	DEBUG(net, 1, "Connecting to %s", this->GetAddressAsString());

	return this->Resolve(0, SOCK_STREAM, AI_ADDRCONFIG, NULL, ConnectLoopProc);
}

/**
 * Helper function to resolve a listening.
 * @param runp information about the socket to try not
 * @return the opened socket or INVALID_SOCKET
 */
static SOCKET ListenLoopProc(addrinfo *runp)
{
	const char *type = runp->ai_socktype == SOCK_STREAM ? "tcp" : "udp";
	const char *address = NetworkAddress(runp->ai_addr, runp->ai_addrlen).GetAddressAsString();

	SOCKET sock = socket(runp->ai_family, runp->ai_socktype, runp->ai_protocol);
	if (sock == INVALID_SOCKET) {
		DEBUG(net, 0, "[%s] Could not create socket on port %s: %s", type, address, strerror(errno));
		return INVALID_SOCKET;
	}

	if (runp->ai_socktype == SOCK_STREAM && !SetNoDelay(sock)) {
		DEBUG(net, 3, "[%s] Setting TCP_NODELAY failed for port %s", type, address);
	}

	int on = 1;
	/* The (const char*) cast is needed for windows!! */
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on)) == -1) {
		DEBUG(net, 3, "[%s] Could not set reusable sockets for port %s: %s", type, address, strerror(errno));
	}

	if (runp->ai_family == AF_INET6 &&
			setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&on, sizeof(on)) == -1) {
		DEBUG(net, 3, "[%s] Could not disable IPv4 over IPv6 on port %s: %s", type, address, strerror(errno));
	}

	if (bind(sock, runp->ai_addr, runp->ai_addrlen) != 0) {
		DEBUG(net, 1, "[%s] Could not bind on port %s: %s", type, address, strerror(errno));
		closesocket(sock);
		return INVALID_SOCKET;
	}

	if (runp->ai_socktype != SOCK_DGRAM && listen(sock, 1) != 0) {
		DEBUG(net, 1, "[%s] Could not listen at port %s: %s", type, address, strerror(errno));
		closesocket(sock);
		return INVALID_SOCKET;
	}

	/* Connection succeeded */
	if (!SetNonBlocking(sock)) DEBUG(net, 0, "[%s] Setting non-blocking mode failed for port %s", type, address);

	DEBUG(net, 1, "[%s] Listening on port %s", type, address);
	return sock;
}

SOCKET NetworkAddress::Listen(int socktype, SocketList *sockets)
{
	return this->Resolve(AF_UNSPEC, socktype, AI_ADDRCONFIG | AI_PASSIVE, sockets, ListenLoopProc);
}

#endif /* ENABLE_NETWORK */
