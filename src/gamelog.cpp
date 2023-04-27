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
#include "timer/timer_game_tick.h"
#include "rev.h"

#include <stdarg.h>

#include "safeguards.h"

extern const SaveLoadVersion SAVEGAME_VERSION;  ///< current savegame version

extern SavegameType _savegame_type; ///< type of savegame we are loading

extern uint32 _ttdp_version;        ///< version of TTDP savegame (if applicable)
extern SaveLoadVersion _sl_version; ///< the major savegame version identifier
extern byte   _sl_minor_version;    ///< the minor savegame version, DO NOT USE!

Gamelog _gamelog; ///< Gamelog instance

LoggedChange::~LoggedChange()
{
	if (this->ct == GLCT_SETTING) free(this->setting.name);
}

Gamelog::Gamelog()
{
	this->data = std::make_unique<GamelogInternalData>();
	this->action_type = GLAT_NONE;
	this->current_action = nullptr;
}

Gamelog::~Gamelog()
{
}

/**
 * Return the revision string for the current client version, for use in gamelog.
 * The string returned is at most GAMELOG_REVISION_LENGTH bytes long.
 */
static const char * GetGamelogRevisionString()
{
	/* Allocate a buffer larger than necessary (git revision hash is 40 bytes) to avoid truncation later */
	static char gamelog_revision[48] = { 0 };
	static_assert(lengthof(gamelog_revision) > GAMELOG_REVISION_LENGTH);

	if (IsReleasedVersion()) {
		return _openttd_revision;
	} else if (gamelog_revision[0] == 0) {
		/* Prefix character indication revision status */
		assert(_openttd_revision_modified < 3);
		gamelog_revision[0] = "gum"[_openttd_revision_modified]; // g = "git", u = "unknown", m = "modified"
		/* Append the revision hash */
		strecat(gamelog_revision, _openttd_revision_hash, lastof(gamelog_revision));
		/* Truncate string to GAMELOG_REVISION_LENGTH bytes */
		gamelog_revision[GAMELOG_REVISION_LENGTH - 1] = '\0';
	}
	return gamelog_revision;
}

/**
 * Stores information about new action, but doesn't allocate it
 * Action is allocated only when there is at least one change
 * @param at type of action
 */
void Gamelog::StartAction(GamelogActionType at)
{
	assert(this->action_type == GLAT_NONE); // do not allow starting new action without stopping the previous first
	this->action_type = at;
}

/**
 * Stops logging of any changes
 */
void Gamelog::StopAction()
{
	assert(this->action_type != GLAT_NONE); // nobody should try to stop if there is no action in progress

	bool print = this->current_action != nullptr;

	this->current_action = nullptr;
	this->action_type = GLAT_NONE;

	if (print) this->PrintDebug(5);
}

void Gamelog::StopAnyAction()
{
	if (this->action_type != GLAT_NONE) this->StopAction();
}

/**
 * Resets and frees all memory allocated - used before loading or starting a new game
 */
void Gamelog::Reset()
{
	assert(this->action_type == GLAT_NONE);
	this->data->action.clear();
	this->current_action  = nullptr;
}

/**
 * Adds the GRF ID, checksum and filename if found to the output iterator
 * @param outputIterator The iterator to add the GRF info to.
 * @param last The end of the buffer
 * @param grfid GRF ID
 * @param md5sum array of md5sum to print, if known
 * @param gc GrfConfig, if known
 */
template <typename T>
static void AddGrfInfo(T &outputIterator, uint grfid, const uint8 *md5sum, const GRFConfig *gc)
{
	if (md5sum != nullptr) {
		fmt::format_to(outputIterator, "GRF ID {:08X}, checksum {}", BSWAP32(grfid), MD5SumToString(md5sum));
	} else {
		fmt::format_to(outputIterator, "GRF ID {:08X}", BSWAP32(grfid));
	}

	if (gc != nullptr) {
		fmt::format_to(outputIterator, ", filename: {} (md5sum matches)", gc->filename);
	} else {
		gc = FindGRFConfig(grfid, FGCM_ANY);
		if (gc != nullptr) {
			fmt::format_to(outputIterator, ", filename: {} (matches GRFID only)", gc->filename);
		} else {
			fmt::format_to(outputIterator, ", unknown GRF");
		}
	}
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

static_assert(lengthof(la_text) == GLAT_END);

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
	GRFPresence() = default;
};
typedef SmallMap<uint32, GRFPresence> GrfIDMapping;

