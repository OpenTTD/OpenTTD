/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file main_gui.h Stuff related to the main gui. */

#ifndef MAIN_GUI_H
#define MAIN_GUI_H

#include "hotkeys.h"

enum MainGuiHotkeys {
	GHK_QUIT,
	GHK_ABANDON,
	GHK_CONSOLE,
	GHK_BOUNDING_BOXES,
	GHK_DIRTY_BLOCKS,
	GHK_CENTER,
	GHK_CENTER_ZOOM,
	GHK_RESET_OBJECT_TO_PLACE,
	GHK_DELETE_WINDOWS,
	GHK_DELETE_NONVITAL_WINDOWS,
	GHK_DELETE_ALL_MESSAGES,
	GHK_REFRESH_SCREEN,
	GHK_CRASH,
	GHK_MONEY,
	GHK_UPDATE_COORDS,
	GHK_TOGGLE_TRANSPARENCY,
	GHK_TOGGLE_INVISIBILITY = GHK_TOGGLE_TRANSPARENCY + 9,
	GHK_TRANSPARENCY_TOOLBAR = GHK_TOGGLE_INVISIBILITY + 8,
	GHK_TRANSPARANCY,
	GHK_CHAT,
	GHK_CHAT_ALL,
	GHK_CHAT_COMPANY,
	GHK_CHAT_SERVER,
	GHK_CLOSE_NEWS,
	GHK_CLOSE_ERROR,
};

#endif /* MAIN_GUI_H */
