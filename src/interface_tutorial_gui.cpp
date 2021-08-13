/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file interface_tutorial_gui.cpp The GUI for the interface tutorials. */

#include "stdafx.h"
#include "gfx_func.h"
#include "settings_type.h"
#include "strings_func.h"
#include "interface_tutorial_gui.h"
#include "interface_tutorial_type.h"
#include "window_gui.h"
#include "window_func.h"

#include "widgets/network_widget.h"

#include "widgets/interface_tutorial_widget.h"

#include "debug.h"

#include "safeguards.h"

struct InterfaceTutorialStepData {
	InterfaceTutorialStep step;
	StringID caption_string_id;
	StringID text_string_id;
	WindowClass window;
	uint32 number;
	uint8 widget;
	bool *setting;
};

static const InterfaceTutorialStepData tutorial_steps[] = {
	{
		INTERFACE_TUTORIAL_MULTIPLAYER_JOIN,
		STR_INTERFACE_TUTORIAL_MULTIPLAYER_JOIN_CAPTION,
		STR_INTERFACE_TUTORIAL_MULTIPLAYER_JOIN_TEXT,
		WC_CLIENT_LIST, 0,
		WID_CL_MATRIX,
		&_settings_client.tutorial.multiplayer_join
	},
};

struct InterfaceTutorialWindow : Window {
	const InterfaceTutorialStepData &data;

	InterfaceTutorialWindow(WindowDesc *desc, const InterfaceTutorialStepData &data) : Window(desc), data(data)
	{
		this->InitNested();

		Window *w = FindWindowById(this->data.window, this->data.number);
		if (w != nullptr) {
			w->SetWidgetHighlight(this->data.widget, TC_LIGHT_BLUE);
		}
	}

	void Close() override
	{
		/* Mark this tutorial as seen, so it doesn't open next time. */
		*this->data.setting = true;

		Window *w = FindWindowById(this->data.window, this->data.number);
		if (w != nullptr) {
			w->SetWidgetHighlight(this->data.widget, TC_INVALID);
		}

		this->Window::Close();
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		if (widget == WID_IT_TEXT) {
			*size = GetStringBoundingBox(STR_INTERFACE_TUTORIAL_TEXT);
			size->height = GetStringHeight(STR_INTERFACE_TUTORIAL_TEXT, size->width - WD_FRAMETEXT_LEFT - WD_FRAMETEXT_RIGHT) + WD_FRAMETEXT_BOTTOM + WD_FRAMETEXT_TOP;
		}
	}

	void SetStringParameters(int widget) const override
	{
		switch (widget) {
			case WID_IT_CAPTION:
				SetDParam(0, this->data.caption_string_id);
				break;

			case WID_IT_TEXT:
				SetDParam(0, this->data.text_string_id);
				break;
		}
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		if (widget != WID_IT_TEXT) return;

		DrawStringMultiLine(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, r.top + WD_FRAMETEXT_TOP, r.bottom - WD_FRAMETEXT_BOTTOM, STR_INTERFACE_TUTORIAL_TEXT, TC_FROMSTRING, SA_CENTER);
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		switch (widget) {
			case WID_IT_CLOSE:
				this->Close();
				break;
		}
	}
};

static const NWidgetPart _nested_interface_tutorial_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE, WID_IT_CAPTION), SetDataTip(STR_INTERFACE_TUTORIAL_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE),
		NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, WID_IT_TEXT), SetFill(1, 1),
	EndContainer(),
	NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_IT_CLOSE), SetMinimalSize(71, 12), SetFill(1, 1), SetDataTip(STR_INTERFACE_TUTORIAL_CLOSE, STR_NULL),
};

static WindowDesc _interface_tutorial_desc(
	WDP_CENTER, nullptr, 0, 0,
	WC_INTERFACE_TUTORIAL, WC_NONE,
	0,
	_nested_interface_tutorial_widgets, lengthof(_nested_interface_tutorial_widgets)
);

void ShowInterfaceTutorial(InterfaceTutorialStep step)
{
	if (step >= INTERFACE_TUTORIAL_END) return;
	if (*tutorial_steps[step].setting) return;

	new InterfaceTutorialWindow(&_interface_tutorial_desc, tutorial_steps[step]);
}
