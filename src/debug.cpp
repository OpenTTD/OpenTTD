/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file debug.cpp Handling of printing debug messages. */

#include "stdafx.h"
#include "console_func.h"
#include "debug.h"
#include "string_func.h"
#include "fileio_func.h"
#include "settings_type.h"
#include <mutex>

#if defined(_WIN32)
#include "os/windows/win32.h"
#endif

#include <fmt/chrono.h>

#include "network/network_admin.h"
SOCKET _debug_socket = INVALID_SOCKET;

#include "safeguards.h"

/** Element in the queue of debug messages that have to be passed to either NetworkAdminConsole or IConsolePrint.*/
struct QueuedDebugItem {
	std::string level;   ///< The used debug level.
	std::string message; ///< The actual formatted message.
};
std::atomic<bool> _debug_remote_console; ///< Whether we need to send data to either NetworkAdminConsole or IConsolePrint.
std::mutex _debug_remote_console_mutex; ///< Mutex to guard the queue of debug messages for either NetworkAdminConsole or IConsolePrint.
std::vector<QueuedDebugItem> _debug_remote_console_queue; ///< Queue for debug messages to be passed to NetworkAdminConsole or IConsolePrint.
std::vector<QueuedDebugItem> _debug_remote_console_queue_spare; ///< Spare queue to swap with _debug_remote_console_queue.

int _debug_driver_level;
int _debug_grf_level;
int _debug_map_level;
int _debug_misc_level;
int _debug_net_level;
int _debug_sprite_level;
int _debug_oldloader_level;
int _debug_npf_level;
int _debug_yapf_level;
int _debug_fontcache_level;
int _debug_script_level;
int _debug_sl_level;
int _debug_gamelog_level;
int _debug_desync_level;
int _debug_console_level;
#ifdef RANDOM_DEBUG
int _debug_random_level;
#endif

struct DebugLevel {
	const char *name;
	int *level;
};

#define DEBUG_LEVEL(x) { #x, &_debug_##x##_level }
	static const DebugLevel debug_level[] = {
	DEBUG_LEVEL(driver),
	DEBUG_LEVEL(grf),
	DEBUG_LEVEL(map),
	DEBUG_LEVEL(misc),
	DEBUG_LEVEL(net),
	DEBUG_LEVEL(sprite),
	DEBUG_LEVEL(oldloader),
	DEBUG_LEVEL(npf),
	DEBUG_LEVEL(yapf),
	DEBUG_LEVEL(fontcache),
	DEBUG_LEVEL(script),
	DEBUG_LEVEL(sl),
	DEBUG_LEVEL(gamelog),
	DEBUG_LEVEL(desync),
	DEBUG_LEVEL(console),
#ifdef RANDOM_DEBUG
	DEBUG_LEVEL(random),
#endif
	};
#undef DEBUG_LEVEL

/**
 * Dump the available debug facility names in the help text.
 * @param output_iterator The iterator to write the string to.
 */
void DumpDebugFacilityNames(std::back_insert_iterator<std::string> &output_iterator)
{
	bool written = false;
	for (const DebugLevel *i = debug_level; i != endof(debug_level); ++i) {
		if (!written) {
			fmt::format_to(output_iterator, "List of debug facility names:\n");
		} else {
			fmt::format_to(output_iterator, ", ");
		}
		fmt::format_to(output_iterator, i->name);
		written = true;
	}
	if (written) {
		fmt::format_to(output_iterator, "\n\n");
	}
}

/**
 * Internal function for outputting the debug line.
 * @param level Debug category.
 * @param message The message to output.
 */
void DebugPrint(const char *level, const std::string &message)
{
	if (_debug_socket != INVALID_SOCKET) {
		std::string msg = fmt::format("{}dbg: [{}] {}\n", GetLogPrefix(), level, message);

		/* Prevent sending a message concurrently, as that might cause interleaved messages. */
		static std::mutex _debug_socket_mutex;
		std::lock_guard<std::mutex> lock(_debug_socket_mutex);

		/* Sending out an error when this fails would be nice, however... the error
		 * would have to be send over this failing socket which won't work. */
		send(_debug_socket, msg.c_str(), (int)msg.size(), 0);
		return;
	}
	if (strcmp(level, "desync") == 0) {
		static FILE *f = FioFOpenFile("commands-out.log", "wb", AUTOSAVE_DIR);
		if (f == nullptr) return;

		fmt::print(f, "{}{}\n", GetLogPrefix(), message);
		fflush(f);
#ifdef RANDOM_DEBUG
	} else if (strcmp(level, "random") == 0) {
		static FILE *f = FioFOpenFile("random-out.log", "wb", AUTOSAVE_DIR);
		if (f == nullptr) return;

		fmt::print(f, "{}\n", message);
		fflush(f);
#endif
	} else {
		fmt::print(stderr, "{}dbg: [{}] {}\n", GetLogPrefix(), level, message);

		if (_debug_remote_console.load()) {
			/* Only add to the queue when there is at least one consumer of the data. */
			std::lock_guard<std::mutex> lock(_debug_remote_console_mutex);
			_debug_remote_console_queue.push_back({ level, message });
		}
	}
}

