/* $Id$ */

/** @file newgrf_house.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "debug.h"
#include "viewport_func.h"
#include "landscape.h"
#include "town.h"
#include "town_map.h"
#include "sprite.h"
#include "newgrf.h"
#include "newgrf_house.h"
#include "newgrf_spritegroup.h"
#include "newgrf_callbacks.h"
#include "newgrf_town.h"
#include "newgrf_sound.h"
#include "newgrf_commons.h"
#include "transparency.h"
#include "functions.h"
#include "player_func.h"
#include "animated_tile_func.h"
#include "date_func.h"

#include "table/strings.h"
#include "table/sprites.h"
#include "table/town_land.h"

static BuildingCounts    _building_counts;
static HouseClassMapping _class_mapping[HOUSE_CLASS_MAX];

HouseOverrideManager _house_mngr(NEW_HOUSE_OFFSET, HOUSE_MAX, INVALID_HOUSE_ID);

/**
 * Check and update town and house values.
 *
 * Checked are the HouseIDs. Updated are the
 * town population the number of houses per
 * town, the town radius and the max passengers
 * of the town.
 */
void UpdateHousesAndTowns()
{
	Town *town;
	InitializeBuildingCounts();

	/* Reset town population and num_houses */
	FOR_ALL_TOWNS(town) {
		town->population = 0;
		town->num_houses = 0;
	}

	for (TileIndex t = 0; t < MapSize(); t++) {
		HouseID house_id;

		if (!IsTileType(t, MP_HOUSE)) continue;

		house_id = GetHouseType(t);
		if (!GetHouseSpecs(house_id)->enabled && house_id >= NEW_HOUSE_OFFSET) {
			/* The specs for this type of house are not available any more, so
			 * replace it with the substitute original house type. */
			house_id = _house_mngr.GetSubstituteID(house_id);
			SetHouseType(t, house_id);
		}

		town = GetTownByTile(t);
		IncreaseBuildingCount(town, house_id);
		if (IsHouseCompleted(t)) town->population += GetHouseSpecs(house_id)->population;

		/* Increase the number of houses for every house tile which
		 * has a size bit set. Multi tile buildings have got only
		 * one tile with such a bit set, so there is no problem. */
		if (GetHouseSpecs(GetHouseType(t))->building_flags & BUILDING_HAS_1_TILE) {
			town->num_houses++;
		}
	}

	/* Update the population and num_house dependant values */
	FOR_ALL_TOWNS(town) {
		UpdateTownRadius(town);
		UpdateTownMaxPass(town);
	}
}

HouseClassID AllocateHouseClassID(byte grf_class_id, uint32 grfid)
{
	/* Start from 1 because 0 means that no class has been assigned. */
	for (int i = 1; i != lengthof(_class_mapping); i++) {
		HouseClassMapping *map = &_class_mapping[i];

		if (map->class_id == grf_class_id && map->grfid == grfid) return (HouseClassID)i;

		if (map->class_id == 0 && map->grfid == 0) {
			map->class_id = grf_class_id;
			map->grfid    = grfid;
			return (HouseClassID)i;
		}
	}
	return HOUSE_NO_CLASS;
}

void InitializeBuildingCounts()
{
	memset(&_building_counts, 0, sizeof(_building_counts));
}

/**
 * IncreaseBuildingCount()
 * Increase the count of a building when it has been added by a town.
 * @param t The town that the building is being built in
 * @param house_id The id of the house being added
 */
void IncreaseBuildingCount(Town *t, HouseID house_id)
{
	HouseClassID class_id = GetHouseSpecs(house_id)->class_id;

	if (!_loaded_newgrf_features.has_newhouses) return;

	/* If there are 255 buildings of this type in this town, there are also
	 * at least that many houses of the same class in the town, and
	 * therefore on the map as well. */
	if (t->building_counts.id_count[house_id] == 255) return;

	t->building_counts.id_count[house_id]++;
	if (_building_counts.id_count[house_id] < 255) _building_counts.id_count[house_id]++;

	/* Similarly, if there are 255 houses of this class in this town, there
	 * must be at least that number on the map too. */
	if (class_id == HOUSE_NO_CLASS || t->building_counts.class_count[class_id] == 255) return;

	t->building_counts.class_count[class_id]++;
	if (_building_counts.class_count[class_id] < 255) _building_counts.class_count[class_id]++;
}

/**
 * DecreaseBuildingCount()
 * Decrease the number of a building when it is deleted.
 * @param t The town that the building was built in
 * @param house_id The id of the house being removed
 */
