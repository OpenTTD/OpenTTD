/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
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


static CargoType TranslateCargo(uint8_t feature, uint8_t ctype)
{
	/* Special cargo types for purchase list and stations */
	if ((feature == GSF_STATIONS || feature == GSF_ROADSTOPS) && ctype == 0xFE) return SpriteGroupCargo::SG_DEFAULT_NA;
	if (ctype == 0xFF) return SpriteGroupCargo::SG_PURCHASE;

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


static bool IsValidGroupID(uint16_t groupid, const char *function)
{
	if (groupid > MAX_SPRITEGROUP || _cur_gps.spritegroups[groupid] == nullptr) {
		GrfMsg(1, "{}: Spritegroup 0x{:04X} out of range or empty, skipping.", function, groupid);
		return false;
	}

	return true;
}

static void VehicleMapSpriteGroup(ByteReader &buf, uint8_t feature, uint8_t idcount)
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
			HandleChangeInfoResult("VehicleMapSpriteGroup", CIR_INVALID_ID, 0, 0);
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
			SetWagonOverrideSprites(engine, SpriteGroupCargo::SG_DEFAULT, _cur_gps.spritegroups[groupid], last_engines);
		} else {
			SetCustomEngineSprites(engine, SpriteGroupCargo::SG_DEFAULT, _cur_gps.spritegroups[groupid]);
			SetEngineGRF(engine, _cur_gps.grffile);
		}
	}
}


static void CanalMapSpriteGroup(ByteReader &buf, uint8_t idcount)
{
	std::vector<uint16_t> cfs;
	cfs.reserve(idcount);
	for (uint i = 0; i < idcount; i++) {
		cfs.push_back(buf.ReadExtendedByte());
	}

	uint8_t cidcount = buf.ReadByte();
	buf.Skip(cidcount * 3);

	uint16_t groupid = buf.ReadWord();
	if (!IsValidGroupID(groupid, "CanalMapSpriteGroup")) return;

	for (auto &cf : cfs) {
		if (cf >= CF_END) {
			GrfMsg(1, "CanalMapSpriteGroup: Canal subset {} out of range, skipping", cf);
			continue;
		}

		_water_feature[cf].grffile = _cur_gps.grffile;
		_water_feature[cf].group = _cur_gps.spritegroups[groupid];
	}
}


static void StationMapSpriteGroup(ByteReader &buf, uint8_t idcount)
{
	if (_cur_gps.grffile->stations.empty()) {
		GrfMsg(1, "StationMapSpriteGroup: No stations defined, skipping");
		return;
	}

	std::vector<uint16_t> stations;
	stations.reserve(idcount);
	for (uint i = 0; i < idcount; i++) {
		stations.push_back(buf.ReadExtendedByte());
	}

	uint8_t cidcount = buf.ReadByte();
	for (uint c = 0; c < cidcount; c++) {
		uint8_t ctype = buf.ReadByte();
		uint16_t groupid = buf.ReadWord();
		if (!IsValidGroupID(groupid, "StationMapSpriteGroup")) continue;

		ctype = TranslateCargo(GSF_STATIONS, ctype);
		if (!IsValidCargoType(ctype)) continue;

		for (auto &station : stations) {
			StationSpec *statspec = station >= _cur_gps.grffile->stations.size() ? nullptr : _cur_gps.grffile->stations[station].get();

			if (statspec == nullptr) {
				GrfMsg(1, "StationMapSpriteGroup: Station {} undefined, skipping", station);
				continue;
			}

			statspec->grf_prop.SetSpriteGroup(ctype, _cur_gps.spritegroups[groupid]);
		}
	}

	uint16_t groupid = buf.ReadWord();
	if (!IsValidGroupID(groupid, "StationMapSpriteGroup")) return;

	for (auto &station : stations) {
		StationSpec *statspec = station >= _cur_gps.grffile->stations.size() ? nullptr : _cur_gps.grffile->stations[station].get();

		if (statspec == nullptr) {
			GrfMsg(1, "StationMapSpriteGroup: Station {} undefined, skipping", station);
			continue;
		}

		if (statspec->grf_prop.HasGrfFile()) {
			GrfMsg(1, "StationMapSpriteGroup: Station {} mapped multiple times, skipping", station);
			continue;
		}

		statspec->grf_prop.SetSpriteGroup(SpriteGroupCargo::SG_DEFAULT, _cur_gps.spritegroups[groupid]);
		statspec->grf_prop.SetGRFFile(_cur_gps.grffile);
		statspec->grf_prop.local_id = station;
		StationClass::Assign(statspec);
	}
}


