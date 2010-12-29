/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file afterload.cpp Code updating data after game load */

#include "../stdafx.h"
#include "../void_map.h"
#include "../signs_base.h"
#include "../depot_base.h"
#include "../window_func.h"
#include "../fios.h"
#include "../gamelog_internal.h"
#include "../network/network.h"
#include "../gfxinit.h"
#include "../functions.h"
#include "../industry.h"
#include "../clear_map.h"
#include "../vehicle_func.h"
#include "../string_func.h"
#include "../date_func.h"
#include "../roadveh.h"
#include "../train.h"
#include "../station_base.h"
#include "../waypoint_base.h"
#include "../roadstop_base.h"
#include "../tunnelbridge_map.h"
#include "../pathfinder/yapf/yapf_cache.h"
#include "../elrail_func.h"
#include "../signs_func.h"
#include "../aircraft.h"
#include "../object_map.h"
#include "../object_base.h"
#include "../tree_map.h"
#include "../company_func.h"
#include "../road_cmd.h"
#include "../ai/ai.hpp"
#include "../ai/ai_gui.hpp"
#include "../town.h"
#include "../economy_base.h"
#include "../animated_tile_func.h"
#include "../subsidy_base.h"
#include "../subsidy_func.h"
#include "../newgrf.h"
#include "../engine_func.h"
#include "../rail_gui.h"
#include "../core/backup_type.hpp"

#include "table/strings.h"

#include "saveload_internal.h"

#include <signal.h>

extern StringID _switch_mode_errorstr;
extern Company *DoStartupNewCompany(bool is_ai, CompanyID company = INVALID_COMPANY);

/**
 * Makes a tile canal or water depending on the surroundings.
 *
 * Must only be used for converting old savegames. Use WaterClass now.
 *
 * This as for example docks and shipdepots do not store
 * whether the tile used to be canal or 'normal' water.
 * @param t the tile to change.
 * @param include_invalid_water_class Also consider WATER_CLASS_INVALID, i.e. industry tiles on land
 */
void SetWaterClassDependingOnSurroundings(TileIndex t, bool include_invalid_water_class)
{
	/* If the slope is not flat, we always assume 'land' (if allowed). Also for one-corner-raised-shores.
	 * Note: Wrt. autosloping under industry tiles this is the most fool-proof behaviour. */
	if (GetTileSlope(t, NULL) != SLOPE_FLAT) {
		if (include_invalid_water_class) {
			SetWaterClass(t, WATER_CLASS_INVALID);
			return;
		} else {
			SlErrorCorrupt("Invalid water class for dry tile");
		}
	}

	/* Mark tile dirty in all cases */
	MarkTileDirtyByTile(t);

	if (TileX(t) == 0 || TileY(t) == 0 || TileX(t) == MapMaxX() - 1 || TileY(t) == MapMaxY() - 1) {
		/* tiles at map borders are always WATER_CLASS_SEA */
		SetWaterClass(t, WATER_CLASS_SEA);
		return;
	}

	bool has_water = false;
	bool has_canal = false;
	bool has_river = false;

	for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
		TileIndex neighbour = TileAddByDiagDir(t, dir);
		switch (GetTileType(neighbour)) {
			case MP_WATER:
				/* clear water and shipdepots have already a WaterClass associated */
				if (IsCoast(neighbour)) {
					has_water = true;
				} else if (!IsLock(neighbour)) {
					switch (GetWaterClass(neighbour)) {
						case WATER_CLASS_SEA:   has_water = true; break;
						case WATER_CLASS_CANAL: has_canal = true; break;
						case WATER_CLASS_RIVER: has_river = true; break;
						default: SlErrorCorrupt("Invalid water class for tile");
					}
				}
				break;

			case MP_RAILWAY:
				/* Shore or flooded halftile */
				has_water |= (GetRailGroundType(neighbour) == RAIL_GROUND_WATER);
				break;

			case MP_TREES:
				/* trees on shore */
				has_water |= (GB(_m[neighbour].m2, 4, 2) == TREE_GROUND_SHORE);
				break;

			default: break;
		}
	}

	if (!has_water && !has_canal && !has_river && include_invalid_water_class) {
		SetWaterClass(t, WATER_CLASS_INVALID);
		return;
	}

	if (has_river && !has_canal) {
		SetWaterClass(t, WATER_CLASS_RIVER);
	} else if (has_canal || !has_water) {
		SetWaterClass(t, WATER_CLASS_CANAL);
	} else {
		SetWaterClass(t, WATER_CLASS_SEA);
	}
}

static void ConvertTownOwner()
{
	for (TileIndex tile = 0; tile != MapSize(); tile++) {
		switch (GetTileType(tile)) {
			case MP_ROAD:
				if (GB(_m[tile].m5, 4, 2) == ROAD_TILE_CROSSING && HasBit(_m[tile].m3, 7)) {
					_m[tile].m3 = OWNER_TOWN;
				}
				/* FALL THROUGH */

			case MP_TUNNELBRIDGE:
				if (GetTileOwner(tile) & 0x80) SetTileOwner(tile, OWNER_TOWN);
				break;

			default: break;
		}
	}
}

/* since savegame version 4.1, exclusive transport rights are stored at towns */
static void UpdateExclusiveRights()
{
	Town *t;

	FOR_ALL_TOWNS(t) {
		t->exclusivity = INVALID_COMPANY;
	}

	/* FIXME old exclusive rights status is not being imported (stored in s->blocked_months_obsolete)
	 *   could be implemented this way:
	 * 1.) Go through all stations
	 *     Build an array town_blocked[ town_id ][ company_id ]
	 *     that stores if at least one station in that town is blocked for a company
	 * 2.) Go through that array, if you find a town that is not blocked for
	 *     one company, but for all others, then give him exclusivity.
	 */
}

static const byte convert_currency[] = {
	 0,  1, 12,  8,  3,
	10, 14, 19,  4,  5,
	 9, 11, 13,  6, 17,
	16, 22, 21,  7, 15,
	18,  2, 20,
};

/* since savegame version 4.2 the currencies are arranged differently */
static void UpdateCurrencies()
{
	_settings_game.locale.currency = convert_currency[_settings_game.locale.currency];
}

/* Up to revision 1413 the invisible tiles at the southern border have not been
 * MP_VOID, even though they should have. This is fixed by this function
 */
static void UpdateVoidTiles()
{
	uint i;

	for (i = 0; i < MapMaxY(); ++i) MakeVoid(i * MapSizeX() + MapMaxX());
	for (i = 0; i < MapSizeX(); ++i) MakeVoid(MapSizeX() * MapMaxY() + i);
}

static inline RailType UpdateRailType(RailType rt, RailType min)
{
	return rt >= min ? (RailType)(rt + 1): rt;
}

/**
 * Update the viewport coordinates of all signs.
 */
void UpdateAllVirtCoords()
{
	UpdateAllStationVirtCoords();
	UpdateAllSignVirtCoords();
	UpdateAllTownVirtCoords();
}

/**
 * Initialization of the windows and several kinds of caches.
 * This is not done directly in AfterLoadGame because these
 * functions require that all saveload conversions have been
 * done. As people tend to add savegame conversion stuff after
 * the intialization of the windows and caches quite some bugs
 * had been made.
 * Moving this out of there is both cleaner and less bug-prone.
 */
static void InitializeWindowsAndCaches()
{
	/* Initialize windows */
	ResetWindowSystem();
	SetupColoursAndInitialWindow();

	/* Update coordinates of the signs. */
	UpdateAllVirtCoords();
	ResetViewportAfterLoadGame();

	Company *c;
	FOR_ALL_COMPANIES(c) {
		/* For each company, verify (while loading a scenario) that the inauguration date is the current year and set it
		 * accordingly if it is not the case.  No need to set it on companies that are not been used already,
		 * thus the MIN_YEAR (which is really nothing more than Zero, initialized value) test */
		if (_file_to_saveload.filetype == FT_SCENARIO && c->inaugurated_year != MIN_YEAR) {
			c->inaugurated_year = _cur_year;
		}
	}

	RecomputePrices();

	SetCachedEngineCounts();

	Station::RecomputeIndustriesNearForAll();
	RebuildSubsidisedSourceAndDestinationCache();

	/* Towns have a noise controlled number of airports system
	 * So each airport's noise value must be added to the town->noise_reached value
	 * Reset each town's noise_reached value to '0' before. */
	UpdateAirportsNoise();

	CheckTrainsLengths();
	ShowNewGRFError();
	ShowAIDebugWindowIfAIError();
}

typedef void (CDECL *SignalHandlerPointer)(int);
static SignalHandlerPointer _prev_segfault = NULL;
static SignalHandlerPointer _prev_abort    = NULL;
static SignalHandlerPointer _prev_fpe      = NULL;

static void CDECL HandleSavegameLoadCrash(int signum);

/**
 * Replaces signal handlers of SIGSEGV and SIGABRT
 * and stores pointers to original handlers in memory.
 */
static void SetSignalHandlers()
{
	_prev_segfault = signal(SIGSEGV, HandleSavegameLoadCrash);
	_prev_abort    = signal(SIGABRT, HandleSavegameLoadCrash);
	_prev_fpe      = signal(SIGFPE,  HandleSavegameLoadCrash);
}

/**
 * Resets signal handlers back to original handlers.
 */
static void ResetSignalHandlers()
{
	signal(SIGSEGV, _prev_segfault);
	signal(SIGABRT, _prev_abort);
	signal(SIGFPE,  _prev_fpe);
}

/**
 * Try to find the overridden GRF identifier of the given GRF.
 * @param c the GRF to get the 'previous' version of.
 * @return the GRF identifier or \a c if none could be found.
 */
static const GRFIdentifier *GetOverriddenIdentifier(const GRFConfig *c)
{
	const LoggedAction *la = &_gamelog_action[_gamelog_actions - 1];
	if (la->at != GLAT_LOAD) return &c->ident;

	const LoggedChange *lcend = &la->change[la->changes];
	for (const LoggedChange *lc = la->change; lc != lcend; lc++) {
		if (lc->ct == GLCT_GRFCOMPAT && lc->grfcompat.grfid == c->ident.grfid) return &lc->grfcompat;
	}

	return &c->ident;
}

/** Was the saveload crash because of missing NewGRFs? */
static bool _saveload_crash_with_missing_newgrfs = false;

/**
 * Did loading the savegame cause a crash? If so,
 * were NewGRFs missing?
 * @return when the saveload crashed due to missing NewGRFs.
 */
bool SaveloadCrashWithMissingNewGRFs()
{
	return _saveload_crash_with_missing_newgrfs;
}

/**
 * Signal handler used to give a user a more useful report for crashes during
 * the savegame loading process; especially when there's problems with the
 * NewGRFs that are required by the savegame.
 * @param signum received signal
 */
