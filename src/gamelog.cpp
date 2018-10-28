/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gamelog.cpp Definition of functions used for logging of important changes in the game */

#include "stdafx.h"
#include "saveload/saveload.h"
#include "string_func.h"
#include "settings_type.h"
#include "gamelog_internal.h"
#include "console_func.h"
#include "debug.h"
#include "date_func.h"
#include "rev.h"

#include <stdarg.h>

#include "safeguards.h"

extern const uint16 SAVEGAME_VERSION;  ///< current savegame version

extern SavegameType _savegame_type; ///< type of savegame we are loading

extern uint32 _ttdp_version;     ///< version of TTDP savegame (if applicable)
extern uint16 _sl_version;       ///< the major savegame version identifier
extern byte   _sl_minor_version; ///< the minor savegame version, DO NOT USE!


static GamelogActionType _gamelog_action_type = GLAT_NONE; ///< action to record if anything changes

LoggedAction *_gamelog_action = NULL;        ///< first logged action
uint _gamelog_actions         = 0;           ///< number of actions
static LoggedAction *_current_action = NULL; ///< current action we are logging, NULL when there is no action active


/**
 * Stores information about new action, but doesn't allocate it
 * Action is allocated only when there is at least one change
 * @param at type of action
 */
void GamelogStartAction(GamelogActionType at)
{
	assert(_gamelog_action_type == GLAT_NONE); // do not allow starting new action without stopping the previous first
	_gamelog_action_type = at;
}

/**
 * Stops logging of any changes
 */
void GamelogStopAction()
{
	assert(_gamelog_action_type != GLAT_NONE); // nobody should try to stop if there is no action in progress

	bool print = _current_action != NULL;

	_current_action = NULL;
	_gamelog_action_type = GLAT_NONE;

	if (print) GamelogPrintDebug(5);
}

/**
 * Frees the memory allocated by a gamelog
 */
void GamelogFree(LoggedAction *gamelog_action, uint gamelog_actions)
{
	for (uint i = 0; i < gamelog_actions; i++) {
		const LoggedAction *la = &gamelog_action[i];
		for (uint j = 0; j < la->changes; j++) {
			const LoggedChange *lc = &la->change[j];
			if (lc->ct == GLCT_SETTING) free(lc->setting.name);
		}
		free(la->change);
	}

	free(gamelog_action);
}

/**
 * Resets and frees all memory allocated - used before loading or starting a new game
 */
void GamelogReset()
{
	assert(_gamelog_action_type == GLAT_NONE);
	GamelogFree(_gamelog_action, _gamelog_actions);

	_gamelog_action  = NULL;
	_gamelog_actions = 0;
	_current_action  = NULL;
}

/**
 * Prints GRF ID, checksum and filename if found
 * @param buf The location in the buffer to draw
 * @param last The end of the buffer
 * @param grfid GRF ID
 * @param md5sum array of md5sum to print, if known
 * @param gc GrfConfig, if known
 * @return The buffer location.
 */
static char *PrintGrfInfo(char *buf, const char *last, uint grfid, const uint8 *md5sum, const GRFConfig *gc)
{
	char txt[40];

	if (md5sum != NULL) {
		md5sumToString(txt, lastof(txt), md5sum);
		buf += seprintf(buf, last, "GRF ID %08X, checksum %s", BSWAP32(grfid), txt);
	} else {
		buf += seprintf(buf, last, "GRF ID %08X", BSWAP32(grfid));
	}

	if (gc != NULL) {
		buf += seprintf(buf, last, ", filename: %s (md5sum matches)", gc->filename);
	} else {
		gc = FindGRFConfig(grfid, FGCM_ANY);
		if (gc != NULL) {
			buf += seprintf(buf, last, ", filename: %s (matches GRFID only)", gc->filename);
		} else {
			buf += seprintf(buf, last, ", unknown GRF");
		}
	}
	return buf;
}


/** Text messages for various logged actions */
static const char * const la_text[] = {
	"new game started",
	"game loaded",
	"GRF config changed",
	"cheat was used",
	"settings changed",
	"GRF bug triggered",
	"emergency savegame",
};

