/* $Id$ */

/** @file core/address.h Wrapper for network addresses. */

#ifndef NETWORK_ADDRESS_H
#define NETWORK_ADDRESS_H

#ifdef ENABLE_NETWORK

#include "os_abstraction.h"

/**
 * Wrapper for (un)resolved network addresses; there's no reason to transform
 * a numeric IP to a string and then back again to pass it to functions. It
 * furthermore allows easier delaying of the hostname lookup.
 */
class NetworkAddress {
private:
	bool resolved;            ///< Has the IP address been resolved
	char *hostname;           ///< The hostname, NULL if there isn't one
	sockaddr_storage address; ///< The resolved address

public:
	/**
	 * Create a network address based on a resolved IP and port
	 * @param ip the resolved ip
	 * @param port the port
	 */
	NetworkAddress(in_addr_t ip, uint16 port) :
		resolved(true),
		hostname(NULL)
	{
		memset(&this->address, 0, sizeof(this->address));
		this->address.ss_family = AF_INET;
		((struct sockaddr_in*)&this->address)->sin_addr.s_addr = ip;
		this->SetPort(port);
	}

	/**
	 * Create a network address based on a resolved IP and port
	 * @param address the IP address with port
	 */
	NetworkAddress(struct sockaddr_storage &address) :
		resolved(true),
		hostname(NULL),
		address(address)
	{
	}

	/**
	 * Create a network address based on a unresolved host and port
	 * @param ip the unresolved hostname
	 * @param port the port
	 */
	NetworkAddress(const char *hostname = NULL, uint16 port = 0) :
		resolved(false),
		hostname(hostname == NULL ? NULL : strdup(hostname))
	{
		memset(&this->address, 0, sizeof(this->address));
		this->address.ss_family = AF_INET;
		this->SetPort(port);
	}

	/**
	 * Make a clone of another address
	 * @param address the address to clone
	 */
	NetworkAddress(const NetworkAddress &address) :
		resolved(address.resolved),
		hostname(address.hostname == NULL ? NULL : strdup(address.hostname)),
		address(address.address)
	{
	}

	/** Clean up our mess */
	~NetworkAddress()
	{
		free(hostname);
	}

	/**
	 * Get the hostname; in case it wasn't given the
	 * IPv4 dotted representation is given.
	 * @return the hostname
	 */
	const char *GetHostname();

	/**
	 * Get the address as a string, e.g. 127.0.0.1:12345.
	 * @return the address
	 */
	const char *GetAddressAsString();

	/**
	 * Get the address in it's internal representation.
	 * @return the address
	 */
	const sockaddr_storage *GetAddress();

	/**
	 * Get the IP address. If the IP has not been resolved yet this will resolve
	 * it possibly blocking this function for a while
	 * @return the IP address
	 */
	uint32 GetIP();

	/**
	 * Get the port
	 * @return the port
	 */
	uint16 GetPort() const;

	/**
	 * Set the port
	 * @param port set the port number
	 */
	void SetPort(uint16 port);

	/**
	 * Check whether the IP address has been resolved already
	 * @return true iff the port has been resolved
	 */
	bool IsResolved() const
	{
		return this->resolved;
	}

	/**
	 * Compare the address of this class with the address of another.
	 * @param address the other address.
	 */
	bool operator == (NetworkAddress &address)
	{
		if (this->IsResolved() != address.IsResolved()) return false;

		if (this->IsResolved()) return memcmp(&this->address, &address.address, sizeof(this->address)) == 0;

		return this->GetPort() == address.GetPort() && strcmp(this->GetHostname(), address.GetHostname()) == 0;
	}
};

#endif /* ENABLE_NETWORK */
#endif /* NETWORK_ADDRESS_H */
