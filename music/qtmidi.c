/* $Id$ */

/**
 * @file qtmidi.c
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


/*
 * OpenTTD includes.
 */
#define  WindowClass OSX_WindowClass
#include <QuickTime/QuickTime.h>
#undef   WindowClass

#include "../stdafx.h"
#include "../openttd.h"
#include "qtmidi.h"

/*
 * System includes. We need to workaround with some defines because there's
 * stuff already defined in QuickTime headers.
 */
#define  OTTD_Random OSX_OTTD_Random
#undef   OTTD_Random
#undef   WindowClass
#undef   SL_ERROR
#undef   bool

#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

// we need to include debug.h after CoreServices because defining DEBUG will break CoreServices in OSX 10.2
#include "../debug.h"


enum {
	midiType = 'Midi' /**< OSType code for MIDI songs. */
};


/**
 * Converts a Unix-like pathname to a @c FSSpec structure which may be
 * used with functions from several MacOS X frameworks (Carbon, QuickTime,
 * etc). The pointed file or directory must exist.
 *
 * @param *path A string containing a Unix-like path.
 * @param *spec Pointer to a @c FSSpec structure where the result will be
 *              stored.
 * @return Wether the conversion was successful.
 */
static bool PathToFSSpec(const char *path, FSSpec *spec)
{
	FSRef ref;
	assert(spec != NULL);
	assert(path != NULL);

	return
		FSPathMakeRef((UInt8*)path, &ref, NULL) == noErr &&
		FSGetCatalogInfo(&ref, kFSCatInfoNone, NULL, NULL, spec, NULL) == noErr;
}


/**
 * Sets the @c OSType of a given file to @c 'Midi', but only if it's not
 * already set.
 *
 * @param *spec A @c FSSpec structure referencing a file.
 */
static void SetMIDITypeIfNeeded(const FSSpec *spec)
{
	FInfo info;
	assert(spec);

	if (noErr != FSpGetFInfo(spec, &info)) return;

	/* Set file type to 'Midi' if the file is _not_ an alias. */
	if (info.fdType != midiType && !(info.fdFlags & kIsAlias)) {
		info.fdType = midiType;
		FSpSetFInfo(spec, &info);
		DEBUG(driver, 3, "qtmidi: changed filetype to 'Midi'");
	}
}


/**
 * Loads a MIDI file and returns it as a QuickTime Movie structure.
 *
 * @param *path String with the path of an existing MIDI file.
 * @param *moov Pointer to a @c Movie where the result will be stored.
 * @return Wether the file was loaded and the @c Movie successfully created.
 */
static bool LoadMovieForMIDIFile(const char *path, Movie *moov)
{
	int fd;
	int ret;
	char magic[4];
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
	 * Perhahaps ugly, but it seems that it does the Right Thing(tm).
	 */
	fd = open(path, O_RDONLY, 0);
	if (fd == -1) return false;
	ret = read(fd, magic, 4);
	close(fd);
	if (ret < 4) return false;

	DEBUG(driver, 3, "qtmidi: header is '%.4s'", magic);
	if (magic[0] != 'M' || magic[1] != 'T' || magic[2] != 'h' || magic[3] != 'd')
		return false;

	if (!PathToFSSpec(path, &fsspec)) return false;
	SetMIDITypeIfNeeded(&fsspec);

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
static void InitQuickTimeIfNeeded(void)
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
enum {
	QT_STATE_IDLE, /**< No file loaded. */
	QT_STATE_PLAY, /**< File loaded, playing. */
	QT_STATE_STOP, /**< File loaded, stopped. */
};


static Movie _quicktime_movie;                  /**< Current QuickTime @c Movie. */
static byte  _quicktime_volume = 127;           /**< Current volume. */
static int   _quicktime_state  = QT_STATE_IDLE; /**< Current player state. */


/**
 * Maps OpenTTD volume to QuickTime notion of volume.
 */
#define VOLUME  ((short)((0x00FF & _quicktime_volume) << 1))


static void StopSong(void);


/**
 * Initialized the MIDI player, including QuickTime initialization.
 *
 * @todo Give better error messages by inspecting error codes returned by
 * @c Gestalt() and @c EnterMovies(). Needs changes in
 * #InitQuickTimeIfNeeded.
 */
static const char* StartDriver(const char * const *parm)
{
	InitQuickTimeIfNeeded();
	return (_quicktime_started) ? NULL : "can't initialize QuickTime";
}


/**
 * Checks wether the player is active.
 *
 * This function is called at regular intervals from OpenTTD's main loop, so
 * we call @c MoviesTask() from here to let QuickTime do its work.
 */
static bool SongIsPlaying(void)
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
					 GetMovieDuration(_quicktime_movie)))
				_quicktime_state = QT_STATE_STOP;
	}

	return _quicktime_state == QT_STATE_PLAY;
}


/**
 * Stops the MIDI player.
 *
 * Stops playing and frees any used resources before returning. As it
 * deinitilizes QuickTime, the #_quicktime_started flag is set to @c false.
 */
static void StopDriver(void)
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
static void PlaySong(const char *filename)
{
	if (!_quicktime_started) return;

	DEBUG(driver, 2, "qtmidi: trying to play '%s'", filename);
	switch (_quicktime_state) {
		case QT_STATE_PLAY:
			StopSong();
			DEBUG(driver, 3, "qtmidi: previous tune stopped");
			/* XXX Fall-through -- no break needed. */
		case QT_STATE_STOP:
			DisposeMovie(_quicktime_movie);
			DEBUG(driver, 3, "qtmidi: previous tune disposed");
			_quicktime_state = QT_STATE_IDLE;
			/* XXX Fall-through -- no break needed. */
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
static void StopSong(void)
{
	if (!_quicktime_started) return;

	switch (_quicktime_state) {
		case QT_STATE_IDLE:
			/* XXX Fall-through -- no break needed. */
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
static void SetVolume(byte vol)
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


/**
 * Table of callbacks that implement the QuickTime MIDI player.
 */
const HalMusicDriver _qtime_music_driver = {
	StartDriver,
	StopDriver,
	PlaySong,
	StopSong,
	SongIsPlaying,
	SetVolume,
};
