/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

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

extern int16 *_preferred_statusbar_size;

#endif /* STATUSBAR_GUI_H */
