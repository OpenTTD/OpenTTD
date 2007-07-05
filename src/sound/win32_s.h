/* $Id$ */

#ifndef SOUND_WIN32_H
#define SOUND_WIN32_H

#include "sound_driver.hpp"

class SoundDriver_Win32: public SoundDriver {
public:
	/* virtual */ bool CanProbe() { return true; }

	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();
};

class FSoundDriver_Win32: public SoundDriverFactory<FSoundDriver_Win32> {
public:
	/* virtual */ const char *GetName() { return "win32"; }
	/* virtual */ const char *GetDescription() { return "Win32 Sound Driver"; }
	/* virtual */ Driver *CreateInstance() { return new SoundDriver_Win32(); }
};

#endif /* SOUND_WIN32_H */
