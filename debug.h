/* $Id$ */

#ifndef DEBUG_H
#define DEBUG_H

#ifdef NO_DEBUG_MESSAGES
	#define DEBUG(name, level)
#else
	#define DEBUG(name, level) if (level == 0 || _debug_ ## name ## _level >= level) debug

	extern int _debug_ai_level;
	extern int _debug_driver_level;
	extern int _debug_grf_level;
	extern int _debug_map_level;
	extern int _debug_misc_level;
	extern int _debug_ms_level;
	extern int _debug_net_level;
	extern int _debug_spritecache_level;
	extern int _debug_oldloader_level;
	extern int _debug_ntp_level;
	extern int _debug_npf_level;
	extern int _debug_yapf_level;
	extern int _debug_freetype_level;
#endif

void CDECL debug(const char *s, ...);

void SetDebugString(const char *s);
const char *GetDebugString(void);

/* MSVCRT of course has to have a different syntax for long long *sigh* */
#if defined(_MSC_VER) || defined(__MINGW32__)
# define OTTD_PRINTF64 "I64"
#else
# define OTTD_PRINTF64 "ll"
#endif

// Used for profiling
#define TIC() {\
	extern uint64 _rdtsc(void);\
	uint64 _xxx_ = _rdtsc();\
	static uint64 __sum__ = 0;\
	static uint32 __i__ = 0;

#define TOC(str, count)\
	__sum__ += _rdtsc() - _xxx_;\
	if (++__i__ == count) {\
		printf("[%s]: %" OTTD_PRINTF64 "u [avg: %.1f]\n", str, __sum__, __sum__/(double)__i__);\
		__i__ = 0;\
		__sum__ = 0;\
	}\
}

#endif /* DEBUG_H */
