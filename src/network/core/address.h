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
	bool resolved;  ///< Has the IP address been resolved
	char *hostname; ///< The hostname, NULL if there isn't one
	uint32 ip;      ///< The resolved IP address
	uint16 port;    ///< The port associated with the address

public:
	/**
	 * Create a network address based on a resolved IP and port
	 * @param ip the resolved ip
	 * @param port the port
	 */
	NetworkAddress(in_addr_t ip, uint16 port) :
		resolved(true),
		hostname(NULL),
		ip(ip),
		port(port)
	{
	}

	/**
	 * Create a network address based on a unresolved host and port
	 * @param ip the unresolved hostname
	 * @param port the port
	 */
	NetworkAddress(const char *hostname, uint16 port) :
		resolved(false),
		hostname(strdup(hostname)),
		ip(0),
		port(port)
	{
	}

	/**
	 * Make a clone of another address
	 * @param address the address to clone
	 */
	NetworkAddress(const NetworkAddress &address) :
		resolved(address.resolved),
		hostname(address.hostname == NULL ? NULL : strdup(address.hostname)),
		ip(address.ip),
		port(address.port)
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
	const char *GetHostname() const;

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
	uint16 GetPort() const
	{
		return this->port;
	}

	/**
	 * Check whether the IP address has been resolved already
	 * @return true iff the port has been resolved
	 */
	bool IsResolved() const
	{
		return this->resolved;
	}
};

#endif /* ENABLE_NETWORK */
#endif /* NETWORK_ADDRESS_H */
