/* $Id$ */

/** @file dmusic.h Base of playing music via DirectMusic.

#ifndef MUSIC_DMUSIC_H
#define MUSIC_DMUSIC_H

#include "music_driver.hpp"

class MusicDriver_DMusic: public MusicDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void PlaySong(const char *filename);

	/* virtual */ void StopSong();

	/* virtual */ bool IsSongPlaying();

	/* virtual */ void SetVolume(byte vol);
};

class FMusicDriver_DMusic: public MusicDriverFactory<FMusicDriver_DMusic> {
public:
	static const int priority = 10;
	/* virtual */ const char *GetName() { return "dmusic"; }
	/* virtual */ const char *GetDescription() { return "DirectMusic MIDI Driver"; }
	/* virtual */ Driver *CreateInstance() { return new MusicDriver_DMusic(); }
};

#endif /* MUSIC_DMUSIC_H */