assert_compile(lengthof(la_text) == GLAT_END);

/**
 * Information about the presence of a Grf at a certain point during gamelog history
 * Note about missing Grfs:
 * Changes to missing Grfs are not logged including manual removal of the Grf.
 * So if the gamelog tells a Grf is missing we do not know whether it was readded or completely removed
 * at some later point.
 */
struct GRFPresence{
	const GRFConfig *gc;  ///< GRFConfig, if known
	bool was_missing;     ///< Grf was missing during some gameload in the past

	GRFPresence(const GRFConfig *gc) : gc(gc), was_missing(false) {}
};
typedef SmallMap<uint32, GRFPresence> GrfIDMapping;

/**
 * Prints active gamelog
 * @param proc the procedure to draw with
 */
void GamelogPrint(GamelogPrintProc *proc)
{
	char buffer[1024];
	GrfIDMapping grf_names;

	proc("---- gamelog start ----");

	const LoggedAction *laend = &_gamelog_action[_gamelog_actions];

	for (const LoggedAction *la = _gamelog_action; la != laend; la++) {
		assert((uint)la->at < GLAT_END);

		seprintf(buffer, lastof(buffer), "Tick %u: %s", (uint)la->tick, la_text[(uint)la->at]);
		proc(buffer);

		const LoggedChange *lcend = &la->change[la->changes];

		for (const LoggedChange *lc = la->change; lc != lcend; lc++) {
			char *buf = buffer;

			switch (lc->ct) {
				default: NOT_REACHED();
				case GLCT_MODE:
					buf += seprintf(buf, lastof(buffer), "New game mode: %u landscape: %u",
						(uint)lc->mode.mode, (uint)lc->mode.landscape);
					break;

				case GLCT_REVISION:
					buf += seprintf(buf, lastof(buffer), "Revision text changed to %s, savegame version %u, ",
						lc->revision.text, lc->revision.slver);

					switch (lc->revision.modified) {
						case 0: buf += seprintf(buf, lastof(buffer), "not "); break;
						case 1: buf += seprintf(buf, lastof(buffer), "maybe "); break;
						default: break;
					}

					buf += seprintf(buf, lastof(buffer), "modified, _openttd_newgrf_version = 0x%08x", lc->revision.newgrf);
					break;

				case GLCT_OLDVER:
					buf += seprintf(buf, lastof(buffer), "Conversion from ");
					switch (lc->oldver.type) {
						default: NOT_REACHED();
						case SGT_OTTD:
							buf += seprintf(buf, lastof(buffer), "OTTD savegame without gamelog: version %u, %u",
								GB(lc->oldver.version, 8, 16), GB(lc->oldver.version, 0, 8));
							break;

						case SGT_TTO:
							buf += seprintf(buf, lastof(buffer), "TTO savegame");
							break;

						case SGT_TTD:
							buf += seprintf(buf, lastof(buffer), "TTD savegame");
							break;

						case SGT_TTDP1:
						case SGT_TTDP2:
							buf += seprintf(buf, lastof(buffer), "TTDP savegame, %s format",
								lc->oldver.type == SGT_TTDP1 ? "old" : "new");
							if (lc->oldver.version != 0) {
								buf += seprintf(buf, lastof(buffer), ", TTDP version %u.%u.%u.%u",
									GB(lc->oldver.version, 24, 8), GB(lc->oldver.version, 20, 4),
									GB(lc->oldver.version, 16, 4), GB(lc->oldver.version, 0, 16));
							}
							break;
					}
					break;

				case GLCT_SETTING:
					buf += seprintf(buf, lastof(buffer), "Setting changed: %s : %d -> %d", lc->setting.name, lc->setting.oldval, lc->setting.newval);
					break;

				case GLCT_GRFADD: {
					const GRFConfig *gc = FindGRFConfig(lc->grfadd.grfid, FGCM_EXACT, lc->grfadd.md5sum);
					buf += seprintf(buf, lastof(buffer), "Added NewGRF: ");
					buf = PrintGrfInfo(buf, lastof(buffer), lc->grfadd.grfid, lc->grfadd.md5sum, gc);
					GrfIDMapping::Pair *gm = grf_names.Find(lc->grfrem.grfid);
					if (gm != grf_names.End() && !gm->second.was_missing) buf += seprintf(buf, lastof(buffer), ". Gamelog inconsistency: GrfID was already added!");
					grf_names[lc->grfadd.grfid] = gc;
					break;
				}

				case GLCT_GRFREM: {
					GrfIDMapping::Pair *gm = grf_names.Find(lc->grfrem.grfid);
					buf += seprintf(buf, lastof(buffer), la->at == GLAT_LOAD ? "Missing NewGRF: " : "Removed NewGRF: ");
					buf = PrintGrfInfo(buf, lastof(buffer), lc->grfrem.grfid, NULL, gm != grf_names.End() ? gm->second.gc : NULL);
					if (gm == grf_names.End()) {
						buf += seprintf(buf, lastof(buffer), ". Gamelog inconsistency: GrfID was never added!");
					} else {
						if (la->at == GLAT_LOAD) {
							/* Missing grfs on load are not removed from the configuration */
							gm->second.was_missing = true;
						} else {
							grf_names.Erase(gm);
						}
					}
					break;
				}

				case GLCT_GRFCOMPAT: {
					const GRFConfig *gc = FindGRFConfig(lc->grfadd.grfid, FGCM_EXACT, lc->grfadd.md5sum);
					buf += seprintf(buf, lastof(buffer), "Compatible NewGRF loaded: ");
					buf = PrintGrfInfo(buf, lastof(buffer), lc->grfcompat.grfid, lc->grfcompat.md5sum, gc);
					if (!grf_names.Contains(lc->grfcompat.grfid)) buf += seprintf(buf, lastof(buffer), ". Gamelog inconsistency: GrfID was never added!");
					grf_names[lc->grfcompat.grfid] = gc;
					break;
				}

				case GLCT_GRFPARAM: {
					GrfIDMapping::Pair *gm = grf_names.Find(lc->grfrem.grfid);
					buf += seprintf(buf, lastof(buffer), "GRF parameter changed: ");
					buf = PrintGrfInfo(buf, lastof(buffer), lc->grfparam.grfid, NULL, gm != grf_names.End() ? gm->second.gc : NULL);
					if (gm == grf_names.End()) buf += seprintf(buf, lastof(buffer), ". Gamelog inconsistency: GrfID was never added!");
					break;
				}

				case GLCT_GRFMOVE: {
					GrfIDMapping::Pair *gm = grf_names.Find(lc->grfrem.grfid);
					buf += seprintf(buf, lastof(buffer), "GRF order changed: %08X moved %d places %s",
						BSWAP32(lc->grfmove.grfid), abs(lc->grfmove.offset), lc->grfmove.offset >= 0 ? "down" : "up" );
					buf = PrintGrfInfo(buf, lastof(buffer), lc->grfmove.grfid, NULL, gm != grf_names.End() ? gm->second.gc : NULL);
					if (gm == grf_names.End()) buf += seprintf(buf, lastof(buffer), ". Gamelog inconsistency: GrfID was never added!");
					break;
				}

				case GLCT_GRFBUG: {
					GrfIDMapping::Pair *gm = grf_names.Find(lc->grfrem.grfid);
					switch (lc->grfbug.bug) {
						default: NOT_REACHED();
						case GBUG_VEH_LENGTH:
							buf += seprintf(buf, lastof(buffer), "Rail vehicle changes length outside a depot: GRF ID %08X, internal ID 0x%X", BSWAP32(lc->grfbug.grfid), (uint)lc->grfbug.data);
							break;
					}
					buf = PrintGrfInfo(buf, lastof(buffer), lc->grfbug.grfid, NULL, gm != grf_names.End() ? gm->second.gc : NULL);
					if (gm == grf_names.End()) buf += seprintf(buf, lastof(buffer), ". Gamelog inconsistency: GrfID was never added!");
					break;
				}

				case GLCT_EMERGENCY:
					break;
			}

			proc(buffer);
		}
	}

	proc("---- gamelog end ----");
}


