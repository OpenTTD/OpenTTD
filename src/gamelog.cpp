/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gamelog.cpp Definition of functions used for logging of fundamental changes to the game */

#include "stdafx.h"
#include "saveload/saveload.h"
#include "string_func.h"
#include "settings_type.h"
#include "gamelog_internal.h"
#include "console_func.h"
#include "debug.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_tick.h"
#include "rev.h"

#include "safeguards.h"

extern const SaveLoadVersion SAVEGAME_VERSION;  ///< current savegame version

extern SavegameType _savegame_type; ///< type of savegame we are loading

extern uint32_t _ttdp_version;        ///< version of TTDP savegame (if applicable)
extern SaveLoadVersion _sl_version; ///< the major savegame version identifier
extern byte   _sl_minor_version;    ///< the minor savegame version, DO NOT USE!

Gamelog _gamelog; ///< Gamelog instance

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
 */
static std::string GetGamelogRevisionString()
{
	if (IsReleasedVersion()) {
		return _openttd_revision;
	}

	/* Prefix character indication revision status */
	assert(_openttd_revision_modified < 3);
	return fmt::format("{}{}",
			"gum"[_openttd_revision_modified], // g = "git", u = "unknown", m = "modified"
			_openttd_revision_hash);
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
 * @param output_iterator The iterator to add the GRF info to.
 * @param last The end of the buffer
 * @param grfid GRF ID
 * @param md5sum array of md5sum to print, if known
 * @param gc GrfConfig, if known
 */
static void AddGrfInfo(std::back_insert_iterator<std::string> &output_iterator, uint32_t grfid, const MD5Hash *md5sum, const GRFConfig *gc)
{
	if (md5sum != nullptr) {
		fmt::format_to(output_iterator, "GRF ID {:08X}, checksum {}", BSWAP32(grfid), FormatArrayAsHex(*md5sum));
	} else {
		fmt::format_to(output_iterator, "GRF ID {:08X}", BSWAP32(grfid));
	}

	if (gc != nullptr) {
		fmt::format_to(output_iterator, ", filename: {} (md5sum matches)", gc->filename);
	} else {
		gc = FindGRFConfig(grfid, FGCM_ANY);
		if (gc != nullptr) {
			fmt::format_to(output_iterator, ", filename: {} (matches GRFID only)", gc->filename);
		} else {
			fmt::format_to(output_iterator, ", unknown GRF");
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
 * Prints active gamelog
 * @param proc the procedure to draw with
 */
void Gamelog::Print(std::function<void(const std::string &)> proc)
{
	GrfIDMapping grf_names;

	proc("---- gamelog start ----");

	for (const LoggedAction &la : this->data->action) {
		assert(la.at < GLAT_END);

		proc(fmt::format("Tick {}: {}", la.tick, la_text[la.at]));

		for (auto &lc : la.change) {
			std::string message;
			auto output_iterator = std::back_inserter(message);
			lc->FormatTo(output_iterator, grf_names, la.at);

			proc(message);
		}
	}

	proc("---- gamelog end ----");
}


/* virtual */ void LoggedChangeMode::FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &, GamelogActionType)
{
	/* Changing landscape, or going from scenario editor to game or back. */
	fmt::format_to(output_iterator, "New game mode: {} landscape: {}", this->mode, this->landscape);
}

/* virtual */ void LoggedChangeRevision::FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &, GamelogActionType)
{
	/* The game was loaded in a diffferent version than before. */
	fmt::format_to(output_iterator, "Revision text changed to {}, savegame version {}, ",
		this->text, this->slver);

	switch (this->modified) {
		case 0: fmt::format_to(output_iterator, "not "); break;
		case 1: fmt::format_to(output_iterator, "maybe "); break;
		default: break;
	}

	fmt::format_to(output_iterator, "modified, _openttd_newgrf_version = 0x{:08x}", this->newgrf);
}

/* virtual */ void LoggedChangeOldVersion::FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &, GamelogActionType)
{
	/* The game was loaded from before 0.7.0-beta1. */
	fmt::format_to(output_iterator, "Conversion from ");
	switch (this->type) {
		default: NOT_REACHED();
		case SGT_OTTD:
			fmt::format_to(output_iterator, "OTTD savegame without gamelog: version {}, {}",
				GB(this->version, 8, 16), GB(this->version, 0, 8));
			break;

		case SGT_TTO:
			fmt::format_to(output_iterator, "TTO savegame");
			break;

		case SGT_TTD:
			fmt::format_to(output_iterator, "TTD savegame");
			break;

		case SGT_TTDP1:
		case SGT_TTDP2:
			fmt::format_to(output_iterator, "TTDP savegame, {} format",
				this->type == SGT_TTDP1 ? "old" : "new");
			if (this->version != 0) {
				fmt::format_to(output_iterator, ", TTDP version {}.{}.{}.{}",
					GB(this->version, 24, 8), GB(this->version, 20, 4),
					GB(this->version, 16, 4), GB(this->version, 0, 16));
			}
			break;
	}
}

/* virtual */ void LoggedChangeSettingChanged::FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &, GamelogActionType)
{
	/* A setting with the SF_NO_NETWORK flag got changed; these settings usually affect NewGRFs, such as road side or wagon speed limits. */
	fmt::format_to(output_iterator, "Setting changed: {} : {} -> {}", this->name, this->oldval, this->newval);
}

