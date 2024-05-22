/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file http.h Basic functions to send and receive HTTP packets.
 */

#ifndef NETWORK_CORE_HTTP_H
#define NETWORK_CORE_HTTP_H

#include "tcp.h"

constexpr int HTTP_429_TOO_MANY_REQUESTS = 429;

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
	virtual void OnReceiveData(std::unique_ptr<char[]> data, size_t length) = 0;

	/**
	 * Check if there is a request to cancel the transfer.
	 *
	 * @return true iff the connection is cancelled.
	 * @note Cancellations are never instant, and can take a bit of time to be processed.
	 * The object needs to remain valid until the OnFailure() callback is called.
	 */
	virtual bool IsCancelled() const = 0;

	/** Silentium */
	virtual ~HTTPCallback() = default;
};

/** Base socket handler for HTTP traffic. */
class NetworkHTTPSocketHandler {
public:
	/**
	 * Connect to the given URI.
	 *
	 * @param uri      the URI to connect to (https://.../..).
	 * @param callback the callback to send data back on.
	 * @param data     the data we want to send. When non-empty, this will be a POST request, otherwise a GET request.
	 */
	static void Connect(const std::string &uri, HTTPCallback *callback, const std::string data = "");

	/**
	 * Do the receiving for all HTTP connections.
	 */
	static void HTTPReceive();
};

/**
 * Initialize the HTTP socket handler.
 */
void NetworkHTTPInitialize();

/**
 * Uninitialize the HTTP socket handler.
 */
void NetworkHTTPUninitialize();

#endif /* NETWORK_CORE_HTTP_H */
