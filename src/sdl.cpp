/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sdl.cpp Implementation of SDL support. */

#include "stdafx.h"

#ifdef WITH_SDL

#include "sdl.h"
#include <SDL.h>

/** Number of users of the SDL library. */
static int _sdl_usage;

#ifdef DYNAMICALLY_LOADED_SDL

#include "os/windows/win32.h"

#define M(x) x "\0"
static const char sdl_files[] =
	M("sdl.dll")
	M("SDL_Init")
	M("SDL_InitSubSystem")
	M("SDL_GetError")
	M("SDL_QuitSubSystem")
	M("SDL_UpdateRect")
	M("SDL_UpdateRects")
	M("SDL_SetColors")
	M("SDL_WM_SetCaption")
	M("SDL_ShowCursor")
	M("SDL_FreeSurface")
	M("SDL_PollEvent")
	M("SDL_WarpMouse")
	M("SDL_GetTicks")
	M("SDL_OpenAudio")
	M("SDL_PauseAudio")
	M("SDL_CloseAudio")
	M("SDL_LockSurface")
	M("SDL_UnlockSurface")
	M("SDL_GetModState")
	M("SDL_Delay")
	M("SDL_Quit")
	M("SDL_SetVideoMode")
	M("SDL_EnableKeyRepeat")
	M("SDL_EnableUNICODE")
	M("SDL_VideoDriverName")
	M("SDL_ListModes")
	M("SDL_GetKeyState")
	M("SDL_LoadBMP_RW")
	M("SDL_RWFromFile")
	M("SDL_SetColorKey")
	M("SDL_WM_SetIcon")
	M("SDL_MapRGB")
	M("SDL_VideoModeOK")
	M("SDL_Linked_Version")
	M("")
;
#undef M

SDLProcs sdl_proc;

static const char *LoadSdlDLL()
{
	if (sdl_proc.SDL_Init != NULL) {
		return NULL;
	}
	if (!LoadLibraryList((Function *)(void *)&sdl_proc, sdl_files)) {
		return "Unable to load sdl.dll";
	}
	return NULL;
}

#endif /* DYNAMICALLY_LOADED_SDL */

/**
 * Open the SDL library.
 * @param x The subsystem to load.
 */
const char *SdlOpen(uint32 x)
{
#ifdef DYNAMICALLY_LOADED_SDL
	{
		const char *s = LoadSdlDLL();
		if (s != NULL) return s;
	}
#endif
	if (_sdl_usage++ == 0) {
		if (SDL_CALL SDL_Init(x | SDL_INIT_NOPARACHUTE) == -1) return SDL_CALL SDL_GetError();
	} else if (x != 0) {
		if (SDL_CALL SDL_InitSubSystem(x) == -1) return SDL_CALL SDL_GetError();
	}

	return NULL;
}

/**
 * Close the SDL library.
 * @param x The subsystem to close.
 */
void SdlClose(uint32 x)
{
	if (x != 0) {
		SDL_CALL SDL_QuitSubSystem(x);
	}
	if (--_sdl_usage == 0) {
		SDL_CALL SDL_Quit();
	}
}

#endif
