/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file http_winhttp.cpp WinHTTP-based implementation for HTTP requests.
 */

#include "../../stdafx.h"
#include "../../debug.h"
#include "../../rev.h"
#include "../network_internal.h"

#include "http.h"
#include "http_shared.h"

#include <mutex>
#include <winhttp.h>

#include "../../safeguards.h"

static HINTERNET _winhttp_session = nullptr;

/** Single HTTP request. */
class NetworkHTTPRequest {
private:
	const std::wstring uri; ///< URI to connect to.
	HTTPThreadSafeCallback callback; ///< Callback to send data back on.
	const std::string data; ///< Data to send, if any.

	HINTERNET connection = nullptr;      ///< Current connection object.
	HINTERNET request = nullptr;         ///< Current request object.
	std::atomic<bool> finished = false;  ///< Whether we are finished with the request.
	int depth = 0;                       ///< Current redirect depth we are in.

public:
	NetworkHTTPRequest(const std::wstring &uri, HTTPCallback *callback, const std::string &data);

	~NetworkHTTPRequest();

	void Connect();
	bool Receive();
	void WinHttpCallback(DWORD code, void *info, DWORD length);
};

static std::vector<NetworkHTTPRequest *> _http_requests;
static std::vector<NetworkHTTPRequest *> _new_http_requests;
static std::mutex _new_http_requests_mutex;

static std::vector<HTTPThreadSafeCallback *> _http_callbacks;
static std::vector<HTTPThreadSafeCallback *> _new_http_callbacks;
static std::mutex _http_callback_mutex;
static std::mutex _new_http_callback_mutex;

/**
 * Create a new HTTP request.
 *
 * @param uri      the URI to connect to (https://.../..).
 * @param callback the callback to send data back on.
 * @param data     the data we want to send. When non-empty, this will be a POST request, otherwise a GET request.
 */
NetworkHTTPRequest::NetworkHTTPRequest(const std::wstring &uri, HTTPCallback *callback, const std::string &data) :
	uri(uri),
	callback(callback),
	data(data)
{
	std::lock_guard<std::mutex> lock(_new_http_callback_mutex);
	_new_http_callbacks.push_back(&this->callback);
}

static std::string GetLastErrorAsString()
{
	char buffer[512];
	DWORD error_code = GetLastError();

	if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS, GetModuleHandleA("winhttp.dll"), error_code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, sizeof(buffer), nullptr) == 0) {
		return fmt::format("unknown error {}", error_code);
	}

	return buffer;
}

/**
 * Callback from the WinHTTP library, called when-ever something changes about the HTTP request status.
 *
 * The callback needs to call some WinHttp functions for certain states, so WinHttp continues
 * to read the request. This also allows us to abort when things go wrong, by simply not calling
 * those functions.
 * Comments with "Next step:" mark where WinHttp needs a call to continue.
 *
 * @param code    The code of the event.
 * @param info    The information about the event.
 * @param length  The length of the information.
 */