void DecreaseBuildingCount(Town *t, HouseID house_id)
{
	HouseClassID class_id = GetHouseSpecs(house_id)->class_id;

	if (!_loaded_newgrf_features.has_newhouses) return;

	if (t->building_counts.id_count[house_id] > 0) t->building_counts.id_count[house_id]--;
	if (_building_counts.id_count[house_id] > 0)   _building_counts.id_count[house_id]--;

	if (class_id == HOUSE_NO_CLASS) return;

	if (t->building_counts.class_count[class_id] > 0) t->building_counts.class_count[class_id]--;
	if (_building_counts.class_count[class_id] > 0)   _building_counts.class_count[class_id]--;
}

static uint32 HouseGetRandomBits(const ResolverObject *object)
{
	const TileIndex tile = object->u.house.tile;
	return (tile == INVALID_TILE || !IsTileType(tile, MP_HOUSE)) ? 0 : GetHouseRandomBits(tile);
}

static uint32 HouseGetTriggers(const ResolverObject *object)
{
	const TileIndex tile = object->u.house.tile;
	return (tile == INVALID_TILE || !IsTileType(tile, MP_HOUSE)) ? 0 : GetHouseTriggers(tile);
}

static void HouseSetTriggers(const ResolverObject *object, int triggers)
{
	const TileIndex tile = object->u.house.tile;
	if (IsTileType(tile, MP_HOUSE)) SetHouseTriggers(tile, triggers);
}

static uint32 GetNumHouses(HouseID house_id, const Town *town)
{
	uint8 map_id_count, town_id_count, map_class_count, town_class_count;
	HouseClassID class_id = GetHouseSpecs(house_id)->class_id;

	map_id_count     = _building_counts.id_count[house_id];
	map_class_count  = _building_counts.class_count[class_id];
	town_id_count    = town->building_counts.id_count[house_id];
	town_class_count = town->building_counts.class_count[class_id];

	return map_class_count << 24 | town_class_count << 16 | map_id_count << 8 | town_id_count;
}

static uint32 GetGRFParameter(HouseID house_id, byte parameter)
{
	const HouseSpec *hs = GetHouseSpecs(house_id);
	const GRFFile *file = hs->grffile;

	if (parameter >= file->param_end) return 0;
	return file->param[parameter];
}

uint32 GetNearbyTileInformation(byte parameter, TileIndex tile)
{
	tile = GetNearbyTile(parameter, tile);
	return GetNearbyTileInformation(tile);
}

/**
 * HouseGetVariable():
 *
 * Used by the resolver to get values for feature 07 deterministic spritegroups.
 */
static uint32 HouseGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	const Town *town = object->u.house.town;
	TileIndex tile   = object->u.house.tile;
	HouseID house_id = object->u.house.house_id;

	if (object->scope == VSG_SCOPE_PARENT) {
		return TownGetVariable(variable, parameter, available, town);
	}

	switch (variable) {
		/* Construction stage. */
		case 0x40: return (IsTileType(tile, MP_HOUSE) ? GetHouseBuildingStage(tile) : 0) | TileHash2Bit(TileX(tile), TileY(tile)) << 2;

		/* Building age. */
		case 0x41: return Clamp(_cur_year - GetHouseConstructionYear(tile), 0, 0xFF);

		/* Town zone */
		case 0x42: return GetTownRadiusGroup(town, tile);

		/* Terrain type */
		case 0x43: return GetTerrainType(tile);

		/* Number of this type of building on the map. */
		case 0x44: return GetNumHouses(house_id, town);

		/* Whether the town is being created or just expanded. */
		case 0x45: return _generating_world ? 1 : 0;

		/* Current animation frame. */
		case 0x46: return IsTileType(tile, MP_HOUSE) ? GetHouseAnimationFrame(tile) : 0;


		/* Building counts for old houses with id = parameter. */
		case 0x60: return GetNumHouses(parameter, town);

		/* Building counts for new houses with id = parameter. */
		case 0x61: {
			const HouseSpec *hs = GetHouseSpecs(house_id);
			if (hs->grffile == NULL) return 0;

			HouseID new_house = _house_mngr.GetID(parameter, hs->grffile->grfid);
			return new_house == INVALID_HOUSE_ID ? 0 : GetNumHouses(new_house, town);
		}

		/* Land info for nearby tiles. */
		case 0x62: return GetNearbyTileInformation(parameter, tile);

		/* Read GRF parameter */
		case 0x7F: return GetGRFParameter(object->u.house.house_id, parameter);
	}

	DEBUG(grf, 1, "Unhandled house property 0x%X", variable);

	*available = false;
	return UINT_MAX;
}

