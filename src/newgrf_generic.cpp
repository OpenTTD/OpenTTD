/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_generic.cpp Handling of generic feature callbacks. */

#include "stdafx.h"
#include "debug.h"
#include "newgrf_spritegroup.h"
#include "industrytype.h"
#include "core/random_func.hpp"
#include "newgrf_sound.h"
#include "water_map.h"
#include <list>

#include "safeguards.h"

/** Scope resolver for generic objects and properties. */
struct GenericScopeResolver : public ScopeResolver {
	CargoID cargo_type;
	uint8 default_selection;
	uint8 src_industry;        ///< Source industry substitute type. 0xFF for "town", 0xFE for "unknown".
	uint8 dst_industry;        ///< Destination industry substitute type. 0xFF for "town", 0xFE for "unknown".
	uint8 distance;
	AIConstructionEvent event;
	uint8 count;
	uint8 station_size;

	/**
	 * Generic scope resolver.
	 * @param ro Surrounding resolver.
	 * @param ai_callback Callback comes from the AI.
	 */
	GenericScopeResolver(ResolverObject &ro, bool ai_callback)
		: ScopeResolver(ro), cargo_type(0), default_selection(0), src_industry(0), dst_industry(0), distance(0),
		event(), count(0), station_size(0), ai_callback(ai_callback)
	{
	}

	/* virtual */ uint32 GetVariable(byte variable, uint32 parameter, bool *available) const;

private:
	bool ai_callback; ///< Callback comes from the AI.
};


/** Resolver object for generic objects/properties. */
struct GenericResolverObject : public ResolverObject {
	GenericScopeResolver generic_scope;

	GenericResolverObject(bool ai_callback, CallbackID callback = CBID_NO_CALLBACK);

	/* virtual */ ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0)
	{
		switch (scope) {
			case VSG_SCOPE_SELF: return &this->generic_scope;
			default: return ResolverObject::GetScope(scope, relative);
		}
	}

	/* virtual */ const SpriteGroup *ResolveReal(const RealSpriteGroup *group) const;
};

struct GenericCallback {
	const GRFFile *file;
	const SpriteGroup *group;

	GenericCallback(const GRFFile *file, const SpriteGroup *group) :
		file(file),
		group(group)
	{ }
};

typedef std::list<GenericCallback> GenericCallbackList;

static GenericCallbackList _gcl[GSF_END];


/**
 * Reset all generic feature callback sprite groups.
 */
void ResetGenericCallbacks()
{
	for (uint8 feature = 0; feature < lengthof(_gcl); feature++) {
		_gcl[feature].clear();
	}
}


/**
 * Add a generic feature callback sprite group to the appropriate feature list.
 * @param feature The feature for the callback.
 * @param file The GRF of the callback.
 * @param group The sprite group of the callback.
 */
void AddGenericCallback(uint8 feature, const GRFFile *file, const SpriteGroup *group)
{
	if (feature >= lengthof(_gcl)) {
		grfmsg(5, "AddGenericCallback: Unsupported feature 0x%02X", feature);
		return;
	}

	/* Generic feature callbacks are evaluated in reverse (i.e. the last group
	 * to be added is evaluated first, etc) thus we push the group to the
	 * beginning of the list so a standard iterator will do the right thing. */
	_gcl[feature].push_front(GenericCallback(file, group));
}

/* virtual */ uint32 GenericScopeResolver::GetVariable(byte variable, uint32 parameter, bool *available) const
{
	if (this->ai_callback) {
		switch (variable) {
			case 0x40: return this->ro.grffile->cargo_map[this->cargo_type];

			case 0x80: return this->cargo_type;
			case 0x81: return CargoSpec::Get(this->cargo_type)->bitnum;
			case 0x82: return this->default_selection;
			case 0x83: return this->src_industry;
			case 0x84: return this->dst_industry;
			case 0x85: return this->distance;
			case 0x86: return this->event;
			case 0x87: return this->count;
			case 0x88: return this->station_size;

			default: break;
		}
	}

	DEBUG(grf, 1, "Unhandled generic feature variable 0x%02X", variable);

	*available = false;
	return UINT_MAX;
}


/* virtual */ const SpriteGroup *GenericResolverObject::ResolveReal(const RealSpriteGroup *group) const
{
	if (group->num_loaded == 0) return NULL;

	return group->loaded[0];
}

/**
 * Generic resolver.
 * @param ai_callback Callback comes from the AI.
 * @param callback Callback ID.
 */
GenericResolverObject::GenericResolverObject(bool ai_callback, CallbackID callback) : ResolverObject(NULL, callback), generic_scope(*this, ai_callback)
{
}


/**
 * Follow a generic feature callback list and return the first successful
 * answer
 * @param feature GRF Feature of callback
 * @param object  pre-populated resolver object
 * @param param1_grfv7 callback_param1 for GRFs up to version 7.
 * @param param1_grfv8 callback_param1 for GRFs from version 8 on.
 * @param[out] file Optionally returns the GRFFile which made the final decision for the callback result. May be NULL if not required.
 * @return callback value if successful or CALLBACK_FAILED
 */
