/* $Id$ */

/** @file driver.h */

#ifndef DRIVER_H
#define DRIVER_H

#include "debug.h"
#include "helpers.hpp"
#include "string.h"
#include <string>
#include <map>

bool GetDriverParamBool(const char* const* parm, const char* name);
int GetDriverParamInt(const char* const* parm, const char* name, int def);

class Driver {
public:
	virtual bool CanProbe() = 0;

	virtual const char *Start(const char * const *parm) = 0;

	virtual void Stop() = 0;

	virtual ~Driver() { }

	enum Type {
		DT_BEGIN = 0,
		DT_SOUND = 0,
		DT_MUSIC,
		DT_VIDEO,
		DT_END,
	};
};

DECLARE_POSTFIX_INCREMENT(Driver::Type);


class DriverFactoryBase {
private:
	Driver::Type type;
	char *name;
	typedef std::map<std::string, DriverFactoryBase *> Drivers;

	static Drivers &GetDrivers()
	{
		static Drivers &s_drivers = *new Drivers();
		return s_drivers;
	}

	static Driver **GetActiveDriver(Driver::Type type)
	{
		static Driver *s_driver[3] = { NULL, NULL, NULL };
		return &s_driver[type];
	}

	static const char *GetDriverTypeName(Driver::Type type)
	{
		static const char *driver_type_name[] = { "sound", "music", "video" };
		return driver_type_name[type];
	}

protected:
	/**
	 * Register a driver internally, based on its name.
	 * @param name the name of the driver.
	 * @note an assert() will be trigger if 2 driver with the same name try to register.
	 */
	void RegisterDriver(const char *name, Driver::Type type)
	{
		/* Don't register nameless Drivers */
		if (name == NULL) return;

		this->name = strdup(name);
		this->type = type;

		/* Prefix the name with driver type to make it unique */
		char buf[32];
		strecpy(buf, GetDriverTypeName(type), lastof(buf));
		strecpy(buf + 5, name, lastof(buf));

		std::pair<Drivers::iterator, bool> P = GetDrivers().insert(Drivers::value_type(buf, this));
		assert(P.second);
	}

public:
	DriverFactoryBase() :
		name(NULL)
	{}

	virtual ~DriverFactoryBase() { if (this->name != NULL) GetDrivers().erase(this->name); free(this->name); }

	/**
	 * Find the requested driver and return its class.
	 * @param name the driver to select.
	 * @post Sets the driver so GetCurrentDriver() returns it too.
	 */
	static Driver *SelectDriver(const char *name, Driver::Type type)
	{
		if (GetDrivers().size() == 0) return NULL;

		if (*name == '\0') {
			/* Probe for this driver */
			Drivers::iterator it = GetDrivers().begin();
			for (; it != GetDrivers().end(); ++it) {
				DriverFactoryBase *d = (*it).second;

				/* Check driver type */
				if (d->type != type) continue;

				Driver *newd = d->CreateInstance();
				if (!newd->CanProbe()) {
					DEBUG(driver, 1, "Skipping probe of driver '%s'", d->name);
				} else {
					const char *err = newd->Start(NULL);
					if (err == NULL) {
						DEBUG(driver, 1, "Successfully probed %s driver '%s'", GetDriverTypeName(type), d->name);
						delete *GetActiveDriver(type);
						*GetActiveDriver(type) = newd;
						return newd;
					}

					DEBUG(driver, 1, "Probing %s driver '%s' failed with error: %s", GetDriverTypeName(type), d->name, err);
				}

				delete newd;
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
					if (np < lengthof(parms) - 1)
						parms[np++] = parm;
					while (*parm != '\0' && *parm != ',')
						parm++;
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
	 * Build a human readable list of available drivers, grouped by type.
	 */
	static char *GetDriversInfo(char *p, const char *last)
	{
		for (Driver::Type type = Driver::DT_BEGIN; type != Driver::DT_END; type++) {
			p += snprintf(p, last - p, "List of %s drivers:\n", GetDriverTypeName(type));

			Drivers::iterator it = GetDrivers().begin();
			for (; it != GetDrivers().end(); it++) {
				DriverFactoryBase *d = (*it).second;
				if (d->type == type) p += snprintf(p, last - p, "%18s: %s\n", d->name, d->GetDescription());
			}

			p += snprintf(p, last - p, "\n");
		}

		return p;
	}

	/**
	 * Get a nice description of the driver-class.
	 */
	virtual const char *GetDescription() = 0;

	/**
	 * Create an instance of this driver-class.
	 */
	virtual Driver *CreateInstance() = 0;
};

#endif /* DRIVER_H */
