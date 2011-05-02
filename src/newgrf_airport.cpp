/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_airport.cpp NewGRF handling of airports. */

#include "stdafx.h"
#include "debug.h"
#include "date_func.h"
#include "newgrf.h"
#include "newgrf_spritegroup.h"
#include "newgrf_text.h"
#include "station_base.h"
#include "newgrf_class_func.h"

/**
 * Reset airport classes to their default state.
 * This includes initialising the defaults classes with an empty
 * entry, for standard airports.
 */
template <typename Tspec, typename Tid, Tid Tmax>
/* static */ void NewGRFClass<Tspec, Tid, Tmax>::InsertDefaults()
{
	AirportClass::SetName(AirportClass::Allocate('SMAL'), STR_AIRPORT_CLASS_SMALL);
	AirportClass::SetName(AirportClass::Allocate('LARG'), STR_AIRPORT_CLASS_LARGE);
	AirportClass::SetName(AirportClass::Allocate('HUB_'), STR_AIRPORT_CLASS_HUB);
	AirportClass::SetName(AirportClass::Allocate('HELI'), STR_AIRPORT_CLASS_HELIPORTS);
}

INSTANTIATE_NEWGRF_CLASS_METHODS(AirportClass, AirportSpec, AirportClassID, APC_MAX)


AirportOverrideManager _airport_mngr(NEW_AIRPORT_OFFSET, NUM_AIRPORTS, AT_INVALID);

AirportSpec AirportSpec::specs[NUM_AIRPORTS]; ///< Airport specifications.

/**
 * Retrieve airport spec for the given airport. If an override is available
 *  it is returned.
 * @param type index of airport
 * @return A pointer to the corresponding AirportSpec
 */
/* static */ const AirportSpec *AirportSpec::Get(byte type)
{
	assert(type < lengthof(AirportSpec::specs));
	const AirportSpec *as = &AirportSpec::specs[type];
	if (type >= NEW_AIRPORT_OFFSET && !as->enabled) {
		byte subst_id = _airport_mngr.GetSubstituteID(type);
		if (subst_id == AT_INVALID) return as;
		as = &AirportSpec::specs[subst_id];
	}
	if (as->grf_prop.override != AT_INVALID) return &AirportSpec::specs[as->grf_prop.override];
	return as;
}

/**
 * Retrieve airport spec for the given airport. Even if an override is
 *  available the base spec is returned.
 * @param type index of airport
 * @return A pointer to the corresponding AirportSpec
 */
/* static */ AirportSpec *AirportSpec::GetWithoutOverride(byte type)
{
	assert(type < lengthof(AirportSpec::specs));
	return &AirportSpec::specs[type];
}

/** Check whether this airport is available to build. */
bool AirportSpec::IsAvailable() const
{
	if (!this->enabled) return false;
	if (_cur_year < this->min_year) return false;
	if (_settings_game.station.never_expire_airports) return true;
	return _cur_year <= this->max_year;
}

/**
 * This function initialize the airportspec array.
 */
void AirportSpec::ResetAirports()
{
	extern const AirportSpec _origin_airport_specs[];
	memset(&AirportSpec::specs, 0, sizeof(AirportSpec::specs));
	memcpy(&AirportSpec::specs, &_origin_airport_specs, sizeof(AirportSpec) * NEW_AIRPORT_OFFSET);

	_airport_mngr.ResetOverride();
}

/**
 * Tie all airportspecs to their class.
 */
void BindAirportSpecs()
{
	for (int i = 0; i < NUM_AIRPORTS; i++) {
		AirportSpec *as = AirportSpec::GetWithoutOverride(i);
		if (as->enabled) AirportClass::Assign(as);
	}
}


void AirportOverrideManager::SetEntitySpec(AirportSpec *as)
{
	byte airport_id = this->AddEntityID(as->grf_prop.local_id, as->grf_prop.grffile->grfid, as->grf_prop.subst_id);

	if (airport_id == invalid_ID) {
		grfmsg(1, "Airport.SetEntitySpec: Too many airports allocated. Ignoring.");
		return;
	}

	memcpy(AirportSpec::GetWithoutOverride(airport_id), as, sizeof(*as));

	/* Now add the overrides. */
	for (int i = 0; i < max_offset; i++) {
		AirportSpec *overridden_as = AirportSpec::GetWithoutOverride(i);

		if (entity_overrides[i] != as->grf_prop.local_id || grfid_overrides[i] != as->grf_prop.grffile->grfid) continue;

		overridden_as->grf_prop.override = airport_id;
		overridden_as->enabled = false;
		entity_overrides[i] = invalid_ID;
		grfid_overrides[i] = 0;
	}
}

