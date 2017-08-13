/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_object.cpp Handling of object NewGRFs. */

#include "stdafx.h"
#include "company_base.h"
#include "company_func.h"
#include "debug.h"
#include "genworld.h"
#include "newgrf_class_func.h"
#include "newgrf_object.h"
#include "newgrf_sound.h"
#include "object_base.h"
#include "object_map.h"
#include "tile_cmd.h"
#include "town.h"
#include "water.h"
#include "newgrf_animation_base.h"

#include "safeguards.h"

/** The override manager for our objects. */
ObjectOverrideManager _object_mngr(NEW_OBJECT_OFFSET, NUM_OBJECTS, INVALID_OBJECT_TYPE);

extern const ObjectSpec _original_objects[NEW_OBJECT_OFFSET];
/** All the object specifications. */
ObjectSpec _object_specs[NUM_OBJECTS];

/**
 * Get the specification associated with a specific ObjectType.
 * @param index The object type to fetch.
 * @return The specification.
 */
/* static */ const ObjectSpec *ObjectSpec::Get(ObjectType index)
{
	assert(index < NUM_OBJECTS);
	return &_object_specs[index];
}

/**
 * Get the specification associated with a tile.
 * @param tile The tile to fetch the data for.
 * @return The specification.
 */
/* static */ const ObjectSpec *ObjectSpec::GetByTile(TileIndex tile)
{
	return ObjectSpec::Get(GetObjectType(tile));
}

/**
 * Check whether the object might be available at some point in this game with the current game mode.
 * @return true if it might be available.
 */
bool ObjectSpec::IsEverAvailable() const
{
	return this->enabled && HasBit(this->climate, _settings_game.game_creation.landscape) &&
			(this->flags & ((_game_mode != GM_EDITOR && !_generating_world) ? OBJECT_FLAG_ONLY_IN_SCENEDIT : OBJECT_FLAG_ONLY_IN_GAME)) == 0;
}

/**
 * Check whether the object was available at some point in the past or present in this game with the current game mode.
 * @return true if it was ever or is available.
 */
bool ObjectSpec::WasEverAvailable() const
{
	return this->IsEverAvailable() && _date > this->introduction_date;
}

/**
 * Check whether the object is available at this time.
 * @return true if it is available.
 */
bool ObjectSpec::IsAvailable() const
{
	return this->WasEverAvailable() &&
			(_date < this->end_of_life_date || this->end_of_life_date < this->introduction_date + 365);
}

/**
 * Gets the index of this spec.
 * @return The index.
 */
uint ObjectSpec::Index() const
{
	return this - _object_specs;
}

/** This function initialize the spec arrays of objects. */
void ResetObjects()
{
	/* Clean the pool. */
	MemSetT(_object_specs, 0, lengthof(_object_specs));

	/* And add our originals. */
	MemCpyT(_object_specs, _original_objects, lengthof(_original_objects));

	for (uint16 i = 0; i < lengthof(_original_objects); i++) {
		_object_specs[i].grf_prop.local_id = i;
	}
}

template <typename Tspec, typename Tid, Tid Tmax>
/* static */ void NewGRFClass<Tspec, Tid, Tmax>::InsertDefaults()
{
	ObjectClassID cls = ObjectClass::Allocate('LTHS');
	ObjectClass::Get(cls)->name = STR_OBJECT_CLASS_LTHS;
	_object_specs[OBJECT_LIGHTHOUSE].cls_id = cls;
	ObjectClass::Assign(&_object_specs[OBJECT_LIGHTHOUSE]);

	cls = ObjectClass::Allocate('TRNS');
	ObjectClass::Get(cls)->name = STR_OBJECT_CLASS_TRNS;
	_object_specs[OBJECT_TRANSMITTER].cls_id = cls;
	ObjectClass::Assign(&_object_specs[OBJECT_TRANSMITTER]);
}

template <typename Tspec, typename Tid, Tid Tmax>
bool NewGRFClass<Tspec, Tid, Tmax>::IsUIAvailable(uint index) const
{
	return this->GetSpec(index)->IsEverAvailable();
}

INSTANTIATE_NEWGRF_CLASS_METHODS(ObjectClass, ObjectSpec, ObjectClassID, OBJECT_CLASS_MAX)

