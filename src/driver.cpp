/* $Id$ */

/** @file driver.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "driver.h"

#include "sound/sound_driver.hpp"
#include "music/music_driver.hpp"
#include "video/video_driver.hpp"

VideoDriver *_video_driver;
char _ini_videodriver[32];
int _num_resolutions;
uint16 _resolutions[32][2];
uint16 _cur_resolution[2];

SoundDriver *_sound_driver;
char _ini_sounddriver[32];

MusicDriver *_music_driver;
char _ini_musicdriver[32];

char _ini_blitter[32];

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
