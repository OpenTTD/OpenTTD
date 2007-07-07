/* $Id$ */

#ifndef MUSIC_BEMIDI_H
#define MUSIC_BEMIDI_H

#include "music_driver.hpp"

class MusicDriver_BeMidi: public MusicDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void PlaySong(const char *filename);

	/* virtual */ void StopSong();

	/* virtual */ bool IsSongPlaying();

	/* virtual */ void SetVolume(byte vol);
};

class FMusicDriver_BeMidi: public MusicDriverFactory<FMusicDriver_BeMidi> {
public:
	static const int priority = 10;
	/* virtual */ const char *GetName() { return "bemidi"; }
	/* virtual */ const char *GetDescription() { return "BeOS MIDI Driver"; }
	/* virtual */ Driver *CreateInstance() { return new MusicDriver_BeMidi(); }
};

#endif /* MUSIC_BEMIDI_H */
