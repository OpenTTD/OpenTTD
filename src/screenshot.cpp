/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file screenshot.cpp The creation of screenshots! */

#include "stdafx.h"
#include "core/backup_type.hpp"
#include "fileio_func.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "screenshot.h"
#include "screenshot_gui.h"
#include "blitter/factory.hpp"
#include "zoom_func.h"
#include "saveload/saveload.h"
#include "company_func.h"
#include "strings_func.h"
#include "error.h"
#include "textbuf_gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "tile_map.h"
#include "landscape.h"
#include "video/video_driver.hpp"
#include "smallmap_gui.h"
#include "screenshot_type.h"

#include "table/strings.h"

#include "safeguards.h"

static const std::string_view SCREENSHOT_NAME = "screenshot"; ///< Default filename of a saved screenshot.
static const std::string_view HEIGHTMAP_NAME  = "heightmap";  ///< Default filename of a saved heightmap.

std::string _screenshot_format_name;  ///< Extension of the current screenshot format.
static std::string _screenshot_name;  ///< Filename of the screenshot file.
std::string _full_screenshot_path;    ///< Pathname of the screenshot file.
uint _heightmap_highest_peak;         ///< When saving a heightmap, this contains the highest peak on the map.

/**
 * Get the screenshot provider for the selected format.
 * If the selected provider is not found, then the first provider will be used instead.
 * @returns ScreenshotProvider, or null if none exist.
 */
static const ScreenshotProvider *GetScreenshotProvider()
{
	const auto &providers = ProviderManager<ScreenshotProvider>::GetProviders();
	if (providers.empty()) return nullptr;

	auto it = std::ranges::find(providers, _screenshot_format_name, &ScreenshotProvider::GetName);
	if (it != std::end(providers)) return *it;

	return providers.front();
}

/** Get filename extension of current screenshot file format. */
std::string_view GetCurrentScreenshotExtension()
{
	auto provider = GetScreenshotProvider();
	if (provider == nullptr) return {};

	return provider->GetName();
}

/**
 * Callback of the screenshot generator that dumps the current video buffer.
 * @see ScreenshotCallback
 */
static void CurrentScreenCallback(void *buf, uint y, uint pitch, uint n)
{
	Blitter *blitter = BlitterFactory::GetCurrentBlitter();
	void *src = blitter->MoveTo(_screen.dst_ptr, 0, y);
	blitter->CopyImageToBuffer(src, buf, _screen.width, n, pitch);
}

/**
 * generate a large piece of the world
 * @param vp Viewport area to draw
 * @param buf Videobuffer with same bitdepth as current blitter
 * @param y First line to render
 * @param pitch Pitch of the videobuffer
 * @param n Number of lines to render
 */
static void LargeWorldCallback(Viewport &vp, void *buf, uint y, uint pitch, uint n)
{
	DrawPixelInfo dpi{
		.dst_ptr = buf,
		.left = 0,
		.top = static_cast<int>(y),
		.width = vp.width,
		.height = static_cast<int>(n),
		.pitch = static_cast<int>(pitch),
		.zoom = ZoomLevel::WorldScreenshot
	};

	/* We are no longer rendering to the screen */
	AutoRestoreBackup screen_backup(_screen, {
		.dst_ptr = buf,
		.left = 0,
		.top = 0,
		.width = static_cast<int>(pitch),
		.height = static_cast<int>(n),
		.pitch = static_cast<int>(pitch),
		.zoom = ZoomLevel::Min
	});
	AutoRestoreBackup disable_anim_backup(_screen_disable_anim, true);
	AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);

	/* Render viewport in blocks of 1600 pixels width */
	int left = 0;
	while (vp.width - left != 0) {
		int wx = std::min(vp.width - left, 1600);
		left += wx;

		ViewportDoDraw(vp,
			ScaleByZoom(left - wx - vp.left, vp.zoom) + vp.virtual_left,
			ScaleByZoom(y - vp.top, vp.zoom) + vp.virtual_top,
			ScaleByZoom(left - vp.left, vp.zoom) + vp.virtual_left,
			ScaleByZoom((y + n) - vp.top, vp.zoom) + vp.virtual_top
		);
	}
}

/**
 * Construct a pathname for a screenshot file.
 * @param default_fn Default filename.
 * @param ext        Extension to use.
 * @param crashlog   Create path for crash.png
 * @return Pathname for a screenshot file.
 */
