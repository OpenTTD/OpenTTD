/* $Id$ */

/** @file unmovable_cmd.cpp Handling of unmovable tiles. */

#include "stdafx.h"
#include "openttd.h"
#include "tile_cmd.h"
#include "landscape.h"
#include "command_func.h"
#include "viewport_func.h"
#include "player_func.h"
#include "player_base.h"
#include "gui.h"
#include "town.h"
#include "sprite.h"
#include "bridge_map.h"
#include "unmovable_map.h"
#include "variables.h"
#include "genworld.h"
#include "bridge.h"
#include "autoslope.h"
#include "transparency.h"
#include "functions.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "player_gui.h"
#include "station_type.h"
#include "economy_func.h"
#include "cheat_func.h"

#include "table/strings.h"
#include "table/sprites.h"
#include "table/unmovable_land.h"

/** Destroy a HQ.
 * During normal gameplay you can only implicitely destroy a HQ when you are
 * rebuilding it. Otherwise, only water can destroy it.
 * @param pid Player requesting the destruction of his HQ
 * @param flags docommand flags of calling function
 * @return cost of the operation
 */
static CommandCost DestroyCompanyHQ(PlayerID pid, uint32 flags)
{
	Player *p = GetPlayer(pid);

	if (flags & DC_EXEC) {
		TileIndex t = p->location_of_house;

		DoClearSquare(t + TileDiffXY(0, 0));
		DoClearSquare(t + TileDiffXY(0, 1));
		DoClearSquare(t + TileDiffXY(1, 0));
		DoClearSquare(t + TileDiffXY(1, 1));
		p->location_of_house = 0; // reset HQ position
		InvalidateWindow(WC_COMPANY, pid);
	}

	/* cost of relocating company is 1% of company value */
	return CommandCost(EXPENSES_PROPERTY, CalculateCompanyValue(p) / 100);
}

void UpdateCompanyHQ(Player *p, uint score)
{
	byte val;
	TileIndex tile = p->location_of_house;

	if (tile == 0) return;

	(val = 0, score < 170) ||
	(val++, score < 350) ||
	(val++, score < 520) ||
	(val++, score < 720) ||
	(val++, true);

	EnlargeCompanyHQ(tile, val);

	MarkTileDirtyByTile(tile + TileDiffXY(0, 0));
	MarkTileDirtyByTile(tile + TileDiffXY(0, 1));
	MarkTileDirtyByTile(tile + TileDiffXY(1, 0));
	MarkTileDirtyByTile(tile + TileDiffXY(1, 1));
}

extern CommandCost CheckFlatLandBelow(TileIndex tile, uint w, uint h, uint flags, uint invalid_dirs, StationID *station, bool check_clear = true);

/** Build or relocate the HQ. This depends if the HQ is already built or not
 * @param tile tile where the HQ will be built or relocated to
 * @param flags type of operation
 * @param p1 unused
 * @param p2 unused
 */
CommandCost CmdBuildCompanyHQ(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Player *p = GetPlayer(_current_player);
	CommandCost cost(EXPENSES_PROPERTY);

	cost = CheckFlatLandBelow(tile, 2, 2, flags, 0, NULL);
	if (CmdFailed(cost)) return cost;

	if (p->location_of_house != 0) { // Moving HQ
		cost.AddCost(DestroyCompanyHQ(_current_player, flags));
	}

	if (flags & DC_EXEC) {
		int score = UpdateCompanyRatingAndValue(p, false);

		p->location_of_house = tile;

		MakeCompanyHQ(tile, _current_player);

		UpdateCompanyHQ(p, score);
		InvalidateWindow(WC_COMPANY, p->index);
	}

	return cost;
}

/** Purchase a land area. Actually you only purchase one tile, so
 * the name is a bit confusing ;p
 * @param tile the tile the player is purchasing
 * @param flags for this command type
 * @param p1 unused
 * @param p2 unused
 * @return error of cost of operation
 */
CommandCost CmdPurchaseLandArea(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	CommandCost cost(EXPENSES_CONSTRUCTION);

	if (IsOwnedLandTile(tile) && IsTileOwner(tile, _current_player)) {
		return_cmd_error(STR_5807_YOU_ALREADY_OWN_IT);
	}

	cost = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(cost)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		MakeOwnedLand(tile, _current_player);
		MarkTileDirtyByTile(tile);
	}

	return cost.AddCost(_price.clear_roughland * 10);
}