static void CDECL HandleSavegameLoadCrash(int signum)
{
	ResetSignalHandlers();

	char buffer[8192];
	char *p = buffer;
	p += seprintf(p, lastof(buffer), "Loading your savegame caused OpenTTD to crash.\n");

	for (const GRFConfig *c = _grfconfig; !_saveload_crash_with_missing_newgrfs && c != NULL; c = c->next) {
		_saveload_crash_with_missing_newgrfs = HasBit(c->flags, GCF_COMPATIBLE) || c->status == GCS_NOT_FOUND;
	}

	if (_saveload_crash_with_missing_newgrfs) {
		p += seprintf(p, lastof(buffer),
			"This is most likely caused by a missing NewGRF or a NewGRF that\n"
			"has been loaded as replacement for a missing NewGRF. OpenTTD\n"
			"cannot easily determine whether a replacement NewGRF is of a newer\n"
			"or older version.\n"
			"It will load a NewGRF with the same GRF ID as the missing NewGRF.\n"
			"This means that if the author makes incompatible NewGRFs with the\n"
			"same GRF ID OpenTTD cannot magically do the right thing. In most\n"
			"cases OpenTTD will load the savegame and not crash, but this is an\n"
			"exception.\n"
			"Please load the savegame with the appropriate NewGRFs installed.\n"
			"The missing/compatible NewGRFs are:\n");

		for (const GRFConfig *c = _grfconfig; c != NULL; c = c->next) {
			if (HasBit(c->flags, GCF_COMPATIBLE)) {
				const GRFIdentifier *replaced = GetOverriddenIdentifier(c);
				char buf[40];
				md5sumToString(buf, lastof(buf), replaced->md5sum);
				p += seprintf(p, lastof(buffer), "NewGRF %08X (checksum %s) not found.\n  Loaded NewGRF \"%s\" with same GRF ID instead.\n", BSWAP32(c->ident.grfid), buf, c->filename);
			}
			if (c->status == GCS_NOT_FOUND) {
				char buf[40];
				md5sumToString(buf, lastof(buf), c->ident.md5sum);
				p += seprintf(p, lastof(buffer), "NewGRF %08X (%s) not found; checksum %s.\n", BSWAP32(c->ident.grfid), c->filename, buf);
			}
		}
	} else {
		p += seprintf(p, lastof(buffer),
			"This is probably caused by a corruption in the savegame.\n"
			"Please file a bug report and attach this savegame.\n");
	}

	ShowInfo(buffer);

	SignalHandlerPointer call = NULL;
	switch (signum) {
		case SIGSEGV: call = _prev_segfault; break;
		case SIGABRT: call = _prev_abort; break;
		case SIGFPE:  call = _prev_fpe; break;
		default: NOT_REACHED();
	}
	if (call != NULL) call(signum);
}

/**
 * Tries to change owner of this rail tile to a valid owner. In very old versions it could happen that
 * a rail track had an invalid owner. When conversion isn't possible, track is removed.
 * @param t tile to update
 */
static void FixOwnerOfRailTrack(TileIndex t)
{
	assert(!Company::IsValidID(GetTileOwner(t)) && (IsLevelCrossingTile(t) || IsPlainRailTile(t)));

	/* remove leftover rail piece from crossing (from very old savegames) */
	Train *v = NULL, *w;
	FOR_ALL_TRAINS(w) {
		if (w->tile == t) {
			v = w;
			break;
		}
	}

	if (v != NULL) {
		/* when there is a train on crossing (it could happen in TTD), set owner of crossing to train owner */
		SetTileOwner(t, v->owner);
		return;
	}

	/* try to find any connected rail */
	for (DiagDirection dd = DIAGDIR_BEGIN; dd < DIAGDIR_END; dd++) {
		TileIndex tt = t + TileOffsByDiagDir(dd);
		if (GetTileTrackStatus(t, TRANSPORT_RAIL, 0, dd) != 0 &&
				GetTileTrackStatus(tt, TRANSPORT_RAIL, 0, ReverseDiagDir(dd)) != 0 &&
				Company::IsValidID(GetTileOwner(tt))) {
			SetTileOwner(t, GetTileOwner(tt));
			return;
		}
	}

	if (IsLevelCrossingTile(t)) {
		/* else change the crossing to normal road (road vehicles won't care) */
		MakeRoadNormal(t, GetCrossingRoadBits(t), GetRoadTypes(t), GetTownIndex(t),
			GetRoadOwner(t, ROADTYPE_ROAD), GetRoadOwner(t, ROADTYPE_TRAM));
		return;
	}

	/* if it's not a crossing, make it clean land */
	MakeClear(t, CLEAR_GRASS, 0);
}