static std::string_view MakeScreenshotName(std::string_view default_fn, std::string_view ext, bool crashlog = false)
{
	bool generate = _screenshot_name.empty();

	if (generate) {
		if (_game_mode == GM_EDITOR || _game_mode == GM_MENU || _local_company == COMPANY_SPECTATOR) {
			_screenshot_name = default_fn;
		} else {
			_screenshot_name = GenerateDefaultSaveName();
		}
	}

	/* Handle user-specified filenames ending in # with automatic numbering */
	if (_screenshot_name.ends_with("#")) {
		generate = true;
		_screenshot_name.pop_back();
	}

	size_t len = _screenshot_name.size();
	/* Add extension to screenshot file */
	format_append(_screenshot_name, ".{}", ext);

	std::string_view screenshot_dir = crashlog ? _personal_dir : FiosGetScreenshotDir();

	for (uint serial = 1;; serial++) {
		_full_screenshot_path = fmt::format("{}{}", screenshot_dir, _screenshot_name);

		if (!generate) break; // allow overwriting of non-automatic filenames
		if (!FileExists(_full_screenshot_path)) break;
		/* If file exists try another one with same name, but just with a higher index */
		_screenshot_name.erase(len);
		format_append(_screenshot_name, "#{}.{}", serial, ext);
	}

	return _full_screenshot_path;
}

/** Make a screenshot of the current screen. */
static bool MakeSmallScreenshot(bool crashlog)
{
	auto provider = GetScreenshotProvider();
	if (provider == nullptr) return false;

	return provider->MakeImage(MakeScreenshotName(SCREENSHOT_NAME, provider->GetName(), crashlog), CurrentScreenCallback, _screen.width, _screen.height,
			BlitterFactory::GetCurrentBlitter()->GetScreenDepth(), _cur_palette.palette);
}

/**
 * Configure a Viewport for rendering (a part of) the map into a screenshot.
 * @param t Screenshot type
 * @param width the width of the screenshot, or 0 for current viewport width (needs to be 0 with SC_VIEWPORT, SC_CRASHLOG, and SC_WORLD).
 * @param height the height of the screenshot, or 0 for current viewport height (needs to be 0 with SC_VIEWPORT, SC_CRASHLOG, and SC_WORLD).
 * @return Viewport
 */
static Viewport SetupScreenshotViewport(ScreenshotType t, uint32_t width = 0, uint32_t height = 0)
{
	Viewport vp{};

	switch(t) {
		case SC_VIEWPORT:
		case SC_CRASHLOG: {
			assert(width == 0 && height == 0);

			Window *w = GetMainWindow();
			vp.virtual_left   = w->viewport->virtual_left;
			vp.virtual_top    = w->viewport->virtual_top;
			vp.virtual_width  = w->viewport->virtual_width;
			vp.virtual_height = w->viewport->virtual_height;

			/* Compute pixel coordinates */
			vp.left = 0;
			vp.top = 0;
			vp.width = _screen.width;
			vp.height = _screen.height;
			vp.overlay = w->viewport->overlay;
			break;
		}
		case SC_WORLD: {
			assert(width == 0 && height == 0);

			/* Determine world coordinates of screenshot */
			vp.zoom = ZoomLevel::WorldScreenshot;

			TileIndex north_tile = _settings_game.construction.freeform_edges ? TileXY(1, 1) : TileXY(0, 0);
			TileIndex south_tile{Map::Size() - 1};

			/* We need to account for a hill or high building at tile 0,0. */
			int extra_height_top = TilePixelHeight(north_tile) + 150;
			/* If there is a hill at the bottom don't create a large black area. */
			int reclaim_height_bottom = TilePixelHeight(south_tile);

			vp.virtual_left   = RemapCoords(TileX(south_tile) * TILE_SIZE, TileY(north_tile) * TILE_SIZE, 0).x;
			vp.virtual_top    = RemapCoords(TileX(north_tile) * TILE_SIZE, TileY(north_tile) * TILE_SIZE, extra_height_top).y;
			vp.virtual_width  = RemapCoords(TileX(north_tile) * TILE_SIZE, TileY(south_tile) * TILE_SIZE, 0).x                     - vp.virtual_left + 1;
			vp.virtual_height = RemapCoords(TileX(south_tile) * TILE_SIZE, TileY(south_tile) * TILE_SIZE, reclaim_height_bottom).y - vp.virtual_top  + 1;

			/* Compute pixel coordinates */
			vp.left = 0;
			vp.top = 0;
			vp.width  = UnScaleByZoom(vp.virtual_width,  vp.zoom);
			vp.height = UnScaleByZoom(vp.virtual_height, vp.zoom);
			vp.overlay = nullptr;
			break;
		}
		default: {
			vp.zoom = (t == SC_ZOOMEDIN) ? _settings_client.gui.zoom_min : ZoomLevel::Viewport;

			Window *w = GetMainWindow();
			vp.virtual_left   = w->viewport->virtual_left;
			vp.virtual_top    = w->viewport->virtual_top;

			if (width == 0 || height == 0) {
				vp.virtual_width  = w->viewport->virtual_width;
				vp.virtual_height = w->viewport->virtual_height;
			} else {
				vp.virtual_width = width << to_underlying(vp.zoom);
				vp.virtual_height = height << to_underlying(vp.zoom);
			}

			/* Compute pixel coordinates */
			vp.left = 0;
			vp.top = 0;
			vp.width  = UnScaleByZoom(vp.virtual_width,  vp.zoom);
			vp.height = UnScaleByZoom(vp.virtual_height, vp.zoom);
			vp.overlay = nullptr;
			break;
		}
	}

	return vp;
}

