/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file debug.cpp Handling of printing debug messages. */

#include "stdafx.h"
#include "core/string_consumer.hpp"
#include "console_func.h"
#include "debug.h"
#include "string_func.h"
#include "fileio_func.h"
#include "settings_type.h"
#include <mutex>

#if defined(_WIN32)
#include "os/windows/win32.h"
#endif

#include "3rdparty/fmt/chrono.h"

#include "network/network_admin.h"

#include "safeguards.h"

/** Element in the queue of debug messages that have to be passed to either NetworkAdminConsole or IConsolePrint.*/
struct QueuedDebugItem {
	std::string_view level;   ///< The used debug level.
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
	std::string_view name;
	int *level;
};

#define DEBUG_LEVEL(x) { #x, &_debug_##x##_level }
static const std::initializer_list<DebugLevel> _debug_levels{
	DEBUG_LEVEL(driver),
	DEBUG_LEVEL(grf),
	DEBUG_LEVEL(map),
	DEBUG_LEVEL(misc),
	DEBUG_LEVEL(net),
	DEBUG_LEVEL(sprite),
	DEBUG_LEVEL(oldloader),
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
	for (const auto &debug_level : _debug_levels) {
		if (!written) {
			fmt::format_to(output_iterator, "List of debug facility names:\n");
		} else {
			fmt::format_to(output_iterator, ", ");
		}
		fmt::format_to(output_iterator, "{}", debug_level.name);
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
void DebugPrint(std::string_view category, int level, std::string &&message)
{
	if (category == "desync" && level != 0) {
		static auto f = FioFOpenFile("commands-out.log", "wb", AUTOSAVE_DIR);
		if (!f.has_value()) return;

		fmt::print(*f, "{}{}\n", GetLogPrefix(true), message);
		fflush(*f);
#ifdef RANDOM_DEBUG
	} else if (category == "random") {
		static auto f = FioFOpenFile("random-out.log", "wb", AUTOSAVE_DIR);
		if (!f.has_value()) return;

		fmt::print(*f, "{}\n", message);
		fflush(*f);
#endif
	} else {
		fmt::print(stderr, "{}dbg: [{}:{}] {}\n", GetLogPrefix(true), category, level, message);

		if (_debug_remote_console.load()) {
			/* Only add to the queue when there is at least one consumer of the data. */
			std::lock_guard<std::mutex> lock(_debug_remote_console_mutex);
			_debug_remote_console_queue.emplace_back(category, std::move(message));
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
void SetDebugString(std::string_view s, SetDebugStringErrorFunc error_func)
{
	StringConsumer consumer{s};

	/* Store planned changes into map during parse */
	std::map<std::string_view, int> new_levels;

	/* Global debugging level? */
	auto level = consumer.TryReadIntegerBase<int>(10);
	if (level.has_value()) {
		for (const auto &debug_level : _debug_levels) {
			new_levels[debug_level.name] = *level;
		}
	}

	static const std::string_view lowercase_letters{"abcdefghijklmnopqrstuvwxyz"};
	static const std::string_view lowercase_letters_and_digits{"abcdefghijklmnopqrstuvwxyz0123456789"};

	/* Individual levels */
	while (consumer.AnyBytesLeft()) {
		consumer.SkipUntilCharIn(lowercase_letters);
		if (!consumer.AnyBytesLeft()) break;

		/* Find the level by name. */
		std::string_view key = consumer.ReadUntilCharNotIn(lowercase_letters);
		auto it = std::ranges::find(_debug_levels, key, &DebugLevel::name);
		if (it == std::end(_debug_levels)) {
			error_func(fmt::format("Unknown debug level '{}'", key));
			return;
		}

		/* Do not skip lowercase letters, so 'net misc=2' won't be resolved
		 * to setting 'net=2' and leaving misc untouched. */
		consumer.SkipUntilCharIn(lowercase_letters_and_digits);
		level = consumer.TryReadIntegerBase<int>(10);
		if (!level.has_value()) {
			error_func(fmt::format("Level for '{}' must be a valid integer.", key));
			return;
		}

		new_levels[it->name] = *level;
	}

	/* Apply the changes after parse is successful */
	for (const auto &debug_level : _debug_levels) {
		const auto &nl = new_levels.find(debug_level.name);
		if (nl != new_levels.end()) {
			*debug_level.level = nl->second;
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
	for (const auto &debug_level : _debug_levels) {
		if (!result.empty()) result += ", ";
		format_append(result, "{}={}", debug_level.name, *debug_level.level);
	}
	return result;
}

/**
 * Get the prefix for logs.
 *
 * If show_date_in_logs or \p force is enabled it returns
 * the date, otherwise it returns an empty string.
 *
 * @return the prefix for logs.
 */
std::string GetLogPrefix(bool force)
{
	std::string log_prefix;
	if (force || _settings_client.gui.show_date_in_logs) {
		log_prefix = fmt::format("[{:%Y-%m-%d %H:%M:%S}] ", fmt::localtime(time(nullptr)));
	}
	return log_prefix;
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
		if (as->update_frequency[ADMIN_UPDATE_CONSOLE].Test(AdminUpdateFrequency::Automatic)) {
			enable = true;
			break;
		}
	}

	_debug_remote_console.store(enable);
}
