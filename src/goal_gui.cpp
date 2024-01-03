/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file goal_gui.cpp GUI for goals. */

#include "stdafx.h"
#include "industry.h"
#include "town.h"
#include "window_gui.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "gui.h"
#include "goal_base.h"
#include "core/geometry_func.hpp"
#include "company_func.h"
#include "company_base.h"
#include "company_gui.h"
#include "story_base.h"
#include "command_func.h"
#include "string_func.h"
#include "goal_cmd.h"

#include "widgets/goal_widget.h"

#include "table/strings.h"

#include "safeguards.h"

/** Goal list columns. */
enum GoalColumn {
	GC_GOAL = 0, ///< Goal text column.
	GC_PROGRESS, ///< Goal progress column.
};

/** Window for displaying goals. */
struct GoalListWindow : public Window {
	Scrollbar *vscroll; ///< Reference to the scrollbar widget.

	GoalListWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_GOAL_SCROLLBAR);
		this->FinishInitNested(window_number);
		this->owner = (Owner)this->window_number;
		NWidgetStacked *wi = this->GetWidget<NWidgetStacked>(WID_GOAL_SELECT_BUTTONS);
		wi->SetDisplayedPlane(window_number == INVALID_COMPANY ? 1 : 0);
		this->OnInvalidateData(0);
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget != WID_GOAL_CAPTION) return;

		if (this->window_number == INVALID_COMPANY) {
			SetDParam(0, STR_GOALS_SPECTATOR_CAPTION);
		} else {
			SetDParam(0, STR_GOALS_CAPTION);
			SetDParam(1, this->window_number);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_GOAL_GLOBAL_BUTTON:
				ShowGoalsList(INVALID_COMPANY);
				break;

			case WID_GOAL_COMPANY_BUTTON:
				ShowGoalsList(_local_company);
				break;

			case WID_GOAL_LIST: {
				int y = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_GOAL_LIST, WidgetDimensions::scaled.framerect.top);
				for (const Goal *s : Goal::Iterate()) {
					if (s->company == this->window_number) {
						if (y == 0) {
							this->HandleClick(s);
							return;
						}
						y--;
					}
				}
				break;
			}

			default:
				break;
		}
	}

	/**
	 * Handle clicking at a goal.
	 * @param s #Goal clicked at.
	 */
	void HandleClick(const Goal *s)
	{
		/* Determine dst coordinate for goal and try to scroll to it. */
		TileIndex xy;
		switch (s->type) {
			case GT_NONE: return;

			case GT_COMPANY:
				/* s->dst here is not a tile, but a CompanyID.
				 * Show the window with the overview of the company instead. */
				ShowCompany((CompanyID)s->dst);
				return;

			case GT_TILE:
				if (!IsValidTile(s->dst)) return;
				xy = s->dst;
				break;

			case GT_INDUSTRY:
				if (!Industry::IsValidID(s->dst)) return;
				xy = Industry::Get(s->dst)->location.tile;
				break;

			case GT_TOWN:
				if (!Town::IsValidID(s->dst)) return;
				xy = Town::Get(s->dst)->xy;
				break;

			case GT_STORY_PAGE: {
				if (!StoryPage::IsValidID(s->dst)) return;

				/* Verify that:
				 * - if global goal: story page must be global.
				 * - if company goal: story page must be global or of the same company.
				 */
				CompanyID goal_company = s->company;
				CompanyID story_company = StoryPage::Get(s->dst)->company;
				if (goal_company == INVALID_COMPANY ? story_company != INVALID_COMPANY : story_company != INVALID_COMPANY && story_company != goal_company) return;

				ShowStoryBook((CompanyID)this->window_number, s->dst);
				return;
			}

			default: NOT_REACHED();
		}

		if (_ctrl_pressed) {
			ShowExtraViewportWindow(xy);
		} else {
			ScrollMainWindowToTile(xy);
		}
	}

	/**
	 * Count the number of lines in this window.
	 * @return the number of lines.
	 */
	uint CountLines()
	{
		/* Count number of (non) awarded goals. */
		uint num = 0;
		for (const Goal *s : Goal::Iterate()) {
			if (s->company == this->window_number) num++;
		}

		/* Count the 'none' lines. */
		if (num == 0) num = 1;

		return num;
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget != WID_GOAL_LIST) return;
		Dimension d = GetStringBoundingBox(STR_GOALS_NONE);

		resize->width = 1;
		resize->height = d.height;

		d.height *= 5;
		d.width += WidgetDimensions::scaled.framerect.Horizontal();
		d.height += WidgetDimensions::scaled.framerect.Vertical();
		*size = maxdim(*size, d);
	}

	/**
	 * Draws a given column of the goal list.
	 * @param column Which column to draw.
	 * @param wid Pointer to the goal list widget.
	 * @param progress_col_width Width of the progress column.
	 * @return max width of drawn text
	 */
	void DrawListColumn(GoalColumn column, NWidgetBase *wid, uint progress_col_width) const
	{
		/* Get column draw area. */
		Rect r = wid->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
		bool rtl = _current_text_dir == TD_RTL;

		int pos = -this->vscroll->GetPosition();
		const int cap = this->vscroll->GetCapacity();

		uint num = 0;
		for (const Goal *s : Goal::Iterate()) {
			if (s->company == this->window_number) {
				if (IsInsideMM(pos, 0, cap)) {
					switch (column) {
						case GC_GOAL: {
							/* Display the goal. */
							SetDParamStr(0, s->text);
							uint width_reduction = progress_col_width > 0 ? progress_col_width + WidgetDimensions::scaled.framerect.Horizontal() : 0;
							DrawString(r.Indent(width_reduction, !rtl), STR_GOALS_TEXT);
							break;
						}

						case GC_PROGRESS:
							if (!s->progress.empty()) {
								SetDParamStr(0, s->progress);
								StringID str = s->completed ? STR_GOALS_PROGRESS_COMPLETE : STR_GOALS_PROGRESS;
								DrawString(r.WithWidth(progress_col_width, !rtl), str, TC_FROMSTRING, SA_RIGHT | SA_FORCE);
							}
							break;
					}
					r.top += GetCharacterHeight(FS_NORMAL);
				}
				pos++;
				num++;
			}
		}

		if (num == 0) {
			if (column == GC_GOAL && IsInsideMM(pos, 0, cap)) {
				DrawString(r, STR_GOALS_NONE);
			}
		}
	}

	void OnPaint() override
	{
		this->DrawWidgets();

		if (this->IsShaded()) return; // Don't draw anything when the window is shaded.

		/* Calculate progress column width. */
		uint max_width = 0;
		for (const Goal *s : Goal::Iterate()) {
			if (!s->progress.empty()) {
				SetDParamStr(0, s->progress);
				StringID str = s->completed ? STR_GOALS_PROGRESS_COMPLETE : STR_GOALS_PROGRESS;
				uint str_width = GetStringBoundingBox(str).width;
				if (str_width > max_width) max_width = str_width;
			}
		}

		NWidgetBase *wid = this->GetWidget<NWidgetBase>(WID_GOAL_LIST);
		uint progress_col_width = std::min(max_width, wid->current_x);

		/* Draw goal list. */
		this->DrawListColumn(GC_PROGRESS, wid, progress_col_width);
		this->DrawListColumn(GC_GOAL, wid, progress_col_width);

	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_GOAL_LIST, WidgetDimensions::scaled.framerect.Vertical());
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->vscroll->SetCount(this->CountLines());
		this->SetWidgetDisabledState(WID_GOAL_COMPANY_BUTTON, _local_company == COMPANY_SPECTATOR);
		this->SetWidgetDirty(WID_GOAL_COMPANY_BUTTON);
		this->SetWidgetDirty(WID_GOAL_LIST);
	}
};

