/* $Id$ */

/** @file sound_driver.hpp Base for all sound drivers. */

#ifndef SOUND_SOUND_DRIVER_HPP
#define SOUND_SOUND_DRIVER_HPP

#include "../driver.h"

class SoundDriver: public Driver {
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
extern char _ini_sounddriver[32];

#endif /* SOUND_SOUND_DRIVER_HPP */