static uint16 GetGenericCallbackResult(uint8 feature, ResolverObject &object, uint32 param1_grfv7, uint32 param1_grfv8, const GRFFile **file)
{
	assert(feature < lengthof(_gcl));

	/* Test each feature callback sprite group. */
	for (GenericCallbackList::const_iterator it = _gcl[feature].begin(); it != _gcl[feature].end(); ++it) {
		object.grffile = it->file;
		object.root_spritegroup = it->group;
		/* Set callback param based on GRF version. */
		object.callback_param1 = it->file->grf_version >= 8 ? param1_grfv8 : param1_grfv7;
		uint16 result = object.ResolveCallback();
		if (result == CALLBACK_FAILED) continue;

		/* Return NewGRF file if necessary */
		if (file != NULL) *file = it->file;

		return result;
	}

	/* No callback returned a valid result, so we've failed. */
	return CALLBACK_FAILED;
}


/**
 * 'Execute' an AI purchase selection callback
 *
 * @param feature GRF Feature to call callback for.
 * @param cargo_type Cargotype to pass to callback. (Variable 80)
 * @param default_selection 'Default selection' to pass to callback. (Variable 82)
 * @param src_industry 'Source industry type' to pass to callback. (Variable 83)
 * @param dst_industry 'Destination industry type' to pass to callback. (Variable 84)
 * @param distance 'Distance between source and destination' to pass to callback. (Variable 85)
 * @param event 'AI construction event' to pass to callback. (Variable 86)
 * @param count 'Construction number' to pass to callback. (Variable 87)
 * @param station_size 'Station size' to pass to callback. (Variable 88)
 * @param[out] file Optionally returns the GRFFile which made the final decision for the callback result. May be NULL if not required.
 * @return callback value if successful or CALLBACK_FAILED
 */
uint16 GetAiPurchaseCallbackResult(uint8 feature, CargoID cargo_type, uint8 default_selection, IndustryType src_industry, IndustryType dst_industry, uint8 distance, AIConstructionEvent event, uint8 count, uint8 station_size, const GRFFile **file)
{
	GenericResolverObject object(true, CBID_GENERIC_AI_PURCHASE_SELECTION);

	if (src_industry != IT_AI_UNKNOWN && src_industry != IT_AI_TOWN) {
		const IndustrySpec *is = GetIndustrySpec(src_industry);
		/* If this is no original industry, use the substitute type */
		if (is->grf_prop.subst_id != INVALID_INDUSTRYTYPE) src_industry = is->grf_prop.subst_id;
	}

	if (dst_industry != IT_AI_UNKNOWN && dst_industry != IT_AI_TOWN) {
		const IndustrySpec *is = GetIndustrySpec(dst_industry);
		/* If this is no original industry, use the substitute type */
		if (is->grf_prop.subst_id != INVALID_INDUSTRYTYPE) dst_industry = is->grf_prop.subst_id;
	}

	object.generic_scope.cargo_type        = cargo_type;
	object.generic_scope.default_selection = default_selection;
	object.generic_scope.src_industry      = src_industry;
	object.generic_scope.dst_industry      = dst_industry;
	object.generic_scope.distance          = distance;
	object.generic_scope.event             = event;
	object.generic_scope.count             = count;
	object.generic_scope.station_size      = station_size;

	uint16 callback = GetGenericCallbackResult(feature, object, 0, 0, file);
	if (callback != CALLBACK_FAILED) callback = GB(callback, 0, 8);
	return callback;
}


/**
 * 'Execute' the ambient sound effect callback.
 * @param tile Tile the sound effect should be generated for.
 */
void AmbientSoundEffectCallback(TileIndex tile)
{
	assert(IsTileType(tile, MP_CLEAR) || IsTileType(tile, MP_TREES) || IsTileType(tile, MP_WATER));

	/* Only run every 1/200-th time. */
	uint32 r; // Save for later
	if (!Chance16R(1, 200, r) || !_settings_client.sound.ambient) return;

	/* Prepare resolver object. */
	GenericResolverObject object(false, CBID_SOUNDS_AMBIENT_EFFECT);

	uint32 param1_v7 = GetTileType(tile) << 28 | Clamp(TileHeight(tile), 0, 15) << 24 | GB(r, 16, 8) << 16 | GetTerrainType(tile);
	uint32 param1_v8 = GetTileType(tile) << 24 | GetTileZ(tile) << 16 | GB(r, 16, 8) << 8 | (HasTileWaterClass(tile) ? GetWaterClass(tile) : 0) << 3 | GetTerrainType(tile);

	/* Run callback. */
	const GRFFile *grf_file;
	uint16 callback = GetGenericCallbackResult(GSF_SOUNDFX, object, param1_v7, param1_v8, &grf_file);

	if (callback != CALLBACK_FAILED) PlayTileSound(grf_file, callback, tile);
}
