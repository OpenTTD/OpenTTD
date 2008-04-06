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

/**
 * Find the requested driver and return its class.
 * @param name the driver to select.
 * @post Sets the driver so GetCurrentDriver() returns it too.
 */
const Driver *DriverFactoryBase::SelectDriver(const char *name, Driver::Type type)
{
	if (GetDrivers().size() == 0) return NULL;

	if (*name == '\0') {
		/* Probe for this driver */
		for (int priority = 10; priority >= 0; priority--) {
			Drivers::iterator it = GetDrivers().begin();
			for (; it != GetDrivers().end(); ++it) {
				DriverFactoryBase *d = (*it).second;

				/* Check driver type */
				if (d->type != type) continue;
				if (d->priority != priority) continue;

				Driver *newd = d->CreateInstance();
				const char *err = newd->Start(NULL);
				if (err == NULL) {
					DEBUG(driver, 1, "Successfully probed %s driver '%s'", GetDriverTypeName(type), d->name);
					delete *GetActiveDriver(type);
					*GetActiveDriver(type) = newd;
					return newd;
				}

				DEBUG(driver, 1, "Probing %s driver '%s' failed with error: %s", GetDriverTypeName(type), d->name, err);
				delete newd;
			}
		}
		error("Couldn't find any suitable %s driver", GetDriverTypeName(type));
	} else {
		char *parm;
		char buffer[256];
		const char *parms[32];

		/* Extract the driver name and put parameter list in parm */
		strecpy(buffer, name, lastof(buffer));
		parm = strchr(buffer, ':');
		parms[0] = NULL;
		if (parm != NULL) {
			uint np = 0;
			/* Tokenize the parm. */
			do {
				*parm++ = '\0';
				if (np < lengthof(parms) - 1) parms[np++] = parm;
				while (*parm != '\0' && *parm != ',') parm++;
			} while (*parm == ',');
			parms[np] = NULL;
		}

		/* Find this driver */
		Drivers::iterator it = GetDrivers().begin();
		for (; it != GetDrivers().end(); ++it) {
			DriverFactoryBase *d = (*it).second;

			/* Check driver type */
			if (d->type != type) continue;

			/* Check driver name */
			if (strcasecmp(buffer, d->name) != 0) continue;

			/* Found our driver, let's try it */
			Driver *newd = d->CreateInstance();

			const char *err = newd->Start(parms);
			if (err != NULL) {
				delete newd;
				error("Unable to load driver '%s'. The error was: %s", d->name, err);
			}

			DEBUG(driver, 1, "Successfully loaded %s driver '%s'", GetDriverTypeName(type), d->name);
			delete *GetActiveDriver(type);
			*GetActiveDriver(type) = newd;
			return newd;
		}
		error("No such %s driver: %s\n", GetDriverTypeName(type), buffer);
	}
}

/**
 * Register a driver internally, based on its name.
 * @param name the name of the driver.
 * @note an assert() will be trigger if 2 driver with the same name try to register.
 */
void DriverFactoryBase::RegisterDriver(const char *name, Driver::Type type, int priority)
{
	/* Don't register nameless Drivers */
	if (name == NULL) return;

	this->name = strdup(name);
	this->type = type;
	this->priority = priority;

	/* Prefix the name with driver type to make it unique */
	char buf[32];
	strecpy(buf, GetDriverTypeName(type), lastof(buf));
	strecpy(buf + 5, name, lastof(buf));

#if !defined(NDEBUG) || defined(WITH_ASSERT)
	/* NDEBUG disables asserts and gives a warning: unused variable 'P' */
	std::pair<Drivers::iterator, bool> P =
#endif /* !NDEBUG */
	GetDrivers().insert(Drivers::value_type(buf, this));
	assert(P.second);
}

/**
 * Build a human readable list of available drivers, grouped by type.
 */
char *DriverFactoryBase::GetDriversInfo(char *p, const char *last)
{
	for (Driver::Type type = Driver::DT_BEGIN; type != Driver::DT_END; type++) {
		p += snprintf(p, last - p, "List of %s drivers:\n", GetDriverTypeName(type));

		for (int priority = 10; priority >= 0; priority--) {
			Drivers::iterator it = GetDrivers().begin();
			for (; it != GetDrivers().end(); it++) {
				DriverFactoryBase *d = (*it).second;
				if (d->type != type) continue;
				if (d->priority != priority) continue;
				p += snprintf(p, last - p, "%18s: %s\n", d->name, d->GetDescription());
			}
		}

		p += snprintf(p, last - p, "\n");
	}

	return p;
}
