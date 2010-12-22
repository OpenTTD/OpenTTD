/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_http.h Basic functions to receive and send HTTP TCP packets.
 */

#ifndef NETWORK_CORE_TCP_HTTP_H
#define NETWORK_CORE_TCP_HTTP_H

#include "tcp.h"

#ifdef ENABLE_NETWORK

/** Callback for when the HTTP handler has something to tell us. */
struct HTTPCallback {
	/**
	 * An error has occurred and the connection has been closed.
	 * @note HTTP socket handler is closed/freed.
	 */
	virtual void OnFailure() = 0;

	/**
	 * We're receiving data.
	 * @param data   the received data, NULL when all data has been received.
	 * @param length the amount of received data, 0 when all data has been received.
	 * @note When NULL is sent the HTTP socket handler is closed/freed.
	 */
	virtual void OnReceiveData(const char *data, size_t length) = 0;

	/** Silentium */
	virtual ~HTTPCallback() {}
};

/** Base socket handler for HTTP traffic. */
class NetworkHTTPSocketHandler : public NetworkSocketHandler {
private:
	char recv_buffer[4096];   ///< Partially received message.
	int recv_pos;             ///< Current position in buffer.
	int recv_length;          ///< Length of the data still retrieving.
	HTTPCallback *callback;   ///< The callback to call for the incoming data.
	const char *data;         ///< The (POST) data we might want to forward (to a redirect).
	int redirect_depth;       ///< The depth of the redirection.

	int HandleHeader();
	int Receive();
public:
	SOCKET sock;              ///< The socket currently connected to

	/**
	 * Whether this socket is currently bound to a socket.
	 * @return true when the socket is bound, false otherwise
	 */
	bool IsConnected() const
	{
		return this->sock != INVALID_SOCKET;
	}

	virtual NetworkRecvStatus CloseConnection(bool error = true);

	/**
	 * Start the querying
	 * @param sock     the socket of this connection
	 * @param callback the callback for HTTP retrieval
	 * @param url      the url at the server
	 * @param data     the data to send
	 * @param depth    the depth (redirect recursion) of the queries
	 */
	NetworkHTTPSocketHandler(SOCKET sock, HTTPCallback *callback,
			const char *host, const char *url, const char *data, int depth);

	/** Free whatever needs to be freed. */
	~NetworkHTTPSocketHandler();

	/**
	 * Connect to the given URI.
	 * @param uri      the URI to connect to.
	 * @param callback the callback to send data back on.
	 * @param data     the data we want to send (as POST).
	 * @param depth    the recursion/redirect depth.
	 */
	static int Connect(char *uri, HTTPCallback *callback,
			const char *data = NULL, int depth = 0);

	/**
	 * Do the receiving for all HTTP connections.
	 */
	static void HTTPReceive();
};

/** Connect with a HTTP server and do ONE query. */
class NetworkHTTPContentConnecter : TCPConnecter {
	HTTPCallback *callback; ///< Callback to tell that we received some data (or won't).
	const char *url;        ///< The URL we want to get at the server.
	const char *data;       ///< The data to send
	int depth;              ///< How far we have recursed

public:
	/**
	 * Start the connecting.
	 * @param address  the address to connect to
	 * @param callback the callback for HTTP retrieval
	 * @param url      the url at the server
	 * @param data     the data to send
	 * @param depth    the depth (redirect recursion) of the queries
	 */
	NetworkHTTPContentConnecter(const NetworkAddress &address,
			HTTPCallback *callback, const char *url,
			const char *data = NULL, int depth = 0) :
		TCPConnecter(address),
		callback(callback),
		url(strdup(url)),
		data(data),
		depth(depth)
	{
	}

	/** Free all our allocated data. */
	~NetworkHTTPContentConnecter()
	{
		free((void*)this->url);
	}

	virtual void OnFailure()
	{
		this->callback->OnFailure();
		free((void*)this->data);
	}

	virtual void OnConnect(SOCKET s)
	{
		new NetworkHTTPSocketHandler(s, this->callback, this->address.GetHostname(), this->url, this->data, this->depth);
		/* We've relinguished control of data now. */
		this->data = NULL;
	}
};

#endif /* ENABLE_NETWORK */

#endif /* NETWORK_CORE_TCP_HTTP_H */
