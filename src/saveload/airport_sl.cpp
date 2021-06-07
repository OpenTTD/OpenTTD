/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file airport_sl.cpp Code handling saving and loading airport ids */

#include "../stdafx.h"

#include "saveload.h"
#include "newgrf_sl.h"

#include "../safeguards.h"

struct APIDChunkHandler : ChunkHandler {
	APIDChunkHandler() : ChunkHandler('APID', CH_TABLE) {}

	void Save() const override
	{
		Save_NewGRFMapping(_airport_mngr);
	}

	void Load() const override
	{
		Load_NewGRFMapping(_airport_mngr);
	}
};

struct ATIDChunkHandler : ChunkHandler {
	ATIDChunkHandler() : ChunkHandler('ATID', CH_TABLE) {}

	void Save() const override
	{
		Save_NewGRFMapping(_airporttile_mngr);
	}

	void Load() const override
	{
		Load_NewGRFMapping(_airporttile_mngr);
	}
};

static const ATIDChunkHandler ATID;
static const APIDChunkHandler APID;
static const ChunkHandlerRef airport_chunk_handlers[] = {
	ATID,
	APID,
};

extern const ChunkHandlerTable _airport_chunk_handlers(airport_chunk_handlers);
