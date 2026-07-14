/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gamelog.h Functions to be called to log fundamental changes to the game. */

#ifndef GAMELOG_H
#define GAMELOG_H

#include "newgrf_config.h"
#include "newgrf_type.h"

/** The actions we log. */
enum class GamelogActionType : uint8_t {
	Start, ///< Game created.
	Load, ///< Game loaded.
	GRF, ///< GRF changed.
	Cheat, ///< Cheat was used.
	Setting, ///< Setting changed.
	GRFBug, ///< GRF bug was triggered.
	Emergency, ///< Emergency savegame.
	End, ///< End marker.
	None = 0xFF, ///< No logging active; in savegames, end of list.
};

/** Type of logged change */
enum class GamelogChangeType : uint8_t {
	Mode, ///< Scenario editor x Game, different landscape.
	Revision, ///< Changed game revision string.
	OldVer, ///< Loaded from savegame without logged data.
	Setting, ///< Non-networksafe setting value changed.
	GRFAdd, ///< Removed GRF.
	GRFRem, ///< Added GRF.
	GRFCompat, ///< Loading compatible GRF.
	GRFParam, ///< GRF parameter changed.
	GRFMove, ///< GRF order changed.
	GRFBug, ///< GRF bug triggered.
	Emergency, ///< Emergency savegame.
	End, ///< End marker.
	None = 0xFF, ///< In savegames, end of list.
};

struct LoggedChange;
struct LoggedAction;
struct GamelogInternalData;

class Gamelog {
private:
	std::unique_ptr<GamelogInternalData> data;
	GamelogActionType action_type;
	struct LoggedAction *current_action;

	void Change(std::unique_ptr<LoggedChange> &&change);

public:
	Gamelog();
	~Gamelog();

	void StartAction(GamelogActionType at);
	void StopAction();
	void StopAnyAction();

	void Reset();

	void Print(std::function<void(const std::string &)> proc);
	void PrintDebug(int level);
	void PrintConsole();

	void Emergency();
	bool TestEmergency();

	void Revision();
	void Mode();
	void Oldver();
	void Setting(const std::string &name, int32_t oldval, int32_t newval);

	void GRFUpdate(const GRFConfigList &oldg, const GRFConfigList &newg);
	void GRFAddList(const GRFConfigList &newg);
	void GRFRemove(GrfID grfid);
	void GRFAdd(const GRFConfig &newg);
	void GRFBug(GrfID grfid, ::GRFBug bug, uint64_t data);
	bool GRFBugReverse(GrfID grfid, uint16_t internal_id);
	void GRFCompatible(const GRFIdentifier &newg);
	void GRFMove(GrfID grfid, int32_t offset);
	void GRFParameters(GrfID grfid);

	void TestRevision();
	void TestMode();

	void Info(uint32_t *last_ottd_rev, uint8_t *ever_modified, bool *removed_newgrfs);
	const GRFIdentifier &GetOverriddenIdentifier(const GRFConfig &c);

	/* Saveload handler for gamelog needs access to internal data. */
	friend struct GLOGChunkHandler;
};

extern Gamelog _gamelog;

#endif /* GAMELOG_H */
