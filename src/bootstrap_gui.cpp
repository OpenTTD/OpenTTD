/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bootstrap_gui.cpp Barely used user interface for bootstrapping OpenTTD, i.e. downloading the required content. */

#include "stdafx.h"
#include "base_media_base.h"
#include "blitter/factory.hpp"

#if defined(ENABLE_NETWORK) && defined(WITH_FREETYPE)

#include "core/geometry_func.hpp"
#include "fontcache.h"
#include "gfx_func.h"
#include "network/network.h"
#include "network/network_content_gui.h"
#include "openttd.h"
#include "strings_func.h"
#include "video/video_driver.hpp"
#include "window_func.h"

#include "widgets/bootstrap_widget.h"

#include "table/strings.h"

#include "safeguards.h"

/** Widgets for the background window to prevent smearing. */
static const struct NWidgetPart _background_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_DARK_BLUE, WID_BB_BACKGROUND), SetResize(1, 1),
};

/**
 * Window description for the background window to prevent smearing.
 */
static WindowDesc _background_desc(
	WDP_MANUAL, NULL, 0, 0,
	WC_BOOTSTRAP, WC_NONE,
	0,
	_background_widgets, lengthof(_background_widgets)
);

/** The background for the game. */
class BootstrapBackground : public Window {
public:
	BootstrapBackground() : Window(&_background_desc)
	{
		this->InitNested(0);
		CLRBITS(this->flags, WF_WHITE_BORDER);
		ResizeWindow(this, _screen.width, _screen.height);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		GfxFillRect(r.left, r.top, r.right, r.bottom, 4, FILLRECT_OPAQUE);
		GfxFillRect(r.left, r.top, r.right, r.bottom, 0, FILLRECT_CHECKER);
	}
};

/** Nested widgets for the download window. */
static const NWidgetPart _nested_boostrap_download_status_window_widgets[] = {
	NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_CONTENT_DOWNLOAD_TITLE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_NCDS_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(350, 0), SetMinimalTextLines(3, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM + 30),
	EndContainer(),
};

/** Window description for the download window */
static WindowDesc _bootstrap_download_status_window_desc(
	WDP_CENTER, NULL, 0, 0,
	WC_NETWORK_STATUS_WINDOW, WC_NONE,
	WDF_MODAL,
	_nested_boostrap_download_status_window_widgets, lengthof(_nested_boostrap_download_status_window_widgets)
);


/** Window for showing the download status of content */
struct BootstrapContentDownloadStatusWindow : public BaseNetworkContentDownloadStatusWindow {
public:
	/** Simple call the constructor of the superclass. */
	BootstrapContentDownloadStatusWindow() : BaseNetworkContentDownloadStatusWindow(&_bootstrap_download_status_window_desc)
	{
	}

	virtual void OnDownloadComplete(ContentID cid)
	{
		/* We have completed downloading. We can trigger finding the right set now. */
		BaseGraphics::FindSets();

		/* And continue going into the menu. */
		_game_mode = GM_MENU;

		/* _exit_game is used to break out of the outer video driver's MainLoop. */
		_exit_game = true;
		delete this;
	}
};

/** The widgets for the query. It has no close box as that sprite does not exist yet. */
static const NWidgetPart _bootstrap_query_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_MISSING_GRAPHICS_SET_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_BAFD_QUESTION), EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BAFD_YES), SetDataTip(STR_MISSING_GRAPHICS_YES_DOWNLOAD, STR_NULL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BAFD_NO), SetDataTip(STR_MISSING_GRAPHICS_NO_QUIT, STR_NULL),
		EndContainer(),
	EndContainer(),
};

/** The window description for the query. */
static WindowDesc _bootstrap_query_desc(
	WDP_CENTER, NULL, 0, 0,
	WC_CONFIRM_POPUP_QUERY, WC_NONE,
	0,
	_bootstrap_query_widgets, lengthof(_bootstrap_query_widgets)
);

/** The window for the query. It can't use the generic query window as that uses sprites that don't exist yet. */
class BootstrapAskForDownloadWindow : public Window, ContentCallback {
	Dimension button_size; ///< The dimension of the button

public:
	/** Start listening to the content client events. */
	BootstrapAskForDownloadWindow() : Window(&_bootstrap_query_desc)
	{
		this->InitNested(WN_CONFIRM_POPUP_QUERY_BOOTSTRAP);
		_network_content_client.AddCallback(this);
	}

