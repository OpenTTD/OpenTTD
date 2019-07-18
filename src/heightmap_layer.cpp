/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file heightmap_layer.cpp Base implementation of heightmap layers. */

#include <iostream> // SFTODO TEMP
#include "stdafx.h"
#include "heightmap_layer_base.h"
#include "ini_type.h" // SFTODO: MOVE THIS IF/WHEN TOWNINIFILE MOVES INTO ANOTHER FILE
#include "fileio_func.h" // SFTODO: MOVE THIS IF/WHEN TOWNINIFILE MOVES INTO ANOTHER FILE
#include "string_func.h" // SFTODO: MOVE THIS IF/WHEN TOWNINIFILE MOVES INTO ANOTHER FILE

HeightmapLayer::~HeightmapLayer() {
	free(this->information);
}

// SFTODO: THIS SHOULD PROBABLY LIVE IN A SEPARATE FILE, OR AT LEAST IN THE CPP FILE CONTAINING TOWNLAYER
struct TownIniFile : IniLoadFile {
	bool error;

	TownIniFile() : error(false) {}

        virtual FILE *OpenFile(const char *filename, Subdirectory subdir, size_t *size) {
		// SFTODO: SHOULD I BE PASSING "b" FLAG?? IniFile::OpenFile() DOES, SO BLINDLY COPYING THIS FOR NOW...
		return FioFOpenFile(filename, "rb", subdir, size);
	}

	virtual void ReportFileError(const char * const pre, const char * const buffer, const char * const post)
        {
		// EHTODO: Is there any way I can include pre/buffer/post in the error message?
		std::cout << "SFTODOERROR: " << pre << buffer << post << std::endl;
		error = true;
        }
};

// SFTODO: DERIVED CLASS TOWNLAYER SHOULD PROBABLY HAVE ITS OWN FILE

TownLayer::TownLayer(uint width, uint height, const char *file)
: HeightmapLayer(HLT_TOWN, width, height), valid(false)
{
	TownIniFile ini;
	// SFTODO: I am probably using TarScanner completely wrong, having to pass "./" at start of filename seems a bit iffy
	char *file2 = str_fmt("./%s", file);
	ini.LoadFromDisk(file2, HEIGHTMAP_DIR);
	if (ini.error) {
		assert(false); // SFTODO PROPER ERROR HANDLING
		return;
	}

	for (IniGroup *town_group = ini.group; town_group != nullptr; town_group = town_group->next) {
		IniItem *name = town_group->GetItem("name", false);
		if (name == nullptr) {
			assert(false); // SFTODO PROPER ERROR HANDLING
			return;
		}
		std::cout << "SFTODOA1 " << name->value << std::endl; // SFTODO TEMP

		IniItem *posx = town_group->GetItem("posx", false);
		if (posx == nullptr) {
			assert(false); // SFTODO PROPER ERROR HANDLING
			return;
		}

		IniItem *posy = town_group->GetItem("posy", false);
		if (posy == nullptr) {
			assert(false); // SFTODO PROPER ERROR HANDLING
			return;
		}

		IniItem *size_item = town_group->GetItem("size", false);
		if (size_item == nullptr) {
			assert(false); // SFTODO PROPER ERROR HANDLING
			return;
		}
		// SFTODO: CASE SENSITIVITY
		TownSize size;
		if (strcmp(size_item->value, "small") == 0) {
			size = TSZ_SMALL;
		} else if (strcmp(size_item->value, "medium") == 0) {
			size = TSZ_MEDIUM;
		} else if (strcmp(size_item->value, "large") == 0) {
			size = TSZ_LARGE;
		} else if (strcmp(size_item->value, "random") == 0) {
			size = TSZ_RANDOM;
		} else {
			assert(false); // SFTODO PROPER ERROR HANDLING
			return;
		}

		bool is_city = false;
		IniItem *city = town_group->GetItem("city", false);
		if (city != nullptr) {
			// SFTODO: CASE SENSITIVITY?
			if (strcmp(city->value, "false") == 0) {
				is_city = false;
			} else if (strcmp(city->value, "true") == 0) {
				is_city = true;
			} else {
				assert(false); // SFTODO PROPER ERROR HANDLING
				return;
			}
		}

		IniItem *layout_item = town_group->GetItem("layout", false);
		if (layout_item == nullptr) {
			assert(false); // SFTODO PROPER ERROR HANDLING
			return;
		}
		TownLayout layout;
		// SFTODO: CASE SENSITIVITY
		if (strcmp(layout_item->value, "original") == 0) {
			layout = TL_ORIGINAL;
		} else if (strcmp(layout_item->value, "better") == 0) {
			layout = TL_BETTER_ROADS;
		} else if (strcmp(layout_item->value, "2x2") == 0) {
			layout = TL_2X2_GRID;
		} else if (strcmp(layout_item->value, "3x3") == 0) {
			layout = TL_3X3_GRID;
		} else if (strcmp(layout_item->value, "random") == 0) {
			layout = TL_RANDOM;
		} else {
			assert(false); // SFTODO PROPER ERROR HANDLING
			return;
		}

		// SFTODO: USE OF ATOI() MEANS NO ERROR CHECKING
		this->towns.emplace_back(name->value, atoi(posx->value), atoi(posy->value), size, is_city, layout);
	}

	this->valid = true; // SFTODO: MAKE SURE THIS IS LAST LINE OF CTOR!
}

TownLayer::~TownLayer()
{
}
