#ifdef __BEOS__

#include "stdafx.h"
#include "ttd.h"
#include "hal.h"

// BeOS System Includes
#include <MidiSynthFile.h>

BMidiSynthFile midiSynthFile;

static char *bemidi_start(char **parm) {
    return NULL;
}

static void bemidi_stop(void) {
  	midiSynthFile.UnloadFile();
}

static void bemidi_play_song(const char *filename) {
	bemidi_stop();
	entry_ref midiRef;
	get_ref_for_path(filename, &midiRef);
	midiSynthFile.LoadFile(&midiRef);
	midiSynthFile.Start();
}

static void bemidi_stop_song(void) {
	midiSynthFile.UnloadFile();
}

static bool bemidi_is_playing(void) {
	if(midiSynthFile.IsFinished() == true)
	{
	return 0;
	} else {
	return 1;
	}
}


static void bemidi_set_volume(byte vol) {
    fprintf(stderr, "BeMidi: Set volume not implemented\n");
}

const HalMusicDriver _bemidi_music_driver = {
	bemidi_start,
	bemidi_stop,
	bemidi_play_song,
	bemidi_stop_song,
	bemidi_is_playing,
	bemidi_set_volume,
};

#endif // __BEOS__