static void GamelogPrintConsoleProc(const char *s)
{
	IConsolePrint(CC_WARNING, s);
}

/** Print the gamelog data to the console. */
void GamelogPrintConsole()
{
	GamelogPrint(&GamelogPrintConsoleProc);
}

static int _gamelog_print_level = 0; ///< gamelog debug level we need to print stuff

static void GamelogPrintDebugProc(const char *s)
{
	DEBUG(gamelog, _gamelog_print_level, "%s", s);
}


/**
 * Prints gamelog to debug output. Code is executed even when
 * there will be no output. It is called very seldom, so it
 * doesn't matter that much. At least it gives more uniform code...
 * @param level debug level we need to print stuff
 */
void GamelogPrintDebug(int level)
{
	_gamelog_print_level = level;
	GamelogPrint(&GamelogPrintDebugProc);
}


/**
 * Allocates new LoggedChange and new LoggedAction if needed.
 * If there is no action active, NULL is returned.
 * @param ct type of change
 * @return new LoggedChange, or NULL if there is no action active
 */
static LoggedChange *GamelogChange(GamelogChangeType ct)
{
	if (_current_action == NULL) {
		if (_gamelog_action_type == GLAT_NONE) return NULL;

		_gamelog_action  = ReallocT(_gamelog_action, _gamelog_actions + 1);
		_current_action  = &_gamelog_action[_gamelog_actions++];

		_current_action->at      = _gamelog_action_type;
		_current_action->tick    = _tick_counter;
		_current_action->change  = NULL;
		_current_action->changes = 0;
	}

	_current_action->change = ReallocT(_current_action->change, _current_action->changes + 1);

	LoggedChange *lc = &_current_action->change[_current_action->changes++];
	lc->ct = ct;

	return lc;
}