/**
 * Make a screenshot of the map.
 * @param t Screenshot type: World or viewport screenshot
 * @param width the width of the screenshot of, or 0 for current viewport width.
 * @param height the height of the screenshot of, or 0 for current viewport height.
 * @return true on success
 */
static bool MakeLargeWorldScreenshot(ScreenshotType t, uint32_t width = 0, uint32_t height = 0)
{
	auto provider = GetScreenshotProvider();
	if (provider == nullptr) return false;

	Viewport vp = SetupScreenshotViewport(t, width, height);

	return provider->MakeImage(MakeScreenshotName(SCREENSHOT_NAME, provider->GetName()),
			[&](void *buf, uint y, uint pitch, uint n) {
				LargeWorldCallback(vp, buf, y, pitch, n);
			}, vp.width, vp.height, BlitterFactory::GetCurrentBlitter()->GetScreenDepth(), _cur_palette.palette);
}

/**
 * Callback for generating a heightmap. Supports 8bpp greyscale only.
 * @param buffer   Destination buffer.
 * @param y        Line number of the first line to write.
 * @param n        Number of lines to write.
 * @see ScreenshotCallback
 */
static void HeightmapCallback(void *buffer, uint y, uint, uint n)
{
	uint8_t *buf = (uint8_t *)buffer;
	while (n > 0) {
		TileIndex ti = TileXY(Map::MaxX(), y);
		for (uint x = Map::MaxX(); true; x--) {
			*buf = 256 * TileHeight(ti) / (1 + _heightmap_highest_peak);
			buf++;
			if (x == 0) break;
			ti = TileAddXY(ti, -1, 0);
		}
		y++;
		n--;
	}
}

/**
 * Make a heightmap of the current map.
 * @param filename Filename to use for saving.
 */
bool MakeHeightmapScreenshot(std::string_view filename)
{
	auto provider = GetScreenshotProvider();
	if (provider == nullptr) return false;

	Colour palette[256];
	for (uint i = 0; i < lengthof(palette); i++) {
		palette[i].a = 0xff;
		palette[i].r = i;
		palette[i].g = i;
		palette[i].b = i;
	}

	_heightmap_highest_peak = 0;
	for (const auto tile : Map::Iterate()) {
		uint h = TileHeight(tile);
		_heightmap_highest_peak = std::max(h, _heightmap_highest_peak);
	}

	return provider->MakeImage(filename, HeightmapCallback, Map::SizeX(), Map::SizeY(), 8, palette);
}

static ScreenshotType _confirmed_screenshot_type; ///< Screenshot type the current query is about to confirm.

/**
 * Callback on the confirmation window for huge screenshots.
 * @param confirmed true on confirmation
 */
static void ScreenshotConfirmationCallback(Window *, bool confirmed)
{
	if (confirmed) MakeScreenshot(_confirmed_screenshot_type, {});
}

/**
 * Make a screenshot.
 * Ask for confirmation first if the screenshot will be huge.
 * @param t Screenshot type: World, defaultzoom, heightmap or viewport screenshot
 * @see MakeScreenshot
 */
void MakeScreenshotWithConfirm(ScreenshotType t)
{
	Viewport vp = SetupScreenshotViewport(t);

	bool heightmap_or_minimap = t == SC_HEIGHTMAP || t == SC_MINIMAP;
	uint64_t width = (heightmap_or_minimap ? Map::SizeX() : vp.width);
	uint64_t height = (heightmap_or_minimap ? Map::SizeY() : vp.height);

	if (width * height > 8192 * 8192) {
		/* Ask for confirmation */
		_confirmed_screenshot_type = t;
		ShowQuery(
			GetEncodedString(STR_WARNING_SCREENSHOT_SIZE_CAPTION),
			GetEncodedString(STR_WARNING_SCREENSHOT_SIZE_MESSAGE, width, height), nullptr, ScreenshotConfirmationCallback);
	} else {
		/* Less than 64M pixels, just do it */
		MakeScreenshot(t, {});
	}
}

