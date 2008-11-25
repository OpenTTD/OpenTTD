/* $Id$ */

/** @file allegro_s.h Base fo playing sound via Allegro. */

#ifndef SOUND_ALLEGRO_H
#define SOUND_ALLEGRO_H

#include "sound_driver.hpp"

class SoundDriver_Allegro: public SoundDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void MainLoop();
};

class FSoundDriver_Allegro: public SoundDriverFactory<FSoundDriver_Allegro> {
public:
	static const int priority = 5;
	/* virtual */ const char *GetName() { return "allegro"; }
	/* virtual */ const char *GetDescription() { return "Allegro Sound Driver"; }
	/* virtual */ Driver *CreateInstance() { return new SoundDriver_Allegro(); }
};

#endif /* SOUND_ALLEGRO_H */
