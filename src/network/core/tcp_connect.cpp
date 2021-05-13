/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_connect.cpp Basic functions to create connections without blocking.
 */

#include "../../stdafx.h"
#include "../../thread.h"

#include "tcp.h"
#include "../network_internal.h"

#include <deque>

#include "../../safeguards.h"

/** List of connections that are currently being created */
static std::vector<TCPConnecter *> _tcp_connecters;

/**
 * Create a new connecter for the given address
 * @param connection_string the address to connect to
 */
TCPConnecter::TCPConnecter(const std::string &connection_string, uint16 default_port)
{
	this->connection_string = NormalizeConnectionString(connection_string, default_port);

	_tcp_connecters.push_back(this);
}

TCPConnecter::~TCPConnecter()
{
	if (this->resolve_thread.joinable()) {
		this->resolve_thread.join();
	}

	for (const auto &socket : this->sockets) {
		closesocket(socket);
	}
	this->sockets.clear();
	this->sock_to_address.clear();

	freeaddrinfo(this->ai);
}

/**
 * Start a connection to the indicated address.
 * @param address The address to connection to.
 */
void TCPConnecter::Connect(addrinfo *address)
{
	SOCKET sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
	if (sock == INVALID_SOCKET) {
		DEBUG(net, 0, "Could not create %s %s socket: %s", NetworkAddress::SocketTypeAsString(address->ai_socktype), NetworkAddress::AddressFamilyAsString(address->ai_family), NetworkError::GetLast().AsString());
		return;
	}

	if (!SetNoDelay(sock)) {
		DEBUG(net, 1, "Setting TCP_NODELAY failed: %s", NetworkError::GetLast().AsString());
	}
	if (!SetNonBlocking(sock)) {
		DEBUG(net, 0, "Setting non-blocking mode failed: %s", NetworkError::GetLast().AsString());
	}

	NetworkAddress network_address = NetworkAddress(address->ai_addr, (int)address->ai_addrlen);
	DEBUG(net, 5, "Attempting to connect to %s", network_address.GetAddressAsString().c_str());

	int err = connect(sock, address->ai_addr, (int)address->ai_addrlen);
	if (err != 0 && !NetworkError::GetLast().IsConnectInProgress()) {
		closesocket(sock);

		DEBUG(net, 1, "Could not connect to %s: %s", network_address.GetAddressAsString().c_str(), NetworkError::GetLast().AsString());
		return;
	}

	this->sock_to_address[sock] = network_address;
	this->sockets.push_back(sock);
}

/**
 * Start the connect() for the next address in the list.
 * @return True iff a new connect() is attempted.
 */
bool TCPConnecter::TryNextAddress()
{
	if (this->current_address >= this->addresses.size()) return false;

	this->last_attempt = std::chrono::steady_clock::now();
	this->Connect(this->addresses[this->current_address++]);

	return true;
}

/**
 * Callback when resolving is done.
 * @param ai A linked-list of address information.
 */
