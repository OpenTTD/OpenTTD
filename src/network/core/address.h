/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file core/address.h Wrapper for network addresses. */

#ifndef NETWORK_CORE_ADDRESS_H
#define NETWORK_CORE_ADDRESS_H

#include "os_abstraction.h"
#include "config.h"
#include "../../string_func.h"
#include "../../core/smallmap_type.hpp"

#ifdef ENABLE_NETWORK

class NetworkAddress;
typedef SmallVector<NetworkAddress, 4> NetworkAddressList;
typedef SmallMap<NetworkAddress, SOCKET, 4> SocketList;

/**
 * Wrapper for (un)resolved network addresses; there's no reason to transform
 * a numeric IP to a string and then back again to pass it to functions. It
 * furthermore allows easier delaying of the hostname lookup.
 */
class NetworkAddress {
private:
	char hostname[NETWORK_HOSTNAME_LENGTH]; ///< The hostname
	int address_length;                     ///< The length of the resolved address
	sockaddr_storage address;               ///< The resolved address

	/**
	 * Helper function to resolve something to a socket.
	 * @param runp information about the socket to try not
	 * @return the opened socket or INVALID_SOCKET
	 */
	typedef SOCKET (*LoopProc)(addrinfo *runp);

	SOCKET Resolve(int family, int socktype, int flags, SocketList *sockets, LoopProc func);
public:
	/**
	 * Create a network address based on a resolved IP and port
	 * @param address the IP address with port
	 */
	NetworkAddress(struct sockaddr_storage &address, int address_length) :
		address_length(address_length),
		address(address)
	{
		*this->hostname = '\0';
	}

	/**
	 * Create a network address based on a resolved IP and port
	 * @param address the IP address with port
	 */
	NetworkAddress(sockaddr *address, int address_length) :
		address_length(address_length)
	{
		*this->hostname = '\0';
		memset(&this->address, 0, sizeof(this->address));
		memcpy(&this->address, address, address_length);
	}

	/**
	 * Create a network address based on a unresolved host and port
	 * @param ip the unresolved hostname
	 * @param port the port
	 * @param family the address family
	 */
	NetworkAddress(const char *hostname = "", uint16 port = 0, int family = AF_UNSPEC) :
		address_length(0)
	{
		/* Also handle IPv6 bracket enclosed hostnames */
		if (StrEmpty(hostname)) hostname = "";
		if (*hostname == '[') hostname++;
		strecpy(this->hostname, StrEmpty(hostname) ? "" : hostname, lastof(this->hostname));
		char *tmp = strrchr(this->hostname, ']');
		if (tmp != NULL) *tmp = '\0';

		memset(&this->address, 0, sizeof(this->address));
		this->address.ss_family = family;
		this->SetPort(port);
	}

	/**
	 * Make a clone of another address
	 * @param address the address to clone
	 */
	NetworkAddress(const NetworkAddress &address)
	{
		memcpy(this, &address, sizeof(*this));
	}

	const char *GetHostname();
	void GetAddressAsString(char *buffer, const char *last, bool with_family = true);
	const char *GetAddressAsString(bool with_family = true);
	const sockaddr_storage *GetAddress();

	/**
	 * Get the (valid) length of the address.
	 * @return the length
	 */
	int GetAddressLength()
	{
		/* Resolve it if we didn't do it already */
		if (!this->IsResolved()) this->GetAddress();
		return this->address_length;
	}

	uint16 GetPort() const;
	void SetPort(uint16 port);

	/**
	 * Check whether the IP address has been resolved already
	 * @return true iff the port has been resolved
	 */
	bool IsResolved() const
	{
		return this->address_length != 0;
	}

	bool IsFamily(int family);
	bool IsInNetmask(char *netmask);

	/**
	 * Compare the address of this class with the address of another.
	 * @param address the other address.
	 * @return < 0 if address is less, 0 if equal and > 0 if address is more
	 */
	int CompareTo(NetworkAddress &address)
	{
		int r = this->GetAddressLength() - address.GetAddressLength();
		if (r == 0) r = this->address.ss_family - address.address.ss_family;
		if (r == 0) r = memcmp(&this->address, &address.address, this->address_length);
		if (r == 0) r = this->GetPort() - address.GetPort();
		return r;
	}

	/**
	 * Compare the address of this class with the address of another.
	 * @param address the other address.
	 * @return true if both match.
	 */
	bool operator == (NetworkAddress &address)
	{
		return this->CompareTo(address) == 0;
	}

	/**
	 * Compare the address of this class with the address of another.
	 * @param address the other address.
	 * @return true if both match.
	 */
	bool operator == (NetworkAddress &address) const
	{
		return const_cast<NetworkAddress*>(this)->CompareTo(address) == 0;
	}
	/**
	 * Compare the address of this class with the address of another.
	 * @param address the other address.
	 * @return true if both do not match.
	 */
	bool operator != (NetworkAddress address) const
	{
		return const_cast<NetworkAddress*>(this)->CompareTo(address) != 0;
	}

	/**
	 * Compare the address of this class with the address of another.
	 * @param address the other address.
	 */
	bool operator < (NetworkAddress &address)
	{
		return this->CompareTo(address) < 0;
	}

	SOCKET Connect();
	void Listen(int socktype, SocketList *sockets);

	static const char *SocketTypeAsString(int socktype);
	static const char *AddressFamilyAsString(int family);
};

#endif /* ENABLE_NETWORK */
#endif /* NETWORK_CORE_ADDRESS_H */
