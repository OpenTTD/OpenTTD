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
#include "error.h"
#include "fileio_func.h"
#include "ini_helper.h"
#include "ini_type.h"
#include "string_func.h"

#include "table/strings.h"

HeightmapLayer::~HeightmapLayer() {
	free(this->information);
}

/** Class for parsing the town layer in an extended heightmap. */
struct TownIniFile : IniLoadFile {
	bool error;

	TownIniFile() : error(false) {}

        virtual FILE *OpenFile(const char *filename, Subdirectory subdir, size_t *size) {
		return FioFOpenFile(filename, "rb", subdir, size);
	}

	virtual void ReportFileError(const char * const pre, const char * const buffer, const char * const post)
        {
		// EHTODO: Is there any way I can include pre/buffer/post in the error message?
		std::cout << "SFTODOERROR: " << pre << buffer << post << std::endl;
		error = true;
        }
};

/**
 * Construct a TownLayer object for use within an extended heightmap.
 * The basic properties are supplied as arguments (which the caller obtains from the top-level metadata.txt)
 * and the towns themselves are parsed from the supplied file.
 * @param width layer width
 * @param height layer height
 * @param default_radius default 'radius' value to use for towns which don't specify their own
 * @param file town file
 */
TownLayer::TownLayer(uint width, uint height, uint default_radius, const char *file)
: HeightmapLayer(HLT_TOWN, width, height), valid(false)
{
	TownIniFile ini;
	char *file2 = str_fmt("./%s", file);
	ini.LoadFromDisk(file2, HEIGHTMAP_DIR);
	free(file2);
	if (ini.error) {
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_PARSING_TOWN_FILE, INVALID_STRING_ID, WL_ERROR);
		return;
	}

	for (IniGroup *town_group = ini.group; town_group != nullptr; town_group = town_group->next) {
		const char *name;
		if (!GetStrGroupItem(town_group, "name", nullptr, &name)) return;
		std::cout << "SFTODOA1 " << name << std::endl; // SFTODO TEMP

		uint posx;
		if (!GetUIntGroupItemWithValidation(town_group, "posx", GET_ITEM_NO_DEFAULT, width - 1, &posx)) return;

		uint posy;
		if (!GetUIntGroupItemWithValidation(town_group, "posy", GET_ITEM_NO_DEFAULT, height - 1, &posy)) return;

		uint radius;
		if (!GetUIntGroupItemWithValidation(town_group, "radius", default_radius, 32, &radius)) return;

		static EnumGroupMap size_lookup({
			{"small",  TSZ_SMALL},
			{"medium", TSZ_MEDIUM},
			{"large",  TSZ_LARGE},
			{"random", TSZ_RANDOM}});
		uint size;
		if (!GetEnumGroupItem(town_group, "size", GET_ITEM_NO_DEFAULT, size_lookup, &size)) return;

		static EnumGroupMap bool_lookup({
			{"false", false},
			{"true", true}});
		uint is_city;
		if (!GetEnumGroupItem(town_group, "city", false, bool_lookup, &is_city)) return;

		static EnumGroupMap layout_lookup({
			{"original", TL_ORIGINAL},
			{"better",   TL_BETTER_ROADS},
			{"2x2",      TL_2X2_GRID},
			{"3x3",      TL_3X3_GRID},
			{"random",   TL_RANDOM}});
		uint layout;
		if (!GetEnumGroupItem(town_group, "layout", TL_RANDOM, layout_lookup, &layout)) return;

		this->towns.emplace_back(name, posx, posy, radius, static_cast<TownSize>(size), is_city, static_cast<TownLayout>(layout));
	}

	this->valid = true;
}

TownLayer::~TownLayer()
{
}
