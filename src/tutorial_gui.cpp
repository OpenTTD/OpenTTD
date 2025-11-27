/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tutorial_gui.cpp GUI for tutorial window. */

#include "stdafx.h"
#include "tutorial_gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "widgets/tutorial_widget.h"

#include "table/strings.h"

#include "safeguards.h"


/** Tutorial window structure */
struct TutorialWindow : public Window {
	TutorialWindow(WindowDesc &desc) : Window(desc)
	{
		this->CreateNestedTree();
		this->FinishInitNested(0);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_TUT_PREVIOUS:
				// TODO: Show previous page
				break;
			
			case WID_TUT_NEXT:
				// TODO: Show next page
				break;
			
			case WID_TUT_CLOSE:
			case WID_TUT_FINISH:
				this->Close();
				break;
			
			case WID_TUT_DONT_SHOW:
				// TODO: Save preference
				this->Close();
				break;
		}
	}
};

/** Tutorial window widgets definition */
static constexpr std::initializer_list<NWidgetPart> _nested_tutorial_widgets = {
	NWidget(WWT_CAPTION, COLOUR_GREY, WID_TUT_CAPTION), SetStringTip(STR_INTRO_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_TUT_PANEL),
		NWidget(NWID_VERTICAL), SetPadding(10),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_TUT_CONTENT), SetMinimalSize(400, 300), SetFill(1, 1),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TUT_PREVIOUS), SetMinimalSize(80, 20), SetStringTip(STR_INTRO_HIGHSCORE, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TUT_NEXT), SetMinimalSize(80, 20), SetStringTip(STR_INTRO_PLAY_SCENARIO, STR_NULL),
				NWidget(NWID_SPACER), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TUT_CLOSE), SetMinimalSize(80, 20), SetStringTip(STR_INTRO_QUIT, STR_TOOLTIP_CLOSE_WINDOW),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TUT_DONT_SHOW), SetMinimalSize(150, 20), SetStringTip(STR_INTRO_GAME_OPTIONS, STR_NULL),
				NWidget(NWID_SPACER), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TUT_FINISH), SetMinimalSize(80, 20), SetStringTip(STR_INTRO_QUIT, STR_NULL),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** Tutorial window description */
static WindowDesc _tutorial_window_desc(
	WDP_CENTER, {}, 0, 0,
	WC_TUTORIAL, WC_NONE,
	{},
	_nested_tutorial_widgets
);

/**
 * Show the tutorial window.
 */
void ShowTutorialWindow()
{
	CloseWindowByClass(WC_TUTORIAL);
	new TutorialWindow(_tutorial_window_desc);
}