/** Sell a land area. Actually you only sell one tile, so
 * the name is a bit confusing ;p
 * @param tile the tile the player is selling
 * @param flags for this command type
 * @param p1 unused
 * @param p2 unused
 * @return error or cost of operation
 */
CommandCost CmdSellLandArea(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (!IsOwnedLandTile(tile)) return CMD_ERROR;
	if (!CheckTileOwnership(tile) && _current_player != OWNER_WATER) return CMD_ERROR;

	if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

	if (flags & DC_EXEC) DoClearSquare(tile);

	return CommandCost(EXPENSES_CONSTRUCTION, - _price.clear_roughland * 2);
}

static Foundation GetFoundation_Unmovable(TileIndex tile, Slope tileh);

static void DrawTile_Unmovable(TileInfo *ti)
{

	switch (GetUnmovableType(ti->tile)) {
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

			AddSortableSpriteToDraw(SPR_STATUE_COMPANY, PLAYER_SPRITE_COLOR(GetTileOwner(ti->tile)), ti->x, ti->y, 16, 16, 25, ti->z, IsTransparencySet(TO_STRUCTURES));
			break;

		case UNMOVABLE_OWNED_LAND:
			DrawClearLandTile(ti, 0);

			AddSortableSpriteToDraw(
				SPR_BOUGHT_LAND, PLAYER_SPRITE_COLOR(GetTileOwner(ti->tile)),
				ti->x + TILE_SIZE / 2, ti->y + TILE_SIZE / 2, 1, 1, BB_HEIGHT_UNDER_BRIDGE, GetSlopeZ(ti->x + TILE_SIZE / 2, ti->y + TILE_SIZE / 2)
			);
			DrawBridgeMiddle(ti);
			break;

		default: {
			assert(IsCompanyHQ(ti->tile));
			if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, FOUNDATION_LEVELED);

			SpriteID palette = PLAYER_SPRITE_COLOR(GetTileOwner(ti->tile));

			const DrawTileSprites *t = &_unmovable_display_datas[GetCompanyHQSection(ti->tile)];
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

static CommandCost ClearTile_Unmovable(TileIndex tile, byte flags)
{
	if (IsCompanyHQ(tile)) {
		if (_current_player == OWNER_WATER) {
			return DestroyCompanyHQ(GetTileOwner(tile), DC_EXEC);
		} else {
			return_cmd_error(STR_5804_COMPANY_HEADQUARTERS_IN);
		}
	}

	if (IsOwnedLand(tile)) {
		return DoCommand(tile, 0, 0, flags, CMD_SELL_LAND_AREA);
	}

	/* checks if you're allowed to remove unmovable things */
	if (_game_mode != GM_EDITOR && _current_player != OWNER_WATER && ((flags & DC_AUTO || !_cheats.magic_bulldozer.value)) )
		return_cmd_error(STR_5800_OBJECT_IN_THE_WAY);

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
	switch (GetUnmovableType(tile)) {
		case UNMOVABLE_TRANSMITTER: td->str = STR_5801_TRANSMITTER; break;
		case UNMOVABLE_LIGHTHOUSE:  td->str = STR_5802_LIGHTHOUSE; break;
		case UNMOVABLE_STATUE:      td->str = STR_2016_STATUE; break;
		case UNMOVABLE_OWNED_LAND:  td->str = STR_5805_COMPANY_OWNED_LAND; break;
		default:                    td->str = STR_5803_COMPANY_HEADQUARTERS; break;
	}
	td->owner = GetTileOwner(tile);
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

static void ClickTile_Unmovable(TileIndex tile)
{
	if (IsCompanyHQ(tile)) ShowPlayerCompany(GetTileOwner(tile));
}


/* checks, if a radio tower is within a 9x9 tile square around tile */
static bool IsRadioTowerNearby(TileIndex tile)
{
	TileIndex tile_s = tile - TileDiffXY(4, 4);

	BEGIN_TILE_LOOP(tile, 9, 9, tile_s)
		if (IsTransmitterTile(tile)) return true;
	END_TILE_LOOP(tile, 9, 9, tile_s)

	return false;
}

void GenerateUnmovables()
{
	if (_opt.landscape == LT_TOYLAND) return;

	/* add radio tower */
	int radiotowser_to_build = ScaleByMapSize(15); // maximum number of radio towers on the map
	int lighthouses_to_build = _opt.landscape == LT_TROPIC ? 0 : ScaleByMapSize1D((Random() & 3) + 7);
	SetGeneratingWorldProgress(GWP_UNMOVABLE, radiotowser_to_build + lighthouses_to_build);

	for (uint i = ScaleByMapSize(1000); i != 0; i--) {
		TileIndex tile = RandomTile();

		uint h;
		if (IsTileType(tile, MP_CLEAR) && GetTileSlope(tile, &h) == SLOPE_FLAT && h >= TILE_HEIGHT * 4 && !IsBridgeAbove(tile)) {
			if (IsRadioTowerNearby(tile)) continue;

			MakeTransmitter(tile);
			IncreaseGeneratingWorldProgress(GWP_UNMOVABLE);
			if (--radiotowser_to_build == 0) break;
		}
	}

	if (_opt.landscape == LT_TROPIC) return;

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
			case DIAGDIR_NE: tile = TileXY(maxx,     r % maxy); break;
			case DIAGDIR_SE: tile = TileXY(r % maxx, 0);        break;
			case DIAGDIR_SW: tile = TileXY(0,        r % maxy); break;
			case DIAGDIR_NW: tile = TileXY(r % maxx, maxy);     break;
		}

		for (int j = 0; j < 20; j++) {
			uint h;
			if (IsTileType(tile, MP_CLEAR) && GetTileSlope(tile, &h) == SLOPE_FLAT && h <= TILE_HEIGHT * 2 && !IsBridgeAbove(tile)) {
				MakeLighthouse(tile);
				IncreaseGeneratingWorldProgress(GWP_UNMOVABLE);
				lighthouses_to_build--;
				assert(tile == TILE_MASK(tile));
				break;
			}
			tile = TILE_MASK(tile + TileOffsByDiagDir(dir));
		}
	}
}

static void ChangeTileOwner_Unmovable(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	if (!IsTileOwner(tile, old_player)) return;

	if (IsOwnedLand(tile) && new_player != PLAYER_SPECTATOR) {
		SetTileOwner(tile, new_player);
	} else if (IsStatueTile(tile)) {
		TownID town = GetStatueTownID(tile);
		Town *t = GetTown(town);
		ClrBit(t->statues, old_player);
		if (new_player != PLAYER_SPECTATOR && !HasBit(t->statues, new_player)) {
			/* Transfer ownership to the new company */
			SetBit(t->statues, new_player);
			SetTileOwner(tile, new_player);
		} else {
			DoClearSquare(tile);
		}

		InvalidateWindow(WC_TOWN_AUTHORITY, town);
	} else {
		DoClearSquare(tile);
	}
}

static CommandCost TerraformTile_Unmovable(TileIndex tile, uint32 flags, uint z_new, Slope tileh_new)
{
	/* Owned land remains unsold */
	if (IsOwnedLand(tile) && CheckTileOwnership(tile)) return CommandCost();

	if (AutoslopeEnabled() && (IsStatue(tile) || IsCompanyHQ(tile))) {
		if (!IsSteepSlope(tileh_new) && (z_new + GetSlopeMaxZ(tileh_new) == GetTileMaxZ(tile))) return CommandCost(EXPENSES_CONSTRUCTION, _price.terraform);
	}

	return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
}

extern const TileTypeProcs _tile_type_unmovable_procs = {
	DrawTile_Unmovable,             /* draw_tile_proc */
	GetSlopeZ_Unmovable,            /* get_slope_z_proc */
	ClearTile_Unmovable,            /* clear_tile_proc */
	GetAcceptedCargo_Unmovable,     /* get_accepted_cargo_proc */
	GetTileDesc_Unmovable,          /* get_tile_desc_proc */
	GetTileTrackStatus_Unmovable,   /* get_tile_track_status_proc */
	ClickTile_Unmovable,            /* click_tile_proc */
	AnimateTile_Unmovable,          /* animate_tile_proc */
	TileLoop_Unmovable,             /* tile_loop_clear */
	ChangeTileOwner_Unmovable,      /* change_tile_owner_clear */
	NULL,                           /* get_produced_cargo_proc */
	NULL,                           /* vehicle_enter_tile_proc */
	GetFoundation_Unmovable,        /* get_foundation_proc */
	TerraformTile_Unmovable,        /* terraform_tile_proc */
};
