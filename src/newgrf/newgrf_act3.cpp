/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act3.cpp NewGRF Action 0x03 handler. */

#include "../stdafx.h"

#include "../debug.h"
#include "../house.h"
#include "../newgrf_engine.h"
#include "../newgrf_badge.h"
#include "../newgrf_badge_type.h"
#include "../newgrf_cargo.h"
#include "../newgrf_house.h"
#include "../newgrf_station.h"
#include "../industrytype.h"
#include "../newgrf_canal.h"
#include "../newgrf_airporttiles.h"
#include "../newgrf_airport.h"
#include "../newgrf_object.h"
#include "../error.h"
#include "../vehicle_base.h"
#include "../road.h"
#include "../newgrf_roadstop.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal_vehicle.h"
#include "newgrf_internal.h"

#include "../safeguards.h"


static CargoType TranslateCargo(GrfSpecFeature feature, uint8_t ctype)
{
	/* Special cargo types for purchase list and stations */
	if ((feature == GSF_STATIONS || feature == GSF_ROADSTOPS) && ctype == 0xFE) return CargoGRFFileProps::SG_DEFAULT_NA;
	if (ctype == 0xFF) return CargoGRFFileProps::SG_PURCHASE;

	auto cargo_list = GetCargoTranslationTable(*_cur_gps.grffile);

	/* Check if the cargo type is out of bounds of the cargo translation table */
	if (ctype >= cargo_list.size()) {
		GrfMsg(1, "TranslateCargo: Cargo type {} out of range (max {}), skipping.", ctype, (unsigned int)_cur_gps.grffile->cargo_list.size() - 1);
		return INVALID_CARGO;
	}

	/* Look up the cargo label from the translation table */
	CargoLabel cl = cargo_list[ctype];
	if (cl == CT_INVALID) {
		GrfMsg(5, "TranslateCargo: Cargo type {} not available in this climate, skipping.", ctype);
		return INVALID_CARGO;
	}

	CargoType cargo_type = GetCargoTypeByLabel(cl);
	if (!IsValidCargoType(cargo_type)) {
		GrfMsg(5, "TranslateCargo: Cargo '{:c}{:c}{:c}{:c}' unsupported, skipping.", GB(cl.base(), 24, 8), GB(cl.base(), 16, 8), GB(cl.base(), 8, 8), GB(cl.base(), 0, 8));
		return INVALID_CARGO;
	}

	GrfMsg(6, "TranslateCargo: Cargo '{:c}{:c}{:c}{:c}' mapped to cargo type {}.", GB(cl.base(), 24, 8), GB(cl.base(), 16, 8), GB(cl.base(), 8, 8), GB(cl.base(), 0, 8), cargo_type);
	return cargo_type;
}


static bool IsValidGroupID(uint16_t groupid, std::string_view function)
{
	if (groupid > MAX_SPRITEGROUP || _cur_gps.spritegroups[groupid] == nullptr) {
		GrfMsg(1, "{}: Spritegroup 0x{:04X} out of range or empty, skipping.", function, groupid);
		return false;
	}

	return true;
}

