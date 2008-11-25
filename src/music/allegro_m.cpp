/* $Id$ */

/** @file allegro_m.cpp Playing music via allegro. */

#ifdef WITH_ALLEGRO

#include "../stdafx.h"
#include "../debug.h"
#include "allegro_m.h"
#include <allegro.h>

static FMusicDriver_Allegro iFMusicDriver_Allegro;
static MIDI *_midi = NULL;

/** There are multiple modules that might be using Allegro and
 * Allegro can only be initiated once. */
extern int _allegro_instance_count;

const char *MusicDriver_Allegro::Start(const char * const *param)
{
	if (_allegro_instance_count == 0 && install_allegro(SYSTEM_AUTODETECT, &errno, NULL)) return NULL;
	_allegro_instance_count++;

	/* Initialise the sound */
	if (install_sound(DIGI_AUTODETECT, MIDI_AUTODETECT, NULL) != 0) return NULL;

	/* Okay, there's no soundcard */
	if (midi_card == MIDI_NONE) {
		DEBUG(driver, 0, "allegro: no midi card found");
	}

	return NULL;
}

void MusicDriver_Allegro::Stop()
{
	if (_midi != NULL) destroy_midi(_midi);
	_midi = NULL;

	if (--_allegro_instance_count == 0) allegro_exit();
}

void MusicDriver_Allegro::PlaySong(const char *filename)
{
	if (_midi != NULL) destroy_midi(_midi);
	_midi = load_midi(filename);
	play_midi(_midi, false);
}

void MusicDriver_Allegro::StopSong()
{
	stop_midi();
}

bool MusicDriver_Allegro::IsSongPlaying()
{
	return midi_pos >= 0;
}

void MusicDriver_Allegro::SetVolume(byte vol)
{
	set_volume(-1, vol);
}

#endif /* WITH_ALLEGRO */