static void TownHouseMapSpriteGroup(ByteReader &buf, uint8_t idcount)
{
	if (_cur_gps.grffile->housespec.empty()) {
		GrfMsg(1, "TownHouseMapSpriteGroup: No houses defined, skipping");
		return;
	}

	std::vector<uint16_t> houses;
	houses.reserve(idcount);
	for (uint i = 0; i < idcount; i++) {
		houses.push_back(buf.ReadExtendedByte());
	}

	/* Skip the cargo type section, we only care about the default group */
	uint8_t cidcount = buf.ReadByte();
	buf.Skip(cidcount * 3);

	uint16_t groupid = buf.ReadWord();
	if (!IsValidGroupID(groupid, "TownHouseMapSpriteGroup")) return;

	for (auto &house : houses) {
		HouseSpec *hs = house >= _cur_gps.grffile->housespec.size() ? nullptr : _cur_gps.grffile->housespec[house].get();

		if (hs == nullptr) {
			GrfMsg(1, "TownHouseMapSpriteGroup: House {} undefined, skipping.", house);
			continue;
		}

		hs->grf_prop.SetSpriteGroup(0, _cur_gps.spritegroups[groupid]);
	}
}

static void IndustryMapSpriteGroup(ByteReader &buf, uint8_t idcount)
{
	if (_cur_gps.grffile->industryspec.empty()) {
		GrfMsg(1, "IndustryMapSpriteGroup: No industries defined, skipping");
		return;
	}

	std::vector<uint16_t> industries;
	industries.reserve(idcount);
	for (uint i = 0; i < idcount; i++) {
		industries.push_back(buf.ReadExtendedByte());
	}

	/* Skip the cargo type section, we only care about the default group */
	uint8_t cidcount = buf.ReadByte();
	buf.Skip(cidcount * 3);

	uint16_t groupid = buf.ReadWord();
	if (!IsValidGroupID(groupid, "IndustryMapSpriteGroup")) return;

	for (auto &industry : industries) {
		IndustrySpec *indsp = industry >= _cur_gps.grffile->industryspec.size() ? nullptr : _cur_gps.grffile->industryspec[industry].get();

		if (indsp == nullptr) {
			GrfMsg(1, "IndustryMapSpriteGroup: Industry {} undefined, skipping", industry);
			continue;
		}

		indsp->grf_prop.SetSpriteGroup(0, _cur_gps.spritegroups[groupid]);
	}
}

static void IndustrytileMapSpriteGroup(ByteReader &buf, uint8_t idcount)
{
	if (_cur_gps.grffile->indtspec.empty()) {
		GrfMsg(1, "IndustrytileMapSpriteGroup: No industry tiles defined, skipping");
		return;
	}

	std::vector<uint16_t> indtiles;
	indtiles.reserve(idcount);
	for (uint i = 0; i < idcount; i++) {
		indtiles.push_back(buf.ReadExtendedByte());
	}

	/* Skip the cargo type section, we only care about the default group */
	uint8_t cidcount = buf.ReadByte();
	buf.Skip(cidcount * 3);

	uint16_t groupid = buf.ReadWord();
	if (!IsValidGroupID(groupid, "IndustrytileMapSpriteGroup")) return;

	for (auto &indtile : indtiles) {
		IndustryTileSpec *indtsp = indtile >= _cur_gps.grffile->indtspec.size() ? nullptr : _cur_gps.grffile->indtspec[indtile].get();

		if (indtsp == nullptr) {
			GrfMsg(1, "IndustrytileMapSpriteGroup: Industry tile {} undefined, skipping", indtile);
			continue;
		}

		indtsp->grf_prop.SetSpriteGroup(0, _cur_gps.spritegroups[groupid]);
	}
}

