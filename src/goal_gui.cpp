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
#include "gfx_func.h"
#include "gui.h"
#include "goal_base.h"
#include "core/geometry_func.hpp"
#include "company_func.h"
#include "command_func.h"

#include "widgets/goal_widget.h"

#include "table/strings.h"

struct GoalListWindow : Window {
	Scrollbar *vscroll;

	GoalListWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->CreateNestedTree(desc);
		this->vscroll = this->GetScrollbar(WID_GL_SCROLLBAR);
		this->FinishInitNested(desc, window_number);
		this->OnInvalidateData(0);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		if (widget != WID_GL_PANEL) return;

		int y = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_GL_PANEL, WD_FRAMERECT_TOP);
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
			y--; // "None"
			if (y < 0) return;
		}

		y -= 2; // "Company specific goals:"
		if (y < 0) return;

		FOR_ALL_GOALS(s) {
			if (s->company == _local_company) {
				y--;
				if (y == 0) {
					this->HandleClick(s);
					return;
				}
			}
		}
	}

	void HandleClick(const Goal *s)
	{
		/* determine dst coordinate for goal and try to scroll to it */
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
	 * @return the number of lines
	 */
	uint CountLines()
	{
		/* Count number of (non) awarded goals */
		uint num_global = 0;
		uint num_company = 0;
		const Goal *s;
		FOR_ALL_GOALS(s) {
			if (s->company == INVALID_COMPANY) {
				num_global++;
			} else if (s->company == _local_company) {
				num_company++;
			}
		}

		/* Count the 'none' lines */
		if (num_global  == 0) num_global = 1;
		if (num_company == 0) num_company = 1;

		/* Global, company and an empty line before the accepted ones. */
		return 3 + num_global + num_company;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != WID_GL_PANEL) return;
		Dimension d = maxdim(GetStringBoundingBox(STR_GOALS_GLOBAL_TITLE), GetStringBoundingBox(STR_GOALS_COMPANY_TITLE));

		resize->height = d.height;

		d.height *= 5;
		d.width += padding.width + WD_FRAMERECT_RIGHT + WD_FRAMERECT_LEFT;
		d.height += padding.height + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
		*size = maxdim(*size, d);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != WID_GL_PANEL) return;

		YearMonthDay ymd;
		ConvertDateToYMD(_date, &ymd);

		int right = r.right - WD_FRAMERECT_RIGHT;
		int y = r.top + WD_FRAMERECT_TOP;
		int x = r.left + WD_FRAMERECT_LEFT;

		int pos = -this->vscroll->GetPosition();
		const int cap = this->vscroll->GetCapacity();

		/* Section for drawing the global goals */
		if (IsInsideMM(pos, 0, cap)) DrawString(x, right, y + pos * FONT_HEIGHT_NORMAL, STR_GOALS_GLOBAL_TITLE);
		pos++;

		uint num = 0;
		const Goal *s;
		FOR_ALL_GOALS(s) {
			if (s->company == INVALID_COMPANY) {
				if (IsInsideMM(pos, 0, cap)) {
					/* Display the goal */
					SetDParamStr(0, s->text);
					DrawString(x, right, y + pos * FONT_HEIGHT_NORMAL, STR_GOALS_TEXT);
				}
				pos++;
				num++;
			}
		}

		if (num == 0) {
			if (IsInsideMM(pos, 0, cap)) DrawString(x, right, y + pos * FONT_HEIGHT_NORMAL, STR_GOALS_NONE);
			pos++;
		}

		/* Section for drawing the company goals */
		pos++;
		if (IsInsideMM(pos, 0, cap)) DrawString(x, right, y + pos * FONT_HEIGHT_NORMAL, STR_GOALS_COMPANY_TITLE);
		pos++;
		num = 0;

		FOR_ALL_GOALS(s) {
			if (s->company == _local_company) {
				if (IsInsideMM(pos, 0, cap)) {
					/* Display the goal */
					SetDParamStr(0, s->text);
					DrawString(x, right, y + pos * FONT_HEIGHT_NORMAL, STR_GOALS_TEXT);
				}
				pos++;
				num++;
			}
		}

		if (num == 0) {
			if (IsInsideMM(pos, 0, cap)) DrawString(x, right, y + pos * FONT_HEIGHT_NORMAL, STR_GOALS_NONE);
			pos++;
		}
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, WID_GL_PANEL);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;
		this->vscroll->SetCount(this->CountLines());
	}
};

static const NWidgetPart _nested_goals_list_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_GOALS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN, WID_GL_PANEL), SetDataTip(0x0, STR_GOALS_TOOLTIP_CLICK_ON_SERVICE_TO_CENTER), SetResize(1, 1), SetScrollbar(WID_GL_SCROLLBAR), EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_GL_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _goals_list_desc(
	WDP_AUTO, 500, 127,
	WC_GOALS_LIST, WC_NONE,
	0,
	_nested_goals_list_widgets, lengthof(_nested_goals_list_widgets)
);

void ShowGoalsList()
{
	AllocateWindowDescFront<GoalListWindow>(&_goals_list_desc, 0);
}



struct GoalQuestionWindow : Window {
	char *question;
	int buttons;
	int button[3];

	GoalQuestionWindow(const WindowDesc *desc, WindowNumber window_number, uint32 button_mask, const char *question) : Window()
	{
		this->question = strdup(question);

		/* Figure out which buttons we have to enable */
		int bit;
		int n = 0;
		FOR_EACH_SET_BIT(bit, button_mask) {
			if (bit >= GOAL_QUESTION_BUTTON_COUNT) break;
			this->button[n++] = bit;
			if (n == 3) break;
		}
		this->buttons = n;
		assert(this->buttons > 0 && this->buttons < 4);

		this->CreateNestedTree(desc);
		this->GetWidget<NWidgetStacked>(WID_GQ_BUTTONS)->SetDisplayedPlane(this->buttons - 1);
		this->FinishInitNested(desc, window_number);
	}

	~GoalQuestionWindow()
	{
		free(this->question);
	}

	virtual void SetStringParameters(int widget) const
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

	virtual void OnClick(Point pt, int widget, int click_count)
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

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != WID_GQ_QUESTION) return;

		SetDParamStr(0, this->question);
		size->height = GetStringHeight(STR_JUST_RAW_STRING, size->width) + WD_PAR_VSEP_WIDE;
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != WID_GQ_QUESTION) return;

		SetDParamStr(0, this->question);
		DrawStringMultiLine(r.left, r.right, r.top, UINT16_MAX, STR_JUST_RAW_STRING, TC_BLACK, SA_TOP | SA_HOR_CENTER);
	}
};

static const NWidgetPart _nested_goal_question_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE), SetDataTip(STR_GOAL_QUESTION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
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

static const WindowDesc _goal_question_list_desc(
	WDP_CENTER, 0, 0,
	WC_GOAL_QUESTION, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_goal_question_widgets, lengthof(_nested_goal_question_widgets)
);


void ShowGoalQuestion(uint16 id, uint32 button_mask, const char *question)
{
	new GoalQuestionWindow(&_goal_question_list_desc, id, button_mask, question);
}
