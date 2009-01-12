/* $Id$ */

/** @file ai_gui.cpp Window for configuring the AIs */

#include "../stdafx.h"
#include "../openttd.h"
#include "../gui.h"
#include "../window_gui.h"
#include "../company_func.h"
#include "../company_base.h"
#include "../company_gui.h"
#include "../economy_func.h"
#include "../variables.h"
#include "../cargotype.h"
#include "../strings_func.h"
#include "../core/alloc_func.hpp"
#include "../window_func.h"
#include "../date_func.h"
#include "../gfx_func.h"
#include "../debug.h"
#include "../command_func.h"
#include "../network/network.h"

#include "ai.hpp"
#include "api/ai_types.hpp"
#include "api/ai_controller.hpp"
#include "api/ai_object.hpp"
#include "api/ai_log.hpp"
#include "ai_info.hpp"

#include "table/strings.h"
#include "../table/sprites.h"

struct AIDebugWindow : public Window {
	enum AIDebugWindowWidgets {
		AID_WIDGET_CLOSEBOX = 0,
		AID_WIDGET_CAPTION,
		AID_WIDGET_VIEW,
		AID_WIDGET_NAME_TEXT,
		AID_WIDGET_RELOAD_TOGGLE,
		AID_WIDGET_LOG_PANEL,
		AID_WIDGET_SCROLLBAR,
		AID_WIDGET_UNUSED_1,
		AID_WIDGET_UNUSED_2,
		AID_WIDGET_UNUSED_3,
		AID_WIDGET_UNUSED_4,
		AID_WIDGET_UNUSED_5,
		AID_WIDGET_UNUSED_6,
		AID_WIDGET_UNUSED_7,

		AID_WIDGET_COMPANY_BUTTON_START,
		AID_WIDGET_COMPANY_BUTTON_END = AID_WIDGET_COMPANY_BUTTON_START + 14,
	};

	static CompanyID ai_debug_company;
	int redraw_timer;

	AIDebugWindow(const WindowDesc *desc, WindowNumber number) : Window(desc, number)
	{
		/* Disable the companies who are not active or not an AI */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			this->SetWidgetDisabledState(i + AID_WIDGET_COMPANY_BUTTON_START, !IsValidCompanyID(i) || !GetCompany(i)->is_ai);
		}
		this->DisableWidget(AID_WIDGET_RELOAD_TOGGLE);

		this->vscroll.cap = 14;
		this->vscroll.pos = 0;
		this->resize.step_height = 12;

		if (ai_debug_company != INVALID_COMPANY) this->LowerWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		/* Check if the currently selected company is still active. */
		if (ai_debug_company == INVALID_COMPANY || !IsValidCompanyID(ai_debug_company)) {
			if (ai_debug_company != INVALID_COMPANY) {
				/* Raise and disable the widget for the previous selection. */
				this->RaiseWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);
				this->DisableWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);

				ai_debug_company = INVALID_COMPANY;
			}

			for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
				if (IsValidCompanyID(i) && GetCompany(i)->is_ai) {
					/* Lower the widget corresponding to this company. */
					this->LowerWidget(i + AID_WIDGET_COMPANY_BUTTON_START);

					ai_debug_company = i;
					break;
				}
			}
		}

		/* Update "Reload AI" button */
		this->SetWidgetDisabledState(AID_WIDGET_RELOAD_TOGGLE, ai_debug_company == INVALID_COMPANY);

		/* Draw standard stuff */
		this->DrawWidgets();

		/* If there are no active companies, don't display anything else. */
		if (ai_debug_company == INVALID_COMPANY) return;

		/* Paint the company icons */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			if (!IsValidCompanyID(i) || !GetCompany(i)->is_ai) {
				/* Check if we have the company as an active company */
				if (!this->IsWidgetDisabled(i + AID_WIDGET_COMPANY_BUTTON_START)) {
					/* Bah, company gone :( */
					this->DisableWidget(i + AID_WIDGET_COMPANY_BUTTON_START);

					/* We need a repaint */
					this->SetDirty();
				}
				continue;
			}

			/* Check if we have the company marked as inactive */
			if (this->IsWidgetDisabled(i + AID_WIDGET_COMPANY_BUTTON_START)) {
				/* New AI! Yippie :p */
				this->EnableWidget(i + AID_WIDGET_COMPANY_BUTTON_START);

				/* We need a repaint */
				this->SetDirty();
			}

			byte x = (i == ai_debug_company) ? 1 : 0;
			DrawCompanyIcon(i, (i % 8) * 37 + 13 + x, (i < 8 ? 0 : 13) + 16 + x);
		}

		/* Draw the AI name */
		AIInfo *info = GetCompany(ai_debug_company)->ai_info;
		assert(info != NULL);
		DoDrawString(info->GetName(), 7, 47, TC_BLACK);

		CompanyID old_company = _current_company;
		_current_company = ai_debug_company;
		AILog::LogData *log = (AILog::LogData *)AIObject::GetLogPointer();
		_current_company = old_company;

		SetVScrollCount(this, (log == NULL) ? 0 : log->used);
		this->InvalidateWidget(AID_WIDGET_SCROLLBAR);
		if (log == NULL) return;

		int y = 6;
		for (int i = this->vscroll.pos; i < (this->vscroll.cap + this->vscroll.pos); i++) {
			uint pos = (log->count + log->pos - i) % log->count;
			if (log->lines[pos] == NULL) break;

			uint colour;
			switch (log->type[pos]) {
				case AILog::LOG_SQ_INFO:  colour = TC_BLACK;  break;
				case AILog::LOG_SQ_ERROR: colour = TC_RED;    break;
				case AILog::LOG_INFO:     colour = TC_BLACK;  break;
				case AILog::LOG_WARNING:  colour = TC_YELLOW; break;
				case AILog::LOG_ERROR:    colour = TC_RED;    break;
				default:                  colour = TC_BLACK;  break;
			}

			DoDrawStringTruncated(log->lines[pos], 7, this->widget[AID_WIDGET_LOG_PANEL].top + y, colour, this->widget[AID_WIDGET_LOG_PANEL].right - this->widget[AID_WIDGET_LOG_PANEL].left - 14);
			y += 12;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		/* Check which button is clicked */
		if (IsInsideMM(widget, AID_WIDGET_COMPANY_BUTTON_START, AID_WIDGET_COMPANY_BUTTON_END + 1)) {
			/* Is it no on disable? */
			if (!this->IsWidgetDisabled(widget)) {
				this->RaiseWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);
				ai_debug_company = (CompanyID)(widget - AID_WIDGET_COMPANY_BUTTON_START);
				this->LowerWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);
				this->SetDirty();
			}
		}
		if (widget == AID_WIDGET_RELOAD_TOGGLE && !this->IsWidgetDisabled(widget)) {
			/* First kill the company of the AI, then start a new one. This should start the current AI again */
			DoCommandP(0, 2, ai_debug_company, CMD_COMPANY_CTRL);
			DoCommandP(0, 1, 0, CMD_COMPANY_CTRL);
		}
	}

	virtual void OnTimeout()
	{
		this->RaiseWidget(AID_WIDGET_RELOAD_TOGGLE);
		this->SetDirty();
	}

	virtual void OnInvalidateData(int data = 0)
	{
		if (data == -1 || ai_debug_company == data) this->SetDirty();
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		this->vscroll.cap += delta.y / (int)this->resize.step_height;
	}
};

