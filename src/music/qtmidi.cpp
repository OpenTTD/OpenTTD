/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file qtmidi.cpp
 * @brief MIDI music player for MacOS X using QuickTime.
 *
 * This music player should work in all MacOS X releases starting from 10.0,
 * as QuickTime is an integral part of the system since the old days of the
 * Motorola 68k-based Macintoshes. The only extra dependency apart from
 * QuickTime itself is Carbon, which is included since 10.0 as well.
 *
 * QuickTime gets fooled with the MIDI files from Transport Tycoon Deluxe
 * because of the @c .gm suffix. To force QuickTime to load the MIDI files
 * without the need of dealing with the individual QuickTime components
 * needed to play music (data source, MIDI parser, note allocators,
 * synthesizers and the like) some Carbon functions are used to set the file
 * type as seen by QuickTime, using @c FSpSetFInfo() (which modifies the
 * file's resource fork).
 */


#ifndef NO_QUICKTIME

#include "../stdafx.h"
#include "qtmidi.h"
#include "../debug.h"

#define Rect  OTTDRect
#define Point OTTDPoint
#include <QuickTime/QuickTime.h>
#undef Rect
#undef Point

#include "../safeguards.h"

static FMusicDriver_QtMidi iFMusicDriver_QtMidi;


static const uint MIDI_TYPE = 'Midi'; ///< OSType code for MIDI songs.


/**
 * Sets the @c OSType of a given file to @c 'Midi', but only if it's not
 * already set.
 *
 * @param *ref A @c FSSpec structure referencing a file.
 */
static void SetMIDITypeIfNeeded(const FSRef *ref)
{
	FSCatalogInfo catalogInfo;

	assert(ref);

	if (noErr != FSGetCatalogInfo(ref, kFSCatInfoNodeFlags | kFSCatInfoFinderInfo, &catalogInfo, NULL, NULL, NULL)) return;
	if (!(catalogInfo.nodeFlags & kFSNodeIsDirectoryMask)) {
		FileInfo * const info = (FileInfo *) catalogInfo.finderInfo;
		if (info->fileType != MIDI_TYPE && !(info->finderFlags & kIsAlias)) {
			OSErr e;
			info->fileType = MIDI_TYPE;
			e = FSSetCatalogInfo(ref, kFSCatInfoFinderInfo, &catalogInfo);
			if (e == noErr) {
				DEBUG(driver, 3, "qtmidi: changed filetype to 'Midi'");
			} else {
				DEBUG(driver, 0, "qtmidi: changing filetype to 'Midi' failed - error %d", e);
			}
		}
	}
}


/**
 * Loads a MIDI file and returns it as a QuickTime Movie structure.
 *
 * @param *path String with the path of an existing MIDI file.
 * @param *moov Pointer to a @c Movie where the result will be stored.
 * @return Whether the file was loaded and the @c Movie successfully created.
 */
static bool LoadMovieForMIDIFile(const char *path, Movie *moov)
{
	int fd;
	int ret;
	char magic[4];
	FSRef fsref;
	FSSpec fsspec;
	short refnum = 0;
	short resid  = 0;

	assert(path != NULL);
	assert(moov != NULL);

	DEBUG(driver, 2, "qtmidi: start loading '%s'...", path);

	/*
	 * XXX Manual check for MIDI header ('MThd'), as I don't know how to make
	 * QuickTime load MIDI files without a .mid suffix without knowing it's
	 * a MIDI file and setting the OSType of the file to the 'Midi' value.
	 * Perhaps ugly, but it seems that it does the Right Thing(tm).
	 */
	fd = open(path, O_RDONLY, 0);
	if (fd == -1) return false;
	ret = read(fd, magic, 4);
	close(fd);
	if (ret < 4) return false;

	DEBUG(driver, 3, "qtmidi: header is '%.4s'", magic);
	if (magic[0] != 'M' || magic[1] != 'T' || magic[2] != 'h' || magic[3] != 'd') {
		return false;
	}

	if (noErr != FSPathMakeRef((const UInt8 *) path, &fsref, NULL)) return false;
	SetMIDITypeIfNeeded(&fsref);

	if (noErr != FSGetCatalogInfo(&fsref, kFSCatInfoNone, NULL, NULL, &fsspec, NULL)) return false;
	if (OpenMovieFile(&fsspec, &refnum, fsRdPerm) != noErr) return false;
	DEBUG(driver, 3, "qtmidi: '%s' successfully opened", path);

	if (noErr != NewMovieFromFile(moov, refnum, &resid, NULL,
				newMovieActive | newMovieDontAskUnresolvedDataRefs, NULL)) {
		CloseMovieFile(refnum);
		return false;
	}
	DEBUG(driver, 3, "qtmidi: movie container created");

	CloseMovieFile(refnum);
	return true;
}


/**
 * Flag which has the @c true value when QuickTime is available and
 * initialized.
 */
static bool _quicktime_started = false;


/**
 * Initialize QuickTime if needed. This function sets the
 * #_quicktime_started flag to @c true if QuickTime is present in the system
 * and it was initialized properly.
 */
static void InitQuickTimeIfNeeded()
{
	OSStatus dummy;

	if (_quicktime_started) return;

	DEBUG(driver, 2, "qtmidi: initializing Quicktime");
	/* Be polite: check wether QuickTime is available and initialize it. */
	_quicktime_started =
		(noErr == Gestalt(gestaltQuickTime, &dummy)) &&
		(noErr == EnterMovies());
	if (!_quicktime_started) DEBUG(driver, 0, "qtmidi: Quicktime initialization failed!");
}


/** Possible states of the QuickTime music driver. */
enum QTStates {
	QT_STATE_IDLE, ///< No file loaded.
	QT_STATE_PLAY, ///< File loaded, playing.
	QT_STATE_STOP, ///< File loaded, stopped.
};


static Movie _quicktime_movie;                  ///< Current QuickTime @c Movie.
static byte  _quicktime_volume = 127;           ///< Current volume.
static int   _quicktime_state  = QT_STATE_IDLE; ///< Current player state.


/**
 * Maps OpenTTD volume to QuickTime notion of volume.
 */
#define VOLUME  ((short)((0x00FF & _quicktime_volume) << 1))


/**
 * Initialized the MIDI player, including QuickTime initialization.
 *
 * @todo Give better error messages by inspecting error codes returned by
 * @c Gestalt() and @c EnterMovies(). Needs changes in
 * #InitQuickTimeIfNeeded.
 */
const char *MusicDriver_QtMidi::Start(const char * const *parm)
{
	InitQuickTimeIfNeeded();
	return (_quicktime_started) ? NULL : "can't initialize QuickTime";
}


/**
 * Checks whether the player is active.
 *
 * This function is called at regular intervals from OpenTTD's main loop, so
 * we call @c MoviesTask() from here to let QuickTime do its work.
 */
bool MusicDriver_QtMidi::IsSongPlaying()
{
	if (!_quicktime_started) return true;

	switch (_quicktime_state) {
		case QT_STATE_IDLE:
		case QT_STATE_STOP:
			/* Do nothing. */
			break;

		case QT_STATE_PLAY:
			MoviesTask(_quicktime_movie, 0);
			/* Check wether movie ended. */
			if (IsMovieDone(_quicktime_movie) ||
					(GetMovieTime(_quicktime_movie, NULL) >=
					 GetMovieDuration(_quicktime_movie))) {
				_quicktime_state = QT_STATE_STOP;
			}
	}

	return _quicktime_state == QT_STATE_PLAY;
}


/**
 * Stops the MIDI player.
 *
 * Stops playing and frees any used resources before returning. As it
 * deinitilizes QuickTime, the #_quicktime_started flag is set to @c false.
 */
void MusicDriver_QtMidi::Stop()
{
	if (!_quicktime_started) return;

	DEBUG(driver, 2, "qtmidi: stopping driver...");
	switch (_quicktime_state) {
		case QT_STATE_IDLE:
			DEBUG(driver, 3, "qtmidi: stopping not needed, already idle");
			/* Do nothing. */
			break;

		case QT_STATE_PLAY:
			StopSong();
			FALLTHROUGH;

		case QT_STATE_STOP:
			DisposeMovie(_quicktime_movie);
	}

	ExitMovies();
	_quicktime_started = false;
}


/**
 * Starts playing a new song.
 *
 * @param filename Path to a MIDI file.
 */
void MusicDriver_QtMidi::PlaySong(const char *filename)
{
	if (!_quicktime_started) return;

	DEBUG(driver, 2, "qtmidi: trying to play '%s'", filename);
	switch (_quicktime_state) {
		case QT_STATE_PLAY:
			StopSong();
			DEBUG(driver, 3, "qtmidi: previous tune stopped");
			FALLTHROUGH;

		case QT_STATE_STOP:
			DisposeMovie(_quicktime_movie);
			DEBUG(driver, 3, "qtmidi: previous tune disposed");
			_quicktime_state = QT_STATE_IDLE;
			FALLTHROUGH;

		case QT_STATE_IDLE:
			LoadMovieForMIDIFile(filename, &_quicktime_movie);
			SetMovieVolume(_quicktime_movie, VOLUME);
			StartMovie(_quicktime_movie);
			_quicktime_state = QT_STATE_PLAY;
	}
	DEBUG(driver, 3, "qtmidi: playing '%s'", filename);
}


/**
 * Stops playing the current song, if the player is active.
 */
void MusicDriver_QtMidi::StopSong()
{
	if (!_quicktime_started) return;

	switch (_quicktime_state) {
		case QT_STATE_IDLE:
			FALLTHROUGH;

		case QT_STATE_STOP:
			DEBUG(driver, 3, "qtmidi: stop requested, but already idle");
			/* Do nothing. */
			break;

		case QT_STATE_PLAY:
			StopMovie(_quicktime_movie);
			_quicktime_state = QT_STATE_STOP;
			DEBUG(driver, 3, "qtmidi: player stopped");
	}
}


/**
 * Changes the playing volume of the MIDI player.
 *
 * As QuickTime controls volume in a per-movie basis, the desired volume is
 * stored in #_quicktime_volume, and the volume is set here using the
 * #VOLUME macro, @b and when loading new song in #PlaySong.
 *
 * @param vol The desired volume, range of the value is @c 0-127
 */
void MusicDriver_QtMidi::SetVolume(byte vol)
{
	if (!_quicktime_started) return;

	_quicktime_volume = vol;

	DEBUG(driver, 2, "qtmidi: set volume to %u (%hi)", vol, VOLUME);
	switch (_quicktime_state) {
		case QT_STATE_IDLE:
			/* Do nothing. */
			break;

		case QT_STATE_PLAY:
		case QT_STATE_STOP:
			SetMovieVolume(_quicktime_movie, VOLUME);
	}
}

#endif /* NO_QUICKTIME */
