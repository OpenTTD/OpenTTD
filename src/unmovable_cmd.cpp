/* $Id$ */

/** @file unmovable_cmd.cpp Handling of unmovable tiles. */

#include "stdafx.h"
#include "openttd.h"
#include "landscape.h"
#include "command_func.h"
#include "viewport_func.h"
#include "company_base.h"
#include "town.h"
#include "sprite.h"
#include "bridge_map.h"
#include "unmovable_map.h"
#include "genworld.h"
#include "autoslope.h"
#include "transparency.h"
#include "functions.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "company_gui.h"
#include "economy_func.h"
#include "cheat_type.h"
#include "landscape_type.h"
#include "unmovable.h"

#include "table/strings.h"
#include "table/sprites.h"
#include "table/unmovable_land.h"

/**
 * Accessor for array _original_unmovable.
 * This will ensure at once : proper access and
 * not allowing modifications of it.
 * @param type of unmovable (which is the index in _original_unmovable)
 * @pre type < UNMOVABLE_MAX
 * @return a pointer to the corresponding unmovable spec
 **/
static inline const UnmovableSpec *GetUnmovableSpec(UnmovableType type)
{
	assert(type < UNMOVABLE_MAX);
	return &_original_unmovable[type];
}

/** Destroy a HQ.
 * During normal gameplay you can only implicitely destroy a HQ when you are
 * rebuilding it. Otherwise, only water can destroy it.
 * @param cid Company requesting the destruction of his HQ
 * @param flags docommand flags of calling function
 * @return cost of the operation
 */
static CommandCost DestroyCompanyHQ(CompanyID cid, DoCommandFlag flags)
{
	Company *c = GetCompany(cid);

	if (flags & DC_EXEC) {
		TileIndex t = c->location_of_HQ;

		DoClearSquare(t);
		DoClearSquare(t + TileDiffXY(0, 1));
		DoClearSquare(t + TileDiffXY(1, 0));
		DoClearSquare(t + TileDiffXY(1, 1));
		c->location_of_HQ = INVALID_TILE; // reset HQ position
		InvalidateWindow(WC_COMPANY, cid);
	}

	/* cost of relocating company is 1% of company value */
	return CommandCost(EXPENSES_PROPERTY, CalculateCompanyValue(c) / 100);
}

void UpdateCompanyHQ(Company *c, uint score)
{
	byte val;
	TileIndex tile = c->location_of_HQ;

	if (tile == INVALID_TILE) return;

	(val = 0, score < 170) ||
	(val++, score < 350) ||
	(val++, score < 520) ||
	(val++, score < 720) ||
	(val++, true);

	EnlargeCompanyHQ(tile, val);

	MarkTileDirtyByTile(tile);
	MarkTileDirtyByTile(tile + TileDiffXY(0, 1));
	MarkTileDirtyByTile(tile + TileDiffXY(1, 0));
	MarkTileDirtyByTile(tile + TileDiffXY(1, 1));
}

extern CommandCost CheckFlatLandBelow(TileIndex tile, uint w, uint h, DoCommandFlag flags, uint invalid_dirs, StationID *station, bool check_clear = true);

/** Build or relocate the HQ. This depends if the HQ is already built or not
 * @param tile tile where the HQ will be built or relocated to
 * @param flags type of operation
 * @param p1 unused
 * @param p2 unused
 */
CommandCost CmdBuildCompanyHQ(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Company *c = GetCompany(_current_company);
	CommandCost cost(EXPENSES_PROPERTY);

	cost = CheckFlatLandBelow(tile, 2, 2, flags, 0, NULL);
	if (CmdFailed(cost)) return cost;

	if (c->location_of_HQ != INVALID_TILE) { // Moving HQ
		cost.AddCost(DestroyCompanyHQ(_current_company, flags));
	}

	if (flags & DC_EXEC) {
		int score = UpdateCompanyRatingAndValue(c, false);

		c->location_of_HQ = tile;

		MakeCompanyHQ(tile, _current_company);

		UpdateCompanyHQ(c, score);
		InvalidateWindow(WC_COMPANY, c->index);
	}

	return cost;
}

