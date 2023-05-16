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

/**
 * Information about the presence of a Grf at a certain point during gamelog history
 * Note about missing Grfs:
 * Changes to missing Grfs are not logged including manual removal of the Grf.
 * So if the gamelog tells a Grf is missing we do not know whether it was readded or completely removed
 * at some later point.
 */
struct GRFPresence {
	const GRFConfig *gc;  ///< GRFConfig, if known
	bool was_missing;     ///< Grf was missing during some gameload in the past

	GRFPresence(const GRFConfig *gc) : gc(gc), was_missing(false) {}
	GRFPresence() = default;
};
using GrfIDMapping = std::map<uint32_t, GRFPresence>;

struct LoggedChange {
	LoggedChange(GamelogChangeType type = GLCT_NONE) : ct(type) {}
	virtual ~LoggedChange() {}
	virtual void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) = 0;

	GamelogChangeType ct;
};

struct LoggedChangeMode : LoggedChange {
	LoggedChangeMode() : LoggedChange(GLCT_MODE) {}
	LoggedChangeMode(byte mode, byte landscape) :
		LoggedChange(GLCT_MODE), mode(mode), landscape(landscape) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;

	byte mode;      ///< new game mode - Editor x Game
	byte landscape; ///< landscape (temperate, arctic, ...)
};

struct LoggedChangeRevision : LoggedChange {
	LoggedChangeRevision() : LoggedChange(GLCT_REVISION) {}
	LoggedChangeRevision(const std::string &text, uint32_t newgrf, uint16_t slver, byte modified) :
		LoggedChange(GLCT_REVISION), text(text), newgrf(newgrf), slver(slver), modified(modified) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;

	std::string text; ///< revision string, _openttd_revision
	uint32_t newgrf;  ///< _openttd_newgrf_version
	uint16_t slver;   ///< _sl_version
	byte modified;    //< _openttd_revision_modified
};

struct LoggedChangeOldVersion : LoggedChange {
	LoggedChangeOldVersion() : LoggedChange(GLCT_OLDVER) {}
	LoggedChangeOldVersion(uint32_t type, uint32_t version) :
		LoggedChange(GLCT_OLDVER), type(type), version(version) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;

	uint32_t type;    ///< type of savegame, @see SavegameType
	uint32_t version; ///< major and minor version OR ttdp version
};

struct LoggedChangeGRFAdd : LoggedChange, GRFIdentifier {
	LoggedChangeGRFAdd() : LoggedChange(GLCT_GRFADD) {}
	LoggedChangeGRFAdd(const GRFIdentifier &ident) :
		LoggedChange(GLCT_GRFADD), GRFIdentifier(ident) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;
};

struct LoggedChangeGRFRemoved : LoggedChange {
	LoggedChangeGRFRemoved() : LoggedChange(GLCT_GRFREM) {}
	LoggedChangeGRFRemoved(uint32_t grfid) :
		LoggedChange(GLCT_GRFREM), grfid(grfid) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;

	uint32_t grfid; ///< ID of removed GRF
};

struct LoggedChangeGRFChanged : LoggedChange, GRFIdentifier {
	LoggedChangeGRFChanged() : LoggedChange(GLCT_GRFCOMPAT) {}
	LoggedChangeGRFChanged(const GRFIdentifier &ident) :
		LoggedChange(GLCT_GRFCOMPAT), GRFIdentifier(ident) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;
};

struct LoggedChangeGRFParameterChanged : LoggedChange {
	LoggedChangeGRFParameterChanged() : LoggedChange(GLCT_GRFPARAM) {}
	LoggedChangeGRFParameterChanged(uint32_t grfid) :
		LoggedChange(GLCT_GRFPARAM), grfid(grfid) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;

	uint32_t grfid; ///< ID of GRF with changed parameters
};

struct LoggedChangeGRFMoved : LoggedChange {
	LoggedChangeGRFMoved() : LoggedChange(GLCT_GRFMOVE) {}
	LoggedChangeGRFMoved(uint32_t grfid, int32_t offset) :
		LoggedChange(GLCT_GRFMOVE), grfid(grfid), offset(offset) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;

	uint32_t grfid; ///< ID of moved GRF
	int32_t offset; ///< offset, positive = move down
};

struct LoggedChangeSettingChanged : LoggedChange {
	LoggedChangeSettingChanged() : LoggedChange(GLCT_SETTING) {}
	LoggedChangeSettingChanged(const std::string &name, int32_t oldval, int32_t newval) :
		LoggedChange(GLCT_SETTING), name(name), oldval(oldval), newval(newval) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;

	std::string name; ///< name of the setting
	int32_t oldval;   ///< old value
	int32_t newval;   ///< new value
};

struct LoggedChangeGRFBug : LoggedChange {
	LoggedChangeGRFBug() : LoggedChange(GLCT_GRFBUG) {}
	LoggedChangeGRFBug(uint64_t data, uint32_t grfid, byte bug) :
		LoggedChange(GLCT_GRFBUG), data(data), grfid(grfid), bug(bug) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;

	uint64_t data;  ///< additional data
	uint32_t grfid; ///< ID of problematic GRF
	byte bug;       ///< type of bug, @see enum GRFBugs
};

struct LoggedChangeEmergencySave : LoggedChange {
	LoggedChangeEmergencySave() : LoggedChange(GLCT_EMERGENCY) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;
};


/** Contains information about one logged action that caused at least one logged change */
struct LoggedAction {
	std::vector<std::unique_ptr<LoggedChange>> change; ///< Logged changes in this action
	GamelogActionType at; ///< Type of action
	uint64_t tick;        ///< Tick when it happened
};

struct GamelogInternalData {
	std::vector<LoggedAction> action;
};

#endif /* GAMELOG_INTERNAL_H */
