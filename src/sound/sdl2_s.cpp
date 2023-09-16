/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sdl2_s.cpp Playing sound via SDL2. */

#ifdef WITH_SDL2

#include "../stdafx.h"

#include "../mixer.h"
#include "sdl_s.h"
#include <SDL.h>

#include "../safeguards.h"

/** Factory for the SDL sound driver. */
static FSoundDriver_SDL iFSoundDriver_SDL;

/**
 * Callback that fills the sound buffer.
 * @param stream   The stream to put data into.
 * @param len      The length of the stream in bytes.
 */
static void CDECL fill_sound_buffer(void *, Uint8 *stream, int len)
{
	MxMixSamples(stream, len / 4);
}

const char *SoundDriver_SDL::Start(const StringList &parm)
{
	SDL_AudioSpec spec;
	SDL_AudioSpec spec_actual;

	/* Only initialise SDL if the video driver hasn't done it already */
	int ret_code = 0;
	if (SDL_WasInit(SDL_INIT_EVERYTHING) == 0) {
		ret_code = SDL_Init(SDL_INIT_AUDIO);
	} else if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
		ret_code = SDL_InitSubSystem(SDL_INIT_AUDIO);
	}
	if (ret_code == -1) return SDL_GetError();

	spec.freq = GetDriverParamInt(parm, "hz", 44100);
	spec.format = AUDIO_S16SYS;
	spec.channels = 2;
	spec.samples = GetDriverParamInt(parm, "samples", 1024);
	spec.callback = fill_sound_buffer;
	SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &spec, &spec_actual, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	MxInitialize(spec_actual.freq);
	SDL_PauseAudioDevice(dev, 0);
	return nullptr;
}

void SoundDriver_SDL::Stop()
{
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	if (SDL_WasInit(SDL_INIT_EVERYTHING) == 0) {
		SDL_Quit(); // If there's nothing left, quit SDL
	}
}

#endif /* WITH_SDL2 */