/**
 * Logs a emergency savegame
 */
void GamelogEmergency()
{
	/* Terminate any active action */
	if (_gamelog_action_type != GLAT_NONE) GamelogStopAction();
	GamelogStartAction(GLAT_EMERGENCY);
	GamelogChange(GLCT_EMERGENCY);
	GamelogStopAction();
}

/**
 * Finds out if current game is a loaded emergency savegame.
 */
bool GamelogTestEmergency()
{
	const LoggedChange *emergency = NULL;

	const LoggedAction *laend = &_gamelog_action[_gamelog_actions];
	for (const LoggedAction *la = _gamelog_action; la != laend; la++) {
		const LoggedChange *lcend = &la->change[la->changes];
		for (const LoggedChange *lc = la->change; lc != lcend; lc++) {
			if (lc->ct == GLCT_EMERGENCY) emergency = lc;
		}
	}

	return (emergency != NULL);
}

/**
 * Logs a change in game revision
 */
void GamelogRevision()
{
	assert(_gamelog_action_type == GLAT_START || _gamelog_action_type == GLAT_LOAD);

	LoggedChange *lc = GamelogChange(GLCT_REVISION);
	if (lc == NULL) return;

	memset(lc->revision.text, 0, sizeof(lc->revision.text));
	strecpy(lc->revision.text, _openttd_revision, lastof(lc->revision.text));
	lc->revision.slver = SAVEGAME_VERSION;
	lc->revision.modified = _openttd_revision_modified;
	lc->revision.newgrf = _openttd_newgrf_version;
}

/**
 * Logs a change in game mode (scenario editor or game)
 */
void GamelogMode()
{
	assert(_gamelog_action_type == GLAT_START || _gamelog_action_type == GLAT_LOAD || _gamelog_action_type == GLAT_CHEAT);

	LoggedChange *lc = GamelogChange(GLCT_MODE);
	if (lc == NULL) return;

	lc->mode.mode      = _game_mode;
	lc->mode.landscape = _settings_game.game_creation.landscape;
}

/**
 * Logs loading from savegame without gamelog
 */