static const SpriteGroup *HouseResolveReal(const ResolverObject *object, const SpriteGroup *group)
{
	/* Houses do not have 'real' groups */
	return NULL;
}

/**
 * NewHouseResolver():
 *
 * Returns a resolver object to be used with feature 07 spritegroups.
 */
static void NewHouseResolver(ResolverObject *res, HouseID house_id, TileIndex tile, Town *town)
{
	res->GetRandomBits = HouseGetRandomBits;
	res->GetTriggers   = HouseGetTriggers;
	res->SetTriggers   = HouseSetTriggers;
	res->GetVariable   = HouseGetVariable;
	res->ResolveReal   = HouseResolveReal;

	res->u.house.tile     = tile;
	res->u.house.town     = town;
	res->u.house.house_id = house_id;

	res->callback        = CBID_NO_CALLBACK;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
	res->count           = 0;
}

uint16 GetHouseCallback(CallbackID callback, uint32 param1, uint32 param2, HouseID house_id, Town *town, TileIndex tile)
{
	ResolverObject object;
	const SpriteGroup *group;

	NewHouseResolver(&object, house_id, tile, town);
	object.callback = callback;
	object.callback_param1 = param1;
	object.callback_param2 = param2;

	group = Resolve(GetHouseSpecs(house_id)->spritegroup, &object);
	if (group == NULL || group->type != SGT_CALLBACK) return CALLBACK_FAILED;

	return group->g.callback.result;
}

void DrawTileLayout(const TileInfo *ti, const SpriteGroup *group, byte stage, HouseID house_id)
{
	const DrawTileSprites *dts = group->g.layout.dts;
	const DrawTileSeqStruct *dtss;

	SpriteID image = dts->ground.sprite;
	SpriteID pal   = dts->ground.pal;

	if (IS_CUSTOM_SPRITE(image)) image += stage;

	if (GB(image, 0, SPRITE_WIDTH) != 0) DrawGroundSprite(image, pal);

	/* End now, if houses are invisible */
	if (IsInvisibilitySet(TO_HOUSES)) return;

	foreach_draw_tile_seq(dtss, dts->seq) {
		if (GB(dtss->image.sprite, 0, SPRITE_WIDTH) == 0) continue;

		image = dtss->image.sprite;
		pal   = dtss->image.pal;

		if (IS_CUSTOM_SPRITE(image)) image += stage;

		if ((HasBit(image, SPRITE_MODIFIER_OPAQUE) || !IsTransparencySet(TO_HOUSES)) && HasBit(image, PALETTE_MODIFIER_COLOR)) {
			if (pal == 0) {
				const HouseSpec *hs = GetHouseSpecs(house_id);
				if (HasBit(hs->callback_mask, CBM_HOUSE_COLOUR)) {
					uint16 callback = GetHouseCallback(CBID_HOUSE_COLOUR, 0, 0, house_id, GetTownByTile(ti->tile), ti->tile);
					if (callback != CALLBACK_FAILED) {
						/* If bit 14 is set, we should use a 2cc colour map, else use the callback value. */
						pal = HasBit(callback, 14) ? GB(callback, 0, 8) + SPR_2CCMAP_BASE : callback;
					}
				} else {
					pal = hs->random_colour[TileHash2Bit(ti->x, ti->y)] + PALETTE_RECOLOR_START;
				}
			}
		} else {
			pal = PAL_NONE;
		}

		if ((byte)dtss->delta_z != 0x80) {
			AddSortableSpriteToDraw(
				image, pal,
				ti->x + dtss->delta_x, ti->y + dtss->delta_y,
				dtss->size_x, dtss->size_y,
				dtss->size_z, ti->z + dtss->delta_z,
				IsTransparencySet(TO_HOUSES)
			);
		} else {
			AddChildSpriteScreen(image, pal, (byte)dtss->delta_x, (byte)dtss->delta_y, IsTransparencySet(TO_HOUSES));
		}
	}
}

void DrawNewHouseTile(TileInfo *ti, HouseID house_id)
{
	const HouseSpec *hs = GetHouseSpecs(house_id);
	const SpriteGroup *group;
	ResolverObject object;

	if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, FOUNDATION_LEVELED);

	NewHouseResolver(&object, house_id, ti->tile, GetTownByTile(ti->tile));

	group = Resolve(hs->spritegroup, &object);
	if (group == NULL || group->type != SGT_TILELAYOUT) {
		/* XXX: This is for debugging purposes really, and shouldn't stay. */
		DrawGroundSprite(SPR_SHADOW_CELL, PAL_NONE);
	} else {
		/* Limit the building stage to the number of stages supplied. */
		byte stage = GetHouseBuildingStage(ti->tile);
		stage = Clamp(stage - 4 + group->g.layout.num_sprites, 0, group->g.layout.num_sprites - 1);
		DrawTileLayout(ti, group, stage, house_id);
	}
}

