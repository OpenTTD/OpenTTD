/* $Id$ */

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
	MusicDriverFactory() { this->RegisterDriver(((T *)this)->GetName(), Driver::DT_MUSIC); }

	/**
	 * Get the long, human readable, name for the Driver-class.
	 */
	const char *GetName();
};

extern MusicDriver *_music_driver;

#endif /* MUSIC_MUSIC_DRIVER_HPP */