static void VehicleMapSpriteGroup(ByteReader &buf, GrfSpecFeature feature, uint8_t idcount)
{
	static std::vector<EngineID> last_engines; // Engine IDs are remembered in case the next action is a wagon override.
	bool wagover = false;

	/* Test for 'wagon override' flag */
	if (HasBit(idcount, 7)) {
		wagover = true;
		/* Strip off the flag */
		idcount = GB(idcount, 0, 7);

		if (last_engines.empty()) {
			GrfMsg(0, "VehicleMapSpriteGroup: WagonOverride: No engine to do override with");
			return;
		}

		GrfMsg(6, "VehicleMapSpriteGroup: WagonOverride: {} engines, {} wagons", last_engines.size(), idcount);
	} else {
		last_engines.resize(idcount);
	}

	std::vector<EngineID> engines;
	engines.reserve(idcount);
	for (uint i = 0; i < idcount; i++) {
		Engine *e = GetNewEngine(_cur_gps.grffile, (VehicleType)feature, buf.ReadExtendedByte());
		if (e == nullptr) {
			/* No engine could be allocated?!? Deal with it. Okay,
			 * this might look bad. Also make sure this NewGRF
			 * gets disabled, as a half loaded one is bad. */
			HandleChangeInfoResult("VehicleMapSpriteGroup", CIR_INVALID_ID, feature, 0);
			return;
		}

		engines.push_back(e->index);
		if (!wagover) last_engines[i] = engines[i];
	}

	uint8_t cidcount = buf.ReadByte();
	for (uint c = 0; c < cidcount; c++) {
		uint8_t ctype = buf.ReadByte();
		uint16_t groupid = buf.ReadWord();
		if (!IsValidGroupID(groupid, "VehicleMapSpriteGroup")) continue;

		GrfMsg(8, "VehicleMapSpriteGroup: * [{}] Cargo type 0x{:X}, group id 0x{:02X}", c, ctype, groupid);

		CargoType cargo_type = TranslateCargo(feature, ctype);
		if (!IsValidCargoType(cargo_type)) continue;

		for (uint i = 0; i < idcount; i++) {
			EngineID engine = engines[i];

			GrfMsg(7, "VehicleMapSpriteGroup: [{}] Engine {}...", i, engine);

			if (wagover) {
				SetWagonOverrideSprites(engine, cargo_type, _cur_gps.spritegroups[groupid], last_engines);
			} else {
				SetCustomEngineSprites(engine, cargo_type, _cur_gps.spritegroups[groupid]);
			}
		}
	}

	uint16_t groupid = buf.ReadWord();
	if (!IsValidGroupID(groupid, "VehicleMapSpriteGroup")) return;

	GrfMsg(8, "-- Default group id 0x{:04X}", groupid);

	for (uint i = 0; i < idcount; i++) {
		EngineID engine = engines[i];

		if (wagover) {
			SetWagonOverrideSprites(engine, CargoGRFFileProps::SG_DEFAULT, _cur_gps.spritegroups[groupid], last_engines);
		} else {
			SetCustomEngineSprites(engine, CargoGRFFileProps::SG_DEFAULT, _cur_gps.spritegroups[groupid]);
			SetEngineGRF(engine, _cur_gps.grffile);
		}
	}
}

/** Handler interface for mapping sprite groups. */
struct MapSpriteGroupHandler {
	virtual ~MapSpriteGroupHandler() {}
	virtual void MapSpecific(uint16_t local_id, uint8_t cid, const SpriteGroup *group) = 0;
	virtual void MapDefault(uint16_t local_id, const SpriteGroup *group) = 0;
};

/** Specializable function to retrieve a NewGRF spec of a particular type. */
template <typename T> static auto *GetSpec(GRFFile *, uint16_t);

/** Common handler for mapping sprite groups for features which only support "Purchase" and "Default" sprites. */
template <typename T>
struct PurchaseDefaultMapSpriteGroupHandler : MapSpriteGroupHandler {
	void MapSpecific(uint16_t local_id, uint8_t cid, const SpriteGroup *group) override
	{
		if (cid != 0xFF) {
			GrfMsg(1, "MapSpriteGroup: Invalid cargo bitnum {}, skipping.", cid);
		} else if (T *spec = GetSpec<T>(_cur_gps.grffile, local_id); spec == nullptr) {
			GrfMsg(1, "MapSpriteGroup: {} undefined, skipping.", local_id);
		} else {
			spec->grf_prop.SetSpriteGroup(StandardSpriteGroup::Purchase, group);
		}
	}

	void MapDefault(uint16_t local_id, const SpriteGroup *group) override
	{
		if (T *spec = GetSpec<T>(_cur_gps.grffile, local_id); spec == nullptr) {
			GrfMsg(1, "MapSpriteGroup: {} undefined, skipping.", local_id);
		} else {
			spec->grf_prop.SetSpriteGroup(StandardSpriteGroup::Default, group);
			spec->grf_prop.SetGRFFile(_cur_gps.grffile);
			spec->grf_prop.local_id = local_id;
		}
	}
};

/** Common handler for mapping sprite groups for features which support cargo-type specific sprites. */
template <typename T, typename Tclass>
struct CargoTypeMapSpriteGroupHandler : MapSpriteGroupHandler {
	void MapSpecific(uint16_t local_id, uint8_t cid, const SpriteGroup *group) override
	{
		CargoType cargo_type = TranslateCargo(GSF_STATIONS, cid);
		if (!IsValidCargoType(cargo_type)) return;

		if (T *spec = GetSpec<T>(_cur_gps.grffile, local_id); spec == nullptr) {
			GrfMsg(1, "MapSpriteGroup: {} undefined, skipping", local_id);
		} else {
			spec->grf_prop.SetSpriteGroup(cargo_type, group);
		}
	}

