/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file tcp_http.cpp Basic functions to receive and send HTTP TCP packets.
 */

#ifdef ENABLE_NETWORK

#include "../../stdafx.h"
#include "../../debug.h"
#include "../../rev.h"
#include "../network_func.h"

#include "tcp_http.h"

/** List of open HTTP connections. */
static SmallVector<NetworkHTTPSocketHandler *, 1> _http_connections;

/**
 * Start the querying
 * @param sock     the socket of this connection
 * @param callback the callback for HTTP retrieval
 * @param url      the url at the server
 * @param data     the data to send
 * @param depth    the depth (redirect recursion) of the queries
 */
NetworkHTTPSocketHandler::NetworkHTTPSocketHandler(SOCKET s,
		HTTPCallback *callback, const char *host, const char *url,
		const char *data, int depth) :
	NetworkSocketHandler(),
	recv_pos(0),
	recv_length(0),
	callback(callback),
	data(data),
	redirect_depth(depth),
	sock(s)
{
	size_t bufferSize = strlen(url) + strlen(host) + strlen(_openttd_revision) + (data == NULL ? 0 : strlen(data)) + 128;
	char *buffer = AllocaM(char, bufferSize);

	DEBUG(net, 7, "[tcp/http] requesting %s%s", host, url);
	if (data != NULL) {
		seprintf(buffer, buffer + bufferSize - 1, "POST %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: OpenTTD/%s\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n%s\r\n", url, host, _openttd_revision, (int)strlen(data), data);
	} else {
		seprintf(buffer, buffer + bufferSize - 1, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: OpenTTD/%s\r\n\r\n", url, host, _openttd_revision);
	}

	ssize_t size = strlen(buffer);
	ssize_t res = send(this->sock, (const char*)buffer, size, 0);
	if (res != size) {
		/* Sending all data failed. Socket can't handle this little bit
		 * of information? Just fall back to the old system! */
		this->callback->OnFailure();
		delete this;
		return;
	}

	*_http_connections.Append() = this;
}

/** Free whatever needs to be freed. */
NetworkHTTPSocketHandler::~NetworkHTTPSocketHandler()
{
	this->CloseConnection();

	if (this->sock != INVALID_SOCKET) closesocket(this->sock);
	this->sock = INVALID_SOCKET;
	free((void*)this->data);
}

NetworkRecvStatus NetworkHTTPSocketHandler::CloseConnection(bool error)
{
	NetworkSocketHandler::CloseConnection(error);
	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Helper to simplify the error handling.
 * @param msg the error message to show.
 */
#define return_error(msg) { DEBUG(net, 0, msg); return -1; }

static const char * const NEWLINE        = "\r\n";             ///< End of line marker
static const char * const END_OF_HEADER  = "\r\n\r\n";         ///< End of header marker
static const char * const HTTP_1_0       = "HTTP/1.0 ";        ///< Preamble for HTTP 1.0 servers
static const char * const HTTP_1_1       = "HTTP/1.1 ";        ///< Preamble for HTTP 1.1 servers
static const char * const CONTENT_LENGTH = "Content-Length: "; ///< Header for the length of the content
static const char * const LOCATION       = "Location: ";       ///< Header for location

/**
 * Handle the header of a HTTP reply.
 * @return amount of data to continue downloading.
 *         > 0: we need to download N bytes.
 *         = 0: we're being redirected.
 *         < 0: an error occurred. Downloading failed.
 * @note if an error occurred the header might not be in its
 *       original state. No effort is undertaken to bring
 *       the header in its original state.
 */
int NetworkHTTPSocketHandler::HandleHeader()
{
	assert(strlen(HTTP_1_0) == strlen(HTTP_1_1));
	assert(strstr(this->recv_buffer, END_OF_HEADER) != NULL);

	/* We expect a HTTP/1.[01] reply */
	if (strncmp(this->recv_buffer, HTTP_1_0, strlen(HTTP_1_0)) != 0 &&
			strncmp(this->recv_buffer, HTTP_1_1, strlen(HTTP_1_1)) != 0) {
		return_error("[tcp/http] received invalid HTTP reply");
	}

	char *status = this->recv_buffer + strlen(HTTP_1_0);
	if (strncmp(status, "200", 3) == 0) {
		/* We are going to receive a document. */

		/* Get the length of the document to receive */
		char *length = strcasestr(this->recv_buffer, CONTENT_LENGTH);
		if (length == NULL) return_error("[tcp/http] missing 'content-length' header");

		/* Skip the header */
		length += strlen(CONTENT_LENGTH);

		/* Search the end of the line. This is safe because the header will
		 * always end with two newlines. */
		char *end_of_line = strstr(length, NEWLINE);

		/* Read the length */
		*end_of_line = '\0';
		int len = atoi(length);
		/* Restore the header. */
		*end_of_line = '\r';

		/* Make sure we're going to download at least something;
		 * zero sized files are, for OpenTTD's purposes, always
		 * wrong. You can't have gzips of 0 bytes! */
		if (len == 0) return_error("[tcp/http] refusing to download 0 bytes");

		DEBUG(net, 7, "[tcp/http] downloading %i bytes", len);
		return len;
	}

	if (strncmp(status, "301", 3) != 0 &&
			strncmp(status, "302", 3) != 0 &&
			strncmp(status, "303", 3) != 0 &&
			strncmp(status, "307", 3) != 0) {
		/* We are not going to be redirected :(. */

		/* Search the end of the line. This is safe because the header will
		 * always end with two newlines. */
		*strstr(status, NEWLINE) = '\0';
		DEBUG(net, 0, "[tcp/http] unhandled status reply %s", status);
		return -1;
	}

	if (this->redirect_depth == 5) return_error("[tcp/http] too many redirects, looping redirects?");

	/* Redirect to other URL */
	char *uri = strcasestr(this->recv_buffer, LOCATION);
	if (uri == NULL) return_error("[tcp/http] missing 'location' header for redirect");

	uri += strlen(LOCATION);

	/* Search the end of the line. This is safe because the header will
	 * always end with two newlines. */
	char *end_of_line = strstr(uri, NEWLINE);
	*end_of_line = '\0';

	DEBUG(net, 6, "[tcp/http] redirecting to %s", uri);

	int ret = NetworkHTTPSocketHandler::Connect(uri, this->callback, this->data, this->redirect_depth + 1);
	if (ret != 0) return ret;

	/* We've relinguished control of data now. */
	this->data = NULL;

	/* Restore the header. */
	*end_of_line = '\r';
	return 0;
}

/**
 * Connect to the given URI.
 * @param uri      the URI to connect to.
 * @param callback the callback to send data back on.
 * @param data     the data we want to send (as POST).
 * @param depth    the recursion/redirect depth.
 */
/* static */ int NetworkHTTPSocketHandler::Connect(char *uri, HTTPCallback *callback, const char *data, int depth)
{
	char *hname = strstr(uri, "://");
	if (hname == NULL) return_error("[tcp/http] invalid location");

	hname += 3;

	char *url = strchr(hname, '/');
	if (url == NULL) return_error("[tcp/http] invalid location");

	*url = '\0';

	/* Fetch the hostname, and possible port number. */
	const char *company = NULL;
	const char *port = NULL;
	ParseConnectionString(&company, &port, hname);
	if (company != NULL) return_error("[tcp/http] invalid hostname");

	NetworkAddress address(hname, port == NULL ? 80 : atoi(port));

	/* Restore the URL. */
	*url = '/';
	new NetworkHTTPContentConnecter(address, callback, url, data, depth);
	return 0;
}

#undef return_error

/**
 * Handle receiving of HTTP data.
 * @return state of the receival of HTTP data.
 *         > 0: we need more cycles for downloading
 *         = 0: we are done downloading
 *         < 0: we have hit an error
 */
int NetworkHTTPSocketHandler::Receive()
{
	for (;;) {
		ssize_t res = recv(this->sock, (char *)this->recv_buffer + this->recv_pos, lengthof(this->recv_buffer) - this->recv_pos, 0);
		if (res == -1) {
			int err = GET_LAST_ERROR();
			if (err != EWOULDBLOCK) {
				/* Something went wrong... (104 is connection reset by peer) */
				if (err != 104) DEBUG(net, 0, "recv failed with error %d", err);
				return -1;
			}
			/* Connection would block, so stop for now */
			return 1;
		}

		/* No more data... did we get everything we wanted? */
		if (res == 0) {
			if (this->recv_length != 0) return -1;

			this->callback->OnReceiveData(NULL, 0);
			return 0;
		}

		/* Wait till we read the end-of-header identifier */
		if (this->recv_length == 0) {
			int read = this->recv_pos + res;
			int end = min(read, lengthof(this->recv_buffer) - 1);

			/* Do a 'safe' search for the end of the header. */
			char prev = this->recv_buffer[end];
			this->recv_buffer[end] = '\0';
			char *end_of_header = strstr(this->recv_buffer, END_OF_HEADER);
			this->recv_buffer[end] = prev;

			if (end_of_header == NULL) {
				if (read == lengthof(this->recv_buffer)) {
					DEBUG(net, 0, "[tcp/http] header too big");
					return -1;
				}
				this->recv_pos = read;
			} else {
				int ret = this->HandleHeader();
				if (ret <= 0) return ret;

				this->recv_length = ret;

				end_of_header += strlen(END_OF_HEADER);
				int len = min(read - (end_of_header - this->recv_buffer), res);
				if (len != 0) {
					this->callback->OnReceiveData(end_of_header, len);
					this->recv_length -= len;
				}

				this->recv_pos = 0;
			}
		} else {
			res = min(this->recv_length, res);
			/* Receive whatever we're expecting. */
			this->callback->OnReceiveData(this->recv_buffer, res);
			this->recv_length -= res;
		}
	}
}

/**
 * Do the receiving for all HTTP connections.
 */
/* static */ void NetworkHTTPSocketHandler::HTTPReceive()
{
	/* No connections, just bail out. */
	if (_http_connections.Length() == 0) return;

	fd_set read_fd;
	struct timeval tv;

	FD_ZERO(&read_fd);
	for (NetworkHTTPSocketHandler **iter = _http_connections.Begin(); iter < _http_connections.End(); iter++) {
		FD_SET((*iter)->sock, &read_fd);
	}

	tv.tv_sec = tv.tv_usec = 0; // don't block at all.
#if !defined(__MORPHOS__) && !defined(__AMIGA__)
	int n = select(FD_SETSIZE, &read_fd, NULL, NULL, &tv);
#else
	int n = WaitSelect(FD_SETSIZE, &read_fd, NULL, NULL, &tv, NULL);
#endif
	if (n == -1) return;

	for (NetworkHTTPSocketHandler **iter = _http_connections.Begin(); iter < _http_connections.End(); /* nothing */) {
		NetworkHTTPSocketHandler *cur = *iter;

		if (FD_ISSET(cur->sock, &read_fd)) {
			int ret = cur->Receive();
			/* First send the failure. */
			if (ret < 0) cur->callback->OnFailure();
			if (ret <= 0) {
				/* Then... the connection can be closed */
				cur->CloseConnection();
				_http_connections.Erase(iter);
				delete cur;
				continue;
			}
		}
		iter++;
	}
}

#endif /* ENABLE_NETWORK */