/** Widgets of the #GoalListWindow. */
static const NWidgetPart _nested_goals_list_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_GOAL_CAPTION), SetDataTip(STR_JUST_STRING1, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_GOAL_SELECT_BUTTONS),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_GOAL_GLOBAL_BUTTON), SetMinimalSize(50, 0), SetDataTip(STR_GOALS_GLOBAL_BUTTON, STR_GOALS_GLOBAL_BUTTON_HELPTEXT),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_GOAL_COMPANY_BUTTON), SetMinimalSize(50, 0), SetDataTip(STR_GOALS_COMPANY_BUTTON, STR_GOALS_COMPANY_BUTTON_HELPTEXT),
		EndContainer(),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN, WID_GOAL_LIST), SetDataTip(0x0, STR_GOALS_TOOLTIP_CLICK_ON_SERVICE_TO_CENTER), SetScrollbar(WID_GOAL_SCROLLBAR), SetResize(1, 1), SetMinimalTextLines(2, 0),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_GOAL_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _goals_list_desc(__FILE__, __LINE__,
	WDP_AUTO, "list_goals", 500, 127,
	WC_GOALS_LIST, WC_NONE,
	0,
	std::begin(_nested_goals_list_widgets), std::end(_nested_goals_list_widgets)
);

/**
 * Open a goal list window.
 * @param company %Company to display the goals for, use #INVALID_COMPANY to display global goals.
 */
