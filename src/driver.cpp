/* $Id$ */

/** @file driver.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "driver.h"
#include "functions.h"
#include "string.h"

#include "sound/sound_driver.hpp"
#include "music/music_driver.hpp"
#include "video/video_driver.hpp"

SoundDriver *_sound_driver;
MusicDriver *_music_driver;
VideoDriver *_video_driver;

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
