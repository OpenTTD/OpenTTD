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
#include "error_func.h"

#if defined(WITH_FREETYPE) || defined(WITH_UNISCRIBE) || defined(WITH_COCOA)

#include "core/geometry_func.hpp"
#include "error.h"
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
	EndContainer(),
};

/**
 * Window description for the background window to prevent smearing.
 */
static WindowDesc _background_desc(__FILE__, __LINE__,
	WDP_MANUAL, nullptr, 0, 0,
	WC_BOOTSTRAP, WC_NONE,
	WDF_NO_CLOSE,
	std::begin(_background_widgets), std::end(_background_widgets)
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

	void DrawWidget(const Rect &r, WidgetID) const override
	{
		GfxFillRect(r.left, r.top, r.right, r.bottom, 4, FILLRECT_OPAQUE);
		GfxFillRect(r.left, r.top, r.right, r.bottom, 0, FILLRECT_CHECKER);
	}
};

/** Nested widgets for the error window. */
static const NWidgetPart _nested_bootstrap_errmsg_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_BEM_CAPTION), SetDataTip(STR_MISSING_GRAPHICS_ERROR_TITLE, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_BEM_MESSAGE), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BEM_QUIT), SetDataTip(STR_MISSING_GRAPHICS_ERROR_QUIT, STR_NULL), SetFill(1, 0),
	EndContainer(),
};

/** Window description for the error window. */
static WindowDesc _bootstrap_errmsg_desc(__FILE__, __LINE__,
	WDP_CENTER, nullptr, 0, 0,
	WC_BOOTSTRAP, WC_NONE,
	WDF_MODAL | WDF_NO_CLOSE,
	std::begin(_nested_bootstrap_errmsg_widgets), std::end(_nested_bootstrap_errmsg_widgets)
);

/** The window for a failed bootstrap. */
class BootstrapErrorWindow : public Window {
public:
	BootstrapErrorWindow() : Window(&_bootstrap_errmsg_desc)
	{
		this->InitNested(1);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		_exit_game = true;
		this->Window::Close();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget == WID_BEM_MESSAGE) {
			*size = GetStringBoundingBox(STR_MISSING_GRAPHICS_ERROR);
			size->width += WidgetDimensions::scaled.frametext.Horizontal();
			size->height += WidgetDimensions::scaled.frametext.Vertical();
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget == WID_BEM_MESSAGE) {
			DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.frametext), STR_MISSING_GRAPHICS_ERROR, TC_FROMSTRING, SA_CENTER);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget == WID_BEM_QUIT) {
			_exit_game = true;
		}
	}
};

/** Nested widgets for the download window. */
static const NWidgetPart _nested_bootstrap_download_status_window_widgets[] = {
	NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_CONTENT_DOWNLOAD_TITLE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_NCDS_PROGRESS_BAR), SetFill(1, 0),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_NCDS_PROGRESS_TEXT), SetFill(1, 0), SetMinimalSize(350, 0),
		EndContainer(),
	EndContainer(),
};

/** Window description for the download window */
static WindowDesc _bootstrap_download_status_window_desc(__FILE__, __LINE__,
	WDP_CENTER, nullptr, 0, 0,
	WC_NETWORK_STATUS_WINDOW, WC_NONE,
	WDF_MODAL | WDF_NO_CLOSE,
	std::begin(_nested_bootstrap_download_status_window_widgets), std::end(_nested_bootstrap_download_status_window_widgets)
);


/** Window for showing the download status of content */
struct BootstrapContentDownloadStatusWindow : public BaseNetworkContentDownloadStatusWindow {
public:
	/** Simple call the constructor of the superclass. */
	BootstrapContentDownloadStatusWindow() : BaseNetworkContentDownloadStatusWindow(&_bootstrap_download_status_window_desc)
	{
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		/* If we are not set to exit the game, it means the bootstrap failed. */
		if (!_exit_game) {
			new BootstrapErrorWindow();
		}
		this->BaseNetworkContentDownloadStatusWindow::Close();
	}

	void OnDownloadComplete(ContentID) override
	{
		/* We have completed downloading. We can trigger finding the right set now. */
		BaseGraphics::FindSets();

		/* And continue going into the menu. */
		_game_mode = GM_MENU;

		/* _exit_game is used to break out of the outer video driver's MainLoop. */
		_exit_game = true;
		this->Close();
	}
};

/** The widgets for the query. It has no close box as that sprite does not exist yet. */
static const NWidgetPart _bootstrap_query_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_MISSING_GRAPHICS_SET_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_BAFD_QUESTION), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BAFD_YES), SetDataTip(STR_MISSING_GRAPHICS_YES_DOWNLOAD, STR_NULL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BAFD_NO), SetDataTip(STR_MISSING_GRAPHICS_NO_QUIT, STR_NULL),
	EndContainer(),
};

