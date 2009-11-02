/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file statusbar_gui.cpp The GUI for the bottom status bar. */

#include "stdafx.h"
#include "openttd.h"
#include "date_func.h"
#include "gfx_func.h"
#include "news_func.h"
#include "company_func.h"
#include "string_func.h"
#include "strings_func.h"
#include "company_base.h"
#include "tilehighlight_func.h"
#include "news_gui.h"
#include "company_gui.h"
#include "window_gui.h"
#include "variables.h"
#include "window_func.h"
#include "statusbar_gui.h"

#include "table/strings.h"
#include "table/sprites.h"

static bool DrawScrollingStatusText(const NewsItem *ni, int scroll_pos, int left, int right, int top, int bottom)
{
	CopyInDParam(0, ni->params, lengthof(ni->params));
	StringID str = ni->string_id;

	char buf[512];
	GetString(buf, str, lastof(buf));
	const char *s = buf;

	char buffer[256];
	char *d = buffer;
	const char *last = lastof(buffer);

	for (;;) {
		WChar c = Utf8Consume(&s);
		if (c == 0) {
			break;
		} else if (c == '\n') {
			if (d + 4 >= last) break;
			d[0] = d[1] = d[2] = d[3] = ' ';
			d += 4;
		} else if (IsPrintable(c)) {
			if (d + Utf8CharLen(c) >= last) break;
			d += Utf8Encode(d, c);
		}
	}
	*d = '\0';

	DrawPixelInfo tmp_dpi;
	if (!FillDrawPixelInfo(&tmp_dpi, left, top, right - left, bottom)) return true;

	int width = GetStringBoundingBox(buffer).width;
	int pos = (_dynlang.text_dir == TD_RTL) ? (scroll_pos - width) : (right - scroll_pos - left);

	DrawPixelInfo *old_dpi = _cur_dpi;
	_cur_dpi = &tmp_dpi;
	DrawString(pos, INT16_MAX, 0, buffer, TC_LIGHT_BLUE, SA_LEFT | SA_FORCE);
	_cur_dpi = old_dpi;

	return (_dynlang.text_dir == TD_RTL) ? (pos < right - left) : (pos + width > 0);
}

enum StatusbarWidget {
	SBW_LEFT,   ///< left part of the statusbar; date is shown there
	SBW_MIDDLE, ///< middle part; current news or company name or *** SAVING *** or *** PAUSED ***
	SBW_RIGHT,  ///< right part; bank balance
};

struct StatusBarWindow : Window {
	bool saving;
	int ticker_scroll;
	int reminder_timeout;

	enum {
		TICKER_STOP    = 1640, ///< scrolling is finished when counter reaches this value
		REMINDER_START =   91, ///< initial value of the reminder counter (right dot on the right)
		REMINDER_STOP  =    0, ///< reminder disappears when counter reaches this value
		COUNTER_STEP   =    2, ///< this is subtracted from active counters every tick
	};

	StatusBarWindow(const WindowDesc *desc) : Window()
	{
		CLRBITS(this->flags4, WF_WHITE_BORDER_MASK);
		this->ticker_scroll    =   TICKER_STOP;
		this->reminder_timeout = REMINDER_STOP;

		this->InitNested(desc);
	}

	virtual Point OnInitialPosition(const WindowDesc *desc, int16 sm_width, int16 sm_height, int window_number)
	{
		Point pt = { (_screen.width - max(sm_width, desc->default_width)) / 2, _screen.height - sm_height };
		return pt;
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		size->height = FONT_HEIGHT_NORMAL + padding.height;
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case SBW_LEFT:
				/* Draw the date */
				SetDParam(0, _date);
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, (_pause_mode || _settings_client.gui.status_long_date) ? STR_WHITE_DATE_LONG : STR_WHITE_DATE_SHORT, TC_FROMSTRING, SA_CENTER);
				break;