bool AfterLoadGame()
{
	SetSignalHandlers();

	TileIndex map_size = MapSize();

	if (IsSavegameVersionBefore(98)) GamelogOldver();

	GamelogTestRevision();
	GamelogTestMode();

	if (IsSavegameVersionBefore(98)) GamelogGRFAddList(_grfconfig);

	if (IsSavegameVersionBefore(119)) {
		_pause_mode = (_pause_mode == 2) ? PM_PAUSED_NORMAL : PM_UNPAUSED;
	} else if (_network_dedicated && (_pause_mode & PM_PAUSED_ERROR) != 0) {
		DEBUG(net, 0, "The loading savegame was paused due to an error state.");
		DEBUG(net, 0, "  The savegame cannot be used for multiplayer!");
		/* Restore the signals */
		ResetSignalHandlers();
		return false;
	} else if (!_networking || _network_server) {
		/* If we are in single player, i.e. not networking, and loading the
		 * savegame or we are loading the savegame as network server we do
		 * not want to be bothered by being paused because of the automatic
		 * reason of a network server, e.g. joining clients or too few
		 * active clients. Note that resetting these values for a network
		 * client are very bad because then the client is going to execute
		 * the game loop when the server is not, i.e. it desyncs. */
		_pause_mode &= ~PMB_PAUSED_NETWORK;
	}

	/* in very old versions, size of train stations was stored differently */
	if (IsSavegameVersionBefore(2)) {
		Station *st;
		FOR_ALL_STATIONS(st) {
			if (st->train_station.tile != 0 && st->train_station.h == 0) {
				uint n = _savegame_type == SGT_OTTD ? 4 : 3; // OTTD uses 4 bits per dimensions, TTD 3 bits
				uint w = GB(st->train_station.w, n, n);
				uint h = GB(st->train_station.w, 0, n);

				if (GetRailStationAxis(st->train_station.tile) != AXIS_X) Swap(w, h);

				st->train_station.w = w;
				st->train_station.h = h;

				assert(GetStationIndex(st->train_station.tile + TileDiffXY(w - 1, h - 1)) == st->index);
			}
		}
	}

	/* in version 2.1 of the savegame, town owner was unified. */
	if (IsSavegameVersionBefore(2, 1)) ConvertTownOwner();

	/* from version 4.1 of the savegame, exclusive rights are stored at towns */
	if (IsSavegameVersionBefore(4, 1)) UpdateExclusiveRights();

	/* from version 4.2 of the savegame, currencies are in a different order */
	if (IsSavegameVersionBefore(4, 2)) UpdateCurrencies();

	/* In old version there seems to be a problem that water is owned by
	 * OWNER_NONE, not OWNER_WATER.. I can't replicate it for the current
	 * (4.3) version, so I just check when versions are older, and then
	 * walk through the whole map.. */
	if (IsSavegameVersionBefore(4, 3)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_WATER) && GetTileOwner(t) >= MAX_COMPANIES) {
				SetTileOwner(t, OWNER_WATER);
			}
		}
	}

	if (IsSavegameVersionBefore(84)) {
		Company *c;
		FOR_ALL_COMPANIES(c) {
			c->name = CopyFromOldName(c->name_1);
			if (c->name != NULL) c->name_1 = STR_SV_UNNAMED;
			c->president_name = CopyFromOldName(c->president_name_1);
			if (c->president_name != NULL) c->president_name_1 = SPECSTR_PRESIDENT_NAME;
		}

		Station *st;
		FOR_ALL_STATIONS(st) {
			st->name = CopyFromOldName(st->string_id);
			/* generating new name would be too much work for little effect, use the station name fallback */
			if (st->name != NULL) st->string_id = STR_SV_STNAME_FALLBACK;
		}

		Town *t;
		FOR_ALL_TOWNS(t) {
			t->name = CopyFromOldName(t->townnametype);
			if (t->name != NULL) t->townnametype = SPECSTR_TOWNNAME_START + _settings_game.game_creation.town_name;
		}
	}

	/* From this point the old names array is cleared. */
	ResetOldNames();

	if (IsSavegameVersionBefore(106)) {
		/* no station is determined by 'tile == INVALID_TILE' now (instead of '0') */
		Station *st;
		FOR_ALL_STATIONS(st) {
			if (st->airport.tile       == 0) st->airport.tile = INVALID_TILE;
			if (st->dock_tile          == 0) st->dock_tile    = INVALID_TILE;
			if (st->train_station.tile == 0) st->train_station.tile   = INVALID_TILE;
		}

		/* the same applies to Company::location_of_HQ */
		Company *c;
		FOR_ALL_COMPANIES(c) {
			if (c->location_of_HQ == 0 || (IsSavegameVersionBefore(4) && c->location_of_HQ == 0xFFFF)) {
				c->location_of_HQ = INVALID_TILE;
			}
		}
	}

	/* convert road side to my format. */
	if (_settings_game.vehicle.road_side) _settings_game.vehicle.road_side = 1;

	/* Check if all NewGRFs are present, we are very strict in MP mode */
	GRFListCompatibility gcf_res = IsGoodGRFConfigList(_grfconfig);
	for (GRFConfig *c = _grfconfig; c != NULL; c = c->next) {
		if (c->status == GCS_NOT_FOUND) {
			GamelogGRFRemove(c->ident.grfid);
		} else if (HasBit(c->flags, GCF_COMPATIBLE)) {
			GamelogGRFCompatible(&c->ident);
		}
	}

	if (_networking && gcf_res != GLC_ALL_GOOD) {
		SetSaveLoadError(STR_NETWORK_ERROR_CLIENT_NEWGRF_MISMATCH);
		/* Restore the signals */
		ResetSignalHandlers();
		return false;
	}

	switch (gcf_res) {
		case GLC_COMPATIBLE: _switch_mode_errorstr = STR_NEWGRF_COMPATIBLE_LOAD_WARNING; break;
		case GLC_NOT_FOUND:  _switch_mode_errorstr = STR_NEWGRF_DISABLED_WARNING; _pause_mode = PM_PAUSED_ERROR; break;
		default: break;
	}

	/* The value of _date_fract got divided, so make sure that old games are converted correctly. */
	if (IsSavegameVersionBefore(11, 1) || (IsSavegameVersionBefore(147) && _date_fract > DAY_TICKS)) _date_fract /= 885;

	/* Update current year
	 * must be done before loading sprites as some newgrfs check it */
	SetDate(_date, _date_fract);

	/* Force dynamic engines off when loading older savegames */
	if (IsSavegameVersionBefore(95)) _settings_game.vehicle.dynamic_engines = 0;

	/* Load the sprites */
	GfxLoadSprites();
	LoadStringWidthTable();

	/* Copy temporary data to Engine pool */
	CopyTempEngineData();

	/* Connect front and rear engines of multiheaded trains and converts
	 * subtype to the new format */
	if (IsSavegameVersionBefore(17, 1)) ConvertOldMultiheadToNew();

	/* Connect front and rear engines of multiheaded trains */
	ConnectMultiheadedTrains();

	/* Fix the CargoPackets *and* fix the caches of CargoLists.
	 * If this isn't done before Stations and especially Vehicles are
	 * running their AfterLoad we might get in trouble. In the case of
	 * vehicles we could give the wrong (cached) count of items in a
	 * vehicle which causes different results when getting their caches
	 * filled; and that could eventually lead to desyncs. */
	CargoPacket::AfterLoad();

	/* Oilrig was moved from id 15 to 9. We have to do this conversion
	 * here as AfterLoadVehicles can check it indirectly via the newgrf
	 * code. */
	if (IsSavegameVersionBefore(139)) {
		Station *st;
		FOR_ALL_STATIONS(st) {
			if (st->airport.tile != INVALID_TILE && st->airport.type == 15) {
				st->airport.type = AT_OILRIG;
			}
		}
	}

	/* Update all vehicles */
	AfterLoadVehicles(true);

	/* Make sure there is an AI attached to an AI company */
	{
		Company *c;
		FOR_ALL_COMPANIES(c) {
			if (c->is_ai && c->ai_instance == NULL) AI::StartNew(c->index);
		}
	}

	/* make sure there is a town in the game */
	if (_game_mode == GM_NORMAL && !ClosestTownFromTile(0, UINT_MAX)) {
		SetSaveLoadError(STR_ERROR_NO_TOWN_IN_SCENARIO);
		/* Restore the signals */
		ResetSignalHandlers();
		return false;
	}

	/* The void tiles on the southern border used to belong to a wrong class (pre 4.3).
	 * This problem appears in savegame version 21 too, see r3455. But after loading the
	 * savegame and saving again, the buggy map array could be converted to new savegame
	 * version. It didn't show up before r12070. */
	if (IsSavegameVersionBefore(87)) UpdateVoidTiles();

	/* If Load Scenario / New (Scenario) Game is used,
	 *  a company does not exist yet. So create one here.
	 * 1 exeption: network-games. Those can have 0 companies
	 *   But this exeption is not true for non dedicated network_servers! */
	if (!Company::IsValidID(COMPANY_FIRST) && (!_networking || (_networking && _network_server && !_network_dedicated))) {
		DoStartupNewCompany(false);
	}

	/* Fix the cache for cargo payments. */
	CargoPayment *cp;
	FOR_ALL_CARGO_PAYMENTS(cp) {
		cp->front->cargo_payment = cp;
		cp->current_station = cp->front->last_station_visited;
	}

	if (IsSavegameVersionBefore(72)) {
		/* Locks in very old savegames had OWNER_WATER as owner */
		for (TileIndex t = 0; t < MapSize(); t++) {
			switch (GetTileType(t)) {
				default: break;

				case MP_WATER:
					if (GetWaterTileType(t) == WATER_TILE_LOCK && GetTileOwner(t) == OWNER_WATER) SetTileOwner(t, OWNER_NONE);
					break;

				case MP_STATION: {
					if (HasBit(_m[t].m6, 3)) SetBit(_m[t].m6, 2);
					StationGfx gfx = GetStationGfx(t);
					StationType st;
					if (       IsInsideMM(gfx,   0,   8)) { // Rail station
						st = STATION_RAIL;
						SetStationGfx(t, gfx - 0);
					} else if (IsInsideMM(gfx,   8,  67)) { // Airport
						st = STATION_AIRPORT;
						SetStationGfx(t, gfx - 8);
					} else if (IsInsideMM(gfx,  67,  71)) { // Truck
						st = STATION_TRUCK;
						SetStationGfx(t, gfx - 67);
					} else if (IsInsideMM(gfx,  71,  75)) { // Bus
						st = STATION_BUS;
						SetStationGfx(t, gfx - 71);
					} else if (gfx == 75) {                 // Oil rig
						st = STATION_OILRIG;
						SetStationGfx(t, gfx - 75);
					} else if (IsInsideMM(gfx,  76,  82)) { // Dock
						st = STATION_DOCK;
						SetStationGfx(t, gfx - 76);
					} else if (gfx == 82) {                 // Buoy
						st = STATION_BUOY;
						SetStationGfx(t, gfx - 82);
					} else if (IsInsideMM(gfx,  83, 168)) { // Extended airport
						st = STATION_AIRPORT;
						SetStationGfx(t, gfx - 83 + 67 - 8);
					} else if (IsInsideMM(gfx, 168, 170)) { // Drive through truck
						st = STATION_TRUCK;
						SetStationGfx(t, gfx - 168 + GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET);
					} else if (IsInsideMM(gfx, 170, 172)) { // Drive through bus
						st = STATION_BUS;
						SetStationGfx(t, gfx - 170 + GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET);
					} else {
						/* Restore the signals */
						ResetSignalHandlers();
						return false;
					}
					SB(_m[t].m6, 3, 3, st);
					break;
				}
			}
		}
	}

	for (TileIndex t = 0; t < map_size; t++) {
		switch (GetTileType(t)) {
			case MP_STATION: {
				BaseStation *bst = BaseStation::GetByTile(t);

				/* Set up station spread */
				bst->rect.BeforeAddTile(t, StationRect::ADD_FORCE);

				/* Waypoints don't have road stops/oil rigs in the old format */
				if (!Station::IsExpected(bst)) break;
				Station *st = Station::From(bst);

				switch (GetStationType(t)) {
					case STATION_TRUCK:
					case STATION_BUS:
						if (IsSavegameVersionBefore(6)) {
							/* From this version on there can be multiple road stops of the
							 * same type per station. Convert the existing stops to the new
							 * internal data structure. */
							RoadStop *rs = new RoadStop(t);
							if (rs == NULL) error("Too many road stops in savegame");

							RoadStop **head =
								IsTruckStop(t) ? &st->truck_stops : &st->bus_stops;
							*head = rs;
						}
						break;

					case STATION_OILRIG: {
						/* Very old savegames sometimes have phantom oil rigs, i.e.
						 * an oil rig which got shut down, but not completly removed from
						 * the map
						 */
						TileIndex t1 = TILE_ADDXY(t, 0, 1);
						if (IsTileType(t1, MP_INDUSTRY) &&
								GetIndustryGfx(t1) == GFX_OILRIG_1) {
							/* The internal encoding of oil rigs was changed twice.
							 * It was 3 (till 2.2) and later 5 (till 5.1).
							 * Setting it unconditionally does not hurt.
							 */
							Station::GetByTile(t)->airport.type = AT_OILRIG;
						} else {
							DeleteOilRig(t);
						}
						break;
					}

					default: break;
				}
				break;
			}

			default: break;
		}
	}

	/* In version 2.2 of the savegame, we have new airports, so status of all aircraft is reset.
	 * This has to be called after the oilrig airport_type update above ^^^ ! */
	if (IsSavegameVersionBefore(2, 2)) UpdateOldAircraft();

	/* In version 6.1 we put the town index in the map-array. To do this, we need
	 *  to use m2 (16bit big), so we need to clean m2, and that is where this is
	 *  all about ;) */
	if (IsSavegameVersionBefore(6, 1)) {
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_HOUSE:
					_m[t].m4 = _m[t].m2;
					SetTownIndex(t, CalcClosestTownFromTile(t)->index);
					break;

				case MP_ROAD:
					_m[t].m4 |= (_m[t].m2 << 4);
					if ((GB(_m[t].m5, 4, 2) == ROAD_TILE_CROSSING ? (Owner)_m[t].m3 : GetTileOwner(t)) == OWNER_TOWN) {
						SetTownIndex(t, CalcClosestTownFromTile(t)->index);
					} else {
						SetTownIndex(t, 0);
					}
					break;

				default: break;
			}
		}
	}

	/* Force the freeform edges to false for old savegames. */
	if (IsSavegameVersionBefore(111)) {
		_settings_game.construction.freeform_edges = false;
	}

	/* From version 9.0, we update the max passengers of a town (was sometimes negative
	 *  before that. */
	if (IsSavegameVersionBefore(9)) {
		Town *t;
		FOR_ALL_TOWNS(t) UpdateTownMaxPass(t);
	}

	/* From version 16.0, we included autorenew on engines, which are now saved, but
	 *  of course, we do need to initialize them for older savegames. */
	if (IsSavegameVersionBefore(16)) {
		Company *c;
		FOR_ALL_COMPANIES(c) {
			c->engine_renew_list            = NULL;
			c->settings.engine_renew        = false;
			c->settings.engine_renew_months = 6;
			c->settings.engine_renew_money  = 100000;
		}

		/* When loading a game, _local_company is not yet set to the correct value.
		 * However, in a dedicated server we are a spectator, so nothing needs to
		 * happen. In case we are not a dedicated server, the local company always
		 * becomes company 0, unless we are in the scenario editor where all the
		 * companies are 'invalid'.
		 */
		c = Company::GetIfValid(COMPANY_FIRST);
		if (!_network_dedicated && c != NULL) {
			c->settings = _settings_client.company;
		}
	}

	if (IsSavegameVersionBefore(48)) {
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					if (IsPlainRail(t)) {
						/* Swap ground type and signal type for plain rail tiles, so the
						 * ground type uses the same bits as for depots and waypoints. */
						uint tmp = GB(_m[t].m4, 0, 4);
						SB(_m[t].m4, 0, 4, GB(_m[t].m2, 0, 4));
						SB(_m[t].m2, 0, 4, tmp);
					} else if (HasBit(_m[t].m5, 2)) {
						/* Split waypoint and depot rail type and remove the subtype. */
						ClrBit(_m[t].m5, 2);
						ClrBit(_m[t].m5, 6);
					}
					break;

				case MP_ROAD:
					/* Swap m3 and m4, so the track type for rail crossings is the
					 * same as for normal rail. */
					Swap(_m[t].m3, _m[t].m4);
					break;

				default: break;
			}
		}
	}

	if (IsSavegameVersionBefore(61)) {
		/* Added the RoadType */
		bool old_bridge = IsSavegameVersionBefore(42);
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_ROAD:
					SB(_m[t].m5, 6, 2, GB(_m[t].m5, 4, 2));
					switch (GetRoadTileType(t)) {
						default: SlErrorCorrupt("Invalid road tile type");
						case ROAD_TILE_NORMAL:
							SB(_m[t].m4, 0, 4, GB(_m[t].m5, 0, 4));
							SB(_m[t].m4, 4, 4, 0);
							SB(_m[t].m6, 2, 4, 0);
							break;
						case ROAD_TILE_CROSSING:
							SB(_m[t].m4, 5, 2, GB(_m[t].m5, 2, 2));
							break;
						case ROAD_TILE_DEPOT:    break;
					}
					SetRoadTypes(t, ROADTYPES_ROAD);
					break;

				case MP_STATION:
					if (IsRoadStop(t)) SetRoadTypes(t, ROADTYPES_ROAD);
					break;

				case MP_TUNNELBRIDGE:
					/* Middle part of "old" bridges */
					if (old_bridge && IsBridge(t) && HasBit(_m[t].m5, 6)) break;
					if (((old_bridge && IsBridge(t)) ? (TransportType)GB(_m[t].m5, 1, 2) : GetTunnelBridgeTransportType(t)) == TRANSPORT_ROAD) {
						SetRoadTypes(t, ROADTYPES_ROAD);
					}
					break;

				default: break;
			}
		}
	}

	if (IsSavegameVersionBefore(114)) {
		bool fix_roadtypes = !IsSavegameVersionBefore(61);
		bool old_bridge = IsSavegameVersionBefore(42);

		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_ROAD:
					if (fix_roadtypes) SetRoadTypes(t, (RoadTypes)GB(_me[t].m7, 5, 3));
					SB(_me[t].m7, 5, 1, GB(_m[t].m3, 7, 1)); // snow/desert
					switch (GetRoadTileType(t)) {
						default: SlErrorCorrupt("Invalid road tile type");
						case ROAD_TILE_NORMAL:
							SB(_me[t].m7, 0, 4, GB(_m[t].m3, 0, 4)); // road works
							SB(_m[t].m6, 3, 3, GB(_m[t].m3, 4, 3));  // ground
							SB(_m[t].m3, 0, 4, GB(_m[t].m4, 4, 4));  // tram bits
							SB(_m[t].m3, 4, 4, GB(_m[t].m5, 0, 4));  // tram owner
							SB(_m[t].m5, 0, 4, GB(_m[t].m4, 0, 4));  // road bits
							break;

						case ROAD_TILE_CROSSING:
							SB(_me[t].m7, 0, 5, GB(_m[t].m4, 0, 5)); // road owner
							SB(_m[t].m6, 3, 3, GB(_m[t].m3, 4, 3));  // ground
							SB(_m[t].m3, 4, 4, GB(_m[t].m5, 0, 4));  // tram owner
							SB(_m[t].m5, 0, 1, GB(_m[t].m4, 6, 1));  // road axis
							SB(_m[t].m5, 5, 1, GB(_m[t].m4, 5, 1));  // crossing state
							break;

						case ROAD_TILE_DEPOT:
							break;
					}
					if (!IsRoadDepot(t) && !HasTownOwnedRoad(t)) {
						const Town *town = CalcClosestTownFromTile(t);
						if (town != NULL) SetTownIndex(t, town->index);
					}
					_m[t].m4 = 0;
					break;

				case MP_STATION:
					if (!IsRoadStop(t)) break;

					if (fix_roadtypes) SetRoadTypes(t, (RoadTypes)GB(_m[t].m3, 0, 3));
					SB(_me[t].m7, 0, 5, HasBit(_m[t].m6, 2) ? OWNER_TOWN : GetTileOwner(t));
					SB(_m[t].m3, 4, 4, _m[t].m1);
					_m[t].m4 = 0;
					break;

				case MP_TUNNELBRIDGE:
					if (old_bridge && IsBridge(t) && HasBit(_m[t].m5, 6)) break;
					if (((old_bridge && IsBridge(t)) ? (TransportType)GB(_m[t].m5, 1, 2) : GetTunnelBridgeTransportType(t)) == TRANSPORT_ROAD) {
						if (fix_roadtypes) SetRoadTypes(t, (RoadTypes)GB(_m[t].m3, 0, 3));

						Owner o = GetTileOwner(t);
						SB(_me[t].m7, 0, 5, o); // road owner
						SB(_m[t].m3, 4, 4, o == OWNER_NONE ? OWNER_TOWN : o); // tram owner
					}
					SB(_m[t].m6, 2, 4, GB(_m[t].m2, 4, 4)); // bridge type
					SB(_me[t].m7, 5, 1, GB(_m[t].m4, 7, 1)); // snow/desert

					_m[t].m2 = 0;
					_m[t].m4 = 0;
					break;

				default: break;
			}
		}
	}

	if (IsSavegameVersionBefore(42)) {
		Vehicle *v;

		for (TileIndex t = 0; t < map_size; t++) {
			if (MayHaveBridgeAbove(t)) ClearBridgeMiddle(t);
			if (IsBridgeTile(t)) {
				if (HasBit(_m[t].m5, 6)) { // middle part
					Axis axis = (Axis)GB(_m[t].m5, 0, 1);

					if (HasBit(_m[t].m5, 5)) { // transport route under bridge?
						if (GB(_m[t].m5, 3, 2) == TRANSPORT_RAIL) {
							MakeRailNormal(
								t,
								GetTileOwner(t),
								axis == AXIS_X ? TRACK_BIT_Y : TRACK_BIT_X,
								GetRailType(t)
							);
						} else {
							TownID town = IsTileOwner(t, OWNER_TOWN) ? ClosestTownFromTile(t, UINT_MAX)->index : 0;

							MakeRoadNormal(
								t,
								axis == AXIS_X ? ROAD_Y : ROAD_X,
								ROADTYPES_ROAD,
								town,
								GetTileOwner(t), OWNER_NONE
							);
						}
					} else {
						if (GB(_m[t].m5, 3, 2) == 0) {
							MakeClear(t, CLEAR_GRASS, 3);
						} else {
							if (GetTileSlope(t, NULL) != SLOPE_FLAT) {
								MakeShore(t);
							} else {
								if (GetTileOwner(t) == OWNER_WATER) {
									MakeSea(t);
								} else {
									MakeCanal(t, GetTileOwner(t), Random());
								}
							}
						}
					}
					SetBridgeMiddle(t, axis);
				} else { // ramp
					Axis axis = (Axis)GB(_m[t].m5, 0, 1);
					uint north_south = GB(_m[t].m5, 5, 1);
					DiagDirection dir = ReverseDiagDir(XYNSToDiagDir(axis, north_south));
					TransportType type = (TransportType)GB(_m[t].m5, 1, 2);

					_m[t].m5 = 1 << 7 | type << 2 | dir;
				}
			}
		}

		FOR_ALL_VEHICLES(v) {
			if (!v->IsGroundVehicle()) continue;
			if (IsBridgeTile(v->tile)) {
				DiagDirection dir = GetTunnelBridgeDirection(v->tile);

				if (dir != DirToDiagDir(v->direction)) continue;
				switch (dir) {
					default: SlErrorCorrupt("Invalid vehicle direction");
					case DIAGDIR_NE: if ((v->x_pos & 0xF) !=  0)            continue; break;
					case DIAGDIR_SE: if ((v->y_pos & 0xF) != TILE_SIZE - 1) continue; break;
					case DIAGDIR_SW: if ((v->x_pos & 0xF) != TILE_SIZE - 1) continue; break;
					case DIAGDIR_NW: if ((v->y_pos & 0xF) !=  0)            continue; break;
				}
			} else if (v->z_pos > GetSlopeZ(v->x_pos, v->y_pos)) {
				v->tile = GetNorthernBridgeEnd(v->tile);
			} else {
				continue;
			}
			if (v->type == VEH_TRAIN) {
				Train::From(v)->track = TRACK_BIT_WORMHOLE;
			} else {
				RoadVehicle::From(v)->state = RVSB_WORMHOLE;
			}
		}
	}

	/* Elrails got added in rev 24 */
	if (IsSavegameVersionBefore(24)) {
		RailType min_rail = RAILTYPE_ELECTRIC;

		Train *v;
		FOR_ALL_TRAINS(v) {
			RailType rt = RailVehInfo(v->engine_type)->railtype;

			v->railtype = rt;
			if (rt == RAILTYPE_ELECTRIC) min_rail = RAILTYPE_RAIL;
		}

		/* .. so we convert the entire map from normal to elrail (so maintain "fairness") */
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
					break;

				case MP_ROAD:
					if (IsLevelCrossing(t)) {
						SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
					}
					break;

				case MP_STATION:
					if (HasStationRail(t)) {
						SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
					}
					break;

				case MP_TUNNELBRIDGE:
					if (GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL) {
						SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
					}
					break;

				default:
					break;
			}
		}

		FOR_ALL_TRAINS(v) {
			if (v->IsFrontEngine() || v->IsFreeWagon()) v->ConsistChanged(true);
		}

	}

	/* In version 16.1 of the savegame a company can decide if trains, which get
	 * replaced, shall keep their old length. In all prior versions, just default
	 * to false */
	if (IsSavegameVersionBefore(16, 1)) {
		Company *c;
		FOR_ALL_COMPANIES(c) c->settings.renew_keep_length = false;
	}

	if (IsSavegameVersionBefore(123)) {
		/* Waypoints became subclasses of stations ... */
		MoveWaypointsToBaseStations();
		/* ... and buoys were moved to waypoints. */
		MoveBuoysToWaypoints();
	}

	/* From version 15, we moved a semaphore bit from bit 2 to bit 3 in m4, making
	 *  room for PBS. Now in version 21 move it back :P. */
	if (IsSavegameVersionBefore(21) && !IsSavegameVersionBefore(15)) {
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					if (HasSignals(t)) {
						/* convert PBS signals to combo-signals */
						if (HasBit(_m[t].m2, 2)) SetSignalType(t, TRACK_X, SIGTYPE_COMBO);

						/* move the signal variant back */
						SetSignalVariant(t, TRACK_X, HasBit(_m[t].m2, 3) ? SIG_SEMAPHORE : SIG_ELECTRIC);
						ClrBit(_m[t].m2, 3);
					}

					/* Clear PBS reservation on track */
					if (!IsRailDepotTile(t)) {
						SB(_m[t].m4, 4, 4, 0);
					} else {
						ClrBit(_m[t].m3, 6);
					}
					break;

				case MP_STATION: // Clear PBS reservation on station
					ClrBit(_m[t].m3, 6);
					break;

				default: break;
			}
		}
	}

	if (IsSavegameVersionBefore(25)) {
		RoadVehicle *rv;
		FOR_ALL_ROADVEHICLES(rv) {
			rv->vehstatus &= ~0x40;
		}
	}

	if (IsSavegameVersionBefore(26)) {
		Station *st;
		FOR_ALL_STATIONS(st) {
			st->last_vehicle_type = VEH_INVALID;
		}
	}

	YapfNotifyTrackLayoutChange(INVALID_TILE, INVALID_TRACK);

	if (IsSavegameVersionBefore(34)) {
		Company *c;
		FOR_ALL_COMPANIES(c) ResetCompanyLivery(c);
	}

	Company *c;
	FOR_ALL_COMPANIES(c) {
		c->avail_railtypes = GetCompanyRailtypes(c->index);
		c->avail_roadtypes = GetCompanyRoadtypes(c->index);
	}

	if (!IsSavegameVersionBefore(27)) AfterLoadStations();

	/* Time starts at 0 instead of 1920.
	 * Account for this in older games by adding an offset */
	if (IsSavegameVersionBefore(31)) {
		Station *st;
		Waypoint *wp;
		Engine *e;
		Industry *i;
		Vehicle *v;

		_date += DAYS_TILL_ORIGINAL_BASE_YEAR;
		_cur_year += ORIGINAL_BASE_YEAR;

		FOR_ALL_STATIONS(st)  st->build_date      += DAYS_TILL_ORIGINAL_BASE_YEAR;
		FOR_ALL_WAYPOINTS(wp) wp->build_date      += DAYS_TILL_ORIGINAL_BASE_YEAR;
		FOR_ALL_ENGINES(e)    e->intro_date       += DAYS_TILL_ORIGINAL_BASE_YEAR;
		FOR_ALL_COMPANIES(c)  c->inaugurated_year += ORIGINAL_BASE_YEAR;
		FOR_ALL_INDUSTRIES(i) i->last_prod_year   += ORIGINAL_BASE_YEAR;

		FOR_ALL_VEHICLES(v) {
			v->date_of_last_service += DAYS_TILL_ORIGINAL_BASE_YEAR;
			v->build_year += ORIGINAL_BASE_YEAR;
		}
	}

	/* From 32 on we save the industry who made the farmland.
	 *  To give this prettyness to old savegames, we remove all farmfields and
	 *  plant new ones. */
	if (IsSavegameVersionBefore(32)) {
		Industry *i;

		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_CLEAR) && IsClearGround(t, CLEAR_FIELDS)) {
				/* remove fields */
				MakeClear(t, CLEAR_GRASS, 3);
			} else if (IsTileType(t, MP_CLEAR) || IsTileType(t, MP_TREES)) {
				/* remove fences around fields */
				SetFenceSE(t, 0);
				SetFenceSW(t, 0);
			}
		}

		FOR_ALL_INDUSTRIES(i) {
			uint j;

			if (GetIndustrySpec(i->type)->behaviour & INDUSTRYBEH_PLANT_ON_BUILT) {
				for (j = 0; j != 50; j++) PlantRandomFarmField(i);
			}
		}
	}

	/* Setting no refit flags to all orders in savegames from before refit in orders were added */
	if (IsSavegameVersionBefore(36)) {
		Order *order;
		Vehicle *v;

		FOR_ALL_ORDERS(order) {
			order->SetRefit(CT_NO_REFIT);
		}

		FOR_ALL_VEHICLES(v) {
			v->current_order.SetRefit(CT_NO_REFIT);
		}
	}

	/* from version 38 we have optional elrails, since we cannot know the
	 * preference of a user, let elrails enabled; it can be disabled manually */
	if (IsSavegameVersionBefore(38)) _settings_game.vehicle.disable_elrails = false;
	/* do the same as when elrails were enabled/disabled manually just now */
	SettingsDisableElrail(_settings_game.vehicle.disable_elrails);
	InitializeRailGUI();

	/* From version 53, the map array was changed for house tiles to allow
	 * space for newhouses grf features. A new byte, m7, was also added. */
	if (IsSavegameVersionBefore(53)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_HOUSE)) {
				if (GB(_m[t].m3, 6, 2) != TOWN_HOUSE_COMPLETED) {
					/* Move the construction stage from m3[7..6] to m5[5..4].
					 * The construction counter does not have to move. */
					SB(_m[t].m5, 3, 2, GB(_m[t].m3, 6, 2));
					SB(_m[t].m3, 6, 2, 0);

					/* The "house is completed" bit is now in m6[2]. */
					SetHouseCompleted(t, false);
				} else {
					/* The "lift has destination" bit has been moved from
					 * m5[7] to m7[0]. */
					SB(_me[t].m7, 0, 1, HasBit(_m[t].m5, 7));
					ClrBit(_m[t].m5, 7);

					/* The "lift is moving" bit has been removed, as it does
					 * the same job as the "lift has destination" bit. */
					ClrBit(_m[t].m1, 7);

					/* The position of the lift goes from m1[7..0] to m6[7..2],
					 * making m1 totally free, now. The lift position does not
					 * have to be a full byte since the maximum value is 36. */
					SetLiftPosition(t, GB(_m[t].m1, 0, 6 ));

					_m[t].m1 = 0;
					_m[t].m3 = 0;
					SetHouseCompleted(t, true);
				}
			}
		}
	}

	/* Check and update house and town values */
	UpdateHousesAndTowns();

	if (IsSavegameVersionBefore(43)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_INDUSTRY)) {
				switch (GetIndustryGfx(t)) {
					case GFX_POWERPLANT_SPARKS:
						_m[t].m3 = GB(_m[t].m1, 2, 5);
						break;

					case GFX_OILWELL_ANIMATED_1:
					case GFX_OILWELL_ANIMATED_2:
					case GFX_OILWELL_ANIMATED_3:
						_m[t].m3 = GB(_m[t].m1, 0, 2);
						break;

					case GFX_COAL_MINE_TOWER_ANIMATED:
					case GFX_COPPER_MINE_TOWER_ANIMATED:
					case GFX_GOLD_MINE_TOWER_ANIMATED:
						 _m[t].m3 = _m[t].m1;
						 break;

					default: // No animation states to change
						break;
				}
			}
		}
	}

	if (IsSavegameVersionBefore(45)) {
		Vehicle *v;
		/* Originally just the fact that some cargo had been paid for was
		 * stored to stop people cheating and cashing in several times. This
		 * wasn't enough though as it was cleared when the vehicle started
		 * loading again, even if it didn't actually load anything, so now the
		 * amount that has been paid is stored. */
		FOR_ALL_VEHICLES(v) {
			ClrBit(v->vehicle_flags, 2);
		}
	}

	/* Buoys do now store the owner of the previous water tile, which can never
	 * be OWNER_NONE. So replace OWNER_NONE with OWNER_WATER. */
	if (IsSavegameVersionBefore(46)) {
		Waypoint *wp;
		FOR_ALL_WAYPOINTS(wp) {
			if ((wp->facilities & FACIL_DOCK) != 0 && IsTileOwner(wp->xy, OWNER_NONE) && TileHeight(wp->xy) == 0) SetTileOwner(wp->xy, OWNER_WATER);
		}
	}

	if (IsSavegameVersionBefore(50)) {
		Aircraft *v;
		/* Aircraft units changed from 8 mph to 1 km-ish/h */
		FOR_ALL_AIRCRAFT(v) {
			if (v->subtype <= AIR_AIRCRAFT) {
				const AircraftVehicleInfo *avi = AircraftVehInfo(v->engine_type);
				v->cur_speed *= 128;
				v->cur_speed /= 10;
				v->acceleration = avi->acceleration;
			}
		}
	}

	if (IsSavegameVersionBefore(49)) FOR_ALL_COMPANIES(c) c->face = ConvertFromOldCompanyManagerFace(c->face);

	if (IsSavegameVersionBefore(52)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsStatueTile(t)) {
				_m[t].m2 = CalcClosestTownFromTile(t)->index;
			}
		}
	}

	/* A setting containing the proportion of towns that grow twice as
	 * fast was added in version 54. From version 56 this is now saved in the
	 * town as cities can be built specifically in the scenario editor. */
	if (IsSavegameVersionBefore(56)) {
		Town *t;

		FOR_ALL_TOWNS(t) {
			if (_settings_game.economy.larger_towns != 0 && (t->index % _settings_game.economy.larger_towns) == 0) {
				t->larger_town = true;
			}
		}
	}

	if (IsSavegameVersionBefore(57)) {
		Vehicle *v;
		/* Added a FIFO queue of vehicles loading at stations */
		FOR_ALL_VEHICLES(v) {
			if ((v->type != VEH_TRAIN || Train::From(v)->IsFrontEngine()) &&  // for all locs
					!(v->vehstatus & (VS_STOPPED | VS_CRASHED)) && // not stopped or crashed
					v->current_order.IsType(OT_LOADING)) {         // loading
				Station::Get(v->last_station_visited)->loading_vehicles.push_back(v);

				/* The loading finished flag is *only* set when actually completely
				 * finished. Because the vehicle is loading, it is not finished. */
				ClrBit(v->vehicle_flags, VF_LOADING_FINISHED);
			}
		}
	} else if (IsSavegameVersionBefore(59)) {
		/* For some reason non-loading vehicles could be in the station's loading vehicle list */

		Station *st;
		FOR_ALL_STATIONS(st) {
			std::list<Vehicle *>::iterator iter;
			for (iter = st->loading_vehicles.begin(); iter != st->loading_vehicles.end();) {
				Vehicle *v = *iter;
				iter++;
				if (!v->current_order.IsType(OT_LOADING)) st->loading_vehicles.remove(v);
			}
		}
	}

	if (IsSavegameVersionBefore(58)) {
		/* Setting difficulty number_industries other than zero get bumped to +1
		 * since a new option (very low at position1) has been added */
		if (_settings_game.difficulty.number_industries > 0) {
			_settings_game.difficulty.number_industries++;
		}

		/* Same goes for number of towns, although no test is needed, just an increment */
		_settings_game.difficulty.number_towns++;
	}

	if (IsSavegameVersionBefore(64)) {
		/* copy the signal type/variant and move signal states bits */
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_RAILWAY) && HasSignals(t)) {
				SetSignalStates(t, GB(_m[t].m2, 4, 4));
				SetSignalVariant(t, INVALID_TRACK, GetSignalVariant(t, TRACK_X));
				SetSignalType(t, INVALID_TRACK, GetSignalType(t, TRACK_X));
				ClrBit(_m[t].m2, 7);
			}
		}
	}

	if (IsSavegameVersionBefore(69)) {
		/* In some old savegames a bit was cleared when it should not be cleared */
		RoadVehicle *rv;
		FOR_ALL_ROADVEHICLES(rv) {
			if (rv->state == 250 || rv->state == 251) {
				SetBit(rv->state, 2);
			}
		}
	}

	if (IsSavegameVersionBefore(70)) {
		/* Added variables to support newindustries */
		Industry *i;
		FOR_ALL_INDUSTRIES(i) i->founder = OWNER_NONE;
	}

	/* From version 82, old style canals (above sealevel (0), WATER owner) are no longer supported.
	    Replace the owner for those by OWNER_NONE. */
	if (IsSavegameVersionBefore(82)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_WATER) &&
					GetWaterTileType(t) == WATER_TILE_CLEAR &&
					GetTileOwner(t) == OWNER_WATER &&
					TileHeight(t) != 0) {
				SetTileOwner(t, OWNER_NONE);
			}
		}
	}

	/*
	 * Add the 'previous' owner to the ship depots so we can reset it with
	 * the correct values when it gets destroyed. This prevents that
	 * someone can remove canals owned by somebody else and it prevents
	 * making floods using the removal of ship depots.
	 */
	if (IsSavegameVersionBefore(83)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_WATER) && IsShipDepot(t)) {
				_m[t].m4 = (TileHeight(t) == 0) ? OWNER_WATER : OWNER_NONE;
			}
		}
	}

	if (IsSavegameVersionBefore(74)) {
		Station *st;
		FOR_ALL_STATIONS(st) {
			for (CargoID c = 0; c < NUM_CARGO; c++) {
				st->goods[c].last_speed = 0;
				if (st->goods[c].cargo.Count() != 0) SetBit(st->goods[c].acceptance_pickup, GoodsEntry::PICKUP);
			}
		}
	}

	if (IsSavegameVersionBefore(78)) {
		Industry *i;
		uint j;
		FOR_ALL_INDUSTRIES(i) {
			const IndustrySpec *indsp = GetIndustrySpec(i->type);
			for (j = 0; j < lengthof(i->produced_cargo); j++) {
				i->produced_cargo[j] = indsp->produced_cargo[j];
			}
			for (j = 0; j < lengthof(i->accepts_cargo); j++) {
				i->accepts_cargo[j] = indsp->accepts_cargo[j];
			}
		}
	}

	/* Before version 81, the density of grass was always stored as zero, and
	 * grassy trees were always drawn fully grassy. Furthermore, trees on rough
	 * land used to have zero density, now they have full density. Therefore,
	 * make all grassy/rough land trees have a density of 3. */
	if (IsSavegameVersionBefore(81)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (GetTileType(t) == MP_TREES) {
				TreeGround groundType = (TreeGround)GB(_m[t].m2, 4, 2);
				if (groundType != TREE_GROUND_SNOW_DESERT) SB(_m[t].m2, 6, 2, 3);
			}
		}
	}


	if (IsSavegameVersionBefore(93)) {
		/* Rework of orders. */
		Order *order;
		FOR_ALL_ORDERS(order) order->ConvertFromOldSavegame();

		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->orders.list != NULL && v->orders.list->GetFirstOrder() != NULL && v->orders.list->GetFirstOrder()->IsType(OT_NOTHING)) {
				v->orders.list->FreeChain();
				v->orders.list = NULL;
			}

			v->current_order.ConvertFromOldSavegame();
			if (v->type == VEH_ROAD && v->IsPrimaryVehicle() && v->FirstShared() == v) {
				FOR_VEHICLE_ORDERS(v, order) order->SetNonStopType(ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS);
			}
		}
	} else if (IsSavegameVersionBefore(94)) {
		/* Unload and transfer are now mutual exclusive. */
		Order *order;
		FOR_ALL_ORDERS(order) {
			if ((order->GetUnloadType() & (OUFB_UNLOAD | OUFB_TRANSFER)) == (OUFB_UNLOAD | OUFB_TRANSFER)) {
				order->SetUnloadType(OUFB_TRANSFER);
				order->SetLoadType(OLFB_NO_LOAD);
			}
		}

		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if ((v->current_order.GetUnloadType() & (OUFB_UNLOAD | OUFB_TRANSFER)) == (OUFB_UNLOAD | OUFB_TRANSFER)) {
				v->current_order.SetUnloadType(OUFB_TRANSFER);
				v->current_order.SetLoadType(OLFB_NO_LOAD);
			}
		}
	}

	if (IsSavegameVersionBefore(84)) {
		/* Set all share owners to INVALID_COMPANY for
		 * 1) all inactive companies
		 *     (when inactive companies were stored in the savegame - TTD, TTDP and some
		 *      *really* old revisions of OTTD; else it is already set in InitializeCompanies())
		 * 2) shares that are owned by inactive companies or self
		 *     (caused by cheating clients in earlier revisions) */
		FOR_ALL_COMPANIES(c) {
			for (uint i = 0; i < 4; i++) {
				CompanyID company = c->share_owners[i];
				if (company == INVALID_COMPANY) continue;
				if (!Company::IsValidID(company) || company == c->index) c->share_owners[i] = INVALID_COMPANY;
			}
		}
	}

	/* The water class was moved/unified. */
	if (IsSavegameVersionBefore(146)) {
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_STATION:
					switch (GetStationType(t)) {
						case STATION_OILRIG:
						case STATION_DOCK:
						case STATION_BUOY:
							SetWaterClass(t, (WaterClass)GB(_m[t].m3, 0, 2));
							SB(_m[t].m3, 0, 2, 0);
							break;

						default:
							SetWaterClass(t, WATER_CLASS_INVALID);
							break;
					}
					break;

				case MP_WATER:
					SetWaterClass(t, (WaterClass)GB(_m[t].m3, 0, 2));
					SB(_m[t].m3, 0, 2, 0);
					break;

				case MP_OBJECT:
					SetWaterClass(t, WATER_CLASS_INVALID);
					break;

				default:
					/* No water class. */
					break;
			}
		}
	}

	if (IsSavegameVersionBefore(86)) {
		for (TileIndex t = 0; t < map_size; t++) {
			/* Move river flag and update canals to use water class */
			if (IsTileType(t, MP_WATER)) {
				if (GetWaterClass(t) != WATER_CLASS_RIVER) {
					if (IsWater(t)) {
						Owner o = GetTileOwner(t);
						if (o == OWNER_WATER) {
							MakeSea(t);
						} else {
							MakeCanal(t, o, Random());
						}
					} else if (IsShipDepot(t)) {
						Owner o = (Owner)_m[t].m4; // Original water owner
						SetWaterClass(t, o == OWNER_WATER ? WATER_CLASS_SEA : WATER_CLASS_CANAL);
					}
				}
			}
		}

		/* Update locks, depots, docks and buoys to have a water class based
		 * on its neighbouring tiles. Done after river and canal updates to
		 * ensure neighbours are correct. */
		for (TileIndex t = 0; t < map_size; t++) {
			if (GetTileSlope(t, NULL) != SLOPE_FLAT) continue;

			if (IsTileType(t, MP_WATER) && IsLock(t)) SetWaterClassDependingOnSurroundings(t, false);
			if (IsTileType(t, MP_STATION) && (IsDock(t) || IsBuoy(t))) SetWaterClassDependingOnSurroundings(t, false);
		}
	}

	if (IsSavegameVersionBefore(87)) {
		for (TileIndex t = 0; t < map_size; t++) {
			/* skip oil rigs at borders! */
			if ((IsTileType(t, MP_WATER) || IsBuoyTile(t)) &&
					(TileX(t) == 0 || TileY(t) == 0 || TileX(t) == MapMaxX() - 1 || TileY(t) == MapMaxY() - 1)) {
				/* Some version 86 savegames have wrong water class at map borders (under buoy, or after removing buoy).
				 * This conversion has to be done before buoys with invalid owner are removed. */
				SetWaterClass(t, WATER_CLASS_SEA);
			}

			if (IsBuoyTile(t) || IsDriveThroughStopTile(t) || IsTileType(t, MP_WATER)) {
				Owner o = GetTileOwner(t);
				if (o < MAX_COMPANIES && !Company::IsValidID(o)) {
					Backup<CompanyByte> cur_company(_current_company, o, FILE_LINE);
					ChangeTileOwner(t, o, INVALID_OWNER);
					cur_company.Restore();
				}
				if (IsBuoyTile(t)) {
					/* reset buoy owner to OWNER_NONE in the station struct
					 * (even if it is owned by active company) */
					Waypoint::GetByTile(t)->owner = OWNER_NONE;
				}
			} else if (IsTileType(t, MP_ROAD)) {
				/* works for all RoadTileType */
				for (RoadType rt = ROADTYPE_ROAD; rt < ROADTYPE_END; rt++) {
					/* update even non-existing road types to update tile owner too */
					Owner o = GetRoadOwner(t, rt);
					if (o < MAX_COMPANIES && !Company::IsValidID(o)) SetRoadOwner(t, rt, OWNER_NONE);
				}
				if (IsLevelCrossing(t)) {
					if (!Company::IsValidID(GetTileOwner(t))) FixOwnerOfRailTrack(t);
				}
			} else if (IsPlainRailTile(t)) {
				if (!Company::IsValidID(GetTileOwner(t))) FixOwnerOfRailTrack(t);
			}
		}

		/* Convert old PF settings to new */
		if (_settings_game.pf.yapf.rail_use_yapf || IsSavegameVersionBefore(28)) {
			_settings_game.pf.pathfinder_for_trains = VPF_YAPF;
		} else {
			_settings_game.pf.pathfinder_for_trains = VPF_NPF;
		}

		if (_settings_game.pf.yapf.road_use_yapf || IsSavegameVersionBefore(28)) {
			_settings_game.pf.pathfinder_for_roadvehs = VPF_YAPF;
		} else {
			_settings_game.pf.pathfinder_for_roadvehs = VPF_NPF;
		}

		if (_settings_game.pf.yapf.ship_use_yapf) {
			_settings_game.pf.pathfinder_for_ships = VPF_YAPF;
		} else {
			_settings_game.pf.pathfinder_for_ships = (_settings_game.pf.new_pathfinding_all ? VPF_NPF : VPF_OPF);
		}
	}

	if (IsSavegameVersionBefore(88)) {
		/* Profits are now with 8 bit fract */
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			v->profit_this_year <<= 8;
			v->profit_last_year <<= 8;
			v->running_ticks = 0;
		}
	}

	if (IsSavegameVersionBefore(91)) {
		/* Increase HouseAnimationFrame from 5 to 7 bits */
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_HOUSE) && GetHouseType(t) >= NEW_HOUSE_OFFSET) {
				SB(_m[t].m6, 2, 6, GB(_m[t].m6, 3, 5));
				SB(_m[t].m3, 5, 1, 0);
			}
		}
	}

	if (IsSavegameVersionBefore(62)) {
		/* Remove all trams from savegames without tram support.
		 * There would be trams without tram track under causing crashes sooner or later. */
		RoadVehicle *v;
		FOR_ALL_ROADVEHICLES(v) {
			if (v->First() == v && HasBit(EngInfo(v->engine_type)->misc_flags, EF_ROAD_TRAM)) {
				if (_switch_mode_errorstr == INVALID_STRING_ID || _switch_mode_errorstr == STR_NEWGRF_COMPATIBLE_LOAD_WARNING) {
					_switch_mode_errorstr = STR_WARNING_LOADGAME_REMOVED_TRAMS;
				}
				delete v;
			}
		}
	}

	if (IsSavegameVersionBefore(99)) {
		for (TileIndex t = 0; t < map_size; t++) {
			/* Set newly introduced WaterClass of industry tiles */
			if (IsTileType(t, MP_STATION) && IsOilRig(t)) {
				SetWaterClassDependingOnSurroundings(t, true);
			}
			if (IsTileType(t, MP_INDUSTRY)) {
				if ((GetIndustrySpec(GetIndustryType(t))->behaviour & INDUSTRYBEH_BUILT_ONWATER) != 0) {
					SetWaterClassDependingOnSurroundings(t, true);
				} else {
					SetWaterClass(t, WATER_CLASS_INVALID);
				}
			}

			/* Replace "house construction year" with "house age" */
			if (IsTileType(t, MP_HOUSE) && IsHouseCompleted(t)) {
				_m[t].m5 = Clamp(_cur_year - (_m[t].m5 + ORIGINAL_BASE_YEAR), 0, 0xFF);
			}
		}
	}

	/* Move the signal variant back up one bit for PBS. We don't convert the old PBS
	 * format here, as an old layout wouldn't work properly anyway. To be safe, we
	 * clear any possible PBS reservations as well. */
	if (IsSavegameVersionBefore(100)) {
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					if (HasSignals(t)) {
						/* move the signal variant */
						SetSignalVariant(t, TRACK_UPPER, HasBit(_m[t].m2, 2) ? SIG_SEMAPHORE : SIG_ELECTRIC);
						SetSignalVariant(t, TRACK_LOWER, HasBit(_m[t].m2, 6) ? SIG_SEMAPHORE : SIG_ELECTRIC);
						ClrBit(_m[t].m2, 2);
						ClrBit(_m[t].m2, 6);
					}

					/* Clear PBS reservation on track */
					if (IsRailDepot(t)) {
						SetDepotReservation(t, false);
					} else {
						SetTrackReservation(t, TRACK_BIT_NONE);
					}
					break;

				case MP_ROAD: // Clear PBS reservation on crossing
					if (IsLevelCrossing(t)) SetCrossingReservation(t, false);
					break;

				case MP_STATION: // Clear PBS reservation on station
					if (HasStationRail(t)) SetRailStationReservation(t, false);
					break;

				case MP_TUNNELBRIDGE: // Clear PBS reservation on tunnels/birdges
					if (GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL) SetTunnelBridgeReservation(t, false);
					break;

				default: break;
			}
		}
	}

	/* Reserve all tracks trains are currently on. */
	if (IsSavegameVersionBefore(101)) {
		const Train *t;
		FOR_ALL_TRAINS(t) {
			if (t->First() == t) t->ReserveTrackUnderConsist();
		}
	}

	if (IsSavegameVersionBefore(102)) {
		for (TileIndex t = 0; t < map_size; t++) {
			/* Now all crossings should be in correct state */
			if (IsLevelCrossingTile(t)) UpdateLevelCrossing(t, false);
		}
	}

	if (IsSavegameVersionBefore(103)) {
		/* Non-town-owned roads now store the closest town */
		UpdateNearestTownForRoadTiles(false);

		/* signs with invalid owner left from older savegames */
		Sign *si;
		FOR_ALL_SIGNS(si) {
			if (si->owner != OWNER_NONE && !Company::IsValidID(si->owner)) si->owner = OWNER_NONE;
		}

		/* Station can get named based on an industry type, but the current ones
		 * are not, so mark them as if they are not named by an industry. */
		Station *st;
		FOR_ALL_STATIONS(st) {
			st->indtype = IT_INVALID;
		}
	}

	if (IsSavegameVersionBefore(104)) {
		Aircraft *a;
		FOR_ALL_AIRCRAFT(a) {
			/* Set engine_type of shadow and rotor */
			if (!a->IsNormalAircraft()) {
				a->engine_type = a->First()->engine_type;
			}
		}

		/* More companies ... */
		Company *c;
		FOR_ALL_COMPANIES(c) {
			if (c->bankrupt_asked == 0xFF) c->bankrupt_asked = 0xFFFF;
		}

		Engine *e;
		FOR_ALL_ENGINES(e) {
			if (e->company_avail == 0xFF) e->company_avail = 0xFFFF;
		}

		Town *t;
		FOR_ALL_TOWNS(t) {
			if (t->have_ratings == 0xFF) t->have_ratings = 0xFFFF;
			for (uint i = 8; i != MAX_COMPANIES; i++) t->ratings[i] = RATING_INITIAL;
		}
	}

	if (IsSavegameVersionBefore(112)) {
		for (TileIndex t = 0; t < map_size; t++) {
			/* Check for HQ bit being set, instead of using map accessor,
			 * since we've already changed it code-wise */
			if (IsTileType(t, MP_OBJECT) && HasBit(_m[t].m5, 7)) {
				/* Move size and part identification of HQ out of the m5 attribute,
				 * on new locations */
				_m[t].m3 = GB(_m[t].m5, 0, 5);
				_m[t].m5 = OBJECT_HQ;
			}
		}
	}
	if (IsSavegameVersionBefore(144)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (!IsTileType(t, MP_OBJECT)) continue;

			/* Reordering/generalisation of the object bits. */
			ObjectType type = GetObjectType(t);
			SB(_m[t].m6, 2, 4, type == OBJECT_HQ ? GB(_m[t].m3, 2, 3) : 0);
			_m[t].m3 = type == OBJECT_HQ ? GB(_m[t].m3, 1, 1) | GB(_m[t].m3, 0, 1) << 4 : 0;

			/* Make sure those bits are clear as well! */
			_m[t].m4 = 0;
			_me[t].m7 = 0;
		}
	}

	if (IsSavegameVersionBefore(147) && Object::GetNumItems() == 0) {
		/* Make real objects for object tiles. */
		for (TileIndex t = 0; t < map_size; t++) {
			if (!IsTileType(t, MP_OBJECT)) continue;

			if (Town::GetNumItems() == 0) {
				/* No towns, so remove all objects! */
				DoClearSquare(t);
			} else {
				uint offset = _m[t].m3;

				/* Also move the animation state. */
				_m[t].m3 = GB(_m[t].m6, 2, 4);
				SB(_m[t].m6, 2, 4, 0);

				if (offset == 0) {
					/* No offset, so make the object. */
					ObjectType type = GetObjectType(t);
					int size = type == OBJECT_HQ ? 2 : 1;

					Object *o = new Object();
					o->location.tile = t;
					o->location.w    = size;
					o->location.h    = size;
					o->build_date    = _date;
					o->town          = type == OBJECT_STATUE ? Town::Get(_m[t].m2) : CalcClosestTownFromTile(t, UINT_MAX);
					_m[t].m2 = o->index;
					Object::IncTypeCount(type);
				} else {
					/* We're at an offset, so get the ID from our "root". */
					TileIndex northern_tile = t - TileXY(GB(offset, 0, 4), GB(offset, 4, 4));
					assert(IsTileType(northern_tile, MP_OBJECT));
					_m[t].m2 = _m[northern_tile].m2;
				}
			}
		}
	}

	if (IsSavegameVersionBefore(113)) {
		/* allow_town_roads is added, set it if town_layout wasn't TL_NO_ROADS */
		if (_settings_game.economy.town_layout == 0) { // was TL_NO_ROADS
			_settings_game.economy.allow_town_roads = false;
			_settings_game.economy.town_layout = TL_BETTER_ROADS;
		} else {
			_settings_game.economy.allow_town_roads = true;
			_settings_game.economy.town_layout = _settings_game.economy.town_layout - 1;
		}

		/* Initialize layout of all towns. Older versions were using different
		 * generator for random town layout, use it if needed. */
		Town *t;
		FOR_ALL_TOWNS(t) {
			if (_settings_game.economy.town_layout != TL_RANDOM) {
				t->layout = _settings_game.economy.town_layout;
				continue;
			}

			/* Use old layout randomizer code */
			byte layout = TileHash(TileX(t->xy), TileY(t->xy)) % 6;
			switch (layout) {
				default: break;
				case 5: layout = 1; break;
				case 0: layout = 2; break;
			}
			t->layout = layout - 1;
		}
	}

	if (IsSavegameVersionBefore(114)) {
		/* There could be (deleted) stations with invalid owner, set owner to OWNER NONE.
		 * The conversion affects oil rigs and buoys too, but it doesn't matter as
		 * they have st->owner == OWNER_NONE already. */
		Station *st;
		FOR_ALL_STATIONS(st) {
			if (!Company::IsValidID(st->owner)) st->owner = OWNER_NONE;
		}
	}

	/* Trains could now stop in a specific location. */
	if (IsSavegameVersionBefore(117)) {
		Order *o;
		FOR_ALL_ORDERS(o) {
			if (o->IsType(OT_GOTO_STATION)) o->SetStopLocation(OSL_PLATFORM_FAR_END);
		}
	}

	if (IsSavegameVersionBefore(120)) {
		extern VehicleDefaultSettings _old_vds;
		Company *c;
		FOR_ALL_COMPANIES(c) {
			c->settings.vehicle = _old_vds;
		}
	}

	if (IsSavegameVersionBefore(121)) {
		/* Delete small ufos heading for non-existing vehicles */
		Vehicle *v;
		FOR_ALL_DISASTERVEHICLES(v) {
			if (v->subtype == 2/*ST_SMALL_UFO*/ && v->current_order.GetDestination() != 0) {
				const Vehicle *u = Vehicle::GetIfValid(v->dest_tile);
				if (u == NULL || u->type != VEH_ROAD || !RoadVehicle::From(u)->IsRoadVehFront()) {
					delete v;
				}
			}
		}

		/* We didn't store cargo payment yet, so make them for vehicles that are
		 * currently at a station and loading/unloading. If they don't get any
		 * payment anymore they just removed in the next load/unload cycle.
		 * However, some 0.7 versions might have cargo payment. For those we just
		 * add cargopayment for the vehicles that don't have it.
		 */
		Station *st;
		FOR_ALL_STATIONS(st) {
			std::list<Vehicle *>::iterator iter;
			for (iter = st->loading_vehicles.begin(); iter != st->loading_vehicles.end(); ++iter) {
				Vehicle *v = *iter;
				if (v->cargo_payment == NULL) v->cargo_payment = new CargoPayment(v);
			}
		}
	}

	if (IsSavegameVersionBefore(122)) {
		/* Animated tiles would sometimes not be actually animated or
		 * in case of old savegames duplicate. */

		extern TileIndex *_animated_tile_list;
		extern uint _animated_tile_count;

		for (uint i = 0; i < _animated_tile_count; /* Nothing */) {
			/* Remove if tile is not animated */
			bool remove = _tile_type_procs[GetTileType(_animated_tile_list[i])]->animate_tile_proc == NULL;

			/* and remove if duplicate */
			for (uint j = 0; !remove && j < i; j++) {
				remove = _animated_tile_list[i] == _animated_tile_list[j];
			}

			if (remove) {
				DeleteAnimatedTile(_animated_tile_list[i]);
			} else {
				i++;
			}
		}
	}

	if (IsSavegameVersionBefore(124)) {
		/* The train station tile area was added */
		Waypoint *wp;
		FOR_ALL_WAYPOINTS(wp) {
			if (wp->facilities & FACIL_TRAIN) {
				wp->train_station.tile = wp->xy;
				wp->train_station.w = 1;
				wp->train_station.h = 1;
			} else {
				wp->train_station.tile = INVALID_TILE;
				wp->train_station.w = 0;
				wp->train_station.h = 0;
			}
		}
	}

	if (IsSavegameVersionBefore(125)) {
		/* Convert old subsidies */
		Subsidy *s;
		FOR_ALL_SUBSIDIES(s) {
			if (s->remaining < 12) {
				/* Converting nonawarded subsidy */
				s->remaining = 12 - s->remaining; // convert "age" to "remaining"
				s->awarded = INVALID_COMPANY; // not awarded to anyone
				const CargoSpec *cs = CargoSpec::Get(s->cargo_type);
				switch (cs->town_effect) {
					case TE_PASSENGERS:
					case TE_MAIL:
						/* Town -> Town */
						s->src_type = s->dst_type = ST_TOWN;
						if (Town::IsValidID(s->src) && Town::IsValidID(s->dst)) continue;
						break;
					case TE_GOODS:
					case TE_FOOD:
						/* Industry -> Town */
						s->src_type = ST_INDUSTRY;
						s->dst_type = ST_TOWN;
						if (Industry::IsValidID(s->src) && Town::IsValidID(s->dst)) continue;
						break;
					default:
						/* Industry -> Industry */
						s->src_type = s->dst_type = ST_INDUSTRY;
						if (Industry::IsValidID(s->src) && Industry::IsValidID(s->dst)) continue;
						break;
				}
			} else {
				/* Do our best for awarded subsidies. The original source or destination industry
				 * can't be determined anymore for awarded subsidies, so invalidate them.
				 * Town -> Town subsidies are converted using simple heuristic */
				s->remaining = 24 - s->remaining; // convert "age of awarded subsidy" to "remaining"
				const CargoSpec *cs = CargoSpec::Get(s->cargo_type);
				switch (cs->town_effect) {
					case TE_PASSENGERS:
					case TE_MAIL: {
						/* Town -> Town */
						const Station *ss = Station::GetIfValid(s->src);
						const Station *sd = Station::GetIfValid(s->dst);
						if (ss != NULL && sd != NULL && ss->owner == sd->owner &&
								Company::IsValidID(ss->owner)) {
							s->src_type = s->dst_type = ST_TOWN;
							s->src = ss->town->index;
							s->dst = sd->town->index;
							s->awarded = ss->owner;
							continue;
						}
						break;
					}
					default:
						break;
				}
			}
			/* Awarded non-town subsidy or invalid source/destination, invalidate */
			delete s;
		}
	}

	if (IsSavegameVersionBefore(126)) {
		/* Recompute inflation based on old unround loan limit
		 * Note: Max loan is 500000. With an inflation of 4% across 170 years
		 *       that results in a max loan of about 0.7 * 2^31.
		 *       So taking the 16 bit fractional part into account there are plenty of bits left
		 *       for unmodified savegames ...
		 */
		uint64 aimed_inflation = (_economy.old_max_loan_unround << 16 | _economy.old_max_loan_unround_fract) / _settings_game.difficulty.max_loan;

		/* ... well, just clamp it then. */
		if (aimed_inflation > MAX_INFLATION) aimed_inflation = MAX_INFLATION;

		/* Simulate the inflation, so we also get the payment inflation */
		while (_economy.inflation_prices < aimed_inflation) {
			AddInflation(false);
		}
	}

	if (IsSavegameVersionBefore(127)) {
		Station *st;
		FOR_ALL_STATIONS(st) UpdateStationAcceptance(st, false);
	}

	if (IsSavegameVersionBefore(128)) {
		const Depot *d;
		FOR_ALL_DEPOTS(d) {
			_m[d->xy].m2 = d->index;
			if (IsTileType(d->xy, MP_WATER)) _m[GetOtherShipDepotTile(d->xy)].m2 = d->index;
		}
	}

	/* The behaviour of force_proceed has been changed. Now
	 * it counts signals instead of some random time out. */
	if (IsSavegameVersionBefore(131)) {
		Train *t;
		FOR_ALL_TRAINS(t) {
			if (t->force_proceed != TFP_NONE) {
				t->force_proceed = TFP_STUCK;
			}
		}
	}

	/* The bits for the tree ground and tree density have
	 * been swapped (m2 bits 7..6 and 5..4. */
	if (IsSavegameVersionBefore(135)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_CLEAR)) {
				if (GetRawClearGround(t) == CLEAR_SNOW) {
					SetClearGroundDensity(t, CLEAR_GRASS, GetClearDensity(t));
					SetBit(_m[t].m3, 4);
				} else {
					ClrBit(_m[t].m3, 4);
				}
			}
			if (IsTileType(t, MP_TREES)) {
				uint density = GB(_m[t].m2, 6, 2);
				uint ground = GB(_m[t].m2, 4, 2);
				uint counter = GB(_m[t].m2, 0, 4);
				_m[t].m2 = ground << 6 | density << 4 | counter;
			}
		}
	}

	/* Wait counter and load/unload ticks got split. */
	if (IsSavegameVersionBefore(136)) {
		Aircraft *a;
		FOR_ALL_AIRCRAFT(a) {
			a->turn_counter = a->current_order.IsType(OT_LOADING) ? 0 : a->load_unload_ticks;
		}

		Train *t;
		FOR_ALL_TRAINS(t) {
			t->wait_counter = t->current_order.IsType(OT_LOADING) ? 0 : t->load_unload_ticks;
		}
	}

	/* Airport tile animation uses animation frame instead of other graphics id */
	if (IsSavegameVersionBefore(137)) {
		struct AirportTileConversion {
			byte old_start;
			byte num_frames;
		};
		static const AirportTileConversion atc[] = {
			{31,  12}, // APT_RADAR_GRASS_FENCE_SW
			{50,   4}, // APT_GRASS_FENCE_NE_FLAG
			{62,   2}, // 1 unused tile
			{66,  12}, // APT_RADAR_FENCE_SW
			{78,  12}, // APT_RADAR_FENCE_NE
			{101, 10}, // 9 unused tiles
			{111,  8}, // 7 unused tiles
			{119, 15}, // 14 unused tiles (radar)
			{140,  4}, // APT_GRASS_FENCE_NE_FLAG_2
		};
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsAirportTile(t)) {
				StationGfx old_gfx = GetStationGfx(t);
				byte offset = 0;
				for (uint i = 0; i < lengthof(atc); i++) {
					if (old_gfx < atc[i].old_start) {
						SetStationGfx(t, old_gfx - offset);
						break;
					}
					if (old_gfx < atc[i].old_start + atc[i].num_frames) {
						SetAnimationFrame(t, old_gfx - atc[i].old_start);
						SetStationGfx(t, atc[i].old_start - offset);
						break;
					}
					offset += atc[i].num_frames - 1;
				}
			}
		}
	}

	if (IsSavegameVersionBefore(139)) {
		Train *t;
		FOR_ALL_TRAINS(t) {
			/* Copy old GOINGUP / GOINGDOWN flags. */
			if (HasBit(t->flags, 1)) {
				ClrBit(t->flags, 1);
				SetBit(t->gv_flags, GVF_GOINGUP_BIT);
			} else if (HasBit(t->flags, 2)) {
				ClrBit(t->flags, 2);
				SetBit(t->gv_flags, GVF_GOINGDOWN_BIT);
			}
		}
	}

	if (IsSavegameVersionBefore(140)) {
		Station *st;
		FOR_ALL_STATIONS(st) {
			if (st->airport.tile != INVALID_TILE) {
				st->airport.w = st->airport.GetSpec()->size_x;
				st->airport.h = st->airport.GetSpec()->size_y;
			}
		}
	}

	if (IsSavegameVersionBefore(141)) {
		for (TileIndex t = 0; t < map_size; t++) {
			/* Reset tropic zone for VOID tiles, they shall not have any. */
			if (IsTileType(t, MP_VOID)) SetTropicZone(t, TROPICZONE_NORMAL);
		}

		/* We need to properly number/name the depots.
		 * The first step is making sure none of the depots uses the
		 * 'default' names, after that we can assign the names. */
		Depot *d;
		FOR_ALL_DEPOTS(d) d->town_cn = UINT16_MAX;

		FOR_ALL_DEPOTS(d) MakeDefaultName(d);
	}

	if (IsSavegameVersionBefore(142)) {
		Depot *d;
		FOR_ALL_DEPOTS(d) d->build_date = _date;
	}

	/* In old versions it was possible to remove an airport while a plane was
	 * taking off or landing. This gives all kind of problems when building
	 * another airport in the same station so we don't allow that anymore.
	 * For old savegames with such aircraft we just throw them in the air and
	 * treat the aircraft like they were flying already. */
	if (IsSavegameVersionBefore(146)) {
		Aircraft *v;
		FOR_ALL_AIRCRAFT(v) {
			if (!v->IsNormalAircraft()) continue;
			Station *st = GetTargetAirportIfValid(v);
			if (st == NULL && v->state != FLYING) {
				v->state = FLYING;
				UpdateAircraftCache(v);
				AircraftNextAirportPos_and_Order(v);
				/* get aircraft back on running altitude */
				if ((v->vehstatus & VS_CRASHED) == 0) SetAircraftPosition(v, v->x_pos, v->y_pos, GetAircraftFlyingAltitude(v));
			}
		}
	}

	/* Move the animation frame to the same location (m7) for all objects. */
	if (IsSavegameVersionBefore(147)) {
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_HOUSE:
					if (GetHouseType(t) >= NEW_HOUSE_OFFSET) {
						uint per_proc = _me[t].m7;
						_me[t].m7 = GB(_m[t].m6, 2, 6) | (GB(_m[t].m3, 5, 1) << 6);
						SB(_m[t].m3, 5, 1, 0);
						SB(_m[t].m6, 2, 6, min(per_proc, 63));
					}
					break;

				case MP_INDUSTRY: {
					uint rand = _me[t].m7;
					_me[t].m7 = _m[t].m3;
					_m[t].m3 = rand;
					break;
				}

				case MP_OBJECT:
					_me[t].m7 = _m[t].m3;
					_m[t].m3 = 0;
					break;

				default:
					/* For stations/airports it's already at m7 */
					break;
			}
		}
	}

	/* Add (random) colour to all objects. */
	if (IsSavegameVersionBefore(148)) {
		Object *o;
		FOR_ALL_OBJECTS(o) {
			Owner owner = GetTileOwner(o->location.tile);
			o->colour = (owner == OWNER_NONE) ? Random() & 0xF : Company::Get(owner)->livery->colour1;
		}
	}

	if (IsSavegameVersionBefore(149)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (!IsTileType(t, MP_STATION)) continue;
			if (!IsBuoy(t) && !IsOilRig(t) && !(IsDock(t) && GetTileSlope(t, NULL) == SLOPE_FLAT)) {
				SetWaterClass(t, WATER_CLASS_INVALID);
			}
		}

		/* Waypoints with custom name may have a non-unique town_cn,
		 * renumber those. First set all affected waypoints to the
		 * highest possible number to get them numbered in the
		 * order they have in the pool. */
		Waypoint *wp;
		FOR_ALL_WAYPOINTS(wp) {
			if (wp->name != NULL) wp->town_cn = UINT16_MAX;
		}

		FOR_ALL_WAYPOINTS(wp) {
			if (wp->name != NULL) MakeDefaultName(wp);
		}
	}

	if (IsSavegameVersionBefore(152)) {
		_industry_builder.Reset(); // Initialize industry build data.

		/* The moment vehicles go from hidden to visible changed. This means
		 * that vehicles don't always get visible anymore causing things to
		 * get messed up just after loading the savegame. This fixes that. */
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			/* Is the vehicle in a tunnel? */
			if (!IsTunnelTile(v->tile)) continue;

			/* Is the vehicle actually at a tunnel entrance/exit? */
			TileIndex vtile = TileVirtXY(v->x_pos, v->y_pos);
			if (!IsTunnelTile(vtile)) continue;

			/* Are we actually in this tunnel? Or maybe a lower tunnel? */
			if (GetSlopeZ(v->x_pos, v->y_pos) != v->z_pos) continue;

			/* What way are we going? */
			const DiagDirection dir = GetTunnelBridgeDirection(vtile);
			const DiagDirection vdir = DirToDiagDir(v->direction);

			/* Have we passed the visibility "switch" state already? */
			byte pos = (DiagDirToAxis(vdir) == AXIS_X ? v->x_pos : v->y_pos) & TILE_UNIT_MASK;
			byte frame = (vdir == DIAGDIR_NE || vdir == DIAGDIR_NW) ? TILE_SIZE - 1 - pos : pos;
			extern const byte _tunnel_visibility_frame[DIAGDIR_END];

			/* Should the vehicle be hidden or not? */
			bool hidden;
			if (dir == vdir) { // Entering tunnel
				hidden = frame >= _tunnel_visibility_frame[dir];
			} else if (dir == ReverseDiagDir(vdir)) { // Leaving tunnel
				hidden = frame < TILE_SIZE - _tunnel_visibility_frame[dir];
			} else { // Something freaky going on?
				NOT_REACHED();
			}
			v->tile = vtile;

			if (hidden) {
				v->vehstatus |= VS_HIDDEN;

				switch (v->type) {
					case VEH_TRAIN: Train::From(v)->track       = TRACK_BIT_WORMHOLE; break;
					case VEH_ROAD:  RoadVehicle::From(v)->state = RVSB_WORMHOLE;      break;
					default: NOT_REACHED();
				}
			} else {
				v->vehstatus &= ~VS_HIDDEN;

				switch (v->type) {
					case VEH_TRAIN: Train::From(v)->track       = DiagDirToDiagTrackBits(vdir); break;
					case VEH_ROAD:  RoadVehicle::From(v)->state = DiagDirToDiagTrackdir(vdir); RoadVehicle::From(v)->frame = frame; break;
					default: NOT_REACHED();
				}
			}
		}
	}

	if (IsSavegameVersionBefore(153)) {
		RoadVehicle *rv;
		FOR_ALL_ROADVEHICLES(rv) {
			if (rv->state == RVSB_IN_DEPOT || rv->state == RVSB_WORMHOLE) continue;

			bool loading = rv->current_order.IsType(OT_LOADING) || rv->current_order.IsType(OT_LEAVESTATION);
			if (HasBit(rv->state, RVS_IN_ROAD_STOP)) {
				extern const byte _road_stop_stop_frame[];
				SB(rv->state, RVS_ENTERED_STOP, 1, loading || rv->frame > _road_stop_stop_frame[rv->state - RVSB_IN_ROAD_STOP + (_settings_game.vehicle.road_side << RVS_DRIVE_SIDE)]);
			} else if (HasBit(rv->state, RVS_IN_DT_ROAD_STOP)) {
				SB(rv->state, RVS_ENTERED_STOP, 1, loading || rv->frame > RVC_DRIVE_THROUGH_STOP_FRAME);
			}
		}
	}

	if (IsSavegameVersionBefore(156)) {
		/* The train's pathfinder lost flag got moved. */
		Train *t;
		FOR_ALL_TRAINS(t) {
			if (!HasBit(t->flags, 5)) continue;

			ClrBit(t->flags, 5);
			SetBit(t->vehicle_flags, VF_PATHFINDER_LOST);
		}
	}

	/* Road stops is 'only' updating some caches */
	AfterLoadRoadStops();
	AfterLoadLabelMaps();

	GamelogPrintDebug(1);

	InitializeWindowsAndCaches();
	/* Restore the signals */
	ResetSignalHandlers();
	return true;
}

/**
 * Reload all NewGRF files during a running game. This is a cut-down
 * version of AfterLoadGame().
 * XXX - We need to reset the vehicle position hash because with a non-empty
 * hash AfterLoadVehicles() will loop infinitely. We need AfterLoadVehicles()
 * to recalculate vehicle data as some NewGRF vehicle sets could have been
 * removed or added and changed statistics
 */
void ReloadNewGRFData()
{
	/* reload grf data */
	GfxLoadSprites();
	LoadStringWidthTable();
	RecomputePrices();
	/* reload vehicles */
	ResetVehiclePosHash();
	AfterLoadVehicles(false);
	StartupEngines();
	SetCachedEngineCounts();
	/* update station graphics */
	AfterLoadStations();
	/* Check and update house and town values */
	UpdateHousesAndTowns();
	/* Update livery selection windows */
	for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) InvalidateWindowData(WC_COMPANY_COLOUR, i);
	/* redraw the whole screen */
	MarkWholeScreenDirty();
	CheckTrainsLengths();
}
