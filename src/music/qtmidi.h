/* $Id$ */

#ifndef MUSIC_MACOSX_QUICKTIME_H
#define MUSIC_MACOSX_QUICKTIME_H

#include "music_driver.hpp"

class MusicDriver_QtMidi: public MusicDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void PlaySong(const char *filename);

	/* virtual */ void StopSong();

	/* virtual */ bool IsSongPlaying();

	/* virtual */ void SetVolume(byte vol);
};

class FMusicDriver_QtMidi: public MusicDriverFactory<FMusicDriver_QtMidi> {
public:
	static const int priorty = 10;
	/* virtual */ const char *GetName() { return "qt"; }
	/* virtual */ const char *GetDescription() { return "QuickTime MIDI Driver"; }
	/* virtual */ Driver *CreateInstance() { return new MusicDriver_QtMidi(); }
};

#endif /* MUSIC_MACOSX_QUICKTIME_H */