/**
 * Constructor of an object scope resolver.
 * @param ro Surrounding resolver.
 * @param obj Object being resolved.
 * @param tile %Tile of the object.
 * @param view View of the object.
 */
ObjectScopeResolver::ObjectScopeResolver(ResolverObject &ro, Object *obj, TileIndex tile, uint8 view)
		: ScopeResolver(ro)
{
	this->obj = obj;
	this->tile = tile;
	this->view = view;
}

/* virtual */ uint32 ObjectScopeResolver::GetRandomBits() const
{
	return IsValidTile(this->tile) && IsTileType(this->tile, MP_OBJECT) ? GetObjectRandomBits(this->tile) : 0;
}

/**
 * Make an analysis of a tile and get the object type.
 * @param tile TileIndex of the tile to query
 * @param cur_grfid GRFID of the current callback chain
 * @return value encoded as per NFO specs
 */
static uint32 GetObjectIDAtOffset(TileIndex tile, uint32 cur_grfid)
{
	if (!IsTileType(tile, MP_OBJECT)) {
		return 0xFFFF;
	}

	const Object *o = Object::GetByTile(tile);
	const ObjectSpec *spec = ObjectSpec::Get(o->type);

	/* Default objects have no associated NewGRF file */
	if (spec->grf_prop.grffile == NULL) {
		return 0xFFFE; // Defined in another grf file
	}

	if (spec->grf_prop.grffile->grfid == cur_grfid) { // same object, same grf ?
		return spec->grf_prop.local_id | o->view << 16;
	}

	return 0xFFFE; // Defined in another grf file
}

/**
 * Based on newhouses equivalent, but adapted for newobjects
 * @param parameter from callback.  It's in fact a pair of coordinates
 * @param tile TileIndex from which the callback was initiated
 * @param index of the object been queried for
 * @param grf_version8 True, if we are dealing with a new NewGRF which uses GRF version >= 8.
 * @return a construction of bits obeying the newgrf format
 */
static uint32 GetNearbyObjectTileInformation(byte parameter, TileIndex tile, ObjectID index, bool grf_version8)
{
	if (parameter != 0) tile = GetNearbyTile(parameter, tile); // only perform if it is required
	bool is_same_object = (IsTileType(tile, MP_OBJECT) && GetObjectIndex(tile) == index);

	return GetNearbyTileInformation(tile, grf_version8) | (is_same_object ? 1 : 0) << 8;
}

/**
 * Get the closest object of a given type.
 * @param tile    The tile to start searching from.
 * @param type    The type of the object to search for.
 * @param current The current object (to ignore).
 * @return The distance to the closest object.
 */
static uint32 GetClosestObject(TileIndex tile, ObjectType type, const Object *current)
{
	uint32 best_dist = UINT32_MAX;
	const Object *o;
	FOR_ALL_OBJECTS(o) {
		if (o->type != type || o == current) continue;

		best_dist = min(best_dist, DistanceManhattan(tile, o->location.tile));
	}

	return best_dist;
}

/**
 * Implementation of var 65
 * @param local_id Parameter given to the callback, which is the set id, or the local id, in our terminology.
 * @param grfid    The object's GRFID.
 * @param tile     The tile to look from.
 * @param current  Object for which the inquiry is made
 * @return The formatted answer to the callback : rr(reserved) cc(count) dddd(manhattan distance of closest sister)
 */
static uint32 GetCountAndDistanceOfClosestInstance(byte local_id, uint32 grfid, TileIndex tile, const Object *current)
{
	uint32 grf_id = GetRegister(0x100);  // Get the GRFID of the definition to look for in register 100h
	uint32 idx;

	/* Determine what will be the object type to look for */
	switch (grf_id) {
		case 0:  // this is a default object type
			idx = local_id;
			break;

		case 0xFFFFFFFF: // current grf
			grf_id = grfid;
			FALLTHROUGH;

		default: // use the grfid specified in register 100h
			idx = _object_mngr.GetID(local_id, grf_id);
			break;
	}

	/* If the object type is invalid, there is none and the closest is far away. */
	if (idx >= NUM_OBJECTS) return 0 | 0xFFFF;

	return Object::GetTypeCount(idx) << 16 | min(GetClosestObject(tile, idx, current), 0xFFFF);
}

