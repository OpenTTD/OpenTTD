/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_connect.cpp Basic functions to create connections without blocking.
 */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"
#include "../../thread/thread.h"

#include "tcp.h"

#include "../../safeguards.h"

/** List of connections that are currently being created */
static std::vector<TCPConnecter *> _tcp_connecters;

/**
 * Create a new connecter for the given address
 * @param address the (un)resolved address to connect to
 */
TCPConnecter::TCPConnecter(const NetworkAddress &address) :
	connected(false),
	aborted(false),
	killed(false),
	sock(INVALID_SOCKET),
	address(address)
{
	_tcp_connecters.push_back(this);
	if (!ThreadObject::New(TCPConnecter::ThreadEntry, this, &this->thread, "ottd:tcp")) {
		this->Connect();
	}
}

/** The actual connection function */
void TCPConnecter::Connect()
{
	this->sock = this->address.Connect();
	if (this->sock == INVALID_SOCKET) {
		this->aborted = true;
	} else {
		this->connected = true;
	}
}

/**
 * Entry point for the new threads.
 * @param param the TCPConnecter instance to call Connect on.
 */
/* static */ void TCPConnecter::ThreadEntry(void *param)
{
	static_cast<TCPConnecter*>(param)->Connect();
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
		if ((cur->connected || cur->aborted) && cur->killed) {
			Erase(_tcp_connecters, iter);
			if (cur->sock != INVALID_SOCKET) closesocket(cur->sock);
			delete cur;
			continue;
		}
		if (cur->connected) {
			Erase(_tcp_connecters, iter);
			cur->OnConnect(cur->sock);
			delete cur;
			continue;
		}
		if (cur->aborted) {
			Erase(_tcp_connecters, iter);
			cur->OnFailure();
			delete cur;
			continue;
		}
		iter++;
	}
}

/** Kill all connection attempts. */
/* static */ void TCPConnecter::KillAll()
{
	for (auto &c : _tcp_connecters) c->killed = true;
}

#endif /* ENABLE_NETWORK */
