/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file debug.h Functions related to debugging. */

#ifndef DEBUG_H
#define DEBUG_H

#include "cpu.h"
#include <chrono>
#include "core/format.hpp"

/* Debugging messages policy:
 * These should be the severities used for direct Debug() calls
 * maximum debugging level should be 10 if really deep, deep
 * debugging is needed.
 * (there is room for exceptions, but you have to have a good cause):
 * 0   - errors or severe warnings
 * 1   - other non-fatal, non-severe warnings
 * 2   - crude progress indicator of functionality
 * 3   - important debugging messages (function entry)
 * 4   - debugging messages (crude loop status, etc.)
 * 5   - detailed debugging information
 * 6.. - extremely detailed spamming
 */

/**
 * Ouptut a line of debugging information.
 * @param name The category of debug information.
 * @param level The maximum debug level this message should be shown at. When the debug level for this category is set lower, then the message will not be shown.
 * @param format_string The formatting string of the message.
 */
#define Debug(name, level, format_string, ...) if ((level) == 0 || _debug_ ## name ## _level >= (level)) DebugPrint(#name, fmt::format(FMT_STRING(format_string), ## __VA_ARGS__))
void DebugPrint(const char *level, const std::string &message);

extern int _debug_driver_level;
extern int _debug_grf_level;
extern int _debug_map_level;
extern int _debug_misc_level;
extern int _debug_net_level;
extern int _debug_sprite_level;
extern int _debug_oldloader_level;
extern int _debug_npf_level;
extern int _debug_yapf_level;
extern int _debug_fontcache_level;
extern int _debug_script_level;
extern int _debug_sl_level;
extern int _debug_gamelog_level;
extern int _debug_desync_level;
extern int _debug_console_level;
#ifdef RANDOM_DEBUG
extern int _debug_random_level;
#endif

void DumpDebugFacilityNames(std::back_insert_iterator<std::string> &output_iterator);
void SetDebugString(const char *s, void (*error_func)(const std::string &));
std::string GetDebugString();

/* Shorter form for passing filename and linenumber */
#define FILE_LINE __FILE__, __LINE__

/** TicToc profiling.
 * Usage:
 * static TicToc::State state("A name", 1);
 * TicToc tt(state);
 * --Do your code--
 */
struct TicToc {
	/** Persistent state for TicToc profiling. */
	struct State {
		const std::string_view name;
		const uint32_t max_count;
		uint32_t count = 0;
		uint64_t chrono_sum = 0;

		constexpr State(std::string_view name, uint32_t max_count) : name(name), max_count(max_count) { }
	};

	State &state;
	std::chrono::high_resolution_clock::time_point chrono_start; ///< real time count.

	inline TicToc(State &state) : state(state), chrono_start(std::chrono::high_resolution_clock::now()) { }

	inline ~TicToc()
	{
		this->state.chrono_sum += (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - this->chrono_start)).count();
		if (++this->state.count == this->state.max_count) {
			Debug(misc, 0, "[{}] {} us [avg: {:.1f} us]", this->state.name, this->state.chrono_sum, this->state.chrono_sum / static_cast<double>(this->state.count));
			this->state.count = 0;
			this->state.chrono_sum = 0;
		}
	}
};

void ShowInfoI(const std::string &str);
#define ShowInfo(format_string, ...) ShowInfoI(fmt::format(FMT_STRING(format_string), ## __VA_ARGS__))

std::string GetLogPrefix();

void DebugSendRemoteMessages();
void DebugReconsiderSendRemoteMessages();

#endif /* DEBUG_H */
