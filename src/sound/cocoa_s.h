/* $Id$ */

/** @file cocoa_s.h Base for Cocoa sound handling. */

#ifndef SOUND_COCOA_H
#define SOUND_COCOA_H

#include "sound_driver.hpp"

class SoundDriver_Cocoa: public SoundDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();
};

class FSoundDriver_Cocoa: public SoundDriverFactory<FSoundDriver_Cocoa> {
public:
	static const int priority = 10;
	/* virtual */ const char *GetName() { return "cocoa"; }
	/* virtual */ const char *GetDescription() { return "Cocoa Sound Driver"; }
	/* virtual */ Driver *CreateInstance() { return new SoundDriver_Cocoa(); }
};

#endif /* SOUND_COCOA_H */
