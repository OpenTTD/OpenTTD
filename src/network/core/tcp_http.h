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

/** Callback for when the HTTP handler has something to tell us. */
struct HTTPCallback {
	/**
	 * An error has occurred and the connection has been closed.
	 * @note HTTP socket handler is closed/freed.
	 */
	virtual void OnFailure() = 0;

	/**
	 * We're receiving data.
	 * @param data   the received data, nullptr when all data has been received.
	 * @param length the amount of received data, 0 when all data has been received.
	 * @note When nullptr is sent the HTTP socket handler is closed/freed.
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

	void CloseSocket();

	NetworkHTTPSocketHandler(SOCKET sock, HTTPCallback *callback,
			const std::string &host, const char *url, const char *data, int depth);

	~NetworkHTTPSocketHandler();

	static int Connect(char *uri, HTTPCallback *callback,
			const char *data = nullptr, int depth = 0);

	static void HTTPReceive();
};

/** Connect with a HTTP server and do ONE query. */
class NetworkHTTPContentConnecter : TCPConnecter {
	std::string hostname;   ///< Hostname we are connecting to.
	HTTPCallback *callback; ///< Callback to tell that we received some data (or won't).
	const char *url;        ///< The URL we want to get at the server.
	const char *data;       ///< The data to send
	int depth;              ///< How far we have recursed

public:
	/**
	 * Start the connecting.
	 * @param hostname The hostname to connect to.
	 * @param callback The callback for HTTP retrieval.
	 * @param url The url at the server.
	 * @param data The data to send.
	 * @param depth The depth (redirect recursion) of the queries.
	 */
	NetworkHTTPContentConnecter(const std::string &hostname, HTTPCallback *callback, const char *url, const char *data = nullptr, int depth = 0) :
		TCPConnecter(hostname, 80),
		hostname(hostname),
		callback(callback),
		url(stredup(url)),
		data(data),
		depth(depth)
	{
	}

	/** Free all our allocated data. */
	~NetworkHTTPContentConnecter()
	{
		free(this->url);
	}

	void OnFailure() override
	{
		this->callback->OnFailure();
		free(this->data);
	}

	void OnConnect(SOCKET s) override
	{
		new NetworkHTTPSocketHandler(s, this->callback, this->hostname, this->url, this->data, this->depth);
		/* We've relinquished control of data now. */
		this->data = nullptr;
	}
};

#endif /* NETWORK_CORE_TCP_HTTP_H */
