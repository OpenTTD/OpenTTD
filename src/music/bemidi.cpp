/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bemidi.cpp Support for BeOS midi. */

#include "../stdafx.h"
#include "../openttd.h"
#include "bemidi.h"
#include "../base_media_base.h"
#include "midifile.hpp"

/* BeOS System Includes */
#include <MidiSynthFile.h>

#include "../safeguards.h"

/** The file we're playing. */
static BMidiSynthFile midiSynthFile;

/** Factory for BeOS' midi player. */
static FMusicDriver_BeMidi iFMusicDriver_BeMidi;

const char *MusicDriver_BeMidi::Start(const char * const *parm)
{
	return NULL;
}

void MusicDriver_BeMidi::Stop()
{
	midiSynthFile.UnloadFile();
}

void MusicDriver_BeMidi::PlaySong(const MusicSongInfo &song)
{
	std::string filename = MidiFile::GetSMFFile(song);

	this->Stop();
	if (!filename.empty()) {
		entry_ref midiRef;
		get_ref_for_path(filename.c_str(), &midiRef);
		midiSynthFile.LoadFile(&midiRef);
		midiSynthFile.Start();
	}
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
