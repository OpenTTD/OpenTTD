/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file driver.h Base for all drivers (video, sound, music, etc). */

#ifndef DRIVER_H
#define DRIVER_H

#include "core/enum_type.hpp"
#include "string_type.h"

const char *GetDriverParam(const StringList &parm, const char *name);
bool GetDriverParamBool(const StringList &parm, const char *name);
int GetDriverParamInt(const StringList &parm, const char *name, int def);

/** A driver for communicating with the user. */
class Driver {
public:
	/**
	 * Start this driver.
	 * @param parm Parameters passed to the driver.
	 * @return nullptr if everything went okay, otherwise an error message.
	 */
	virtual const char *Start(const StringList &parm) = 0;

	/**
	 * Stop this driver.
	 */
	virtual void Stop() = 0;

	virtual ~Driver() = default;

	/** The type of driver */
	enum Type {
		DT_BEGIN = 0, ///< Helper for iteration
		DT_MUSIC = 0, ///< A music driver, needs to be before sound to properly shut down extmidi forked music players
		DT_SOUND,     ///< A sound driver
		DT_VIDEO,     ///< A video driver
		DT_END,       ///< Helper for iteration
	};

	/**
	 * Get the name of this driver.
	 * @return The name of the driver.
	 */
	virtual const char *GetName() const = 0;
};

DECLARE_POSTFIX_INCREMENT(Driver::Type)


/** Base for all driver factories. */
class DriverFactoryBase {
private:
	friend class MusicDriver;
	friend class SoundDriver;
	friend class VideoDriver;

	Driver::Type type;       ///< The type of driver.
	int priority;            ///< The priority of this factory.
	const char *name;        ///< The name of the drivers of this factory.
	const char *description; ///< The description of this driver.

	typedef std::map<std::string, DriverFactoryBase *> Drivers; ///< Type for a map of drivers.

	/**
	 * Get the map with drivers.
	 */
	static Drivers &GetDrivers()
	{
		static Drivers &s_drivers = *new Drivers();
		return s_drivers;
	}

	/**
	 * Get the active driver for the given type.
	 * @param type The type to get the driver for.
	 * @return The active driver.
	 */
	static Driver **GetActiveDriver(Driver::Type type)
	{
		static Driver *s_driver[3] = { nullptr, nullptr, nullptr };
		return &s_driver[type];
	}

	/**
	 * Get the driver type name.
	 * @param type The type of driver to get the name of.
	 * @return The name of the type.
	 */
	static const char *GetDriverTypeName(Driver::Type type)
	{
		static const char * const driver_type_name[] = { "music", "sound", "video" };
		return driver_type_name[type];
	}

	static bool SelectDriverImpl(const std::string &name, Driver::Type type);

	static void MarkVideoDriverOperational();

protected:
	DriverFactoryBase(Driver::Type type, int priority, const char *name, const char *description);

	virtual ~DriverFactoryBase();

	/**
	 * Does the driver use hardware acceleration (video-drivers only).
	 * @return True if the driver uses hardware acceleration.
	 */
	virtual bool UsesHardwareAcceleration() const
	{
		return false;
	}

public:
	/**
	 * Shuts down all active drivers
	 */
	static void ShutdownDrivers()
	{
		for (Driver::Type dt = Driver::DT_BEGIN; dt < Driver::DT_END; dt++) {
			Driver *driver = *GetActiveDriver(dt);
			if (driver != nullptr) driver->Stop();
		}
	}

	static void SelectDriver(const std::string &name, Driver::Type type);
	static void GetDriversInfo(std::back_insert_iterator<std::string> &output_iterator);

	/**
	 * Get a nice description of the driver-class.
	 * @return The description.
	 */
	const char *GetDescription() const
	{
		return this->description;
	}

	/**
	 * Create an instance of this driver-class.
	 * @return The instance.
	 */
	virtual Driver *CreateInstance() const = 0;
};

#endif /* DRIVER_H */