static void CargoMapSpriteGroup(ByteReader &buf, uint8_t idcount)
{
	std::vector<uint16_t> cargoes;
	cargoes.reserve(idcount);
	for (uint i = 0; i < idcount; i++) {
		cargoes.push_back(buf.ReadExtendedByte());
	}

	/* Skip the cargo type section, we only care about the default group */
	uint8_t cidcount = buf.ReadByte();
	buf.Skip(cidcount * 3);

	uint16_t groupid = buf.ReadWord();
	if (!IsValidGroupID(groupid, "CargoMapSpriteGroup")) return;

	for (auto &cargo_type : cargoes) {
		if (cargo_type >= NUM_CARGO) {
			GrfMsg(1, "CargoMapSpriteGroup: Cargo type {} out of range, skipping", cargo_type);
			continue;
		}

		CargoSpec *cs = CargoSpec::Get(cargo_type);
		cs->grffile = _cur_gps.grffile;
		cs->group = _cur_gps.spritegroups[groupid];
	}
}

static void ObjectMapSpriteGroup(ByteReader &buf, uint8_t idcount)
{
	if (_cur_gps.grffile->objectspec.empty()) {
		GrfMsg(1, "ObjectMapSpriteGroup: No object tiles defined, skipping");
		return;
	}

	std::vector<uint16_t> objects;
	objects.reserve(idcount);
	for (uint i = 0; i < idcount; i++) {
		objects.push_back(buf.ReadExtendedByte());
	}

	uint8_t cidcount = buf.ReadByte();
	for (uint c = 0; c < cidcount; c++) {
		uint8_t ctype = buf.ReadByte();
		uint16_t groupid = buf.ReadWord();
		if (!IsValidGroupID(groupid, "ObjectMapSpriteGroup")) continue;

		/* The only valid option here is purchase list sprite groups. */
		if (ctype != 0xFF) {
			GrfMsg(1, "ObjectMapSpriteGroup: Invalid cargo bitnum {} for objects, skipping.", ctype);
			continue;
		}

		for (auto &object : objects) {
			ObjectSpec *spec = object >= _cur_gps.grffile->objectspec.size() ? nullptr : _cur_gps.grffile->objectspec[object].get();

			if (spec == nullptr) {
				GrfMsg(1, "ObjectMapSpriteGroup: Object {} undefined, skipping", object);
				continue;
			}

			spec->grf_prop.SetSpriteGroup(OBJECT_SPRITE_GROUP_PURCHASE, _cur_gps.spritegroups[groupid]);
		}
	}

	uint16_t groupid = buf.ReadWord();
	if (!IsValidGroupID(groupid, "ObjectMapSpriteGroup")) return;

	for (auto &object : objects) {
		ObjectSpec *spec = object >= _cur_gps.grffile->objectspec.size() ? nullptr : _cur_gps.grffile->objectspec[object].get();

		if (spec == nullptr) {
			GrfMsg(1, "ObjectMapSpriteGroup: Object {} undefined, skipping", object);
			continue;
		}

		if (spec->grf_prop.HasGrfFile()) {
			GrfMsg(1, "ObjectMapSpriteGroup: Object {} mapped multiple times, skipping", object);
			continue;
		}

		spec->grf_prop.SetSpriteGroup(OBJECT_SPRITE_GROUP_DEFAULT, _cur_gps.spritegroups[groupid]);
		spec->grf_prop.SetGRFFile(_cur_gps.grffile);
		spec->grf_prop.local_id = object;
	}
}