/**
 * Prints active gamelog
 * @param proc the procedure to draw with
 */
void Gamelog::Print(std::function<void(const std::string &)> proc)
{
	GrfIDMapping grf_names;

	proc("---- gamelog start ----");

	for (const LoggedAction &la : this->data->action) {
		assert((uint)la.at < GLAT_END);

		proc(fmt::format("Tick {}: {}", la.tick, la_text[la.at]));

		for (const LoggedChange &lc : la.change) {
			std::string message;
			auto outputIterator = std::back_inserter(message);

			switch (lc.ct) {
				default: NOT_REACHED();
				case GLCT_MODE:
					/* Changing landscape, or going from scenario editor to game or back. */
					fmt::format_to(outputIterator, "New game mode: {} landscape: {}", lc.mode.mode, lc.mode.landscape);
					break;

				case GLCT_REVISION:
					/* The game was loaded in a diffferent version than before. */
					fmt::format_to(outputIterator, "Revision text changed to {}, savegame version {}, ",
						lc.revision.text, lc.revision.slver);

					switch (lc.revision.modified) {
						case 0: message += "not "; break;
						case 1: message += "maybe "; break;
						default: break;
					}

					fmt::format_to(outputIterator, "modified, _openttd_newgrf_version = 0x{:08x}", lc.revision.newgrf);
					break;

				case GLCT_OLDVER:
					/* The game was loaded from before 0.7.0-beta1. */
					message += "Conversion from ";
					switch (lc.oldver.type) {
						default: NOT_REACHED();
						case SGT_OTTD:
							fmt::format_to(outputIterator, "OTTD savegame without gamelog: version {}, {}",
								GB(lc.oldver.version, 8, 16), GB(lc.oldver.version, 0, 8));
							break;

						case SGT_TTO:
							message += "TTO savegame";
							break;

						case SGT_TTD:
							message += "TTD savegame";
							break;

						case SGT_TTDP1:
						case SGT_TTDP2:
							fmt::format_to(outputIterator, "TTDP savegame, {} format",
								lc.oldver.type == SGT_TTDP1 ? "old" : "new");
							if (lc.oldver.version != 0) {
								fmt::format_to(outputIterator, ", TTDP version {}.{}.{}.{}",
									GB(lc.oldver.version, 24, 8), GB(lc.oldver.version, 20, 4),
									GB(lc.oldver.version, 16, 4), GB(lc.oldver.version, 0, 16));
							}
							break;
					}
					break;

				case GLCT_SETTING:
					/* A setting with the SF_NO_NETWORK flag got changed; these settings usually affect NewGRFs, such as road side or wagon speed limits. */
					fmt::format_to(outputIterator, "Setting changed: {} : {} -> {}", lc.setting.name, lc.setting.oldval, lc.setting.newval);
					break;

				case GLCT_GRFADD: {
					/* A NewGRF got added to the game, either at the start of the game (never an issue), or later on when it could be an issue. */
					const GRFConfig *gc = FindGRFConfig(lc.grfadd.grfid, FGCM_EXACT, lc.grfadd.md5sum);
					message += "Added NewGRF: ";
					AddGrfInfo(outputIterator, lc.grfadd.grfid, lc.grfadd.md5sum, gc);
					GrfIDMapping::Pair *gm = grf_names.Find(lc.grfrem.grfid);
					if (gm != grf_names.End() && !gm->second.was_missing) message += ". Gamelog inconsistency: GrfID was already added!";
					grf_names[lc.grfadd.grfid] = gc;
					break;
				}

				case GLCT_GRFREM: {
					/* A NewGRF got removed from the game, either manually or by it missing when loading the game. */
					GrfIDMapping::Pair *gm = grf_names.Find(lc.grfrem.grfid);
					message += la.at == GLAT_LOAD ? "Missing NewGRF: " : "Removed NewGRF: ";
					AddGrfInfo(outputIterator, lc.grfrem.grfid, nullptr, gm != grf_names.End() ? gm->second.gc : nullptr);
					if (gm == grf_names.End()) {
						message += ". Gamelog inconsistency: GrfID was never added!";
					} else {
						if (la.at == GLAT_LOAD) {
							/* Missing grfs on load are not removed from the configuration */
							gm->second.was_missing = true;
						} else {
							grf_names.Erase(gm);
						}
					}
					break;
				}

				case GLCT_GRFCOMPAT: {
					/* Another version of the same NewGRF got loaded. */
					const GRFConfig *gc = FindGRFConfig(lc.grfadd.grfid, FGCM_EXACT, lc.grfadd.md5sum);
					message += "Compatible NewGRF loaded: ";
					AddGrfInfo(outputIterator, lc.grfcompat.grfid, lc.grfcompat.md5sum, gc);
					if (!grf_names.Contains(lc.grfcompat.grfid)) message += ". Gamelog inconsistency: GrfID was never added!";
					grf_names[lc.grfcompat.grfid] = gc;
					break;
				}

				case GLCT_GRFPARAM: {
					/* A parameter of a NewGRF got changed after the game was started. */
					GrfIDMapping::Pair *gm = grf_names.Find(lc.grfrem.grfid);
					message += "GRF parameter changed: ";
					AddGrfInfo(outputIterator, lc.grfparam.grfid, nullptr, gm != grf_names.End() ? gm->second.gc : nullptr);
					if (gm == grf_names.End()) message += ". Gamelog inconsistency: GrfID was never added!";
					break;
				}

				case GLCT_GRFMOVE: {
					/* The order of NewGRFs got changed, which might cause some other NewGRFs to behave differently. */
					GrfIDMapping::Pair *gm = grf_names.Find(lc.grfrem.grfid);
					fmt::format_to(outputIterator, "GRF order changed: {:08X} moved {} places {}",
						BSWAP32(lc.grfmove.grfid), abs(lc.grfmove.offset), lc.grfmove.offset >= 0 ? "down" : "up" );
					AddGrfInfo(outputIterator, lc.grfmove.grfid, nullptr, gm != grf_names.End() ? gm->second.gc : nullptr);
					if (gm == grf_names.End()) message += ". Gamelog inconsistency: GrfID was never added!";
					break;
				}

				case GLCT_GRFBUG: {
					/* A specific bug in a NewGRF, that could cause wide spread problems, has been noted during the execution of the game. */
					GrfIDMapping::Pair *gm = grf_names.Find(lc.grfrem.grfid);
					assert(lc.grfbug.bug == GBUG_VEH_LENGTH);

					fmt::format_to(outputIterator, "Rail vehicle changes length outside a depot: GRF ID {:08X}, internal ID 0x{:X}", BSWAP32(lc.grfbug.grfid), (uint)lc.grfbug.data);
					AddGrfInfo(outputIterator, lc.grfbug.grfid, nullptr, gm != grf_names.End() ? gm->second.gc : nullptr);
					if (gm == grf_names.End()) message += ". Gamelog inconsistency: GrfID was never added!";
					break;
				}

				case GLCT_EMERGENCY:
					/* At one point the savegame was made during the handling of a game crash.
					 * The generic code already mentioned the emergency savegame, and there is no extra information to log. */
					break;
			}

			proc(message);
		}
	}

	proc("---- gamelog end ----");
}


