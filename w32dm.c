/*********************************************************************
 * OpenTTD: An Open Source Transport Tycoon Deluxe clone             *
 * Copyright (c) 2002-2004 OpenTTD Developers. All Rights Reserved.  *
 *                                                                   *
 * Web site: http://openttd.sourceforge.net/                         *
 *********************************************************************/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* DirectMusic driver for Win32 */
/* Based on dxmci from TTDPatch */

#include "stdafx.h"

#ifdef WIN32_ENABLE_DIRECTMUSIC_SUPPORT

#include "ttd.h"
#include "sound.h"
#include "hal.h"

static char * DMusicMidiStart(char **parm);
static void DMusicMidiStop();
static void DMusicMidiPlaySong(const char *filename);
static void DMusicMidiStopSong();
static bool DMusicMidiIsSongPlaying();
static void DMusicMidiSetVolume(byte vol);

const HalMusicDriver _dmusic_midi_driver = {
	DMusicMidiStart,
	DMusicMidiStop,
	DMusicMidiPlaySong,
	DMusicMidiStopSong,
	DMusicMidiIsSongPlaying,
	DMusicMidiSetVolume,
};

extern bool LoadMIDI (char *directory, char *filename);
extern bool InitDirectMusic (void);
extern void ReleaseSegment (void);
extern void ShutdownDirectMusic (void);
extern bool LoadMIDI (char *directory, char *filename);
extern void PlaySegment (void);
extern void StopSegment (void);
extern bool IsSegmentPlaying (void);
extern void SetVolume (long);

bool seeking = false;

static char * DMusicMidiStart(char **parm)
{
	InitDirectMusic();
	return 0;
}

static void DMusicMidiStop()
{
	StopSegment();
}

static void DMusicMidiPlaySong(const char *filename)
{
	char *pos;
	char dir[MAX_PATH];
	char file[MAX_PATH];

	// split full path into directory and file components
	ttd_strlcpy(dir, filename, MAX_PATH);
	pos = strrchr(dir, '\\') + 1;
	ttd_strlcpy(file, pos, MAX_PATH);
	//strchr(file, ' ')[0] = 0;
	*pos = 0;

	LoadMIDI(dir, file);
	PlaySegment();
	seeking = true;
}

static void DMusicMidiStopSong()
{
	StopSegment();
}

static bool DMusicMidiIsSongPlaying()
{
	if ((IsSegmentPlaying() == 0) && (seeking == true)) // Not the nicest code, but there is a
		return(true);                                   // short delay before playing actually
                                                        // starts. OpenTTD makes no provision for
	if (IsSegmentPlaying() == 1)                        // this.
		seeking = false;


	return(IsSegmentPlaying());
}

static void DMusicMidiSetVolume(byte vol)
{
	SetVolume(vol);
}

#endif