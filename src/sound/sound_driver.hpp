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

/** Base for all sound drivers. */
class SoundDriver : public Driver {
public:
	/** Called once every tick */
	virtual void MainLoop() {}

	/**
	 * Whether the driver has an output from which the user can hear sound.
	 * Or in other words, whether we should warn the user if no soundset is
	 * loaded and that loading one would fix the sound problems.
	 * @return True for all drivers except null.
	 */
	virtual bool HasOutput() const
	{
		return true;
	}

	/**
	 * Get the currently active instance of the sound driver.
	 */
	static SoundDriver *GetInstance()
	{
		return static_cast<SoundDriver*>(*DriverFactoryBase::GetActiveDriver(Driver::DT_SOUND));
	}
};

extern std::string _ini_sounddriver;

#endif /* SOUND_SOUND_DRIVER_HPP */