/** Print the gamelog data to the console. */
void Gamelog::PrintConsole()
{
	this->Print([](const std::string &s) {
		IConsolePrint(CC_WARNING, s);
	});
}

/**
 * Prints gamelog to debug output. Code is executed even when
 * there will be no output. It is called very seldom, so it
 * doesn't matter that much. At least it gives more uniform code...
 * @param level debug level we need to print stuff
 */
void Gamelog::PrintDebug(int level)
{
	this->Print([level](const std::string &s) {
		Debug(gamelog, level, "{}", s);
	});
}


/**
 * Allocates new LoggedChange and new LoggedAction if needed.
 * If there is no action active, nullptr is returned.
 * @param ct type of change
 * @return new LoggedChange, or nullptr if there is no action active
 */
LoggedChange *Gamelog::Change(GamelogChangeType ct)
{
	if (this->current_action == nullptr) {
		if (this->action_type == GLAT_NONE) return nullptr;

		this->current_action = &this->data->action.emplace_back();
		this->current_action->at = this->action_type;
		this->current_action->tick = TimerGameTick::counter;
	}

	LoggedChange *lc = &this->current_action->change.emplace_back();
	lc->ct = ct;

	return lc;
}


/**
 * Logs a emergency savegame
 */
void Gamelog::Emergency()
{
	/* Terminate any active action */
	if (this->action_type != GLAT_NONE) this->StopAction();
	this->StartAction(GLAT_EMERGENCY);
	this->Change(GLCT_EMERGENCY);
	this->StopAction();
}