void TCPConnecter::OnResolved(addrinfo *ai)
{
	std::deque<addrinfo *> addresses_ipv4, addresses_ipv6;

	/* Apply "Happy Eyeballs" if it is likely IPv6 is functional. */

	/* Detect if IPv6 is likely to succeed or not. */
	bool seen_ipv6 = false;
	bool resort = true;
	for (addrinfo *runp = ai; runp != nullptr; runp = runp->ai_next) {
		if (runp->ai_family == AF_INET6) {
			seen_ipv6 = true;
		} else if (!seen_ipv6) {
			/* We see an IPv4 before an IPv6; this most likely means there is
			 * no IPv6 available on the system, so keep the order of this
			 * list. */
			resort = false;
			break;
		}
	}

	/* Convert the addrinfo into NetworkAddresses. */
	for (addrinfo *runp = ai; runp != nullptr; runp = runp->ai_next) {
		if (resort) {
			if (runp->ai_family == AF_INET6) {
				addresses_ipv6.emplace_back(runp);
			} else {
				addresses_ipv4.emplace_back(runp);
			}
		} else {
			this->addresses.emplace_back(runp);
		}
	}

	/* If we want to resort, make the list like IPv6 / IPv4 / IPv6 / IPv4 / ..
	 * for how ever many (round-robin) DNS entries we have. */
	if (resort) {
		while (!addresses_ipv4.empty() || !addresses_ipv6.empty()) {
			if (!addresses_ipv6.empty()) {
				this->addresses.push_back(addresses_ipv6.front());
				addresses_ipv6.pop_front();
			}
			if (!addresses_ipv4.empty()) {
				this->addresses.push_back(addresses_ipv4.front());
				addresses_ipv4.pop_front();
			}
		}
	}

	if (_debug_net_level >= 6) {
		DEBUG(net, 6, "%s resolved in:", this->connection_string.c_str());
		for (const auto &address : this->addresses) {
			DEBUG(net, 6, "- %s", NetworkAddress(address->ai_addr, (int)address->ai_addrlen).GetAddressAsString().c_str());
		}
	}

	this->current_address = 0;
}

/**
 * Start resolving the hostname.
 *
 * This function must change "status" to either Status::FAILURE
 * or Status::CONNECTING before returning.
 */
void TCPConnecter::Resolve()
{
	/* Port is already guaranteed part of the connection_string. */
	NetworkAddress address = ParseConnectionString(this->connection_string, 0);

	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_socktype = SOCK_STREAM;

	char port_name[6];
	seprintf(port_name, lastof(port_name), "%u", address.GetPort());

	static bool getaddrinfo_timeout_error_shown = false;
	auto start = std::chrono::steady_clock::now();

	addrinfo *ai;
	int error = getaddrinfo(address.GetHostname(), port_name, &hints, &ai);

	auto end = std::chrono::steady_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
	if (!getaddrinfo_timeout_error_shown && duration >= std::chrono::seconds(5)) {
		DEBUG(net, 0, "getaddrinfo() for address \"%s\" took %i seconds", this->connection_string.c_str(), (int)duration.count());
		DEBUG(net, 0, "  This is likely an issue in the DNS name resolver's configuration causing it to time out");
		getaddrinfo_timeout_error_shown = true;
	}

	if (error != 0) {
		DEBUG(net, 0, "Failed to resolve DNS for %s", this->connection_string.c_str());
		this->status = Status::FAILURE;
		return;
	}

	this->ai = ai;
	this->OnResolved(ai);

	this->status = Status::CONNECTING;
}

/**
 * Thunk to start Resolve() on the right instance.
 */
/* static */ void TCPConnecter::ResolveThunk(TCPConnecter *connecter)
{
	connecter->Resolve();
}

/**
 * Check if there was activity for this connecter.
 * @return True iff the TCPConnecter is done and can be cleaned up.
 */