	void MapDefault(uint16_t local_id, const SpriteGroup *group) override
	{
		if (T *spec = GetSpec<T>(_cur_gps.grffile, local_id); spec == nullptr) {
			GrfMsg(1, "MapSpriteGroup: {} undefined, skipping", local_id);
		} else if (spec->grf_prop.HasGrfFile()) {
			GrfMsg(1, "MapSpriteGroup: {} mapped multiple times, skipping", local_id);
		} else {
			spec->grf_prop.SetSpriteGroup(CargoGRFFileProps::SG_DEFAULT, group);
			spec->grf_prop.SetGRFFile(_cur_gps.grffile);
			spec->grf_prop.local_id = local_id;
			Tclass::Assign(spec);
		}
	}
};

struct CanalMapSpriteGroupHandler : MapSpriteGroupHandler {
	void MapSpecific(uint16_t, uint8_t, const SpriteGroup *) override {}

	void MapDefault(uint16_t local_id, const SpriteGroup *group) override
	{
		if (local_id >= CF_END) {
			GrfMsg(1, "CanalMapSpriteGroup: Canal subset {} out of range, skipping", local_id);
		} else {
			_water_feature[local_id].grffile = _cur_gps.grffile;
			_water_feature[local_id].group = group;
		}
	}
};

template <> auto *GetSpec<StationSpec>(GRFFile *grffile, uint16_t local_id) { return local_id < grffile->stations.size() ? grffile->stations[local_id].get() : nullptr; }
struct StationMapSpriteGroupHandler : CargoTypeMapSpriteGroupHandler<StationSpec, StationClass> {};

template <> auto *GetSpec<HouseSpec>(GRFFile *grffile, uint16_t local_id) { return local_id < grffile->housespec.size() ? grffile->housespec[local_id].get() : nullptr; }
struct TownHouseMapSpriteGroupHandler : PurchaseDefaultMapSpriteGroupHandler<HouseSpec> {};

template <> auto *GetSpec<IndustrySpec>(GRFFile *grffile, uint16_t local_id) { return local_id < grffile->industryspec.size() ? grffile->industryspec[local_id].get() : nullptr; }
struct IndustryMapSpriteGroupHandler : PurchaseDefaultMapSpriteGroupHandler<IndustrySpec> {};

template <> auto *GetSpec<IndustryTileSpec>(GRFFile *grffile, uint16_t local_id) { return local_id < grffile->indtspec.size() ? grffile->indtspec[local_id].get() : nullptr; }
struct IndustryTileMapSpriteGroupHandler : PurchaseDefaultMapSpriteGroupHandler<IndustryTileSpec> {};

struct CargoMapSpriteGroupHandler : MapSpriteGroupHandler {
	void MapSpecific(uint16_t, uint8_t, const SpriteGroup *) override {}

	void MapDefault(uint16_t local_id, const SpriteGroup *group) override
	{
		if (local_id >= NUM_CARGO) {
			GrfMsg(1, "CargoMapSpriteGroup: Cargo type {} out of range, skipping", local_id);
		} else {
			CargoSpec *cs = CargoSpec::Get(local_id);
			cs->grffile = _cur_gps.grffile;
			cs->group = group;
		}
	}
};

template <> auto *GetSpec<ObjectSpec>(GRFFile *grffile, uint16_t local_id) { return local_id < grffile->objectspec.size() ? grffile->objectspec[local_id].get() : nullptr; }
struct ObjectMapSpriteGroupHandler : PurchaseDefaultMapSpriteGroupHandler<ObjectSpec> {};

struct RailTypeMapSpriteGroupHandler : MapSpriteGroupHandler {
	void MapSpecific(uint16_t local_id, uint8_t cid, const SpriteGroup *group) override
	{
		if (cid >= RTSG_END) return;

		const auto &type_map = _cur_gps.grffile->railtype_map;
		RailType railtype = local_id < std::size(type_map) ? type_map[local_id] : INVALID_RAILTYPE;
		if (railtype == INVALID_RAILTYPE) return;

		extern RailTypeInfo _railtypes[RAILTYPE_END];
		RailTypeInfo &rti = _railtypes[railtype];
		rti.grffile[cid] = _cur_gps.grffile;
		rti.group[cid] = group;
	}

	void MapDefault(uint16_t, const SpriteGroup *) override {}
};