/** Purchase a land area. Actually you only purchase one tile, so
 * the name is a bit confusing ;p
 * @param tile the tile the company is purchasing
 * @param flags for this command type
 * @param p1 unused
 * @param p2 unused
 * @return error of cost of operation
 */
CommandCost CmdPurchaseLandArea(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	CommandCost cost(EXPENSES_CONSTRUCTION);

	if (IsOwnedLandTile(tile) && IsTileOwner(tile, _current_company)) {
		return_cmd_error(STR_5807_YOU_ALREADY_OWN_IT);
	}

	cost = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(cost)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		MakeOwnedLand(tile, _current_company);
		MarkTileDirtyByTile(tile);
	}

	return cost.AddCost(GetUnmovableSpec(UNMOVABLE_OWNED_LAND)->GetBuildingCost());
}

/** Sell a land area. Actually you only sell one tile, so
 * the name is a bit confusing ;p
 * @param tile the tile the company is selling
 * @param flags for this command type
 * @param p1 unused
 * @param p2 unused
 * @return error or cost of operation
 */
CommandCost CmdSellLandArea(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (!IsOwnedLandTile(tile)) return CMD_ERROR;
	if (!CheckTileOwnership(tile) && _current_company != OWNER_WATER) return CMD_ERROR;

	if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

	if (flags & DC_EXEC) DoClearSquare(tile);

	return CommandCost(EXPENSES_CONSTRUCTION, - GetUnmovableSpec(UNMOVABLE_OWNED_LAND)->GetRemovalCost());
}

static Foundation GetFoundation_Unmovable(TileIndex tile, Slope tileh);

static void DrawTile_Unmovable(TileInfo *ti)
{

	switch (GetUnmovableType(ti->tile)) {
		default: NOT_REACHED(); break;
		case UNMOVABLE_TRANSMITTER:
		case UNMOVABLE_LIGHTHOUSE: {
			const DrawTileSeqStruct *dtu = &_draw_tile_transmitterlighthouse_data[GetUnmovableType(ti->tile)];

			if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, FOUNDATION_LEVELED);
			DrawClearLandTile(ti, 2);

			if (IsInvisibilitySet(TO_STRUCTURES)) break;

			AddSortableSpriteToDraw(
				dtu->image.sprite, PAL_NONE, ti->x | dtu->delta_x, ti->y | dtu->delta_y,
				dtu->size_x, dtu->size_y, dtu->size_z, ti->z,
				IsTransparencySet(TO_STRUCTURES)
			);
			break;
		}

		case UNMOVABLE_STATUE:
			/* This should prevent statues from sinking into the ground when on a slope. */
			if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, GetFoundation_Unmovable(ti->tile, ti->tileh));

			DrawGroundSprite(SPR_CONCRETE_GROUND, PAL_NONE);

			if (IsInvisibilitySet(TO_STRUCTURES)) break;

			AddSortableSpriteToDraw(SPR_STATUE_COMPANY, COMPANY_SPRITE_COLOUR(GetTileOwner(ti->tile)), ti->x, ti->y, 16, 16, 25, ti->z, IsTransparencySet(TO_STRUCTURES));
			break;

		case UNMOVABLE_OWNED_LAND:
			DrawClearLandTile(ti, 0);

			AddSortableSpriteToDraw(
				SPR_BOUGHT_LAND, COMPANY_SPRITE_COLOUR(GetTileOwner(ti->tile)),
				ti->x + TILE_SIZE / 2, ti->y + TILE_SIZE / 2, 1, 1, BB_HEIGHT_UNDER_BRIDGE, GetSlopeZ(ti->x + TILE_SIZE / 2, ti->y + TILE_SIZE / 2)
			);
			DrawBridgeMiddle(ti);
			break;

		case UNMOVABLE_HQ:{
			assert(IsCompanyHQ(ti->tile));
			if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, FOUNDATION_LEVELED);

			SpriteID palette = COMPANY_SPRITE_COLOUR(GetTileOwner(ti->tile));

			const DrawTileSprites *t = &_unmovable_display_datas[GetCompanyHQSize(ti->tile) << 2 | GetCompanyHQSection(ti->tile)];
			DrawGroundSprite(t->ground.sprite, palette);

			if (IsInvisibilitySet(TO_STRUCTURES)) break;

			const DrawTileSeqStruct *dtss;
			foreach_draw_tile_seq(dtss, t->seq) {
				AddSortableSpriteToDraw(
					dtss->image.sprite, palette,
					ti->x + dtss->delta_x, ti->y + dtss->delta_y,
					dtss->size_x, dtss->size_y,
					dtss->size_z, ti->z + dtss->delta_z,
					IsTransparencySet(TO_STRUCTURES)
				);
			}
			break;
		}
	}
}

