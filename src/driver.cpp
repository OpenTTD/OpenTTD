/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file driver.cpp Base for all driver handling. */

#include "stdafx.h"
#include "debug.h"
#include "error.h"
#include "error_func.h"
#include "sound/sound_driver.hpp"
#include "music/music_driver.hpp"
#include "video/video_driver.hpp"
#include "string_func.h"
#include "table/strings.h"
#include "fileio_func.h"
#include <sstream>

#ifdef _WIN32
# include <windows.h>
#else
# include <unistd.h>
#endif /* _WIN32 */

#include "safeguards.h"

std::string _ini_videodriver;        ///< The video driver a stored in the configuration file.
std::vector<Dimension> _resolutions; ///< List of resolutions.
Dimension _cur_resolution;           ///< The current resolution.
bool _rightclick_emulate;            ///< Whether right clicking is emulated.

std::string _ini_sounddriver;        ///< The sound driver a stored in the configuration file.

std::string _ini_musicdriver;        ///< The music driver a stored in the configuration file.

std::string _ini_blitter;            ///< The blitter as stored in the configuration file.
bool _blitter_autodetected;          ///< Was the blitter autodetected or specified by the user?

static const std::string HWACCELERATION_TEST_FILE = "hwaccel.dat"; ///< Filename to test if we crashed last time we tried to use hardware acceleration.

/**
 * Get a string parameter the list of parameters.
 * @param parm The parameters.
 * @param name The parameter name we're looking for.
 * @return The parameter value.
 */
const char *GetDriverParam(const StringList &parm, const char *name)
{
	if (parm.empty()) return nullptr;

	size_t len = strlen(name);
	for (auto &p : parm) {
		if (p.compare(0, len, name) == 0) {
			if (p.length() == len) return "";
			if (p[len] == '=') return p.c_str() + len + 1;
		}
	}
	return nullptr;
}

/**
 * Get a boolean parameter the list of parameters.
 * @param parm The parameters.
 * @param name The parameter name we're looking for.
 * @return The parameter value.
 */
bool GetDriverParamBool(const StringList &parm, const char *name)
{
	return GetDriverParam(parm, name) != nullptr;
}

/**
 * Get an integer parameter the list of parameters.
 * @param parm The parameters.
 * @param name The parameter name we're looking for.
 * @param def  The default value if the parameter doesn't exist.
 * @return The parameter value.
 */
int GetDriverParamInt(const StringList &parm, const char *name, int def)
{
	const char *p = GetDriverParam(parm, name);
	return p != nullptr ? atoi(p) : def;
}

/**
 * Find the requested driver and return its class.
 * @param name the driver to select.
 * @param type the type of driver to select
 * @post Sets the driver so GetCurrentDriver() returns it too.
 */
void DriverFactoryBase::SelectDriver(const std::string &name, Driver::Type type)
{
	if (!DriverFactoryBase::SelectDriverImpl(name, type)) {
		name.empty() ?
			UserError("Failed to autoprobe {} driver", GetDriverTypeName(type)) :
			UserError("Failed to select requested {} driver '{}'", GetDriverTypeName(type), name);
	}
}

/**
 * Find the requested driver and return its class.
 * @param name the driver to select.
 * @param type the type of driver to select
 * @post Sets the driver so GetCurrentDriver() returns it too.
 * @return True upon success, otherwise false.
 */