/** Used by the resolver to get values for feature 0F deterministic spritegroups. */
/* virtual */ uint32 ObjectScopeResolver::GetVariable(byte variable, uint32 parameter, bool *available) const
{
	/* We get the town from the object, or we calculate the closest
	 * town if we need to when there's no object. */
	const Town *t = NULL;

	if (this->obj == NULL) {
		switch (variable) {
			/* Allow these when there's no object. */
			case 0x41:
			case 0x60:
			case 0x61:
			case 0x62:
			case 0x64:
				break;

			/* Allow these, but find the closest town. */
			case 0x45:
			case 0x46:
				if (!IsValidTile(this->tile)) goto unhandled;
				t = ClosestTownFromTile(this->tile, UINT_MAX);
				break;

			/* Construction date */
			case 0x42: return _date;

			/* Object founder information */
			case 0x44: return _current_company;

			/* Object view */
			case 0x48: return this->view;

			/*
			 * Disallow the rest:
			 * 0x40: Relative position is passed as parameter during construction.
			 * 0x43: Animation counter is only for actual tiles.
			 * 0x47: Object colour is only valid when its built.
			 * 0x63: Animation counter of nearby tile, see above.
			 */
			default:
				goto unhandled;
		}

		/* If there's an invalid tile, then we don't have enough information at all. */
		if (!IsValidTile(this->tile)) goto unhandled;
	} else {
		t = this->obj->town;
	}

	switch (variable) {
		/* Relative position. */
		case 0x40: {
			uint offset = this->tile - this->obj->location.tile;
			uint offset_x = TileX(offset);
			uint offset_y = TileY(offset);
			return offset_y << 20 | offset_x << 16 | offset_y << 8 | offset_x;
		}

		/* Tile information. */
		case 0x41: return GetTileSlope(this->tile) << 8 | GetTerrainType(this->tile);

		/* Construction date */
		case 0x42: return this->obj->build_date;

		/* Animation counter */
		case 0x43: return GetAnimationFrame(this->tile);

		/* Object founder information */
		case 0x44: return GetTileOwner(this->tile);

		/* Get town zone and Manhattan distance of closest town */
		case 0x45: return GetTownRadiusGroup(t, this->tile) << 16 | min(DistanceManhattan(this->tile, t->xy), 0xFFFF);

		/* Get square of Euclidian distance of closes town */
		case 0x46: return GetTownRadiusGroup(t, this->tile) << 16 | min(DistanceSquare(this->tile, t->xy), 0xFFFF);

		/* Object colour */
		case 0x47: return this->obj->colour;

		/* Object view */
		case 0x48: return this->obj->view;

		/* Get object ID at offset param */
		case 0x60: return GetObjectIDAtOffset(GetNearbyTile(parameter, this->tile), this->ro.grffile->grfid);

		/* Get random tile bits at offset param */
		case 0x61: {
			TileIndex tile = GetNearbyTile(parameter, this->tile);
			return (IsTileType(tile, MP_OBJECT) && Object::GetByTile(tile) == this->obj) ? GetObjectRandomBits(tile) : 0;
		}

		/* Land info of nearby tiles */
		case 0x62: return GetNearbyObjectTileInformation(parameter, this->tile, this->obj == NULL ? INVALID_OBJECT : this->obj->index, this->ro.grffile->grf_version >= 8);

		/* Animation counter of nearby tile */
		case 0x63: {
			TileIndex tile = GetNearbyTile(parameter, this->tile);
			return (IsTileType(tile, MP_OBJECT) && Object::GetByTile(tile) == this->obj) ? GetAnimationFrame(tile) : 0;
		}

		/* Count of object, distance of closest instance */
		case 0x64: return GetCountAndDistanceOfClosestInstance(parameter, this->ro.grffile->grfid, this->tile, this->obj);
	}

unhandled:
	DEBUG(grf, 1, "Unhandled object variable 0x%X", variable);

	*available = false;
	return UINT_MAX;
}

/**
 * Constructor of the object resolver.
 * @param obj Object being resolved.
 * @param tile %Tile of the object.
 * @param view View of the object.
 * @param callback Callback ID.
 * @param callback_param1 First parameter (var 10) of the callback.
 * @param callback_param2 Second parameter (var 18) of the callback.
 */
