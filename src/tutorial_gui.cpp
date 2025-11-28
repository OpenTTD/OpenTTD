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
#include "gfx_func.h"
#include"settings_type.h"

#include "table/strings.h"

#include "safeguards.h"

struct TutorialPages 
{
	uint index=0;
	StringID title_id;
	StringID body_id;
	std::string image_path;
};

/** Tutorial window structure */
struct TutorialWindow : public Window {
	std::vector<TutorialPages> tutorial_pages{};
	uint current_page_index = 0;

	TutorialWindow(WindowDesc &desc) : Window(desc)
	{
		this->CreateNestedTree();
		this->FinishInitNested(0);
		LoadPagesFromStrings();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_TUT_PREVIOUS:
				// Show previous page
				current_page_index = (current_page_index > 0) ? current_page_index - 1 : 0;
				UpdateUIForPage(current_page_index);
				break;
			
			case WID_TUT_NEXT:
				//Show next page
				current_page_index = (current_page_index< 6) ? current_page_index + 1 : 6;
				UpdateUIForPage(current_page_index);
				break;
			
			case WID_TUT_CLOSE:
			case WID_TUT_FINISH:
				this->Close();
				break;
			
			case WID_TUT_DONT_SHOW:
				//Save preference
				_settings_client.gui.tutorial_completed = true;
				WindowDesc::SaveToConfig();
				this->Close();
				break;
		}
	}

void DrawWidget(const Rect&r,WidgetID widget)const override
{
	if (widget!= WID_TUT_CONTENT) return;
	if (current_page_index >= tutorial_pages.size()) return;
	const TutorialPages &page = tutorial_pages[current_page_index];
	Rect text_rect=r.Shrink(5);
	int y=text_rect.top;
    DrawString(text_rect.left,text_rect.right,y+1, page.title_id,TC_BLACK,SA_HOR_CENTER,false,FS_LARGE);
	y+= GetCharacterHeight(FS_LARGE)+5;
	DrawStringMultiLine(text_rect.left, text_rect.right,y,text_rect.bottom, page.body_id, TC_BLACK, SA_LEFT, false, FS_NORMAL);
}

void LoadPagesFromStrings()
{
	tutorial_pages.clear();
	for(uint i = 0;i<6; i++) {
		StringID title_id = static_cast<StringID>(STR_TUTORIAL_PAGE_1_TITLE + i * 2);
		StringID body_id = static_cast<StringID>(STR_TUTORIAL_PAGE_1_BODY + i * 2);
		
		TutorialPages page;
		page.index = i;
		page.title_id = title_id;
		page.body_id = body_id;
		page.image_path="";// Placeholder for image path if needed in future
		
		tutorial_pages.push_back(page);
	}
}

void UpdateUIForPage(uint index)
{
	if(index >= tutorial_pages.size()) return;
	this->SetWidgetDirty(WID_TUT_CONTENT);
};


};
/** Tutorial window widgets definition */
static constexpr std::initializer_list<NWidgetPart> _nested_tutorial_widgets = {
	NWidget(WWT_CAPTION, COLOUR_GREY, WID_TUT_CAPTION), SetStringTip(STR_TUTORIAL_TITLE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_TUT_PANEL),
		NWidget(NWID_VERTICAL), SetPadding(10),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_TUT_CONTENT), SetMinimalSize(400, 300), SetFill(1, 1),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TUT_PREVIOUS), SetMinimalSize(80, 20), SetStringTip(STR_TUTORIAL_PREV, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TUT_NEXT), SetMinimalSize(80, 20), SetStringTip(STR_TUTORIAL_NEXT, STR_NULL),
				NWidget(NWID_SPACER), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TUT_CLOSE), SetMinimalSize(80, 20), SetStringTip(STR_TUTORIAL_FINISH, STR_TOOLTIP_CLOSE_WINDOW),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TUT_DONT_SHOW), SetMinimalSize(150, 20), SetStringTip(STR_TUTORIAL_DONT_SHOW_AGAIN, STR_NULL),
				NWidget(NWID_SPACER), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_TUT_FINISH), SetMinimalSize(80, 20), SetStringTip(STR_TUTORIAL_CLOSING_NOTE, STR_NULL),
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
	if (_settings_client.gui.tutorial_completed) {
        return;
    }
	CloseWindowByClass(WC_TUTORIAL);
	new TutorialWindow(_tutorial_window_desc);
}

