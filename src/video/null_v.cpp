/* $Id$ */

#include "../stdafx.h"
#include "../openttd.h"
#include "../gfx.h"
#include "../variables.h"
#include "../window.h"
#include "../debug.h"
#include "../blitter/factory.hpp"
#include "null_v.h"

static const char* NullVideoStart(const char* const* parm)
{
	_screen.width = _screen.pitch = _cur_resolution[0];
	_screen.height = _cur_resolution[1];
	/* Do not render, nor blit */
	DEBUG(misc, 1, "Forcing blitter 'null'...");
	BlitterFactoryBase::SelectBlitter("null");
	return NULL;
}

static void NullVideoStop() { }

static void NullVideoMakeDirty(int left, int top, int width, int height) {}

static void NullVideoMainLoop()
{
	uint i;

	for (i = 0; i < 1000; i++) {
		GameLoop();
		_screen.dst_ptr = NULL;
		UpdateWindows();
	}
}

static bool NullVideoChangeRes(int w, int h) { return false; }
static void NullVideoFullScreen(bool fs) {}

const HalVideoDriver _null_video_driver = {
	NullVideoStart,
	NullVideoStop,
	NullVideoMakeDirty,
	NullVideoMainLoop,
	NullVideoChangeRes,
	NullVideoFullScreen,
};
