/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sprite_file.cpp Implementation of logic specific to the SpriteFile class. */

#include "../stdafx.h"
#include "sprite_file_type.hpp"

/** Signature of a container version 2 GRF. */
extern const uint8_t _grf_cont_v2_sig[8] = {'G', 'R', 'F', 0x82, 0x0D, 0x0A, 0x1A, 0x0A};

/**
 * Get the container version of the currently opened GRF file.
 * @return Container version of the GRF file or 0 if the file is corrupt/no GRF file.
 */
static uint8_t GetGRFContainerVersion(SpriteFile &file)
{
	size_t pos = file.GetPos();

	if (file.ReadWord() == 0) {
		/* Check for GRF container version 2, which is identified by the bytes
		 * '47 52 46 82 0D 0A 1A 0A' at the start of the file. */
		for (uint i = 0; i < lengthof(_grf_cont_v2_sig); i++) {
			if (file.ReadByte() != _grf_cont_v2_sig[i]) return 0; // Invalid format
		}

		return 2;
	}

	/* Container version 1 has no header, rewind to start. */
	file.SeekTo(pos, SEEK_SET);
	return 1;
}

/**
 * Create the SpriteFile.
 * @param filename      Name of the file at the disk.
 * @param subdir        The sub directory to search this file in.
 * @param palette_remap Whether a palette remap needs to be performed for this file.
 */
SpriteFile::SpriteFile(const std::string &filename, Subdirectory subdir, bool palette_remap)
	: RandomAccessFile(filename, subdir), palette_remap(palette_remap)
{
	this->container_version = GetGRFContainerVersion(*this);
	this->content_begin = this->GetPos();
}
