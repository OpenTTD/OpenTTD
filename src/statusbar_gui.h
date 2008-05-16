/* $Id$ */

/** @file statusbar_gui.h Functions, definitions and such used only by the GUI. */

#ifndef STATUSBAR_GUI_H
#define STATUSBAR_GUI_H

enum StatusBarInvalidate
{
	SBI_SAVELOAD_START,
	SBI_SAVELOAD_FINISH,
	SBI_SHOW_TICKER,
	SBI_SHOW_REMINDER,
	SBI_END
};

bool IsNewsTickerShown();
void ShowStatusBar();

#endif /* STATUSBAR_GUI_H */
