/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_debug_data.h Data 'tables' for NewGRF debugging. */

#include "../newgrf_house.h"
#include "../newgrf_engine.h"
#include "../newgrf_roadtype.h"
#include "../newgrf_roadstop.h"

/* Helper for filling property tables */
#define NIP(prop, base_class, variable, type, name) { name, [] (const void *b) -> uint32_t { return static_cast<const base_class *>(b)->variable; }, prop, type }

/* Helper for filling callback tables */
#define NIC(cb_id, base_class, variable, bit) { #cb_id, [] (const void *b) -> uint32_t { return static_cast<const base_class *>(b)->variable.base(); }, bit, cb_id }

/* Helper for filling variable tables */
#define NIV(var, name) { name, var }


/*** NewGRF Vehicles ***/

#define NICV(cb_id, bit) NIC(cb_id, Engine, info.callback_mask, bit)
static const NICallback _nic_vehicles[] = {
	NICV(CBID_VEHICLE_VISUAL_EFFECT,         VehicleCallbackMask::VisualEffect),
	NICV(CBID_VEHICLE_LENGTH,                VehicleCallbackMask::Length),
	NICV(CBID_VEHICLE_LOAD_AMOUNT,           VehicleCallbackMask::LoadAmount),
	NICV(CBID_VEHICLE_REFIT_CAPACITY,        VehicleCallbackMask::RefitCapacity),
	NICV(CBID_VEHICLE_ARTIC_ENGINE,          VehicleCallbackMask::ArticEngine),
	NICV(CBID_VEHICLE_CARGO_SUFFIX,          VehicleCallbackMask::CargoSuffix),
	NICV(CBID_TRAIN_ALLOW_WAGON_ATTACH,      std::monostate{}),
	NICV(CBID_VEHICLE_ADDITIONAL_TEXT,       std::monostate{}),
	NICV(CBID_VEHICLE_COLOUR_MAPPING,        VehicleCallbackMask::ColourRemap),
	NICV(CBID_VEHICLE_START_STOP_CHECK,      std::monostate{}),
	NICV(CBID_VEHICLE_32DAY_CALLBACK,        std::monostate{}),
	NICV(CBID_VEHICLE_SOUND_EFFECT,          VehicleCallbackMask::SoundEffect),
	NICV(CBID_VEHICLE_AUTOREPLACE_SELECTION, std::monostate{}),
	NICV(CBID_VEHICLE_MODIFY_PROPERTY,       std::monostate{}),
	NICV(CBID_VEHICLE_NAME,                  VehicleCallbackMask::Name),
};


static const NIVariable _niv_vehicles[] = {
	NIV(0x40, "position in consist and length"),
	NIV(0x41, "position and length of chain of same vehicles"),
	NIV(0x42, "transported cargo types"),
	NIV(0x43, "player info"),
	NIV(0x44, "aircraft info"),
	NIV(0x45, "curvature info"),
	NIV(0x46, "motion counter"),
	NIV(0x47, "vehicle cargo info"),
	NIV(0x48, "vehicle type info"),
	NIV(0x49, "year of construction"),
	NIV(0x4A, "current rail/road type info"),
	NIV(0x4B, "long date of last service"),
	NIV(0x4C, "current max speed"),
	NIV(0x4D, "position in articulated vehicle"),
	NIV(0x60, "count vehicle id occurrences"),
	/* 0x61 not useful, since it requires register 0x10F */
	NIV(0x62, "curvature/position difference to other vehicle"),
	NIV(0x63, "tile compatibility wrt. track-type"),
};

class NIHVehicle : public NIHelper {
	bool IsInspectable(uint index) const override        { return Vehicle::Get(index)->GetGRF() != nullptr; }
	uint GetParent(uint index) const override            { const Vehicle *first = Vehicle::Get(index)->First(); return GetInspectWindowNumber(GetGrfSpecFeature(first->type), first->index); }
	const void *GetInstance(uint index)const override    { return Vehicle::Get(index); }
	const void *GetSpec(uint index) const override       { return Vehicle::Get(index)->GetEngine(); }
	std::string GetName(uint index) const override       { return GetString(STR_VEHICLE_NAME, index); }
	uint32_t GetGRFID(uint index) const override                   { return Vehicle::Get(index)->GetGRFID(); }
	std::span<const BadgeID> GetBadges(uint index) const override { return Vehicle::Get(index)->GetEngine()->badges; }

	uint Resolve(uint index, uint var, uint param, bool &avail) const override
	{
		Vehicle *v = Vehicle::Get(index);
		VehicleResolverObject ro(v->engine_type, v, VehicleResolverObject::WO_CACHED);
		return ro.GetScope(VSG_SCOPE_SELF)->GetVariable(var, param, avail);
	}
};

static const NIFeature _nif_vehicle = {
	{},
	_nic_vehicles,
	_niv_vehicles,
	std::make_unique<NIHVehicle>(),
};


/*** NewGRF station (tiles) ***/

#define NICS(cb_id, bit) NIC(cb_id, StationSpec, callback_mask, bit)
static const NICallback _nic_stations[] = {
	NICS(CBID_STATION_AVAILABILITY,     StationCallbackMask::Avail),
	NICS(CBID_STATION_DRAW_TILE_LAYOUT, StationCallbackMask::DrawTileLayout),
	NICS(CBID_STATION_BUILD_TILE_LAYOUT,std::monostate{}),
	NICS(CBID_STATION_ANIMATION_TRIGGER, std::monostate{}),
	NICS(CBID_STATION_ANIMATION_NEXT_FRAME, StationCallbackMask::AnimationNextFrame),
	NICS(CBID_STATION_ANIMATION_SPEED,  StationCallbackMask::AnimationSpeed),
	NICS(CBID_STATION_LAND_SLOPE_CHECK, StationCallbackMask::SlopeCheck),
};

