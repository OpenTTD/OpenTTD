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
	Facility facility; ///< The facility of the message.
	std::string message; ///< The actual formatted message.
};
std::atomic<bool> _debug_remote_console; ///< Whether we need to send data to either NetworkAdminConsole or IConsolePrint.
std::mutex _debug_remote_console_mutex; ///< Mutex to guard the queue of debug messages for either NetworkAdminConsole or IConsolePrint.
std::vector<QueuedDebugItem> _debug_remote_console_queue; ///< Queue for debug messages to be passed to NetworkAdminConsole or IConsolePrint.
std::vector<QueuedDebugItem> _debug_remote_console_queue_spare; ///< Spare queue to swap with _debug_remote_console_queue.

/** Severity level for each debug facility. */
EnumIndexArray<Severity, Facility, Facility::End> _debug_level;

/** Name for each debug facility. */
static EnumIndexArray<std::string_view, Facility, Facility::End> _debug_facilities = {
	"driver", // Facility::Driver
	"grf", // Facility::Grf
	"map", // Facility::Map
	"misc", // Facility::Misc
	"net", // Facility::Net
	"sprite", // Facility::Sprite
	"oldloader", // Facility::Oldloader
	"yapf", // Facility::Yapf
	"fontcache", // Facility::Fontcache
	"script", // Facility::Script
	"sl", // Facility::Sl
	"gamelog", // Facility::Gamelog
	"desync", // Facility::Desync
	"console", // Facility::Console
	"random", // Facility::Random
};

/**
 * Dump the available debug facility names in the help text.
 * @param output_iterator The iterator to write the string to.
 */
void DumpDebugFacilityNames(std::back_insert_iterator<std::string> &output_iterator)
{
	bool written = false;
	for (Facility facility : EnumRange(Facility::End)) {
		if (!written) {
			fmt::format_to(output_iterator, "List of debug facility names:\n");
		} else {
			fmt::format_to(output_iterator, ", ");
		}
		fmt::format_to(output_iterator, "{}", _debug_facilities[facility]);
		written = true;
	}
	if (written) {
		fmt::format_to(output_iterator, "\n\n");
	}
}

/**
 * Internal function for outputting the debug line.
 * @param facility The facility category/classification of the debug message.
 * @param severity The severity of the debug level; lower is more likely to be shown.
 * @param message The message to output.
 */
void DebugPrint(Facility facility, Severity severity, std::string &&message)
{
	if (facility == Facility::Desync && severity != Severity::Fatal) {
		static auto f = FioFOpenFile("commands-out.log", "wb", Subdirectory::Autosave);
		if (!f.has_value()) return;

		fmt::print(*f, "{}{}\n", GetLogPrefix(true), message);
		fflush(*f);
#ifdef RANDOM_DEBUG
	} else if (facility == Facility::Random) {
		static auto f = FioFOpenFile("random-out.log", "wb", Subdirectory::Autosave);
		if (!f.has_value()) return;

		fmt::print(*f, "{}\n", message);
		fflush(*f);
#endif
	} else {
		fmt::print(stderr, "{}dbg: [{}:{}] {}\n", GetLogPrefix(true), _debug_facilities[facility], severity, message);

		if (_debug_remote_console.load()) {
			/* Only add to the queue when there is at least one consumer of the data. */
			std::lock_guard<std::mutex> lock(_debug_remote_console_mutex);
			_debug_remote_console_queue.emplace_back(facility, std::move(message));
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

	/* Store planned changes into a temporary array during parse */
	auto new_debug_level = _debug_level;

	/* Global debugging level? */
	auto level = consumer.TryReadIntegerBase<int>(10);
	if (level.has_value()) {
		new_debug_level.fill(static_cast<Severity>(*level));
	}

	static const std::string_view lowercase_letters{"abcdefghijklmnopqrstuvwxyz"};
	static const std::string_view lowercase_letters_and_digits{"abcdefghijklmnopqrstuvwxyz0123456789"};

	/* Individual levels */
	while (consumer.AnyBytesLeft()) {
		consumer.SkipUntilCharIn(lowercase_letters);
		if (!consumer.AnyBytesLeft()) break;

		/* Find the level by name. */
		std::string_view key = consumer.ReadUntilCharNotIn(lowercase_letters);
		auto it = std::ranges::find(_debug_facilities, key);
		if (it == std::end(_debug_facilities)) {
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

		new_debug_level[static_cast<Facility>(std::distance(_debug_facilities.begin(), it))] = static_cast<Severity>(*level);
	}

	/* Apply the changes after parse is successful */
	_debug_level = new_debug_level;
}

/**
 * Print out the current debug-level.
 * Just return a string with the values of all the debug categories.
 * @return string with debug-levels
 */
std::string GetDebugString()
{
	std::string result;
	for (Facility facility : EnumRange(Facility::End)) {
		if (!result.empty()) result += ", ";
		format_append(result, "{}={}", _debug_facilities[facility], _debug_level[facility]);
	}
	return result;
}

/**
 * Get the prefix for logs.
 *
 * If show_date_in_logs or \p force is enabled it returns
 * the date, otherwise it returns an empty string.
 *
 * @param force Whether to force the prefix on.
 * @return The prefix for logs.
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
		NetworkAdminConsole(_debug_facilities[item.facility], item.message);
		if (_settings_client.gui.developer >= 2) IConsolePrint(CC_DEBUG, "dbg: [{}] {}", _debug_facilities[item.facility], item.message);
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
		if (as->update_frequency[AdminUpdateType::Console].Test(AdminUpdateFrequency::Automatic)) {
			enable = true;
			break;
		}
	}

	_debug_remote_console.store(enable);
}
