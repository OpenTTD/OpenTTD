/* $Id$ */

#ifndef MUSIC_WIN32_H
#define MUSIC_WIN32_H

#include "music_driver.hpp"

class MusicDriver_Win32: public MusicDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void PlaySong(const char *filename);

	/* virtual */ void StopSong();

	/* virtual */ bool IsSongPlaying();

	/* virtual */ void SetVolume(byte vol);
};

class FMusicDriver_Win32: public MusicDriverFactory<FMusicDriver_Win32> {
public:
	static const int priority = 5;
	/* virtual */ const char *GetName() { return "win32"; }
	/* virtual */ const char *GetDescription() { return "Win32 Music Driver"; }
	/* virtual */ Driver *CreateInstance() { return new MusicDriver_Win32(); }
};

#endif /* MUSIC_WIN32_H */
