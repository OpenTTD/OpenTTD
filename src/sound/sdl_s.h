/* $Id$ */

/** @file sdl_s.h Base fo playing sound via SDL. */

#ifndef SOUND_SDL_H
#define SOUND_SDL_H

#include "sound_driver.hpp"

class SoundDriver_SDL: public SoundDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();
};

class FSoundDriver_SDL: public SoundDriverFactory<FSoundDriver_SDL> {
public:
	static const int priority = 5;
	/* virtual */ const char *GetName() { return "sdl"; }
	/* virtual */ const char *GetDescription() { return "SDL Sound Driver"; }
	/* virtual */ Driver *CreateInstance() { return new SoundDriver_SDL(); }
};

#endif /* SOUND_SDL_H */