static void RailTypeMapSpriteGroup(ByteReader &buf, uint8_t idcount)
{
	std::vector<uint8_t> railtypes;
	railtypes.reserve(idcount);
	for (uint i = 0; i < idcount; i++) {
		uint16_t id = buf.ReadExtendedByte();
		railtypes.push_back(id < RAILTYPE_END ? _cur_gps.grffile->railtype_map[id] : INVALID_RAILTYPE);
	}

	uint8_t cidcount = buf.ReadByte();
	for (uint c = 0; c < cidcount; c++) {
		uint8_t ctype = buf.ReadByte();
		uint16_t groupid = buf.ReadWord();
		if (!IsValidGroupID(groupid, "RailTypeMapSpriteGroup")) continue;

		if (ctype >= RTSG_END) continue;

		extern RailTypeInfo _railtypes[RAILTYPE_END];
		for (auto &railtype : railtypes) {
			if (railtype != INVALID_RAILTYPE) {
				RailTypeInfo *rti = &_railtypes[railtype];

				rti->grffile[ctype] = _cur_gps.grffile;
				rti->group[ctype] = _cur_gps.spritegroups[groupid];
			}
		}
	}

	/* Railtypes do not use the default group. */
	buf.ReadWord();
}

static void RoadTypeMapSpriteGroup(ByteReader &buf, uint8_t idcount, RoadTramType rtt)
{
	std::array<RoadType, ROADTYPE_END> &type_map = (rtt == RTT_TRAM) ? _cur_gps.grffile->tramtype_map : _cur_gps.grffile->roadtype_map;

	std::vector<uint8_t> roadtypes;
	roadtypes.reserve(idcount);
	for (uint i = 0; i < idcount; i++) {
		uint16_t id = buf.ReadExtendedByte();
		roadtypes.push_back(id < ROADTYPE_END ? type_map[id] : INVALID_ROADTYPE);
	}

	uint8_t cidcount = buf.ReadByte();
	for (uint c = 0; c < cidcount; c++) {
		uint8_t ctype = buf.ReadByte();
		uint16_t groupid = buf.ReadWord();
		if (!IsValidGroupID(groupid, "RoadTypeMapSpriteGroup")) continue;

		if (ctype >= ROTSG_END) continue;

		extern RoadTypeInfo _roadtypes[ROADTYPE_END];
		for (auto &roadtype : roadtypes) {
			if (roadtype != INVALID_ROADTYPE) {
				RoadTypeInfo *rti = &_roadtypes[roadtype];

				rti->grffile[ctype] = _cur_gps.grffile;
				rti->group[ctype] = _cur_gps.spritegroups[groupid];
			}
		}
	}

	/* Roadtypes do not use the default group. */
	buf.ReadWord();
}

static void AirportMapSpriteGroup(ByteReader &buf, uint8_t idcount)
{
	if (_cur_gps.grffile->airportspec.empty()) {
		GrfMsg(1, "AirportMapSpriteGroup: No airports defined, skipping");
		return;
	}

	std::vector<uint16_t> airports;
	airports.reserve(idcount);
	for (uint i = 0; i < idcount; i++) {
		airports.push_back(buf.ReadExtendedByte());
	}

	/* Skip the cargo type section, we only care about the default group */
	uint8_t cidcount = buf.ReadByte();
	buf.Skip(cidcount * 3);

	uint16_t groupid = buf.ReadWord();
	if (!IsValidGroupID(groupid, "AirportMapSpriteGroup")) return;

	for (auto &airport : airports) {
		AirportSpec *as = airport >= _cur_gps.grffile->airportspec.size() ? nullptr : _cur_gps.grffile->airportspec[airport].get();

		if (as == nullptr) {
			GrfMsg(1, "AirportMapSpriteGroup: Airport {} undefined, skipping", airport);
			continue;
		}

		as->grf_prop.SetSpriteGroup(0, _cur_gps.spritegroups[groupid]);
	}
}