/* virtual */ void LoggedChangeGRFAdd::FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType)
{
	/* A NewGRF got added to the game, either at the start of the game (never an issue), or later on when it could be an issue. */
	const GRFConfig *gc = FindGRFConfig(this->grfid, FGCM_EXACT, &this->md5sum);
	fmt::format_to(output_iterator, "Added NewGRF: ");
	AddGrfInfo(output_iterator, this->grfid, &this->md5sum, gc);
	auto gm = grf_names.find(this->grfid);
	if (gm != grf_names.end() && !gm->second.was_missing) fmt::format_to(output_iterator, ". Gamelog inconsistency: GrfID was already added!");
	grf_names[this->grfid] = gc;
}

/* virtual */ void LoggedChangeGRFRemoved::FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType action_type)
{
	/* A NewGRF got removed from the game, either manually or by it missing when loading the game. */
	auto gm = grf_names.find(this->grfid);
	fmt::format_to(output_iterator, action_type == GLAT_LOAD ? "Missing NewGRF: " : "Removed NewGRF: ");
	AddGrfInfo(output_iterator, this->grfid, nullptr, gm != grf_names.end() ? gm->second.gc : nullptr);
	if (gm == grf_names.end()) {
		fmt::format_to(output_iterator, ". Gamelog inconsistency: GrfID was never added!");
	} else {
		if (action_type == GLAT_LOAD) {
			/* Missing grfs on load are not removed from the configuration */
			gm->second.was_missing = true;
		} else {
			grf_names.erase(gm);
		}
	}
}

/* virtual */ void LoggedChangeGRFChanged::FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType)
{
	/* Another version of the same NewGRF got loaded. */
	const GRFConfig *gc = FindGRFConfig(this->grfid, FGCM_EXACT, &this->md5sum);
	fmt::format_to(output_iterator, "Compatible NewGRF loaded: ");
	AddGrfInfo(output_iterator, this->grfid, &this->md5sum, gc);
	if (grf_names.count(this->grfid) == 0) fmt::format_to(output_iterator, ". Gamelog inconsistency: GrfID was never added!");
	grf_names[this->grfid] = gc;
}

/* virtual */ void LoggedChangeGRFParameterChanged::FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType)
{
	/* A parameter of a NewGRF got changed after the game was started. */
	auto gm = grf_names.find(this->grfid);
	fmt::format_to(output_iterator, "GRF parameter changed: ");
	AddGrfInfo(output_iterator, this->grfid, nullptr, gm != grf_names.end() ? gm->second.gc : nullptr);
	if (gm == grf_names.end()) fmt::format_to(output_iterator, ". Gamelog inconsistency: GrfID was never added!");
}

/* virtual */ void LoggedChangeGRFMoved::FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType)
{
	/* The order of NewGRFs got changed, which might cause some other NewGRFs to behave differently. */
	auto gm = grf_names.find(this->grfid);
	fmt::format_to(output_iterator, "GRF order changed: {:08X} moved {} places {}",
		BSWAP32(this->grfid), abs(this->offset), this->offset >= 0 ? "down" : "up" );
	AddGrfInfo(output_iterator, this->grfid, nullptr, gm != grf_names.end() ? gm->second.gc : nullptr);
	if (gm == grf_names.end()) fmt::format_to(output_iterator, ". Gamelog inconsistency: GrfID was never added!");
}

