/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

 /** @file screenshot_gui.cpp GUI functions related to screenshots. */

#include "stdafx.h"
#include "gui.h"
#include "viewport_func.h"
#include "window_func.h"
#include "window_gui.h"
#include "screenshot.h"
#include "textbuf_gui.h"

#include "widgets/screenshot_widget.h"

#include "table/strings.h"

static ScreenshotType _screenshot_type;

struct ScreenshotWindow : Window {
	ScreenshotWindow(WindowDesc *desc) : Window(desc) {
		this->CreateNestedTree();
		this->FinishInitNested();
	}

	void OnPaint() override {
		this->DrawWidgets();
	}

	void OnClick(Point pt, int widget, int click_count) override {
		if (widget < 0) return;
		ScreenshotType st;
		switch (widget) {
			default:
			case WID_SC_TAKE:             st = SC_VIEWPORT;    break;
			case WID_SC_TAKE_ZOOMIN:      st = SC_ZOOMEDIN;    break;
			case WID_SC_TAKE_DEFAULTZOOM: st = SC_DEFAULTZOOM; break;
			case WID_SC_TAKE_WORLD:       st = SC_WORLD;       break;
			case WID_SC_TAKE_HEIGHTMAP:   st = SC_HEIGHTMAP;   break;
		}
		TakeScreenshot(st);
	}

	/**
	 * Make a screenshot.
	 * Ask for confirmation if the screenshot will be huge.
	 * @param t Screenshot type: World, defaultzoom, heightmap or viewport screenshot
	 */
	static void TakeScreenshot(ScreenshotType st) {
		ViewPort vp;
		SetupScreenshotViewport(st, &vp);
		if ((uint64)vp.width * (uint64)vp.height > 8192 * 8192) {
			/* Ask for confirmation */
			_screenshot_type = st;
			ShowQuery(STR_WARNING_SCREENSHOT_SIZE_CAPTION, STR_WARNING_SCREENSHOT_SIZE_MESSAGE, nullptr, ScreenshotConfirmationCallback);
		}
		else {
			/* Less than 64M pixels, just do it */
			MakeScreenshot(st, nullptr);
		}
	}

	/**
	 * Callback on the confirmation window for huge screenshots.
	 * @param w Window with viewport
	 * @param confirmed true on confirmation
	 */
	static void ScreenshotConfirmationCallback(Window *w, bool confirmed) {
		if (confirmed) MakeScreenshot(_screenshot_type, nullptr);
	}
};

static const NWidgetPart _nested_screenshot[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_SCREENSHOT_CAPTION, 0),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_VERTICAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SC_TAKE), SetFill(1, 1), SetDataTip(STR_SCREENSHOT_SCREENSHOT, 0), SetMinimalTextLines(2, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SC_TAKE_ZOOMIN), SetFill(1, 1), SetDataTip(STR_SCREENSHOT_ZOOMIN_SCREENSHOT, 0), SetMinimalTextLines(2, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SC_TAKE_DEFAULTZOOM), SetFill(1, 1), SetDataTip(STR_SCREENSHOT_DEFAULTZOOM_SCREENSHOT, 0), SetMinimalTextLines(2, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SC_TAKE_WORLD), SetFill(1, 1), SetDataTip(STR_SCREENSHOT_WORLD_SCREENSHOT, 0), SetMinimalTextLines(2, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SC_TAKE_HEIGHTMAP), SetFill(1, 1), SetDataTip(STR_SCREENSHOT_HEIGHTMAP_SCREENSHOT, 0), SetMinimalTextLines(2, 0),
	EndContainer(),
};

static WindowDesc _screenshot_window_desc(
	WDP_AUTO, "take_a_screenshot", 200, 100,
	WC_SCREENSHOT, WC_NONE,
	0,
	_nested_screenshot, lengthof(_nested_screenshot)
);

void ShowScreenshotWindow() {
	DeleteWindowById(WC_SCREENSHOT, 0);
	new ScreenshotWindow(&_screenshot_window_desc);
}