void NetworkHTTPRequest::WinHttpCallback(DWORD code, void *info, DWORD length)
{
	if (this->finished) return;

	switch (code) {
		case WINHTTP_CALLBACK_STATUS_RESOLVING_NAME:
		case WINHTTP_CALLBACK_STATUS_NAME_RESOLVED:
		case WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER:
		case WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER:
		case WINHTTP_CALLBACK_STATUS_SENDING_REQUEST:
		case WINHTTP_CALLBACK_STATUS_REQUEST_SENT:
		case WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE:
		case WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED:
		case WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION:
		case WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED:
		case WINHTTP_CALLBACK_STATUS_HANDLE_CREATED:
		case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
			/* We don't care about these events, and explicitly ignore them. */
			break;

		case WINHTTP_CALLBACK_STATUS_REDIRECT:
			/* Make sure we are not in a redirect loop. */
			if (this->depth++ > 5) {
				Debug(net, 0, "HTTP request failed: too many redirects");
				this->finished = true;
				this->callback.OnFailure();
				return;
			}
			break;

		case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
			/* Next step: read response. */
			WinHttpReceiveResponse(this->request, nullptr);
			break;

		case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
		{
			/* Retrieve the status code. */
			DWORD status_code = 0;
			DWORD status_code_size = sizeof(status_code);
			WinHttpQueryHeaders(this->request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_code_size, WINHTTP_NO_HEADER_INDEX);
			Debug(net, 3, "HTTP request status code: {}", status_code);

			/* If there is any error, we simply abort the request. */
			if (status_code >= 400) {
				/* No need to be verbose about rate limiting. */
				Debug(net, status_code == HTTP_429_TOO_MANY_REQUESTS ? 1 : 0, "HTTP request failed: status-code {}", status_code);
				this->finished = true;
				this->callback.OnFailure();
				return;
			}

			/* Next step: query for any data. */
			WinHttpQueryDataAvailable(this->request, nullptr);
		} break;

		case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
		{
			/* Retrieve the amount of data available to process. */
			DWORD size = *(DWORD *)info;

			/* Next step: read the data in a temporary allocated buffer.
			 * The buffer will be free'd by OnReceiveData() in the next step. */
			char *buffer = size == 0 ? nullptr : new char[size];
			WinHttpReadData(this->request, buffer, size, 0);
		} break;

		case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
			Debug(net, 4, "HTTP callback: {} bytes", length);

			this->callback.OnReceiveData(std::unique_ptr<char[]>(static_cast<char *>(info)), length);

			if (length == 0) {
				/* Next step: no more data available: request is finished. */
				this->finished = true;
				Debug(net, 1, "HTTP request succeeded");
			} else {
				/* Next step: query for more data. */
				WinHttpQueryDataAvailable(this->request, nullptr);
			}

			break;

		case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE:
		case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
			Debug(net, 0, "HTTP request failed: {}", GetLastErrorAsString());
			this->finished = true;
			this->callback.OnFailure();
			break;

		default:
			Debug(net, 0, "HTTP request failed: unexepected callback code 0x{:x}", code);
			this->finished = true;
			this->callback.OnFailure();
			return;
	}
}

