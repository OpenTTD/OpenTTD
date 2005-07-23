#include "stdafx.h"
#include "openttd.h"
#include "driver.h"
#include "functions.h"
#include "hal.h"
#include "string.h"

typedef struct {
	const DriverDesc *descs;
	const char *name;
	void *var;
} DriverClass;

static DriverClass _driver_classes[] = {
	{_video_driver_descs, "video", &_video_driver},
	{_sound_driver_descs, "sound", &_sound_driver},
	{_music_driver_descs, "music", &_music_driver},
};

enum {
	DF_PRIORITY_MASK = 0xF,
};

static const DriverDesc* GetDriverByName(const DriverDesc* dd, const char* name)
{
	for (; dd->name != NULL; dd++) {
		if (strcmp(dd->name, name) == 0) return dd;
	}
	return NULL;
}

static const DriverDesc* ChooseDefaultDriver(const DriverDesc* dd)
{
	byte os_version = GetOSVersion();
	const DriverDesc *best = NULL;
	int best_pri = -1;

	for (; dd->name != NULL; dd++) {
		if ((int)(dd->flags & DF_PRIORITY_MASK) > best_pri && os_version >= (byte)dd->flags) {
			best_pri = dd->flags & DF_PRIORITY_MASK;
			best = dd;
		}
	}
	return best;
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

	if (!*name) {
		dd = ChooseDefaultDriver(dc->descs);
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
	}
	var = dc->var;
	if (*var != NULL) ((const HalCommonDriver*)*var)->stop();
	*var = NULL;
	drv = dd->drv;

	err = ((const HalCommonDriver*)drv)->start(parms);
	if (err != NULL)
		error("Unable to load driver %s(%s). The error was: %s\n", dd->name, dd->longname, err);
	*var = drv;
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
