/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file http_curl.cpp CURL-based implementation for HTTP requests.
 */

#include "../../stdafx.h"
#include "../../debug.h"
#include "../../rev.h"
#include "../network_internal.h"

#include "http.h"

#include <curl/curl.h>

#include "../../safeguards.h"

/** Single HTTP request. */
class NetworkHTTPRequest {
private:
	std::string uri;        ///< URI to connect to.
	HTTPCallback *callback; ///< Callback to send data back on.
	const char *data;       ///< Data to send, if any.

	CURL *curl = nullptr;          ///< CURL handle.
	CURLM *multi_handle = nullptr; ///< CURL multi-handle.

public:
	NetworkHTTPRequest(const std::string &uri, HTTPCallback *callback, const char *data = nullptr);

	~NetworkHTTPRequest();

	void Connect();
	bool Receive();
};

static std::vector<NetworkHTTPRequest *> _http_requests;
static std::vector<NetworkHTTPRequest *> _new_http_requests;
static CURLSH *_curl_share = nullptr;

/**
 * Create a new HTTP request.
 *
 * @param uri      the URI to connect to (https://.../..).
 * @param callback the callback to send data back on.
 * @param data     optionally, the data we want to send. When set, this will be a POST request, otherwise a GET request.
 */
NetworkHTTPRequest::NetworkHTTPRequest(const std::string &uri, HTTPCallback *callback, const char *data) :
	uri(uri),
	callback(callback),
	data(data)
{
}

/**
 * Start the HTTP request handling.
 *
 * This is done in an async manner, so we can do other things while waiting for
 * the HTTP request to finish. The actual receiving of the data is done in
 * Receive().
 */
void NetworkHTTPRequest::Connect()
{
	Debug(net, 1, "HTTP request to {}", uri);

	this->curl = curl_easy_init();
	assert(this->curl != nullptr);

	if (_debug_net_level >= 5) {
		curl_easy_setopt(this->curl, CURLOPT_VERBOSE, 1L);
	}

	curl_easy_setopt(curl, CURLOPT_SHARE, _curl_share);

	if (this->data != nullptr) {
		curl_easy_setopt(this->curl, CURLOPT_POST, 1L);
		curl_easy_setopt(this->curl, CURLOPT_POSTFIELDS, this->data);
	}

	curl_easy_setopt(this->curl, CURLOPT_URL, this->uri.c_str());

	/* Setup our (C-style) callback function which we pipe back into the callback. */
	curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t size, size_t nmemb, void *userdata) -> size_t {
		Debug(net, 4, "HTTP callback: {} bytes", size * nmemb);
		HTTPCallback *callback = static_cast<HTTPCallback *>(userdata);
		callback->OnReceiveData(ptr, size * nmemb);
		return size * nmemb;
	});
	curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, this->callback);

	/* Setup some default options. */
	std::string user_agent = fmt::format("OpenTTD/{}", GetNetworkRevisionString());
	curl_easy_setopt(this->curl, CURLOPT_USERAGENT, user_agent.c_str());
	curl_easy_setopt(this->curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(this->curl, CURLOPT_MAXREDIRS, 5L);

	/* Give the connection about 10 seconds to complete. */
	curl_easy_setopt(this->curl, CURLOPT_CONNECTTIMEOUT, 10L);

	/* Set a buffer of 100KiB, as the default of 16KiB seems a bit small. */
	curl_easy_setopt(this->curl, CURLOPT_BUFFERSIZE, 100L * 1024L);

	/* Fail our call if we don't receive a 2XX return value. */
	curl_easy_setopt(this->curl, CURLOPT_FAILONERROR, 1L);

	/* Create a multi-handle so we can do the call async. */
	this->multi_handle = curl_multi_init();
	curl_multi_add_handle(this->multi_handle, this->curl);

	/* Trigger it for the first time so it becomes active. */
	int still_running;
	curl_multi_perform(this->multi_handle, &still_running);
}

/**
 * Poll and process the HTTP request/response.
 *
 * @return True iff the request is done; no call to Receive() should be done after it returns true.
 */
bool NetworkHTTPRequest::Receive()
{
	int still_running = 0;

	/* Check for as long as there is activity on the socket, but once in a while return.
	 * This allows the GUI to always update, even on really fast downloads. */
	for (int count = 0; count < 100; count++) {
		/* Check if there was activity in the multi-handle. */
		int numfds;
		curl_multi_wait(this->multi_handle, NULL, 0, 0, &numfds);
		if (numfds == 0) return false;

		/* Let CURL process the activity. */
		curl_multi_perform(this->multi_handle, &still_running);
		if (still_running == 0) break;
	}

	/* The download is still pending (so the count is reached). Update GUI. */
	if (still_running != 0) return false;

	/* The request is done; check the result and close up. */
	int msgq;
	CURLMsg *msg = curl_multi_info_read(this->multi_handle, &msgq);
	/* We can safely assume this returns something, as otherwise the multi-handle wouldn't be empty. */
	assert(msg != nullptr);
	assert(msg->msg == CURLMSG_DONE);

	CURLcode res = msg->data.result;
	if (res == CURLE_OK) {
		Debug(net, 1, "HTTP request succeeded");
		this->callback->OnReceiveData(nullptr, 0);
	} else {
		Debug(net, 0, "HTTP request failed: {}", curl_easy_strerror(res));
		this->callback->OnFailure();
	}

	return true;
}

/**
 * Destructor of the HTTP request.
 *
 * Makes sure all handlers are closed, and all memory is free'd.
 */
NetworkHTTPRequest::~NetworkHTTPRequest()
{
	if (this->curl) {
		curl_multi_remove_handle(this->multi_handle, this->curl);
		curl_multi_cleanup(this->multi_handle);

		curl_easy_cleanup(this->curl);
	}

	free(this->data);
}

/* static */ void NetworkHTTPSocketHandler::Connect(const std::string &uri, HTTPCallback *callback, const char *data)
{
	auto request = new NetworkHTTPRequest(uri, callback, data);
	request->Connect();
	_new_http_requests.push_back(request);
}

/* static */ void NetworkHTTPSocketHandler::HTTPReceive()
{
	if (!_new_http_requests.empty()) {
		/* We delay adding new requests, as Receive() below can cause a callback which adds a new requests. */
		_http_requests.insert(_http_requests.end(), _new_http_requests.begin(), _new_http_requests.end());
		_new_http_requests.clear();
	}

	if (_http_requests.empty()) return;

	for (auto it = _http_requests.begin(); it != _http_requests.end(); /* nothing */) {
		NetworkHTTPRequest *cur = *it;

		if (cur->Receive()) {
			it = _http_requests.erase(it);
			delete cur;
			continue;
		}

		++it;
	}
}

void NetworkHTTPInitialize()
{
	curl_global_init(CURL_GLOBAL_DEFAULT);

	/* Create a share that tracks DNS, SSL session, and connections. As this
	 * always runs in the same thread, sharing a connection should be fine. */
	_curl_share = curl_share_init();
	curl_share_setopt(_curl_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
	curl_share_setopt(_curl_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
	curl_share_setopt(_curl_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
}

void NetworkHTTPUninitialize()
{
	curl_share_cleanup(_curl_share);
	curl_global_cleanup();
}