/** The window description for the query. */
static WindowDesc _bootstrap_query_desc(__FILE__, __LINE__,
	WDP_CENTER, nullptr, 0, 0,
	WC_CONFIRM_POPUP_QUERY, WC_NONE,
	WDF_NO_CLOSE,
	std::begin(_bootstrap_query_widgets), std::end(_bootstrap_query_widgets)
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
	void Close([[maybe_unused]] int data = 0) override
	{
		_network_content_client.RemoveCallback(this);
		this->Window::Close();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		/* We cache the button size. This is safe as no reinit can happen here. */
		if (this->button_size.width == 0) {
			this->button_size = maxdim(GetStringBoundingBox(STR_MISSING_GRAPHICS_YES_DOWNLOAD), GetStringBoundingBox(STR_MISSING_GRAPHICS_NO_QUIT));
			this->button_size.width += WidgetDimensions::scaled.frametext.Horizontal();
			this->button_size.height += WidgetDimensions::scaled.frametext.Vertical();
		}

		switch (widget) {
			case WID_BAFD_QUESTION:
				/* The question is twice as wide as the buttons, and determine the height based on the width. */
				size->width = this->button_size.width * 2;
				size->height = GetStringHeight(STR_MISSING_GRAPHICS_SET_MESSAGE, size->width - WidgetDimensions::scaled.frametext.Horizontal()) + WidgetDimensions::scaled.frametext.Vertical();
				break;

			case WID_BAFD_YES:
			case WID_BAFD_NO:
				*size = this->button_size;
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_BAFD_QUESTION) return;

		DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.frametext), STR_MISSING_GRAPHICS_SET_MESSAGE, TC_FROMSTRING, SA_CENTER);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
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

	void OnConnect(bool success) override
	{
		if (!success) {
			UserError("Failed to connect to content server. Please acquire a graphics set for OpenTTD. See section 1.4 of README.md.");
			/* _exit_game is used to break out of the outer video driver's MainLoop. */
			_exit_game = true;
			this->Close();
			return;
		}

		/* Once connected, request the metadata. */
		_network_content_client.RequestContentList(CONTENT_TYPE_BASE_GRAPHICS);
	}

	void OnReceiveContentInfo(const ContentInfo *ci) override
	{
		/* And once the meta data is received, start downloading it. */
		_network_content_client.Select(ci->id);
		new BootstrapContentDownloadStatusWindow();
		this->Close();
	}
};

#endif /* defined(WITH_FREETYPE) */

#if defined(__EMSCRIPTEN__)
#	include <emscripten.h>
#	include "network/network.h"
#	include "network/network_content.h"
#	include "openttd.h"
#	include "video/video_driver.hpp"

class BootstrapEmscripten : public ContentCallback {
	bool downloading{false};
	uint total_files{0};
	uint total_bytes{0};
	uint downloaded_bytes{0};

public:
	BootstrapEmscripten()
	{
		_network_content_client.AddCallback(this);
		_network_content_client.Connect();
	}

	~BootstrapEmscripten()
	{
		_network_content_client.RemoveCallback(this);
	}

	void OnConnect(bool success) override
	{
		if (!success) {
			EM_ASM({ if (window["openttd_bootstrap_failed"]) openttd_bootstrap_failed(); });
			return;
		}

		/* Once connected, request the metadata. */
		_network_content_client.RequestContentList(CONTENT_TYPE_BASE_GRAPHICS);
	}

	void OnReceiveContentInfo(const ContentInfo *ci) override
	{
		if (this->downloading) return;

		/* And once the metadata is received, start downloading it. */
		_network_content_client.Select(ci->id);
		_network_content_client.DownloadSelectedContent(this->total_files, this->total_bytes);
		this->downloading = true;

		EM_ASM({ if (window["openttd_bootstrap"]) openttd_bootstrap($0, $1); }, this->downloaded_bytes, this->total_bytes);
	}

	void OnDownloadProgress(const ContentInfo *, int bytes) override
	{
		/* A negative value means we are resetting; for example, when retrying or using a fallback. */
		if (bytes < 0) {
			this->downloaded_bytes = 0;
		} else {
			this->downloaded_bytes += bytes;
		}

		EM_ASM({ if (window["openttd_bootstrap"]) openttd_bootstrap($0, $1); }, this->downloaded_bytes, this->total_bytes);
	}

	void OnDownloadComplete(ContentID) override
	{
		/* _exit_game is used to break out of the outer video driver's MainLoop. */
		_exit_game = true;

		delete this;
	}
};
#endif /* __EMSCRIPTEN__ */

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
#if defined(__EMSCRIPTEN__) || (defined(_WIN32) && defined(WITH_UNISCRIBE)) || (defined(WITH_FREETYPE) && (defined(WITH_FONTCONFIG) || defined(__APPLE__))) || defined(WITH_COCOA)
	if (!_network_available) goto failure;

	/* First tell the game we're bootstrapping. */
	_game_mode = GM_BOOTSTRAP;

#if defined(__EMSCRIPTEN__)
	new BootstrapEmscripten();
#else
	/* Initialise the font cache. */
	InitializeUnicodeGlyphMap();
	/* Next "force" finding a suitable non-sprite font as the local font is missing. */
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
#endif /* __EMSCRIPTEN__ */

	/* Process the user events. */
	VideoDriver::GetInstance()->MainLoop();

	/* _exit_game is used to get out of the video driver's main loop.
	 * In case GM_BOOTSTRAP is still set we did not exit it via the
	 * "download complete" event, so it was a manual exit. Obey it. */
	_exit_game = _game_mode == GM_BOOTSTRAP;
	if (_exit_game) return false;

	/* Try to probe the graphics. Should work this time. */
	if (!BaseGraphics::SetSet({})) goto failure;

	/* Finally we can continue heading for the menu. */
	_game_mode = GM_MENU;
	return true;
#endif

	/* Failure to get enough working to get a graphics set. */
failure:
	UserError("Failed to find a graphics set. Please acquire a graphics set for OpenTTD. See section 1.4 of README.md.");
	return false;
}
