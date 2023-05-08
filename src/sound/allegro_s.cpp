/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file allegro_s.cpp Playing sound via Allegro. */

#ifdef WITH_ALLEGRO

#include "../stdafx.h"

#include "../mixer.h"
#include "../debug.h"
#include "allegro_s.h"
#include <allegro.h>

#include "../safeguards.h"

static FSoundDriver_Allegro iFSoundDriver_Allegro;
/** The stream we are writing too */
static AUDIOSTREAM *_stream = nullptr;
/** The number of samples in the buffer */
static int _buffer_size;

void SoundDriver_Allegro::MainLoop()
{
	/* We haven't opened a stream yet */
	if (_stream == nullptr) return;

	void *data = get_audio_stream_buffer(_stream);
	/* We don't have to fill the stream yet */
	if (data == nullptr) return;

	/* Mix the samples */
	MxMixSamples(data, _buffer_size);

	/* Allegro sound is always unsigned, so we need to correct that */
	uint16_t *snd = (uint16_t*)data;
	for (int i = 0; i < _buffer_size * 2; i++) snd[i] ^= 0x8000;

	/* Tell we've filled the stream */
	free_audio_stream_buffer(_stream);
}

/**
 * There are multiple modules that might be using Allegro and
 * Allegro can only be initiated once.
 */
extern int _allegro_instance_count;

const char *SoundDriver_Allegro::Start(const StringList &parm)
{
	if (_allegro_instance_count == 0 && install_allegro(SYSTEM_AUTODETECT, &errno, nullptr)) {
		Debug(driver, 0, "allegro: install_allegro failed '{}'", allegro_error);
		return "Failed to set up Allegro";
	}
	_allegro_instance_count++;

	/* Initialise the sound */
	if (install_sound(DIGI_AUTODETECT, MIDI_AUTODETECT, nullptr) != 0) {
		Debug(driver, 0, "allegro: install_sound failed '{}'", allegro_error);
		return "Failed to set up Allegro sound";
	}

	/* Okay, there's no soundcard */
	if (digi_card == DIGI_NONE) {
		Debug(driver, 0, "allegro: no sound card found");
		return "No sound card found";
	}

	int hz = GetDriverParamInt(parm, "hz", 44100);
	_buffer_size = GetDriverParamInt(parm, "samples", 1024) * hz / 11025;
	_stream = play_audio_stream(_buffer_size, 16, true, hz, 255, 128);
	MxInitialize(hz);
	return nullptr;
}

void SoundDriver_Allegro::Stop()
{
	if (_stream != nullptr) {
		stop_audio_stream(_stream);
		_stream = nullptr;
	}
	remove_sound();

	if (--_allegro_instance_count == 0) allegro_exit();
}

#endif /* WITH_ALLEGRO */