static const NIVariable _niv_stations[] = {
	NIV(0x40, "platform info and relative position"),
	NIV(0x41, "platform info and relative position for individually built sections"),
	NIV(0x42, "terrain and track type"),
	NIV(0x43, "player info"),
	NIV(0x44, "path signalling info"),
	NIV(0x45, "rail continuation info"),
	NIV(0x46, "platform info and relative position from middle"),
	NIV(0x47, "platform info and relative position from middle for individually built sections"),
	NIV(0x48, "bitmask of accepted cargoes"),
	NIV(0x49, "platform info and relative position of same-direction section"),
	NIV(0x4A, "current animation frame"),
	NIV(0x60, "amount of cargo waiting"),
	NIV(0x61, "time since last cargo pickup"),
	NIV(0x62, "rating of cargo"),
	NIV(0x63, "time spent on route"),
	NIV(0x64, "information about last vehicle picking cargo up"),
	NIV(0x65, "amount of cargo acceptance"),
	NIV(0x66, "animation frame of nearby tile"),
	NIV(0x67, "land info of nearby tiles"),
	NIV(0x68, "station info of nearby tiles"),
	NIV(0x69, "information about cargo accepted in the past"),
	NIV(0x6A, "GRFID of nearby station tiles"),
	NIV(0x6B, "station ID of nearby tiles"),
};

class NIHStation : public NIHelper {
	bool IsInspectable(uint index) const override        { return GetStationSpec(TileIndex{index}) != nullptr; }
	uint GetParent(uint index) const override            { return GetInspectWindowNumber(GSF_FAKE_TOWNS, Station::GetByTile(TileIndex{index})->town->index); }
	const void *GetInstance(uint ) const override        { return nullptr; }
	const void *GetSpec(uint index) const override       { return GetStationSpec(TileIndex{index}); }
	std::string GetName(uint index) const override       { return GetString(STR_NEWGRF_INSPECT_CAPTION_OBJECT_AT, STR_STATION_NAME, GetStationIndex(index), index); }
	uint32_t GetGRFID(uint index) const override           { return (this->IsInspectable(index)) ? GetStationSpec(TileIndex{index})->grf_prop.grfid : 0; }
	std::span<const BadgeID> GetBadges(uint index) const override { return this->IsInspectable(index) ? GetStationSpec(TileIndex{index})->badges : std::span<const BadgeID>{}; }

	uint Resolve(uint index, uint var, uint param, bool &avail) const override
	{
		TileIndex tile{index};
		StationResolverObject ro(GetStationSpec(tile), Station::GetByTile(tile), tile);
		return ro.GetScope(VSG_SCOPE_SELF)->GetVariable(var, param, avail);
	}
};

static const NIFeature _nif_station = {
	{},
	_nic_stations,
	_niv_stations,
	std::make_unique<NIHStation>(),
};


/*** NewGRF house tiles ***/

#define NICH(cb_id, bit) NIC(cb_id, HouseSpec, callback_mask, bit)
static const NICallback _nic_house[] = {
	NICH(CBID_HOUSE_ALLOW_CONSTRUCTION,        HouseCallbackMask::AllowConstruction),
	NICH(CBID_HOUSE_ANIMATION_NEXT_FRAME,      HouseCallbackMask::AnimationNextFrame),
	NICH(CBID_HOUSE_ANIMATION_TRIGGER_TILE_LOOP, HouseCallbackMask::AnimationTriggerTileLoop),
	NICH(CBID_HOUSE_ANIMATION_TRIGGER_CONSTRUCTION_STAGE_CHANGED, HouseCallbackMask::AnimationTriggerConstructionStageChanged),
	NICH(CBID_HOUSE_COLOUR,                    HouseCallbackMask::Colour),
	NICH(CBID_HOUSE_CARGO_ACCEPTANCE,          HouseCallbackMask::CargoAcceptance),
	NICH(CBID_HOUSE_ANIMATION_SPEED,           HouseCallbackMask::AnimationSpeed),
	NICH(CBID_HOUSE_DESTRUCTION,               HouseCallbackMask::Destruction),
	NICH(CBID_HOUSE_ACCEPT_CARGO,              HouseCallbackMask::AcceptCargo),
	NICH(CBID_HOUSE_PRODUCE_CARGO,             HouseCallbackMask::ProduceCargo),
	NICH(CBID_HOUSE_DENY_DESTRUCTION,          HouseCallbackMask::DenyDestruction),
	NICH(CBID_HOUSE_ANIMATION_TRIGGER_WATCHED_CARGO_ACCEPTED, std::monostate{}),
	NICH(CBID_HOUSE_CUSTOM_NAME,               std::monostate{}),
	NICH(CBID_HOUSE_DRAW_FOUNDATIONS,          HouseCallbackMask::DrawFoundations),
	NICH(CBID_HOUSE_AUTOSLOPE,                 HouseCallbackMask::Autoslope),
};

static const NIVariable _niv_house[] = {
	NIV(0x40, "construction stage of tile and pseudo-random value"),
	NIV(0x41, "age of building in years"),
	NIV(0x42, "town zone"),
	NIV(0x43, "terrain type"),
	NIV(0x44, "building counts"),
	NIV(0x45, "town expansion bits"),
	NIV(0x46, "current animation frame"),
	NIV(0x47, "xy coordinate of the building"),
	NIV(0x60, "other building counts (old house type)"),
	NIV(0x61, "other building counts (new house type)"),
	NIV(0x62, "land info of nearby tiles"),
	NIV(0x63, "current animation frame of nearby house tile"),
	NIV(0x64, "cargo acceptance history of nearby stations"),
	NIV(0x65, "distance of nearest house matching a given criterion"),
	NIV(0x66, "class and ID of nearby house tile"),
	NIV(0x67, "GRFID of nearby house tile"),
};

