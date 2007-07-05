/* $Id$ */

#ifndef MUSIC_EXTERNAL_H
#define MUSIC_EXTERNAL_H

#include "music_driver.hpp"

class MusicDriver_ExtMidi: public MusicDriver {
private:
	char song[MAX_PATH];
	pid_t pid;

	void DoPlay();
	void DoStop();

public:
	/* virtual */ bool CanProbe() { return true; }

	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void PlaySong(const char *filename);

	/* virtual */ void StopSong();

	/* virtual */ bool IsSongPlaying();

	/* virtual */ void SetVolume(byte vol);
};

class FMusicDriver_ExtMidi: public MusicDriverFactory<FMusicDriver_ExtMidi> {
public:
	/* virtual */ const char *GetName() { return "extmidi"; }
	/* virtual */ const char *GetDescription() { return "External MIDI Driver"; }
	/* virtual */ Driver *CreateInstance() { return new MusicDriver_ExtMidi(); }
};

#endif /* MUSIC_EXTERNAL_H */