void GamelogOldver()
{
	assert(_gamelog_action_type == GLAT_LOAD);

	LoggedChange *lc = GamelogChange(GLCT_OLDVER);
	if (lc == NULL) return;

	lc->oldver.type = _savegame_type;
	lc->oldver.version = (_savegame_type == SGT_OTTD ? ((uint32)_sl_version << 8 | _sl_minor_version) : _ttdp_version);
}

/**
 * Logs change in game settings. Only non-networksafe settings are logged
 * @param name setting name
 * @param oldval old setting value
 * @param newval new setting value
 */
void GamelogSetting(const char *name, int32 oldval, int32 newval)
{
	assert(_gamelog_action_type == GLAT_SETTING);

	LoggedChange *lc = GamelogChange(GLCT_SETTING);
	if (lc == NULL) return;

	lc->setting.name = stredup(name);
	lc->setting.oldval = oldval;
	lc->setting.newval = newval;
}


/**
 * Finds out if current revision is different than last revision stored in the savegame.
 * Appends GLCT_REVISION when the revision string changed
 */
void GamelogTestRevision()
{
	const LoggedChange *rev = NULL;

	const LoggedAction *laend = &_gamelog_action[_gamelog_actions];
	for (const LoggedAction *la = _gamelog_action; la != laend; la++) {
		const LoggedChange *lcend = &la->change[la->changes];
		for (const LoggedChange *lc = la->change; lc != lcend; lc++) {
			if (lc->ct == GLCT_REVISION) rev = lc;
		}
	}

	if (rev == NULL || strcmp(rev->revision.text, _openttd_revision) != 0 ||
			rev->revision.modified != _openttd_revision_modified ||
			rev->revision.newgrf != _openttd_newgrf_version) {
		GamelogRevision();
	}
}

/**
 * Finds last stored game mode or landscape.
 * Any change is logged
 */
void GamelogTestMode()
{
	const LoggedChange *mode = NULL;

	const LoggedAction *laend = &_gamelog_action[_gamelog_actions];
	for (const LoggedAction *la = _gamelog_action; la != laend; la++) {
		const LoggedChange *lcend = &la->change[la->changes];
		for (const LoggedChange *lc = la->change; lc != lcend; lc++) {
			if (lc->ct == GLCT_MODE) mode = lc;
		}
	}

	if (mode == NULL || mode->mode.mode != _game_mode || mode->mode.landscape != _settings_game.game_creation.landscape) GamelogMode();
}


/**
 * Logs triggered GRF bug.
 * @param grfid ID of problematic GRF
 * @param bug type of bug, @see enum GRFBugs
 * @param data additional data
 */
static void GamelogGRFBug(uint32 grfid, byte bug, uint64 data)
{
	assert(_gamelog_action_type == GLAT_GRFBUG);

	LoggedChange *lc = GamelogChange(GLCT_GRFBUG);
	if (lc == NULL) return;

	lc->grfbug.data  = data;
	lc->grfbug.grfid = grfid;
	lc->grfbug.bug   = bug;
}

/**
 * Logs GRF bug - rail vehicle has different length after reversing.
 * Ensures this is logged only once for each GRF and engine type
 * This check takes some time, but it is called pretty seldom, so it
 * doesn't matter that much (ideally it shouldn't be called at all).
 * @param grfid the broken NewGRF
 * @param internal_id the internal ID of whatever's broken in the NewGRF
 * @return true iff a unique record was done
 */
bool GamelogGRFBugReverse(uint32 grfid, uint16 internal_id)
{
	const LoggedAction *laend = &_gamelog_action[_gamelog_actions];
	for (const LoggedAction *la = _gamelog_action; la != laend; la++) {
		const LoggedChange *lcend = &la->change[la->changes];
		for (const LoggedChange *lc = la->change; lc != lcend; lc++) {
			if (lc->ct == GLCT_GRFBUG && lc->grfbug.grfid == grfid &&
					lc->grfbug.bug == GBUG_VEH_LENGTH && lc->grfbug.data == internal_id) {
				return false;
			}
		}
	}

	GamelogStartAction(GLAT_GRFBUG);
	GamelogGRFBug(grfid, GBUG_VEH_LENGTH, internal_id);
	GamelogStopAction();

	return true;
}


