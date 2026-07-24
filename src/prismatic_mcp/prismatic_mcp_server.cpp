/*
 * This file is part of korczis's PRIVATE RESEARCH FORK of OpenTTD and is NOT
 * part of upstream OpenTTD. RESEARCH-ONLY / UNSTABLE / DEBUG-ONLY.
 *
 * Embedded, loopback-only MCP server (stable spec 2025-11-25, Streamable HTTP).
 * Compiled only when OTTD_PRISMATIC_MCP_SERVER is defined.
 *
 * Design (see research/prismatic-mcp/):
 *  - The listener binds 127.0.0.1 and ::1 only, never a public interface.
 *  - Every byte is serviced on the OpenTTD main thread from GameLoop()->Poll(),
 *    so request handlers touch pools/globals with no locking, exactly like the
 *    admin network. No worker thread ever dereferences game state.
 *  - Auth is a 256-bit random bearer token generated at Start(), compared in
 *    constant time. The token is never logged and never written to openttd.cfg.
 *  - This first vertical implements: initialize, notifications/initialized, ping,
 *    resources/list, resources/read (read-only, live game state). Tools, the
 *    approval workflow and mutations are staged behind this transport; see the
 *    implementation plan. It deliberately does NOT expose any mutation yet.
 */

/** @file prismatic_mcp_server.cpp Loopback MCP transport + read-only resources (research-only). */

#include "../stdafx.h"

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "prismatic_mcp.h"

#include "../3rdparty/nlohmann/json.hpp"
#include "../core/random_func.hpp"
#include "../debug.h"
#include "../rev.h"
#include "../openttd.h"
#include "../company_base.h"
#include "../vehicle_base.h"
#include "../town.h"
#include "../map_func.h"
#include "../timer/timer_game_calendar.h"
#include "../timer/timer_game_tick.h"
#include "../network/core/os_abstraction.h"
#include "../network/core/address.h"

#include <array>
#include <charconv>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../safeguards.h"

namespace PrismaticMCP {

using json = nlohmann::json;

namespace {

/* ---- HTTP/transport limits (mission §11.6, conservative defaults) ---------- */
constexpr size_t MAX_REQUEST_BODY = 1u << 20;   ///< 1 MiB request body cap.
constexpr size_t MAX_HEADER_BYTES = 32u << 10;  ///< 32 KiB header section cap.
constexpr size_t MAX_CONNECTIONS = 16;          ///< Simultaneous TCP connections.

/** One in-flight loopback HTTP connection. Raw byte buffer; no OpenTTD Packet framing. */
struct Connection {
	SOCKET sock = INVALID_SOCKET;
	std::string inbuf;             ///< Accumulated request bytes until headers+body complete.
	bool headers_done = false;
	size_t content_length = 0;
	size_t header_end = 0;         ///< Offset of body start (end of "\r\n\r\n").
};

/** Process-local server state. Never serialized into a savegame; ephemeral. */
struct ServerState {
	bool running = false;
	uint16_t port = 0;
	SocketList listeners;          ///< Loopback listening sockets (v4 + v6).
	std::vector<Connection> conns;
	std::array<uint8_t, 32> token{}; ///< 256-bit bearer token (raw bytes).
	bool token_valid = false;
	uint64_t requests_total = 0;
	uint64_t requests_denied = 0;
	std::string last_error;
};

ServerState _mcp;

/* ---- small helpers -------------------------------------------------------- */

std::string ToHex(std::span<const uint8_t> bytes)
{
	static const char *digits = "0123456789abcdef";
	std::string out;
	out.reserve(bytes.size() * 2);
	for (uint8_t b : bytes) {
		out.push_back(digits[b >> 4]);
		out.push_back(digits[b & 0xF]);
	}
	return out;
}

/** Constant-time compare of the presented token (hex) against the live token. */
bool TokenMatches(std::string_view presented_hex)
{
	if (!_mcp.token_valid) return false;
	std::string expected = ToHex(_mcp.token);
	if (presented_hex.size() != expected.size()) return false;
	unsigned diff = 0;
	for (size_t i = 0; i < expected.size(); i++) diff |= (unsigned)(expected[i] ^ presented_hex[i]);
	return diff == 0;
}

std::string_view TrimOWS(std::string_view s)
{
	while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
	while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
	return s;
}

/* ---- live game-state serializers (main thread only) ----------------------- */

/** The common response envelope (mission §15), stamped with the live snapshot. */
json MakeEnvelope()
{
	json env;
	env["schema_version"] = "1.0";
	env["source"] = "OpenTTD";
	env["source_revision"] = _openttd_revision;
	env["protocol_version"] = std::string(PROTOCOL_VERSION);
	env["game_tick"] = (uint64_t)TimerGameTick::counter;

	TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::date);
	env["game_date"] = fmt::format("{:04}-{:02}-{:02}", (int)ymd.year.base(), (int)ymd.month + 1, (int)ymd.day);