/**
 * Set debugging levels by parsing the text in \a s.
 * For setting individual levels a string like \c "net=3,grf=6" should be used.
 * If the string starts with a number, the number is used as global debugging level.
 * @param s Text describing the wanted debugging levels.
 * @param error_func The function to call if a parse error occurs.
 */
void SetDebugString(const char *s, void (*error_func)(const std::string &))
{
	int v;
	char *end;
	const char *t;

	/* Store planned changes into map during parse */
	std::map<const char *, int> new_levels;

	/* Global debugging level? */
	if (*s >= '0' && *s <= '9') {
		const DebugLevel *i;

		v = std::strtoul(s, &end, 0);
		s = end;

		for (i = debug_level; i != endof(debug_level); ++i) {
			new_levels[i->name] = v;
		}
	}

	/* Individual levels */
	for (;;) {
		/* skip delimiters */
		while (*s == ' ' || *s == ',' || *s == '\t') s++;
		if (*s == '\0') break;

		t = s;
		while (*s >= 'a' && *s <= 'z') s++;

		/* check debugging levels */
		const DebugLevel *found = nullptr;
		for (const DebugLevel *i = debug_level; i != endof(debug_level); ++i) {
			if (s == t + strlen(i->name) && strncmp(t, i->name, s - t) == 0) {
				found = i;
				break;
			}
		}

		if (*s == '=') s++;
		v = std::strtoul(s, &end, 0);
		s = end;
		if (found != nullptr) {
			new_levels[found->name] = v;
		} else {
			std::string error_string = fmt::format("Unknown debug level '{}'", std::string(t, s - t));
			error_func(error_string);
			return;
		}
	}

	/* Apply the changes after parse is successful */
	for (const DebugLevel *i = debug_level; i != endof(debug_level); ++i) {
		const auto &nl = new_levels.find(i->name);
		if (nl != new_levels.end()) {
			*i->level = nl->second;
		}
	}
}

/**
 * Print out the current debug-level.
 * Just return a string with the values of all the debug categories.
 * @return string with debug-levels
 */
std::string GetDebugString()
{
	std::string result;
	for (const DebugLevel *i = debug_level; i != endof(debug_level); ++i) {
		if (!result.empty()) result += ", ";
		fmt::format_to(std::back_inserter(result), "{}={}", i->name, *i->level);
	}
	return result;
}

/**
 * Get the prefix for logs; if show_date_in_logs is enabled it returns
 * the date, otherwise it returns nothing.
 * @return the prefix for logs (do not free), never nullptr
 */
const char *GetLogPrefix()
{
	static std::string _log_prefix;
	if (_settings_client.gui.show_date_in_logs) {
		_log_prefix = fmt::format("[{:%Y-%m-%d %H:%M:%S}] ", fmt::localtime(time(nullptr)));
	} else {
		_log_prefix.clear();
	}
	return _log_prefix.c_str();
}

/**
 * Send the queued Debug messages to either NetworkAdminConsole or IConsolePrint from the
 * GameLoop thread to prevent concurrent accesses to both the NetworkAdmin's packet queue
 * as well as IConsolePrint's buffers.
 *
 * This is to be called from the GameLoop thread.
 */
void DebugSendRemoteMessages()
{
	if (!_debug_remote_console.load()) return;

	{
		std::lock_guard<std::mutex> lock(_debug_remote_console_mutex);
		std::swap(_debug_remote_console_queue, _debug_remote_console_queue_spare);
	}

	for (auto &item : _debug_remote_console_queue_spare) {
		NetworkAdminConsole(item.level, item.message);
		if (_settings_client.gui.developer >= 2) IConsolePrint(CC_DEBUG, "dbg: [{}] {}", item.level, item.message);
	}

	_debug_remote_console_queue_spare.clear();
}

/**
 * Reconsider whether we need to send debug messages to either NetworkAdminConsole
 * or IConsolePrint. The former is when they have enabled console handling whereas
 * the latter depends on the gui.developer setting's value.
 *
 * This is to be called from the GameLoop thread.
 */
void DebugReconsiderSendRemoteMessages()
{
	bool enable = _settings_client.gui.developer >= 2;

	for (ServerNetworkAdminSocketHandler *as : ServerNetworkAdminSocketHandler::IterateActive()) {
		if (as->update_frequency[ADMIN_UPDATE_CONSOLE] & ADMIN_FREQUENCY_AUTOMATIC) {
			enable = true;
			break;
		}
	}

	_debug_remote_console.store(enable);
}
