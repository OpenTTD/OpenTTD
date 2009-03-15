/* $Id$ */

/** @file afterload.cpp Code updating data after game load */

#include "../stdafx.h"
#include "../void_map.h"
#include "../signs_base.h"
#include "../window_func.h"
#include "../fios.h"
#include "../train.h"
#include "../string_func.h"
#include "../gamelog.h"
#include "../network/network.h"
#include "../gfxinit.h"
#include "../functions.h"
#include "../industry_map.h"
#include "../town_map.h"
#include "../clear_map.h"
#include "../vehicle_func.h"
#include "../newgrf_station.h"
#include "../yapf/yapf.hpp"
#include "../elrail_func.h"
#include "../signs_func.h"
#include "../aircraft.h"
#include "../unmovable_map.h"
#include "../tree_map.h"
#include "../company_func.h"
#include "../road_cmd.h"
#include "../ai/ai.hpp"

#include "table/strings.h"

#include "saveload_internal.h"

#include <signal.h>

extern StringID _switch_mode_errorstr;
extern Company *DoStartupNewCompany(bool is_ai);
extern void InitializeRailGUI();

/**
 * Makes a tile canal or water depending on the surroundings.
 *
 * Must only be used for converting old savegames. Use WaterClass now.
 *
 * This as for example docks and shipdepots do not store
 * whether the tile used to be canal or 'normal' water.
 * @param t the tile to change.
 * @param o the owner of the new tile.
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
			NOT_REACHED();
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
						default: NOT_REACHED();
					}
				}
				break;

			case MP_RAILWAY:
				/* Shore or flooded halftile */
				has_water |= (GetRailGroundType(neighbour) == RAIL_GROUND_WATER);
				break;

			case MP_TREES:
				/* trees on shore */
				has_water |= (GetTreeGround(neighbour) == TREE_GROUND_SHORE);
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
				/* FALLTHROUGH */

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
 * Initialization of the windows and several kinds of caches.
 * This is not done directly in AfterLoadGame because these
 * functions require that all saveload conversions have been
 * done. As people tend to add savegame conversion stuff after
 * the intialization of the windows and caches quite some bugs
 * had been made.
 * Moving this out of there is both cleaner and less bug-prone.
 *
 * @return true if everything went according to plan, otherwise false.
 */
static bool InitializeWindowsAndCaches()
{
	/* Initialize windows */
	ResetWindowSystem();
	SetupColoursAndInitialWindow();

	ResetViewportAfterLoadGame();

	/* Update coordinates of the signs. */
	UpdateAllStationVirtCoord();
	UpdateAllSignVirtCoords();
	UpdateAllTownVirtCoords();
	UpdateAllWaypointSigns();

	Company *c;
	FOR_ALL_COMPANIES(c) {
		/* For each company, verify (while loading a scenario) that the inauguration date is the current year and set it
		 * accordingly if it is not the case.  No need to set it on companies that are not been used already,
		 * thus the MIN_YEAR (which is really nothing more than Zero, initialized value) test */
		if (_file_to_saveload.filetype == FT_SCENARIO && c->inaugurated_year != MIN_YEAR) {
			c->inaugurated_year = _cur_year;
		}
	}

	SetCachedEngineCounts();

	/* Towns have a noise controlled number of airports system
	 * So each airport's noise value must be added to the town->noise_reached value
	 * Reset each town's noise_reached value to '0' before. */
	UpdateAirportsNoise();

	CheckTrainsLengths();

	return true;
}

/**
 * Signal handler used to give a user a more useful report for crashes during
 * the savegame loading process; especially when there's problems with the
 * NewGRFs that are required by the savegame.
 * @param unused well... unused
 */
void CDECL HandleSavegameLoadCrash(int unused)
{
	char buffer[8192];
	char *p = buffer;
	p += seprintf(p, lastof(buffer),
			"Loading your savegame caused OpenTTD to crash.\n"
			"This is most likely caused by a missing NewGRF or a NewGRF that has been\n"
			"loaded as replacement for a missing NewGRF. OpenTTD cannot easily\n"
			"determine whether a replacement NewGRF is of a newer or older version.\n"
			"It will load a NewGRF with the same GRF ID as the missing NewGRF. This\n"
			"means that if the author makes incompatible NewGRFs with the same GRF ID\n"
			"OpenTTD cannot magically do the right thing. In most cases OpenTTD will\n"
			"load the savegame and not crash, but this is an exception.\n"
			"Please load the savegame with the appropriate NewGRFs. When loading a\n"
			"savegame still crashes when all NewGRFs are found you should file a\n"
			"bug report. The missing NewGRFs are:\n");

	for (GRFConfig *c = _grfconfig; c != NULL; c = c->next) {
		if (HasBit(c->flags, GCF_COMPATIBLE)) {
			char buf[40];
			md5sumToString(buf, lastof(buf), c->md5sum);
			p += seprintf(p, lastof(buffer), "NewGRF %08X (%s) not found; checksum %s. Tried another NewGRF with same GRF ID\n", BSWAP32(c->grfid), c->filename, buf);
		}
		if (c->status == GCS_NOT_FOUND) {
			char buf[40];
			md5sumToString(buf, lastof(buf), c->md5sum);
			p += seprintf(p, lastof(buffer), "NewGRF %08X (%s) not found; checksum %s\n", BSWAP32(c->grfid), c->filename, buf);
		}
	}

	ShowInfo(buffer);
}