static uint GetSlopeZ_Unmovable(TileIndex tile, uint x, uint y)
{
	if (IsOwnedLand(tile)) {
		uint z;
		Slope tileh = GetTileSlope(tile, &z);

		return z + GetPartialZ(x & 0xF, y & 0xF, tileh);
	} else {
		return GetTileMaxZ(tile);
	}
}

static Foundation GetFoundation_Unmovable(TileIndex tile, Slope tileh)
{
	return IsOwnedLand(tile) ? FOUNDATION_NONE : FlatteningFoundation(tileh);
}

static CommandCost ClearTile_Unmovable(TileIndex tile, DoCommandFlag flags)
{
	if (IsCompanyHQ(tile)) {
		if (_current_company == OWNER_WATER) {
			return DestroyCompanyHQ(GetTileOwner(tile), DC_EXEC);
		} else {
			return_cmd_error(flags & DC_AUTO ? STR_5804_COMPANY_HEADQUARTERS_IN : INVALID_STRING_ID);
		}
	}

	if (IsOwnedLand(tile)) {
		return DoCommand(tile, 0, 0, flags, CMD_SELL_LAND_AREA);
	}

	/* checks if you're allowed to remove unmovable things */
	if (_game_mode != GM_EDITOR && _current_company != OWNER_WATER && ((flags & DC_AUTO || !_cheats.magic_bulldozer.value)) )
		return_cmd_error(flags & DC_AUTO ? STR_5800_OBJECT_IN_THE_WAY : INVALID_STRING_ID);

	if (IsStatue(tile)) {
		if (flags & DC_AUTO) return_cmd_error(STR_5800_OBJECT_IN_THE_WAY);

		TownID town = GetStatueTownID(tile);
		ClrBit(GetTown(town)->statues, GetTileOwner(tile));
		InvalidateWindow(WC_TOWN_AUTHORITY, town);
	}

	if (flags & DC_EXEC) {
		DoClearSquare(tile);
	}

	return CommandCost();
}

static void GetAcceptedCargo_Unmovable(TileIndex tile, AcceptedCargo ac)
{
	if (!IsCompanyHQ(tile)) return;

	/* HQ accepts passenger and mail; but we have to divide the values
	 * between 4 tiles it occupies! */

	/* HQ level (depends on company performance) in the range 1..5. */
	uint level = GetCompanyHQSize(tile) + 1;

	/* Top town building generates 10, so to make HQ interesting, the top
	 * type makes 20. */
	ac[CT_PASSENGERS] = max(1U, level);

	/* Top town building generates 4, HQ can make up to 8. The
	 * proportion passengers:mail is different because such a huge
	 * commercial building generates unusually high amount of mail
	 * correspondence per physical visitor. */
	ac[CT_MAIL] = max(1U, level / 2);
}


static void GetTileDesc_Unmovable(TileIndex tile, TileDesc *td)
{
	td->str = GetUnmovableSpec(GetUnmovableType(tile))->name;
	td->owner[0] = GetTileOwner(tile);
}

static void AnimateTile_Unmovable(TileIndex tile)
{
	/* not used */
}