class NIHHouse : public NIHelper {
	bool IsInspectable(uint index) const override        { return HouseSpec::Get(GetHouseType(index))->grf_prop.HasGrfFile(); }
	uint GetParent(uint index) const override            { return GetInspectWindowNumber(GSF_FAKE_TOWNS, GetTownIndex(index)); }
	const void *GetInstance(uint)const override          { return nullptr; }
	const void *GetSpec(uint index) const override       { return HouseSpec::Get(GetHouseType(index)); }
	std::string GetName(uint index) const override       { return GetString(STR_NEWGRF_INSPECT_CAPTION_OBJECT_AT, STR_TOWN_NAME, GetTownIndex(index), index); }
	uint32_t GetGRFID(uint index) const override           { return (this->IsInspectable(index)) ? HouseSpec::Get(GetHouseType(index))->grf_prop.grfid : 0; }
	std::span<const BadgeID> GetBadges(uint index) const override { return HouseSpec::Get(GetHouseType(index))->badges; }

	uint Resolve(uint index, uint var, uint param, bool &avail) const override
	{
		TileIndex tile{index};
		HouseResolverObject ro(GetHouseType(tile), tile, Town::GetByTile(tile));
		return ro.GetScope(VSG_SCOPE_SELF)->GetVariable(var, param, avail);
	}
};

static const NIFeature _nif_house = {
	{},
	_nic_house,
	_niv_house,
	std::make_unique<NIHHouse>(),
};


/*** NewGRF industry tiles ***/

#define NICIT(cb_id, bit) NIC(cb_id, IndustryTileSpec, callback_mask, bit)
static const NICallback _nic_industrytiles[] = {
	NICIT(CBID_INDTILE_ANIMATION_TRIGGER, std::monostate{}),
	NICIT(CBID_INDTILE_ANIMATION_NEXT_FRAME, IndustryTileCallbackMask::AnimationNextFrame),
	NICIT(CBID_INDTILE_ANIMATION_SPEED,  IndustryTileCallbackMask::AnimationSpeed),
	NICIT(CBID_INDTILE_CARGO_ACCEPTANCE, IndustryTileCallbackMask::CargoAcceptance),
	NICIT(CBID_INDTILE_ACCEPT_CARGO,     IndustryTileCallbackMask::AcceptCargo),
	NICIT(CBID_INDTILE_SHAPE_CHECK,      IndustryTileCallbackMask::ShapeCheck),
	NICIT(CBID_INDTILE_DRAW_FOUNDATIONS, IndustryTileCallbackMask::DrawFoundations),
	NICIT(CBID_INDTILE_AUTOSLOPE,        IndustryTileCallbackMask::Autoslope),
};

static const NIVariable _niv_industrytiles[] = {
	NIV(0x40, "construction stage of tile"),
	NIV(0x41, "ground type"),
	NIV(0x42, "current town zone in nearest town"),
	NIV(0x43, "relative position"),
	NIV(0x44, "animation frame"),
	NIV(0x60, "land info of nearby tiles"),
	NIV(0x61, "animation stage of nearby tiles"),
	NIV(0x62, "get industry or airport tile ID at offset"),
};

class NIHIndustryTile : public NIHelper {
	bool IsInspectable(uint index) const override        { return GetIndustryTileSpec(GetIndustryGfx(index))->grf_prop.HasGrfFile(); }
	uint GetParent(uint index) const override            { return GetInspectWindowNumber(GSF_INDUSTRIES, GetIndustryIndex(index)); }
	const void *GetInstance(uint)const override          { return nullptr; }
	const void *GetSpec(uint index) const override       { return GetIndustryTileSpec(GetIndustryGfx(index)); }
	std::string GetName(uint index) const override       { return GetString(STR_NEWGRF_INSPECT_CAPTION_OBJECT_AT, STR_INDUSTRY_NAME, GetIndustryIndex(index), index); }
	uint32_t GetGRFID(uint index) const override           { return (this->IsInspectable(index)) ? GetIndustryTileSpec(GetIndustryGfx(index))->grf_prop.grfid : 0; }
	std::span<const BadgeID> GetBadges(uint index) const override { return GetIndustryTileSpec(GetIndustryGfx(index))->badges; }

	uint Resolve(uint index, uint var, uint param, bool &avail) const override
	{
		TileIndex tile{index};
		IndustryTileResolverObject ro(GetIndustryGfx(tile), tile, Industry::GetByTile(tile));
		return ro.GetScope(VSG_SCOPE_SELF)->GetVariable(var, param, avail);
	}
};

static const NIFeature _nif_industrytile = {
	{},
	_nic_industrytiles,
	_niv_industrytiles,
	std::make_unique<NIHIndustryTile>(),
};


/*** NewGRF industries ***/
#define NIP_PRODUCED_CARGO(prop, base_class, slot, type, name) { name, [] (const void *b) -> uint32_t { return static_cast<const base_class *>(b)->GetProduced(slot).cargo; }, prop, type }
#define NIP_ACCEPTED_CARGO(prop, base_class, slot, type, name) { name, [] (const void *b) -> uint32_t { return static_cast<const base_class *>(b)->GetAccepted(slot).cargo; }, prop, type }

