/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file map_sl.cpp Code handling saving and loading of map */

#include "../stdafx.h"
#include "../map_func.h"
#include "../core/bitmath_func.hpp"
#include "../fios.h"

#include "saveload.h"

#include "../safeguards.h"

static uint32 _map_dim_x;
static uint32 _map_dim_y;

static const SaveLoadGlobVarList _map_dimensions[] = {
	SLEG_CONDVAR(_map_dim_x, SLE_UINT32, 6, SL_MAX_VERSION),
	SLEG_CONDVAR(_map_dim_y, SLE_UINT32, 6, SL_MAX_VERSION),
	    SLEG_END()
};

static void Save_MAPS()
{
	_map_dim_x = MapSizeX();
	_map_dim_y = MapSizeY();
	SlGlobList(_map_dimensions);
}

static void Load_MAPS()
{
	SlGlobList(_map_dimensions);
	AllocateMap(_map_dim_x, _map_dim_y);
}

static void Check_MAPS()
{
	SlGlobList(_map_dimensions);
	_load_check_data.map_size_x = _map_dim_x;
	_load_check_data.map_size_y = _map_dim_y;
}

static const uint MAP_SL_BUF_SIZE = 4096;

static void Load_MAPT()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	for (TileIndex i = 0; i != size;) {
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _m[i++].type = buf[j];
	}
}

static void Save_MAPT()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size);
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _m[i++].type;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
	}
}

static void Load_MAPH()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	for (TileIndex i = 0; i != size;) {
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _m[i++].height = buf[j];
	}
}

static void Save_MAPH()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size);
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _m[i++].height;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
	}
}

static void Load_MAP1()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	for (TileIndex i = 0; i != size;) {
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _m[i++].m1 = buf[j];
	}
}

static void Save_MAP1()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size);
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _m[i++].m1;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
	}
}

static void Load_MAP2()
{
	SmallStackSafeStackAlloc<uint16, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	for (TileIndex i = 0; i != size;) {
		SlArray(buf, MAP_SL_BUF_SIZE,
			/* In those versions the m2 was 8 bits */
			IsSavegameVersionBefore(5) ? SLE_FILE_U8 | SLE_VAR_U16 : SLE_UINT16
		);
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _m[i++].m2 = buf[j];
	}
}

static void Save_MAP2()
{
	SmallStackSafeStackAlloc<uint16, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size * sizeof(uint16));
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _m[i++].m2;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT16);
	}
}

static void Load_MAP3()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	for (TileIndex i = 0; i != size;) {
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _m[i++].m3 = buf[j];
	}
}

static void Save_MAP3()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size);
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _m[i++].m3;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
	}
}

static void Load_MAP4()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	for (TileIndex i = 0; i != size;) {
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _m[i++].m4 = buf[j];
	}
}

static void Save_MAP4()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size);
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _m[i++].m4;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
	}
}

static void Load_MAP5()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	for (TileIndex i = 0; i != size;) {
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _m[i++].m5 = buf[j];
	}
}

static void Save_MAP5()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size);
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _m[i++].m5;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
	}
}

static void Load_MAP6()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	if (IsSavegameVersionBefore(42)) {
		for (TileIndex i = 0; i != size;) {
			/* 1024, otherwise we overflow on 64x64 maps! */
			SlArray(buf, 1024, SLE_UINT8);
			for (uint j = 0; j != 1024; j++) {
				_me[i++].m6 = GB(buf[j], 0, 2);
				_me[i++].m6 = GB(buf[j], 2, 2);
				_me[i++].m6 = GB(buf[j], 4, 2);
				_me[i++].m6 = GB(buf[j], 6, 2);
			}
		}
	} else {
		for (TileIndex i = 0; i != size;) {
			SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
			for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _me[i++].m6 = buf[j];
		}
	}
}

static void Save_MAP6()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size);
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _me[i++].m6;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
	}
}

static void Load_MAP7()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	for (TileIndex i = 0; i != size;) {
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _me[i++].m7 = buf[j];
	}
}

static void Save_MAP7()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size);
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _me[i++].m7;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
	}
}

static void Load_MAP8()
{
	SmallStackSafeStackAlloc<uint16, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	for (TileIndex i = 0; i != size;) {
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT16);
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _me[i++].m8 = buf[j];
	}
}

static void Save_MAP8()
{
	SmallStackSafeStackAlloc<uint16, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size * sizeof(uint16));
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _me[i++].m8;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT16);
	}
}


extern const ChunkHandler _map_chunk_handlers[] = {
	{ 'MAPS', Save_MAPS, Load_MAPS, NULL, Check_MAPS, CH_RIFF },
	{ 'MAPT', Save_MAPT, Load_MAPT, NULL, NULL,       CH_RIFF },
	{ 'MAPH', Save_MAPH, Load_MAPH, NULL, NULL,       CH_RIFF },
	{ 'MAPO', Save_MAP1, Load_MAP1, NULL, NULL,       CH_RIFF },
	{ 'MAP2', Save_MAP2, Load_MAP2, NULL, NULL,       CH_RIFF },
	{ 'M3LO', Save_MAP3, Load_MAP3, NULL, NULL,       CH_RIFF },
	{ 'M3HI', Save_MAP4, Load_MAP4, NULL, NULL,       CH_RIFF },
	{ 'MAP5', Save_MAP5, Load_MAP5, NULL, NULL,       CH_RIFF },
	{ 'MAPE', Save_MAP6, Load_MAP6, NULL, NULL,       CH_RIFF },
	{ 'MAP7', Save_MAP7, Load_MAP7, NULL, NULL,       CH_RIFF },
	{ 'MAP8', Save_MAP8, Load_MAP8, NULL, NULL,       CH_RIFF | CH_LAST },
};