void AnimateNewHouseTile(TileIndex tile)
{
	const HouseSpec *hs = GetHouseSpecs(GetHouseType(tile));
	byte animation_speed = hs->animation_speed;
	bool frame_set_by_callback = false;

	if (HasBit(hs->callback_mask, CBM_HOUSE_ANIMATION_SPEED)) {
		uint16 callback_res = GetHouseCallback(CBID_HOUSE_ANIMATION_SPEED, 0, 0, GetHouseType(tile), GetTownByTile(tile), tile);
		if (callback_res != CALLBACK_FAILED) animation_speed = Clamp(callback_res & 0xFF, 2, 16);
	}

	/* An animation speed of 2 means the animation frame changes 4 ticks, and
	 * increasing this value by one doubles the wait. 2 is the minimum value
	 * allowed for animation_speed, which corresponds to 120ms, and 16 is the
	 * maximum, corresponding to around 33 minutes. */
	if (_tick_counter % (1 << animation_speed) != 0) return;

	byte frame      = GetHouseAnimationFrame(tile);
	byte num_frames = GB(hs->animation_frames, 0, 7);

	if (HasBit(hs->callback_mask, CBM_HOUSE_ANIMATION_NEXT_FRAME)) {
		uint32 param = (hs->extra_flags & CALLBACK_1A_RANDOM_BITS) ? Random() : 0;
		uint16 callback_res = GetHouseCallback(CBID_HOUSE_ANIMATION_NEXT_FRAME, param, 0, GetHouseType(tile), GetTownByTile(tile), tile);

		if (callback_res != CALLBACK_FAILED) {
			frame_set_by_callback = true;

			switch (callback_res & 0xFF) {
				case 0xFF:
					DeleteAnimatedTile(tile);
					break;
				case 0xFE:
					/* Carry on as normal. */
					frame_set_by_callback = false;
					break;
				default:
					frame = callback_res & 0xFF;
					break;
			}

			/* If the lower 7 bits of the upper byte of the callback
			 * result are not empty, it is a sound effect. */
			if (GB(callback_res, 8, 7) != 0) PlayTileSound(hs->grffile, GB(callback_res, 8, 7), tile);
		}
	}

	if (!frame_set_by_callback) {
		if (frame < num_frames) {
			frame++;
		} else if (frame == num_frames && HasBit(hs->animation_frames, 7)) {
			/* This animation loops, so start again from the beginning */
			frame = 0;
		} else {
			/* This animation doesn't loop, so stay here */
			DeleteAnimatedTile(tile);
		}
	}

	SetHouseAnimationFrame(tile, frame);
	MarkTileDirtyByTile(tile);
}

void ChangeHouseAnimationFrame(const GRFFile *file, TileIndex tile, uint16 callback_result)
{
	switch (callback_result & 0xFF) {
		case 0xFD: /* Do nothing. */         break;
		case 0xFE: AddAnimatedTile(tile);    break;
		case 0xFF: DeleteAnimatedTile(tile); break;
		default:
			SetHouseAnimationFrame(tile, callback_result & 0xFF);
			AddAnimatedTile(tile);
			break;
	}
	/* If the lower 7 bits of the upper byte of the callback
	 * result are not empty, it is a sound effect. */
	if (GB(callback_result, 8, 7) != 0) PlayTileSound(file, GB(callback_result, 8, 7), tile);
}

bool CanDeleteHouse(TileIndex tile)
{
	const HouseSpec *hs = GetHouseSpecs(GetHouseType(tile));

	/* Human players are always allowed to remove buildings, as is water and
	 * anyone using the scenario editor. */
	if ((IsValidPlayer(_current_player) && IsHumanPlayer(_current_player))
			|| _current_player == OWNER_WATER || _current_player == OWNER_NONE) return true;

	if (HasBit(hs->callback_mask, CBM_HOUSE_DENY_DESTRUCTION)) {
		uint16 callback_res = GetHouseCallback(CBID_HOUSE_DENY_DESTRUCTION, 0, 0, GetHouseType(tile), GetTownByTile(tile), tile);
		return (callback_res == CALLBACK_FAILED || callback_res == 0);
	} else {
		return !(hs->extra_flags & BUILDING_IS_PROTECTED);
	}
}

