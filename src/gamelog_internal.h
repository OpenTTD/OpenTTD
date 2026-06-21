/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gamelog_internal.h Declaration shared among gamelog.cpp and saveload/gamelog_sl.cpp. */

#ifndef GAMELOG_INTERNAL_H
#define GAMELOG_INTERNAL_H

#include "gamelog.h"
#include "openttd.h"
#include "landscape_type.h"

/**
 * Information about the presence of a Grf at a certain point during gamelog history
 * Note about missing Grfs:
 * Changes to missing Grfs are not logged including manual removal of the Grf.
 * So if the gamelog tells a Grf is missing we do not know whether it was re-added or completely removed
 * at some later point.
 */
struct GRFPresence {
	const GRFConfig *gc = nullptr; ///< GRFConfig, if known
	bool was_missing = false; ///< Grf was missing during some gameload in the past

	GRFPresence(const GRFConfig *gc) : gc(gc) {}
	GRFPresence() = default;
};
using GrfIDMapping = std::map<uint32_t, GRFPresence>;

struct LoggedChange {
	LoggedChange(GamelogChangeType type = GamelogChangeType::None) : ct(type) {}
	/** Ensure the destructor of the sub classes are called as well. */
	virtual ~LoggedChange() = default;

	/**
	 * Format the content of this change into the given output.
	 * @param output_iterator Destination of the formatted content.
	 * @param grf_names Cache/mapping of names of NewGRFs seen in the logs.
	 * @param action_type The context in which this method was called.
	 */
	virtual void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) = 0;

	GamelogChangeType ct{};
};

/** Log element for the change of the game mode and landscape. */
struct LoggedChangeMode : LoggedChange {
	/** Constructor for savegame loading. */
	LoggedChangeMode() : LoggedChange(GamelogChangeType::Mode) {}

	/**
	 * Create the log for changing the game mode.
	 * @param mode The new GameMode.
	 * @param landscape The new landscape.
	 */
	LoggedChangeMode(GameMode mode, LandscapeType landscape) :
		LoggedChange(GamelogChangeType::Mode), mode(mode), landscape(landscape) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;

	GameMode mode{}; ///< new game mode - Editor x Game
	LandscapeType landscape{}; ///< landscape (temperate, arctic, ...)
};

struct LoggedChangeRevision : LoggedChange {
	LoggedChangeRevision() : LoggedChange(GamelogChangeType::Revision) {}
	LoggedChangeRevision(const std::string &text, uint32_t newgrf, uint16_t slver, uint8_t modified) :
		LoggedChange(GamelogChangeType::Revision), text(text), newgrf(newgrf), slver(slver), modified(modified) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;

	std::string text{}; ///< revision string, _openttd_revision
	uint32_t newgrf = 0; ///< _openttd_newgrf_version
	uint16_t slver = 0; ///< _sl_version
	uint8_t modified = 0; ///< _openttd_revision_modified
};

struct LoggedChangeOldVersion : LoggedChange {
	LoggedChangeOldVersion() : LoggedChange(GamelogChangeType::OldVer) {}
	LoggedChangeOldVersion(uint32_t type, uint32_t version) :
		LoggedChange(GamelogChangeType::OldVer), type(type), version(version) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;

	uint32_t type = 0; ///< type of savegame, @see SavegameType
	uint32_t version = 0; ///< major and minor version OR ttdp version
};

struct LoggedChangeGRFAdd : LoggedChange, GRFIdentifier {
	LoggedChangeGRFAdd() : LoggedChange(GamelogChangeType::GRFAdd) {}
	LoggedChangeGRFAdd(const GRFIdentifier &ident) :
		LoggedChange(GamelogChangeType::GRFAdd), GRFIdentifier(ident) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;
};

struct LoggedChangeGRFRemoved : LoggedChange {
	LoggedChangeGRFRemoved() : LoggedChange(GamelogChangeType::GRFRem) {}
	LoggedChangeGRFRemoved(uint32_t grfid) :
		LoggedChange(GamelogChangeType::GRFRem), grfid(grfid) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;

	uint32_t grfid = 0; ///< ID of removed GRF
};

struct LoggedChangeGRFChanged : LoggedChange, GRFIdentifier {
	LoggedChangeGRFChanged() : LoggedChange(GamelogChangeType::GRFCompat) {}
	LoggedChangeGRFChanged(const GRFIdentifier &ident) :
		LoggedChange(GamelogChangeType::GRFCompat), GRFIdentifier(ident) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;
};

struct LoggedChangeGRFParameterChanged : LoggedChange {
	LoggedChangeGRFParameterChanged() : LoggedChange(GamelogChangeType::GRFParam) {}
	LoggedChangeGRFParameterChanged(uint32_t grfid) :
		LoggedChange(GamelogChangeType::GRFParam), grfid(grfid) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;

	uint32_t grfid = 0; ///< ID of GRF with changed parameters
};

struct LoggedChangeGRFMoved : LoggedChange {
	LoggedChangeGRFMoved() : LoggedChange(GamelogChangeType::GRFMove) {}
	LoggedChangeGRFMoved(uint32_t grfid, int32_t offset) :
		LoggedChange(GamelogChangeType::GRFMove), grfid(grfid), offset(offset) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;

	uint32_t grfid = 0; ///< ID of moved GRF
	int32_t offset = 0; ///< offset, positive = move down
};

struct LoggedChangeSettingChanged : LoggedChange {
	LoggedChangeSettingChanged() : LoggedChange(GamelogChangeType::Setting) {}
	LoggedChangeSettingChanged(const std::string &name, int32_t oldval, int32_t newval) :
		LoggedChange(GamelogChangeType::Setting), name(name), oldval(oldval), newval(newval) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;

	std::string name{}; ///< name of the setting
	int32_t oldval = 0; ///< old value
	int32_t newval = 0; ///< new value
};

struct LoggedChangeGRFBug : LoggedChange {
	LoggedChangeGRFBug() : LoggedChange(GamelogChangeType::GRFBug) {}
	LoggedChangeGRFBug(uint64_t data, uint32_t grfid, GRFBug bug) :
		LoggedChange(GamelogChangeType::GRFBug), data(data), grfid(grfid), bug(bug) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;

	uint64_t data = 0; ///< additional data
	uint32_t grfid = 0; ///< ID of problematic GRF
	GRFBug bug{}; ///< type of bug, @see enum GRFBugs
};

struct LoggedChangeEmergencySave : LoggedChange {
	LoggedChangeEmergencySave() : LoggedChange(GamelogChangeType::Emergency) {}
	void FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type) override;
};


/** Contains information about one logged action that caused at least one logged change */
struct LoggedAction {
	std::vector<std::unique_ptr<LoggedChange>> change; ///< Logged changes in this action
	GamelogActionType at{}; ///< Type of action
	uint64_t tick = 0; ///< Tick when it happened
};

struct GamelogInternalData {
	std::vector<LoggedAction> action;
};

#endif /* GAMELOG_INTERNAL_H */
