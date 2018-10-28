/* $Id$ */

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
#include "date_func.h"
#include "viewport_func.h"
#include "gui.h"
#include "goal_base.h"
#include "core/geometry_func.hpp"
#include "company_func.h"
#include "company_base.h"
#include "story_base.h"
#include "command_func.h"
#include "string_func.h"

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
		this->OnInvalidateData(0);
	}

	/* virtual */ void SetStringParameters(int widget) const
	{
		if (widget != WID_GOAL_CAPTION) return;

		if (this->window_number == INVALID_COMPANY) {
			SetDParam(0, STR_GOALS_SPECTATOR_CAPTION);
		} else {
			SetDParam(0, STR_GOALS_CAPTION);
			SetDParam(1, this->window_number);
		}
	}

	/* virtual */ void OnClick(Point pt, int widget, int click_count)
	{
		if (widget != WID_GOAL_LIST) return;

		int y = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_GOAL_LIST, WD_FRAMERECT_TOP);
		int num = 0;
		const Goal *s;
		FOR_ALL_GOALS(s) {
			if (s->company == INVALID_COMPANY) {
				y--;
				if (y == 0) {
					this->HandleClick(s);
					return;
				}
				num++;
			}
		}

		if (num == 0) {
			y--; // "None" line.
			if (y < 0) return;
		}

		y -= 2; // "Company specific goals:" line.
		if (y < 0) return;

		FOR_ALL_GOALS(s) {
			if (s->company == this->window_number) {
				y--;
				if (y == 0) {
					this->HandleClick(s);
					return;
				}
			}
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
			case GT_COMPANY: return;

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
			ShowExtraViewPortWindow(xy);
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
		uint num_global = 0;
		uint num_company = 0;
		const Goal *s;
		FOR_ALL_GOALS(s) {
			if (s->company == INVALID_COMPANY) {
				num_global++;
			} else if (s->company == this->window_number) {
				num_company++;
			}
		}

		/* Count the 'none' lines. */
		if (num_global  == 0) num_global = 1;
		if (num_company == 0) num_company = 1;

		/* Global, company and an empty line before the accepted ones. */
		return 3 + num_global + num_company;
	}

	/* virtual */ void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != WID_GOAL_LIST) return;
		Dimension d = maxdim(GetStringBoundingBox(STR_GOALS_GLOBAL_TITLE), GetStringBoundingBox(STR_GOALS_COMPANY_TITLE));

		resize->height = d.height;

		d.height *= 5;
		d.width += padding.width + WD_FRAMERECT_RIGHT + WD_FRAMERECT_LEFT;
		d.height += padding.height + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
		*size = maxdim(*size, d);
	}

	/**
	 * Draws either the global goals or the company goal section.
	 * This is a helper method for #DrawWidget.
	 * @param[in,out] pos Vertical line number to draw.
	 * @param cap Number of lines to draw in the window.
	 * @param x Left edge of the text line to draw.
	 * @param y Vertical position of the top edge of the window.
	 * @param right Right edge of the text line to draw.
	 * @param global_section Whether the global goals are printed.
	 * @param column Which column to draw.
	 */
	void DrawPartialGoalList(int &pos, const int cap, int x, int y, int right, uint progress_col_width, bool global_section, GoalColumn column) const
	{
		if (column == GC_GOAL && IsInsideMM(pos, 0, cap)) DrawString(x, right, y + pos * FONT_HEIGHT_NORMAL, global_section ? STR_GOALS_GLOBAL_TITLE : STR_GOALS_COMPANY_TITLE);
		pos++;

		bool rtl = _current_text_dir == TD_RTL;

		uint num = 0;
		const Goal *s;
		FOR_ALL_GOALS(s) {
			if (global_section ? s->company == INVALID_COMPANY : (s->company == this->window_number && s->company != INVALID_COMPANY)) {
				if (IsInsideMM(pos, 0, cap)) {
					switch (column) {
						case GC_GOAL: {
							/* Display the goal. */
							SetDParamStr(0, s->text);
							uint width_reduction = progress_col_width > 0 ? progress_col_width + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT : 0;
							DrawString(x + (rtl ? width_reduction : 0), right - (rtl ? 0 : width_reduction), y + pos * FONT_HEIGHT_NORMAL, STR_GOALS_TEXT);
							break;
						}

						case GC_PROGRESS:
							if (s->progress != NULL) {
								SetDParamStr(0, s->progress);
								StringID str = s->completed ? STR_GOALS_PROGRESS_COMPLETE : STR_GOALS_PROGRESS;
								int progress_x = x;
								int progress_right = rtl ? x + progress_col_width : right;
								DrawString(progress_x, progress_right, y + pos * FONT_HEIGHT_NORMAL, str, TC_FROMSTRING, SA_RIGHT | SA_FORCE);
							}
							break;
					}
				}
				pos++;
				num++;
			}
		}

		if (num == 0) {
			if (column == GC_GOAL && IsInsideMM(pos, 0, cap)) {
				StringID str = !global_section && this->window_number == INVALID_COMPANY ? STR_GOALS_SPECTATOR_NONE : STR_GOALS_NONE;
				DrawString(x, right, y + pos * FONT_HEIGHT_NORMAL, str);
			}
			pos++;
		}
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
		int y = wid->pos_y + WD_FRAMERECT_TOP;
		int x = wid->pos_x + WD_FRAMERECT_LEFT;
		int right = x + wid->current_x - WD_FRAMERECT_RIGHT;

		int pos = -this->vscroll->GetPosition();
		const int cap = this->vscroll->GetCapacity();

		/* Draw partial list with global goals. */
		DrawPartialGoalList(pos, cap, x, y, right, progress_col_width, true, column);

		/* Draw partial list with company goals. */
		pos++;
		DrawPartialGoalList(pos, cap, x, y, right, progress_col_width, false, column);
	}

	/* virtual */ void OnPaint()
	{
		this->DrawWidgets();

		if (this->IsShaded()) return; // Don't draw anything when the window is shaded.

		/* Calculate progress column width. */
		uint max_width = 0;
		Goal *s;
		FOR_ALL_GOALS(s) {
			if (s->progress != NULL) {
				SetDParamStr(0, s->progress);
				StringID str = s->completed ? STR_GOALS_PROGRESS_COMPLETE : STR_GOALS_PROGRESS;
				uint str_width = GetStringBoundingBox(str).width;
				if (str_width > max_width) max_width = str_width;
			}
		}

		NWidgetBase *wid = this->GetWidget<NWidgetBase>(WID_GOAL_LIST);
		uint progress_col_width = min(max_width, wid->current_x);

		/* Draw goal list. */
		this->DrawListColumn(GC_PROGRESS, wid, progress_col_width);
		this->DrawListColumn(GC_GOAL, wid, progress_col_width);

	}

	/* virtual */ void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, WID_GOAL_LIST);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	/* virtual */ void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;
		this->vscroll->SetCount(this->CountLines());
		this->SetWidgetDirty(WID_GOAL_LIST);
	}
};