static void TileLoop_Unmovable(TileIndex tile)
{
	if (!IsCompanyHQ(tile)) return;

	/* HQ accepts passenger and mail; but we have to divide the values
	 * between 4 tiles it occupies! */

	/* HQ level (depends on company performance) in the range 1..5. */
	uint level = GetCompanyHQSize(tile) + 1;
	assert(level < 6);

	uint r = Random();
	/* Top town buildings generate 250, so the top HQ type makes 256. */
	if (GB(r, 0, 8) < (256 / 4 / (6 - level))) {
		uint amt = GB(r, 0, 8) / 8 / 4 + 1;
		if (_economy.fluct <= 0) amt = (amt + 1) >> 1;
		MoveGoodsToStation(tile, 2, 2, CT_PASSENGERS, amt);
	}

	/* Top town building generates 90, HQ can make up to 196. The
	 * proportion passengers:mail is about the same as in the acceptance
	 * equations. */
	if (GB(r, 8, 8) < (196 / 4 / (6 - level))) {
		uint amt = GB(r, 8, 8) / 8 / 4 + 1;
		if (_economy.fluct <= 0) amt = (amt + 1) >> 1;
		MoveGoodsToStation(tile, 2, 2, CT_MAIL, amt);
	}
}


static TrackStatus GetTileTrackStatus_Unmovable(TileIndex tile, TransportType mode, uint sub_mode, DiagDirection side)
{
	return 0;
}

static bool ClickTile_Unmovable(TileIndex tile)
{
	if (!IsCompanyHQ(tile)) return false;

	ShowCompany(GetTileOwner(tile));
	return true;
}


/* checks, if a radio tower is within a 9x9 tile square around tile */
static bool IsRadioTowerNearby(TileIndex tile)
{
	TileIndex tile_s = tile - TileDiffXY(min(TileX(tile), 4U), min(TileY(tile), 4U));
	uint w = min(TileX(tile), 4U) + 1 + min(MapMaxX() - TileX(tile), 4U);
	uint h = min(TileY(tile), 4U) + 1 + min(MapMaxY() - TileY(tile), 4U);

	BEGIN_TILE_LOOP(tile, w, h, tile_s)
		if (IsTransmitterTile(tile)) return true;
	END_TILE_LOOP(tile, w, h, tile_s)

	return false;
}

void GenerateUnmovables()
{
	if (_settings_game.game_creation.landscape == LT_TOYLAND) return;

	/* add radio tower */
	int radiotower_to_build = ScaleByMapSize(15); // maximum number of radio towers on the map
	int lighthouses_to_build = _settings_game.game_creation.landscape == LT_TROPIC ? 0 : ScaleByMapSize1D((Random() & 3) + 7);

	/* Scale the amount of lighthouses with the amount of land at the borders. */
	if (_settings_game.construction.freeform_edges && lighthouses_to_build != 0) {
		uint num_water_tiles = 0;
		for (uint x = 0; x < MapMaxX(); x++) {
			if (IsTileType(TileXY(x, 1), MP_WATER)) num_water_tiles++;
			if (IsTileType(TileXY(x, MapMaxY() - 1), MP_WATER)) num_water_tiles++;
		}
		for (uint y = 1; y < MapMaxY() - 1; y++) {
			if (IsTileType(TileXY(1, y), MP_WATER)) num_water_tiles++;
			if (IsTileType(TileXY(MapMaxX() - 1, y), MP_WATER)) num_water_tiles++;
		}
		/* The -6 is because the top borders are MP_VOID (-2) and all corners
		 * are counted twice (-4). */
		lighthouses_to_build = lighthouses_to_build * num_water_tiles / (2 * MapMaxY() + 2 * MapMaxX() - 6);
	}

	SetGeneratingWorldProgress(GWP_UNMOVABLE, radiotower_to_build + lighthouses_to_build);

	for (uint i = ScaleByMapSize(1000); i != 0; i--) {
		TileIndex tile = RandomTile();

		uint h;
		if (IsTileType(tile, MP_CLEAR) && GetTileSlope(tile, &h) == SLOPE_FLAT && h >= TILE_HEIGHT * 4 && !IsBridgeAbove(tile)) {
			if (IsRadioTowerNearby(tile)) continue;

			MakeTransmitter(tile);
			IncreaseGeneratingWorldProgress(GWP_UNMOVABLE);
			if (--radiotower_to_build == 0) break;
		}
	}

	/* add lighthouses */
	uint maxx = MapMaxX();
	uint maxy = MapMaxY();
	for (int loop_count = 0; loop_count < 1000 && lighthouses_to_build != 0; loop_count++) {
		uint r = Random();

		/* Scatter the lighthouses more evenly around the perimeter */
		int perimeter = (GB(r, 16, 16) % (2 * (maxx + maxy))) - maxy;
		DiagDirection dir;
		for (dir = DIAGDIR_NE; perimeter > 0; dir++) {
			perimeter -= (DiagDirToAxis(dir) == AXIS_X) ? maxx : maxy;
		}

		TileIndex tile;
		switch (dir) {
			default:
			case DIAGDIR_NE: tile = TileXY(maxx - 1, r % maxy); break;
			case DIAGDIR_SE: tile = TileXY(r % maxx, 1); break;
			case DIAGDIR_SW: tile = TileXY(1,        r % maxy); break;
			case DIAGDIR_NW: tile = TileXY(r % maxx, maxy - 1); break;
		}

		/* Only build lighthouses at tiles where the border is sea. */
		if (!IsTileType(tile, MP_WATER)) continue;

		for (int j = 0; j < 19; j++) {
			uint h;
			if (IsTileType(tile, MP_CLEAR) && GetTileSlope(tile, &h) == SLOPE_FLAT && h <= TILE_HEIGHT * 2 && !IsBridgeAbove(tile)) {
				MakeLighthouse(tile);
				IncreaseGeneratingWorldProgress(GWP_UNMOVABLE);
				lighthouses_to_build--;
				assert(tile < MapSize());
				break;
			}
			tile = AddTileIndexDiffCWrap(tile, TileIndexDiffCByDiagDir(dir));
			if (tile == INVALID_TILE) break;
		}
	}
}