/* virtual */ void LoggedChangeGRFBug::FormatTo(std::back_insert_iterator<std::string> &output_iterator, GrfIDMapping &grf_names, GamelogActionType)
{
	/* A specific bug in a NewGRF, that could cause wide spread problems, has been noted during the execution of the game. */
	auto gm = grf_names.find(this->grfid);
	assert(this->bug == GBUG_VEH_LENGTH);

	fmt::format_to(output_iterator, "Rail vehicle changes length outside a depot: GRF ID {:08X}, internal ID 0x{:X}", BSWAP32(this->grfid), this->data);
	AddGrfInfo(output_iterator, this->grfid, nullptr, gm != grf_names.end() ? gm->second.gc : nullptr);
	if (gm == grf_names.end()) fmt::format_to(output_iterator, ". Gamelog inconsistency: GrfID was never added!");
}

/* virtual */ void LoggedChangeEmergencySave::FormatTo(std::back_insert_iterator<std::string> &, GrfIDMapping &, GamelogActionType)
{
	/* At one point the savegame was made during the handling of a game crash.
	 * The generic code already mentioned the emergency savegame, and there is no extra information to log. */
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
 * Allocates a new LoggedAction if needed, and add the change when action is active.
 * @param change The actual change.
 */
void Gamelog::Change(std::unique_ptr<LoggedChange> &&change)
{
	if (this->current_action == nullptr) {
		if (this->action_type == GLAT_NONE) return;

		this->current_action = &this->data->action.emplace_back();
		this->current_action->at = this->action_type;
		this->current_action->tick = TimerGameTick::counter;
	}

	this->current_action->change.push_back(std::move(change));
}


/**
 * Logs a emergency savegame
 */
void Gamelog::Emergency()
{
	/* Terminate any active action */
	if (this->action_type != GLAT_NONE) this->StopAction();
	this->StartAction(GLAT_EMERGENCY);
	this->Change(std::make_unique<LoggedChangeEmergencySave>());
	this->StopAction();
}

/**
 * Finds out if current game is a loaded emergency savegame.
 */
bool Gamelog::TestEmergency()
{
	for (const LoggedAction &la : this->data->action) {
		for (const auto &lc : la.change) {
			if (lc->ct == GLCT_EMERGENCY) return true;
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

	this->Change(std::make_unique<LoggedChangeRevision>(
		GetGamelogRevisionString(), _openttd_newgrf_version, SAVEGAME_VERSION, _openttd_revision_modified));
}

/**
 * Logs a change in game mode (scenario editor or game)
 */
void Gamelog::Mode()
{
	assert(this->action_type == GLAT_START || this->action_type == GLAT_LOAD || this->action_type == GLAT_CHEAT);

	this->Change(std::make_unique<LoggedChangeMode>(_game_mode, _settings_game.game_creation.landscape));
}

/**
 * Logs loading from savegame without gamelog
 */
void Gamelog::Oldver()
{
	assert(this->action_type == GLAT_LOAD);

	this->Change(std::make_unique<LoggedChangeOldVersion>(_savegame_type,
		(_savegame_type == SGT_OTTD ? ((uint32_t)_sl_version << 8 | _sl_minor_version) : _ttdp_version)));
}

/**
 * Logs change in game settings. Only non-networksafe settings are logged
 * @param name setting name
 * @param oldval old setting value
 * @param newval new setting value
 */
void Gamelog::Setting(const std::string &name, int32_t oldval, int32_t newval)
{
	assert(this->action_type == GLAT_SETTING);

	this->Change(std::make_unique<LoggedChangeSettingChanged>(name, oldval, newval));
}


/**
 * Finds out if current revision is different than last revision stored in the savegame.
 * Appends GLCT_REVISION when the revision string changed
 */
void Gamelog::TestRevision()
{
	const LoggedChangeRevision *rev = nullptr;

	for (const LoggedAction &la : this->data->action) {
		for (const auto &lc : la.change) {
			if (lc->ct == GLCT_REVISION) rev = static_cast<const LoggedChangeRevision *>(lc.get());
		}
	}

	if (rev == nullptr || rev->text != GetGamelogRevisionString() ||
			rev->modified != _openttd_revision_modified ||
			rev->newgrf != _openttd_newgrf_version) {
		this->Revision();
	}
}

/**
 * Finds last stored game mode or landscape.
 * Any change is logged
 */
void Gamelog::TestMode()
{
	const LoggedChangeMode *mode = nullptr;

	for (const LoggedAction &la : this->data->action) {
		for (const auto &lc : la.change) {
			if (lc->ct == GLCT_MODE) mode = static_cast<const LoggedChangeMode *>(lc.get());
		}
	}

	if (mode == nullptr || mode->mode != _game_mode || mode->landscape != _settings_game.game_creation.landscape) this->Mode();
}


/**
 * Logs triggered GRF bug.
 * @param grfid ID of problematic GRF
 * @param bug type of bug, @see enum GRFBugs
 * @param data additional data
 */
void Gamelog::GRFBug(uint32_t grfid, byte bug, uint64_t data)
{
	assert(this->action_type == GLAT_GRFBUG);

	this->Change(std::make_unique<LoggedChangeGRFBug>(data, grfid, bug));
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
bool Gamelog::GRFBugReverse(uint32_t grfid, uint16_t internal_id)
{
	for (const LoggedAction &la : this->data->action) {
		for (const auto &lc : la.change) {
			if (lc->ct == GLCT_GRFBUG) {
				LoggedChangeGRFBug *bug = static_cast<LoggedChangeGRFBug *>(lc.get());
				if (bug->grfid == grfid && bug->bug == GBUG_VEH_LENGTH && bug->data == internal_id) {
					return false;
				}
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
void Gamelog::GRFRemove(uint32_t grfid)
{
	assert(this->action_type == GLAT_LOAD || this->action_type == GLAT_GRF);

	this->Change(std::make_unique<LoggedChangeGRFRemoved>(grfid));
}

/**
 * Logs adding of a GRF
 * @param newg added GRF
 */
void Gamelog::GRFAdd(const GRFConfig *newg)
{
	assert(this->action_type == GLAT_LOAD || this->action_type == GLAT_START || this->action_type == GLAT_GRF);

	if (!IsLoggableGrfConfig(newg)) return;

	this->Change(std::make_unique<LoggedChangeGRFAdd>(newg->ident));
}

/**
 * Logs loading compatible GRF
 * (the same ID, but different MD5 hash)
 * @param newg new (updated) GRF
 */
void Gamelog::GRFCompatible(const GRFIdentifier *newg)
{
	assert(this->action_type == GLAT_LOAD || this->action_type == GLAT_GRF);

	this->Change(std::make_unique<LoggedChangeGRFChanged>(*newg));
}

/**
 * Logs changing GRF order
 * @param grfid GRF that is moved
 * @param offset how far it is moved, positive = moved down
 */
void Gamelog::GRFMove(uint32_t grfid, int32_t offset)
{
	assert(this->action_type == GLAT_GRF);

	this->Change(std::make_unique<LoggedChangeGRFMoved>(grfid, offset));
}

/**
 * Logs change in GRF parameters.
 * Details about parameters changed are not stored
 * @param grfid ID of GRF to store
 */
void Gamelog::GRFParameters(uint32_t grfid)
{
	assert(this->action_type == GLAT_GRF);

	this->Change(std::make_unique<LoggedChangeGRFParameterChanged>(grfid));
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
			if (og->ident.md5sum != ng->ident.md5sum) {
				/* md5sum changed, probably loading 'compatible' GRF */
				this->GRFCompatible(&nl[n]->ident);
			}

			if (og->num_params != ng->num_params || og->param == ng->param) {
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
void Gamelog::Info(uint32_t *last_ottd_rev, byte *ever_modified, bool *removed_newgrfs)
{
	for (const LoggedAction &la : this->data->action) {
		for (const auto &lc : la.change) {
			switch (lc->ct) {
				default: break;

				case GLCT_REVISION: {
					const LoggedChangeRevision *rev = static_cast<const LoggedChangeRevision *>(lc.get());
					*last_ottd_rev = rev->newgrf;
					*ever_modified = std::max(*ever_modified, rev->modified);
					break;
				}

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

	for (const auto &lc : la.change) {
		if (lc->ct != GLCT_GRFCOMPAT) continue;

		const LoggedChangeGRFChanged *grf = static_cast<const LoggedChangeGRFChanged *>(lc.get());
		if (grf->grfid == c->ident.grfid) return grf;
	}

	return &c->ident;
}