bool DriverFactoryBase::SelectDriverImpl(const std::string &name, Driver::Type type)
{
	if (GetDrivers().empty()) return false;

	if (name.empty()) {
		/* Probe for this driver, but do not fall back to dedicated/null! */
		for (int priority = 10; priority > 0; priority--) {
			for (auto &it : GetDrivers()) {
				DriverFactoryBase *d = it.second;

				/* Check driver type */
				if (d->type != type) continue;
				if (d->priority != priority) continue;

				if (type == Driver::DT_VIDEO && !_video_hw_accel && d->UsesHardwareAcceleration()) continue;

				if (type == Driver::DT_VIDEO && _video_hw_accel && d->UsesHardwareAcceleration()) {
					/* Check if we have already tried this driver in last run.
					 * If it is here, it most likely means we crashed. So skip
					 * hardware acceleration. */
					auto filename = FioFindFullPath(BASE_DIR, HWACCELERATION_TEST_FILE);
					if (!filename.empty()) {
						unlink(filename.c_str());

						Debug(driver, 1, "Probing {} driver '{}' skipped due to earlier crash", GetDriverTypeName(type), d->name);

						_video_hw_accel = false;
						ErrorMessageData msg(STR_VIDEO_DRIVER_ERROR, STR_VIDEO_DRIVER_ERROR_HARDWARE_ACCELERATION_CRASH, true);
						ScheduleErrorMessage(msg);
						continue;
					}

					/* Write empty file to note we are attempting hardware acceleration. */
					auto f = FioFOpenFile(HWACCELERATION_TEST_FILE, "w", BASE_DIR);
					FioFCloseFile(f);
				}

				Driver *oldd = *GetActiveDriver(type);
				Driver *newd = d->CreateInstance();
				*GetActiveDriver(type) = newd;

				const char *err = newd->Start({});
				if (err == nullptr) {
					Debug(driver, 1, "Successfully probed {} driver '{}'", GetDriverTypeName(type), d->name);
					delete oldd;
					return true;
				}

				*GetActiveDriver(type) = oldd;
				Debug(driver, 1, "Probing {} driver '{}' failed with error: {}", GetDriverTypeName(type), d->name, err);
				delete newd;

				if (type == Driver::DT_VIDEO && _video_hw_accel && d->UsesHardwareAcceleration()) {
					_video_hw_accel = false;
					ErrorMessageData msg(STR_VIDEO_DRIVER_ERROR, STR_VIDEO_DRIVER_ERROR_NO_HARDWARE_ACCELERATION, true);
					ScheduleErrorMessage(msg);
				}
			}
		}
		UserError("Couldn't find any suitable {} driver", GetDriverTypeName(type));
	} else {
		/* Extract the driver name and put parameter list in parm */
		std::istringstream buffer(name);
		std::string dname;
		std::getline(buffer, dname, ':');

		std::string param;
		std::vector<std::string> parms;
		while (std::getline(buffer, param, ',')) {
			parms.push_back(param);
		}

		/* Find this driver */
		for (auto &it : GetDrivers()) {
			DriverFactoryBase *d = it.second;

			/* Check driver type */
			if (d->type != type) continue;

			/* Check driver name */
			if (!StrEqualsIgnoreCase(dname, d->name)) continue;

			/* Found our driver, let's try it */
			Driver *newd = d->CreateInstance();

			const char *err = newd->Start(parms);
			if (err != nullptr) {
				delete newd;
				UserError("Unable to load driver '{}'. The error was: {}", d->name, err);
			}

			Debug(driver, 1, "Successfully loaded {} driver '{}'", GetDriverTypeName(type), d->name);
			delete *GetActiveDriver(type);
			*GetActiveDriver(type) = newd;
			return true;
		}
		UserError("No such {} driver: {}\n", GetDriverTypeName(type), dname);
	}
}

/**
 * Mark the current video driver as operational.
 */
void DriverFactoryBase::MarkVideoDriverOperational()
{
	/* As part of the detection whether the GPU driver crashes the game,
	 * and as we are operational now, remove the hardware acceleration
	 * test-file. */
	auto filename = FioFindFullPath(BASE_DIR, HWACCELERATION_TEST_FILE);
	if (!filename.empty()) unlink(filename.c_str());
}

/**
 * Build a human readable list of available drivers, grouped by type.
 * @param output_iterator The iterator to write the string to.
 */
void DriverFactoryBase::GetDriversInfo(std::back_insert_iterator<std::string> &output_iterator)
{
	for (Driver::Type type = Driver::DT_BEGIN; type != Driver::DT_END; type++) {
		fmt::format_to(output_iterator, "List of {} drivers:\n", GetDriverTypeName(type));

		for (int priority = 10; priority >= 0; priority--) {
			for (auto &it : GetDrivers()) {
				DriverFactoryBase *d = it.second;
				if (d->type != type) continue;
				if (d->priority != priority) continue;
				fmt::format_to(output_iterator, "{:>18}: {}\n", d->name, d->GetDescription());
			}
		}

		fmt::format_to(output_iterator, "\n");
	}
}

/**
 * Construct a new DriverFactory.
 * @param type        The type of driver.
 * @param priority    The priority within the driver class.
 * @param name        The name of the driver.
 * @param description A long-ish description of the driver.
 */
DriverFactoryBase::DriverFactoryBase(Driver::Type type, int priority, const char *name, const char *description) :
	type(type), priority(priority), name(name), description(description)
{
	/* Prefix the name with driver type to make it unique */
	std::string typed_name = fmt::format("{}{}", GetDriverTypeName(type), name);

	Drivers &drivers = GetDrivers();
	assert(drivers.find(typed_name) == drivers.end());
	drivers.insert(Drivers::value_type(typed_name, this));
}

/**
 * Frees memory used for this->name
 */
DriverFactoryBase::~DriverFactoryBase()
{
	/* Prefix the name with driver type to make it unique */
	std::string typed_name = fmt::format("{}{}", GetDriverTypeName(type), name);

	Drivers::iterator it = GetDrivers().find(typed_name);
	assert(it != GetDrivers().end());

	GetDrivers().erase(it);
	if (GetDrivers().empty()) delete &GetDrivers();
}
