/* $Id$ */

/** @file gamelog_sl.cpp Code handling saving and loading of gamelog data */

#include "../stdafx.h"
#include "../gamelog.h"
#include "../gamelog_internal.h"
#include "../core/alloc_func.hpp"

#include "saveload.h"

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
	SLE_ARR(LoggedChange, revision.text,     SLE_UINT8,  NETWORK_REVISION_LENGTH),
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

static const SaveLoad *_glog_desc[] = {
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

assert_compile(lengthof(_glog_desc) == GLCT_END);

static void Load_GLOG()
{
	assert(_gamelog_action == NULL);
	assert(_gamelog_actions == 0);

	GamelogActionType at;
	while ((at = (GamelogActionType)SlReadByte()) != GLAT_NONE) {
		_gamelog_action = ReallocT(_gamelog_action, _gamelog_actions + 1);
		LoggedAction *la = &_gamelog_action[_gamelog_actions++];

		la->at = at;

		SlObject(la, _glog_action_desc); // has to be saved after 'DATE'!
		la->change = NULL;
		la->changes = 0;

		GamelogChangeType ct;
		while ((ct = (GamelogChangeType)SlReadByte()) != GLCT_NONE) {
			la->change = ReallocT(la->change, la->changes + 1);

			LoggedChange *lc = &la->change[la->changes++];
			/* for SLE_STR, pointer has to be valid! so make it NULL */
			memset(lc, 0, sizeof(*lc));
			lc->ct = ct;

			assert((uint)ct < GLCT_END);

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


extern const ChunkHandler _gamelog_chunk_handlers[] = {
	{ 'GLOG', Save_GLOG, Load_GLOG, CH_RIFF | CH_LAST }
};
