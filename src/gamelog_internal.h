/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gamelog_internal.h Declaration shared among gamelog.cpp and saveload/gamelog_sl.cpp */

#ifndef GAMELOG_INTERNAL_H
#define GAMELOG_INTERNAL_H

#include "gamelog.h"

/** Type of logged change */
enum GamelogChangeType {
	GLCT_MODE,        ///< Scenario editor x Game, different landscape
	GLCT_REVISION,    ///< Changed game revision string
	GLCT_OLDVER,      ///< Loaded from savegame without logged data
	GLCT_SETTING,     ///< Non-networksafe setting value changed
	GLCT_GRFADD,      ///< Removed GRF
	GLCT_GRFREM,      ///< Added GRF
	GLCT_GRFCOMPAT,   ///< Loading compatible GRF
	GLCT_GRFPARAM,    ///< GRF parameter changed
	GLCT_GRFMOVE,     ///< GRF order changed
	GLCT_GRFBUG,      ///< GRF bug triggered
	GLCT_EMERGENCY,   ///< Emergency savegame
	GLCT_END,         ///< So we know how many GLCTs are there
	GLCT_NONE = 0xFF, ///< In savegames, end of list
};


static const uint GAMELOG_REVISION_LENGTH = 15;

/** Contains information about one logged change */
struct LoggedChange {
	GamelogChangeType ct; ///< Type of change logged in this struct
	union {
		struct {
			byte mode;       ///< new game mode - Editor x Game
			byte landscape;  ///< landscape (temperate, arctic, ...)
		} mode;
		struct {
			char text[GAMELOG_REVISION_LENGTH]; ///< revision string, _openttd_revision
			uint32 newgrf;   ///< _openttd_newgrf_version
			uint16 slver;    ///< _sl_version
			byte modified;   ///< _openttd_revision_modified
		} revision;
		struct {
			uint32 type;     ///< type of savegame, @see SavegameType
			uint32 version;  ///< major and minor version OR ttdp version
		} oldver;
		GRFIdentifier grfadd;    ///< ID and md5sum of added GRF
		struct {
			uint32 grfid;    ///< ID of removed GRF
		} grfrem;
		GRFIdentifier grfcompat; ///< ID and new md5sum of changed GRF
		struct {
			uint32 grfid;    ///< ID of GRF with changed parameters
		} grfparam;
		struct {
			uint32 grfid;    ///< ID of moved GRF
			int32 offset;    ///< offset, positive = move down
		} grfmove;
		struct {
			char *name;      ///< name of the setting
			int32 oldval;    ///< old value
			int32 newval;    ///< new value
		} setting;
		struct {
			uint64 data;     ///< additional data
			uint32 grfid;    ///< ID of problematic GRF
			byte bug;        ///< type of bug, @see enum GRFBugs
		} grfbug;
	};
};


/** Contains information about one logged action that caused at least one logged change */
struct LoggedAction {
	LoggedChange *change; ///< First logged change in this action
	uint32 changes;       ///< Number of changes in this action
	GamelogActionType at; ///< Type of action
	uint16 tick;          ///< Tick when it happened
};

extern LoggedAction *_gamelog_action;
extern uint _gamelog_actions;

#endif /* GAMELOG_INTERNAL_H */
