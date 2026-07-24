/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_sl.cpp Code handling saving and loading of map. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/map_sl_compat.h"

#include "../map_func.h"
#include "../core/bitmath_func.hpp"
#include "../fios.h"

#include "../safeguards.h"

static uint32_t _map_dim_x;
static uint32_t _map_dim_y;

static const SaveLoad _map_desc[] = {
	SLEG_CONDVAR("dim_x", _map_dim_x, VarTypes::U32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),
	SLEG_CONDVAR("dim_y", _map_dim_y, VarTypes::U32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),
};

struct MAPSChunkHandler : ChunkHandler {
	MAPSChunkHandler() : ChunkHandler("MAPS", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_map_desc);

		_map_dim_x = Map::SizeX();
		_map_dim_y = Map::SizeY();

		SlSetArrayIndex(0);
		SlGlobList(_map_desc);
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_map_desc, _map_sl_compat);

		if (!IsSavegameVersionBefore(SaveLoadVersion::RiffToArray) && SlIterateArray() == -1) return;
		SlGlobList(slt);
		if (!IsSavegameVersionBefore(SaveLoadVersion::RiffToArray) && SlIterateArray() != -1) SlErrorCorrupt("Too many MAPS entries");

		Map::Allocate(_map_dim_x, _map_dim_y);
	}

	void LoadCheck(size_t) const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_map_desc, _map_sl_compat);

		if (!IsSavegameVersionBefore(SaveLoadVersion::RiffToArray) && SlIterateArray() == -1) return;
		SlGlobList(slt);
		if (!IsSavegameVersionBefore(SaveLoadVersion::RiffToArray) && SlIterateArray() != -1) SlErrorCorrupt("Too many MAPS entries");

		_load_check_data.map_size_x = _map_dim_x;
		_load_check_data.map_size_y = _map_dim_y;
	}
};

static constexpr uint MAP_SL_BUF_SIZE = MIN_MAP_SIZE * MIN_MAP_SIZE; ///< Buffer size for saving/loading the map array. Sized to the smallest map.

struct MAPTChunkHandler : ChunkHandler {
	MAPTChunkHandler() : ChunkHandler("MAPT", ChunkType::Riff) {}

	void Load() const override
	{
		std::array<uint8_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		for (TileIndex i{}; i != size;) {
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U8);
			for (auto b : buf) Tile(i++).type() = b;
		}
	}

	void Save() const override
	{
		std::array<uint8_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		SlSetLength(size);
		for (TileIndex i{}; i != size;) {
			for (auto &b : buf) b = Tile(i++).type();
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U8);
		}
	}
};

struct MAPHChunkHandler : ChunkHandler {
	MAPHChunkHandler() : ChunkHandler("MAPH", ChunkType::Riff) {}

	void Load() const override
	{
		std::array<uint8_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		for (TileIndex i{}; i != size;) {
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U8);
			for (auto b : buf) Tile(i++).height() = b;
		}
	}

	void Save() const override
	{
		std::array<uint8_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		SlSetLength(size);
		for (TileIndex i{}; i != size;) {
			for (auto &b : buf) b = Tile(i++).height();
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U8);
		}
	}
};

struct MAPOChunkHandler : ChunkHandler {
	MAPOChunkHandler() : ChunkHandler("MAPO", ChunkType::Riff) {}

	void Load() const override
	{
		std::array<uint8_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		for (TileIndex i{}; i != size;) {
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U8);
			for (auto b : buf) Tile(i++).m1() = b;
		}
	}

	void Save() const override
	{
		std::array<uint8_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		SlSetLength(size);
		for (TileIndex i{}; i != size;) {
			for (auto &b : buf) b = Tile(i++).m1();
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U8);
		}
	}
};

struct MAP2ChunkHandler : ChunkHandler {
	MAP2ChunkHandler() : ChunkHandler("MAP2", ChunkType::Riff) {}

	void Load() const override
	{
		std::array<uint16_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		for (TileIndex i{}; i != size;) {
			SlCopy(buf.data(), MAP_SL_BUF_SIZE,
				/* In those versions the m2 was 8 bits */
				IsSavegameVersionBefore(SaveLoadVersion::BigMap) ? VarFileType::U8 | VarMemType::U16 : VarTypes::U16
			);
			for (auto b : buf) Tile(i++).m2() = b;
		}
	}

	void Save() const override
	{
		std::array<uint16_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		SlSetLength(static_cast<uint32_t>(size) * sizeof(uint16_t));
		for (TileIndex i{}; i != size;) {
			for (auto &b : buf) b = Tile(i++).m2();
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U16);
		}
	}
};

struct M3LOChunkHandler : ChunkHandler {
	M3LOChunkHandler() : ChunkHandler("M3LO", ChunkType::Riff) {}

