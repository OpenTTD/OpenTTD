/* $Id$ */

/** @file allegro_m.h Base support for playing music via allegro. */

#ifndef MUSIC_ALLEGRO_H
#define MUSIC_ALLEGRO_H

#include "music_driver.hpp"

class MusicDriver_Allegro: public MusicDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void PlaySong(const char *filename);

	/* virtual */ void StopSong();

	/* virtual */ bool IsSongPlaying();

	/* virtual */ void SetVolume(byte vol);
};

class FMusicDriver_Allegro: public MusicDriverFactory<FMusicDriver_Allegro> {
public:
	static const int priority = 1;
	/* virtual */ const char *GetName() { return "allegro"; }
	/* virtual */ const char *GetDescription() { return "Allegro MIDI Driver"; }
	/* virtual */ Driver *CreateInstance() { return new MusicDriver_Allegro(); }
};

#endif /* MUSIC_ALLEGRO_H */