ObjectResolverObject::ObjectResolverObject(const ObjectSpec *spec, Object *obj, TileIndex tile, uint8 view,
		CallbackID callback, uint32 param1, uint32 param2)
	: ResolverObject(spec->grf_prop.grffile, callback, param1, param2), object_scope(*this, obj, tile, view)
{
	this->town_scope = NULL;
	this->root_spritegroup = (obj == NULL && spec->grf_prop.spritegroup[CT_PURCHASE_OBJECT] != NULL) ?
			spec->grf_prop.spritegroup[CT_PURCHASE_OBJECT] : spec->grf_prop.spritegroup[0];
}

ObjectResolverObject::~ObjectResolverObject()
{
	delete this->town_scope;
}

/**
 * Get the town resolver scope that belongs to this object resolver.
 * On the first call, the town scope is created (if possible).
 * @return Town scope, if available.
 */
TownScopeResolver *ObjectResolverObject::GetTown()
{
	if (this->town_scope == NULL) {
		Town *t;
		if (this->object_scope.obj != NULL) {
			t = this->object_scope.obj->town;
		} else {
			t = ClosestTownFromTile(this->object_scope.tile, UINT_MAX);
		}
		if (t == NULL) return NULL;
		this->town_scope = new TownScopeResolver(*this, t, this->object_scope.obj == NULL);
	}
	return this->town_scope;
}

/**
 * Perform a callback for an object.
 * @param callback The callback to perform.
 * @param param1   The first parameter to pass to the NewGRF.
 * @param param2   The second parameter to pass to the NewGRF.
 * @param spec     The specification of the object / the entry point.
 * @param o        The object to call the callback for.
 * @param tile     The tile the callback is called for.
 * @param view     The view of the object (only used when o == NULL).
 * @return The result of the callback.
 */
uint16 GetObjectCallback(CallbackID callback, uint32 param1, uint32 param2, const ObjectSpec *spec, Object *o, TileIndex tile, uint8 view)
{
	ObjectResolverObject object(spec, o, tile, view, callback, param1, param2);
	return object.ResolveCallback();
}

/**
 * Draw an group of sprites on the map.
 * @param ti    Information about the tile to draw on.
 * @param group The group of sprites to draw.
 * @param spec  Object spec to draw.
 */
static void DrawTileLayout(const TileInfo *ti, const TileLayoutSpriteGroup *group, const ObjectSpec *spec)
{
	const DrawTileSprites *dts = group->ProcessRegisters(NULL);
	PaletteID palette = ((spec->flags & OBJECT_FLAG_2CC_COLOUR) ? SPR_2CCMAP_BASE : PALETTE_RECOLOUR_START) + Object::GetByTile(ti->tile)->colour;

	SpriteID image = dts->ground.sprite;
	PaletteID pal  = dts->ground.pal;

	if (GB(image, 0, SPRITE_WIDTH) != 0) {
		/* If the ground sprite is the default flat water sprite, draw also canal/river borders
		 * Do not do this if the tile's WaterClass is 'land'. */
		if ((image == SPR_FLAT_WATER_TILE || spec->flags & OBJECT_FLAG_DRAW_WATER) && IsTileOnWater(ti->tile)) {
			DrawWaterClassGround(ti);
		} else {
			DrawGroundSprite(image, GroundSpritePaletteTransform(image, pal, palette));
		}
	}

	DrawNewGRFTileSeq(ti, dts, TO_STRUCTURES, 0, palette);
}

/**
 * Draw an object on the map.
 * @param ti   Information about the tile to draw on.
 * @param spec Object spec to draw.
 */
void DrawNewObjectTile(TileInfo *ti, const ObjectSpec *spec)
{
	Object *o = Object::GetByTile(ti->tile);
	ObjectResolverObject object(spec, o, ti->tile);

	const SpriteGroup *group = object.Resolve();
	if (group == NULL || group->type != SGT_TILELAYOUT) return;

	DrawTileLayout(ti, (const TileLayoutSpriteGroup *)group, spec);
}

/**
 * Draw representation of an object (tile) for GUI purposes.
 * @param x    Position x of image.
 * @param y    Position y of image.
 * @param spec Object spec to draw.
 * @param view The object's view.
 */
