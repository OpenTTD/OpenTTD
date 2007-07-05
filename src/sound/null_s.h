/* $Id$ */

#ifndef SOUND_NULL_H
#define SOUND_NULL_H

#include "sound_driver.hpp"

class SoundDriver_Null: public SoundDriver {
public:
	/* virtual */ bool CanProbe() { return false; }

	/* virtual */ const char *Start(const char * const *param) { return NULL; }

	/* virtual */ void Stop() { }
};

class FSoundDriver_Null: public SoundDriverFactory<FSoundDriver_Null> {
public:
	/* virtual */ const char *GetName() { return "null"; }
	/* virtual */ const char *GetDescription() { return "Null Sound Driver"; }
	/* virtual */ Driver *CreateInstance() { return new SoundDriver_Null(); }
};

#endif /* SOUND_NULL_H */
