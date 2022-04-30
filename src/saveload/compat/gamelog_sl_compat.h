/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gamelog_sl_compat.h Loading for gamelog chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_GAMELOG_H
#define SAVELOAD_COMPAT_GAMELOG_H

#include "../saveload.h"

/** Original field order for SlGamelogMode. */
const SaveLoadCompat _gamelog_mode_sl_compat[] = {
	SLC_VAR("mode.mode"),
	SLC_VAR("mode.landscape"),
};

/** Original field order for SlGamelogRevision. */
const SaveLoadCompat _gamelog_revision_sl_compat[] = {
	SLC_VAR("revision.text"),
	SLC_VAR("revision.newgrf"),
	SLC_VAR("revision.slver"),
	SLC_VAR("revision.modified"),
};

/** Original field order for SlGamelogOldver. */
const SaveLoadCompat _gamelog_oldver_sl_compat[] = {
	SLC_VAR("oldver.type"),
	SLC_VAR("oldver.version"),
};

/** Original field order for SlGamelogSetting. */
const SaveLoadCompat _gamelog_setting_sl_compat[] = {
	SLC_VAR("setting.name"),
	SLC_VAR("setting.oldval"),
	SLC_VAR("setting.newval"),
};

/** Original field order for SlGamelogGrfadd. */
const SaveLoadCompat _gamelog_grfadd_sl_compat[] = {
	SLC_VAR("grfadd.grfid"),
	SLC_VAR("grfadd.md5sum"),
};

/** Original field order for SlGamelogGrfrem. */
const SaveLoadCompat _gamelog_grfrem_sl_compat[] = {
	SLC_VAR("grfrem.grfid"),
};

/** Original field order for SlGamelogGrfcompat. */
const SaveLoadCompat _gamelog_grfcompat_sl_compat[] = {
	SLC_VAR("grfcompat.grfid"),
	SLC_VAR("grfcompat.md5sum"),
};

/** Original field order for SlGamelogGrfparam. */
const SaveLoadCompat _gamelog_grfparam_sl_compat[] = {
	SLC_VAR("grfparam.grfid"),
};

/** Original field order for SlGamelogGrfmove. */
const SaveLoadCompat _gamelog_grfmove_sl_compat[] = {
	SLC_VAR("grfmove.grfid"),
	SLC_VAR("grfmove.offset"),
};

/** Original field order for SlGamelogGrfbug. */
const SaveLoadCompat _gamelog_grfbug_sl_compat[] = {
	SLC_VAR("grfbug.data"),
	SLC_VAR("grfbug.grfid"),
	SLC_VAR("grfbug.bug"),
};

/** Original field order for SlGamelogEmergency. */
const SaveLoadCompat _gamelog_emergency_sl_compat[] = {
	SLC_VAR("is_emergency_save"),
};

/** Original field order for SlGamelogAction. */
const SaveLoadCompat _gamelog_action_sl_compat[] = {
	SLC_VAR("ct"),
	SLC_VAR("mode"),
	SLC_VAR("revision"),
	SLC_VAR("oldver"),
	SLC_VAR("setting"),
	SLC_VAR("grfadd"),
	SLC_VAR("grfrem"),
	SLC_VAR("grfcompat"),
	SLC_VAR("grfparam"),
	SLC_VAR("grfmove"),
	SLC_VAR("grfbug"),
	SLC_VAR("emergency"),
};

/** Original field order for _gamelog_desc. */
const SaveLoadCompat _gamelog_sl_compat[] = {
	SLC_VAR("at"),
	SLC_VAR("tick"),
	SLC_VAR("action"),
};

#endif /* SAVELOAD_COMPAT_GAMELOG_H */
