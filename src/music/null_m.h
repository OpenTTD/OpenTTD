/* $Id$ */

#ifndef MUSIC_NULL_H
#define MUSIC_NULL_H

#include "music_driver.hpp"

class MusicDriver_Null: public MusicDriver {
public:
	/* virtual */ const char *Start(const char * const *param) { return NULL; }

	/* virtual */ void Stop() { }

	/* virtual */ void PlaySong(const char *filename) { }

	/* virtual */ void StopSong() { }

	/* virtual */ bool IsSongPlaying() { return true; }

	/* virtual */ void SetVolume(byte vol) { }
};

class FMusicDriver_Null: public MusicDriverFactory<FMusicDriver_Null> {
public:
	static const int priority = 0;
	/* virtual */ const char *GetName() { return "null"; }
	/* virtual */ const char *GetDescription() { return "Null Music Driver"; }
	/* virtual */ Driver *CreateInstance() { return new MusicDriver_Null(); }
};

#endif /* MUSIC_NULL_H */
