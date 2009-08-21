/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sound_driver.hpp Base for all sound drivers. */

#ifndef SOUND_SOUND_DRIVER_HPP
#define SOUND_SOUND_DRIVER_HPP

#include "../driver.h"

class SoundDriver: public Driver {
public:
	/* Called once every tick */
	virtual void MainLoop() {}
};

class SoundDriverFactoryBase: public DriverFactoryBase {
};

template <class T>
class SoundDriverFactory: public SoundDriverFactoryBase {
public:
	SoundDriverFactory() { this->RegisterDriver(((T *)this)->GetName(), Driver::DT_SOUND, ((T *)this)->priority); }

	/**
	 * Get the long, human readable, name for the Driver-class.
	 */
	const char *GetName();
};

extern SoundDriver *_sound_driver;
extern char *_ini_sounddriver;

#endif /* SOUND_SOUND_DRIVER_HPP */
