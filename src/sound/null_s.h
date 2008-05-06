/* $Id$ */

/** @file null_s.h Base for the sound of silence. */

#ifndef SOUND_NULL_H
#define SOUND_NULL_H

#include "sound_driver.hpp"

class SoundDriver_Null: public SoundDriver {
public:
	/* virtual */ const char *Start(const char * const *param) { return NULL; }

	/* virtual */ void Stop() { }
};

class FSoundDriver_Null: public SoundDriverFactory<FSoundDriver_Null> {
public:
	static const int priority = 0;
	/* virtual */ const char *GetName() { return "null"; }
	/* virtual */ const char *GetDescription() { return "Null Sound Driver"; }
	/* virtual */ Driver *CreateInstance() { return new SoundDriver_Null(); }
};

#endif /* SOUND_NULL_H */