template <RoadTramType TRoadTramType>
struct RoadTypeMapSpriteGroupHandler : MapSpriteGroupHandler {
	void MapSpecific(uint16_t local_id, uint8_t cid, const SpriteGroup *group) override
	{
		if (cid >= ROTSG_END) return;

		const auto &type_map = (TRoadTramType == RTT_TRAM) ? _cur_gps.grffile->tramtype_map : _cur_gps.grffile->roadtype_map;
		RoadType roadtype = local_id < std::size(type_map) ? type_map[local_id] : INVALID_ROADTYPE;
		if (roadtype == INVALID_ROADTYPE) return;

		extern RoadTypeInfo _roadtypes[ROADTYPE_END];
		RoadTypeInfo &rti = _roadtypes[roadtype];
		rti.grffile[cid] = _cur_gps.grffile;
		rti.group[cid] = group;
	}

	void MapDefault(uint16_t, const SpriteGroup *) override {}
};

template <> auto *GetSpec<AirportSpec>(GRFFile *grffile, uint16_t local_id) { return local_id < grffile->airportspec.size() ? grffile->airportspec[local_id].get() : nullptr; }
struct AirportMapSpriteGroupHandler : PurchaseDefaultMapSpriteGroupHandler<AirportSpec> {};

template <> auto *GetSpec<AirportTileSpec>(GRFFile *grffile, uint16_t local_id) { return local_id < grffile->airtspec.size() ? grffile->airtspec[local_id].get() : nullptr; }
struct AirportTileMapSpriteGroupHandler : PurchaseDefaultMapSpriteGroupHandler<AirportTileSpec> {};

template <> auto *GetSpec<RoadStopSpec>(GRFFile *grffile, uint16_t local_id) { return local_id < grffile->roadstops.size() ? grffile->roadstops[local_id].get() : nullptr; }
struct RoadStopMapSpriteGroupHandler : CargoTypeMapSpriteGroupHandler<RoadStopSpec, RoadStopClass> {};

struct BadgeMapSpriteGroupHandler : MapSpriteGroupHandler {
	void MapSpecific(uint16_t local_id, uint8_t cid, const SpriteGroup *group) override
	{
		if (cid >= GSF_END) return;

		auto found = _cur_gps.grffile->badge_map.find(local_id);
		if (found == std::end(_cur_gps.grffile->badge_map)) {
			GrfMsg(1, "BadgeMapSpriteGroup: Badge {} undefined, skipping", local_id);
		} else {
			auto &badge = *GetBadge(found->second);
			badge.grf_prop.SetSpriteGroup(static_cast<GrfSpecFeature>(cid), group);
		}
	}

	void MapDefault(uint16_t local_id, const SpriteGroup *group) override
	{
		auto found = _cur_gps.grffile->badge_map.find(local_id);
		if (found == std::end(_cur_gps.grffile->badge_map)) {
			GrfMsg(1, "BadgeMapSpriteGroup: Badge {} undefined, skipping", local_id);
		} else {
			auto &badge = *GetBadge(found->second);
			badge.grf_prop.SetSpriteGroup(GSF_DEFAULT, group);
			badge.grf_prop.SetGRFFile(_cur_gps.grffile);
			badge.grf_prop.local_id = local_id;
		}
	}
};

static void MapSpriteGroup(ByteReader &buf, uint8_t idcount, MapSpriteGroupHandler &&handler)
{
	/* Read IDs to map into memory. */
	std::array<uint16_t, 256> local_ids_buffer;
	for (uint i = 0; i != idcount; ++i) {
		local_ids_buffer[i] = buf.ReadExtendedByte();
	}
	std::span<const uint16_t> local_ids{local_ids_buffer.begin(), idcount};

	/* Handle specific mappings. */
	uint8_t cidcount = buf.ReadByte();
	for (uint c = 0; c != cidcount; ++c) {
		uint8_t cid = buf.ReadByte();
		uint16_t groupid = buf.ReadWord();
		if (!IsValidGroupID(groupid, "MapSpriteGroup")) continue;
		for (uint16_t local_id : local_ids) {
			handler.MapSpecific(local_id, cid, _cur_gps.spritegroups[groupid]);
		}
	}

	/* Handle default mapping. */
	uint16_t groupid = buf.ReadWord();
	if (!IsValidGroupID(groupid, "MapSpriteGroup")) return;
	for (uint16_t local_id : local_ids) {
		handler.MapDefault(local_id, _cur_gps.spritegroups[groupid]);
	}
}

