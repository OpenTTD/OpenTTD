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

/* Debugging messages policy:
 * These should be the severities used for direct DEBUG() calls
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
 * Output a line of debugging information.
 * @param name Category
 * @param level Debugging level, higher levels means more detailed information.
 */
#define DEBUG(name, level, ...) if ((level) == 0 || _debug_ ## name ## _level >= (level)) debug(#name, __VA_ARGS__)

extern int _debug_driver_level;
extern int _debug_grf_level;
extern int _debug_map_level;
extern int _debug_misc_level;
extern int _debug_net_level;
extern int _debug_sprite_level;
extern int _debug_oldloader_level;
extern int _debug_npf_level;
extern int _debug_yapf_level;
extern int _debug_freetype_level;
extern int _debug_script_level;
extern int _debug_sl_level;
extern int _debug_gamelog_level;
extern int _debug_desync_level;
extern int _debug_console_level;
#ifdef RANDOM_DEBUG
extern int _debug_random_level;
#endif

void CDECL debug(const char *dbg, const char *format, ...) WARN_FORMAT(2, 3);

char *DumpDebugFacilityNames(char *buf, char *last);
void SetDebugString(const char *s);
const char *GetDebugString();

/* Shorter form for passing filename and linenumber */
#define FILE_LINE __FILE__, __LINE__

/* Used for profiling
 *
 * Usage:
 * TIC();
 *   --Do your code--
 * TOC("A name", 1);
 *
 * When you run the TIC() / TOC() multiple times, you can increase the '1'
 *  to only display average stats every N values. Some things to know:
 *
 * for (int i = 0; i < 5; i++) {
 *   TIC();
 *     --Do your code--
 *   TOC("A name", 5);
 * }
 *
 * Is the correct usage for multiple TIC() / TOC() calls.
 *
 * TIC() / TOC() creates its own block, so make sure not the mangle
 *  it with another block.
 *
 * The output is counted in CPU cycles, and not comparable across
 *  machines. Mainly useful for local optimisations.
 **/
#define TIC() {\
	uint64 _xxx_ = ottd_rdtsc();\
	static uint64 _sum_ = 0;\
	static uint32 _i_ = 0;

#define TOC(str, count)\
	_sum_ += ottd_rdtsc() - _xxx_;\
	if (++_i_ == count) {\
		DEBUG(misc, 0, "[%s] " OTTD_PRINTF64 " [avg: %.1f]", str, _sum_, _sum_/(double)_i_);\
		_i_ = 0;\
		_sum_ = 0;\
	}\
}

/* Chrono based version. The output is in microseconds. */
#define TICC() {\
	auto _start_ = std::chrono::high_resolution_clock::now();\
	static uint64 _sum_ = 0;\
	static uint32 _i_ = 0;

#define TOCC(str, _count_)\
	_sum_ += (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - _start_)).count();\
	if (++_i_ == _count_) {\
		DEBUG(misc, 0, "[%s] " OTTD_PRINTF64 " us [avg: %.1f us]", str, _sum_, _sum_/(double)_i_);\
		_i_ = 0;\
		_sum_ = 0;\
	}\
}


void ShowInfo(const char *str);
void CDECL ShowInfoF(const char *str, ...) WARN_FORMAT(1, 2);

const char *GetLogPrefix();

#endif /* DEBUG_H */
