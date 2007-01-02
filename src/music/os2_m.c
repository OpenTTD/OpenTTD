/* $Id$ */

#include "../stdafx.h"
#include "../openttd.h"
#include "os2_m.h"

#define INCL_DOS
#define INCL_OS2MM
#define INCL_WIN

#include <stdarg.h>
#include <os2.h>
#include <os2me.h>

/**********************
 * OS/2 MIDI PLAYER
 **********************/

/* Interesting how similar the MCI API in OS/2 is to the Win32 MCI API,
 * eh? Anyone would think they both came from the same place originally! ;)
 */

static long CDECL MidiSendCommand(const char *cmd, ...)
{
	va_list va;
	char buf[512];
	va_start(va, cmd);
	vsprintf(buf, cmd, va);
	va_end(va);
	return mciSendString(buf, NULL, 0, NULL, 0);
}

static void OS2MidiPlaySong(const char *filename)
{
	MidiSendCommand("close all");

	if (MidiSendCommand("open %s type sequencer alias song", filename) != 0)
		return;

	MidiSendCommand("play song from 0");
}

static void OS2MidiStopSong(void)
{
	MidiSendCommand("close all");
}

static void OS2MidiSetVolume(byte vol)
{
	MidiSendCommand("set song audio volume %d", ((vol/127)*100));
}

static bool OS2MidiIsSongPlaying(void)
{
	char buf[16];
	mciSendString("status song mode", buf, sizeof(buf), NULL, 0);
	return strcmp(buf, "playing") == 0 || strcmp(buf, "seeking") == 0;
}

static const char *OS2MidiStart(const char * const *parm)
{
	return 0;
}

static void OS2MidiStop(void)
{
	MidiSendCommand("close all");
}

const HalMusicDriver _os2_music_driver = {
	OS2MidiStart,
	OS2MidiStop,
	OS2MidiPlaySong,
	OS2MidiStopSong,
	OS2MidiIsSongPlaying,
	OS2MidiSetVolume,
};
