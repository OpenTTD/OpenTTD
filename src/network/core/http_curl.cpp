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
#include "../../thread.h"
#include "../network_internal.h"

#include "http.h"

#include <atomic>
#include <condition_variable>
#include <curl/curl.h>
#include <memory>
#include <mutex>
#include <queue>

#include "../../safeguards.h"

/** Single HTTP request. */
class NetworkHTTPRequest {
public:
	/**
	 * Create a new HTTP request.
	 *
	 * @param uri      the URI to connect to (https://.../..).
	 * @param callback the callback to send data back on.
	 * @param data     optionally, the data we want to send. When set, this will be a POST request, otherwise a GET request.
	 */
	NetworkHTTPRequest(const std::string &uri, HTTPCallback *callback, const char *data = nullptr) :
		uri(uri),
		callback(callback),
		data(data)
	{
	}

	/**
	 * Destructor of the HTTP request.
	 */
	~NetworkHTTPRequest()
	{
		free(this->data);
	}

	std::string uri;        ///< URI to connect to.
	HTTPCallback *callback; ///< Callback to send data back on.
	const char *data;       ///< Data to send, if any.
};

static std::thread _http_thread;
static std::atomic<bool> _http_thread_exit = false;
static std::queue<std::unique_ptr<NetworkHTTPRequest>> _http_requests;
static std::mutex _http_mutex;
static std::condition_variable _http_cv;

/* static */ void NetworkHTTPSocketHandler::Connect(const std::string &uri, HTTPCallback *callback, const char *data)
{
	std::lock_guard<std::mutex> lock(_http_mutex);
	_http_requests.push(std::make_unique<NetworkHTTPRequest>(uri, callback, data));
	_http_cv.notify_one();
}

/* static */ void NetworkHTTPSocketHandler::HTTPReceive()
{
}

void HttpThread()
{
	CURL *curl = curl_easy_init();
	assert(curl != nullptr);

	for (;;) {
		std::unique_lock<std::mutex> lock(_http_mutex);

		/* Wait for a new request. */
		while (_http_requests.empty() && !_http_thread_exit) {
			_http_cv.wait(lock);
		}
		if (_http_thread_exit) break;

		std::unique_ptr<NetworkHTTPRequest> request = std::move(_http_requests.front());
		_http_requests.pop();

		/* Release the lock, as we will take a while to process the request. */
		lock.unlock();

		/* Reset to default settings. */
		curl_easy_reset(curl);

		if (_debug_net_level >= 5) {
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		}

		/* Setup some default options. */
		std::string user_agent = fmt::format("OpenTTD/{}", GetNetworkRevisionString());
		curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);

		/* Give the connection about 10 seconds to complete. */
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

		/* Set a buffer of 100KiB, as the default of 16KiB seems a bit small. */
		curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 100L * 1024L);

		/* Fail our call if we don't receive a 2XX return value. */
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

		/* Prepare POST body and URI. */
		if (request->data != nullptr) {
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->data);
		}
		curl_easy_setopt(curl, CURLOPT_URL, request->uri.c_str());

		/* Setup our (C-style) callback function which we pipe back into the callback. */
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t size, size_t nmemb, void *userdata) -> size_t {
			Debug(net, 4, "HTTP callback: {} bytes", size * nmemb);
			HTTPCallback *callback = static_cast<HTTPCallback *>(userdata);
			callback->OnReceiveData(ptr, size * nmemb);
			return size * nmemb;
		});
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, request->callback);

		/* Create a callback from which we can cancel. Sadly, there is no other
		 * thread-safe way to do this. If the connection went idle, it can take
		 * up to a second before this callback is called. There is little we can
		 * do about this. */
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, +[](void *userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) -> int {
			return _http_thread_exit ? 1 : 0;
		});

		/* Perform the request. */
		CURLcode res = curl_easy_perform(curl);

		if (res == CURLE_OK) {
			Debug(net, 1, "HTTP request succeeded");
			request->callback->OnReceiveData(nullptr, 0);
		} else {
			Debug(net, 0, "HTTP request failed: {}", curl_easy_strerror(res));
			request->callback->OnFailure();
		}
	}

	curl_easy_cleanup(curl);
}

void NetworkHTTPInitialize()
{
	curl_global_init(CURL_GLOBAL_DEFAULT);

	_http_thread_exit = false;
	StartNewThread(&_http_thread, "ottd:http", &HttpThread);
}

void NetworkHTTPUninitialize()
{
	curl_global_cleanup();

	_http_thread_exit = true;

	{
		std::lock_guard<std::mutex> lock(_http_mutex);
		_http_cv.notify_one();
	}

	if (_http_thread.joinable()) {
		_http_thread.join();
	}
}
