/* $Id$ */

#ifndef SOUND_WIN32_H
#define SOUND_WIN32_H

#include "sound_driver.hpp"

class SoundDriver_Win32: public SoundDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();
};

class FSoundDriver_Win32: public SoundDriverFactory<FSoundDriver_Win32> {
public:
	static const int priority = 10;
	/* virtual */ const char *GetName() { return "win32"; }
	/* virtual */ const char *GetDescription() { return "Win32 WaveOut Driver"; }
	/* virtual */ Driver *CreateInstance() { return new SoundDriver_Win32(); }
};

#endif /* SOUND_WIN32_H */