			case SBW_RIGHT: {
				/* Draw company money, if any */
				const Company *c = Company::GetIfValid(_local_company);
				if (c != NULL) {
					SetDParam(0, c->money);
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, STR_COMPANY_MONEY, TC_FROMSTRING, SA_CENTER);
				}
			} break;

			case SBW_MIDDLE:
				/* Draw status bar */
				if (this->saving) { // true when saving is active
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, STR_STATUSBAR_SAVING_GAME, TC_FROMSTRING, SA_CENTER);
				} else if (_do_autosave) {
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, STR_STATUSBAR_AUTOSAVE, TC_FROMSTRING, SA_CENTER);
				} else if (_pause_mode != PM_UNPAUSED) {
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, STR_STATUSBAR_PAUSED, TC_FROMSTRING, SA_CENTER);
				} else if (this->ticker_scroll < TICKER_STOP && FindWindowById(WC_NEWS_WINDOW, 0) == NULL && _statusbar_news_item.string_id != 0) {
					/* Draw the scrolling news text */
					if (!DrawScrollingStatusText(&_statusbar_news_item, this->ticker_scroll, r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, r.bottom)) {
						InvalidateWindowData(WC_STATUS_BAR, 0, SBI_NEWS_DELETED);
						if (Company::IsValidID(_local_company)) {
							/* This is the default text */
							SetDParam(0, _local_company);
							DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, STR_STATUSBAR_COMPANY_NAME, TC_FROMSTRING, SA_CENTER);
						}
					}
				} else {
					if (Company::IsValidID(_local_company)) {
						/* This is the default text */
						SetDParam(0, _local_company);
						DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, STR_STATUSBAR_COMPANY_NAME, TC_FROMSTRING, SA_CENTER);
					}
				}

				if (this->reminder_timeout > 0) DrawSprite(SPR_BLOT, PALETTE_TO_RED, r.right - WD_FRAMERECT_RIGHT - 10, r.top + WD_FRAMERECT_TOP + 1);
				break;
		}
	}

	virtual void OnInvalidateData(int data)
	{
		switch (data) {
			default: NOT_REACHED();
			case SBI_SAVELOAD_START:  this->saving = true;  break;
			case SBI_SAVELOAD_FINISH: this->saving = false; break;
			case SBI_SHOW_TICKER:     this->ticker_scroll = 0; break;
			case SBI_SHOW_REMINDER:   this->reminder_timeout = REMINDER_START; break;
			case SBI_NEWS_DELETED:
				this->ticker_scroll    =   TICKER_STOP; // reset ticker ...
				this->reminder_timeout = REMINDER_STOP; // ... and reminder
				break;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case SBW_MIDDLE: ShowLastNewsMessage(); break;
			case SBW_RIGHT:  if (_local_company != COMPANY_SPECTATOR) ShowCompanyFinances(_local_company); break;
			default: ResetObjectToPlace();
		}
	}

	virtual void OnTick()
	{
		if (_pause_mode != PM_UNPAUSED) return;

		if (this->ticker_scroll < TICKER_STOP) { // Scrolling text
			this->ticker_scroll += COUNTER_STEP;
			this->SetWidgetDirty(SBW_MIDDLE);
		}

		if (this->reminder_timeout > REMINDER_STOP) { // Red blot to show there are new unread newsmessages
			this->reminder_timeout -= COUNTER_STEP;
		} else if (this->reminder_timeout < REMINDER_STOP) {
			this->reminder_timeout = REMINDER_STOP;
			this->SetWidgetDirty(SBW_MIDDLE);
		}
	}
};

static const NWidgetPart _nested_main_status_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, SBW_LEFT), SetMinimalSize(140, 12), EndContainer(),
		NWidget(WWT_PUSHBTN, COLOUR_GREY, SBW_MIDDLE), SetMinimalSize(40, 12), SetDataTip(0x0, STR_STATUSBAR_TOOLTIP_SHOW_LAST_NEWS), SetResize(1, 0),
		NWidget(WWT_PUSHBTN, COLOUR_GREY, SBW_RIGHT), SetMinimalSize(140, 12),
	EndContainer(),
};

static const WindowDesc _main_status_desc(
	WDP_CENTER, 0, 320, 12, 640, 12,
	WC_STATUS_BAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_NO_FOCUS,
	NULL, _nested_main_status_widgets, lengthof(_nested_main_status_widgets)
);

/**
 * Checks whether the news ticker is currently being used.
 */
bool IsNewsTickerShown()
{
	const StatusBarWindow *w = dynamic_cast<StatusBarWindow*>(FindWindowById(WC_STATUS_BAR, 0));
	return w != NULL && w->ticker_scroll < StatusBarWindow::TICKER_STOP;
}

void ShowStatusBar()
{
	new StatusBarWindow(&_main_status_desc);
}
