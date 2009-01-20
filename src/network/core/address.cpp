/* $Id$ */

/** @file core/address.cpp Implementation of the address. */

#include "../../stdafx.h"

#ifdef ENABLE_NETWORK

#include "address.h"
#include "host.h"

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

#endif /* ENABLE_NETWORK */