static const NIProperty _nip_industries[] = {
	NIP_PRODUCED_CARGO(0x25, Industry,  0, NIT_CARGO, "produced cargo 0"),
	NIP_PRODUCED_CARGO(0x25, Industry,  1, NIT_CARGO, "produced cargo 1"),
	NIP_PRODUCED_CARGO(0x25, Industry,  2, NIT_CARGO, "produced cargo 2"),
	NIP_PRODUCED_CARGO(0x25, Industry,  3, NIT_CARGO, "produced cargo 3"),
	NIP_PRODUCED_CARGO(0x25, Industry,  4, NIT_CARGO, "produced cargo 4"),
	NIP_PRODUCED_CARGO(0x25, Industry,  5, NIT_CARGO, "produced cargo 5"),
	NIP_PRODUCED_CARGO(0x25, Industry,  6, NIT_CARGO, "produced cargo 6"),
	NIP_PRODUCED_CARGO(0x25, Industry,  7, NIT_CARGO, "produced cargo 7"),
	NIP_PRODUCED_CARGO(0x25, Industry,  8, NIT_CARGO, "produced cargo 8"),
	NIP_PRODUCED_CARGO(0x25, Industry,  9, NIT_CARGO, "produced cargo 9"),
	NIP_PRODUCED_CARGO(0x25, Industry, 10, NIT_CARGO, "produced cargo 10"),
	NIP_PRODUCED_CARGO(0x25, Industry, 11, NIT_CARGO, "produced cargo 11"),
	NIP_PRODUCED_CARGO(0x25, Industry, 12, NIT_CARGO, "produced cargo 12"),
	NIP_PRODUCED_CARGO(0x25, Industry, 13, NIT_CARGO, "produced cargo 13"),
	NIP_PRODUCED_CARGO(0x25, Industry, 14, NIT_CARGO, "produced cargo 14"),
	NIP_PRODUCED_CARGO(0x25, Industry, 15, NIT_CARGO, "produced cargo 15"),
	NIP_ACCEPTED_CARGO(0x26, Industry,  0, NIT_CARGO, "accepted cargo 0"),
	NIP_ACCEPTED_CARGO(0x26, Industry,  1, NIT_CARGO, "accepted cargo 1"),
	NIP_ACCEPTED_CARGO(0x26, Industry,  2, NIT_CARGO, "accepted cargo 2"),
	NIP_ACCEPTED_CARGO(0x26, Industry,  3, NIT_CARGO, "accepted cargo 3"),
	NIP_ACCEPTED_CARGO(0x26, Industry,  4, NIT_CARGO, "accepted cargo 4"),
	NIP_ACCEPTED_CARGO(0x26, Industry,  5, NIT_CARGO, "accepted cargo 5"),
	NIP_ACCEPTED_CARGO(0x26, Industry,  6, NIT_CARGO, "accepted cargo 6"),
	NIP_ACCEPTED_CARGO(0x26, Industry,  7, NIT_CARGO, "accepted cargo 7"),
	NIP_ACCEPTED_CARGO(0x26, Industry,  8, NIT_CARGO, "accepted cargo 8"),
	NIP_ACCEPTED_CARGO(0x26, Industry,  9, NIT_CARGO, "accepted cargo 9"),
	NIP_ACCEPTED_CARGO(0x26, Industry, 10, NIT_CARGO, "accepted cargo 10"),
	NIP_ACCEPTED_CARGO(0x26, Industry, 11, NIT_CARGO, "accepted cargo 11"),
	NIP_ACCEPTED_CARGO(0x26, Industry, 12, NIT_CARGO, "accepted cargo 12"),
	NIP_ACCEPTED_CARGO(0x26, Industry, 13, NIT_CARGO, "accepted cargo 13"),
	NIP_ACCEPTED_CARGO(0x26, Industry, 14, NIT_CARGO, "accepted cargo 14"),
	NIP_ACCEPTED_CARGO(0x26, Industry, 15, NIT_CARGO, "accepted cargo 15"),
};

#undef NIP_PRODUCED_CARGO
#undef NIP_ACCEPTED_CARGO

#define NICI(cb_id, bit) NIC(cb_id, IndustrySpec, callback_mask, bit)
static const NICallback _nic_industries[] = {
	NICI(CBID_INDUSTRY_PROBABILITY,          IndustryCallbackMask::Probability),
	NICI(CBID_INDUSTRY_LOCATION,             IndustryCallbackMask::Location),
	NICI(CBID_INDUSTRY_PRODUCTION_CHANGE,    IndustryCallbackMask::ProductionChange),
	NICI(CBID_INDUSTRY_MONTHLYPROD_CHANGE,   IndustryCallbackMask::MonthlyProdChange),
	NICI(CBID_INDUSTRY_CARGO_SUFFIX,         IndustryCallbackMask::CargoSuffix),
	NICI(CBID_INDUSTRY_FUND_MORE_TEXT,       IndustryCallbackMask::FundMoreText),
	NICI(CBID_INDUSTRY_WINDOW_MORE_TEXT,     IndustryCallbackMask::WindowMoreText),
	NICI(CBID_INDUSTRY_SPECIAL_EFFECT,       IndustryCallbackMask::SpecialEffect),
	NICI(CBID_INDUSTRY_REFUSE_CARGO,         IndustryCallbackMask::RefuseCargo),
	NICI(CBID_INDUSTRY_DECIDE_COLOUR,        IndustryCallbackMask::DecideColour),
	NICI(CBID_INDUSTRY_INPUT_CARGO_TYPES,    IndustryCallbackMask::InputCargoTypes),
	NICI(CBID_INDUSTRY_OUTPUT_CARGO_TYPES,   IndustryCallbackMask::OutputCargoTypes),
	NICI(CBID_INDUSTRY_PROD_CHANGE_BUILD,    IndustryCallbackMask::ProdChangeBuild),
};

