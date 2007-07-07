/* $Id$ */

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

#endif /* SOUND_SOUND_DRIVER_HPP */
