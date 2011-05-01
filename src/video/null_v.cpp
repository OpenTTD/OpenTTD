/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file null_v.cpp The videio driver that doesn't blit. */

#include "../stdafx.h"
#include "../gfx_func.h"
#include "../blitter/factory.hpp"
#include "null_v.h"

/** Factory for the null video driver. */
static FVideoDriver_Null iFVideoDriver_Null;

const char *VideoDriver_Null::Start(const char * const *parm)
{
	this->ticks = GetDriverParamInt(parm, "ticks", 1000);
	_screen.width  = _screen.pitch = _cur_resolution.width;
	_screen.height = _cur_resolution.height;
	_screen.dst_ptr = NULL;
	ScreenSizeChanged();

	/* Do not render, nor blit */
	DEBUG(misc, 1, "Forcing blitter 'null'...");
	BlitterFactoryBase::SelectBlitter("null");
	return NULL;
}

void VideoDriver_Null::Stop() { }

void VideoDriver_Null::MakeDirty(int left, int top, int width, int height) {}

void VideoDriver_Null::MainLoop()
{
	uint i;

	for (i = 0; i < this->ticks; i++) {
		GameLoop();
		UpdateWindows();
	}
}

bool VideoDriver_Null::ChangeResolution(int w, int h) { return false; }

bool VideoDriver_Null::ToggleFullscreen(bool fs) { return false; }
