/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sdl3_s.cpp Playing sound via SDL3. */

#ifdef WITH_SDL3

#include "../stdafx.h"

#include "../mixer.h"
#include "sdl_s.h"
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>

#include "../safeguards.h"

/** Factory for the SDL sound driver. */
static FSoundDriver_SDL iFSoundDriver_SDL;

/** SDL audio stream. */
static SDL_AudioStream *_sdl_audio_stream = nullptr;

/**
 * Callback that fills the sound buffer.
 * @param stream SDL audio stream requesting more data.
 * @param bytes_requested Number of additional bytes requested.
 */
void SDLCALL FillAudioStream(void *, SDL_AudioStream *stream, int bytes_requested, int)
{
	if (bytes_requested <= 0) return;

	Uint8 *data = SDL_stack_alloc(Uint8, bytes_requested);
	if (data == nullptr) return;

	MxMixSamples(data, bytes_requested / 4);
	SDL_PutAudioStreamData(stream, data, bytes_requested);
	SDL_stack_free(data);
}

std::optional<std::string_view> SoundDriver_SDL::Start(const StringList &parm)
{
	/* Only initialise SDL if the video driver hasn't done it already */
	bool success = false;
	if (!SDL_WasInit(SDL_INIT_VIDEO)) {
		success = SDL_Init(SDL_INIT_AUDIO);
	} else if (!SDL_WasInit(SDL_INIT_AUDIO)) {
		success = SDL_InitSubSystem(SDL_INIT_AUDIO);
	}
	if (!success) return SDL_GetError();

	const SDL_AudioSpec spec{ SDL_AUDIO_S16, 2, GetDriverParamInt(parm, "hz", 44100) };

	_sdl_audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, FillAudioStream, nullptr);
	if (_sdl_audio_stream == nullptr) return SDL_GetError();

	MxInitialize(spec.freq);

	if (!SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(_sdl_audio_stream))) return SDL_GetError();

	return std::nullopt;
}

void SoundDriver_SDL::Stop()
{
	if (_sdl_audio_stream != nullptr) {
		SDL_DestroyAudioStream(_sdl_audio_stream);
		_sdl_audio_stream = nullptr;
	}

	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	if (!SDL_WasInit(SDL_INIT_VIDEO)) {
		SDL_Quit(); // If there's nothing left, quit SDL
	}
}

#endif /* WITH_SDL3 */
