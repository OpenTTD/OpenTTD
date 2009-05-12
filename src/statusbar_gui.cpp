/* $Id$ */

/** @file statusbar_gui.cpp The GUI for the bottom status bar. */

#include "stdafx.h"
#include "openttd.h"
#include "settings_type.h"
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

	StatusBarWindow(const WindowDesc *desc) : Window(desc)
	{
		CLRBITS(this->flags4, WF_WHITE_BORDER_MASK);
		this->ticker_scroll    =   TICKER_STOP;
		this->reminder_timeout = REMINDER_STOP;

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		const Company *c = (_local_company == COMPANY_SPECTATOR) ? NULL : GetCompany(_local_company);

		this->DrawWidgets();
		SetDParam(0, _date);
		DrawString(this->widget[SBW_LEFT].left + 1, this->widget[SBW_LEFT].right - 1, 1, (_pause_mode || _settings_client.gui.status_long_date) ? STR_DATE_LONG_WHITE : STR_DATE_SHORT_WHITE, TC_FROMSTRING, SA_CENTER);

		if (c != NULL) {
			/* Draw company money */
			SetDParam(0, c->money);
			DrawString(this->widget[SBW_RIGHT].left + 1, this->widget[SBW_RIGHT].right - 1, 1, STR_COMPANY_MONEY, TC_FROMSTRING, SA_CENTER);
		}

		/* Draw status bar */
		if (this->saving) { // true when saving is active
			DrawString(this->widget[SBW_MIDDLE].left + 1, this->widget[SBW_MIDDLE].right - 1, 1, STR_STATUSBAR_SAVING_GAME, TC_FROMSTRING, SA_CENTER);
		} else if (_do_autosave) {
			DrawString(this->widget[SBW_MIDDLE].left + 1, this->widget[SBW_MIDDLE].right - 1, 1, STR_STATUSBAR_AUTOSAVE, TC_FROMSTRING, SA_CENTER);
		} else if (_pause_mode != PM_UNPAUSED) {
			DrawString(this->widget[SBW_MIDDLE].left + 1, this->widget[SBW_MIDDLE].right - 1, 1, STR_STATUSBAR_PAUSED, TC_FROMSTRING, SA_CENTER);
		} else if (this->ticker_scroll < TICKER_STOP && FindWindowById(WC_NEWS_WINDOW, 0) == NULL && _statusbar_news_item.string_id != 0) {
			/* Draw the scrolling news text */
			if (!DrawScrollingStatusText(&_statusbar_news_item, this->ticker_scroll, this->widget[SBW_MIDDLE].left + 1, this->widget[SBW_MIDDLE].right - 1, this->widget[SBW_MIDDLE].top + 1, this->widget[SBW_MIDDLE].bottom)) {
				this->ticker_scroll = TICKER_STOP;
				if (c != NULL) {
					/* This is the default text */
					SetDParam(0, c->index);
					DrawString(this->widget[SBW_MIDDLE].left + 1, this->widget[SBW_MIDDLE].right - 1, 1, STR_STATUSBAR_COMPANY_NAME, TC_FROMSTRING, SA_CENTER);
				}
			}
		} else {
			if (c != NULL) {
				/* This is the default text */
				SetDParam(0, c->index);
				DrawString(this->widget[SBW_MIDDLE].left + 1, this->widget[SBW_MIDDLE].right - 1, 1, STR_STATUSBAR_COMPANY_NAME, TC_FROMSTRING, SA_CENTER);
			}
		}

		if (this->reminder_timeout > 0) DrawSprite(SPR_BLOT, PALETTE_TO_RED, this->widget[SBW_MIDDLE].right - 11, 2);
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
			this->InvalidateWidget(SBW_MIDDLE);
		}

		if (this->reminder_timeout > REMINDER_STOP) { // Red blot to show there are new unread newsmessages
			this->reminder_timeout -= COUNTER_STEP;
		} else if (this->reminder_timeout < REMINDER_STOP) {
			this->reminder_timeout = REMINDER_STOP;
			this->InvalidateWidget(SBW_MIDDLE);
		}
	}
};

static const Widget _main_status_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,   COLOUR_GREY,     0,   139,     0,    11, 0x0, STR_NULL},
{    WWT_PUSHBTN,   RESIZE_RIGHT,  COLOUR_GREY,   140,   179,     0,    11, 0x0, STR_STATUSBAR_TOOLTIP_SHOW_LAST_NEWS},
{    WWT_PUSHBTN,   RESIZE_LR,     COLOUR_GREY,   180,   319,     0,    11, 0x0, STR_NULL},
{   WIDGETS_END},
};

static const NWidgetPart _nested_main_status_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, SBW_LEFT), SetMinimalSize(140, 12), EndContainer(),
		NWidget(WWT_PUSHBTN, COLOUR_GREY, SBW_MIDDLE), SetMinimalSize(40, 12), SetDataTip(0x0, STR_STATUSBAR_TOOLTIP_SHOW_LAST_NEWS), SetResize(1, 0),
		NWidget(WWT_PUSHBTN, COLOUR_GREY, SBW_RIGHT), SetMinimalSize(140, 12),
	EndContainer(),
};

static WindowDesc _main_status_desc(
	WDP_CENTER, 0, 320, 12, 640, 12,
	WC_STATUS_BAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_NO_FOCUS,
	_main_status_widgets, _nested_main_status_widgets, lengthof(_nested_main_status_widgets)
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
	_main_status_desc.top = _screen.height - 12;
	new StatusBarWindow(&_main_status_desc);
}
