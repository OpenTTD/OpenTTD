/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file music_driver.hpp Base for all music playback. */

#ifndef MUSIC_MUSIC_DRIVER_HPP
#define MUSIC_MUSIC_DRIVER_HPP

#include "../driver.h"

class MusicDriver: public Driver {
public:
	virtual void PlaySong(const char *filename) = 0;

	virtual void StopSong() = 0;

	virtual bool IsSongPlaying() = 0;

	virtual void SetVolume(byte vol) = 0;
};

class MusicDriverFactoryBase: public DriverFactoryBase {
};

template <class T>
class MusicDriverFactory: public MusicDriverFactoryBase {
public:
	MusicDriverFactory() { this->RegisterDriver(((T *)this)->GetName(), Driver::DT_MUSIC, ((T *)this)->priority); }

	/**
	 * Get the long, human readable, name for the Driver-class.
	 */
	const char *GetName();
};

extern MusicDriver *_music_driver;
extern char *_ini_musicdriver;

#endif /* MUSIC_MUSIC_DRIVER_HPP */