static const NIVariable _niv_industries[] = {
	NIV(0x40, "waiting cargo 0"),
	NIV(0x41, "waiting cargo 1"),
	NIV(0x42, "waiting cargo 2"),
	NIV(0x43, "distance to closest dry/land tile"),
	NIV(0x44, "layout number"),
	NIV(0x45, "player info"),
	NIV(0x46, "industry construction date"),
	NIV(0x60, "get industry tile ID at offset"),
	NIV(0x61, "get random tile bits at offset"),
	NIV(0x62, "land info of nearby tiles"),
	NIV(0x63, "animation stage of nearby tiles"),
	NIV(0x64, "distance on nearest industry with given type"),
	NIV(0x65, "get town zone and Manhattan distance of closest town"),
	NIV(0x66, "get square of Euclidean distance of closes town"),
	NIV(0x67, "count of industry and distance of closest instance"),
	NIV(0x68, "count of industry and distance of closest instance with layout filter"),
	NIV(0x69, "produced cargo waiting"),
	NIV(0x6A, "cargo produced this month"),
	NIV(0x6B, "cargo transported this month"),
	NIV(0x6C, "cargo produced last month"),
	NIV(0x6D, "cargo transported last month"),
	NIV(0x6E, "date since cargo was delivered"),
	NIV(0x6F, "waiting input cargo"),
	NIV(0x70, "production rate"),
	NIV(0x71, "percentage of cargo transported last month"),
};

class NIHIndustry : public NIHelper {
	bool IsInspectable(uint index) const override        { return GetIndustrySpec(Industry::Get(index)->type)->grf_prop.HasGrfFile(); }
	uint GetParent(uint index) const override            { return GetInspectWindowNumber(GSF_FAKE_TOWNS, Industry::Get(index)->town->index); }
	const void *GetInstance(uint index)const override    { return Industry::Get(index); }
	const void *GetSpec(uint index) const override       { return GetIndustrySpec(Industry::Get(index)->type); }
	std::string GetName(uint index) const override       { return GetString(STR_INDUSTRY_NAME, index); }
	uint32_t GetGRFID(uint index) const override           { return (this->IsInspectable(index)) ? GetIndustrySpec(Industry::Get(index)->type)->grf_prop.grfid : 0; }
	std::span<const BadgeID> GetBadges(uint index) const override { return GetIndustrySpec(Industry::Get(index)->type)->badges; }

	uint Resolve(uint index, uint var, uint param, bool &avail) const override
	{
		Industry *i = Industry::Get(index);
		IndustriesResolverObject ro(i->location.tile, i, i->type);
		return ro.GetScope(VSG_SCOPE_SELF)->GetVariable(var, param, avail);
	}

	const std::span<int32_t> GetPSA(uint index, uint32_t) const override
	{
		const Industry *i = (const Industry *)this->GetInstance(index);
		if (i->psa == nullptr) return {};
		return i->psa->storage;
	}
};

static const NIFeature _nif_industry = {
	_nip_industries,
	_nic_industries,
	_niv_industries,
	std::make_unique<NIHIndustry>(),
};


/*** NewGRF objects ***/

#define NICO(cb_id, bit) NIC(cb_id, ObjectSpec, callback_mask, bit)
static const NICallback _nic_objects[] = {
	NICO(CBID_OBJECT_LAND_SLOPE_CHECK,     ObjectCallbackMask::SlopeCheck),
	NICO(CBID_OBJECT_ANIMATION_NEXT_FRAME, ObjectCallbackMask::AnimationNextFrame),
	NICO(CBID_OBJECT_ANIMATION_TRIGGER, std::monostate{}),
	NICO(CBID_OBJECT_ANIMATION_SPEED,      ObjectCallbackMask::AnimationSpeed),
	NICO(CBID_OBJECT_COLOUR,               ObjectCallbackMask::Colour),
	NICO(CBID_OBJECT_FUND_MORE_TEXT,       ObjectCallbackMask::FundMoreText),
	NICO(CBID_OBJECT_AUTOSLOPE,            ObjectCallbackMask::Autoslope),
};

static const NIVariable _niv_objects[] = {
	NIV(0x40, "relative position"),
	NIV(0x41, "tile information"),
	NIV(0x42, "construction date"),
	NIV(0x43, "animation counter"),
	NIV(0x44, "object founder"),
	NIV(0x45, "get town zone and Manhattan distance of closest town"),
	NIV(0x46, "get square of Euclidean distance of closes town"),
	NIV(0x47, "colour"),
	NIV(0x48, "view"),
	NIV(0x60, "get object ID at offset"),
	NIV(0x61, "get random tile bits at offset"),
	NIV(0x62, "land info of nearby tiles"),
	NIV(0x63, "animation stage of nearby tiles"),
	NIV(0x64, "distance on nearest object with given type"),
};

