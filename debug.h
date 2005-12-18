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
	extern int _debug_pbs_level;
	extern int _debug_ntp_level;
	extern int _debug_npf_level;
#endif

void CDECL debug(const char *s, ...);

void SetDebugString(const char *s);
const char *GetDebugString(void);

#endif /* DEBUG_H */
