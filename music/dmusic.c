/* $Id$ */

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

#include "openttd.h"
#include "string.h"
#include "variables.h"
#include "sound.h"
#include "music/dmusic.h"

static const char * DMusicMidiStart(const char * const *parm);
static void DMusicMidiStop(void);
static void DMusicMidiPlaySong(const char *filename);
static void DMusicMidiStopSong(void);
static bool DMusicMidiIsSongPlaying(void);
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

static bool seeking = false;

static const char * DMusicMidiStart(const char * const *parm)
{
	return InitDirectMusic() ? NULL : "Unable to initialize DirectMusic";
}

static void DMusicMidiStop(void)
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
	*pos = '\0';

	LoadMIDI(dir, file);
	PlaySegment();
	seeking = true;
}

static void DMusicMidiStopSong(void)
{
	StopSegment();
}

static bool DMusicMidiIsSongPlaying(void)
{
	/* Not the nicest code, but there is a short delay before playing actually 
	 * starts. OpenTTD makes no provision for this. */
	if (!IsSegmentPlaying() && seeking) return true;
	if (IsSegmentPlaying()) seeking = false;

	return IsSegmentPlaying();
}

static void DMusicMidiSetVolume(byte vol)
{
	SetVolume(vol);
}

#endif /* WIN32_ENABLE_DIRECTMUSIC_SUPPORT */
