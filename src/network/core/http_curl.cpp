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
#include "../../fileio_func.h"
#include "../../rev.h"
#include "../../thread.h"
#include "../network_internal.h"

#include "http.h"

#include <atomic>
#include <condition_variable>
#include <curl/curl.h>
#include <mutex>
#include <queue>

#include "../../safeguards.h"

#if defined(UNIX)
/** List of certificate bundles, depending on OS. Taken from: https://go.dev/src/crypto/x509/root_linux.go. */
static auto _certificate_files = {
	"/etc/ssl/certs/ca-certificates.crt",                // Debian/Ubuntu/Gentoo etc.
	"/etc/pki/tls/certs/ca-bundle.crt",                  // Fedora/RHEL 6
	"/etc/ssl/ca-bundle.pem",                            // OpenSUSE
	"/etc/pki/tls/cacert.pem",                           // OpenELEC
	"/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem", // CentOS/RHEL 7
	"/etc/ssl/cert.pem",                                 // Alpine Linux
};
/** List of certificate directories, depending on OS. Taken from: https://go.dev/src/crypto/x509/root_linux.go. */
static auto _certificate_directories = {
	"/etc/ssl/certs",                                    // SLES10/SLES11, https://golang.org/issue/12139
	"/etc/pki/tls/certs",                                // Fedora/RHEL
	"/system/etc/security/cacerts",                      // Android
};
#endif /* UNIX */

/** Single HTTP request. */
class NetworkHTTPRequest {
public:
	/**
	 * Create a new HTTP request.
	 *
	 * @param uri      the URI to connect to (https://.../..).
	 * @param callback the callback to send data back on.
	 * @param data     the data we want to send. When non-empty, this will be a POST request, otherwise a GET request.
	 */
	NetworkHTTPRequest(const std::string &uri, HTTPCallback *callback, const std::string &data) :
		uri(uri),
		callback(callback),
		data(data)
	{
	}

	const std::string uri;        ///< URI to connect to.
	HTTPCallback *callback;       ///< Callback to send data back on.
	const std::string data;       ///< Data to send, if any.
};

static std::thread _http_thread;
static std::atomic<bool> _http_thread_exit = false;
static std::queue<std::unique_ptr<NetworkHTTPRequest>> _http_requests;
static std::mutex _http_mutex;
static std::condition_variable _http_cv;
#if defined(UNIX)
static std::string _http_ca_file = "";
static std::string _http_ca_path = "";
#endif /* UNIX */

/* static */ void NetworkHTTPSocketHandler::Connect(const std::string &uri, HTTPCallback *callback, const std::string data)
{
#if defined(UNIX)
	if (_http_ca_file.empty() && _http_ca_path.empty()) {
		callback->OnFailure();
		return;
	}
#endif /* UNIX */

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
		curl_slist *headers = nullptr;

		if (_debug_net_level >= 5) {
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		}

		/* Setup some default options. */
		std::string user_agent = fmt::format("OpenTTD/{}", GetNetworkRevisionString());
		curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);

		/* Ensure we validate the certificate and hostname of the server. */
#if defined(UNIX)
		curl_easy_setopt(curl, CURLOPT_CAINFO, _http_ca_file.empty() ? nullptr : _http_ca_file.c_str());
		curl_easy_setopt(curl, CURLOPT_CAPATH, _http_ca_path.empty() ? nullptr : _http_ca_path.c_str());
#endif /* UNIX */
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, true);

		/* Give the connection about 10 seconds to complete. */
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

		/* Set a buffer of 100KiB, as the default of 16KiB seems a bit small. */
		curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 100L * 1024L);

		/* Fail our call if we don't receive a 2XX return value. */
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

		/* Prepare POST body and URI. */
		if (!request->data.empty()) {
			/* When the payload starts with a '{', it is a JSON payload. */
			if (StrStartsWith(request->data, "{")) {
				headers = curl_slist_append(headers, "Content-Type: application/json");
			} else {
				headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
			}

			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->data.c_str());
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
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
			const HTTPCallback *callback = static_cast<HTTPCallback *>(userdata);
			return (callback->IsCancelled() || _http_thread_exit) ? 1 : 0;
		});
		curl_easy_setopt(curl, CURLOPT_XFERINFODATA, request->callback);

		/* Perform the request. */
		CURLcode res = curl_easy_perform(curl);

		curl_slist_free_all(headers);

		if (res == CURLE_OK) {
			Debug(net, 1, "HTTP request succeeded");
			request->callback->OnReceiveData(nullptr, 0);
		} else {
			long status_code = 0;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

			/* No need to be verbose about rate limiting. */
			Debug(net, (request->callback->IsCancelled() || _http_thread_exit || status_code == HTTP_429_TOO_MANY_REQUESTS) ? 1 : 0, "HTTP request failed: status_code: {}, error: {}", status_code, curl_easy_strerror(res));
			request->callback->OnFailure();
		}
	}

	curl_easy_cleanup(curl);
}

void NetworkHTTPInitialize()
{
	curl_global_init(CURL_GLOBAL_DEFAULT);

#if defined(UNIX)
	/* Depending on the Linux distro, certificates can either be in
	 * a bundle or a folder, in a wide range of different locations.
	 * Try to find what location is used by this OS. */
	for (auto &ca_file : _certificate_files) {
		if (FileExists(ca_file)) {
			_http_ca_file = ca_file;
			break;
		}
	}
	if (_http_ca_file.empty()) {
		for (auto &ca_path : _certificate_directories) {
			if (FileExists(ca_path)) {
				_http_ca_path = ca_path;
				break;
			}
		}
	}
	Debug(net, 3, "Using certificate file: {}", _http_ca_file.empty() ? "none" : _http_ca_file);
	Debug(net, 3, "Using certificate path: {}", _http_ca_path.empty() ? "none" : _http_ca_path);

	/* Tell the user why HTTPS will not be working. */
	if (_http_ca_file.empty() && _http_ca_path.empty()) {
		Debug(net, 0, "No certificate files or directories found, HTTPS will not work!");
	}
#endif /* UNIX */

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