	const char *mode = "menu";
	switch (_game_mode) {
		case GameMode::Normal: mode = "normal"; break;
		case GameMode::Editor: mode = "editor"; break;
		case GameMode::Bootstrap: mode = "bootstrap"; break;
		default: mode = "menu"; break;
	}
	env["game_mode"] = mode;
	env["availability"] = "available";
	env["redactions"] = json::array();
	env["truncated"] = false;
	env["next_cursor"] = nullptr;
	return env;
}

json ResourceMcpStatus()
{
	json env = MakeEnvelope();
	json d;
	d["running"] = _mcp.running;
	d["endpoint"] = GetEndpoint();
	d["bind"] = "127.0.0.1,::1";
	d["port"] = _mcp.port;
	d["read_only"] = true;
	d["requests_total"] = _mcp.requests_total;
	d["requests_denied"] = _mcp.requests_denied;
	d["active_connections"] = (uint64_t)_mcp.conns.size();
	env["data"] = d;
	return env;
}

json ResourceMetaBuild()
{
	json env = MakeEnvelope();
	json d;
	d["openttd_version"] = _openttd_revision;
	d["revision_hash"] = std::string(_openttd_revision_hash);
	d["newgrf_version"] = _openttd_newgrf_version;
	d["research_instrumentation"] = true;
	d["mcp_server"] = true;
#ifdef DEDICATED
	d["dedicated"] = true;
#else
	d["dedicated"] = false;
#endif
	env["data"] = d;
	return env;
}

json ResourceGameState()
{
	json env = MakeEnvelope();
	json d;
	d["paused"] = _pause_mode.Any();
	d["map_width"] = Map::SizeX();
	d["map_height"] = Map::SizeY();
	d["num_companies"] = (uint64_t)Company::GetNumItems();
	d["num_vehicles"] = (uint64_t)Vehicle::GetNumItems();
	d["num_towns"] = (uint64_t)Town::GetNumItems();
	env["data"] = d;
	return env;
}

json ResourceCompanies()
{
	json env = MakeEnvelope();
	json list = json::array();
	/* Deterministic order: pools iterate by ascending index. */
	for (const Company *c : Company::Iterate()) {
		json jc;
		jc["id"] = c->index.base();
		jc["name"] = c->name;              // empty if the player never renamed it
		jc["is_ai"] = c->is_ai;
		jc["money"] = (int64_t)c->money;
		jc["money_unit"] = "internal_currency";
		jc["inaugurated_year"] = (int)c->inaugurated_year_calendar.base();
		list.push_back(std::move(jc));
	}
	env["data"] = json{{"companies", std::move(list)}};
	return env;
}

/** Static resource catalogue. Deterministic order; each entry is read-only here. */
struct ResourceEntry {
	const char *uri;
	const char *name;
	const char *description;
	json (*build)();
};
const std::array<ResourceEntry, 4> RESOURCES = {{
	{"openttd://mcp/status", "MCP server status", "Embedded MCP server runtime status", &ResourceMcpStatus},
	{"openttd://meta/build", "Build metadata", "OpenTTD version, revision and feature gates", &ResourceMetaBuild},
	{"openttd://game/state", "Game state", "Mode, pause, map size and object counts", &ResourceGameState},
	{"openttd://companies", "Companies", "All companies with basic finances (server-visible)", &ResourceCompanies},
}};

/* ---- JSON-RPC dispatch (main thread) -------------------------------------- */

json RpcError(const json &id, int code, std::string_view message)
{
	return json{{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", std::string(message)}}}};
}

json RpcResult(const json &id, json result)
{
	return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
}

/** Dispatch a single JSON-RPC request object. Returns std::nullopt for notifications. */
std::optional<json> DispatchOne(const json &req)
{
	json id = req.contains("id") ? req["id"] : json(nullptr);
	bool is_notification = !req.contains("id");

	if (!req.contains("method") || !req["method"].is_string()) {
		return is_notification ? std::nullopt : std::optional<json>(RpcError(id, -32600, "Invalid Request: missing method"));
	}
	std::string method = req["method"].get<std::string>();

	if (method == "initialize") {
		json caps = {{"resources", json::object()}, {"tools", json::object()}, {"logging", json::object()}};
		json result = {
			{"protocolVersion", std::string(PROTOCOL_VERSION)},
			{"capabilities", caps},
			{"serverInfo", {{"name", "openttd-prismatic-mcp"}, {"version", _openttd_revision}}},
			{"instructions", "RESEARCH-ONLY embedded OpenTTD MCP server. Read-only resources are live; "
			 "no game-state mutation is exposed by this build."},
		};
		return RpcResult(id, result);
	}
	if (method == "notifications/initialized") return std::nullopt; // notification, no reply
	if (method == "ping") return RpcResult(id, json::object());

	if (method == "resources/list") {
		json arr = json::array();
		for (const auto &r : RESOURCES) {
			arr.push_back({{"uri", r.uri}, {"name", r.name}, {"description", r.description}, {"mimeType", "application/json"}});
		}
		return RpcResult(id, json{{"resources", arr}});
	}
	if (method == "resources/read") {
		if (!req.contains("params") || !req["params"].contains("uri")) {
			return RpcError(id, -32602, "Invalid params: missing uri");
		}
		std::string uri = req["params"]["uri"].get<std::string>();
		for (const auto &r : RESOURCES) {
			if (uri == r.uri) {
				json contents = json::array();
				contents.push_back({{"uri", r.uri}, {"mimeType", "application/json"}, {"text", r.build().dump()}});
				return RpcResult(id, json{{"contents", contents}});
			}
		}
		return RpcError(id, -32602, "Unknown resource uri");
	}
	if (method == "tools/list") return RpcResult(id, json{{"tools", json::array()}});
	if (method == "prompts/list") return RpcResult(id, json{{"prompts", json::array()}});

	return is_notification ? std::nullopt : std::optional<json>(RpcError(id, -32601, "Method not found"));
}

/* ---- HTTP request handling ------------------------------------------------ */

/** Build a minimal HTTP/1.1 response with a JSON body and Connection: close. */
std::string HttpResponse(int status, std::string_view reason, std::string_view body, std::string_view extra_headers = "")
{
	std::string out;
	out += "HTTP/1.1 " + fmt::format("{}", status) + " " + std::string(reason) + "\r\n";
	out += "Content-Type: application/json\r\n";
	out += "Content-Length: " + fmt::format("{}", body.size()) + "\r\n";
	if (!extra_headers.empty()) out += std::string(extra_headers);
	out += "Connection: close\r\n\r\n";
	out += std::string(body);
	return out;
}

/** Parse and answer one fully-received HTTP request. Returns the raw response bytes. */
std::string HandleHttpRequest(std::string_view raw, size_t header_end, size_t content_length)
{
	_mcp.requests_total++;

	/* Request line: METHOD SP path SP HTTP/1.1 */
	size_t line_end = raw.find("\r\n");
	std::string_view request_line = raw.substr(0, line_end);
	size_t sp1 = request_line.find(' ');
	size_t sp2 = request_line.find(' ', sp1 + 1);
	if (sp1 == std::string_view::npos || sp2 == std::string_view::npos) {
		_mcp.requests_denied++;
		return HttpResponse(400, "Bad Request", R"({"error":"malformed request line"})");
	}
	std::string_view http_method = request_line.substr(0, sp1);
	std::string_view path = request_line.substr(sp1 + 1, sp2 - sp1 - 1);

	/* Header scan for Authorization + Origin. Header section is [line_end+2, header_end). */
	std::string_view headers = raw.substr(line_end + 2, header_end - (line_end + 2));
	std::string_view auth, origin;
	size_t pos = 0;
	while (pos < headers.size()) {
		size_t nl = headers.find("\r\n", pos);
		if (nl == std::string_view::npos) nl = headers.size();
		std::string_view h = headers.substr(pos, nl - pos);
		size_t colon = h.find(':');
		if (colon != std::string_view::npos) {
			std::string_view name = TrimOWS(h.substr(0, colon));
			std::string_view value = TrimOWS(h.substr(colon + 1));
			/* case-insensitive header name compare */
			auto ieq = [](std::string_view a, std::string_view b) {
				if (a.size() != b.size()) return false;
				for (size_t i = 0; i < a.size(); i++) if (tolower(a[i]) != tolower(b[i])) return false;
				return true;
			};
			if (ieq(name, "authorization")) auth = value;
			else if (ieq(name, "origin")) origin = value;
		}
		pos = nl + 2;
	}

	/* Loopback-only + Origin policy: reject any browser Origin (mission §12). */
	if (!origin.empty()) {
		_mcp.requests_denied++;
		return HttpResponse(403, "Forbidden", R"({"error":"Origin header not allowed for loopback MCP endpoint"})");
	}

	if (path != "/mcp") {
		return HttpResponse(404, "Not Found", R"({"error":"only /mcp is served"})");
	}

	/* Bearer auth, constant-time. */
	std::string_view bearer;
	if (auth.size() > 7 && (auth.substr(0, 7) == "Bearer " || auth.substr(0, 7) == "bearer ")) {
		bearer = TrimOWS(auth.substr(7));
	}
	if (!TokenMatches(bearer)) {
		_mcp.requests_denied++;
		return HttpResponse(401, "Unauthorized", R"({"error":"missing or invalid bearer token"})",
			"WWW-Authenticate: Bearer\r\n");
	}

	if (http_method == "DELETE") {
		/* Explicit session termination: this stateless build has no session table yet. */
		return HttpResponse(200, "OK", R"({"ok":true})");
	}
	if (http_method != "POST") {
		return HttpResponse(405, "Method Not Allowed", R"({"error":"POST required for JSON-RPC"})",
			"Allow: POST, DELETE\r\n");
	}

	std::string_view body = raw.substr(header_end, content_length);
	json parsed;
	try {
		parsed = json::parse(body);
	} catch (const json::exception &) {
		return HttpResponse(200, "OK", RpcError(nullptr, -32700, "Parse error").dump());
	}

	/* Reject JSON-RPC batches explicitly (mission: batch rejected). */
	if (parsed.is_array()) {
		return HttpResponse(200, "OK", RpcError(nullptr, -32600, "Batch requests are not supported").dump());
	}

	std::optional<json> reply = DispatchOne(parsed);
	if (!reply.has_value()) {
		/* Notification: 202 Accepted with empty body per Streamable HTTP. */
		return HttpResponse(202, "Accepted", "");
	}
	return HttpResponse(200, "OK", reply->dump());
}

/* ---- socket servicing (main thread, non-blocking) ------------------------- */

void CloseConnection(Connection &c)
{
	if (c.sock != INVALID_SOCKET) closesocket(c.sock);
	c.sock = INVALID_SOCKET;
}

/** Try to parse headers once enough bytes have arrived; sets content_length. */
bool TryCompleteHeaders(Connection &c)
{
	if (c.headers_done) return true;
	size_t end = c.inbuf.find("\r\n\r\n");
	if (end == std::string::npos) {
		if (c.inbuf.size() > MAX_HEADER_BYTES) { CloseConnection(c); }
		return false;
	}
	c.header_end = end + 4;
	c.headers_done = true;

	/* Find Content-Length (case-insensitive). */
	std::string lower = c.inbuf.substr(0, c.header_end);
	for (char &ch : lower) ch = (char)tolower(ch);
	size_t p = lower.find("content-length:");
	if (p != std::string::npos) {
		p += strlen("content-length:");
		while (p < lower.size() && (lower[p] == ' ' || lower[p] == '\t')) p++;
		size_t end = lower.find_first_not_of("0123456789", p);
		unsigned long long value = 0;
		std::from_chars(lower.c_str() + p, lower.c_str() + (end == std::string::npos ? lower.size() : end), value);
		c.content_length = (size_t)value;
		if (c.content_length > MAX_REQUEST_BODY) { CloseConnection(c); return false; }
	}
	return true;
}

void ServiceConnection(Connection &c)
{
	if (c.sock == INVALID_SOCKET) return;

	/* Read whatever is available (non-blocking). */
	char buf[4096];
	for (;;) {
		ssize_t n = recv(c.sock, buf, sizeof(buf), 0);
		if (n > 0) {
			c.inbuf.append(buf, (size_t)n);
			if (c.inbuf.size() > MAX_REQUEST_BODY + MAX_HEADER_BYTES) { CloseConnection(c); return; }
			continue;
		}
		if (n == 0) { CloseConnection(c); return; } // peer closed
		NetworkError err = NetworkError::GetLast();
		if (err.WouldBlock()) break;
		CloseConnection(c);
		return;
	}

	if (!TryCompleteHeaders(c)) return;
	if (c.inbuf.size() < c.header_end + c.content_length) return; // body still arriving

	std::string response = HandleHttpRequest(c.inbuf, c.header_end, c.content_length);
	/* Best-effort blocking-ish send of the whole response, then close. */
	size_t sent = 0;
	while (sent < response.size()) {
		ssize_t n = send(c.sock, response.data() + sent, response.size() - sent, 0);
		if (n > 0) { sent += (size_t)n; continue; }
		NetworkError err = NetworkError::GetLast();
		if (err.WouldBlock()) continue;
		break;
	}
	CloseConnection(c);
}

} // anonymous namespace

/* ---- public API ----------------------------------------------------------- */

void Init()
{
	/* Nothing heavy at startup; the listener is created only on explicit Start(). */
	_mcp.running = false;
}

bool Start(uint16_t port, std::string &error)
{
	if (_mcp.running) { error = "already running"; return false; }
	_mcp.port = (port == 0) ? DEFAULT_PORT : port;

	/* Bind loopback only: construct explicit v4 + v6 loopback addresses rather
	 * than using the all-interfaces default in GetBindAddresses(). */
	_mcp.listeners.clear();
	NetworkAddress v4("127.0.0.1", _mcp.port);
	v4.Listen(SOCK_STREAM, &_mcp.listeners);
	NetworkAddress v6("::1", _mcp.port);
	v6.Listen(SOCK_STREAM, &_mcp.listeners);

	if (_mcp.listeners.empty()) {
		error = "could not bind loopback listener on port " + fmt::format("{}", _mcp.port);
		_mcp.last_error = error;
		return false;
	}

	RandomBytesWithFallback(_mcp.token);
	_mcp.token_valid = true;
	_mcp.running = true;
	_mcp.requests_total = 0;
	_mcp.requests_denied = 0;
	Debug(net, 1, "[prismatic-mcp] RESEARCH-ONLY MCP server listening on {} (loopback only)", GetEndpoint());
	return true;
}

void Stop()
{
	if (!_mcp.running && _mcp.listeners.empty()) return;
	for (auto &c : _mcp.conns) CloseConnection(c);
	_mcp.conns.clear();
	for (auto &pair : _mcp.listeners) closesocket(pair.first);
	_mcp.listeners.clear();
	_mcp.token.fill(0);
	_mcp.token_valid = false;
	_mcp.running = false;
	Debug(net, 1, "[prismatic-mcp] MCP server stopped");
}

void Shutdown()
{
	Stop();
}

void OnGameModeChanged()
{
	/* Snapshots are built fresh per request in this vertical, so nothing to
	 * invalidate yet; hook kept so lifecycle wiring is in place. */
}

void Poll()
{
	if (!_mcp.running || _mcp.listeners.empty()) return;

	fd_set read_fds;
	FD_ZERO(&read_fds);
	SOCKET max_fd = 0;
	for (auto &pair : _mcp.listeners) {
		FD_SET(pair.first, &read_fds);
		if (pair.first > max_fd) max_fd = pair.first;
	}
	for (auto &c : _mcp.conns) {
		if (c.sock == INVALID_SOCKET) continue;
		FD_SET(c.sock, &read_fds);
		if (c.sock > max_fd) max_fd = c.sock;
	}

	timeval tv = {0, 0}; // never block the game loop
	if (select(max_fd + 1, &read_fds, nullptr, nullptr, &tv) <= 0) {
		/* Still service any connections that already have buffered work. */
	}

	/* Accept new connections. */
	for (auto &pair : _mcp.listeners) {
		if (!FD_ISSET(pair.first, &read_fds)) continue;
		for (;;) {
			sockaddr_storage ss;
			socklen_t slen = sizeof(ss);
			SOCKET s = accept(pair.first, (sockaddr *)&ss, &slen);
			if (s == INVALID_SOCKET) break;
			if (_mcp.conns.size() >= MAX_CONNECTIONS) { closesocket(s); break; }
			SetNonBlocking(s);
			SetNoDelay(s);
			Connection c;
			c.sock = s;
			_mcp.conns.push_back(std::move(c));
		}
	}

	/* Service existing connections. */
	for (auto &c : _mcp.conns) ServiceConnection(c);

	/* Reap closed connections. */
	_mcp.conns.erase(
		std::remove_if(_mcp.conns.begin(), _mcp.conns.end(), [](const Connection &c) { return c.sock == INVALID_SOCKET; }),
		_mcp.conns.end());
}

bool IsRunning() { return _mcp.running; }

std::string GetEndpoint()
{
	if (!_mcp.running) return "";
	return "http://127.0.0.1:" + fmt::format("{}", _mcp.port) + "/mcp";
}

std::string GetToken()
{
	if (!_mcp.token_valid) return "";
	return ToHex(_mcp.token);
}

void RotateToken()
{
	if (!_mcp.running) return;
	RandomBytesWithFallback(_mcp.token);
	_mcp.token_valid = true;
	/* No session table yet; rotation simply invalidates the old bearer value. */
	Debug(net, 1, "[prismatic-mcp] bearer token rotated");
}

std::string GetStatusLine()
{
	std::string s = "prismatic_mcp";
	s += _mcp.running ? " running" : " stopped";
	if (_mcp.running) {
		s += " endpoint=" + GetEndpoint();
		s += " read_only=true";
		s += " requests=" + fmt::format("{}", _mcp.requests_total);
		s += " denied=" + fmt::format("{}", _mcp.requests_denied);
		s += " conns=" + fmt::format("{}", _mcp.conns.size());
	}
	return s;
}

} // namespace PrismaticMCP

#endif /* OTTD_PRISMATIC_MCP_SERVER */
