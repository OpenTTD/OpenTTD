/* $Id$ */

#ifndef SOUND_COCOA_H
#define SOUND_COCOA_H

#include "sound_driver.hpp"

class SoundDriver_Cocoa: public SoundDriver {
public:
	/* virtual */ bool CanProbe() { return true; }

	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();
};

class FSoundDriver_Cocoa: public SoundDriverFactory<FSoundDriver_Cocoa> {
public:
	/* virtual */ const char *GetName() { return "cocoa"; }
	/* virtual */ const char *GetDescription() { return "Cocoa Sound Driver"; }
	/* virtual */ Driver *CreateInstance() { return new SoundDriver_Cocoa(); }
};

#endif /* SOUND_COCOA_H */