/**
 * Decides if GRF should be logged
 * @param g grf to determine
 * @return true iff GRF is not static and is loaded
 */
static inline bool IsLoggableGrfConfig(const GRFConfig *g)
{
	return !HasBit(g->flags, GCF_STATIC) && g->status != GCS_NOT_FOUND;
}

/**
 * Logs removal of a GRF
 * @param grfid ID of removed GRF
 */
void GamelogGRFRemove(uint32 grfid)
{
	assert(_gamelog_action_type == GLAT_LOAD || _gamelog_action_type == GLAT_GRF);

	LoggedChange *lc = GamelogChange(GLCT_GRFREM);
	if (lc == NULL) return;

	lc->grfrem.grfid = grfid;
}

/**
 * Logs adding of a GRF
 * @param newg added GRF
 */
void GamelogGRFAdd(const GRFConfig *newg)
{
	assert(_gamelog_action_type == GLAT_LOAD || _gamelog_action_type == GLAT_START || _gamelog_action_type == GLAT_GRF);

	if (!IsLoggableGrfConfig(newg)) return;

	LoggedChange *lc = GamelogChange(GLCT_GRFADD);
	if (lc == NULL) return;

	lc->grfadd = newg->ident;
}

/**
 * Logs loading compatible GRF
 * (the same ID, but different MD5 hash)
 * @param newg new (updated) GRF
 */
void GamelogGRFCompatible(const GRFIdentifier *newg)
{
	assert(_gamelog_action_type == GLAT_LOAD || _gamelog_action_type == GLAT_GRF);

	LoggedChange *lc = GamelogChange(GLCT_GRFCOMPAT);
	if (lc == NULL) return;

	lc->grfcompat = *newg;
}

/**
 * Logs changing GRF order
 * @param grfid GRF that is moved
 * @param offset how far it is moved, positive = moved down
 */
static void GamelogGRFMove(uint32 grfid, int32 offset)
{
	assert(_gamelog_action_type == GLAT_GRF);

	LoggedChange *lc = GamelogChange(GLCT_GRFMOVE);
	if (lc == NULL) return;

	lc->grfmove.grfid  = grfid;
	lc->grfmove.offset = offset;
}

/**
 * Logs change in GRF parameters.
 * Details about parameters changed are not stored
 * @param grfid ID of GRF to store
 */
static void GamelogGRFParameters(uint32 grfid)
{
	assert(_gamelog_action_type == GLAT_GRF);

	LoggedChange *lc = GamelogChange(GLCT_GRFPARAM);
	if (lc == NULL) return;

	lc->grfparam.grfid = grfid;
}

/**
 * Logs adding of list of GRFs.
 * Useful when old savegame is loaded or when new game is started
 * @param newg head of GRF linked list
 */
void GamelogGRFAddList(const GRFConfig *newg)
{
	assert(_gamelog_action_type == GLAT_START || _gamelog_action_type == GLAT_LOAD);

	for (; newg != NULL; newg = newg->next) {
		GamelogGRFAdd(newg);
	}
}

/** List of GRFs using array of pointers instead of linked list */
struct GRFList {
	uint n;
	const GRFConfig *grf[];
};

/**
 * Generates GRFList
 * @param grfc head of GRF linked list
 */
static GRFList *GenerateGRFList(const GRFConfig *grfc)
{
	uint n = 0;
	for (const GRFConfig *g = grfc; g != NULL; g = g->next) {
		if (IsLoggableGrfConfig(g)) n++;
	}

	GRFList *list = (GRFList*)MallocT<byte>(sizeof(GRFList) + n * sizeof(GRFConfig*));

	list->n = 0;
	for (const GRFConfig *g = grfc; g != NULL; g = g->next) {
		if (IsLoggableGrfConfig(g)) list->grf[list->n++] = g;
	}

	return list;
}

/**
 * Compares two NewGRF lists and logs any change
 * @param oldc original GRF list
 * @param newc new GRF list
 */
