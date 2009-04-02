/* $Id$ */

/** @file core/address.cpp Implementation of the address. */

#include "../../stdafx.h"

#ifdef ENABLE_NETWORK

#include "address.h"
#include "config.h"
#include "host.h"
#include "../../string_func.h"

const char *NetworkAddress::GetHostname() const
{
	if (this->hostname != NULL) return this->hostname;

	in_addr addr;
	addr.s_addr = this->ip;
	return inet_ntoa(addr);
}

uint32 NetworkAddress::GetIP()
{
	if (!this->resolved) {
		this->ip = NetworkResolveHost(this->hostname);
		this->resolved = true;
	}
	return this->ip;
}

const char *NetworkAddress::GetAddressAsString() const
{
	/* 6 = for the : and 5 for the decimal port number */
	static char buf[NETWORK_HOSTNAME_LENGTH + 6];

	seprintf(buf, lastof(buf), "%s:%d", this->GetHostname(), this->GetPort());
	return buf;
}

#endif /* ENABLE_NETWORK */
