/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gamelog_sl.cpp Code handling saving and loading of gamelog data */

#include "../stdafx.h"
#include "../gamelog_internal.h"
#include "../fios.h"

#include "saveload.h"

#include "../safeguards.h"

static const SaveLoad _glog_action_desc[] = {
	SLE_VAR(LoggedAction, tick,              SLE_UINT16),
	SLE_END()
};

static const SaveLoad _glog_mode_desc[] = {
	SLE_VAR(LoggedChange, mode.mode,         SLE_UINT8),
	SLE_VAR(LoggedChange, mode.landscape,    SLE_UINT8),
	SLE_END()
};

static const SaveLoad _glog_revision_desc[] = {
	SLE_ARR(LoggedChange, revision.text,     SLE_UINT8,  GAMELOG_REVISION_LENGTH),
	SLE_VAR(LoggedChange, revision.newgrf,   SLE_UINT32),
	SLE_VAR(LoggedChange, revision.slver,    SLE_UINT16),
	SLE_VAR(LoggedChange, revision.modified, SLE_UINT8),
	SLE_END()
};

static const SaveLoad _glog_oldver_desc[] = {
	SLE_VAR(LoggedChange, oldver.type,       SLE_UINT32),
	SLE_VAR(LoggedChange, oldver.version,    SLE_UINT32),
	SLE_END()
};

static const SaveLoad _glog_setting_desc[] = {
	SLE_STR(LoggedChange, setting.name,      SLE_STR,    128),
	SLE_VAR(LoggedChange, setting.oldval,    SLE_INT32),
	SLE_VAR(LoggedChange, setting.newval,    SLE_INT32),
	SLE_END()
};

static const SaveLoad _glog_grfadd_desc[] = {
	SLE_VAR(LoggedChange, grfadd.grfid,      SLE_UINT32    ),
	SLE_ARR(LoggedChange, grfadd.md5sum,     SLE_UINT8,  16),
	SLE_END()
};

static const SaveLoad _glog_grfrem_desc[] = {
	SLE_VAR(LoggedChange, grfrem.grfid,      SLE_UINT32),
	SLE_END()
};

static const SaveLoad _glog_grfcompat_desc[] = {
	SLE_VAR(LoggedChange, grfcompat.grfid,   SLE_UINT32    ),
	SLE_ARR(LoggedChange, grfcompat.md5sum,  SLE_UINT8,  16),
	SLE_END()
};

static const SaveLoad _glog_grfparam_desc[] = {
	SLE_VAR(LoggedChange, grfparam.grfid,    SLE_UINT32),
	SLE_END()
};

static const SaveLoad _glog_grfmove_desc[] = {
	SLE_VAR(LoggedChange, grfmove.grfid,     SLE_UINT32),
	SLE_VAR(LoggedChange, grfmove.offset,    SLE_INT32),
	SLE_END()
};

static const SaveLoad _glog_grfbug_desc[] = {
	SLE_VAR(LoggedChange, grfbug.data,       SLE_UINT64),
	SLE_VAR(LoggedChange, grfbug.grfid,      SLE_UINT32),
	SLE_VAR(LoggedChange, grfbug.bug,        SLE_UINT8),
	SLE_END()
};

static const SaveLoad _glog_emergency_desc[] = {
	SLE_END()
};

static const SaveLoad * const _glog_desc[] = {
	_glog_mode_desc,
	_glog_revision_desc,
	_glog_oldver_desc,
	_glog_setting_desc,
	_glog_grfadd_desc,
	_glog_grfrem_desc,
	_glog_grfcompat_desc,
	_glog_grfparam_desc,
	_glog_grfmove_desc,
	_glog_grfbug_desc,
	_glog_emergency_desc,
};

static_assert(lengthof(_glog_desc) == GLCT_END);

static void Load_GLOG_common(LoggedAction *&gamelog_action, uint &gamelog_actions)
{
	assert(gamelog_action == nullptr);
	assert(gamelog_actions == 0);

	byte type;
	while ((type = SlReadByte()) != GLAT_NONE) {
		if (type >= GLAT_END) SlErrorCorrupt("Invalid gamelog action type");
		GamelogActionType at = (GamelogActionType)type;

		gamelog_action = ReallocT(gamelog_action, gamelog_actions + 1);
		LoggedAction *la = &gamelog_action[gamelog_actions++];

		la->at = at;

		SlObject(la, _glog_action_desc); // has to be saved after 'DATE'!
		la->change = nullptr;
		la->changes = 0;

		while ((type = SlReadByte()) != GLCT_NONE) {
			if (type >= GLCT_END) SlErrorCorrupt("Invalid gamelog change type");
			GamelogChangeType ct = (GamelogChangeType)type;

			la->change = ReallocT(la->change, la->changes + 1);

			LoggedChange *lc = &la->change[la->changes++];
			/* for SLE_STR, pointer has to be valid! so make it nullptr */
			memset(lc, 0, sizeof(*lc));
			lc->ct = ct;

			SlObject(lc, _glog_desc[ct]);
		}
	}
}

static void Save_GLOG()
{
	const LoggedAction *laend = &_gamelog_action[_gamelog_actions];
	size_t length = 0;

	for (const LoggedAction *la = _gamelog_action; la != laend; la++) {
		const LoggedChange *lcend = &la->change[la->changes];
		for (LoggedChange *lc = la->change; lc != lcend; lc++) {
			assert((uint)lc->ct < lengthof(_glog_desc));
			length += SlCalcObjLength(lc, _glog_desc[lc->ct]) + 1;
		}
		length += 4;
	}
	length++;

	SlSetLength(length);

	for (LoggedAction *la = _gamelog_action; la != laend; la++) {
		SlWriteByte(la->at);
		SlObject(la, _glog_action_desc);

		const LoggedChange *lcend = &la->change[la->changes];
		for (LoggedChange *lc = la->change; lc != lcend; lc++) {
			SlWriteByte(lc->ct);
			assert((uint)lc->ct < GLCT_END);
			SlObject(lc, _glog_desc[lc->ct]);
		}
		SlWriteByte(GLCT_NONE);
	}
	SlWriteByte(GLAT_NONE);
}

static void Load_GLOG()
{
	Load_GLOG_common(_gamelog_action, _gamelog_actions);
}

static void Check_GLOG()
{
	Load_GLOG_common(_load_check_data.gamelog_action, _load_check_data.gamelog_actions);
}

extern const ChunkHandler _gamelog_chunk_handlers[] = {
	{ 'GLOG', Save_GLOG, Load_GLOG, nullptr, Check_GLOG, CH_RIFF | CH_LAST }
};
