/* $Id$ */

#include "../stdafx.h"
#include "../openttd.h"
#include "../gfx_func.h"
#include "../variables.h"
#include "../debug.h"
#include "../blitter/factory.hpp"
#include "null_v.h"

static FVideoDriver_Null iFVideoDriver_Null;

const char *VideoDriver_Null::Start(const char* const *parm)
{
	this->ticks = GetDriverParamInt(parm, "ticks", 1000);
	_screen.width = _screen.pitch = _cur_resolution[0];
	_screen.height = _cur_resolution[1];
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
		_screen.dst_ptr = NULL;
		UpdateWindows();
	}
}

bool VideoDriver_Null::ChangeResolution(int w, int h) { return false; }

void VideoDriver_Null::ToggleFullscreen(bool fs) {}