static void AirportTileMapSpriteGroup(ByteReader &buf, uint8_t idcount)
{
	if (_cur_gps.grffile->airtspec.empty()) {
		GrfMsg(1, "AirportTileMapSpriteGroup: No airport tiles defined, skipping");
		return;
	}

	std::vector<uint16_t> airptiles;
	airptiles.reserve(idcount);
	for (uint i = 0; i < idcount; i++) {
		airptiles.push_back(buf.ReadExtendedByte());
	}

	/* Skip the cargo type section, we only care about the default group */
	uint8_t cidcount = buf.ReadByte();
	buf.Skip(cidcount * 3);

	uint16_t groupid = buf.ReadWord();
	if (!IsValidGroupID(groupid, "AirportTileMapSpriteGroup")) return;

	for (auto &airptile : airptiles) {
		AirportTileSpec *airtsp = airptile >= _cur_gps.grffile->airtspec.size() ? nullptr : _cur_gps.grffile->airtspec[airptile].get();

		if (airtsp == nullptr) {
			GrfMsg(1, "AirportTileMapSpriteGroup: Airport tile {} undefined, skipping", airptile);
			continue;
		}

		airtsp->grf_prop.SetSpriteGroup(0, _cur_gps.spritegroups[groupid]);
	}
}

static void RoadStopMapSpriteGroup(ByteReader &buf, uint8_t idcount)
{
	if (_cur_gps.grffile->roadstops.empty()) {
		GrfMsg(1, "RoadStopMapSpriteGroup: No roadstops defined, skipping");
		return;
	}

	std::vector<uint16_t> roadstops;
	roadstops.reserve(idcount);
	for (uint i = 0; i < idcount; i++) {
		roadstops.push_back(buf.ReadExtendedByte());
	}

	uint8_t cidcount = buf.ReadByte();
	for (uint c = 0; c < cidcount; c++) {
		uint8_t ctype = buf.ReadByte();
		uint16_t groupid = buf.ReadWord();
		if (!IsValidGroupID(groupid, "RoadStopMapSpriteGroup")) continue;

		ctype = TranslateCargo(GSF_ROADSTOPS, ctype);
		if (!IsValidCargoType(ctype)) continue;

		for (auto &roadstop : roadstops) {
			RoadStopSpec *roadstopspec = roadstop >= _cur_gps.grffile->roadstops.size() ? nullptr : _cur_gps.grffile->roadstops[roadstop].get();

			if (roadstopspec == nullptr) {
				GrfMsg(1, "RoadStopMapSpriteGroup: Road stop {} undefined, skipping", roadstop);
				continue;
			}

			roadstopspec->grf_prop.SetSpriteGroup(ctype, _cur_gps.spritegroups[groupid]);
		}
	}

	uint16_t groupid = buf.ReadWord();
	if (!IsValidGroupID(groupid, "RoadStopMapSpriteGroup")) return;

	for (auto &roadstop : roadstops) {
		RoadStopSpec *roadstopspec = roadstop >= _cur_gps.grffile->roadstops.size() ? nullptr : _cur_gps.grffile->roadstops[roadstop].get();

		if (roadstopspec == nullptr) {
			GrfMsg(1, "RoadStopMapSpriteGroup: Road stop {} undefined, skipping.", roadstop);
			continue;
		}

		if (roadstopspec->grf_prop.HasGrfFile()) {
			GrfMsg(1, "RoadStopMapSpriteGroup: Road stop {} mapped multiple times, skipping", roadstop);
			continue;
		}

		roadstopspec->grf_prop.SetSpriteGroup(SpriteGroupCargo::SG_DEFAULT, _cur_gps.spritegroups[groupid]);
		roadstopspec->grf_prop.SetGRFFile(_cur_gps.grffile);
		roadstopspec->grf_prop.local_id = roadstop;
		RoadStopClass::Assign(roadstopspec);
	}
}