/** Widgets of the #GoalListWindow. */
static const NWidgetPart _nested_goals_list_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_GOAL_CAPTION), SetDataTip(STR_JUST_STRING, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN), SetDataTip(0x0, STR_GOALS_TOOLTIP_CLICK_ON_SERVICE_TO_CENTER), SetScrollbar(WID_GOAL_SCROLLBAR),
			NWidget(WWT_EMPTY, COLOUR_GREY, WID_GOAL_LIST), SetResize(1, 1), SetMinimalTextLines(2, 0), SetFill(1, 1), SetPadding(WD_FRAMERECT_TOP, 2, WD_FRAMETEXT_BOTTOM, 2),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_GOAL_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _goals_list_desc(
	WDP_AUTO, "list_goals", 500, 127,
	WC_GOALS_LIST, WC_NONE,
	0,
	_nested_goals_list_widgets, lengthof(_nested_goals_list_widgets)
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
	char *question; ///< Question to ask (private copy).
	int buttons;    ///< Number of valid buttons in #button.
	int button[3];  ///< Buttons to display.
	byte type;      ///< Type of question.

	GoalQuestionWindow(WindowDesc *desc, WindowNumber window_number, byte type, uint32 button_mask, const char *question) : Window(desc), type(type)
	{
		assert(type < GOAL_QUESTION_TYPE_COUNT);
		this->question = stredup(question);

		/* Figure out which buttons we have to enable. */
		uint bit;
		int n = 0;
		FOR_EACH_SET_BIT(bit, button_mask) {
			if (bit >= GOAL_QUESTION_BUTTON_COUNT) break;
			this->button[n++] = bit;
			if (n == 3) break;
		}
		this->buttons = n;
		assert(this->buttons > 0 && this->buttons < 4);

		this->CreateNestedTree();
		this->GetWidget<NWidgetStacked>(WID_GQ_BUTTONS)->SetDisplayedPlane(this->buttons - 1);
		this->FinishInitNested(window_number);
	}

	~GoalQuestionWindow()
	{
		free(this->question);
	}

	/* virtual */ void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_GQ_CAPTION:
				SetDParam(0, STR_GOAL_QUESTION_CAPTION_QUESTION + this->type);
				break;

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

	/* virtual */ void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_GQ_BUTTON_1:
				DoCommandP(0, this->window_number, this->button[0], CMD_GOAL_QUESTION_ANSWER);
				delete this;
				break;

			case WID_GQ_BUTTON_2:
				DoCommandP(0, this->window_number, this->button[1], CMD_GOAL_QUESTION_ANSWER);
				delete this;
				break;

			case WID_GQ_BUTTON_3:
				DoCommandP(0, this->window_number, this->button[2], CMD_GOAL_QUESTION_ANSWER);
				delete this;
				break;
		}
	}

	/* virtual */ void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != WID_GQ_QUESTION) return;

		SetDParamStr(0, this->question);
		size->height = GetStringHeight(STR_JUST_RAW_STRING, size->width) + WD_PAR_VSEP_WIDE;
	}

	/* virtual */ void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != WID_GQ_QUESTION) return;

		SetDParamStr(0, this->question);
		DrawStringMultiLine(r.left, r.right, r.top, UINT16_MAX, STR_JUST_RAW_STRING, TC_BLACK, SA_TOP | SA_HOR_CENTER);
	}
};

