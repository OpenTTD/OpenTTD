/* $Id$ */

/** @file debug.h */

#ifndef DEBUG_H
#define DEBUG_H

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

/* Of course MSVC 2003 and lower has no support for variadic macros
 * so we need to work around this... *sigh* */
#if defined(_MSC_VER) && (_MSC_VER < 1400)
	#define NO_VARARG_MACRO
#endif

#if defined(NO_VARARG_MACRO)
	enum DebugLevelType {
		ai,
		driver,
		grf,
		map,
		misc,
		ms,
		net,
		sprite,
		oldloader,
		ntp,
		npf,
		yapf,
		freetype,
		sl,
		station,
	};
#endif /* NO_VARARG_MACRO */

#ifdef NO_DEBUG_MESSAGES
	#if defined(NO_VARARG_MACRO)
		static inline void DEBUG(int name, int level, ...) {}
	#elif defined(__GNUC__) && (__GNUC__ < 3)
		#define DEBUG(name, level, args...)
	#else
		#define DEBUG(name, level, ...)
	#endif
#else /* NO_DEBUG_MESSAGES */
	#if defined(NO_VARARG_MACRO)
		void CDECL DEBUG(int name, int level, ...);
	#elif defined(__GNUC__) && (__GNUC__ < 3)
		#define DEBUG(name, level, args...) if ((level == 0) || ( _debug_ ## name ## _level >= level)) debug(#name, args)
	#else
		#define DEBUG(name, level, ...) if (level == 0 || _debug_ ## name ## _level >= level) debug(#name, __VA_ARGS__)
	#endif

	extern int _debug_ai_level;
	extern int _debug_driver_level;
	extern int _debug_grf_level;
	extern int _debug_map_level;
	extern int _debug_misc_level;
	extern int _debug_ms_level;
	extern int _debug_net_level;
	extern int _debug_sprite_level;
	extern int _debug_oldloader_level;
	extern int _debug_ntp_level;
	extern int _debug_npf_level;
	extern int _debug_yapf_level;
	extern int _debug_freetype_level;
	extern int _debug_sl_level;
	extern int _debug_station_level;

	#if !defined(NO_VARARG_MACRO)
		void CDECL debug(const char *dbg, ...);
	#endif /* NO_VARARG_MACRO */
#endif /* NO_DEBUG_MESSAGES */

void SetDebugString(const char *s);
const char *GetDebugString();

/* MSVCRT of course has to have a different syntax for long long *sigh* */
#if defined(_MSC_VER) || defined(__MINGW32__)
	#define OTTD_PRINTF64 "I64"
#else
	#define OTTD_PRINTF64 "ll"
#endif

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
 *     --Do yuor code--
 *   TOC("A name", 5);
 * }
 *
 * Is the correct usage for multiple TIC() / TOC() calls.
 *
 * TIC() / TOC() creates it's own block, so make sure not the mangle
 *  it with an other block.
 **/
#define TIC() {\
	extern uint64 _rdtsc();\
	uint64 _xxx_ = _rdtsc();\
	static uint64 __sum__ = 0;\
	static uint32 __i__ = 0;

#define TOC(str, count)\
	__sum__ += _rdtsc() - _xxx_;\
	if (++__i__ == count) {\
		DEBUG(misc, 0, "[%s] %" OTTD_PRINTF64 "u [avg: %.1f]\n", str, __sum__, __sum__/(double)__i__);\
		__i__ = 0;\
		__sum__ = 0;\
	}\
}

void ShowInfo(const char *str);
void CDECL ShowInfoF(const char *str, ...);

#ifdef DEBUG_DUMP_COMMANDS
	void CDECL DebugDumpCommands(const char *s, ...);
#else /* DEBUG_DUMP_COMMANDS */
	static inline void DebugDumpCommands(const char *s, ...) {}
#endif /* DEBUG_DUMP_COMMANDS */

#endif /* DEBUG_H */