void ShowGoalsList(CompanyID company)
{
	if (!Company::IsValidID(company)) company = (CompanyID)INVALID_COMPANY;

	AllocateWindowDescFront<GoalListWindow>(&_goals_list_desc, company);
}

/** Ask a question about a goal. */
struct GoalQuestionWindow : public Window {
	std::string question; ///< Question to ask (private copy).
	int buttons;          ///< Number of valid buttons in #button.
	int button[3];        ///< Buttons to display.
	TextColour colour;    ///< Colour of the question text.

	GoalQuestionWindow(WindowDesc *desc, WindowNumber window_number, TextColour colour, uint32_t button_mask, const std::string &question) : Window(desc), colour(colour)
	{
		this->question = question;

		/* Figure out which buttons we have to enable. */
		int n = 0;
		for (uint bit : SetBitIterator(button_mask)) {
			if (bit >= GOAL_QUESTION_BUTTON_COUNT) break;
			this->button[n++] = bit;
			if (n == 3) break;
		}
		this->buttons = n;
		assert(this->buttons < 4);

		this->CreateNestedTree();
		if (this->buttons == 0) {
			this->GetWidget<NWidgetStacked>(WID_GQ_BUTTONS)->SetDisplayedPlane(SZSP_HORIZONTAL);
		} else {
			this->GetWidget<NWidgetStacked>(WID_GQ_BUTTONS)->SetDisplayedPlane(this->buttons - 1);
		}
		this->FinishInitNested(window_number);
	}


	void SetStringParameters(WidgetID widget) const override
	{
		switch (widget) {
			case WID_GQ_BUTTON_1:
				SetDParam(0, STR_GOAL_QUESTION_BUTTON_CANCEL + this->button[0]);
				break;

			case WID_GQ_BUTTON_2:
				SetDParam(0, STR_GOAL_QUESTION_BUTTON_CANCEL + this->button[1]);
				break;

			case WID_GQ_BUTTON_3:
				SetDParam(0, STR_GOAL_QUESTION_BUTTON_CANCEL + this->button[2]);
				break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_GQ_BUTTON_1:
				Command<CMD_GOAL_QUESTION_ANSWER>::Post(this->window_number, this->button[0]);
				this->Close();
				break;

			case WID_GQ_BUTTON_2:
				Command<CMD_GOAL_QUESTION_ANSWER>::Post(this->window_number, this->button[1]);
				this->Close();
				break;

			case WID_GQ_BUTTON_3:
				Command<CMD_GOAL_QUESTION_ANSWER>::Post(this->window_number, this->button[2]);
				this->Close();
				break;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget != WID_GQ_QUESTION) return;

		SetDParamStr(0, this->question);
		size->height = GetStringHeight(STR_JUST_RAW_STRING, size->width);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_GQ_QUESTION) return;

		SetDParamStr(0, this->question);
		DrawStringMultiLine(r, STR_JUST_RAW_STRING, this->colour, SA_TOP | SA_HOR_CENTER);
	}
};

