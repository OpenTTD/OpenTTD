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

/** Driver for all music playback. */
class MusicDriver: public Driver {
public:
	/**
	 * Play a particular song.
	 * @param filename The name of file with the song to play.
	 */
	virtual void PlaySong(const char *filename) = 0;

	/**
	 * Stop playing the current song.
	 */
	virtual void StopSong() = 0;

	/**
	 * Are we currently playing a song?
	 * @return True if a song is being played.
	 */
	virtual bool IsSongPlaying() = 0;

	/**
	 * Set the volume, if possible.
	 * @param vol The new volume.
	 */
	virtual void SetVolume(byte vol) = 0;
};

/** Base of the factory for the music drivers. */
class MusicDriverFactoryBase: public DriverFactoryBase {
};

/**
 * Factory for the music drivers.
 * @tparam T The type of the music factory to register.
 */
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