/* Action 0x03 */
static void FeatureMapSpriteGroup(ByteReader &buf)
{
	/* <03> <feature> <n-id> <ids>... <num-cid> [<cargo-type> <cid>]... <def-cid>
	 * id-list    := [<id>] [id-list]
	 * cargo-list := <cargo-type> <cid> [cargo-list]
	 *
	 * B feature       see action 0
	 * B n-id          bits 0-6: how many IDs this definition applies to
	 *                 bit 7: if set, this is a wagon override definition (see below)
	 * E ids           the IDs for which this definition applies
	 * B num-cid       number of cargo IDs (sprite group IDs) in this definition
	 *                 can be zero, in that case the def-cid is used always
	 * B cargo-type    type of this cargo type (e.g. mail=2, wood=7, see below)
	 * W cid           cargo ID (sprite group ID) for this type of cargo
	 * W def-cid       default cargo ID (sprite group ID) */

	GrfSpecFeature feature{buf.ReadByte()};
	uint8_t idcount = buf.ReadByte();

	if (feature >= GSF_END) {
		GrfMsg(1, "FeatureMapSpriteGroup: Unsupported feature 0x{:02X}, skipping", feature);
		return;
	}

	/* If idcount is zero, this is a feature callback */
	if (idcount == 0) {
		/* Skip number of cargo ids? */
		buf.ReadByte();
		uint16_t groupid = buf.ReadWord();
		if (!IsValidGroupID(groupid, "FeatureMapSpriteGroup")) return;

		GrfMsg(6, "FeatureMapSpriteGroup: Adding generic feature callback for feature 0x{:02X}", feature);

		AddGenericCallback(feature, _cur_gps.grffile, _cur_gps.spritegroups[groupid]);
		return;
	}

	/* Mark the feature as used by the grf (generic callbacks do not count) */
	_cur_gps.grffile->grf_features.Set(feature);

	GrfMsg(6, "FeatureMapSpriteGroup: Feature 0x{:02X}, {} ids", feature, idcount);

	switch (feature) {
		case GSF_TRAINS:
		case GSF_ROADVEHICLES:
		case GSF_SHIPS:
		case GSF_AIRCRAFT: VehicleMapSpriteGroup(buf, feature, idcount); return;
		case GSF_CANALS: MapSpriteGroup(buf, idcount, CanalMapSpriteGroupHandler{}); return;
		case GSF_STATIONS: MapSpriteGroup(buf, idcount, StationMapSpriteGroupHandler{}); return;
		case GSF_HOUSES: MapSpriteGroup(buf, idcount, TownHouseMapSpriteGroupHandler{}); return;
		case GSF_INDUSTRIES: MapSpriteGroup(buf, idcount, IndustryMapSpriteGroupHandler{}); return;
		case GSF_INDUSTRYTILES: MapSpriteGroup(buf, idcount, IndustryTileMapSpriteGroupHandler{}); return;
		case GSF_CARGOES: MapSpriteGroup(buf, idcount, CargoMapSpriteGroupHandler{}); return;
		case GSF_AIRPORTS: MapSpriteGroup(buf, idcount, AirportMapSpriteGroupHandler{}); return;
		case GSF_OBJECTS: MapSpriteGroup(buf, idcount, ObjectMapSpriteGroupHandler{}); return;
		case GSF_RAILTYPES: MapSpriteGroup(buf, idcount, RailTypeMapSpriteGroupHandler{}); return;
		case GSF_ROADTYPES: MapSpriteGroup(buf, idcount, RoadTypeMapSpriteGroupHandler<RTT_ROAD>{}); return;
		case GSF_TRAMTYPES: MapSpriteGroup(buf, idcount, RoadTypeMapSpriteGroupHandler<RTT_TRAM>{}); return;
		case GSF_AIRPORTTILES: MapSpriteGroup(buf, idcount, AirportTileMapSpriteGroupHandler{}); return;
		case GSF_ROADSTOPS: MapSpriteGroup(buf, idcount, RoadStopMapSpriteGroupHandler{}); return;
		case GSF_BADGES: MapSpriteGroup(buf, idcount, BadgeMapSpriteGroupHandler{}); return;

		default:
			GrfMsg(1, "FeatureMapSpriteGroup: Unsupported feature 0x{:02X}, skipping", feature);
			return;
	}
}

template <> void GrfActionHandler<0x03>::FileScan(ByteReader &) { }
template <> void GrfActionHandler<0x03>::SafetyScan(ByteReader &buf) { GRFUnsafe(buf); }
template <> void GrfActionHandler<0x03>::LabelScan(ByteReader &) { }
template <> void GrfActionHandler<0x03>::Init(ByteReader &) { }
template <> void GrfActionHandler<0x03>::Reserve(ByteReader &) { }
template <> void GrfActionHandler<0x03>::Activation(ByteReader &buf) { FeatureMapSpriteGroup(buf); }