static void ChangeTileOwner_Unmovable(TileIndex tile, Owner old_owner, Owner new_owner)
{
	if (!IsTileOwner(tile, old_owner)) return;

	if (IsOwnedLand(tile) && new_owner != INVALID_OWNER) {
		SetTileOwner(tile, new_owner);
	} else if (IsStatueTile(tile)) {
		TownID town = GetStatueTownID(tile);
		Town *t = GetTown(town);
		ClrBit(t->statues, old_owner);
		if (new_owner != INVALID_OWNER && !HasBit(t->statues, new_owner)) {
			/* Transfer ownership to the new company */
			SetBit(t->statues, new_owner);
			SetTileOwner(tile, new_owner);
		} else {
			DoClearSquare(tile);
		}

		InvalidateWindow(WC_TOWN_AUTHORITY, town);
	} else {
		DoClearSquare(tile);
	}
}

static CommandCost TerraformTile_Unmovable(TileIndex tile, DoCommandFlag flags, uint z_new, Slope tileh_new)
{
	/* Owned land remains unsold */
	if (IsOwnedLand(tile) && CheckTileOwnership(tile)) return CommandCost();

	if (AutoslopeEnabled() && (IsStatue(tile) || IsCompanyHQ(tile))) {
		if (!IsSteepSlope(tileh_new) && (z_new + GetSlopeMaxZ(tileh_new) == GetTileMaxZ(tile))) return CommandCost(EXPENSES_CONSTRUCTION, _price.terraform);
	}

	return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
}

extern const TileTypeProcs _tile_type_unmovable_procs = {
	DrawTile_Unmovable,             // draw_tile_proc
	GetSlopeZ_Unmovable,            // get_slope_z_proc
	ClearTile_Unmovable,            // clear_tile_proc
	GetAcceptedCargo_Unmovable,     // get_accepted_cargo_proc
	GetTileDesc_Unmovable,          // get_tile_desc_proc
	GetTileTrackStatus_Unmovable,   // get_tile_track_status_proc
	ClickTile_Unmovable,            // click_tile_proc
	AnimateTile_Unmovable,          // animate_tile_proc
	TileLoop_Unmovable,             // tile_loop_clear
	ChangeTileOwner_Unmovable,      // change_tile_owner_clear
	NULL,                           // get_produced_cargo_proc
	NULL,                           // vehicle_enter_tile_proc
	GetFoundation_Unmovable,        // get_foundation_proc
	TerraformTile_Unmovable,        // terraform_tile_proc
};
