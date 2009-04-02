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
	if (this->hostname == NULL) {
		char buf[NETWORK_HOSTNAME_LENGTH] = { '\0' };
		getnameinfo((struct sockaddr *)&this->address, sizeof(this->address), buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);
		this->hostname = strdup(buf);
	}
	return this->hostname;
}

uint16 NetworkAddress::GetPort() const
{
	switch (this->address.ss_family) {
		case AF_INET:
			return ntohs(((struct sockaddr_in *)&this->address)->sin_port);

		default:
			NOT_REACHED();
	}
}

void NetworkAddress::SetPort(uint16 port)
{
	switch (this->address.ss_family) {
		case AF_INET:
			((struct sockaddr_in*)&this->address)->sin_port = htons(port);
			break;

		default:
			NOT_REACHED();
	}
}

const char *NetworkAddress::GetAddressAsString()
{
	/* 6 = for the : and 5 for the decimal port number */
	static char buf[NETWORK_HOSTNAME_LENGTH + 6];

	seprintf(buf, lastof(buf), "%s:%d", this->GetHostname(), this->GetPort());
	return buf;
}

const sockaddr_storage *NetworkAddress::GetAddress()
{
	if (!this->resolved) {
		((struct sockaddr_in *)&this->address)->sin_addr.s_addr = NetworkResolveHost(this->hostname);
		this->resolved = true;
	}
	return &this->address;
}

SOCKET NetworkAddress::Connect()
{
	DEBUG(net, 1, "Connecting to %s", this->GetAddressAsString());

	struct addrinfo *ai;
	struct addrinfo hints;
	memset(&hints, 0, sizeof (hints));
	hints.ai_flags    = AI_ADDRCONFIG;
	hints.ai_socktype = SOCK_STREAM;

	/* The port needs to be a string. Six is enough to contain all characters + '\0'. */
	char port_name[6];
	seprintf(port_name, lastof(port_name), "%u", this->GetPort());

	int e = getaddrinfo(this->GetHostname(), port_name, &hints, &ai);
	if (e != 0) {
		DEBUG(net, 0, "getaddrinfo failed: %s", gai_strerror(e));
		return false;
	}

	SOCKET sock = INVALID_SOCKET;
	for (struct addrinfo *runp = ai; runp != NULL; runp = runp->ai_next) {
		sock = socket(runp->ai_family, runp->ai_socktype, runp->ai_protocol);
		if (sock == INVALID_SOCKET) continue;

		if (!SetNoDelay(sock)) DEBUG(net, 1, "Setting TCP_NODELAY failed");

		if (connect(sock, runp->ai_addr, runp->ai_addrlen) != 0) continue;

		/* Connection succeeded */
		if (!SetNonBlocking(sock)) DEBUG(net, 0, "Setting non-blocking mode failed");

		break;
	}
	freeaddrinfo (ai);

	return sock;
}

#endif /* ENABLE_NETWORK */
