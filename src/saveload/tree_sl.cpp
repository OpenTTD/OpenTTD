/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tree_sl.cpp Code handling saving and loading of trees. */

#include "../stdafx.h"

#include "saveload.h"
#include "newgrf_sl.h"

#include "../safeguards.h"

struct TRIDChunkHandler : NewGRFMappingChunkHandler {
	TRIDChunkHandler() : NewGRFMappingChunkHandler("TRID", _tree_mngr) {}
};

static const TRIDChunkHandler TRID;
static const ChunkHandlerRef tree_chunk_handlers[] = {
	TRID,
};

extern const ChunkHandlerTable _tree_chunk_handlers(tree_chunk_handlers);
