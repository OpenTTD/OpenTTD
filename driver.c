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

#include "sound/null_s.h"
#include "sound/sdl_s.h"
#include "sound/win32_s.h"

#include "video/dedicated_v.h"
#include "video/null_v.h"
#include "video/sdl_v.h"
#include "video/win32_v.h"

typedef struct DriverDesc {
	const char* name;
	const char* longname;
	const void* drv;
} DriverDesc;

typedef struct DriverClass {
	const DriverDesc *descs;
	const char *name;
	void *var;
} DriverClass;

static const DriverDesc _video_driver_descs[];
static const DriverDesc _sound_driver_descs[];
static const DriverDesc _music_driver_descs[];

static const DriverClass _driver_classes[] = {
	{_video_driver_descs, "video", &_video_driver},
	{_sound_driver_descs, "sound", &_sound_driver},
	{_music_driver_descs, "music", &_music_driver},
};

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
	const void **var;
	const void *drv;
	const char *err;
	char *parm;
	char buffer[256];
	const char *parms[32];

	parms[0] = NULL;

	if (*name == '\0') {
		for (dd = dc->descs; dd->name != NULL; dd++) {
			err = ((const HalCommonDriver*)dd->drv)->start(parms);
			if (err == NULL) break;
			DEBUG(driver, 1) ("Probing %s driver \"%s\" failed with error: %s",
				dc->name, dd->name, err
			);
		}
		if (dd->name == NULL) {
			error("Couldn't find any suitable %s driver", dc->name);
		}

		DEBUG(driver, 1)
			("Successfully probed %s driver \"%s\"", dc->name, dd->name);

		var = dc->var;
		*var = dd->drv;
	} else {
		// Extract the driver name and put parameter list in parm
		ttd_strlcpy(buffer, name, sizeof(buffer));
		parm = strchr(buffer, ':');
		if (parm) {
			uint np = 0;
			// Tokenize the parm.
			do {
				*parm++ = 0;
				if (np < lengthof(parms) - 1)
					parms[np++] = parm;
				while (*parm != 0 && *parm != ',')
					parm++;
			} while (*parm == ',');
			parms[np] = NULL;
		}
		dd = GetDriverByName(dc->descs, buffer);
		if (dd == NULL)
			error("No such %s driver: %s\n", dc->name, buffer);

		var = dc->var;
		if (*var != NULL) ((const HalCommonDriver*)*var)->stop();
		*var = NULL;
		drv = dd->drv;

		err = ((const HalCommonDriver*)drv)->start(parms);
		if (err != NULL) {
			error("Unable to load driver %s(%s). The error was: %s\n",
				dd->name, dd->longname, err
			);
		}
		*var = drv;
	}
}


static const char* GetDriverParam(const char* const* parm, const char* name)
{
	uint len = strlen(name);

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


void GetDriverList(char* p)
{
	const DriverClass* dc;

	for (dc = _driver_classes; dc != endof(_driver_classes); dc++) {
		const DriverDesc* dd;

		p += sprintf(p, "List of %s drivers:\n", dc->name);
		for (dd = dc->descs; dd->name != NULL; dd++) {
			p += sprintf(p, "%10s: %s\n", dd->name, dd->longname);
		}
	}
}


static const DriverDesc _music_driver_descs[] = {
#ifdef __BEOS__
	{ "bemidi",  "BeOS MIDI Driver",        &_bemidi_music_driver },
#endif
#ifdef __OS2__
	{ "os2",     "OS/2 Music Driver",       &_os2_music_driver},
#endif
#ifdef WIN32_ENABLE_DIRECTMUSIC_SUPPORT
	{ "dmusic",  "DirectMusic MIDI Driver", &_dmusic_midi_driver },
#endif
#ifdef WIN32
	{ "win32",   "Win32 MIDI Driver",       &_win32_music_driver },
#endif
#ifdef UNIX
#if !defined(__BEOS__) && !defined(__MORPHOS__) && !defined(__AMIGA__)
	{ "extmidi", "External MIDI Driver",    &_extmidi_music_driver },
#endif
#endif
	{ "null",    "Null Music Driver",       &_null_music_driver },
	{ NULL, NULL, NULL}
};

static const DriverDesc _sound_driver_descs[] = {
#ifdef WIN32
	{ "win32", "Win32 WaveOut Driver", &_win32_sound_driver },
#endif
#ifdef WITH_SDL
	{	"sdl",   "SDL Sound Driver",     &_sdl_sound_driver },
#endif
	{	"null",  "Null Sound Driver",    &_null_sound_driver },
	{	NULL, NULL, NULL}
};

static const DriverDesc _video_driver_descs[] = {
#ifdef WIN32
	{ "win32",      "Win32 GDI Video Driver", &_win32_video_driver },
#endif
#ifdef WITH_SDL
	{ "sdl",        "SDL Video Driver",       &_sdl_video_driver },
#endif
	{ "null",       "Null Video Driver",      &_null_video_driver},
#ifdef ENABLE_NETWORK
	{ "dedicated",	"Dedicated Video Driver",	&_dedicated_video_driver},
#endif
	{ NULL, NULL, NULL}
};
