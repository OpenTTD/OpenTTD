/* $Id$ */

/** @file allegro_s.cpp Playing sound via Allegro. */

#ifdef WITH_ALLEGRO

#include "../stdafx.h"

#include "../driver.h"
#include "../mixer.h"
#include "../sdl.h"
#include "../debug.h"
#include "allegro_s.h"
#include <allegro.h>

static FSoundDriver_Allegro iFSoundDriver_Allegro;
/** The stream we are writing too */
static AUDIOSTREAM *_stream = NULL;
/** The number of samples in the buffer */
static const int BUFFER_SIZE = 512;

void SoundDriver_Allegro::MainLoop()
{
	/* We haven't opened a stream yet */
	if (_stream == NULL) return;

	void *data = get_audio_stream_buffer(_stream);
	/* We don't have to fill the stream yet */
	if (data == NULL) return;

	/* Mix the samples */
	MxMixSamples(data, BUFFER_SIZE);

	/* Allegro sound is always unsigned, so we need to correct that */
	uint16 *snd = (uint16*)data;
	for (int i = 0; i < BUFFER_SIZE * 2; i++) snd[i] ^= 0x8000;

	/* Tell we've filled the stream */
	free_audio_stream_buffer(_stream);
}

/** There are multiple modules that might be using Allegro and
 * Allegro can only be initiated once. */
extern int _allegro_instance_count;

const char *SoundDriver_Allegro::Start(const char * const *parm)
{
	if (_allegro_instance_count == 0 && install_allegro(SYSTEM_AUTODETECT, &errno, NULL)) return NULL;
	_allegro_instance_count++;

	/* Initialise the sound */
	if (install_sound(DIGI_AUTODETECT, MIDI_AUTODETECT, NULL) != 0) return NULL;

	/* Okay, there's no soundcard */
	if (digi_card == DIGI_NONE) {
		DEBUG(driver, 0, "allegro: no sound card found");
		return NULL;
	}

	_stream = play_audio_stream(BUFFER_SIZE, 16, true, 11025, 255, 128);
	return NULL;
}

void SoundDriver_Allegro::Stop()
{
	if (_stream != NULL) {
		stop_audio_stream(_stream);
		_stream = NULL;
	}
	remove_sound();

	if (--_allegro_instance_count == 0) allegro_exit();
}

#endif /* WITH_ALLEGRO */