class NIHObject : public NIHelper {
	bool IsInspectable(uint index) const override        { return ObjectSpec::GetByTile(TileIndex{index})->grf_prop.HasGrfFile(); }
	uint GetParent(uint index) const override            { return GetInspectWindowNumber(GSF_FAKE_TOWNS, Object::GetByTile(TileIndex{index})->town->index); }
	const void *GetInstance(uint index)const override    { return Object::GetByTile(TileIndex{index}); }
	const void *GetSpec(uint index) const override       { return ObjectSpec::GetByTile(TileIndex{index}); }
	std::string GetName(uint index) const override       { return GetString(STR_NEWGRF_INSPECT_CAPTION_OBJECT_AT, STR_NEWGRF_INSPECT_CAPTION_OBJECT_AT_OBJECT, INVALID_STRING_ID, index); }
	uint32_t GetGRFID(uint index) const override           { return (this->IsInspectable(index)) ? ObjectSpec::GetByTile(TileIndex{index})->grf_prop.grfid : 0; }
	std::span<const BadgeID> GetBadges(uint index) const override { return ObjectSpec::GetByTile(TileIndex{index})->badges; }

	uint Resolve(uint index, uint var, uint param, bool &avail) const override
	{
		TileIndex tile{index};
		ObjectResolverObject ro(ObjectSpec::GetByTile(tile), Object::GetByTile(tile), tile);
		return ro.GetScope(VSG_SCOPE_SELF)->GetVariable(var, param, avail);
	}
};

static const NIFeature _nif_object = {
	{},
	_nic_objects,
	_niv_objects,
	std::make_unique<NIHObject>(),
};


/*** NewGRF rail types ***/

static const NIVariable _niv_railtypes[] = {
	NIV(0x40, "terrain type"),
	NIV(0x41, "enhanced tunnels"),
	NIV(0x42, "level crossing status"),
	NIV(0x43, "construction date"),
	NIV(0x44, "town zone"),
	NIV(0x45, "track types"),
};

class NIHRailType : public NIHelper {
	bool IsInspectable(uint) const override              { return true; }
	uint GetParent(uint) const override                  { return UINT32_MAX; }
	const void *GetInstance(uint) const override         { return nullptr; }
	const void *GetSpec(uint) const override             { return nullptr; }
	std::string GetName(uint index) const override       { return GetString(STR_NEWGRF_INSPECT_CAPTION_OBJECT_AT, STR_NEWGRF_INSPECT_CAPTION_OBJECT_AT_RAIL_TYPE, INVALID_STRING_ID, index); }
	uint32_t GetGRFID(uint) const override               { return 0; }
	std::span<const BadgeID> GetBadges(uint index) const override { return GetRailTypeInfo(GetRailType(TileIndex{index}))->badges; }

	uint Resolve(uint index, uint var, uint param, bool &avail) const override
	{
		/* There is no unique GRFFile for the tile. Multiple GRFs can define different parts of the railtype.
		 * However, currently the NewGRF Debug GUI does not display variables depending on the GRF (like 0x7F) anyway. */
		RailTypeResolverObject ro(nullptr, TileIndex{index}, TCX_NORMAL, RTSG_END);
		return ro.GetScope(VSG_SCOPE_SELF)->GetVariable(var, param, avail);
	}
};

static const NIFeature _nif_railtype = {
	{},
	{},
	_niv_railtypes,
	std::make_unique<NIHRailType>(),
};


/*** NewGRF airport tiles ***/

#define NICAT(cb_id, bit) NIC(cb_id, AirportTileSpec, callback_mask, bit)
static const NICallback _nic_airporttiles[] = {
	NICAT(CBID_AIRPTILE_DRAW_FOUNDATIONS, AirportTileCallbackMask::DrawFoundations),
	NICAT(CBID_AIRPTILE_ANIMATION_TRIGGER, std::monostate{}),
	NICAT(CBID_AIRPTILE_ANIMATION_NEXT_FRAME, AirportTileCallbackMask::AnimationNextFrame),
	NICAT(CBID_AIRPTILE_ANIMATION_SPEED,  AirportTileCallbackMask::AnimationSpeed),
};

class NIHAirportTile : public NIHelper {
	bool IsInspectable(uint index) const override        { return AirportTileSpec::Get(GetAirportGfx(index))->grf_prop.HasGrfFile(); }
	uint GetParent(uint index) const override            { return GetInspectWindowNumber(GSF_AIRPORTS, GetStationIndex(index)); }
	const void *GetInstance(uint)const override          { return nullptr; }
	const void *GetSpec(uint index) const override       { return AirportTileSpec::Get(GetAirportGfx(index)); }
	std::string GetName(uint index) const override       { return GetString(STR_NEWGRF_INSPECT_CAPTION_OBJECT_AT, STR_STATION_NAME, GetStationIndex(index), index); }
	uint32_t GetGRFID(uint index) const override           { return (this->IsInspectable(index)) ? AirportTileSpec::Get(GetAirportGfx(index))->grf_prop.grfid : 0; }
	std::span<const BadgeID> GetBadges(uint index) const override { return AirportTileSpec::Get(GetAirportGfx(index))->badges; }

	uint Resolve(uint index, uint var, uint param, bool &avail) const override
	{
		TileIndex tile{index};
		AirportTileResolverObject ro(AirportTileSpec::GetByTile(tile), tile, Station::GetByTile(tile));
		return ro.GetScope(VSG_SCOPE_SELF)->GetVariable(var, param, avail);
	}
};

static const NIFeature _nif_airporttile = {
	{},
	_nic_airporttiles,
	_niv_industrytiles, // Yes, they share this (at least now)
	std::make_unique<NIHAirportTile>(),
};


/*** NewGRF airports ***/

static const NIVariable _niv_airports[] = {
	NIV(0x40, "Layout number"),
	NIV(0x48, "bitmask of accepted cargoes"),
	NIV(0x60, "amount of cargo waiting"),
	NIV(0x61, "time since last cargo pickup"),
	NIV(0x62, "rating of cargo"),
	NIV(0x63, "time spent on route"),
	NIV(0x64, "information about last vehicle picking cargo up"),
	NIV(0x65, "amount of cargo acceptance"),
	NIV(0x69, "information about cargo accepted in the past"),
	NIV(0xF1, "type of the airport"),
	NIV(0xF6, "airport block status"),
	NIV(0xFA, "built date"),
};

