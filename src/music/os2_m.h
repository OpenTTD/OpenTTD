/* $Id$ */

#ifndef MUSIC_OS2_H
#define MUSIC_OS2_H

#include "music_driver.hpp"

class MusicDriver_OS2: public MusicDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void PlaySong(const char *filename);

	/* virtual */ void StopSong();

	/* virtual */ bool IsSongPlaying();

	/* virtual */ void SetVolume(byte vol);
};

class FMusicDriver_OS2: public MusicDriverFactory<FMusicDriver_OS2> {
public:
	static const int priority = 10;
	/* virtual */ const char *GetName() { return "os2"; }
	/* virtual */ const char *GetDescription() { return "OS/2 Music Driver"; }
	/* virtual */ Driver *CreateInstance() { return new MusicDriver_OS2(); }
};

#endif /* MUSIC_OS2_H */