/**
 * Tries to change owner of this rail tile to a valid owner. In very old versions it could happen that
 * a rail track had an invalid owner. When conversion isn't possible, track is removed.
 * @param t tile to update
 */
static void FixOwnerOfRailTrack(TileIndex t)
{
	assert(!IsValidCompanyID(GetTileOwner(t)) && (IsLevelCrossingTile(t) || IsPlainRailTile(t)));

	/* remove leftover rail piece from crossing (from very old savegames) */
	Vehicle *v = NULL, *w;
	FOR_ALL_VEHICLES(w) {
		if (w->type == VEH_TRAIN && w->tile == t) {
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
				IsValidCompanyID(GetTileOwner(tt))) {
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
	typedef void (CDECL *SignalHandlerPointer)(int);
	SignalHandlerPointer prev_segfault = signal(SIGSEGV, HandleSavegameLoadCrash);
	SignalHandlerPointer prev_abort = signal(SIGABRT, HandleSavegameLoadCrash);

	TileIndex map_size = MapSize();
	Company *c;

	if (CheckSavegameVersion(98)) GamelogOldver();

	GamelogTestRevision();
	GamelogTestMode();

	if (CheckSavegameVersion(98)) GamelogGRFAddList(_grfconfig);

	/* in very old versions, size of train stations was stored differently */
	if (CheckSavegameVersion(2)) {
		Station *st;
		FOR_ALL_STATIONS(st) {
			if (st->train_tile != 0 && st->trainst_h == 0) {
				uint n = _savegame_type == SGT_OTTD ? 4 : 3; // OTTD uses 4 bits per dimensions, TTD 3 bits
				uint w = GB(st->trainst_w, n, n);
				uint h = GB(st->trainst_w, 0, n);

				if (GetRailStationAxis(st->train_tile) != AXIS_X) Swap(w, h);

				st->trainst_w = w;
				st->trainst_h = h;

				assert(GetStationIndex(st->train_tile + TileDiffXY(w - 1, h - 1)) == st->index);
			}
		}
	}

	/* in version 2.1 of the savegame, town owner was unified. */
	if (CheckSavegameVersionOldStyle(2, 1)) ConvertTownOwner();

	/* from version 4.1 of the savegame, exclusive rights are stored at towns */
	if (CheckSavegameVersionOldStyle(4, 1)) UpdateExclusiveRights();

	/* from version 4.2 of the savegame, currencies are in a different order */
	if (CheckSavegameVersionOldStyle(4, 2)) UpdateCurrencies();

	/* In old version there seems to be a problem that water is owned by
	 * OWNER_NONE, not OWNER_WATER.. I can't replicate it for the current
	 * (4.3) version, so I just check when versions are older, and then
	 * walk through the whole map.. */
	if (CheckSavegameVersionOldStyle(4, 3)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_WATER) && GetTileOwner(t) >= MAX_COMPANIES) {
				SetTileOwner(t, OWNER_WATER);
			}
		}
	}

	if (CheckSavegameVersion(84)) {
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

		Waypoint *wp;
		FOR_ALL_WAYPOINTS(wp) {
			wp->name = CopyFromOldName(wp->string);
			wp->string = STR_EMPTY;
		}
	}

	/* From this point the old names array is cleared. */
	ResetOldNames();

	if (CheckSavegameVersion(106)) {
		/* no station is determined by 'tile == INVALID_TILE' now (instead of '0') */
		Station *st;
		FOR_ALL_STATIONS(st) {
			if (st->airport_tile == 0) st->airport_tile = INVALID_TILE;
			if (st->dock_tile    == 0) st->dock_tile    = INVALID_TILE;
			if (st->train_tile   == 0) st->train_tile   = INVALID_TILE;
		}

		/* the same applies to Company::location_of_HQ */
		Company *c;
		FOR_ALL_COMPANIES(c) {
			if (c->location_of_HQ == 0 || (CheckSavegameVersion(4) && c->location_of_HQ == 0xFFFF)) {
				c->location_of_HQ = INVALID_TILE;
			}
		}
	}

	/* convert road side to my format. */
	if (_settings_game.vehicle.road_side) _settings_game.vehicle.road_side = 1;

	/* Check if all NewGRFs are present, we are very strict in MP mode */
	GRFListCompatibility gcf_res = IsGoodGRFConfigList();
	if (_networking && gcf_res != GLC_ALL_GOOD) {
		SetSaveLoadError(STR_NETWORK_ERR_CLIENT_NEWGRF_MISMATCH);
		/* Restore the signals */
		signal(SIGSEGV, prev_segfault);
		signal(SIGABRT, prev_abort);
		return false;
	}

	switch (gcf_res) {
		case GLC_COMPATIBLE: _switch_mode_errorstr = STR_NEWGRF_COMPATIBLE_LOAD_WARNING; break;
		case GLC_NOT_FOUND: _switch_mode_errorstr = STR_NEWGRF_DISABLED_WARNING; _pause_game = -1; break;
		default: break;
	}

	/* Update current year
	 * must be done before loading sprites as some newgrfs check it */
	SetDate(_date);

	/* Force dynamic engines off when loading older savegames */
	if (CheckSavegameVersion(95)) _settings_game.vehicle.dynamic_engines = 0;

	/* Load the sprites */
	GfxLoadSprites();
	LoadStringWidthTable();

	/* Copy temporary data to Engine pool */
	CopyTempEngineData();

	/* Connect front and rear engines of multiheaded trains and converts
	 * subtype to the new format */
	if (CheckSavegameVersionOldStyle(17, 1)) ConvertOldMultiheadToNew();

	/* Connect front and rear engines of multiheaded trains */
	ConnectMultiheadedTrains();

	/* reinit the landscape variables (landscape might have changed) */
	InitializeLandscapeVariables(true);

	/* Update all vehicles */
	AfterLoadVehicles(true);

	/* Make sure there is an AI attached to an AI company */
	{
		Company *c;
		FOR_ALL_COMPANIES(c) {
			if (c->is_ai && c->ai_instance == NULL) AI::StartNew(c->index);
		}
	}

	/* Update all waypoints */
	if (CheckSavegameVersion(12)) FixOldWaypoints();

	/* make sure there is a town in the game */
	if (_game_mode == GM_NORMAL && !ClosestTownFromTile(0, UINT_MAX)) {
		SetSaveLoadError(STR_NO_TOWN_IN_SCENARIO);
		/* Restore the signals */
		signal(SIGSEGV, prev_segfault);
		signal(SIGABRT, prev_abort);
		return false;
	}

	/* The void tiles on the southern border used to belong to a wrong class (pre 4.3).
	 * This problem appears in savegame version 21 too, see r3455. But after loading the
	 * savegame and saving again, the buggy map array could be converted to new savegame
	 * version. It didn't show up before r12070. */
	if (CheckSavegameVersion(87)) UpdateVoidTiles();

	/* If Load Scenario / New (Scenario) Game is used,
	 *  a company does not exist yet. So create one here.
	 * 1 exeption: network-games. Those can have 0 companies
	 *   But this exeption is not true for non dedicated network_servers! */
	if (!IsValidCompanyID(COMPANY_FIRST) && (!_networking || (_networking && _network_server && !_network_dedicated)))
		DoStartupNewCompany(false);

	if (CheckSavegameVersion(72)) {
		/* Locks/shiplifts in very old savegames had OWNER_WATER as owner */
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
					if (       IsInsideMM(gfx,   0,   8)) { // Railway station
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
					} else if (gfx == 75) {                    // Oil rig
						st = STATION_OILRIG;
						SetStationGfx(t, gfx - 75);
					} else if (IsInsideMM(gfx,  76,  82)) { // Dock
						st = STATION_DOCK;
						SetStationGfx(t, gfx - 76);
					} else if (gfx == 82) {                    // Buoy
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
						signal(SIGSEGV, prev_segfault);
						signal(SIGABRT, prev_abort);
						return false;
					}
					SB(_m[t].m6, 3, 3, st);
				} break;
			}
		}
	}

	for (TileIndex t = 0; t < map_size; t++) {
		switch (GetTileType(t)) {
			case MP_STATION: {
				Station *st = GetStationByTile(t);

				/* Set up station spread; buoys do not have one */
				if (!IsBuoy(t)) st->rect.BeforeAddTile(t, StationRect::ADD_FORCE);

				switch (GetStationType(t)) {
					case STATION_TRUCK:
					case STATION_BUS:
						if (CheckSavegameVersion(6)) {
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
							GetStationByTile(t)->airport_type = AT_OILRIG;
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
	if (CheckSavegameVersionOldStyle(2, 2)) UpdateOldAircraft();

	/* In version 6.1 we put the town index in the map-array. To do this, we need
	 *  to use m2 (16bit big), so we need to clean m2, and that is where this is
	 *  all about ;) */
	if (CheckSavegameVersionOldStyle(6, 1)) {
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
	if (CheckSavegameVersion(111)) {
		_settings_game.construction.freeform_edges = false;
	}

	/* From version 9.0, we update the max passengers of a town (was sometimes negative
	 *  before that. */
	if (CheckSavegameVersion(9)) {
		Town *t;
		FOR_ALL_TOWNS(t) UpdateTownMaxPass(t);
	}

	/* From version 16.0, we included autorenew on engines, which are now saved, but
	 *  of course, we do need to initialize them for older savegames. */
	if (CheckSavegameVersion(16)) {
		FOR_ALL_COMPANIES(c) {
			c->engine_renew_list   = NULL;
			c->engine_renew        = false;
			c->engine_renew_months = -6;
			c->engine_renew_money  = 100000;
		}

		/* When loading a game, _local_company is not yet set to the correct value.
		 * However, in a dedicated server we are a spectator, so nothing needs to
		 * happen. In case we are not a dedicated server, the local company always
		 * becomes company 0, unless we are in the scenario editor where all the
		 * companies are 'invalid'.
		 */
		if (!_network_dedicated && IsValidCompanyID(COMPANY_FIRST)) {
			c = GetCompany(COMPANY_FIRST);
			c->engine_renew        = _settings_client.gui.autorenew;
			c->engine_renew_months = _settings_client.gui.autorenew_months;
			c->engine_renew_money  = _settings_client.gui.autorenew_money;
		}
	}

	if (CheckSavegameVersion(48)) {
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					if (IsPlainRailTile(t)) {
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

	if (CheckSavegameVersion(61)) {
		/* Added the RoadType */
		bool old_bridge = CheckSavegameVersion(42);
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_ROAD:
					SB(_m[t].m5, 6, 2, GB(_m[t].m5, 4, 2));
					switch (GetRoadTileType(t)) {
						default: NOT_REACHED();
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

	if (CheckSavegameVersion(114)) {
		bool fix_roadtypes = !CheckSavegameVersion(61);
		bool old_bridge = CheckSavegameVersion(42);

		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_ROAD:
					if (fix_roadtypes) SetRoadTypes(t, (RoadTypes)GB(_me[t].m7, 5, 3));
					SB(_me[t].m7, 5, 1, GB(_m[t].m3, 7, 1)); // snow/desert
					switch (GetRoadTileType(t)) {
						default: NOT_REACHED();
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
					if (!HasTownOwnedRoad(t)) {
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

	if (CheckSavegameVersion(42)) {
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
									MakeWater(t);
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
			if (v->type != VEH_TRAIN && v->type != VEH_ROAD) continue;
			if (IsBridgeTile(v->tile)) {
				DiagDirection dir = GetTunnelBridgeDirection(v->tile);

				if (dir != DirToDiagDir(v->direction)) continue;
				switch (dir) {
					default: NOT_REACHED();
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
				v->u.rail.track = TRACK_BIT_WORMHOLE;
			} else {
				v->u.road.state = RVSB_WORMHOLE;
			}
		}
	}

	/* Elrails got added in rev 24 */
	if (CheckSavegameVersion(24)) {
		Vehicle *v;
		RailType min_rail = RAILTYPE_ELECTRIC;

		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_TRAIN) {
				RailType rt = RailVehInfo(v->engine_type)->railtype;

				v->u.rail.railtype = rt;
				if (rt == RAILTYPE_ELECTRIC) min_rail = RAILTYPE_RAIL;
			}
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
					if (IsRailwayStation(t)) {
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

		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_TRAIN && (IsFrontEngine(v) || IsFreeWagon(v))) TrainConsistChanged(v, true);
		}

	}

	/* In version 16.1 of the savegame a company can decide if trains, which get
	 * replaced, shall keep their old length. In all prior versions, just default
	 * to false */
	if (CheckSavegameVersionOldStyle(16, 1)) {
		FOR_ALL_COMPANIES(c) c->renew_keep_length = false;
	}

	/* In version 17, ground type is moved from m2 to m4 for depots and
	 * waypoints to make way for storing the index in m2. The custom graphics
	 * id which was stored in m4 is now saved as a grf/id reference in the
	 * waypoint struct. */
	if (CheckSavegameVersion(17)) {
		Waypoint *wp;

		FOR_ALL_WAYPOINTS(wp) {
			if (wp->deleted == 0) {
				const StationSpec *statspec = NULL;

				if (HasBit(_m[wp->xy].m3, 4))
					statspec = GetCustomStationSpec(STAT_CLASS_WAYP, _m[wp->xy].m4 + 1);

				if (statspec != NULL) {
					wp->stat_id = _m[wp->xy].m4 + 1;
					wp->grfid = statspec->grffile->grfid;
					wp->localidx = statspec->localidx;
				} else {
					/* No custom graphics set, so set to default. */
					wp->stat_id = 0;
					wp->grfid = 0;
					wp->localidx = 0;
				}

				/* Move ground type bits from m2 to m4. */
				_m[wp->xy].m4 = GB(_m[wp->xy].m2, 0, 4);
				/* Store waypoint index in the tile. */
				_m[wp->xy].m2 = wp->index;
			}
		}
	} else {
		/* As of version 17, we recalculate the custom graphic ID of waypoints
		 * from the GRF ID / station index. */
		AfterLoadWaypoints();
	}

	/* From version 15, we moved a semaphore bit from bit 2 to bit 3 in m4, making
	 *  room for PBS. Now in version 21 move it back :P. */
	if (CheckSavegameVersion(21) && !CheckSavegameVersion(15)) {
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

	if (CheckSavegameVersion(25)) {
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_ROAD) {
				v->vehstatus &= ~0x40;
				v->u.road.slot = NULL;
				v->u.road.slot_age = 0;
			}
		}
	} else {
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_ROAD && v->u.road.slot != NULL) v->u.road.slot->num_vehicles++;
		}
	}

	if (CheckSavegameVersion(26)) {
		Station *st;
		FOR_ALL_STATIONS(st) {
			st->last_vehicle_type = VEH_INVALID;
		}
	}

	YapfNotifyTrackLayoutChange(INVALID_TILE, INVALID_TRACK);

	if (CheckSavegameVersion(34)) FOR_ALL_COMPANIES(c) ResetCompanyLivery(c);

	FOR_ALL_COMPANIES(c) {
		c->avail_railtypes = GetCompanyRailtypes(c->index);
		c->avail_roadtypes = GetCompanyRoadtypes(c->index);
	}

	if (!CheckSavegameVersion(27)) AfterLoadStations();

	/* Time starts at 0 instead of 1920.
	 * Account for this in older games by adding an offset */
	if (CheckSavegameVersion(31)) {
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
	if (CheckSavegameVersion(32)) {
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
	if (CheckSavegameVersion(36)) {
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
	if (CheckSavegameVersion(38)) _settings_game.vehicle.disable_elrails = false;
	/* do the same as when elrails were enabled/disabled manually just now */
	SettingsDisableElrail(_settings_game.vehicle.disable_elrails);
	InitializeRailGUI();

	/* From version 53, the map array was changed for house tiles to allow
	 * space for newhouses grf features. A new byte, m7, was also added. */
	if (CheckSavegameVersion(53)) {
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

	if (CheckSavegameVersion(43)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_INDUSTRY)) {
				switch (GetIndustryGfx(t)) {
					case GFX_POWERPLANT_SPARKS:
						SetIndustryAnimationState(t, GB(_m[t].m1, 2, 5));
						break;

					case GFX_OILWELL_ANIMATED_1:
					case GFX_OILWELL_ANIMATED_2:
					case GFX_OILWELL_ANIMATED_3:
						SetIndustryAnimationState(t, GB(_m[t].m1, 0, 2));
						break;

					case GFX_COAL_MINE_TOWER_ANIMATED:
					case GFX_COPPER_MINE_TOWER_ANIMATED:
					case GFX_GOLD_MINE_TOWER_ANIMATED:
						 SetIndustryAnimationState(t, _m[t].m1);
						 break;

					default: // No animation states to change
						break;
				}
			}
		}
	}

	if (CheckSavegameVersion(44)) {
		Vehicle *v;
		/* If we remove a station while cargo from it is still enroute, payment calculation will assume
		 * 0, 0 to be the source of the cargo, resulting in very high payments usually. v->source_xy
		 * stores the coordinates, preserving them even if the station is removed. However, if a game is loaded
		 * where this situation exists, the cargo-source information is lost. in this case, we set the source
		 * to the current tile of the vehicle to prevent excessive profits
		 */
		FOR_ALL_VEHICLES(v) {
			const CargoList::List *packets = v->cargo.Packets();
			for (CargoList::List::const_iterator it = packets->begin(); it != packets->end(); it++) {
				CargoPacket *cp = *it;
				cp->source_xy = IsValidStationID(cp->source) ? GetStation(cp->source)->xy : v->tile;
				cp->loaded_at_xy = cp->source_xy;
			}
			v->cargo.InvalidateCache();
		}

		/* Store position of the station where the goods come from, so there
		 * are no very high payments when stations get removed. However, if the
		 * station where the goods came from is already removed, the source
		 * information is lost. In that case we set it to the position of this
		 * station */
		Station *st;
		FOR_ALL_STATIONS(st) {
			for (CargoID c = 0; c < NUM_CARGO; c++) {
				GoodsEntry *ge = &st->goods[c];

				const CargoList::List *packets = ge->cargo.Packets();
				for (CargoList::List::const_iterator it = packets->begin(); it != packets->end(); it++) {
					CargoPacket *cp = *it;
					cp->source_xy = IsValidStationID(cp->source) ? GetStation(cp->source)->xy : st->xy;
					cp->loaded_at_xy = cp->source_xy;
				}
			}
		}
	}

	if (CheckSavegameVersion(45)) {
		Vehicle *v;
		/* Originally just the fact that some cargo had been paid for was
		 * stored to stop people cheating and cashing in several times. This
		 * wasn't enough though as it was cleared when the vehicle started
		 * loading again, even if it didn't actually load anything, so now the
		 * amount of cargo that has been paid for is stored. */
		FOR_ALL_VEHICLES(v) {
			const CargoList::List *packets = v->cargo.Packets();
			for (CargoList::List::const_iterator it = packets->begin(); it != packets->end(); it++) {
				CargoPacket *cp = *it;
				cp->paid_for = HasBit(v->vehicle_flags, 2);
			}
			ClrBit(v->vehicle_flags, 2);
			v->cargo.InvalidateCache();
		}
	}

	/* Buoys do now store the owner of the previous water tile, which can never
	 * be OWNER_NONE. So replace OWNER_NONE with OWNER_WATER. */
	if (CheckSavegameVersion(46)) {
		Station *st;
		FOR_ALL_STATIONS(st) {
			if (st->IsBuoy() && IsTileOwner(st->xy, OWNER_NONE) && TileHeight(st->xy) == 0) SetTileOwner(st->xy, OWNER_WATER);
		}
	}

	if (CheckSavegameVersion(50)) {
		Vehicle *v;
		/* Aircraft units changed from 8 mph to 1 km/h */
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_AIRCRAFT && v->subtype <= AIR_AIRCRAFT) {
				const AircraftVehicleInfo *avi = AircraftVehInfo(v->engine_type);
				v->cur_speed *= 129;
				v->cur_speed /= 10;
				v->max_speed = avi->max_speed;
				v->acceleration = avi->acceleration;
			}
		}
	}

	if (CheckSavegameVersion(49)) FOR_ALL_COMPANIES(c) c->face = ConvertFromOldCompanyManagerFace(c->face);

	if (CheckSavegameVersion(52)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsStatueTile(t)) {
				_m[t].m2 = CalcClosestTownFromTile(t)->index;
			}
		}
	}

	/* A setting containing the proportion of towns that grow twice as
	 * fast was added in version 54. From version 56 this is now saved in the
	 * town as cities can be built specifically in the scenario editor. */
	if (CheckSavegameVersion(56)) {
		Town *t;

		FOR_ALL_TOWNS(t) {
			if (_settings_game.economy.larger_towns != 0 && (t->index % _settings_game.economy.larger_towns) == 0) {
				t->larger_town = true;
			}
		}
	}

	if (CheckSavegameVersion(57)) {
		Vehicle *v;
		/* Added a FIFO queue of vehicles loading at stations */
		FOR_ALL_VEHICLES(v) {
			if ((v->type != VEH_TRAIN || IsFrontEngine(v)) &&  // for all locs
					!(v->vehstatus & (VS_STOPPED | VS_CRASHED)) && // not stopped or crashed
					v->current_order.IsType(OT_LOADING)) {         // loading
				GetStation(v->last_station_visited)->loading_vehicles.push_back(v);

				/* The loading finished flag is *only* set when actually completely
				 * finished. Because the vehicle is loading, it is not finished. */
				ClrBit(v->vehicle_flags, VF_LOADING_FINISHED);
			}
		}
	} else if (CheckSavegameVersion(59)) {
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

	if (CheckSavegameVersion(58)) {
		/* Setting difficulty number_industries other than zero get bumped to +1
		 * since a new option (very low at position1) has been added */
		if (_settings_game.difficulty.number_industries > 0) {
			_settings_game.difficulty.number_industries++;
		}

		/* Same goes for number of towns, although no test is needed, just an increment */
		_settings_game.difficulty.number_towns++;
	}

	if (CheckSavegameVersion(64)) {
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

	if (CheckSavegameVersion(69)) {
		/* In some old savegames a bit was cleared when it should not be cleared */
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_ROAD && (v->u.road.state == 250 || v->u.road.state == 251)) {
				SetBit(v->u.road.state, RVS_IS_STOPPING);
			}
		}
	}

	if (CheckSavegameVersion(70)) {
		/* Added variables to support newindustries */
		Industry *i;
		FOR_ALL_INDUSTRIES(i) i->founder = OWNER_NONE;
	}

	/* From version 82, old style canals (above sealevel (0), WATER owner) are no longer supported.
	    Replace the owner for those by OWNER_NONE. */
	if (CheckSavegameVersion(82)) {
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
	if (CheckSavegameVersion(83)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_WATER) && IsShipDepot(t)) {
				_m[t].m4 = (TileHeight(t) == 0) ? OWNER_WATER : OWNER_NONE;
			}
		}
	}

	if (CheckSavegameVersion(74)) {
		Station *st;
		FOR_ALL_STATIONS(st) {
			for (CargoID c = 0; c < NUM_CARGO; c++) {
				st->goods[c].last_speed = 0;
				if (st->goods[c].cargo.Count() != 0) SetBit(st->goods[c].acceptance_pickup, GoodsEntry::PICKUP);
			}
		}
	}

	if (CheckSavegameVersion(78)) {
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
	if (CheckSavegameVersion(81)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (GetTileType(t) == MP_TREES) {
				TreeGround groundType = GetTreeGround(t);
				if (groundType != TREE_GROUND_SNOW_DESERT) SetTreeGroundDensity(t, groundType, 3);
			}
		}
	}


	if (CheckSavegameVersion(93)) {
		/* Rework of orders. */
		Order *order;
		FOR_ALL_ORDERS(order) order->ConvertFromOldSavegame();

		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->orders.list != NULL && v->orders.list->GetFirstOrder() != NULL && !v->orders.list->GetFirstOrder()->IsValid()) {
				v->orders.list->FreeChain();
				v->orders.list = NULL;
			}

			v->current_order.ConvertFromOldSavegame();
			if (v->type == VEH_ROAD && v->IsPrimaryVehicle() && v->FirstShared() == v) {
				FOR_VEHICLE_ORDERS(v, order) order->SetNonStopType(ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS);
			}
		}
	} else if (CheckSavegameVersion(94)) {
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

	if (CheckSavegameVersion(84)) {
		/* Update go to buoy orders because they are just waypoints */
		Order *order;
		FOR_ALL_ORDERS(order) {
			if (order->IsType(OT_GOTO_STATION) && GetStation(order->GetDestination())->IsBuoy()) {
				order->SetLoadType(OLF_LOAD_IF_POSSIBLE);
				order->SetUnloadType(OUF_UNLOAD_IF_POSSIBLE);
			}
		}

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
				if (!IsValidCompanyID(company) || company == c->index) c->share_owners[i] = INVALID_COMPANY;
			}
		}
	}

	if (CheckSavegameVersion(86)) {
		for (TileIndex t = 0; t < map_size; t++) {
			/* Move river flag and update canals to use water class */
			if (IsTileType(t, MP_WATER)) {
				if (GetWaterClass(t) != WATER_CLASS_RIVER) {
					if (IsWater(t)) {
						Owner o = GetTileOwner(t);
						if (o == OWNER_WATER) {
							MakeWater(t);
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

	if (CheckSavegameVersion(87)) {
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
				if (o < MAX_COMPANIES && !IsValidCompanyID(o)) {
					_current_company = o;
					ChangeTileOwner(t, o, INVALID_OWNER);
				}
				if (IsBuoyTile(t)) {
					/* reset buoy owner to OWNER_NONE in the station struct
					 * (even if it is owned by active company) */
					GetStationByTile(t)->owner = OWNER_NONE;
				}
			} else if (IsTileType(t, MP_ROAD)) {
				/* works for all RoadTileType */
				for (RoadType rt = ROADTYPE_ROAD; rt < ROADTYPE_END; rt++) {
					/* update even non-existing road types to update tile owner too */
					Owner o = GetRoadOwner(t, rt);
					if (o < MAX_COMPANIES && !IsValidCompanyID(o)) SetRoadOwner(t, rt, OWNER_NONE);
				}
				if (IsLevelCrossing(t)) {
					if (!IsValidCompanyID(GetTileOwner(t))) FixOwnerOfRailTrack(t);
				}
			} else if (IsTileType(t, MP_RAILWAY) && IsPlainRailTile(t)) {
				if (!IsValidCompanyID(GetTileOwner(t))) FixOwnerOfRailTrack(t);
			}
		}

		/* Convert old PF settings to new */
		if (_settings_game.pf.yapf.rail_use_yapf || CheckSavegameVersion(28)) {
			_settings_game.pf.pathfinder_for_trains = VPF_YAPF;
		} else {
			_settings_game.pf.pathfinder_for_trains = (_settings_game.pf.new_pathfinding_all ? VPF_NPF : VPF_NTP);
		}

		if (_settings_game.pf.yapf.road_use_yapf || CheckSavegameVersion(28)) {
			_settings_game.pf.pathfinder_for_roadvehs = VPF_YAPF;
		} else {
			_settings_game.pf.pathfinder_for_roadvehs = (_settings_game.pf.new_pathfinding_all ? VPF_NPF : VPF_OPF);
		}

		if (_settings_game.pf.yapf.ship_use_yapf) {
			_settings_game.pf.pathfinder_for_ships = VPF_YAPF;
		} else {
			_settings_game.pf.pathfinder_for_ships = (_settings_game.pf.new_pathfinding_all ? VPF_NPF : VPF_OPF);
		}
	}

	if (CheckSavegameVersion(88)) {
		/* Profits are now with 8 bit fract */
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			v->profit_this_year <<= 8;
			v->profit_last_year <<= 8;
			v->running_ticks = 0;
		}
	}

	if (CheckSavegameVersion(91)) {
		/* Increase HouseAnimationFrame from 5 to 7 bits */
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_HOUSE) && GetHouseType(t) >= NEW_HOUSE_OFFSET) {
				SetHouseAnimationFrame(t, GB(_m[t].m6, 3, 5));
			}
		}
	}

	if (CheckSavegameVersion(62)) {
		/* Remove all trams from savegames without tram support.
		 * There would be trams without tram track under causing crashes sooner or later. */
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_ROAD && v->First() == v &&
					HasBit(EngInfo(v->engine_type)->misc_flags, EF_ROAD_TRAM)) {
				if (_switch_mode_errorstr == INVALID_STRING_ID || _switch_mode_errorstr == STR_NEWGRF_COMPATIBLE_LOAD_WARNING) {
					_switch_mode_errorstr = STR_LOADGAME_REMOVED_TRAMS;
				}
				delete v;
			}
		}
	}

	if (CheckSavegameVersion(99)) {
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
	if (CheckSavegameVersion(100)) {
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
					if (IsRailDepot(t) ||IsRailWaypoint(t)) {
						SetDepotWaypointReservation(t, false);
					} else {
						SetTrackReservation(t, TRACK_BIT_NONE);
					}
					break;

				case MP_ROAD: // Clear PBS reservation on crossing
					if (IsLevelCrossing(t)) SetCrossingReservation(t, false);
					break;

				case MP_STATION: // Clear PBS reservation on station
					if (IsRailwayStation(t)) SetRailwayStationReservation(t, false);
					break;

				case MP_TUNNELBRIDGE: // Clear PBS reservation on tunnels/birdges
					if (GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL) SetTunnelBridgeReservation(t, false);
					break;

				default: break;
			}
		}
	}

	/* Reserve all tracks trains are currently on. */
	if (CheckSavegameVersion(101)) {
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_TRAIN) {
				if ((v->u.rail.track & TRACK_BIT_WORMHOLE) == TRACK_BIT_WORMHOLE) {
					TryReserveRailTrack(v->tile, DiagDirToDiagTrack(GetTunnelBridgeDirection(v->tile)));
				} else if ((v->u.rail.track & TRACK_BIT_MASK) != TRACK_BIT_NONE) {
					TryReserveRailTrack(v->tile, TrackBitsToTrack(v->u.rail.track));
				}
			}
		}
	}

	if (CheckSavegameVersion(102)) {
		for (TileIndex t = 0; t < map_size; t++) {
			/* Now all crossings should be in correct state */
			if (IsLevelCrossingTile(t)) UpdateLevelCrossing(t, false);
		}
	}

	if (CheckSavegameVersion(103)) {
		/* Non-town-owned roads now store the closest town */
		UpdateNearestTownForRoadTiles(false);

		/* signs with invalid owner left from older savegames */
		Sign *si;
		FOR_ALL_SIGNS(si) {
			if (si->owner != OWNER_NONE && !IsValidCompanyID(si->owner)) si->owner = OWNER_NONE;
		}

		/* Station can get named based on an industry type, but the current ones
		 * are not, so mark them as if they are not named by an industry. */
		Station *st;
		FOR_ALL_STATIONS(st) {
			st->indtype = IT_INVALID;
		}
	}

	if (CheckSavegameVersion(104)) {
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			/* Set engine_type of shadow and rotor */
			if (v->type == VEH_AIRCRAFT && !IsNormalAircraft(v)) {
				v->engine_type = v->First()->engine_type;
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

	if (CheckSavegameVersion(112)) {
		for (TileIndex t = 0; t < map_size; t++) {
			/* Check for HQ bit being set, instead of using map accessor,
			 * since we've already changed it code-wise */
			if (IsTileType(t, MP_UNMOVABLE) && HasBit(_m[t].m5, 7)) {
				/* Move size and part identification of HQ out of the m5 attribute,
				 * on new locations */
				uint8 old_m5 = _m[t].m5;
				_m[t].m5 = UNMOVABLE_HQ;
				SetCompanyHQSize(t, GB(old_m5, 2, 3));
				SetCompanyHQSection(t, GB(old_m5, 0, 2));
			}
		}
	}

	if (CheckSavegameVersion(113)) {
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

	if (CheckSavegameVersion(114)) {
		/* There could be (deleted) stations with invalid owner, set owner to OWNER NONE.
		 * The conversion affects oil rigs and buoys too, but it doesn't matter as
		 * they have st->owner == OWNER_NONE already. */
		Station *st;
		FOR_ALL_STATIONS(st) {
			if (!IsValidCompanyID(st->owner)) st->owner = OWNER_NONE;
		}

		/* Give owners to waypoints, based on rail tracks it is sitting on.
		 * If none is available, specify OWNER_NONE.
		 * This code was in CheckSavegameVersion(101) in the past, but in some cases,
		 * the owner of waypoints could be incorrect. */
		Waypoint *wp;
		FOR_ALL_WAYPOINTS(wp) {
			Owner owner = IsTileType(wp->xy, MP_RAILWAY) ? GetTileOwner(wp->xy) : OWNER_NONE;
			wp->owner = IsValidCompanyID(owner) ? owner : OWNER_NONE;
		}
	}

	GamelogPrintDebug(1);

	bool ret = InitializeWindowsAndCaches();
	/* Restore the signals */
	signal(SIGSEGV, prev_segfault);
	signal(SIGABRT, prev_abort);
	return ret;
}

/** Reload all NewGRF files during a running game. This is a cut-down
 * version of AfterLoadGame().
 * XXX - We need to reset the vehicle position hash because with a non-empty
 * hash AfterLoadVehicles() will loop infinitely. We need AfterLoadVehicles()
 * to recalculate vehicle data as some NewGRF vehicle sets could have been
 * removed or added and changed statistics */
void ReloadNewGRFData()
{
	/* reload grf data */
	GfxLoadSprites();
	LoadStringWidthTable();
	ResetEconomy();
	/* reload vehicles */
	ResetVehiclePosHash();
	AfterLoadVehicles(false);
	StartupEngines();
	SetCachedEngineCounts();
	/* update station and waypoint graphics */
	AfterLoadWaypoints();
	AfterLoadStations();
	/* Check and update house and town values */
	UpdateHousesAndTowns();
	/* Update livery selection windows */
	for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) InvalidateWindowData(WC_COMPANY_COLOUR, i, _loaded_newgrf_features.has_2CC);
	/* redraw the whole screen */
	MarkWholeScreenDirty();
	CheckTrainsLengths();
}