class NIHAirport : public NIHelper {
	bool IsInspectable(uint index) const override        { return AirportSpec::Get(Station::Get(index)->airport.type)->grf_prop.HasGrfFile(); }
	uint GetParent(uint index) const override            { return GetInspectWindowNumber(GSF_FAKE_TOWNS, Station::Get(index)->town->index); }
	const void *GetInstance(uint index)const override    { return Station::Get(index); }
	const void *GetSpec(uint index) const override       { return AirportSpec::Get(Station::Get(index)->airport.type); }
	std::string GetName(uint index) const override       { return GetString(STR_NEWGRF_INSPECT_CAPTION_OBJECT_AT, STR_STATION_NAME, index, Station::Get(index)->airport.tile); }
	uint32_t GetGRFID(uint index) const override         { return (this->IsInspectable(index)) ? AirportSpec::Get(Station::Get(index)->airport.type)->grf_prop.grfid : 0; }
	std::span<const BadgeID> GetBadges(uint index) const override { return AirportSpec::Get(Station::Get(index)->airport.type)->badges; }

	uint Resolve(uint index, uint var, uint param, bool &avail) const override
	{
		Station *st = Station::Get(index);
		AirportResolverObject ro(st->airport.tile, st, AirportSpec::Get(st->airport.type), st->airport.layout);
		return ro.GetScope(VSG_SCOPE_SELF)->GetVariable(var, param, avail);
	}

	const std::span<int32_t> GetPSA(uint index, uint32_t) const override
	{
		const Station *st = (const Station *)this->GetInstance(index);
		if (st->airport.psa == nullptr) return {};
		return st->airport.psa->storage;
	}
};

static const NIFeature _nif_airport = {
	{},
	{},
	_niv_airports,
	std::make_unique<NIHAirport>(),
};


/*** NewGRF towns ***/

static const NIVariable _niv_towns[] = {
	NIV(0x40, "larger town effect on this town"),
	NIV(0x41, "town index"),
	NIV(0x82, "population"),
	NIV(0x94, "zone radius 0"),
	NIV(0x96, "zone radius 1"),
	NIV(0x98, "zone radius 2"),
	NIV(0x9A, "zone radius 3"),
	NIV(0x9C, "zone radius 4"),
	NIV(0xB6, "number of buildings"),
};

class NIHTown : public NIHelper {
	bool IsInspectable(uint index) const override        { return Town::IsValidID(index); }
	uint GetParent(uint) const override                  { return UINT32_MAX; }
	const void *GetInstance(uint index)const override    { return Town::Get(index); }
	const void *GetSpec(uint) const override             { return nullptr; }
	std::string GetName(uint index) const override       { return GetString(STR_TOWN_NAME, index); }
	uint32_t GetGRFID(uint) const override               { return 0; }
	bool PSAWithParameter() const override               { return true; }
	std::span<const BadgeID> GetBadges(uint) const override { return {}; }

	uint Resolve(uint index, uint var, uint param, bool &avail) const override
	{
		TownResolverObject ro(nullptr, Town::Get(index), true);
		return ro.GetScope(VSG_SCOPE_SELF)->GetVariable(var, param, avail);
	}

	const std::span<int32_t> GetPSA(uint index, uint32_t grfid) const override
	{
		Town *t = Town::Get(index);

		for (const auto &it : t->psa_list) {
			if (it->grfid == grfid) return it->storage;
		}

		return {};
	}
};

static const NIFeature _nif_town = {
	{},
	{},
	_niv_towns,
	std::make_unique<NIHTown>(),
};

/*** NewGRF road types ***/

static const NIVariable _niv_roadtypes[] = {
	NIV(0x40, "terrain type"),
	NIV(0x41, "enhanced tunnels"),
	NIV(0x42, "level crossing status"),
	NIV(0x43, "construction date"),
	NIV(0x44, "town zone"),
	NIV(0x45, "track types"),
};

template <RoadTramType TRoadTramType>
class NIHRoadType : public NIHelper {
	bool IsInspectable(uint) const override              { return true; }
	uint GetParent(uint) const override                  { return UINT32_MAX; }
	const void *GetInstance(uint) const override         { return nullptr; }
	const void *GetSpec(uint) const override             { return nullptr; }
	std::string GetName(uint index) const override       { return GetString(STR_NEWGRF_INSPECT_CAPTION_OBJECT_AT, STR_NEWGRF_INSPECT_CAPTION_OBJECT_AT_ROAD_TYPE, INVALID_STRING_ID, index); }
	uint32_t GetGRFID(uint) const override               { return 0; }
	std::span<const BadgeID> GetBadges(uint index) const override
	{
		RoadType rt = GetRoadType(TileIndex{index}, TRoadTramType);
		if (rt == INVALID_ROADTYPE) return {};
		return GetRoadTypeInfo(rt)->badges;
	}

	uint Resolve(uint index, uint var, uint param, bool &avail) const override
	{
		/* There is no unique GRFFile for the tile. Multiple GRFs can define different parts of the railtype.
		 * However, currently the NewGRF Debug GUI does not display variables depending on the GRF (like 0x7F) anyway. */
		RoadTypeResolverObject ro(nullptr, TileIndex{index}, TCX_NORMAL, ROTSG_END);
		return ro.GetScope(VSG_SCOPE_SELF)->GetVariable(var, param, avail);
	}
};

