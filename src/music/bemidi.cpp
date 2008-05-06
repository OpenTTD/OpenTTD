/* $Id$ */

/** @file bemidi.cpp Support for BeOS midi. */

#include "../stdafx.h"
#include "../openttd.h"
#include "bemidi.h"

// BeOS System Includes
#include <MidiSynthFile.h>

static BMidiSynthFile midiSynthFile;

static FMusicDriver_BeMidi iFMusicDriver_BeMidi;

const char *MusicDriver_BeMidi::Start(const char * const *parm)
{
	return NULL;
}

void MusicDriver_BeMidi::Stop()
{
	midiSynthFile.UnloadFile();
}

void MusicDriver_BeMidi::PlaySong(const char *filename)
{
	bemidi_stop();
	entry_ref midiRef;
	get_ref_for_path(filename, &midiRef);
	midiSynthFile.LoadFile(&midiRef);
	midiSynthFile.Start();
}

void MusicDriver_BeMidi::StopSong()
{
	midiSynthFile.UnloadFile();
}

bool MusicDriver_BeMidi::IsSongPlaying()
{
	return !midiSynthFile.IsFinished();
}

void MusicDriver_BeMidi::SetVolume(byte vol)
{
	fprintf(stderr, "BeMidi: Set volume not implemented\n");
}
