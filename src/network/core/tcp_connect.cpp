/* $Id$ */

/**
 * @file tcp_connect.cpp Basic functions to create connections without blocking.
 */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"
#include "../../debug.h"
#include "../../core/smallvec_type.hpp"
#include "../../thread.h"

#include "tcp.h"

/** List of connections that are currently being created */
static SmallVector<TCPConnecter *,  1> _tcp_connecters;

TCPConnecter::TCPConnecter(const NetworkAddress &address) :
	connected(false),
	aborted(false),
	killed(false),
	sock(INVALID_SOCKET),
	address(address)
{
	*_tcp_connecters.Append() = this;
	if (!ThreadObject::New(TCPConnecter::ThreadEntry, this, &this->thread)) {
		this->Connect();
	}
}

void TCPConnecter::Connect()
{
	DEBUG(net, 1, "Connecting to %s %d", address.GetHostname(), address.GetPort());

	this->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (this->sock == INVALID_SOCKET) {
		this->aborted = true;
		return;
	}

	if (!SetNoDelay(this->sock)) DEBUG(net, 1, "Setting TCP_NODELAY failed");

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = address.GetIP();
	sin.sin_port = htons(address.GetPort());

	/* We failed to connect for which reason what so ever */
	if (connect(this->sock, (struct sockaddr*) &sin, sizeof(sin)) != 0) {
		closesocket(this->sock);
		this->sock = INVALID_SOCKET;
		this->aborted = true;
		return;
	}

	if (!SetNonBlocking(this->sock)) DEBUG(net, 0, "Setting non-blocking mode failed");

	this->connected = true;
}


/* static */ void TCPConnecter::ThreadEntry(void *param)
{
	static_cast<TCPConnecter*>(param)->Connect();
}

/* static */ void TCPConnecter::CheckCallbacks()
{
	for (TCPConnecter **iter = _tcp_connecters.Begin(); iter < _tcp_connecters.End(); /* nothing */) {
		TCPConnecter *cur = *iter;
		if ((cur->connected || cur->aborted) && cur->killed) {
			_tcp_connecters.Erase(iter);
			if (cur->sock != INVALID_SOCKET) closesocket(cur->sock);
			delete cur;
			continue;
		}
		if (cur->connected) {
			_tcp_connecters.Erase(iter);
			cur->OnConnect(cur->sock);
			delete cur;
			continue;
		}
		if (cur->aborted) {
			_tcp_connecters.Erase(iter);
			cur->OnFailure();
			delete cur;
			continue;
		}
		iter++;
	}
}

/* static */ void TCPConnecter::KillAll()
{
	for (TCPConnecter **iter = _tcp_connecters.Begin(); iter != _tcp_connecters.End(); iter++) (*iter)->killed = true;
}

#endif /* ENABLE_NETWORK */