/** Widgets of the goal question window. */
static const NWidgetPart _nested_goal_question_widgets_question[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE, WID_GQ_CAPTION), SetDataTip(STR_GOAL_QUESTION_CAPTION_QUESTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE),
		NWidget(NWID_VERTICAL), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GQ_QUESTION), SetMinimalSize(300, 0), SetFill(1, 0),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_GQ_BUTTONS),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(85, WidgetDimensions::unscaled.hsep_wide, 85), SetPadding(WidgetDimensions::unscaled.vsep_wide, 0, 0, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_1), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(65, WidgetDimensions::unscaled.hsep_wide, 65), SetPadding(WidgetDimensions::unscaled.vsep_wide, 0, 0, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_1), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_2), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(25, WidgetDimensions::unscaled.hsep_wide, 25), SetPadding(WidgetDimensions::unscaled.vsep_wide, 0, 0, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_1), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_2), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_3), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const NWidgetPart _nested_goal_question_widgets_info[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE, WID_GQ_CAPTION), SetDataTip(STR_GOAL_QUESTION_CAPTION_INFORMATION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE),
		NWidget(NWID_VERTICAL), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GQ_QUESTION), SetMinimalSize(300, 0), SetFill(1, 0),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_GQ_BUTTONS),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(85, WidgetDimensions::unscaled.hsep_wide, 85), SetPadding(WidgetDimensions::unscaled.vsep_wide, 0, 0, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_1), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(65, WidgetDimensions::unscaled.hsep_wide, 65), SetPadding(WidgetDimensions::unscaled.vsep_wide, 0, 0, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_1), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_2), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(25, WidgetDimensions::unscaled.hsep_wide, 25), SetPadding(WidgetDimensions::unscaled.vsep_wide, 0, 0, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_1), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_2), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_3), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const NWidgetPart _nested_goal_question_widgets_warning[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_YELLOW),
		NWidget(WWT_CAPTION, COLOUR_YELLOW, WID_GQ_CAPTION), SetDataTip(STR_GOAL_QUESTION_CAPTION_WARNING, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_YELLOW),
		NWidget(NWID_VERTICAL), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GQ_QUESTION), SetMinimalSize(300, 0), SetFill(1, 0),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_GQ_BUTTONS),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(85, WidgetDimensions::unscaled.hsep_wide, 85), SetPadding(WidgetDimensions::unscaled.vsep_wide, 0, 0, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GQ_BUTTON_1), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(65, WidgetDimensions::unscaled.hsep_wide, 65), SetPadding(WidgetDimensions::unscaled.vsep_wide, 0, 0, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GQ_BUTTON_1), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GQ_BUTTON_2), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(25, WidgetDimensions::unscaled.hsep_wide, 25), SetPadding(WidgetDimensions::unscaled.vsep_wide, 0, 0, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GQ_BUTTON_1), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GQ_BUTTON_2), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GQ_BUTTON_3), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const NWidgetPart _nested_goal_question_widgets_error[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_RED),
		NWidget(WWT_CAPTION, COLOUR_RED, WID_GQ_CAPTION), SetDataTip(STR_GOAL_QUESTION_CAPTION_ERROR, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_RED),
		NWidget(NWID_VERTICAL), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GQ_QUESTION), SetMinimalSize(300, 0), SetFill(1, 0),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_GQ_BUTTONS),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(85, WidgetDimensions::unscaled.hsep_wide, 85), SetPadding(WidgetDimensions::unscaled.vsep_wide, 0, 0, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GQ_BUTTON_1), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(65, WidgetDimensions::unscaled.hsep_wide, 65), SetPadding(WidgetDimensions::unscaled.vsep_wide, 0, 0, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GQ_BUTTON_1), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GQ_BUTTON_2), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(25, WidgetDimensions::unscaled.hsep_wide, 25), SetPadding(WidgetDimensions::unscaled.vsep_wide, 0, 0, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GQ_BUTTON_1), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GQ_BUTTON_2), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GQ_BUTTON_3), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _goal_question_list_desc[] = {
	{
		__FILE__, __LINE__,
		WDP_CENTER, nullptr, 0, 0,
		WC_GOAL_QUESTION, WC_NONE,
		WDF_CONSTRUCTION,
		std::begin(_nested_goal_question_widgets_question), std::end(_nested_goal_question_widgets_question),
	},
	{
		__FILE__, __LINE__,
		WDP_CENTER, nullptr, 0, 0,
		WC_GOAL_QUESTION, WC_NONE,
		WDF_CONSTRUCTION,
		std::begin(_nested_goal_question_widgets_info), std::end(_nested_goal_question_widgets_info),
	},
	{
		__FILE__, __LINE__,
		WDP_CENTER, nullptr, 0, 0,
		WC_GOAL_QUESTION, WC_NONE,
		WDF_CONSTRUCTION,
		std::begin(_nested_goal_question_widgets_warning), std::end(_nested_goal_question_widgets_warning),
	},
	{
		__FILE__, __LINE__,
		WDP_CENTER, nullptr, 0, 0,
		WC_GOAL_QUESTION, WC_NONE,
		WDF_CONSTRUCTION,
		std::begin(_nested_goal_question_widgets_error), std::end(_nested_goal_question_widgets_error),
	},
};

/**
 * Display a goal question.
 * @param id Window number to use.
 * @param type Type of question.
 * @param button_mask Buttons to display.
 * @param question Question to ask.
 */
void ShowGoalQuestion(uint16_t id, byte type, uint32_t button_mask, const std::string &question)
{
	assert(type < GQT_END);
	new GoalQuestionWindow(&_goal_question_list_desc[type], id, type == 3 ? TC_WHITE : TC_BLACK, button_mask, question);
}