uint32 AirportGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	const Station *st = object->u.airport.st;
	byte layout       = object->u.airport.layout;

	if (object->scope == VSG_SCOPE_PARENT) {
		DEBUG(grf, 1, "Parent scope for airports unavailable");
		*available = false;
		return UINT_MAX;
	}

	switch (variable) {
		case 0x40: return layout;
	}

	if (st == NULL) {
		*available = false;
		return UINT_MAX;
	}

	switch (variable) {
		/* Get a variable from the persistent storage */
		case 0x7C: return st->airport.psa.Get(parameter);

		case 0xF0: return st->facilities;
		case 0xFA: return Clamp(st->build_date - DAYS_TILL_ORIGINAL_BASE_YEAR, 0, 65535);
	}

	return st->GetNewGRFVariable(object, variable, parameter, available);
}

static const SpriteGroup *AirportResolveReal(const ResolverObject *object, const RealSpriteGroup *group)
{
	/* Airport action 2s should always have only 1 "loaded" state, but some
	 * times things don't follow the spec... */
	if (group->num_loaded > 0) return group->loaded[0];
	if (group->num_loading > 0) return group->loading[0];

	return NULL;
}

static uint32 AirportGetRandomBits(const ResolverObject *object)
{
	const Station *st = object->u.airport.st;
	const TileIndex tile = object->u.airport.tile;
	return (st == NULL ? 0 : st->random_bits) | (tile == INVALID_TILE ? 0 : GetStationTileRandomBits(tile) << 16);
}

static uint32 AirportGetTriggers(const ResolverObject *object)
{
	return 0;
}

static void AirportSetTriggers(const ResolverObject *object, int triggers)
{
}

static void NewAirportResolver(ResolverObject *res, TileIndex tile, Station *st, byte airport_id, byte layout)
{
	res->GetRandomBits = AirportGetRandomBits;
	res->GetTriggers   = AirportGetTriggers;
	res->SetTriggers   = AirportSetTriggers;
	res->GetVariable   = AirportGetVariable;
	res->ResolveReal   = AirportResolveReal;

	res->psa                  = st != NULL ? &st->airport.psa : NULL;
	res->u.airport.st         = st;
	res->u.airport.airport_id = airport_id;
	res->u.airport.layout     = layout;
	res->u.airport.tile       = tile;

	res->callback        = CBID_NO_CALLBACK;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
	res->count           = 0;

	const AirportSpec *as = AirportSpec::Get(airport_id);
	res->grffile         = as->grf_prop.grffile;
}

SpriteID GetCustomAirportSprite(const AirportSpec *as, byte layout)
{
	const SpriteGroup *group;
	ResolverObject object;

	NewAirportResolver(&object, INVALID_TILE, NULL, as->GetIndex(), layout);

	group = SpriteGroup::Resolve(as->grf_prop.spritegroup[0], &object);
	if (group == NULL) return as->preview_sprite;

	return group->GetResult();
}

uint16 GetAirportCallback(CallbackID callback, uint32 param1, uint32 param2, Station *st, TileIndex tile)
{
	ResolverObject object;

	NewAirportResolver(&object, tile, st, st->airport.type, st->airport.layout);
	object.callback = callback;
	object.callback_param1 = param1;
	object.callback_param2 = param2;

	const SpriteGroup *group = SpriteGroup::Resolve(st->airport.GetSpec()->grf_prop.spritegroup[0], &object);
	if (group == NULL) return CALLBACK_FAILED;

	return group->GetCallbackResult();
}

/**
 * Get a custom text for the airport.
 * @param as The airport type's specification.
 * @param layout The layout index.
 * @param callback The callback to call.
 * @return The custom text.
 */
StringID GetAirportTextCallback(const AirportSpec *as, byte layout, uint16 callback)
{
	const SpriteGroup *group;
	ResolverObject object;

	NewAirportResolver(&object, INVALID_TILE, NULL, as->GetIndex(), layout);
	object.callback = (CallbackID)callback;

	group = SpriteGroup::Resolve(as->grf_prop.spritegroup[0], &object);
	if (group == NULL) return STR_UNDEFINED;

	return GetGRFStringID(as->grf_prop.grffile->grfid, 0xD000 + group->GetResult());
}
