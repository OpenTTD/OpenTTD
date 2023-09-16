/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_airport.cpp NewGRF handling of airports. */

#include "stdafx.h"
#include "debug.h"
#include "timer/timer_game_calendar.h"
#include "newgrf_spritegroup.h"
#include "newgrf_text.h"
#include "station_base.h"
#include "newgrf_class_func.h"
#include "town.h"

#include "safeguards.h"

/**
 * Reset airport classes to their default state.
 * This includes initialising the defaults classes with an empty
 * entry, for standard airports.
 */
template <typename Tspec, typename Tid, Tid Tmax>
/* static */ void NewGRFClass<Tspec, Tid, Tmax>::InsertDefaults()
{
	AirportClass::Get(AirportClass::Allocate('SMAL'))->name = STR_AIRPORT_CLASS_SMALL;
	AirportClass::Get(AirportClass::Allocate('LARG'))->name = STR_AIRPORT_CLASS_LARGE;
	AirportClass::Get(AirportClass::Allocate('HUB_'))->name = STR_AIRPORT_CLASS_HUB;
	AirportClass::Get(AirportClass::Allocate('HELI'))->name = STR_AIRPORT_CLASS_HELIPORTS;
}

template <typename Tspec, typename Tid, Tid Tmax>
bool NewGRFClass<Tspec, Tid, Tmax>::IsUIAvailable(uint) const
{
	return true;
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
		if (_airport_mngr.GetGRFID(type) == 0) return as;
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
	if (TimerGameCalendar::year < this->min_year) return false;
	if (_settings_game.station.never_expire_airports) return true;
	return TimerGameCalendar::year <= this->max_year;
}

/**
 * Check if the airport would be within the map bounds at the given tile.
 * @param table Selected layout table. This affects airport rotation, and therefore dimensions.
 * @param tile Top corner of the airport.
 * @return true iff the airport would be within the map bounds at the given tile.
 */
bool AirportSpec::IsWithinMapBounds(byte table, TileIndex tile) const
{
	if (table >= this->num_table) return false;

	byte w = this->size_x;
	byte h = this->size_y;
	if (this->rotation[table] == DIR_E || this->rotation[table] == DIR_W) Swap(w, h);

	return TileX(tile) + w < Map::SizeX() &&
		TileY(tile) + h < Map::SizeY();
}

/**
 * This function initializes the airportspec array.
 */
void AirportSpec::ResetAirports()
{
	extern const AirportSpec _origin_airport_specs[NEW_AIRPORT_OFFSET];

	auto insert = std::copy(std::begin(_origin_airport_specs), std::end(_origin_airport_specs), std::begin(AirportSpec::specs));
	std::fill(insert, std::end(AirportSpec::specs), AirportSpec{});

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

	if (airport_id == this->invalid_id) {
		GrfMsg(1, "Airport.SetEntitySpec: Too many airports allocated. Ignoring.");
		return;
	}

	*AirportSpec::GetWithoutOverride(airport_id) = *as;

	/* Now add the overrides. */
	for (int i = 0; i < this->max_offset; i++) {
		AirportSpec *overridden_as = AirportSpec::GetWithoutOverride(i);

		if (this->entity_overrides[i] != as->grf_prop.local_id || this->grfid_overrides[i] != as->grf_prop.grffile->grfid) continue;

		overridden_as->grf_prop.override = airport_id;
		overridden_as->enabled = false;
		this->entity_overrides[i] = this->invalid_id;
		this->grfid_overrides[i] = 0;
	}
}

/* virtual */ uint32_t AirportScopeResolver::GetVariable(byte variable, [[maybe_unused]] uint32_t parameter, bool *available) const
{
	switch (variable) {
		case 0x40: return this->layout;
	}

	if (this->st == nullptr) {
		*available = false;
		return UINT_MAX;
	}

	switch (variable) {
		/* Get a variable from the persistent storage */
		case 0x7C: return (this->st->airport.psa != nullptr) ? this->st->airport.psa->GetValue(parameter) : 0;

		case 0xF0: return this->st->facilities;
		case 0xFA: return ClampTo<uint16_t>(this->st->build_date - CalendarTime::DAYS_TILL_ORIGINAL_BASE_YEAR);
	}

	return this->st->GetNewGRFVariable(this->ro, variable, parameter, available);
}

