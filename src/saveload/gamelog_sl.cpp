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


class SlGamelogMode : public DefaultSaveLoadHandler<SlGamelogMode, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(LoggedChange, mode.mode,         SLE_UINT8),
		SLE_VAR(LoggedChange, mode.landscape,    SLE_UINT8),
	};

	void GenericSaveLoad(LoggedChange *lc) const
	{
		if (lc->ct != GLCT_MODE) return;
		SlObject(lc, this->GetDescription());
	}

	void Save(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void Load(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void LoadCheck(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
};

class SlGamelogRevision : public DefaultSaveLoadHandler<SlGamelogRevision, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_ARR(LoggedChange, revision.text,     SLE_UINT8,  GAMELOG_REVISION_LENGTH),
		SLE_VAR(LoggedChange, revision.newgrf,   SLE_UINT32),
		SLE_VAR(LoggedChange, revision.slver,    SLE_UINT16),
		SLE_VAR(LoggedChange, revision.modified, SLE_UINT8),
	};

	void GenericSaveLoad(LoggedChange *lc) const
	{
		if (lc->ct != GLCT_REVISION) return;
		SlObject(lc, this->GetDescription());
	}

	void Save(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void Load(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void LoadCheck(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
};

class SlGamelogOldver : public DefaultSaveLoadHandler<SlGamelogOldver, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(LoggedChange, oldver.type,       SLE_UINT32),
		SLE_VAR(LoggedChange, oldver.version,    SLE_UINT32),
	};

	void GenericSaveLoad(LoggedChange *lc) const
	{
		if (lc->ct != GLCT_OLDVER) return;
		SlObject(lc, this->GetDescription());
	}

	void Save(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void Load(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void LoadCheck(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
};

class SlGamelogSetting : public DefaultSaveLoadHandler<SlGamelogSetting, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_STR(LoggedChange, setting.name,      SLE_STR,    128),
		SLE_VAR(LoggedChange, setting.oldval,    SLE_INT32),
		SLE_VAR(LoggedChange, setting.newval,    SLE_INT32),
	};

	void GenericSaveLoad(LoggedChange *lc) const
	{
		if (lc->ct != GLCT_SETTING) return;
		SlObject(lc, this->GetDescription());
	}

	void Save(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void Load(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void LoadCheck(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
};

class SlGamelogGrfadd : public DefaultSaveLoadHandler<SlGamelogGrfadd, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(LoggedChange, grfadd.grfid,      SLE_UINT32    ),
		SLE_ARR(LoggedChange, grfadd.md5sum,     SLE_UINT8,  16),
	};

	void GenericSaveLoad(LoggedChange *lc) const
	{
		if (lc->ct != GLCT_GRFADD) return;
		SlObject(lc, this->GetDescription());
	}

	void Save(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void Load(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void LoadCheck(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
};

class SlGamelogGrfrem : public DefaultSaveLoadHandler<SlGamelogGrfrem, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(LoggedChange, grfrem.grfid,      SLE_UINT32),
	};

	void GenericSaveLoad(LoggedChange *lc) const
	{
		if (lc->ct != GLCT_GRFREM) return;
		SlObject(lc, this->GetDescription());
	}

	void Save(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void Load(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void LoadCheck(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
};

class SlGamelogGrfcompat : public DefaultSaveLoadHandler<SlGamelogGrfcompat, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(LoggedChange, grfcompat.grfid,   SLE_UINT32    ),
		SLE_ARR(LoggedChange, grfcompat.md5sum,  SLE_UINT8,  16),
	};

	void GenericSaveLoad(LoggedChange *lc) const
	{
		if (lc->ct != GLCT_GRFCOMPAT) return;
		SlObject(lc, this->GetDescription());
	}

	void Save(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void Load(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void LoadCheck(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
};

class SlGamelogGrfparam : public DefaultSaveLoadHandler<SlGamelogGrfparam, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(LoggedChange, grfparam.grfid,    SLE_UINT32),
	};

	void GenericSaveLoad(LoggedChange *lc) const
	{
		if (lc->ct != GLCT_GRFPARAM) return;
		SlObject(lc, this->GetDescription());
	}

	void Save(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void Load(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void LoadCheck(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
};

class SlGamelogGrfmove : public DefaultSaveLoadHandler<SlGamelogGrfmove, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(LoggedChange, grfmove.grfid,     SLE_UINT32),
		SLE_VAR(LoggedChange, grfmove.offset,    SLE_INT32),
	};

	void GenericSaveLoad(LoggedChange *lc) const
	{
		if (lc->ct != GLCT_GRFMOVE) return;
		SlObject(lc, this->GetDescription());
	}

	void Save(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void Load(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void LoadCheck(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
};

class SlGamelogGrfbug : public DefaultSaveLoadHandler<SlGamelogGrfbug, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(LoggedChange, grfbug.data,       SLE_UINT64),
		SLE_VAR(LoggedChange, grfbug.grfid,      SLE_UINT32),
		SLE_VAR(LoggedChange, grfbug.bug,        SLE_UINT8),
	};

	void GenericSaveLoad(LoggedChange *lc) const
	{
		if (lc->ct != GLCT_GRFBUG) return;
		SlObject(lc, this->GetDescription());
	}

	void Save(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void Load(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void LoadCheck(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
};

static bool _is_emergency_save = true;

class SlGamelogEmergency : public DefaultSaveLoadHandler<SlGamelogEmergency, LoggedChange> {
public:
	/* We need to store something, so store a "true" value. */
	inline static const SaveLoad description[] = {
		SLEG_CONDVAR(_is_emergency_save, SLE_BOOL, SLV_RIFF_TO_ARRAY, SL_MAX_VERSION),
	};

	void GenericSaveLoad(LoggedChange *lc) const
	{
		if (lc->ct != GLCT_EMERGENCY) return;

		_is_emergency_save = true;
		SlObject(lc, this->GetDescription());
	}

	void Save(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void Load(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
	void LoadCheck(LoggedChange *lc) const override { this->GenericSaveLoad(lc); }
};

class SlGamelogAction : public DefaultSaveLoadHandler<SlGamelogAction, LoggedAction> {
public:
	inline static const SaveLoad description[] = {
		SLE_SAVEBYTE(LoggedChange, ct),
		SLEG_STRUCT(SlGamelogMode),
		SLEG_STRUCT(SlGamelogRevision),
		SLEG_STRUCT(SlGamelogOldver),
		SLEG_STRUCT(SlGamelogSetting),
		SLEG_STRUCT(SlGamelogGrfadd),
		SLEG_STRUCT(SlGamelogGrfrem),
		SLEG_STRUCT(SlGamelogGrfcompat),
		SLEG_STRUCT(SlGamelogGrfparam),
		SLEG_STRUCT(SlGamelogGrfmove),
		SLEG_STRUCT(SlGamelogGrfbug),
		SLEG_STRUCT(SlGamelogEmergency),
	};

	void Save(LoggedAction *la) const override
	{
		SlSetStructListLength(la->changes);

		const LoggedChange *lcend = &la->change[la->changes];
		for (LoggedChange *lc = la->change; lc != lcend; lc++) {
			assert((uint)lc->ct < GLCT_END);
			SlObject(lc, this->GetDescription());
		}
	}

	void Load(LoggedAction *la) const override
	{
		if (IsSavegameVersionBefore(SLV_RIFF_TO_ARRAY)) {
			byte type;
			while ((type = SlReadByte()) != GLCT_NONE) {
				if (type >= GLCT_END) SlErrorCorrupt("Invalid gamelog change type");
				GamelogChangeType ct = (GamelogChangeType)type;

				la->change = ReallocT(la->change, la->changes + 1);

				LoggedChange *lc = &la->change[la->changes++];
				memset(lc, 0, sizeof(*lc));
				lc->ct = ct;

				SlObject(lc, this->GetDescription());
			}
			return;
		}

		size_t length = SlGetStructListLength(UINT32_MAX);
		la->change = ReallocT(la->change, length);

		for (size_t i = 0; i < length; i++) {
			LoggedChange *lc = &la->change[i];
			memset(lc, 0, sizeof(*lc));

			lc->ct = (GamelogChangeType)SlReadByte();
			SlObject(lc, this->GetDescription());
		}
	}

	void LoadCheck(LoggedAction *la) const override { this->Load(la); }
};

static const SaveLoad _gamelog_desc[] = {
	SLE_CONDVAR(LoggedAction, at,            SLE_UINT8,   SLV_RIFF_TO_ARRAY, SL_MAX_VERSION),
	SLE_VAR(LoggedAction, tick,              SLE_UINT16),
	SLEG_STRUCTLIST(SlGamelogAction),
};

static void Load_GLOG_common(LoggedAction *&gamelog_action, uint &gamelog_actions)
{
	assert(gamelog_action == nullptr);
	assert(gamelog_actions == 0);

	if (IsSavegameVersionBefore(SLV_RIFF_TO_ARRAY)) {
		byte type;
		while ((type = SlReadByte()) != GLAT_NONE) {
			if (type >= GLAT_END) SlErrorCorrupt("Invalid gamelog action type");

			gamelog_action = ReallocT(gamelog_action, gamelog_actions + 1);
			LoggedAction *la = &gamelog_action[gamelog_actions++];
			memset(la, 0, sizeof(*la));

			la->at = (GamelogActionType)type;
			SlObject(la, _gamelog_desc);
		}
		return;
	}

	while (SlIterateArray() != -1) {
		gamelog_action = ReallocT(gamelog_action, gamelog_actions + 1);
		LoggedAction *la = &gamelog_action[gamelog_actions++];
		memset(la, 0, sizeof(*la));

		SlObject(la, _gamelog_desc);
	}
}

static void Save_GLOG()
{
	const LoggedAction *laend = &_gamelog_action[_gamelog_actions];

	uint i = 0;
	for (LoggedAction *la = _gamelog_action; la != laend; la++, i++) {
		SlSetArrayIndex(i);
		SlObject(la, _gamelog_desc);
	}
}

static void Load_GLOG()
{
	Load_GLOG_common(_gamelog_action, _gamelog_actions);
}

static void Check_GLOG()
{
	Load_GLOG_common(_load_check_data.gamelog_action, _load_check_data.gamelog_actions);
}

static const ChunkHandler gamelog_chunk_handlers[] = {
	{ 'GLOG', Save_GLOG, Load_GLOG, nullptr, Check_GLOG, CH_ARRAY }
};

extern const ChunkHandlerTable _gamelog_chunk_handlers(gamelog_chunk_handlers);
