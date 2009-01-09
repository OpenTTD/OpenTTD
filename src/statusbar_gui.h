/* $Id$ */

/** @file statusbar_gui.h Functions, definitions and such used only by the GUI. */

#ifndef STATUSBAR_GUI_H
#define STATUSBAR_GUI_H

enum StatusBarInvalidate {
	SBI_SAVELOAD_START,  ///< started saving
	SBI_SAVELOAD_FINISH, ///< finished saving
	SBI_SHOW_TICKER,     ///< start scolling news
	SBI_SHOW_REMINDER,   ///< show a reminder (dot on the right side of the statusbar)
	SBI_NEWS_DELETED,    ///< abort current news display (active news were deleted)
	SBI_END
};

bool IsNewsTickerShown();
void ShowStatusBar();

#endif /* STATUSBAR_GUI_H */