void GamelogGRFUpdate(const GRFConfig *oldc, const GRFConfig *newc)
{
	GRFList *ol = GenerateGRFList(oldc);
	GRFList *nl = GenerateGRFList(newc);

	uint o = 0, n = 0;

	while (o < ol->n && n < nl->n) {
		const GRFConfig *og = ol->grf[o];
		const GRFConfig *ng = nl->grf[n];

		if (og->ident.grfid != ng->ident.grfid) {
			uint oi, ni;
			for (oi = 0; oi < ol->n; oi++) {
				if (ol->grf[oi]->ident.grfid == nl->grf[n]->ident.grfid) break;
			}
			if (oi < o) {
				/* GRF was moved, this change has been logged already */
				n++;
				continue;
			}
			if (oi == ol->n) {
				/* GRF couldn't be found in the OLD list, GRF was ADDED */
				GamelogGRFAdd(nl->grf[n++]);
				continue;
			}
			for (ni = 0; ni < nl->n; ni++) {
				if (nl->grf[ni]->ident.grfid == ol->grf[o]->ident.grfid) break;
			}
			if (ni < n) {
				/* GRF was moved, this change has been logged already */
				o++;
				continue;
			}
			if (ni == nl->n) {
				/* GRF couldn't be found in the NEW list, GRF was REMOVED */
				GamelogGRFRemove(ol->grf[o++]->ident.grfid);
				continue;
			}

			/* o < oi < ol->n
			 * n < ni < nl->n */
			assert(ni > n && ni < nl->n);
			assert(oi > o && oi < ol->n);

			ni -= n; // number of GRFs it was moved downwards
			oi -= o; // number of GRFs it was moved upwards

			if (ni >= oi) { // prefer the one that is moved further
				/* GRF was moved down */
				GamelogGRFMove(ol->grf[o++]->ident.grfid, ni);
			} else {
				GamelogGRFMove(nl->grf[n++]->ident.grfid, -(int)oi);
			}
		} else {
			if (memcmp(og->ident.md5sum, ng->ident.md5sum, sizeof(og->ident.md5sum)) != 0) {
				/* md5sum changed, probably loading 'compatible' GRF */
				GamelogGRFCompatible(&nl->grf[n]->ident);
			}

			if (og->num_params != ng->num_params || memcmp(og->param, ng->param, og->num_params * sizeof(og->param[0])) != 0) {
				GamelogGRFParameters(ol->grf[o]->ident.grfid);
			}

			o++;
			n++;
		}
	}

	while (o < ol->n) GamelogGRFRemove(ol->grf[o++]->ident.grfid); // remaining GRFs were removed ...
	while (n < nl->n) GamelogGRFAdd   (nl->grf[n++]);              // ... or added

	free(ol);
	free(nl);
}

/**
 * Get some basic information from the given gamelog.
 * @param gamelog_action Pointer to the gamelog to extract information from.
 * @param gamelog_actions Number of actions in the given gamelog.
 * @param[out] last_ottd_rev OpenTTD NewGRF version from the binary that saved the savegame last.
 * @param[out] ever_modified Max value of 'modified' from all binaries that ever saved this savegame.
 * @param[out] removed_newgrfs Set to true if any NewGRFs have been removed.
 */
void GamelogInfo(LoggedAction *gamelog_action, uint gamelog_actions, uint32 *last_ottd_rev, byte *ever_modified, bool *removed_newgrfs)
{
	const LoggedAction *laend = &gamelog_action[gamelog_actions];
	for (const LoggedAction *la = gamelog_action; la != laend; la++) {
		const LoggedChange *lcend = &la->change[la->changes];
		for (const LoggedChange *lc = la->change; lc != lcend; lc++) {
			switch (lc->ct) {
				default: break;

				case GLCT_REVISION:
					*last_ottd_rev = lc->revision.newgrf;
					*ever_modified = max(*ever_modified, lc->revision.modified);
					break;

				case GLCT_GRFREM:
					*removed_newgrfs = true;
					break;
			}
		}
	}
}