GrfSpecFeature AirportResolverObject::GetFeature() const
{
	return GSF_AIRPORTS;
}

uint32_t AirportResolverObject::GetDebugID() const
{
	return AirportSpec::Get(this->airport_scope.airport_id)->grf_prop.local_id;
}

/* virtual */ uint32_t AirportScopeResolver::GetRandomBits() const
{
	return this->st == nullptr ? 0 : this->st->random_bits;
}

/**
 * Store a value into the object's persistent storage.
 * @param pos Position in the persistent storage to use.
 * @param value Value to store.
 */
/* virtual */ void AirportScopeResolver::StorePSA(uint pos, int32_t value)
{
	if (this->st == nullptr) return;

	if (this->st->airport.psa == nullptr) {
		/* There is no need to create a storage if the value is zero. */
		if (value == 0) return;

		/* Create storage on first modification. */
		uint32_t grfid = (this->ro.grffile != nullptr) ? this->ro.grffile->grfid : 0;
		assert(PersistentStorage::CanAllocateItem());
		this->st->airport.psa = new PersistentStorage(grfid, GSF_AIRPORTS, this->st->airport.tile);
	}
	this->st->airport.psa->StoreValue(pos, value);
}

/**
 * Get the town scope associated with a station, if it exists.
 * On the first call, the town scope is created (if possible).
 * @return Town scope, if available.
 */
TownScopeResolver *AirportResolverObject::GetTown()
{
	if (!this->town_scope) {
		Town *t = nullptr;
		if (this->airport_scope.st != nullptr) {
			t = this->airport_scope.st->town;
		} else if (this->airport_scope.tile != INVALID_TILE) {
			t = ClosestTownFromTile(this->airport_scope.tile, UINT_MAX);
		}
		if (t == nullptr) return nullptr;
		this->town_scope.reset(new TownScopeResolver(*this, t, this->airport_scope.st == nullptr));
	}
	return this->town_scope.get();
}

/**
 * Constructor of the airport resolver.
 * @param tile %Tile for the callback, only valid for airporttile callbacks.
 * @param st %Station of the airport for which the callback is run, or \c nullptr for build gui.
 * @param airport_id Type of airport for which the callback is run.
 * @param layout Layout of the airport to build.
 * @param callback Callback ID.
 * @param param1 First parameter (var 10) of the callback.
 * @param param2 Second parameter (var 18) of the callback.
 */
AirportResolverObject::AirportResolverObject(TileIndex tile, Station *st, byte airport_id, byte layout,
		CallbackID callback, uint32_t param1, uint32_t param2)
	: ResolverObject(AirportSpec::Get(airport_id)->grf_prop.grffile, callback, param1, param2), airport_scope(*this, tile, st, airport_id, layout)
{
	this->root_spritegroup = AirportSpec::Get(airport_id)->grf_prop.spritegroup[0];
}

SpriteID GetCustomAirportSprite(const AirportSpec *as, byte layout)
{
	AirportResolverObject object(INVALID_TILE, nullptr, as->GetIndex(), layout);
	const SpriteGroup *group = object.Resolve();
	if (group == nullptr) return as->preview_sprite;

	return group->GetResult();
}

uint16_t GetAirportCallback(CallbackID callback, uint32_t param1, uint32_t param2, Station *st, TileIndex tile)
{
	AirportResolverObject object(tile, st, st->airport.type, st->airport.layout, callback, param1, param2);
	return object.ResolveCallback();
}

/**
 * Get a custom text for the airport.
 * @param as The airport type's specification.
 * @param layout The layout index.
 * @param callback The callback to call.
 * @return The custom text.
 */
StringID GetAirportTextCallback(const AirportSpec *as, byte layout, uint16_t callback)
{
	AirportResolverObject object(INVALID_TILE, nullptr, as->GetIndex(), layout, (CallbackID)callback);
	uint16_t cb_res = object.ResolveCallback();
	if (cb_res == CALLBACK_FAILED || cb_res == 0x400) return STR_UNDEFINED;
	if (cb_res > 0x400) {
		ErrorUnknownCallbackResult(as->grf_prop.grffile->grfid, callback, cb_res);
		return STR_UNDEFINED;
	}

	return GetGRFStringID(as->grf_prop.grffile->grfid, 0xD000 + cb_res);
}
