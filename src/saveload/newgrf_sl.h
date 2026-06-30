/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_sl.h Code handling saving and loading of NewGRF mappings. */

#ifndef SAVELOAD_NEWGRF_SL_H
#define SAVELOAD_NEWGRF_SL_H

#include "../newgrf_commons.h"
#include "saveload.h"

/** Chunk handler for data of the OverrideManagerBase. */
struct NewGRFMappingChunkHandler : ChunkHandler {
	OverrideManagerBase &mapping; ///< The override manager to save/load.

	/**
	 * Create the handler.
	 * @param id The identifier of the chunk
	 * @param mapping The override manager to save/load.
	 */
	NewGRFMappingChunkHandler(ChunkId id, OverrideManagerBase &mapping) : ChunkHandler(id, ChunkType::Table), mapping(mapping) {}
	void Save() const override;
	void Load() const override;
};

#endif /* SAVELOAD_NEWGRF_SL_H */
