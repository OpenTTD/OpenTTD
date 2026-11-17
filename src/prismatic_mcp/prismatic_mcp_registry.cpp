/*
 * This file is part of korczis's research fork of OpenTTD and is NOT part of upstream OpenTTD.
 * RESEARCH-ONLY / UNSTABLE.
 *
 * The instance registry, from the instance's side: whatever started this process (the control
 * panel, mcp-human-e2e, a person at a terminal), its MCP server announces itself, so every
 * instance of a cluster is discoverable without the launcher knowing about discovery. A reader
 * (the panel's GET /api/instances, Majordomus, a script) drops a record whose owner_pid is gone.
 * Observer effect: one small file written at start and removed at stop.
 */

/** @file prismatic_mcp_registry.cpp Self-registration of MCP servers in the instance registry (research-only). */

#include "../stdafx.h"

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "prismatic_mcp_registry.h"

#include "../3rdparty/nlohmann/json.hpp"
#include "../network/network.h"
#include "../settings_type.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#ifndef _WIN32
#	include <csignal>
#	include <signal.h>
#	include <unistd.h>
#endif

#include "../safeguards.h"

namespace PrismaticMCP::Registry {

namespace {

/** The record this process wrote, so Withdraw() removes exactly that one, and what it announced. */
std::filesystem::path _record;
std::string _endpoint;
uint16_t _port = 0;

#ifndef _WIN32
/** The record's path as a C string for the signal handler, which may only unlink (async-signal-safe). */
char _record_c[1024] = {};

/**
 * Remove the record on a terminating signal, then die of that signal as before. A process the
 * dedicated driver does not cover (a GUI client, a null-driver client) otherwise ends on SIGTERM
 * without Withdraw(), and leaves a record only a reader's owner_pid check can tell is stale.
 * @param sig The signal received.
 */
extern "C" void WithdrawOnSignal(int sig)
{
	if (_record_c[0] != '\0') unlink(_record_c);
	std::signal(sig, SIG_DFL);
	std::raise(sig);
}

/** Hook SIGTERM, SIGINT and SIGHUP once, only where no handler is set (the dedicated driver sets its own). */
void HookSignals()
{
	static bool hooked = false;
	if (hooked) return;
	hooked = true;
	for (int sig : {SIGTERM, SIGINT, SIGHUP}) {
		struct sigaction old {};
		if (sigaction(sig, nullptr, &old) == 0 && old.sa_handler == SIG_DFL) std::signal(sig, WithdrawOnSignal);
	}
}
#endif

std::filesystem::path Directory()
{
	if (const char *dir = std::getenv("PRISMATIC_MCP_REGISTRY"); dir != nullptr && *dir != '\0') return dir;
	const char *home = std::getenv("HOME");
	if (home == nullptr || *home == '\0') return {};
	return std::filesystem::path(home) / ".local" / "state" / "prismatic-mcp" / "instances";
}

long Pid()
{
#ifdef _WIN32
	return 0;
#else
	return static_cast<long>(getpid());
#endif
}

/**
 * What kind of process this is: a headless dedicated server, or a process with an interface.
 * Only that is fixed for the life of the process; whether a GUI process is a network client, a
 * host or a single-player game changes as it plays, so the live role is read from the instance
 * itself (openttd://game/state), never from a record written once at start.
 */
std::string Kind()
{
	return _network_dedicated ? "server" : "gui";
}

} // namespace

void Announce(const std::string &endpoint, uint16_t port, const std::string &token_file)
{
	std::filesystem::path dir = Directory();
	if (dir.empty()) return;
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	if (ec) return;

	const std::string kind = Kind();
	const std::string name = fmt::format("openttd-{}-{}", kind, port);
	nlohmann::json record = {
		{"schema", "mcp-instance/v1"}, {"name", name}, {"kind", kind}, {"owner_pid", Pid()},
		{"registered", std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count()},
		{"endpoint", endpoint}, {"producer", "openttd prismatic_mcp"},
		{"role", kind == "server" ? "dedicated server" : "GUI process (live role in openttd://game/state)"},
		{"game_port", _network_dedicated ? nlohmann::json(_settings_client.network.server_port) : nlohmann::json(nullptr)},
	};
	if (!token_file.empty()) record["token_file"] = std::filesystem::absolute(token_file, ec).string();
	/* Who supervises this process: a JSON object in PRISMATIC_MCP_SUPERVISOR (tools/research/mcp-e2e sets
	 * it to the Majordomus task the stack was started under), passed through; anything else is ignored. */
	if (const char *sup = std::getenv("PRISMATIC_MCP_SUPERVISOR"); sup != nullptr && *sup != '\0') {
		nlohmann::json s = nlohmann::json::parse(sup, nullptr, false);
		if (s.is_object()) record["supervisor"] = std::move(s);
	}

	/* Write beside, then rename: a reader never sees half a record. Owner read/write only. */
	std::filesystem::path path = dir / (name + ".json");
	std::filesystem::path tmp = dir / ("." + name + ".tmp");
	{
		std::ofstream f(tmp, std::ios::out | std::ios::trunc);
		if (!f) return;
		f << record.dump(2) << "\n";
	}
	std::filesystem::permissions(tmp, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
		std::filesystem::perm_options::replace, ec);
	std::filesystem::rename(tmp, path, ec);
	if (!ec) {
		_record = path;
		_endpoint = endpoint;
		_port = port;
#ifndef _WIN32
		const std::string native = path.string();
		if (native.size() < sizeof(_record_c)) {
			std::fill(std::begin(_record_c), std::end(_record_c), '\0');
			std::copy(native.begin(), native.end(), _record_c);
			HookSignals();
		}
#endif
	}
}

void AnnounceTokenFile(const std::string &token_file)
{
	if (_record.empty()) return;
	Announce(_endpoint, _port, token_file);
}

void Withdraw()
{
	if (_record.empty()) return;
#ifndef _WIN32
	_record_c[0] = '\0';
#endif
	std::error_code ec;
	std::filesystem::remove(_record, ec);
	_record.clear();
}

} // namespace PrismaticMCP::Registry

#endif /* OTTD_PRISMATIC_MCP_SERVER */
