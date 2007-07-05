/* $Id$ */

#ifndef SOUND_SDL_H
#define SOUND_SDL_H

#include "sound_driver.hpp"

class SoundDriver_SDL: public SoundDriver {
public:
	/* virtual */ bool CanProbe() { return true; }

	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();
};

class FSoundDriver_SDL: public SoundDriverFactory<FSoundDriver_SDL> {
public:
	/* virtual */ const char *GetName() { return "sdl"; }
	/* virtual */ const char *GetDescription() { return "SDL Sound Driver"; }
	/* virtual */ Driver *CreateInstance() { return new SoundDriver_SDL(); }
};

#endif /* SOUND_SDL_H */