void DrawNewObjectTileInGUI(int x, int y, const ObjectSpec *spec, uint8 view)
{
	ObjectResolverObject object(spec, NULL, INVALID_TILE, view);
	const SpriteGroup *group = object.Resolve();
	if (group == NULL || group->type != SGT_TILELAYOUT) return;

	const DrawTileSprites *dts = ((const TileLayoutSpriteGroup *)group)->ProcessRegisters(NULL);

	PaletteID palette;
	if (Company::IsValidID(_local_company)) {
		/* Get the colours of our company! */
		if (spec->flags & OBJECT_FLAG_2CC_COLOUR) {
			const Livery *l = Company::Get(_local_company)->livery;
			palette = SPR_2CCMAP_BASE + l->colour1 + l->colour2 * 16;
		} else {
			palette = COMPANY_SPRITE_COLOUR(_local_company);
		}
	} else {
		/* There's no company, so just take the base palette. */
		palette = (spec->flags & OBJECT_FLAG_2CC_COLOUR) ? SPR_2CCMAP_BASE : PALETTE_RECOLOUR_START;
	}

	SpriteID image = dts->ground.sprite;
	PaletteID pal  = dts->ground.pal;

	if (GB(image, 0, SPRITE_WIDTH) != 0) {
		DrawSprite(image, GroundSpritePaletteTransform(image, pal, palette), x, y);
	}

	DrawNewGRFTileSeqInGUI(x, y, dts, 0, palette);
}

/**
 * Perform a callback for an object.
 * @param callback The callback to perform.
 * @param param1   The first parameter to pass to the NewGRF.
 * @param param2   The second parameter to pass to the NewGRF.
 * @param spec     The specification of the object / the entry point.
 * @param o        The object to call the callback for.
 * @param tile     The tile the callback is called for.
 * @param extra_data Ignored.
 * @return The result of the callback.
 */
uint16 StubGetObjectCallback(CallbackID callback, uint32 param1, uint32 param2, const ObjectSpec *spec, Object *o, TileIndex tile, int extra_data)
{
	return GetObjectCallback(callback, param1, param2, spec, o, tile);
}

/** Helper class for animation control. */
struct ObjectAnimationBase : public AnimationBase<ObjectAnimationBase, ObjectSpec, Object, int, StubGetObjectCallback> {
	static const CallbackID cb_animation_speed      = CBID_OBJECT_ANIMATION_SPEED;
	static const CallbackID cb_animation_next_frame = CBID_OBJECT_ANIMATION_NEXT_FRAME;

	static const ObjectCallbackMask cbm_animation_speed      = CBM_OBJ_ANIMATION_SPEED;
	static const ObjectCallbackMask cbm_animation_next_frame = CBM_OBJ_ANIMATION_NEXT_FRAME;
};

/**
 * Handle the animation of the object tile.
 * @param tile The tile to animate.
 */
void AnimateNewObjectTile(TileIndex tile)
{
	const ObjectSpec *spec = ObjectSpec::GetByTile(tile);
	if (spec == NULL || !(spec->flags & OBJECT_FLAG_ANIMATION)) return;

	ObjectAnimationBase::AnimateTile(spec, Object::GetByTile(tile), tile, (spec->flags & OBJECT_FLAG_ANIM_RANDOM_BITS) != 0);
}

/**
 * Trigger the update of animation on a single tile.
 * @param o       The object that got triggered.
 * @param tile    The location of the triggered tile.
 * @param trigger The trigger that is triggered.
 * @param spec    The spec associated with the object.
 */
void TriggerObjectTileAnimation(Object *o, TileIndex tile, ObjectAnimationTrigger trigger, const ObjectSpec *spec)
{
	if (!HasBit(spec->animation.triggers, trigger)) return;

	ObjectAnimationBase::ChangeAnimationFrame(CBID_OBJECT_ANIMATION_START_STOP, spec, o, tile, Random(), trigger);
}

/**
 * Trigger the update of animation on a whole object.
 * @param o       The object that got triggered.
 * @param trigger The trigger that is triggered.
 * @param spec    The spec associated with the object.
 */
void TriggerObjectAnimation(Object *o, ObjectAnimationTrigger trigger, const ObjectSpec *spec)
{
	if (!HasBit(spec->animation.triggers, trigger)) return;

	TILE_AREA_LOOP(tile, o->location) {
		TriggerObjectTileAnimation(o, tile, trigger, spec);
	}
}