static const NIFeature _nif_roadtype = {
	{},
	{},
	_niv_roadtypes,
	std::make_unique<NIHRoadType<RoadTramType::RTT_ROAD>>(),
};

static const NIFeature _nif_tramtype = {
	{},
	{},
	_niv_roadtypes,
	std::make_unique<NIHRoadType<RoadTramType::RTT_TRAM>>(),
};

#define NICRS(cb_id, bit) NIC(cb_id, RoadStopSpec, callback_mask, bit)
static const NICallback _nic_roadstops[] = {
	NICRS(CBID_STATION_AVAILABILITY,     RoadStopCallbackMask::Avail),
	NICRS(CBID_STATION_ANIMATION_TRIGGER, std::monostate{}),
	NICRS(CBID_STATION_ANIMATION_NEXT_FRAME, RoadStopCallbackMask::AnimationNextFrame),
	NICRS(CBID_STATION_ANIMATION_SPEED,  RoadStopCallbackMask::AnimationSpeed),
};

static const NIVariable _nif_roadstops[] = {
	NIV(0x40, "view/rotation"),
	NIV(0x41, "stop type"),
	NIV(0x42, "terrain type"),
	NIV(0x43, "road type"),
	NIV(0x44, "tram type"),
	NIV(0x45, "town zone and Manhattan distance of town"),
	NIV(0x46, "square of Euclidean distance of town"),
	NIV(0x47, "player info"),
	NIV(0x48, "bitmask of accepted cargoes"),
	NIV(0x49, "current animation frame"),
	NIV(0x60, "amount of cargo waiting"),
	NIV(0x61, "time since last cargo pickup"),
	NIV(0x62, "rating of cargo"),
	NIV(0x63, "time spent on route"),
	NIV(0x64, "information about last vehicle picking cargo up"),
	NIV(0x65, "amount of cargo acceptance"),
	NIV(0x66, "animation frame of nearby tile"),
	NIV(0x67, "land info of nearby tiles"),
	NIV(0x68, "road stop info of nearby tiles"),
	NIV(0x69, "information about cargo accepted in the past"),
	NIV(0x6A, "GRFID of nearby road stop tiles"),
	NIV(0x6B, "road stop ID of nearby tiles"),
};

class NIHRoadStop : public NIHelper {
	bool IsInspectable(uint index) const override        { return GetRoadStopSpec(TileIndex{index}) != nullptr; }
	uint GetParent(uint index) const override            { return GetInspectWindowNumber(GSF_FAKE_TOWNS, BaseStation::GetByTile(TileIndex{index})->town->index); }
	const void *GetInstance(uint)const override          { return nullptr; }
	const void *GetSpec(uint index) const override       { return GetRoadStopSpec(TileIndex{index}); }
	std::string GetName(uint index) const override       { return GetString(STR_NEWGRF_INSPECT_CAPTION_OBJECT_AT, STR_STATION_NAME, GetStationIndex(index), index); }
	uint32_t GetGRFID(uint index) const override           { return (this->IsInspectable(index)) ? GetRoadStopSpec(TileIndex{index})->grf_prop.grfid : 0; }
	std::span<const BadgeID> GetBadges(uint index) const override { return this->IsInspectable(index) ? GetRoadStopSpec(TileIndex{index})->badges : std::span<const BadgeID>{}; }

	uint Resolve(uint index, uint var, uint32_t param, bool &avail) const override
	{
		TileIndex tile{index};
		StationGfx view = GetStationGfx(tile);
		RoadStopResolverObject ro(GetRoadStopSpec(tile), BaseStation::GetByTile(tile), tile, INVALID_ROADTYPE, GetStationType(tile), view);
		return ro.GetScope(VSG_SCOPE_SELF)->GetVariable(var, param, avail);
	}
};

static const NIFeature _nif_roadstop = {
	{},
	_nic_roadstops,
	_nif_roadstops,
	std::make_unique<NIHRoadStop>(),
};

/** Table with all NIFeatures. */
static const NIFeature * const _nifeatures[] = {
	&_nif_vehicle,      // GSF_TRAINS
	&_nif_vehicle,      // GSF_ROADVEHICLES
	&_nif_vehicle,      // GSF_SHIPS
	&_nif_vehicle,      // GSF_AIRCRAFT
	&_nif_station,      // GSF_STATIONS
	nullptr,               // GSF_CANALS (no callbacks/action2 implemented)
	nullptr,               // GSF_BRIDGES (no callbacks/action2)
	&_nif_house,        // GSF_HOUSES
	nullptr,               // GSF_GLOBALVAR (has no "physical" objects)
	&_nif_industrytile, // GSF_INDUSTRYTILES
	&_nif_industry,     // GSF_INDUSTRIES
	nullptr,               // GSF_CARGOES (has no "physical" objects)
	nullptr,               // GSF_SOUNDFX (has no "physical" objects)
	&_nif_airport,      // GSF_AIRPORTS
	nullptr,               // GSF_SIGNALS (feature not implemented)
	&_nif_object,       // GSF_OBJECTS
	&_nif_railtype,     // GSF_RAILTYPES
	&_nif_airporttile,  // GSF_AIRPORTTILES
	&_nif_roadtype,     // GSF_ROADTYPES
	&_nif_tramtype,     // GSF_TRAMTYPES
	&_nif_roadstop,     // GSF_ROADSTOPS
	&_nif_town,         // GSF_FAKE_TOWNS
	nullptr,
};
static_assert(lengthof(_nifeatures) == GSF_FAKE_END);
