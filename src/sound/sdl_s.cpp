/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sdl_s.cpp Playing sound via SDL. */

#ifdef WITH_SDL

#include "../stdafx.h"

#include "../mixer.h"
#include "../sdl.h"
#include "sdl_s.h"
#include <SDL.h>

/** Factory for the SDL sound driver. */
static FSoundDriver_SDL iFSoundDriver_SDL;

/**
 * Callback that fills the sound buffer.
 * @param userdata Ignored.
 * @param stream   The stream to put data into.
 * @param len      The length of the stream in bytes.
 */
static void CDECL fill_sound_buffer(void *userdata, Uint8 *stream, int len)
{
	MxMixSamples(stream, len / 4);
}

const char *SoundDriver_SDL::Start(const char * const *parm)
{
	SDL_AudioSpec spec;

	const char *s = SdlOpen(SDL_INIT_AUDIO);
	if (s != NULL) return s;

	spec.freq = GetDriverParamInt(parm, "hz", 44100);
	spec.format = AUDIO_S16SYS;
	spec.channels = 2;
	spec.samples = GetDriverParamInt(parm, "samples", 1024);
	spec.callback = fill_sound_buffer;
	MxInitialize(spec.freq);
	SDL_CALL SDL_OpenAudio(&spec, &spec);
	SDL_CALL SDL_PauseAudio(0);
	return NULL;
}

void SoundDriver_SDL::Stop()
{
	SDL_CALL SDL_CloseAudio();
	SdlClose(SDL_INIT_AUDIO);
}

#endif /* WITH_SDL */