bool TCPConnecter::CheckActivity()
{
	switch (this->status.load()) {
		case Status::INIT:
			/* Start the thread delayed, so the vtable is loaded. This allows classes
			 * to overload functions used by Resolve() (in case threading is disabled). */
			if (StartNewThread(&this->resolve_thread, "ottd:resolve", &TCPConnecter::ResolveThunk, this)) {
				this->status = Status::RESOLVING;
				return false;
			}

			/* No threads, do a blocking resolve. */
			this->Resolve();

			/* Continue as we are either failed or can start the first
			 * connection. The rest of this function handles exactly that. */
			break;

		case Status::RESOLVING:
			/* Wait till Resolve() comes back with an answer (in case it runs threaded). */
			return false;

		case Status::FAILURE:
			/* Ensure the OnFailure() is called from the game-thread instead of the
			 * resolve-thread, as otherwise we can get into some threading issues. */
			this->OnFailure();
			return true;

		case Status::CONNECTING:
			break;
	}

	/* If there are no attempts pending, connect to the next. */
	if (this->sockets.empty()) {
		if (!this->TryNextAddress()) {
			/* There were no more addresses to try, so we failed. */
			this->OnFailure();
			return true;
		}
		return false;
	}

	fd_set write_fd;
	FD_ZERO(&write_fd);
	for (const auto &socket : this->sockets) {
		FD_SET(socket, &write_fd);
	}

	timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = 0;
	int n = select(FD_SETSIZE, NULL, &write_fd, NULL, &tv);
	/* select() failed; hopefully next try it doesn't. */
	if (n < 0) {
		/* select() normally never fails; so hopefully it works next try! */
		DEBUG(net, 1, "select() failed: %s", NetworkError::GetLast().AsString());
		return false;
	}

	/* No socket updates. */
	if (n == 0) {
		/* Wait 250ms between attempting another address. */
		if (std::chrono::steady_clock::now() < this->last_attempt + std::chrono::milliseconds(250)) return false;

		/* Try the next address in the list. */
		if (this->TryNextAddress()) return false;

		/* Wait up to 3 seconds since the last connection we started. */
		if (std::chrono::steady_clock::now() < this->last_attempt + std::chrono::milliseconds(3000)) return false;

		/* More than 3 seconds no socket reported activity, and there are no
		 * more address to try. Timeout the attempt. */
		DEBUG(net, 0, "Timeout while connecting to %s", this->connection_string.c_str());

		for (const auto &socket : this->sockets) {
			closesocket(socket);
		}
		this->sockets.clear();
		this->sock_to_address.clear();

		this->OnFailure();
		return true;
	}

	/* Check for errors on any of the sockets. */
	for (auto it = this->sockets.begin(); it != this->sockets.end(); /* nothing */) {
		NetworkError socket_error = GetSocketError(*it);
		if (socket_error.HasError()) {
			DEBUG(net, 1, "Could not connect to %s: %s", this->sock_to_address[*it].GetAddressAsString().c_str(), socket_error.AsString());
			closesocket(*it);
			this->sock_to_address.erase(*it);
			it = this->sockets.erase(it);
		} else {
			it++;
		}
	}

	/* In case all sockets had an error, queue a new one. */
	if (this->sockets.empty()) {
		if (!this->TryNextAddress()) {
			/* There were no more addresses to try, so we failed. */
			this->OnFailure();
			return true;
		}
		return false;
	}

	/* At least one socket is connected. The first one that does is the one
	 * we will be using, and we close all other sockets. */
	SOCKET connected_socket = INVALID_SOCKET;
	for (auto it = this->sockets.begin(); it != this->sockets.end(); /* nothing */) {
		if (connected_socket == INVALID_SOCKET && FD_ISSET(*it, &write_fd)) {
			connected_socket = *it;
		} else {
			closesocket(*it);
		}
		this->sock_to_address.erase(*it);
		it = this->sockets.erase(it);
	}
	assert(connected_socket != INVALID_SOCKET);

	DEBUG(net, 3, "Connected to %s", this->connection_string.c_str());
	if (_debug_net_level >= 5) {
		DEBUG(net, 5, "- using %s", NetworkAddress::GetPeerName(connected_socket).c_str());
	}

	this->OnConnect(connected_socket);
	return true;
}

/**
 * Check whether we need to call the callback, i.e. whether we
 * have connected or aborted and call the appropriate callback
 * for that. It's done this way to ease on the locking that
 * would otherwise be needed everywhere.
 */
/* static */ void TCPConnecter::CheckCallbacks()
{
	for (auto iter = _tcp_connecters.begin(); iter < _tcp_connecters.end(); /* nothing */) {
		TCPConnecter *cur = *iter;

		if (cur->CheckActivity()) {
			iter = _tcp_connecters.erase(iter);
			delete cur;
		} else {
			iter++;
		}
	}
}

/** Kill all connection attempts. */
/* static */ void TCPConnecter::KillAll()
{
	for (auto iter = _tcp_connecters.begin(); iter < _tcp_connecters.end(); /* nothing */) {
		TCPConnecter *cur = *iter;
		iter = _tcp_connecters.erase(iter);
		delete cur;
	}
}
