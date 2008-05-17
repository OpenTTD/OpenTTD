/* $Id$ */

/** @file statusbar_gui.cpp The GUI for the bottom status bar. */

#include "stdafx.h"
#include "openttd.h"
#include "settings_type.h"
#include "date_func.h"
#include "gfx_func.h"
#include "news_func.h"
#include "player_func.h"
#include "string_func.h"
#include "strings_func.h"
#include "player_base.h"
#include "tilehighlight_func.h"
#include "news_gui.h"
#include "player_gui.h"
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
		} else if (c == 0x0D) {
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

	StatusBarWindow(const WindowDesc *desc) : Window(desc)
	{
		CLRBITS(this->flags4, WF_WHITE_BORDER_MASK);
		this->ticker_scroll = -1280;

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		const Player *p = (_local_player == PLAYER_SPECTATOR) ? NULL : GetPlayer(_local_player);

		this->DrawWidgets();
		SetDParam(0, _date);
		DrawStringCentered(70, 1, (_pause_game || _patches.status_long_date) ? STR_00AF : STR_00AE, TC_FROMSTRING);

		if (p != NULL) {
			/* Draw player money */
			SetDParam(0, p->player_money);
			DrawStringCentered(this->widget[2].left + 70, 1, STR_0004, TC_FROMSTRING);
		}

		/* Draw status bar */
		if (this->saving) { // true when saving is active
			DrawStringCenteredTruncated(this->widget[1].left + 1, this->widget[1].right - 1, 1, STR_SAVING_GAME, TC_FROMSTRING);
		} else if (_do_autosave) {
			DrawStringCenteredTruncated(this->widget[1].left + 1, this->widget[1].right - 1, 1, STR_032F_AUTOSAVE, TC_FROMSTRING);
		} else if (_pause_game) {
			DrawStringCenteredTruncated(this->widget[1].left + 1, this->widget[1].right - 1, 1, STR_0319_PAUSED, TC_FROMSTRING);
		} else if (this->ticker_scroll > -1280 && FindWindowById(WC_NEWS_WINDOW, 0) == NULL && _statusbar_news_item.string_id != 0) {
			/* Draw the scrolling news text */
			if (!DrawScrollingStatusText(&_statusbar_news_item, this->ticker_scroll, this->widget[1].right - this->widget[1].left - 2)) {
				this->ticker_scroll = -1280;
				if (p != NULL) {
					/* This is the default text */
					SetDParam(0, p->index);
					DrawStringCenteredTruncated(this->widget[1].left + 1, this->widget[1].right - 1, 1, STR_02BA, TC_FROMSTRING);
				}
			}
		} else {
			if (p != NULL) {
				/* This is the default text */
				SetDParam(0, p->index);
				DrawStringCenteredTruncated(this->widget[1].left + 1, this->widget[1].right - 1, 1, STR_02BA, TC_FROMSTRING);
			}
		}

		if (this->reminder_timeout > 0) DrawSprite(SPR_BLOT, PALETTE_TO_RED, this->widget[1].right - 11, 2);
	}

	virtual void OnInvalidateData(int data)
	{
		switch (data) {
			default: NOT_REACHED();
			case SBI_SAVELOAD_START:  this->saving = true;  break;
			case SBI_SAVELOAD_FINISH: this->saving = false; break;
			case SBI_SHOW_TICKER:     this->ticker_scroll    = 360; break;
			case SBI_SHOW_REMINDER:   this->reminder_timeout =  91; break;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case 1: ShowLastNewsMessage(); break;
			case 2: if (_local_player != PLAYER_SPECTATOR) ShowPlayerFinances(_local_player); break;
			default: ResetObjectToPlace();
		}
	}

	virtual void OnTick()
	{
		if (_pause_game) return;

		if (this->ticker_scroll > -1280) { // Scrolling text
			this->ticker_scroll -= 2;
			this->InvalidateWidget(1);
		}

		if (this->reminder_timeout > 0) { // Red blot to show there are new unread newsmessages
			this->reminder_timeout -= 2;
		} else if (this->reminder_timeout < 0) {
			this->reminder_timeout = 0;
			this->InvalidateWidget(1);
		}
	}
};

static const Widget _main_status_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   139,     0,    11, 0x0, STR_NULL},
{    WWT_PUSHBTN,   RESIZE_RIGHT,   14,   140,   179,     0,    11, 0x0, STR_02B7_SHOW_LAST_MESSAGE_OR_NEWS},
{    WWT_PUSHBTN,   RESIZE_LR,      14,   180,   319,     0,    11, 0x0, STR_NULL},
{   WIDGETS_END},
};

static WindowDesc _main_status_desc = {
	WDP_CENTER, 0, 320, 12, 640, 12,
	WC_STATUS_BAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_main_status_widgets,
	NULL
};

/**
 * Checks whether the news ticker is currently being used.
 */
bool IsNewsTickerShown()
{
	const StatusBarWindow *w = dynamic_cast<StatusBarWindow*>(FindWindowById(WC_STATUS_BAR, 0));
	return w != NULL && w->ticker_scroll > -1280;
}

void ShowStatusBar()
{
	_main_status_desc.top = _screen.height - 12;
	new StatusBarWindow(&_main_status_desc);
}