/**
 * Make a screenshot.
 * @param t    the type of screenshot to make.
 * @param name the name to give to the screenshot.
 * @param width the width of the screenshot of, or 0 for current viewport width (only works for SC_ZOOMEDIN and SC_DEFAULTZOOM).
 * @param height the height of the screenshot of, or 0 for current viewport height (only works for SC_ZOOMEDIN and SC_DEFAULTZOOM).
 * @return true iff the screenshot was made successfully
 */
static bool RealMakeScreenshot(ScreenshotType t, const std::string &name, uint32_t width, uint32_t height)
{
	if (t == SC_VIEWPORT) {
		/* First draw the dirty parts of the screen and only then change the name
		 * of the screenshot. This way the screenshot will always show the name
		 * of the previous screenshot in the 'successful' message instead of the
		 * name of the new screenshot (or an empty name). */
		SetScreenshotWindowVisibility(true);
		UndrawMouseCursor();
		DrawDirtyBlocks();
		SetScreenshotWindowVisibility(false);
	}

	_screenshot_name = name;

	bool ret;
	switch (t) {
		case SC_VIEWPORT:
			ret = MakeSmallScreenshot(false);
			break;

		case SC_CRASHLOG:
			ret = MakeSmallScreenshot(true);
			break;

		case SC_ZOOMEDIN:
		case SC_DEFAULTZOOM:
			ret = MakeLargeWorldScreenshot(t, width, height);
			break;

		case SC_WORLD:
			ret = MakeLargeWorldScreenshot(t);
			break;

		case SC_HEIGHTMAP: {
			auto provider = GetScreenshotProvider();
			if (provider == nullptr) {
				ret = false;
			} else {
				ret = MakeHeightmapScreenshot(MakeScreenshotName(HEIGHTMAP_NAME, provider->GetName()));
			}
			break;
		}

		case SC_MINIMAP:
			ret = MakeMinimapWorldScreenshot();
			break;

		default:
			NOT_REACHED();
	}

	if (ret) {
		if (t == SC_HEIGHTMAP) {
			ShowErrorMessage(GetEncodedString(STR_MESSAGE_HEIGHTMAP_SUCCESSFULLY, _screenshot_name, _heightmap_highest_peak), {}, WL_WARNING);
		} else {
			ShowErrorMessage(GetEncodedString(STR_MESSAGE_SCREENSHOT_SUCCESSFULLY, _screenshot_name), {}, WL_WARNING);
		}
	} else {
		ShowErrorMessage(GetEncodedString(STR_ERROR_SCREENSHOT_FAILED), {}, WL_ERROR);
	}

	return ret;
}

/**
 * Schedule making a screenshot.
 * Unconditionally take a screenshot of the requested type.
 * @param t    the type of screenshot to make.
 * @param name the name to give to the screenshot.
 * @param width the width of the screenshot of, or 0 for current viewport width (only works for SC_ZOOMEDIN and SC_DEFAULTZOOM).
 * @param height the height of the screenshot of, or 0 for current viewport height (only works for SC_ZOOMEDIN and SC_DEFAULTZOOM).
 * @return true iff the screenshot was successfully made.
 * @see MakeScreenshotWithConfirm
 */
bool MakeScreenshot(ScreenshotType t, const std::string &name, uint32_t width, uint32_t height)
{
	if (t == SC_CRASHLOG) {
		/* Video buffer might or might not be locked. */
		VideoDriver::VideoBufferLocker lock;

		return RealMakeScreenshot(t, name, width, height);
	}

	VideoDriver::GetInstance()->QueueOnMainThread([=] { // Capture by value to not break scope.
		RealMakeScreenshot(t, name, width, height);
	});

	return true;
}


static void MinimapScreenCallback(void *buf, uint y, uint pitch, uint n)
{
	uint32_t *ubuf = (uint32_t *)buf;
	uint num = (pitch * n);
	for (uint i = 0; i < num; i++) {
		uint row = y + (int)(i / pitch);
		uint col = (Map::SizeX() - 1) - (i % pitch);

		TileIndex tile = TileXY(col, row);
		uint8_t val = GetSmallMapOwnerPixels(tile, GetTileType(tile), IncludeHeightmap::Never) & 0xFF;

		uint32_t colour_buf = 0;
		colour_buf  = (_cur_palette.palette[val].b << 0);
		colour_buf |= (_cur_palette.palette[val].g << 8);
		colour_buf |= (_cur_palette.palette[val].r << 16);

		*ubuf = colour_buf;
		ubuf++;   // Skip alpha
	}
}

/**
 * Make a minimap screenshot.
 */
bool MakeMinimapWorldScreenshot()
{
	auto provider = GetScreenshotProvider();
	if (provider == nullptr) return false;

	return provider->MakeImage(MakeScreenshotName(SCREENSHOT_NAME, provider->GetName()), MinimapScreenCallback, Map::SizeX(), Map::SizeY(), 32, _cur_palette.palette);
}
