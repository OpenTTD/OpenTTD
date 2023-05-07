/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gamelog.h Functions to be called to log possibly unsafe game events */

#ifndef GAMELOG_H
#define GAMELOG_H

#include "newgrf_config.h"

/** The actions we log. */
enum GamelogActionType : uint8 {
	GLAT_START,        ///< Game created
	GLAT_LOAD,         ///< Game loaded
	GLAT_GRF,          ///< GRF changed
	GLAT_CHEAT,        ///< Cheat was used
	GLAT_SETTING,      ///< Setting changed
	GLAT_GRFBUG,       ///< GRF bug was triggered
	GLAT_EMERGENCY,    ///< Emergency savegame
	GLAT_END,          ///< So we know how many GLATs are there
	GLAT_NONE  = 0xFF, ///< No logging active; in savegames, end of list
};

/** Type of logged change */
enum GamelogChangeType : uint8 {
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

struct LoggedChange;
struct LoggedAction;
struct GamelogInternalData;

class Gamelog {
private:
	std::unique_ptr<GamelogInternalData> data;
	GamelogActionType action_type;
	struct LoggedAction *current_action;

	LoggedChange *Change(GamelogChangeType ct);

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
	void Setting(const std::string &name, int32 oldval, int32 newval);

	void GRFUpdate(const GRFConfig *oldg, const GRFConfig *newg);
	void GRFAddList(const GRFConfig *newg);
	void GRFRemove(uint32 grfid);
	void GRFAdd(const GRFConfig *newg);
	void GRFBug(uint32 grfid, byte bug, uint64 data);
	bool GRFBugReverse(uint32 grfid, uint16 internal_id);
	void GRFCompatible(const GRFIdentifier *newg);
	void GRFMove(uint32 grfid, int32 offset);
	void GRFParameters(uint32 grfid);

	void TestRevision();
	void TestMode();

	void Info(uint32 *last_ottd_rev, byte *ever_modified, bool *removed_newgrfs);
	const GRFIdentifier *GetOverriddenIdentifier(const GRFConfig *c);

	/* Saveload handler for gamelog needs access to internal data. */
	friend struct GLOGChunkHandler;
};

extern Gamelog _gamelog;

#endif /* GAMELOG_H */