	void Load() const override
	{
		std::array<uint8_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		for (TileIndex i{}; i != size;) {
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U8);
			for (auto b : buf) Tile(i++).m3() = b;
		}
	}

	void Save() const override
	{
		std::array<uint8_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		SlSetLength(size);
		for (TileIndex i{}; i != size;) {
			for (auto &b : buf) b = Tile(i++).m3();
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U8);
		}
	}
};

struct M3HIChunkHandler : ChunkHandler {
	M3HIChunkHandler() : ChunkHandler("M3HI", ChunkType::Riff) {}

	void Load() const override
	{
		std::array<uint8_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		for (TileIndex i{}; i != size;) {
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U8);
			for (auto b : buf) Tile(i++).m4() = b;
		}
	}

	void Save() const override
	{
		std::array<uint8_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		SlSetLength(size);
		for (TileIndex i{}; i != size;) {
			for (auto &b : buf) b = Tile(i++).m4();
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U8);
		}
	}
};

struct MAP5ChunkHandler : ChunkHandler {
	MAP5ChunkHandler() : ChunkHandler("MAP5", ChunkType::Riff) {}

	void Load() const override
	{
		std::array<uint8_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		for (TileIndex i{}; i != size;) {
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U8);
			for (auto b : buf) Tile(i++).m5() = b;
		}
	}

	void Save() const override
	{
		std::array<uint8_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		SlSetLength(size);
		for (TileIndex i{}; i != size;) {
			for (auto &b : buf) b = Tile(i++).m5();
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U8);
		}
	}
};

struct MAPEChunkHandler : ChunkHandler {
	MAPEChunkHandler() : ChunkHandler("MAPE", ChunkType::Riff) {}

	void Load() const override
	{
		uint size = Map::Size();

		if (IsSavegameVersionBefore(SaveLoadVersion::BridgeWormhole)) {
			/* Since this loads 4 tiles per read byte, amend the buffer size to suit. */
			std::array<uint8_t, MAP_SL_BUF_SIZE / 4> buf;
			for (TileIndex i{}; i != size;) {
				SlCopy(buf.data(), buf.size(), VarTypes::U8);
				for (auto b : buf) {
					Tile(i++).m6() = GB(b, 0, 2);
					Tile(i++).m6() = GB(b, 2, 2);
					Tile(i++).m6() = GB(b, 4, 2);
					Tile(i++).m6() = GB(b, 6, 2);
				}
			}
		} else {
			std::array<uint8_t, MAP_SL_BUF_SIZE> buf;
			for (TileIndex i{}; i != size;) {
				SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U8);
				for (auto b : buf) Tile(i++).m6() = b;
			}
		}
	}

	void Save() const override
	{
		std::array<uint8_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		SlSetLength(size);
		for (TileIndex i{}; i != size;) {
			for (auto &b : buf) b = Tile(i++).m6();
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U8);
		}
	}
};

struct MAP7ChunkHandler : ChunkHandler {
	MAP7ChunkHandler() : ChunkHandler("MAP7", ChunkType::Riff) {}

	void Load() const override
	{
		std::array<uint8_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		for (TileIndex i{}; i != size;) {
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U8);
			for (auto b : buf) Tile(i++).m7() = b;
		}
	}

	void Save() const override
	{
		std::array<uint8_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		SlSetLength(size);
		for (TileIndex i{}; i != size;) {
			for (auto &b : buf) b = Tile(i++).m7();
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U8);
		}
	}
};

struct MAP8ChunkHandler : ChunkHandler {
	MAP8ChunkHandler() : ChunkHandler("MAP8", ChunkType::Riff) {}

	void Load() const override
	{
		std::array<uint16_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		for (TileIndex i{}; i != size;) {
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U16);
			for (auto b : buf) Tile(i++).m8() = b;
		}
	}

	void Save() const override
	{
		std::array<uint16_t, MAP_SL_BUF_SIZE> buf;
		uint size = Map::Size();

		SlSetLength(static_cast<uint32_t>(size) * sizeof(uint16_t));
		for (TileIndex i{}; i != size;) {
			for (auto &b : buf) b = Tile(i++).m8();
			SlCopy(buf.data(), MAP_SL_BUF_SIZE, VarTypes::U16);
		}
	}
};

static const MAPSChunkHandler MAPS;
static const MAPTChunkHandler MAPT;
static const MAPHChunkHandler MAPH;
static const MAPOChunkHandler MAPO;
static const MAP2ChunkHandler MAP2;
static const M3LOChunkHandler M3LO;
static const M3HIChunkHandler M3HI;
static const MAP5ChunkHandler MAP5;
static const MAPEChunkHandler MAPE;
static const MAP7ChunkHandler MAP7;
static const MAP8ChunkHandler MAP8;
static const ChunkHandlerRef map_chunk_handlers[] = {
	MAPS,
	MAPT,
	MAPH,
	MAPO,
	MAP2,
	M3LO,
	M3HI,
	MAP5,
	MAPE,
	MAP7,
	MAP8,
};

extern const ChunkHandlerTable _map_chunk_handlers(map_chunk_handlers);