static void BadgeMapSpriteGroup(ByteReader &buf, uint8_t idcount)
{
	if (_cur_gps.grffile->badge_map.empty()) {
		GrfMsg(1, "BadgeMapSpriteGroup: No badges defined, skipping");
		return;
	}

	std::vector<uint16_t> local_ids;
	local_ids.reserve(idcount);
	for (uint i = 0; i < idcount; i++) {
		local_ids.push_back(buf.ReadExtendedByte());
	}

	uint8_t cidcount = buf.ReadByte();
	for (uint c = 0; c < cidcount; c++) {
		uint8_t ctype = buf.ReadByte();
		uint16_t groupid = buf.ReadWord();
		if (!IsValidGroupID(groupid, "BadgeMapSpriteGroup")) continue;

		if (ctype >= GSF_END) continue;

		for (const auto &local_id : local_ids) {
			auto found = _cur_gps.grffile->badge_map.find(local_id);
			if (found == std::end(_cur_gps.grffile->badge_map)) {
				GrfMsg(1, "BadgeMapSpriteGroup: Badge {} undefined, skipping", local_id);
				continue;
			}

			auto &badge = *GetBadge(found->second);
			badge.grf_prop.SetSpriteGroup(ctype, _cur_gps.spritegroups[groupid]);
		}
	}

	uint16_t groupid = buf.ReadWord();
	if (!IsValidGroupID(groupid, "BadgeMapSpriteGroup")) return;

	for (auto &local_id : local_ids) {
		auto found = _cur_gps.grffile->badge_map.find(local_id);
		if (found == std::end(_cur_gps.grffile->badge_map)) {
			GrfMsg(1, "BadgeMapSpriteGroup: Badge {} undefined, skipping", local_id);
			continue;
		}

		auto &badge = *GetBadge(found->second);
		badge.grf_prop.SetSpriteGroup(GSF_END, _cur_gps.spritegroups[groupid]);
		badge.grf_prop.SetGRFFile(_cur_gps.grffile);
		badge.grf_prop.local_id = local_id;
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

	uint8_t feature = buf.ReadByte();
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
	SetBit(_cur_gps.grffile->grf_features, feature);

	GrfMsg(6, "FeatureMapSpriteGroup: Feature 0x{:02X}, {} ids", feature, idcount);

	switch (feature) {
		case GSF_TRAINS:
		case GSF_ROADVEHICLES:
		case GSF_SHIPS:
		case GSF_AIRCRAFT:
			VehicleMapSpriteGroup(buf, feature, idcount);
			return;

		case GSF_CANALS:
			CanalMapSpriteGroup(buf, idcount);
			return;

		case GSF_STATIONS:
			StationMapSpriteGroup(buf, idcount);
			return;

		case GSF_HOUSES:
			TownHouseMapSpriteGroup(buf, idcount);
			return;

		case GSF_INDUSTRIES:
			IndustryMapSpriteGroup(buf, idcount);
			return;

		case GSF_INDUSTRYTILES:
			IndustrytileMapSpriteGroup(buf, idcount);
			return;

		case GSF_CARGOES:
			CargoMapSpriteGroup(buf, idcount);
			return;

		case GSF_AIRPORTS:
			AirportMapSpriteGroup(buf, idcount);
			return;

		case GSF_OBJECTS:
			ObjectMapSpriteGroup(buf, idcount);
			break;

		case GSF_RAILTYPES:
			RailTypeMapSpriteGroup(buf, idcount);
			break;

		case GSF_ROADTYPES:
			RoadTypeMapSpriteGroup(buf, idcount, RTT_ROAD);
			break;

		case GSF_TRAMTYPES:
			RoadTypeMapSpriteGroup(buf, idcount, RTT_TRAM);
			break;

		case GSF_AIRPORTTILES:
			AirportTileMapSpriteGroup(buf, idcount);
			return;

		case GSF_ROADSTOPS:
			RoadStopMapSpriteGroup(buf, idcount);
			return;

		case GSF_BADGES:
			BadgeMapSpriteGroup(buf, idcount);
			break;

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