/** Widgets of the goal question window. */
static const NWidgetPart _nested_goal_question_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE, WID_GQ_CAPTION), SetDataTip(STR_WHITE_STRING, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE),
		NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GQ_QUESTION), SetMinimalSize(300, 0), SetPadding(8, 8, 8, 8), SetFill(1, 0),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_GQ_BUTTONS),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(85, 10, 85),
				NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_1), SetDataTip(STR_BLACK_STRING, STR_NULL), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(65, 10, 65),
				NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_1), SetDataTip(STR_BLACK_STRING, STR_NULL), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_2), SetDataTip(STR_BLACK_STRING, STR_NULL), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(25, 10, 25),
				NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_1), SetDataTip(STR_BLACK_STRING, STR_NULL), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_2), SetDataTip(STR_BLACK_STRING, STR_NULL), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_GQ_BUTTON_3), SetDataTip(STR_BLACK_STRING, STR_NULL), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 8),
	EndContainer(),
};

static WindowDesc _goal_question_list_desc(
	WDP_CENTER, NULL, 0, 0,
	WC_GOAL_QUESTION, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_goal_question_widgets, lengthof(_nested_goal_question_widgets)
);

/**
 * Display a goal question.
 * @param id Window number to use.
 * @param type Type of question.
 * @param button_mask Buttons to display.
 * @param question Question to ask.
 */
void ShowGoalQuestion(uint16 id, byte type, uint32 button_mask, const char *question)
{
	new GoalQuestionWindow(&_goal_question_list_desc, id, type, button_mask, question);
}