static void CALLBACK StaticWinHttpCallback(HINTERNET, DWORD_PTR context, DWORD code, void *info, DWORD length)
{
	if (context == 0) return;

	NetworkHTTPRequest *request = (NetworkHTTPRequest *)context;
	request->WinHttpCallback(code, info, length);
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
	Debug(net, 1, "HTTP request to {}", std::string(uri.begin(), uri.end()));

	URL_COMPONENTS url_components = {};
	wchar_t scheme[32];
	wchar_t hostname[128];
	wchar_t url_path[4096];

	/* Convert the URL to its components. */
	url_components.dwStructSize = sizeof(url_components);
	url_components.lpszScheme = scheme;
	url_components.dwSchemeLength = lengthof(scheme);
	url_components.lpszHostName = hostname;
	url_components.dwHostNameLength = lengthof(hostname);
	url_components.lpszUrlPath = url_path;
	url_components.dwUrlPathLength = lengthof(url_path);
	WinHttpCrackUrl(this->uri.c_str(), 0, 0, &url_components);

	/* Create the HTTP connection. */
	this->connection = WinHttpConnect(_winhttp_session, url_components.lpszHostName, url_components.nPort, 0);
	if (this->connection == nullptr) {
		Debug(net, 0, "HTTP request failed: {}", GetLastErrorAsString());
		this->finished = true;
		this->callback.OnFailure();
		return;
	}

	this->request = WinHttpOpenRequest(connection, data.empty() ? L"GET" : L"POST", url_components.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, url_components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
	if (this->request == nullptr) {
		WinHttpCloseHandle(this->connection);

		Debug(net, 0, "HTTP request failed: {}", GetLastErrorAsString());
		this->finished = true;
		this->callback.OnFailure();
		return;
	}

	/* Send the request (possibly with a payload). */
	if (data.empty()) {
		WinHttpSendRequest(this->request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, reinterpret_cast<DWORD_PTR>(this));
	} else {
		/* When the payload starts with a '{', it is a JSON payload. */
		LPCWSTR content_type = StrStartsWith(data, "{") ? L"Content-Type: application/json\r\n" : L"Content-Type: application/x-www-form-urlencoded\r\n";
		WinHttpSendRequest(this->request, content_type, -1, const_cast<char *>(data.c_str()), static_cast<DWORD>(data.size()), static_cast<DWORD>(data.size()), reinterpret_cast<DWORD_PTR>(this));
	}
}

/**
 * Poll and process the HTTP request/response.
 *
 * @return True iff the request is done; no call to Receive() should be done after it returns true.
 */
bool NetworkHTTPRequest::Receive()
{
	if (this->callback.cancelled && !this->finished) {
		Debug(net, 1, "HTTP request failed: cancelled by user");
		this->finished = true;
		this->callback.OnFailure();
		/* Fall-through, as we are waiting for IsQueueEmpty() to happen. */
	}

	return this->finished && this->callback.IsQueueEmpty();
}

/**
 * Destructor of the HTTP request.
 *
 * Makes sure all handlers are closed, and all memory is free'd.
 */
NetworkHTTPRequest::~NetworkHTTPRequest()
{
	if (this->request) {
		WinHttpCloseHandle(this->request);
		WinHttpCloseHandle(this->connection);
	}

	std::lock_guard<std::mutex> lock(_http_callback_mutex);
	_http_callbacks.erase(std::remove(_http_callbacks.begin(), _http_callbacks.end(), &this->callback), _http_callbacks.end());
}

/* static */ void NetworkHTTPSocketHandler::Connect(const std::string &uri, HTTPCallback *callback, const std::string data)
{
	auto request = new NetworkHTTPRequest(std::wstring(uri.begin(), uri.end()), callback, data);
	request->Connect();

	std::lock_guard<std::mutex> lock(_new_http_requests_mutex);
	_new_http_requests.push_back(request);
}

/* static */ void NetworkHTTPSocketHandler::HTTPReceive()
{
	/* Process all callbacks. */
	{
		std::lock_guard<std::mutex> lock(_http_callback_mutex);

		{
			std::lock_guard<std::mutex> lock(_new_http_callback_mutex);
			if (!_new_http_callbacks.empty()) {
				/* We delay adding new callbacks, as HandleQueue() below might add a new callback. */
				_http_callbacks.insert(_http_callbacks.end(), _new_http_callbacks.begin(), _new_http_callbacks.end());
				_new_http_callbacks.clear();
			}
		}

		for (auto &callback : _http_callbacks) {
			callback->HandleQueue();
		}
	}

	/* Process all requests. */
	{
		std::lock_guard<std::mutex> lock(_new_http_requests_mutex);
		if (!_new_http_requests.empty()) {
			/* We delay adding new requests, as Receive() below can cause a callback which adds a new requests. */
			_http_requests.insert(_http_requests.end(), _new_http_requests.begin(), _new_http_requests.end());
			_new_http_requests.clear();
		}
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
	/* We create a single session, from which we build up every other request. */
	std::string user_agent = fmt::format("OpenTTD/{}", GetNetworkRevisionString());
	_winhttp_session = WinHttpOpen(std::wstring(user_agent.begin(), user_agent.end()).c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);

	/* Set the callback function for all requests. The "context" maps it back into the actual request instance. */
	WinHttpSetStatusCallback(_winhttp_session, StaticWinHttpCallback, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0);

	/* 10 seconds timeout for requests. */
	WinHttpSetTimeouts(_winhttp_session, 10000, 10000, 10000, 10000);
}

void NetworkHTTPUninitialize()
{
	WinHttpCloseHandle(_winhttp_session);
}