CompanyID AIDebugWindow::ai_debug_company = INVALID_COMPANY;

static const Widget _ai_debug_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                   STR_018B_CLOSE_WINDOW},                 // AID_WIDGET_CLOSEBOX
{    WWT_CAPTION,  RESIZE_RIGHT,  COLOUR_GREY,    11,   298,     0,    13, STR_AI_DEBUG,               STR_018C_WINDOW_TITLE_DRAG_THIS},       // AID_WIDGET_CAPTION
{      WWT_PANEL,  RESIZE_RIGHT,  COLOUR_GREY,     0,   298,    14,    40, 0x0,                        STR_NULL},                              // AID_WIDGET_VIEW

{      WWT_PANEL,  RESIZE_RIGHT,  COLOUR_GREY,     0,   149,    41,    60, 0x0,                        STR_AI_DEBUG_NAME_TIP},                 // AID_WIDGET_NAME_TEXT
{ WWT_PUSHTXTBTN,     RESIZE_LR,  COLOUR_GREY,   150,   298,    41,    60, STR_AI_DEBUG_RELOAD,        STR_AI_DEBUG_RELOAD_TIP},               // AID_WIDGET_RELOAD_TOGGLE
{      WWT_PANEL,     RESIZE_RB,  COLOUR_GREY,     0,   286,    61,   240, 0x0,                        STR_NULL},                              // AID_WIDGET_LOG_PANEL
{  WWT_SCROLLBAR,    RESIZE_LRB,  COLOUR_GREY,   287,   298,    61,   228, STR_NULL,                   STR_0190_SCROLL_BAR_SCROLLS_LIST},      // AID_WIDGET_SCROLLBAR
/* As this is WIP, leave the next few so we can work a bit with the GUI */
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   101,   120, 0x0,                        STR_NULL},                              // AID_WIDGET_UNUSED_1
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   121,   140, 0x0,                        STR_NULL},                              // AID_WIDGET_UNUSED_2
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   141,   160, 0x0,                        STR_NULL},                              // AID_WIDGET_UNUSED_3
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   161,   180, 0x0,                        STR_NULL},                              // AID_WIDGET_UNUSED_4
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   181,   200, 0x0,                        STR_NULL},                              // AID_WIDGET_UNUSED_5
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   201,   220, 0x0,                        STR_NULL},                              // AID_WIDGET_UNUSED_6
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   221,   240, 0x0,                        STR_NULL},                              // AID_WIDGET_UNUSED_7

{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,    38,    14,    26, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY}, // AID_WIDGET_COMPANY_BUTTON_START
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    39,    75,    14,    26, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    76,   112,    14,    26, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   113,   149,    14,    26, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   150,   186,    14,    26, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   187,   223,    14,    26, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   224,   260,    14,    26, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   261,   297,    14,    26, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,    38,    27,    39, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    39,    75,    27,    39, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    76,   112,    27,    39, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   113,   149,    27,    39, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   150,   186,    27,    39, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   187,   223,    27,    39, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   224,   260,    27,    39, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY}, // AID_WIDGET_COMPANY_BUTTON_END
{  WWT_RESIZEBOX,   RESIZE_LRTB,  COLOUR_GREY,   287,   298,   229,   240, STR_NULL,                   STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _ai_debug_desc = {
	WDP_AUTO, WDP_AUTO, 299, 241, 299, 241,
	WC_AI_DEBUG, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_RESIZABLE,
	_ai_debug_widgets
};

void ShowAIDebugWindow()
{
	if (!_networking || _network_server) {
		AllocateWindowDescFront<AIDebugWindow>(&_ai_debug_desc, 0);
	} else {
		ShowErrorMessage(INVALID_STRING_ID, STR_AI_DEBUG_SERVER_ONLY, 0, 0);
	}
}
