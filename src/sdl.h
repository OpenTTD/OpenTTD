/* $Id$ */

#ifndef SDL_H
#define SDL_H

const char* SdlOpen(uint32 x);
void SdlClose(uint32 x);

#ifdef WIN32
	#define DYNAMICALLY_LOADED_SDL
#endif

#ifdef DYNAMICALLY_LOADED_SDL
	#include <SDL.h>

	struct SDLProcs {
		int (SDLCALL *SDL_Init)(Uint32);
		int (SDLCALL *SDL_InitSubSystem)(Uint32);
		char *(SDLCALL *SDL_GetError)();
		void (SDLCALL *SDL_QuitSubSystem)(Uint32);
		void (SDLCALL *SDL_UpdateRect)(SDL_Surface *, Sint32, Sint32, Uint32, Uint32);
		void (SDLCALL *SDL_UpdateRects)(SDL_Surface *, int, SDL_Rect *);
		int (SDLCALL *SDL_SetColors)(SDL_Surface *, SDL_Color *, int, int);
		void (SDLCALL *SDL_WM_SetCaption)(const char *, const char *);
		int (SDLCALL *SDL_ShowCursor)(int);
		void (SDLCALL *SDL_FreeSurface)(SDL_Surface *);
		int (SDLCALL *SDL_PollEvent)(SDL_Event *);
		void (SDLCALL *SDL_WarpMouse)(Uint16, Uint16);
		uint32 (SDLCALL *SDL_GetTicks)();
		int (SDLCALL *SDL_OpenAudio)(SDL_AudioSpec *, SDL_AudioSpec*);
		void (SDLCALL *SDL_PauseAudio)(int);
		void (SDLCALL *SDL_CloseAudio)();
		int (SDLCALL *SDL_LockSurface)(SDL_Surface*);
		void (SDLCALL *SDL_UnlockSurface)(SDL_Surface*);
		SDLMod (SDLCALL *SDL_GetModState)();
		void (SDLCALL *SDL_Delay)(Uint32);
		void (SDLCALL *SDL_Quit)();
		SDL_Surface *(SDLCALL *SDL_SetVideoMode)(int, int, int, Uint32);
		int (SDLCALL *SDL_EnableKeyRepeat)(int, int);
		void (SDLCALL *SDL_EnableUNICODE)(int);
		void (SDLCALL *SDL_VideoDriverName)(char *, int);
		SDL_Rect **(SDLCALL *SDL_ListModes)(void *, int);
		Uint8 *(SDLCALL *SDL_GetKeyState)(int *);
		SDL_Surface *(SDLCALL *SDL_LoadBMP_RW)(SDL_RWops *, int);
		SDL_RWops *(SDLCALL *SDL_RWFromFile)(const char *, const char *);
		int (SDLCALL *SDL_SetColorKey)(SDL_Surface *, Uint32, Uint32);
		void (SDLCALL *SDL_WM_SetIcon)(SDL_Surface *, Uint8 *);
		Uint32 (SDLCALL *SDL_MapRGB)(SDL_PixelFormat *, Uint8, Uint8, Uint8);
	};

	extern SDLProcs sdl_proc;

	#define SDL_CALL sdl_proc.
#else
	#define SDL_CALL
#endif

#endif /* SDL_H */