/**
 * Finds out if current game is a loaded emergency savegame.
 */
bool Gamelog::TestEmergency()
{
	for (const LoggedAction &la : this->data->action) {
		for (const LoggedChange &lc : la.change) {
			if (lc.ct == GLCT_EMERGENCY) return true;
		}
	}

	return false;
}

/**
 * Logs a change in game revision
 */
void Gamelog::Revision()
{
	assert(this->action_type == GLAT_START || this->action_type == GLAT_LOAD);

	LoggedChange *lc = this->Change(GLCT_REVISION);
	if (lc == nullptr) return;

	memset(lc->revision.text, 0, sizeof(lc->revision.text));
	strecpy(lc->revision.text, GetGamelogRevisionString(), lastof(lc->revision.text));
	lc->revision.slver = SAVEGAME_VERSION;
	lc->revision.modified = _openttd_revision_modified;
	lc->revision.newgrf = _openttd_newgrf_version;
}

/**
 * Logs a change in game mode (scenario editor or game)
 */
void Gamelog::Mode()
{
	assert(this->action_type == GLAT_START || this->action_type == GLAT_LOAD || this->action_type == GLAT_CHEAT);

	LoggedChange *lc = this->Change(GLCT_MODE);
	if (lc == nullptr) return;

	lc->mode.mode      = _game_mode;
	lc->mode.landscape = _settings_game.game_creation.landscape;
}

/**
 * Logs loading from savegame without gamelog
 */
void Gamelog::Oldver()
{
	assert(this->action_type == GLAT_LOAD);

	LoggedChange *lc = this->Change(GLCT_OLDVER);
	if (lc == nullptr) return;

	lc->oldver.type = _savegame_type;
	lc->oldver.version = (_savegame_type == SGT_OTTD ? ((uint32)_sl_version << 8 | _sl_minor_version) : _ttdp_version);
}

/**
 * Logs change in game settings. Only non-networksafe settings are logged
 * @param name setting name
 * @param oldval old setting value
 * @param newval new setting value
 */
void Gamelog::Setting(const std::string &name, int32 oldval, int32 newval)
{
	assert(this->action_type == GLAT_SETTING);

	LoggedChange *lc = this->Change(GLCT_SETTING);
	if (lc == nullptr) return;

	lc->setting.name = stredup(name.c_str());
	lc->setting.oldval = oldval;
	lc->setting.newval = newval;
}


/**
 * Finds out if current revision is different than last revision stored in the savegame.
 * Appends GLCT_REVISION when the revision string changed
 */
void Gamelog::TestRevision()
{
	const LoggedChange *rev = nullptr;

	for (const LoggedAction &la : this->data->action) {
		for (const LoggedChange &lc : la.change) {
			if (lc.ct == GLCT_REVISION) rev = &lc;
		}
	}

	if (rev == nullptr || strcmp(rev->revision.text, GetGamelogRevisionString()) != 0 ||
			rev->revision.modified != _openttd_revision_modified ||
			rev->revision.newgrf != _openttd_newgrf_version) {
		this->Revision();
	}
}

/**
 * Finds last stored game mode or landscape.
 * Any change is logged
 */
void Gamelog::TestMode()
{
	const LoggedChange *mode = nullptr;

	for (const LoggedAction &la : this->data->action) {
		for (const LoggedChange &lc : la.change) {
			if (lc.ct == GLCT_MODE) mode = &lc;
		}
	}

	if (mode == nullptr || mode->mode.mode != _game_mode || mode->mode.landscape != _settings_game.game_creation.landscape) this->Mode();
}