static void AnimationControl(TileIndex tile, uint16 random_bits)
{
	const HouseSpec *hs = GetHouseSpecs(GetHouseType(tile));

	if (HasBit(hs->callback_mask, CBM_HOUSE_ANIMATION_START_STOP)) {
		uint32 param = (hs->extra_flags & SYNCHRONISED_CALLBACK_1B) ? (GB(Random(), 0, 16) | random_bits << 16) : Random();
		uint16 callback_res = GetHouseCallback(CBID_HOUSE_ANIMATION_START_STOP, param, 0, GetHouseType(tile), GetTownByTile(tile), tile);

		if (callback_res != CALLBACK_FAILED) ChangeHouseAnimationFrame(hs->grffile, tile, callback_res);
	}
}

bool NewHouseTileLoop(TileIndex tile)
{
	const HouseSpec *hs = GetHouseSpecs(GetHouseType(tile));

	if (GetHouseProcessingTime(tile) > 0) {
		DecHouseProcessingTime(tile);
		return true;
	}

	TriggerHouse(tile, HOUSE_TRIGGER_TILE_LOOP);
	TriggerHouse(tile, HOUSE_TRIGGER_TILE_LOOP_TOP);

	if (HasBit(hs->callback_mask, CBM_HOUSE_ANIMATION_START_STOP)) {
		/* If this house is marked as having a synchronised callback, all the
		 * tiles will have the callback called at once, rather than when the
		 * tile loop reaches them. This should only be enabled for the northern
		 * tile, or strange things will happen (here, and in TTDPatch). */
		if (hs->extra_flags & SYNCHRONISED_CALLBACK_1B) {
			uint16 random = GB(Random(), 0, 16);

			if (hs->building_flags & BUILDING_HAS_1_TILE)  AnimationControl(tile, random);
			if (hs->building_flags & BUILDING_2_TILES_Y)   AnimationControl(TILE_ADDXY(tile, 0, 1), random);
			if (hs->building_flags & BUILDING_2_TILES_X)   AnimationControl(TILE_ADDXY(tile, 1, 0), random);
			if (hs->building_flags & BUILDING_HAS_4_TILES) AnimationControl(TILE_ADDXY(tile, 1, 1), random);
		} else {
			AnimationControl(tile, 0);
		}
	}

	/* Check callback 21, which determines if a house should be destroyed. */
	if (HasBit(hs->callback_mask, CBM_HOUSE_DESTRUCTION)) {
		uint16 callback_res = GetHouseCallback(CBID_HOUSE_DESTRUCTION, 0, 0, GetHouseType(tile), GetTownByTile(tile), tile);
		if (callback_res != CALLBACK_FAILED && GB(callback_res, 0, 8) > 0) {
			ClearTownHouse(GetTownByTile(tile), tile);
			return false;
		}
	}

	SetHouseProcessingTime(tile, hs->processing_time);
	return true;
}

static void DoTriggerHouse(TileIndex tile, HouseTrigger trigger, byte base_random, bool first)
{
	ResolverObject object;

	/* We can't trigger a non-existent building... */
	assert(IsTileType(tile, MP_HOUSE));

	HouseID hid = GetHouseType(tile);
	HouseSpec *hs = GetHouseSpecs(hid);

	if (hs->spritegroup == NULL) return;

	NewHouseResolver(&object, hid, tile, GetTownByTile(tile));

	object.callback = CBID_RANDOM_TRIGGER;
	object.trigger = trigger;

	const SpriteGroup *group = Resolve(hs->spritegroup, &object);
	if (group == NULL) return;

	byte new_random_bits = Random();
	byte random_bits = GetHouseRandomBits(tile);
	random_bits &= ~object.reseed;
	random_bits |= (first ? new_random_bits : base_random) & object.reseed;
	SetHouseRandomBits(tile, random_bits);

	switch (trigger) {
		case HOUSE_TRIGGER_TILE_LOOP:
			/* Random value already set. */
			break;

		case HOUSE_TRIGGER_TILE_LOOP_TOP:
			if (!first) break;
			/* Random value of first tile already set. */
			if (hs->building_flags & BUILDING_2_TILES_Y)   DoTriggerHouse(TILE_ADDXY(tile, 0, 1), trigger, random_bits, false);
			if (hs->building_flags & BUILDING_2_TILES_X)   DoTriggerHouse(TILE_ADDXY(tile, 1, 0), trigger, random_bits, false);
			if (hs->building_flags & BUILDING_HAS_4_TILES) DoTriggerHouse(TILE_ADDXY(tile, 1, 1), trigger, random_bits, false);
			break;
	}
}

void TriggerHouse(TileIndex t, HouseTrigger trigger)
{
	DoTriggerHouse(t, trigger, 0, true);
}
