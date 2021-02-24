/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sdl2_default_v.cpp Implementation of the default backend for SDL2 video driver. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../gfx_func.h"
#include "../rev.h"
#include "../blitter/factory.hpp"
#include "../network/network.h"
#include "../thread.h"
#include "../progress.h"
#include "../core/random_func.hpp"
#include "../core/math_func.hpp"
#include "../core/mem_func.hpp"
#include "../core/geometry_func.hpp"
#include "../fileio_func.h"
#include "../framerate_type.h"
#include "../window_func.h"
#include "sdl2_default_v.h"
#include <SDL.h>
#include <mutex>
#include <condition_variable>
#ifdef __EMSCRIPTEN__
#	include <emscripten.h>
#	include <emscripten/html5.h>
#endif

#include "../safeguards.h"

static FVideoDriver_SDL_Default iFVideoDriver_SDL_Default;

static SDL_Surface *_sdl_surface;
static SDL_Surface *_sdl_rgb_surface;
static SDL_Surface *_sdl_real_surface;
static SDL_Palette *_sdl_palette;

void VideoDriver_SDL_Default::UpdatePalette()
{
	SDL_Color pal[256];

	for (int i = 0; i != this->local_palette.count_dirty; i++) {
		pal[i].r = this->local_palette.palette[this->local_palette.first_dirty + i].r;
		pal[i].g = this->local_palette.palette[this->local_palette.first_dirty + i].g;
		pal[i].b = this->local_palette.palette[this->local_palette.first_dirty + i].b;
		pal[i].a = 0;
	}

	SDL_SetPaletteColors(_sdl_palette, pal, this->local_palette.first_dirty, this->local_palette.count_dirty);
	SDL_SetSurfacePalette(_sdl_surface, _sdl_palette);
}

void VideoDriver_SDL_Default::MakePalette()
{
	if (_sdl_palette == nullptr) {
		_sdl_palette = SDL_AllocPalette(256);
		if (_sdl_palette == nullptr) usererror("SDL2: Couldn't allocate palette: %s", SDL_GetError());
	}

	_cur_palette.first_dirty = 0;
	_cur_palette.count_dirty = 256;
	this->local_palette = _cur_palette;
	this->UpdatePalette();

	if (_sdl_surface != _sdl_real_surface) {
		/* When using a shadow surface, also set our palette on the real screen. This lets SDL
		 * allocate as many colors (or approximations) as
		 * possible, instead of using only the default SDL
		 * palette. This allows us to get more colors exactly
		 * right and might allow using better approximations for
		 * other colors.
		 *
		 * Note that colors allocations are tried in-order, so
		 * this favors colors further up into the palette. Also
		 * note that if two colors from the same animation
		 * sequence are approximated using the same color, that
		 * animation will stop working.
		 *
		 * Since changing the system palette causes the colours
		 * to change right away, and allocations might
		 * drastically change, we can't use this for animation,
		 * since that could cause weird coloring between the
		 * palette change and the blitting below, so we only set
		 * the real palette during initialisation.
		 */
		SDL_SetSurfacePalette(_sdl_real_surface, _sdl_palette);
	}
}

void VideoDriver_SDL_Default::Paint()
{
	PerformanceMeasurer framerate(PFE_VIDEO);

	if (IsEmptyRect(this->dirty_rect) && _cur_palette.count_dirty == 0) return;

	if (_cur_palette.count_dirty != 0) {
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();

		switch (blitter->UsePaletteAnimation()) {
			case Blitter::PALETTE_ANIMATION_VIDEO_BACKEND:
				this->UpdatePalette();
				break;

			case Blitter::PALETTE_ANIMATION_BLITTER: {
				bool need_buf = _screen.dst_ptr == nullptr;
				if (need_buf) _screen.dst_ptr = this->GetVideoPointer();
				blitter->PaletteAnimate(this->local_palette);
				if (need_buf) {
					this->ReleaseVideoPointer();
					_screen.dst_ptr = nullptr;
				}
				break;
			}

			case Blitter::PALETTE_ANIMATION_NONE:
				break;

			default:
				NOT_REACHED();
		}
		_cur_palette.count_dirty = 0;
	}

	SDL_Rect r = { this->dirty_rect.left, this->dirty_rect.top, this->dirty_rect.right - this->dirty_rect.left, this->dirty_rect.bottom - this->dirty_rect.top };

	if (_sdl_surface != _sdl_real_surface) {
		SDL_BlitSurface(_sdl_surface, &r, _sdl_real_surface, &r);
	}
	SDL_UpdateWindowSurfaceRects(this->sdl_window, &r, 1);

	this->dirty_rect = {};
}

bool VideoDriver_SDL_Default::AllocateBackingStore(int w, int h, bool force)
{
	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();

	_sdl_real_surface = SDL_GetWindowSurface(this->sdl_window);
	if (_sdl_real_surface == nullptr) usererror("SDL2: Couldn't get window surface: %s", SDL_GetError());

	if (!force && w == _sdl_real_surface->w && h == _sdl_real_surface->h) return false;

	/* Free any previously allocated rgb surface. */
	if (_sdl_rgb_surface != nullptr) {
		SDL_FreeSurface(_sdl_rgb_surface);
		_sdl_rgb_surface = nullptr;
	}

	if (bpp == 8) {
		_sdl_rgb_surface = SDL_CreateRGBSurface(0, w, h, 8, 0, 0, 0, 0);
		if (_sdl_rgb_surface == nullptr) usererror("SDL2: Couldn't allocate shadow surface: %s", SDL_GetError());

		_sdl_surface = _sdl_rgb_surface;
	} else {
		_sdl_surface = _sdl_real_surface;
	}

	/* X11 doesn't appreciate it if we invalidate areas outside the window
	 * if shared memory is enabled (read: it crashes). So, as we might have
	 * gotten smaller, reset our dirty rects. GameSizeChanged() a bit lower
	 * will mark the whole screen dirty again anyway, but this time with the
	 * new dimensions. */
	this->dirty_rect = {};

	_screen.width = _sdl_surface->w;
	_screen.height = _sdl_surface->h;
	_screen.pitch = _sdl_surface->pitch / (bpp / 8);
	_screen.dst_ptr = this->GetVideoPointer();

	this->MakePalette();

	return true;
}

void *VideoDriver_SDL_Default::GetVideoPointer()
{
	return _sdl_surface->pixels;
}
