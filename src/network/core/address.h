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
	char *hostname;           ///< The hostname, NULL if there isn't one
	size_t address_length;    ///< The length of the resolved address
	sockaddr_storage address; ///< The resolved address

	/**
	 * Helper function to resolve something to a socket.
	 * @param runp information about the socket to try not
	 * @return the opened socket or INVALID_SOCKET
	 */
	typedef SOCKET (*LoopProc)(addrinfo *runp);

	/**
	 * Resolve this address into a socket
	 * @param family the type of 'protocol' (IPv4, IPv6)
	 * @param socktype the type of socket (TCP, UDP, etc)
	 * @param flags the flags to send to getaddrinfo
	 * @param func the inner working while looping over the address info
	 * @return the resolved socket or INVALID_SOCKET.
	 */
	SOCKET Resolve(int family, int socktype, int flags, LoopProc func);
public:
	/**
	 * Create a network address based on a resolved IP and port
	 * @param ip the resolved ip
	 * @param port the port
	 */
	NetworkAddress(in_addr_t ip, uint16 port) :
		hostname(NULL),
		address_length(sizeof(sockaddr))
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
	NetworkAddress(struct sockaddr_storage &address, size_t address_length) :
		hostname(NULL),
		address_length(address_length),
		address(address)
	{
	}

	/**
	 * Create a network address based on a unresolved host and port
	 * @param ip the unresolved hostname
	 * @param port the port
	 */
	NetworkAddress(const char *hostname = "0.0.0.0", uint16 port = 0) :
		hostname(strdup(hostname)),
		address_length(0)
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
		hostname(address.hostname == NULL ? NULL : strdup(address.hostname)),
		address_length(address.address_length),
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
	 * Get the (valid) length of the address.
	 * @return the length
	 */
	size_t GetAddressLength()
	{
		/* Resolve it if we didn't do it already */
		if (!this->IsResolved()) this->GetAddress();
		return this->address_length;
	}

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
		return this->address_length != 0;
	}

	/**
	 * Compare the address of this class with the address of another.
	 * @param address the other address.
	 */
	bool operator == (NetworkAddress &address)
	{
		if (this->IsResolved() && address.IsResolved()) return memcmp(&this->address, &address.address, sizeof(this->address)) == 0;
		return this->GetPort() == address.GetPort() && strcmp(this->GetHostname(), address.GetHostname()) == 0;
	}

	/**
	 * Assign another address to ourself
	 * @param other obviously the address to assign to us
	 * @return 'this'
	 */
	NetworkAddress& operator = (const NetworkAddress &other)
	{
		if (this != &other) { // protect against invalid self-assignment
			free(this->hostname);
			memcpy(this, &other, sizeof(*this));
			if (other.hostname != NULL) this->hostname = strdup(other.hostname);
		}
		return *this;
	}

	/**
	 * Connect to the given address.
	 * @return the connected socket or INVALID_SOCKET.
	 */
	SOCKET Connect();

	/**
	 * Make the given socket listen.
	 * @param family the type of 'protocol' (IPv4, IPv6)
	 * @param socktype the type of socket (TCP, UDP, etc)
	 * @return the listening socket or INVALID_SOCKET.
	 */
	SOCKET Listen(int family, int socktype);
};

#endif /* ENABLE_NETWORK */
#endif /* NETWORK_ADDRESS_H */
