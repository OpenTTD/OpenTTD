/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bootstrap.cpp Bootstrap OpenTTD, i.e. downloading the required content. */

#include "stdafx.h"
#include "base_media_graphics.h"
#include "blitter/factory.hpp"
#include "error_func.h"
#include "network/network.h"
#include "openttd.h"
#include "video/video_driver.hpp"

#include "safeguards.h"

extern void HandleBootstrapGui();

/**
 * Handle all procedures for bootstrapping OpenTTD without a base graphics set.
 * This requires all kinds of trickery that is needed to avoid the use of
 * sprites from the base graphics set which are pretty interwoven.
 * @return True if a base set exists, otherwise false.
 */
bool HandleBootstrap()
{
	if (BaseGraphics::GetUsedSet() != nullptr) return true;

	/* No user interface, bail out with an error. */
	if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() == 0) goto failure;

	/* If there is no network or no non-sprite font, then there is nothing we can do. Go straight to failure. */
	if (!_network_available) goto failure;

#if defined(__EMSCRIPTEN__) || defined(WITH_UNISCRIBE) || (defined(WITH_FREETYPE) && (defined(WITH_FONTCONFIG))) || defined(WITH_COCOA)

	/* First tell the game we're bootstrapping. */
	_game_mode = GM_BOOTSTRAP;

	HandleBootstrapGui();

	/* Process the user events. */
	VideoDriver::GetInstance()->MainLoop();

	/* _exit_game is used to get out of the video driver's main loop.
	 * In case GM_BOOTSTRAP is still set we did not exit it via the
	 * "download complete" event, so it was a manual exit. Obey it. */
	_exit_game = _game_mode == GM_BOOTSTRAP;
	if (_exit_game) return false;

	/* Try to probe the graphics. Should work this time. */
	if (!BaseGraphics::SetSet(nullptr)) goto failure;

	/* Finally we can continue heading for the menu. */
	_game_mode = GM_MENU;
	return true;

#endif /* defined(__EMSCRIPTEN__) || defined(WITH_UNISCRIBE) || (defined(WITH_FREETYPE) && (defined(WITH_FONTCONFIG))) || defined(WITH_COCOA) */

	/* Failure to get enough working to get a graphics set. */
failure:
	UserError("Failed to find a graphics set. Please acquire a graphics set for OpenTTD. See section 1.4 of README.md.");
	return false;
}