	/** Stop listening to the content client events. */
	~BootstrapAskForDownloadWindow()
	{
		_network_content_client.RemoveCallback(this);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		/* We cache the button size. This is safe as no reinit can happen here. */
		if (this->button_size.width == 0) {
			this->button_size = maxdim(GetStringBoundingBox(STR_MISSING_GRAPHICS_YES_DOWNLOAD), GetStringBoundingBox(STR_MISSING_GRAPHICS_NO_QUIT));
			this->button_size.width += WD_FRAMETEXT_LEFT + WD_FRAMETEXT_RIGHT;
			this->button_size.height += WD_FRAMETEXT_TOP + WD_FRAMETEXT_BOTTOM;
		}

		switch (widget) {
			case WID_BAFD_QUESTION:
				/* The question is twice as wide as the buttons, and determine the height based on the width. */
				size->width = this->button_size.width * 2;
				size->height = GetStringHeight(STR_MISSING_GRAPHICS_SET_MESSAGE, size->width - WD_FRAMETEXT_LEFT - WD_FRAMETEXT_RIGHT) + WD_FRAMETEXT_BOTTOM + WD_FRAMETEXT_TOP;
				break;

			case WID_BAFD_YES:
			case WID_BAFD_NO:
				*size = this->button_size;
				break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != 0) return;

		DrawStringMultiLine(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, r.top + WD_FRAMETEXT_TOP, r.bottom - WD_FRAMETEXT_BOTTOM, STR_MISSING_GRAPHICS_SET_MESSAGE, TC_FROMSTRING, SA_CENTER);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_BAFD_YES:
				/* We got permission to connect! Yay! */
				_network_content_client.Connect();
				break;

			case WID_BAFD_NO:
				_exit_game = true;
				break;

			default:
				break;
		}
	}

	virtual void OnConnect(bool success)
	{
		/* Once connected, request the metadata. */
		_network_content_client.RequestContentList(CONTENT_TYPE_BASE_GRAPHICS);
	}

	virtual void OnReceiveContentInfo(const ContentInfo *ci)
	{
		/* And once the meta data is received, start downloading it. */
		_network_content_client.Select(ci->id);
		new BootstrapContentDownloadStatusWindow();
		delete this;
	}
};

#endif /* defined(ENABLE_NETWORK) && defined(WITH_FREETYPE) */

/**
 * Handle all procedures for bootstrapping OpenTTD without a base graphics set.
 * This requires all kinds of trickery that is needed to avoid the use of
 * sprites from the base graphics set which are pretty interwoven.
 * @return True if a base set exists, otherwise false.
 */
bool HandleBootstrap()
{
	if (BaseGraphics::GetUsedSet() != NULL) return true;

	/* No user interface, bail out with an error. */
	if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() == 0) goto failure;

	/* If there is no network or no freetype, then there is nothing we can do. Go straight to failure. */
#if defined(ENABLE_NETWORK) && defined(WITH_FREETYPE) && (defined(WITH_FONTCONFIG) || defined(WIN32) || defined(__APPLE__))
	if (!_network_available) goto failure;

	/* First tell the game we're bootstrapping. */
	_game_mode = GM_BOOTSTRAP;

	/* Initialise the freetype font code. */
	InitializeUnicodeGlyphMap();
	/* Next "force" finding a suitable freetype font as the local font is missing. */
	CheckForMissingGlyphs(false);

	/* Initialise the palette. The biggest step is 'faking' some recolour sprites.
	 * This way the mauve and gray colours work and we can show the user interface. */
	GfxInitPalettes();
	static const int offsets[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x80, 0, 0, 0, 0x04, 0x08 };
	for (uint i = 0; i != 16; i++) {
		for (int j = 0; j < 8; j++) {
			_colour_gradient[i][j] = offsets[i] + j;
		}
	}

	/* Finally ask the question. */
	new BootstrapBackground();
	new BootstrapAskForDownloadWindow();

	/* Process the user events. */
	VideoDriver::GetInstance()->MainLoop();

	/* _exit_game is used to get out of the video driver's main loop.
	 * In case GM_BOOTSTRAP is still set we did not exit it via the
	 * "download complete" event, so it was a manual exit. Obey it. */
	_exit_game = _game_mode == GM_BOOTSTRAP;
	if (_exit_game) return false;

	/* Try to probe the graphics. Should work this time. */
	if (!BaseGraphics::SetSet(NULL)) goto failure;

	/* Finally we can continue heading for the menu. */
	_game_mode = GM_MENU;
	return true;
#endif

	/* Failure to get enough working to get a graphics set. */
failure:
	usererror("Failed to find a graphics set. Please acquire a graphics set for OpenTTD. See section 4.1 of README.md.");
	return false;
}
