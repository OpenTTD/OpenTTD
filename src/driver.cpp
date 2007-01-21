/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "driver.h"
#include "functions.h"
#include "hal.h"
#include "string.h"

#include "music/bemidi.h"
#include "music/dmusic.h"
#include "music/extmidi.h"
#include "music/null_m.h"
#include "music/os2_m.h"
#include "music/win32_m.h"
#include "music/qtmidi.h"

#include "sound/null_s.h"
#include "sound/sdl_s.h"
#include "sound/cocoa_s.h"
#include "sound/win32_s.h"

#include "video/dedicated_v.h"
#include "video/null_v.h"
#include "video/sdl_v.h"
#include "video/cocoa_v.h"
#include "video/win32_v.h"

typedef struct DriverDesc {
	const char* name;
	const char* longname;
	const HalCommonDriver* drv;
} DriverDesc;

typedef struct DriverClass {
	const DriverDesc *descs;
	const char *name;
	const HalCommonDriver** drv;
} DriverClass;


#define M(x, y, z) { x, y, (const HalCommonDriver *)(void *)z }
static const DriverDesc _music_driver_descs[] = {
#ifdef __BEOS__
	M("bemidi",  "BeOS MIDI Driver",        &_bemidi_music_driver),
#endif
#if defined(__OS2__) && !defined(__INNOTEK_LIBC__)
	M("os2",     "OS/2 Music Driver",       &_os2_music_driver),
#endif
#ifdef WIN32_ENABLE_DIRECTMUSIC_SUPPORT
	M("dmusic",  "DirectMusic MIDI Driver", &_dmusic_midi_driver),
#endif
#if defined(WIN32) && !defined(WINCE)
	M("win32",   "Win32 MIDI Driver",       &_win32_music_driver),
#endif
#if defined(__APPLE__) && !defined(DEDICATED)
	M("qt",      "QuickTime MIDI Driver",   &_qtime_music_driver),
#endif
#ifdef UNIX
#if !defined(__MORPHOS__) && !defined(__AMIGA__)
	M("extmidi", "External MIDI Driver",    &_extmidi_music_driver),
#endif
#endif
	M("null",    "Null Music Driver",       &_null_music_driver),
	M(NULL, NULL, NULL)
};

static const DriverDesc _sound_driver_descs[] = {
#if defined(WIN32) && !defined(WINCE)
	M("win32", "Win32 WaveOut Driver", &_win32_sound_driver),
#endif
#ifdef WITH_SDL
	M("sdl",   "SDL Sound Driver",     &_sdl_sound_driver),
#endif
#ifdef WITH_COCOA
	M("cocoa", "Cocoa Sound Driver",   &_cocoa_sound_driver),
#endif
	M("null",  "Null Sound Driver",    &_null_sound_driver),
	M(NULL, NULL, NULL)
};

static const DriverDesc _video_driver_descs[] = {
#ifdef WIN32
	M("win32",      "Win32 GDI Video Driver", &_win32_video_driver),
#endif
#ifdef WITH_SDL
	M("sdl",        "SDL Video Driver",       &_sdl_video_driver),
#endif
#ifdef WITH_COCOA
	M("cocoa",      "Cocoa Video Driver",       &_cocoa_video_driver),
#endif
	M("null",       "Null Video Driver",      &_null_video_driver),
#ifdef ENABLE_NETWORK
	M("dedicated",  "Dedicated Video Driver", &_dedicated_video_driver),
#endif
	M(NULL, NULL, NULL)
};
#undef M


#define M(x, y, z) { x, y, (const HalCommonDriver **)(void *)z }
static const DriverClass _driver_classes[] = {
	M(_video_driver_descs, "video", &_video_driver),
	M(_sound_driver_descs, "sound", &_sound_driver),
	M(_music_driver_descs, "music", &_music_driver)
};
#undef M

static const DriverDesc* GetDriverByName(const DriverDesc* dd, const char* name)
{
	for (; dd->name != NULL; dd++) {
		if (strcmp(dd->name, name) == 0) return dd;
	}
	return NULL;
}

void LoadDriver(int driver, const char *name)
{
	const DriverClass *dc = &_driver_classes[driver];
	const DriverDesc *dd;
	const char *err;

	if (*name == '\0') {
		for (dd = dc->descs; dd->name != NULL; dd++) {
			err = dd->drv->start(NULL);
			if (err == NULL) break;
			DEBUG(driver, 1, "Probing %s driver '%s' failed with error: %s",
				dc->name, dd->name, err
			);
		}
		if (dd->name == NULL) error("Couldn't find any suitable %s driver", dc->name);

		DEBUG(driver, 1, "Successfully probed %s driver '%s'", dc->name, dd->name);

		*dc->drv = dd->drv;
	} else {
		char* parm;
		char buffer[256];
		const char* parms[32];

		// Extract the driver name and put parameter list in parm
		ttd_strlcpy(buffer, name, sizeof(buffer));
		parm = strchr(buffer, ':');
		parms[0] = NULL;
		if (parm != NULL) {
			uint np = 0;
			// Tokenize the parm.
			do {
				*parm++ = '\0';
				if (np < lengthof(parms) - 1)
					parms[np++] = parm;
				while (*parm != '\0' && *parm != ',')
					parm++;
			} while (*parm == ',');
			parms[np] = NULL;
		}
		dd = GetDriverByName(dc->descs, buffer);
		if (dd == NULL)
			error("No such %s driver: %s\n", dc->name, buffer);

		if (*dc->drv != NULL) (*dc->drv)->stop();
		*dc->drv = NULL;

		err = dd->drv->start(parms);
		if (err != NULL) {
			error("Unable to load driver %s(%s). The error was: %s\n",
				dd->name, dd->longname, err
			);
		}
		*dc->drv = dd->drv;
	}
}


static const char* GetDriverParam(const char* const* parm, const char* name)
{
	size_t len;

	if (parm == NULL) return NULL;

	len = strlen(name);
	for (; *parm != NULL; parm++) {
		const char* p = *parm;

		if (strncmp(p, name, len) == 0) {
			if (p[len] == '=')  return p + len + 1;
			if (p[len] == '\0') return p + len;
		}
	}
	return NULL;
}

bool GetDriverParamBool(const char* const* parm, const char* name)
{
	return GetDriverParam(parm, name) != NULL;
}

int GetDriverParamInt(const char* const* parm, const char* name, int def)
{
	const char* p = GetDriverParam(parm, name);
	return p != NULL ? atoi(p) : def;
}


char *GetDriverList(char* p, const char *last)
{
	const DriverClass* dc;

	for (dc = _driver_classes; dc != endof(_driver_classes); dc++) {
		const DriverDesc* dd;

		p += snprintf(p, last - p, "List of %s drivers:\n", dc->name);
		for (dd = dc->descs; dd->name != NULL; dd++) {
			p += snprintf(p, last - p, "%10s: %s\n", dd->name, dd->longname);
		}
		p = strecpy(p, "\n", last);
	}

	return p;
}
