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

static bool DrawScrollingStatusText(const NewsItem *ni, int pos, int width)
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
	if (!FillDrawPixelInfo(&tmp_dpi, 141, 1, width, 11)) return true;

	DrawPixelInfo *old_dpi = _cur_dpi;
	_cur_dpi = &tmp_dpi;

	int x = DoDrawString(buffer, pos, 0, TC_LIGHT_BLUE);
	_cur_dpi = old_dpi;

	return x > 0;
}

struct StatusBarWindow : Window {
	bool saving;
	int ticker_scroll;
	int reminder_timeout;

	enum {
		TICKER_START   =   360, ///< initial value of the ticker counter (scrolling news)
		TICKER_STOP    = -1280, ///< scrolling is finished when counter reaches this value
		REMINDER_START =    91, ///< initial value of the reminder counter (right dot on the right)
		REMINDER_STOP  =     0, ///< reminder disappears when counter reaches this value
		COUNTER_STEP   =     2, ///< this is subtracted from active counters every tick
	};

	enum StatusbarWidget {
		SBW_LEFT,   ///< left part of the statusbar; date is shown there
		SBW_MIDDLE, ///< middle part; current news or company name or *** SAVING *** or *** PAUSED ***
		SBW_RIGHT,  ///< right part; bank balance
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
		DrawStringCentered(70, 1, (_pause_game || _settings_client.gui.status_long_date) ? STR_00AF : STR_00AE, TC_FROMSTRING);

		if (c != NULL) {
			/* Draw company money */
			SetDParam(0, c->money);
			DrawStringCentered(this->widget[SBW_RIGHT].left + 70, 1, STR_0004, TC_FROMSTRING);
		}

		/* Draw status bar */
		if (this->saving) { // true when saving is active
			DrawStringCenteredTruncated(this->widget[SBW_MIDDLE].left + 1, this->widget[SBW_MIDDLE].right - 1, 1, STR_SAVING_GAME, TC_FROMSTRING);
		} else if (_do_autosave) {
			DrawStringCenteredTruncated(this->widget[SBW_MIDDLE].left + 1, this->widget[SBW_MIDDLE].right - 1, 1, STR_032F_AUTOSAVE, TC_FROMSTRING);
		} else if (_pause_game) {
			DrawStringCenteredTruncated(this->widget[SBW_MIDDLE].left + 1, this->widget[SBW_MIDDLE].right - 1, 1, STR_0319_PAUSED, TC_FROMSTRING);
		} else if (this->ticker_scroll > TICKER_STOP && FindWindowById(WC_NEWS_WINDOW, 0) == NULL && _statusbar_news_item.string_id != 0) {
			/* Draw the scrolling news text */
			if (!DrawScrollingStatusText(&_statusbar_news_item, this->ticker_scroll, this->widget[SBW_MIDDLE].right - this->widget[SBW_MIDDLE].left - 2)) {
				this->ticker_scroll = TICKER_STOP;
				if (c != NULL) {
					/* This is the default text */
					SetDParam(0, c->index);
					DrawStringCenteredTruncated(this->widget[SBW_MIDDLE].left + 1, this->widget[SBW_MIDDLE].right - 1, 1, STR_02BA, TC_FROMSTRING);
				}
			}
		} else {
			if (c != NULL) {
				/* This is the default text */
				SetDParam(0, c->index);
				DrawStringCenteredTruncated(this->widget[SBW_MIDDLE].left + 1, this->widget[SBW_MIDDLE].right - 1, 1, STR_02BA, TC_FROMSTRING);
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
			case SBI_SHOW_TICKER:     this->ticker_scroll    =   TICKER_START; break;
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
		if (_pause_game) return;

		if (this->ticker_scroll > TICKER_STOP) { // Scrolling text
			this->ticker_scroll -= COUNTER_STEP;
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
{    WWT_PUSHBTN,   RESIZE_RIGHT,  COLOUR_GREY,   140,   179,     0,    11, 0x0, STR_02B7_SHOW_LAST_MESSAGE_OR_NEWS},
{    WWT_PUSHBTN,   RESIZE_LR,     COLOUR_GREY,   180,   319,     0,    11, 0x0, STR_NULL},
{   WIDGETS_END},
};

static WindowDesc _main_status_desc(
	WDP_CENTER, 0, 320, 12, 640, 12,
	WC_STATUS_BAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_NO_FOCUS,
	_main_status_widgets
);

/**
 * Checks whether the news ticker is currently being used.
 */
bool IsNewsTickerShown()
{
	const StatusBarWindow *w = dynamic_cast<StatusBarWindow*>(FindWindowById(WC_STATUS_BAR, 0));
	return w != NULL && w->ticker_scroll > StatusBarWindow::TICKER_STOP;
}

void ShowStatusBar()
{
	_main_status_desc.top = _screen.height - 12;
	new StatusBarWindow(&_main_status_desc);
}
