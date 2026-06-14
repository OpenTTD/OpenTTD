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
#include <SDL3/SDL.h>

#include "../safeguards.h"

/** Factory for the SDL sound driver. */
static FSoundDriver_SDL iFSoundDriver_SDL;

/** SDL audio stream. */
static SDL_AudioStream *_sdl_audio_stream = nullptr;

/**
 * Callback that fills the sound buffer.
 * @param stream   The stream to put data into.
 * @param len      The length of the stream in bytes.
 */
static void CDECL fill_sound_buffer(void *, Uint8 *stream, int len)
{
	MxMixSamples(stream, len / 4);
}

namespace {
	/**
	 * SDL3 audio callback that feeds OpenTTD samples into the stream.
	 * @param userdata User data passed to SDL.
	 * @param stream SDL audio stream requesting more data.
	 * @param bytes_requested Number of additional bytes requested.
	 * @return Nothing.
	 */
	void SDLCALL FillAudioStream(void *userdata, SDL_AudioStream *stream, int bytes_requested, int)
	{
		if (bytes_requested > 0) {
			Uint8 *data = SDL_stack_alloc(Uint8, bytes_requested);
			if (data) {
				fill_sound_buffer(userdata, data, bytes_requested);
				SDL_PutAudioStreamData(stream, data, bytes_requested);
				SDL_stack_free(data);
			}
		}
	}
}

std::optional<std::string_view> SoundDriver_SDL::Start(const StringList &parm)
{
	/* Only initialise SDL if the video driver hasn't done it already */
	int ret_code = 0;
	if (SDL_WasInit(0) == 0) {
		ret_code = SDL_Init(SDL_INIT_AUDIO);
	} else if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
		ret_code = SDL_InitSubSystem(SDL_INIT_AUDIO);
	}
	if (ret_code == -1) return SDL_GetError();

	const SDL_AudioSpec spec = {
		SDL_AUDIO_S16, 2, GetDriverParamInt(parm, "hz", 44100)
	};

	_sdl_audio_stream = SDL_OpenAudioDeviceStream(
		SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
		&spec,
		FillAudioStream,
		nullptr);

	if (_sdl_audio_stream == nullptr) return SDL_GetError();

	MxInitialize(spec.freq);

	if (!SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(_sdl_audio_stream))) {
		return SDL_GetError();
	}

	return std::nullopt;
}

void SoundDriver_SDL::Stop()
{
	if (_sdl_audio_stream != nullptr) {
		SDL_DestroyAudioStream(_sdl_audio_stream);
		_sdl_audio_stream = nullptr;
	}

	SDL_QuitSubSystem(SDL_INIT_AUDIO);

	if (SDL_WasInit(0) == 0) {
		SDL_Quit(); // If there's nothing left, quit SDL
	}
}

#endif /* WITH_SDL3 */
