/* $Id$ */

#ifdef WITH_SDL

#include "../stdafx.h"

#include "../driver.h"
#include "../mixer.h"
#include "../sdl.h"
#include "sdl_s.h"
#include <SDL.h>

static FSoundDriver_SDL iFSoundDriver_SDL;

static void CDECL fill_sound_buffer(void *userdata, Uint8 *stream, int len)
{
	MxMixSamples(stream, len / 4);
}

const char *SoundDriver_SDL::Start(const char * const *parm)
{
	SDL_AudioSpec spec;

	const char *s = SdlOpen(SDL_INIT_AUDIO);
	if (s != NULL) return s;

	spec.freq = GetDriverParamInt(parm, "hz", 11025);
	spec.format = AUDIO_S16SYS;
	spec.channels = 2;
	spec.samples = 512;
	spec.callback = fill_sound_buffer;
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