/**
 * Logs triggered GRF bug.
 * @param grfid ID of problematic GRF
 * @param bug type of bug, @see enum GRFBugs
 * @param data additional data
 */
void Gamelog::GRFBug(uint32 grfid, byte bug, uint64 data)
{
	assert(this->action_type == GLAT_GRFBUG);

	LoggedChange *lc = this->Change(GLCT_GRFBUG);
	if (lc == nullptr) return;

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
bool Gamelog::GRFBugReverse(uint32 grfid, uint16 internal_id)
{
	for (const LoggedAction &la : this->data->action) {
		for (const LoggedChange &lc : la.change) {
			if (lc.ct == GLCT_GRFBUG && lc.grfbug.grfid == grfid &&
					lc.grfbug.bug == GBUG_VEH_LENGTH && lc.grfbug.data == internal_id) {
				return false;
			}
		}
	}

	this->StartAction(GLAT_GRFBUG);
	this->GRFBug(grfid, GBUG_VEH_LENGTH, internal_id);
	this->StopAction();

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
void Gamelog::GRFRemove(uint32 grfid)
{
	assert(this->action_type == GLAT_LOAD || this->action_type == GLAT_GRF);

	LoggedChange *lc = this->Change(GLCT_GRFREM);
	if (lc == nullptr) return;

	lc->grfrem.grfid = grfid;
}

/**
 * Logs adding of a GRF
 * @param newg added GRF
 */
void Gamelog::GRFAdd(const GRFConfig *newg)
{
	assert(this->action_type == GLAT_LOAD || this->action_type == GLAT_START || this->action_type == GLAT_GRF);

	if (!IsLoggableGrfConfig(newg)) return;

	LoggedChange *lc = this->Change(GLCT_GRFADD);
	if (lc == nullptr) return;

	lc->grfadd = newg->ident;
}

/**
 * Logs loading compatible GRF
 * (the same ID, but different MD5 hash)
 * @param newg new (updated) GRF
 */
void Gamelog::GRFCompatible(const GRFIdentifier *newg)
{
	assert(this->action_type == GLAT_LOAD || this->action_type == GLAT_GRF);

	LoggedChange *lc = this->Change(GLCT_GRFCOMPAT);
	if (lc == nullptr) return;

	lc->grfcompat = *newg;
}

/**
 * Logs changing GRF order
 * @param grfid GRF that is moved
 * @param offset how far it is moved, positive = moved down
 */
void Gamelog::GRFMove(uint32 grfid, int32 offset)
{
	assert(this->action_type == GLAT_GRF);

	LoggedChange *lc = this->Change(GLCT_GRFMOVE);
	if (lc == nullptr) return;

	lc->grfmove.grfid  = grfid;
	lc->grfmove.offset = offset;
}

/**
 * Logs change in GRF parameters.
 * Details about parameters changed are not stored
 * @param grfid ID of GRF to store
 */
void Gamelog::GRFParameters(uint32 grfid)
{
	assert(this->action_type == GLAT_GRF);

	LoggedChange *lc = this->Change(GLCT_GRFPARAM);
	if (lc == nullptr) return;

	lc->grfparam.grfid = grfid;
}

/**
 * Logs adding of list of GRFs.
 * Useful when old savegame is loaded or when new game is started
 * @param newg head of GRF linked list
 */
void Gamelog::GRFAddList(const GRFConfig *newg)
{
	assert(this->action_type == GLAT_START || this->action_type == GLAT_LOAD);

	for (; newg != nullptr; newg = newg->next) {
		this->GRFAdd(newg);
	}
}

/**
 * Generates GRFList
 * @param grfc head of GRF linked list
 */
static std::vector<const GRFConfig *> GenerateGRFList(const GRFConfig *grfc)
{
	std::vector<const GRFConfig *> list;
	for (const GRFConfig *g = grfc; g != nullptr; g = g->next) {
		if (IsLoggableGrfConfig(g)) list.push_back(g);
	}

	return list;
}

/**
 * Compares two NewGRF lists and logs any change
 * @param oldc original GRF list
 * @param newc new GRF list
 */
void Gamelog::GRFUpdate(const GRFConfig *oldc, const GRFConfig *newc)
{
	std::vector<const GRFConfig *> ol = GenerateGRFList(oldc);
	std::vector<const GRFConfig *> nl = GenerateGRFList(newc);

	uint o = 0, n = 0;

	while (o < ol.size() && n < nl.size()) {
		const GRFConfig *og = ol[o];
		const GRFConfig *ng = nl[n];

		if (og->ident.grfid != ng->ident.grfid) {
			uint oi, ni;
			for (oi = 0; oi < ol.size(); oi++) {
				if (ol[oi]->ident.grfid == nl[n]->ident.grfid) break;
			}
			if (oi < o) {
				/* GRF was moved, this change has been logged already */
				n++;
				continue;
			}
			if (oi == ol.size()) {
				/* GRF couldn't be found in the OLD list, GRF was ADDED */
				this->GRFAdd(nl[n++]);
				continue;
			}
			for (ni = 0; ni < nl.size(); ni++) {
				if (nl[ni]->ident.grfid == ol[o]->ident.grfid) break;
			}
			if (ni < n) {
				/* GRF was moved, this change has been logged already */
				o++;
				continue;
			}
			if (ni == nl.size()) {
				/* GRF couldn't be found in the NEW list, GRF was REMOVED */
				this->GRFRemove(ol[o++]->ident.grfid);
				continue;
			}

			/* o < oi < ol->n
			 * n < ni < nl->n */
			assert(ni > n && ni < nl.size());
			assert(oi > o && oi < ol.size());

			ni -= n; // number of GRFs it was moved downwards
			oi -= o; // number of GRFs it was moved upwards

			if (ni >= oi) { // prefer the one that is moved further
				/* GRF was moved down */
				this->GRFMove(ol[o++]->ident.grfid, ni);
			} else {
				this->GRFMove(nl[n++]->ident.grfid, -(int)oi);
			}
		} else {
			if (memcmp(og->ident.md5sum, ng->ident.md5sum, sizeof(og->ident.md5sum)) != 0) {
				/* md5sum changed, probably loading 'compatible' GRF */
				this->GRFCompatible(&nl[n]->ident);
			}

			if (og->num_params != ng->num_params || memcmp(og->param, ng->param, og->num_params * sizeof(og->param[0])) != 0) {
				this->GRFParameters(ol[o]->ident.grfid);
			}

			o++;
			n++;
		}
	}

	while (o < ol.size()) this->GRFRemove(ol[o++]->ident.grfid); // remaining GRFs were removed ...
	while (n < nl.size()) this->GRFAdd   (nl[n++]);              // ... or added
}

/**
 * Get some basic information from the given gamelog.
 * @param[out] last_ottd_rev OpenTTD NewGRF version from the binary that saved the savegame last.
 * @param[out] ever_modified Max value of 'modified' from all binaries that ever saved this savegame.
 * @param[out] removed_newgrfs Set to true if any NewGRFs have been removed.
 */
void Gamelog::Info(uint32 *last_ottd_rev, byte *ever_modified, bool *removed_newgrfs)
{
	for (const LoggedAction &la : this->data->action) {
		for (const LoggedChange &lc : la.change) {
			switch (lc.ct) {
				default: break;

				case GLCT_REVISION:
					*last_ottd_rev = lc.revision.newgrf;
					*ever_modified = std::max(*ever_modified, lc.revision.modified);
					break;

				case GLCT_GRFREM:
					*removed_newgrfs = true;
					break;
			}
		}
	}
}

/**
 * Try to find the overridden GRF identifier of the given GRF.
 * @param c the GRF to get the 'previous' version of.
 * @return the GRF identifier or \a c if none could be found.
 */
const GRFIdentifier *Gamelog::GetOverriddenIdentifier(const GRFConfig *c)
{
	assert(c != nullptr);
	const LoggedAction &la = this->data->action.back();
	if (la.at != GLAT_LOAD) return &c->ident;

	for (const LoggedChange &lc : la.change) {
		if (lc.ct == GLCT_GRFCOMPAT && lc.grfcompat.grfid == c->ident.grfid) return &lc.grfcompat;
	}

	return &c->ident;
}
