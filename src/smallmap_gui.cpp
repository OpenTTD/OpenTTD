/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file smallmap_gui.cpp GUI that shows a small map of the world with metadata like owner or height. */

#include "stdafx.h"
#include "clear_map.h"
#include "industry.h"
#include "station_map.h"
#include "landscape.h"
#include "tree_map.h"
#include "viewport_func.h"
#include "town.h"
#include "tunnelbridge_map.h"
#include "core/endian_func.hpp"
#include "vehicle_base.h"
#include "sound_func.h"
#include "window_func.h"
#include "company_base.h"

#include "smallmap_gui.h"

#include "table/strings.h"

#include <bitset>

#include "safeguards.h"

static int _smallmap_industry_count; ///< Number of used industries
static int _smallmap_company_count;  ///< Number of entries in the owner legend.
static int _smallmap_cargo_count;    ///< Number of cargos in the link stats legend.

/** Link stat colours shown in legenda. */
static uint8 _linkstat_colours_in_legenda[] = {0, 1, 3, 5, 7, 9, 11};

static const int NUM_NO_COMPANY_ENTRIES = 4; ///< Number of entries in the owner legend that are not companies.

static const uint8 PC_ROUGH_LAND      = 0x52; ///< Dark green palette colour for rough land.
static const uint8 PC_GRASS_LAND      = 0x54; ///< Dark green palette colour for grass land.
static const uint8 PC_BARE_LAND       = 0x37; ///< Brown palette colour for bare land.
static const uint8 PC_FIELDS          = 0x25; ///< Light brown palette colour for fields.
static const uint8 PC_TREES           = 0x57; ///< Green palette colour for trees.
static const uint8 PC_WATER           = 0xCA; ///< Dark blue palette colour for water.

/** Macro for ordinary entry of LegendAndColour */
#define MK(a, b) {a, b, INVALID_INDUSTRYTYPE, 0, INVALID_COMPANY, true, false, false}

/** Macro for a height legend entry with configurable colour. */
#define MC(col_break)  {0, STR_TINY_BLACK_HEIGHT, INVALID_INDUSTRYTYPE, 0, INVALID_COMPANY, true, false, col_break}

/** Macro for non-company owned property entry of LegendAndColour */
#define MO(a, b) {a, b, INVALID_INDUSTRYTYPE, 0, INVALID_COMPANY, true, false, false}

/** Macro used for forcing a rebuild of the owner legend the first time it is used. */
#define MOEND() {0, 0, INVALID_INDUSTRYTYPE, 0, OWNER_NONE, true, true, false}

/** Macro for end of list marker in arrays of LegendAndColour */
#define MKEND() {0, STR_NULL, INVALID_INDUSTRYTYPE, 0, INVALID_COMPANY, true, true, false}

/**
 * Macro for break marker in arrays of LegendAndColour.
 * It will have valid data, though
 */
#define MS(a, b) {a, b, INVALID_INDUSTRYTYPE, 0, INVALID_COMPANY, true, false, true}

/** Legend text giving the colours to look for on the minimap */
static LegendAndColour _legend_land_contours[] = {
	MK(PC_BLACK,           STR_SMALLMAP_LEGENDA_ROADS),
	MK(PC_GREY,            STR_SMALLMAP_LEGENDA_RAILROADS),
	MK(PC_LIGHT_BLUE,      STR_SMALLMAP_LEGENDA_STATIONS_AIRPORTS_DOCKS),
	MK(PC_DARK_RED,        STR_SMALLMAP_LEGENDA_BUILDINGS_INDUSTRIES),
	MK(PC_WHITE,           STR_SMALLMAP_LEGENDA_VEHICLES),

	/* Placeholders for the colours and heights of the legend.
	 * The following values are set at BuildLandLegend() based
	 * on each colour scheme and the maximum map height. */
	MC(true),
	MC(false),
	MC(false),
	MC(false),
	MC(false),
	MC(false),
	MC(true),
	MC(false),
	MC(false),
	MC(false),
	MC(false),
	MC(false),
	MKEND()
};

static const LegendAndColour _legend_vehicles[] = {
	MK(PC_RED,             STR_SMALLMAP_LEGENDA_TRAINS),
	MK(PC_YELLOW,          STR_SMALLMAP_LEGENDA_ROAD_VEHICLES),
	MK(PC_LIGHT_BLUE,      STR_SMALLMAP_LEGENDA_SHIPS),
	MK(PC_WHITE,           STR_SMALLMAP_LEGENDA_AIRCRAFT),

	MS(PC_BLACK,           STR_SMALLMAP_LEGENDA_TRANSPORT_ROUTES),
	MK(PC_DARK_RED,        STR_SMALLMAP_LEGENDA_BUILDINGS_INDUSTRIES),
	MKEND()
};

static const LegendAndColour _legend_routes[] = {
	MK(PC_BLACK,           STR_SMALLMAP_LEGENDA_ROADS),
	MK(PC_GREY,            STR_SMALLMAP_LEGENDA_RAILROADS),
	MK(PC_DARK_RED,        STR_SMALLMAP_LEGENDA_BUILDINGS_INDUSTRIES),

	MS(PC_VERY_DARK_BROWN, STR_SMALLMAP_LEGENDA_RAILROAD_STATION),
	MK(PC_ORANGE,          STR_SMALLMAP_LEGENDA_TRUCK_LOADING_BAY),
	MK(PC_YELLOW,          STR_SMALLMAP_LEGENDA_BUS_STATION),
	MK(PC_RED,             STR_SMALLMAP_LEGENDA_AIRPORT_HELIPORT),
	MK(PC_LIGHT_BLUE,      STR_SMALLMAP_LEGENDA_DOCK),
	MKEND()
};

static const LegendAndColour _legend_vegetation[] = {
	MK(PC_ROUGH_LAND,      STR_SMALLMAP_LEGENDA_ROUGH_LAND),
	MK(PC_GRASS_LAND,      STR_SMALLMAP_LEGENDA_GRASS_LAND),
	MK(PC_BARE_LAND,       STR_SMALLMAP_LEGENDA_BARE_LAND),
	MK(PC_FIELDS,          STR_SMALLMAP_LEGENDA_FIELDS),
	MK(PC_TREES,           STR_SMALLMAP_LEGENDA_TREES),
	MK(PC_GREEN,           STR_SMALLMAP_LEGENDA_FOREST),

	MS(PC_GREY,            STR_SMALLMAP_LEGENDA_ROCKS),
	MK(PC_ORANGE,          STR_SMALLMAP_LEGENDA_DESERT),
	MK(PC_LIGHT_BLUE,      STR_SMALLMAP_LEGENDA_SNOW),
	MK(PC_BLACK,           STR_SMALLMAP_LEGENDA_TRANSPORT_ROUTES),
	MK(PC_DARK_RED,        STR_SMALLMAP_LEGENDA_BUILDINGS_INDUSTRIES),
	MKEND()
};

static LegendAndColour _legend_land_owners[NUM_NO_COMPANY_ENTRIES + MAX_COMPANIES + 1] = {
	MO(PC_WATER,           STR_SMALLMAP_LEGENDA_WATER),
	MO(0x00,               STR_SMALLMAP_LEGENDA_NO_OWNER), // This colour will vary depending on settings.
	MO(PC_DARK_RED,        STR_SMALLMAP_LEGENDA_TOWNS),
	MO(PC_DARK_GREY,       STR_SMALLMAP_LEGENDA_INDUSTRIES),
	/* The legend will be terminated the first time it is used. */
	MOEND(),
};

#undef MK
#undef MC
#undef MS
#undef MO
#undef MOEND
#undef MKEND

/** Legend entries for the link stats view. */
static LegendAndColour _legend_linkstats[NUM_CARGO + lengthof(_linkstat_colours_in_legenda) + 1];
/**
 * Allow room for all industries, plus a terminator entry
 * This is required in order to have the industry slots all filled up
 */
static LegendAndColour _legend_from_industries[NUM_INDUSTRYTYPES + 1];
/** For connecting industry type to position in industries list(small map legend) */
static uint _industry_to_list_pos[NUM_INDUSTRYTYPES];
/** Show heightmap in industry and owner mode of smallmap window. */
static bool _smallmap_show_heightmap = false;
/** Highlight a specific industry type */
static IndustryType _smallmap_industry_highlight = INVALID_INDUSTRYTYPE;
/** State of highlight blinking */
static bool _smallmap_industry_highlight_state;
/** For connecting company ID to position in owner list (small map legend) */
static uint _company_to_list_pos[MAX_COMPANIES];

/**
 * Fills an array for the industries legends.
 */
void BuildIndustriesLegend()
{
	uint j = 0;

	/* Add each name */
	for (uint i = 0; i < NUM_INDUSTRYTYPES; i++) {
		IndustryType ind = _sorted_industry_types[i];
		const IndustrySpec *indsp = GetIndustrySpec(ind);
		if (indsp->enabled) {
			_legend_from_industries[j].legend = indsp->name;
			_legend_from_industries[j].colour = indsp->map_colour;
			_legend_from_industries[j].type = ind;
			_legend_from_industries[j].show_on_map = true;
			_legend_from_industries[j].col_break = false;
			_legend_from_industries[j].end = false;

			/* Store widget number for this industry type. */
			_industry_to_list_pos[ind] = j;
			j++;
		}
	}
	/* Terminate the list */
	_legend_from_industries[j].end = true;

	/* Store number of enabled industries */
	_smallmap_industry_count = j;
}

/**
 * Populate legend table for the link stat view.
 */
void BuildLinkStatsLegend()
{
	/* Clear the legend */
	memset(_legend_linkstats, 0, sizeof(_legend_linkstats));

	uint i = 0;
	for (; i < _sorted_cargo_specs_size; ++i) {
		const CargoSpec *cs = _sorted_cargo_specs[i];

		_legend_linkstats[i].legend = cs->name;
		_legend_linkstats[i].colour = cs->legend_colour;
		_legend_linkstats[i].type = cs->Index();
		_legend_linkstats[i].show_on_map = true;
	}

	_legend_linkstats[i].col_break = true;
	_smallmap_cargo_count = i;

	for (; i < _smallmap_cargo_count + lengthof(_linkstat_colours_in_legenda); ++i) {
		_legend_linkstats[i].legend = STR_EMPTY;
		_legend_linkstats[i].colour = LinkGraphOverlay::LINK_COLOURS[_linkstat_colours_in_legenda[i - _smallmap_cargo_count]];
		_legend_linkstats[i].show_on_map = true;
	}

	_legend_linkstats[_smallmap_cargo_count].legend = STR_LINKGRAPH_LEGEND_UNUSED;
	_legend_linkstats[i - 1].legend = STR_LINKGRAPH_LEGEND_OVERLOADED;
	_legend_linkstats[(_smallmap_cargo_count + i - 1) / 2].legend = STR_LINKGRAPH_LEGEND_SATURATED;
	_legend_linkstats[i].end = true;
}

static const LegendAndColour * const _legend_table[] = {
	_legend_land_contours,
	_legend_vehicles,
	_legend_from_industries,
	_legend_linkstats,
	_legend_routes,
	_legend_vegetation,
	_legend_land_owners,
};

#define MKCOLOUR(x)         TO_LE32X(x)

#define MKCOLOUR_XXXX(x)    (MKCOLOUR(0x01010101) * (uint)(x))
#define MKCOLOUR_X0X0(x)    (MKCOLOUR(0x01000100) * (uint)(x))
#define MKCOLOUR_0X0X(x)    (MKCOLOUR(0x00010001) * (uint)(x))
#define MKCOLOUR_0XX0(x)    (MKCOLOUR(0x00010100) * (uint)(x))
#define MKCOLOUR_X00X(x)    (MKCOLOUR(0x01000001) * (uint)(x))

#define MKCOLOUR_XYXY(x, y) (MKCOLOUR_X0X0(x) | MKCOLOUR_0X0X(y))
#define MKCOLOUR_XYYX(x, y) (MKCOLOUR_X00X(x) | MKCOLOUR_0XX0(y))

#define MKCOLOUR_0000       MKCOLOUR_XXXX(0x00)
#define MKCOLOUR_0FF0       MKCOLOUR_0XX0(0xFF)
#define MKCOLOUR_F00F       MKCOLOUR_X00X(0xFF)
#define MKCOLOUR_FFFF       MKCOLOUR_XXXX(0xFF)

#include "table/heightmap_colours.h"

/** Colour scheme of the smallmap. */
struct SmallMapColourScheme {
	uint32 *height_colours;            ///< Cached colours for each level in a map.
	const uint32 *height_colours_base; ///< Base table for determining the colours
	size_t colour_count;               ///< The number of colours.
	uint32 default_colour;             ///< Default colour of the land.
};

/** Available colour schemes for height maps. */
static SmallMapColourScheme _heightmap_schemes[] = {
	{NULL, _green_map_heights,      lengthof(_green_map_heights),      MKCOLOUR_XXXX(0x54)}, ///< Green colour scheme.
	{NULL, _dark_green_map_heights, lengthof(_dark_green_map_heights), MKCOLOUR_XXXX(0x62)}, ///< Dark green colour scheme.
	{NULL, _violet_map_heights,     lengthof(_violet_map_heights),     MKCOLOUR_XXXX(0x82)}, ///< Violet colour scheme.
};

/**
 * (Re)build the colour tables for the legends.
 */
void BuildLandLegend()
{
	/* The smallmap window has never been initialized, so no need to change the legend. */
	if (_heightmap_schemes[0].height_colours == NULL) return;

	/*
	 * The general idea of this function is to fill the legend with an appropriate evenly spaced
	 * selection of height levels. All entries with STR_TINY_BLACK_HEIGHT are reserved for this.
	 * At the moment there are twelve of these.
	 *
	 * The table below defines up to which height level a particular delta in the legend should be
	 * used. One could opt for just dividing the maximum height and use that as delta, but that
	 * creates many "ugly" legend labels, e.g. once every 950 meter. As a result, this table will
	 * reduce the number of deltas to 7: every 100m, 200m, 300m, 500m, 750m, 1000m and 1250m. The
	 * deltas are closer together at the lower numbers because going from 12 entries to just 4, as
	 * would happen when replacing 200m and 300m by 250m, would mean the legend would be short and
	 * that might not be considered appropriate.
	 *
	 * The current method yields at least 7 legend entries and at most 12. It can be increased to
	 * 8 by adding a 150m and 400m option, but especially 150m creates ugly heights.
	 *
	 * It tries to evenly space the legend items over the two columns that are there for the legend.
	 */

	/* Table for delta; if max_height is less than the first column, use the second column as value. */
	uint deltas[][2] = { { 24, 2 }, { 48, 4 }, { 72, 6 }, { 120, 10 }, { 180, 15 }, { 240, 20 }, { MAX_TILE_HEIGHT + 1, 25 }};
	uint i = 0;
	for (; _settings_game.construction.max_heightlevel >= deltas[i][0]; i++) {
		/* Nothing to do here. */
	}
	uint delta = deltas[i][1];

	int total_entries = (_settings_game.construction.max_heightlevel / delta) + 1;
	int rows = CeilDiv(total_entries, 2);
	int j = 0;

	for (i = 0; i < lengthof(_legend_land_contours) - 1 && j < total_entries; i++) {
		if (_legend_land_contours[i].legend != STR_TINY_BLACK_HEIGHT) continue;

		_legend_land_contours[i].col_break = j % rows == 0;
		_legend_land_contours[i].end = false;
		_legend_land_contours[i].height = j * delta;
		_legend_land_contours[i].colour = _heightmap_schemes[_settings_client.gui.smallmap_land_colour].height_colours[j * delta];
		j++;
	}
	_legend_land_contours[i].end = true;
}

/**
 * Completes the array for the owned property legend.
 */
void BuildOwnerLegend()
{
	_legend_land_owners[1].colour = _heightmap_schemes[_settings_client.gui.smallmap_land_colour].default_colour;

	int i = NUM_NO_COMPANY_ENTRIES;
	const Company *c;
	FOR_ALL_COMPANIES(c) {
		_legend_land_owners[i].colour = _colour_gradient[c->colour][5];
		_legend_land_owners[i].company = c->index;
		_legend_land_owners[i].show_on_map = true;
		_legend_land_owners[i].col_break = false;
		_legend_land_owners[i].end = false;
		_company_to_list_pos[c->index] = i;
		i++;
	}

	/* Terminate the list */
	_legend_land_owners[i].end = true;

	/* Store maximum amount of owner legend entries. */
	_smallmap_company_count = i;
}

struct AndOr {
	uint32 mor;
	uint32 mand;
};

static inline uint32 ApplyMask(uint32 colour, const AndOr *mask)
{
	return (colour & mask->mand) | mask->mor;
}


/** Colour masks for "Contour" and "Routes" modes. */
static const AndOr _smallmap_contours_andor[] = {
	{MKCOLOUR_0000               , MKCOLOUR_FFFF}, // MP_CLEAR
	{MKCOLOUR_0XX0(PC_GREY      ), MKCOLOUR_F00F}, // MP_RAILWAY
	{MKCOLOUR_0XX0(PC_BLACK     ), MKCOLOUR_F00F}, // MP_ROAD
	{MKCOLOUR_0XX0(PC_DARK_RED  ), MKCOLOUR_F00F}, // MP_HOUSE
	{MKCOLOUR_0000               , MKCOLOUR_FFFF}, // MP_TREES
	{MKCOLOUR_XXXX(PC_LIGHT_BLUE), MKCOLOUR_0000}, // MP_STATION
	{MKCOLOUR_XXXX(PC_WATER     ), MKCOLOUR_0000}, // MP_WATER
	{MKCOLOUR_0000               , MKCOLOUR_FFFF}, // MP_VOID
	{MKCOLOUR_XXXX(PC_DARK_RED  ), MKCOLOUR_0000}, // MP_INDUSTRY
	{MKCOLOUR_0000               , MKCOLOUR_FFFF}, // MP_TUNNELBRIDGE
	{MKCOLOUR_0XX0(PC_DARK_RED  ), MKCOLOUR_F00F}, // MP_OBJECT
	{MKCOLOUR_0XX0(PC_GREY      ), MKCOLOUR_F00F},
};

/** Colour masks for "Vehicles", "Industry", and "Vegetation" modes. */
static const AndOr _smallmap_vehicles_andor[] = {
	{MKCOLOUR_0000               , MKCOLOUR_FFFF}, // MP_CLEAR
	{MKCOLOUR_0XX0(PC_BLACK     ), MKCOLOUR_F00F}, // MP_RAILWAY
	{MKCOLOUR_0XX0(PC_BLACK     ), MKCOLOUR_F00F}, // MP_ROAD
	{MKCOLOUR_0XX0(PC_DARK_RED  ), MKCOLOUR_F00F}, // MP_HOUSE
	{MKCOLOUR_0000               , MKCOLOUR_FFFF}, // MP_TREES
	{MKCOLOUR_0XX0(PC_BLACK     ), MKCOLOUR_F00F}, // MP_STATION
	{MKCOLOUR_XXXX(PC_WATER     ), MKCOLOUR_0000}, // MP_WATER
	{MKCOLOUR_0000               , MKCOLOUR_FFFF}, // MP_VOID
	{MKCOLOUR_XXXX(PC_DARK_RED  ), MKCOLOUR_0000}, // MP_INDUSTRY
	{MKCOLOUR_0000               , MKCOLOUR_FFFF}, // MP_TUNNELBRIDGE
	{MKCOLOUR_0XX0(PC_DARK_RED  ), MKCOLOUR_F00F}, // MP_OBJECT
	{MKCOLOUR_0XX0(PC_BLACK     ), MKCOLOUR_F00F},
};

/** Mapping of tile type to importance of the tile (higher number means more interesting to show). */
static const byte _tiletype_importance[] = {
	2, // MP_CLEAR
	8, // MP_RAILWAY
	7, // MP_ROAD
	5, // MP_HOUSE
	2, // MP_TREES
	9, // MP_STATION
	2, // MP_WATER
	1, // MP_VOID
	6, // MP_INDUSTRY
	8, // MP_TUNNELBRIDGE
	2, // MP_OBJECT
	0,
};


/**
 * Return the colour a tile would be displayed with in the small map in mode "Contour".
 * @param tile The tile of which we would like to get the colour.
 * @param t    Effective tile type of the tile (see #GetTileColours).
 * @return The colour of tile in the small map in mode "Contour"
 */
static inline uint32 GetSmallMapContoursPixels(TileIndex tile, TileType t)
{
	const SmallMapColourScheme *cs = &_heightmap_schemes[_settings_client.gui.smallmap_land_colour];
	return ApplyMask(cs->height_colours[TileHeight(tile)], &_smallmap_contours_andor[t]);
}

/**
 * Return the colour a tile would be displayed with in the small map in mode "Vehicles".
 *
 * @param tile The tile of which we would like to get the colour.
 * @param t    Effective tile type of the tile (see #GetTileColours).
 * @return The colour of tile in the small map in mode "Vehicles"
 */
static inline uint32 GetSmallMapVehiclesPixels(TileIndex tile, TileType t)
{
	const SmallMapColourScheme *cs = &_heightmap_schemes[_settings_client.gui.smallmap_land_colour];
	return ApplyMask(cs->default_colour, &_smallmap_vehicles_andor[t]);
}

/**
 * Return the colour a tile would be displayed with in the small map in mode "Industries".
 *
 * @param tile The tile of which we would like to get the colour.
 * @param t    Effective tile type of the tile (see #GetTileColours).
 * @return The colour of tile in the small map in mode "Industries"
 */
static inline uint32 GetSmallMapIndustriesPixels(TileIndex tile, TileType t)
{
	const SmallMapColourScheme *cs = &_heightmap_schemes[_settings_client.gui.smallmap_land_colour];
	return ApplyMask(_smallmap_show_heightmap ? cs->height_colours[TileHeight(tile)] : cs->default_colour, &_smallmap_vehicles_andor[t]);
}

/**
 * Return the colour a tile would be displayed with in the small map in mode "Routes".
 *
 * @param tile The tile of which we would like to get the colour.
 * @param t    Effective tile type of the tile (see #GetTileColours).
 * @return The colour of tile  in the small map in mode "Routes"
 */
static inline uint32 GetSmallMapRoutesPixels(TileIndex tile, TileType t)
{
	if (t == MP_STATION) {
		switch (GetStationType(tile)) {
			case STATION_RAIL:    return MKCOLOUR_XXXX(PC_VERY_DARK_BROWN);
			case STATION_AIRPORT: return MKCOLOUR_XXXX(PC_RED);
			case STATION_TRUCK:   return MKCOLOUR_XXXX(PC_ORANGE);
			case STATION_BUS:     return MKCOLOUR_XXXX(PC_YELLOW);
			case STATION_DOCK:    return MKCOLOUR_XXXX(PC_LIGHT_BLUE);
			default:              return MKCOLOUR_FFFF;
		}
	} else if (t == MP_RAILWAY) {
		AndOr andor = {
			MKCOLOUR_0XX0(GetRailTypeInfo(GetRailType(tile))->map_colour),
			_smallmap_contours_andor[t].mand
		};

		const SmallMapColourScheme *cs = &_heightmap_schemes[_settings_client.gui.smallmap_land_colour];
		return ApplyMask(cs->default_colour, &andor);
	}

	/* Ground colour */
	const SmallMapColourScheme *cs = &_heightmap_schemes[_settings_client.gui.smallmap_land_colour];
	return ApplyMask(cs->default_colour, &_smallmap_contours_andor[t]);
}

/**
 * Return the colour a tile would be displayed with in the small map in mode "link stats".
 *
 * @param tile The tile of which we would like to get the colour.
 * @param t    Effective tile type of the tile (see #GetTileColours).
 * @return The colour of tile in the small map in mode "link stats"
 */
static inline uint32 GetSmallMapLinkStatsPixels(TileIndex tile, TileType t)
{
	return _smallmap_show_heightmap ? GetSmallMapContoursPixels(tile, t) : GetSmallMapRoutesPixels(tile, t);
}

static const uint32 _vegetation_clear_bits[] = {
	MKCOLOUR_XXXX(PC_GRASS_LAND), ///< full grass
	MKCOLOUR_XXXX(PC_ROUGH_LAND), ///< rough land
	MKCOLOUR_XXXX(PC_GREY),       ///< rocks
	MKCOLOUR_XXXX(PC_FIELDS),     ///< fields
	MKCOLOUR_XXXX(PC_LIGHT_BLUE), ///< snow
	MKCOLOUR_XXXX(PC_ORANGE),     ///< desert
	MKCOLOUR_XXXX(PC_GRASS_LAND), ///< unused
	MKCOLOUR_XXXX(PC_GRASS_LAND), ///< unused
};

/**
 * Return the colour a tile would be displayed with in the smallmap in mode "Vegetation".
 *
 * @param tile The tile of which we would like to get the colour.
 * @param t    Effective tile type of the tile (see #GetTileColours).
 * @return The colour of tile  in the smallmap in mode "Vegetation"
 */
static inline uint32 GetSmallMapVegetationPixels(TileIndex tile, TileType t)
{
	switch (t) {
		case MP_CLEAR:
			return (IsClearGround(tile, CLEAR_GRASS) && GetClearDensity(tile) < 3) ? MKCOLOUR_XXXX(PC_BARE_LAND) : _vegetation_clear_bits[GetClearGround(tile)];

		case MP_INDUSTRY:
			return IsTileForestIndustry(tile) ? MKCOLOUR_XXXX(PC_GREEN) : MKCOLOUR_XXXX(PC_DARK_RED);

		case MP_TREES:
			if (GetTreeGround(tile) == TREE_GROUND_SNOW_DESERT || GetTreeGround(tile) == TREE_GROUND_ROUGH_SNOW) {
				return (_settings_game.game_creation.landscape == LT_ARCTIC) ? MKCOLOUR_XYYX(PC_LIGHT_BLUE, PC_TREES) : MKCOLOUR_XYYX(PC_ORANGE, PC_TREES);
			}
			return MKCOLOUR_XYYX(PC_GRASS_LAND, PC_TREES);

		default:
			return ApplyMask(MKCOLOUR_XXXX(PC_GRASS_LAND), &_smallmap_vehicles_andor[t]);
	}
}

/**
 * Return the colour a tile would be displayed with in the small map in mode "Owner".
 *
 * @param tile The tile of which we would like to get the colour.
 * @param t    Effective tile type of the tile (see #GetTileColours).
 * @return The colour of tile in the small map in mode "Owner"
 */
static inline uint32 GetSmallMapOwnerPixels(TileIndex tile, TileType t)
{
	Owner o;

	switch (t) {
		case MP_INDUSTRY: return MKCOLOUR_XXXX(PC_DARK_GREY);
		case MP_HOUSE:    return MKCOLOUR_XXXX(PC_DARK_RED);
		default:          o = GetTileOwner(tile); break;
		/* FIXME: For MP_ROAD there are multiple owners.
		 * GetTileOwner returns the rail owner (level crossing) resp. the owner of ROADTYPE_ROAD (normal road),
		 * even if there are no ROADTYPE_ROAD bits on the tile.
		 */
	}

	if ((o < MAX_COMPANIES && !_legend_land_owners[_company_to_list_pos[o]].show_on_map) || o == OWNER_NONE || o == OWNER_WATER) {
		if (t == MP_WATER) return MKCOLOUR_XXXX(PC_WATER);
		const SmallMapColourScheme *cs = &_heightmap_schemes[_settings_client.gui.smallmap_land_colour];
		return _smallmap_show_heightmap ? cs->height_colours[TileHeight(tile)] : cs->default_colour;
	} else if (o == OWNER_TOWN) {
		return MKCOLOUR_XXXX(PC_DARK_RED);
	}

	return MKCOLOUR_XXXX(_legend_land_owners[_company_to_list_pos[o]].colour);
}

/** Vehicle colours in #SMT_VEHICLES mode. Indexed by #VehicleTypeByte. */
static const byte _vehicle_type_colours[6] = {
	PC_RED, PC_YELLOW, PC_LIGHT_BLUE, PC_WHITE, PC_BLACK, PC_RED
};


/** Notify the industry chain window to stop sending newly selected industries. */
/* static */ void SmallMapWindow::BreakIndustryChainLink()
{
	InvalidateWindowClassesData(WC_INDUSTRY_CARGOES, NUM_INDUSTRYTYPES);
}

inline Point SmallMapWindow::SmallmapRemapCoords(int x, int y) const
{
	Point pt;
	pt.x = (y - x) * 2;
	pt.y = y + x;
	return pt;
}

/**
 * Remap tile to location on this smallmap.
 * @param tile_x X coordinate of the tile.
 * @param tile_y Y coordinate of the tile.
 * @return Position to draw on.
 */
inline Point SmallMapWindow::RemapTile(int tile_x, int tile_y) const
{
	int x_offset = tile_x - this->scroll_x / (int)TILE_SIZE;
	int y_offset = tile_y - this->scroll_y / (int)TILE_SIZE;

	if (this->zoom == 1) return SmallmapRemapCoords(x_offset, y_offset);

	/* For negative offsets, round towards -inf. */
	if (x_offset < 0) x_offset -= this->zoom - 1;
	if (y_offset < 0) y_offset -= this->zoom - 1;

	return SmallmapRemapCoords(x_offset / this->zoom, y_offset / this->zoom);
}

/**
 * Determine the tile relative to the base tile of the smallmap, and the pixel position at
 * that tile for a point in the smallmap.
 * @param px       Horizontal coordinate of the pixel.
 * @param py       Vertical coordinate of the pixel.
 * @param sub[out] Pixel position at the tile (0..3).
 * @param add_sub  Add current #subscroll to the position.
 * @return Tile being displayed at the given position relative to #scroll_x and #scroll_y.
 * @note The #subscroll offset is already accounted for.
 */
inline Point SmallMapWindow::PixelToTile(int px, int py, int *sub, bool add_sub) const
{
	if (add_sub) px += this->subscroll;  // Total horizontal offset.

	/* For each two rows down, add a x and a y tile, and
	 * For each four pixels to the right, move a tile to the right. */
	Point pt = {((py >> 1) - (px >> 2)) * this->zoom, ((py >> 1) + (px >> 2)) * this->zoom};
	px &= 3;

	if (py & 1) { // Odd number of rows, handle the 2 pixel shift.
		if (px < 2) {
			pt.x += this->zoom;
			px += 2;
		} else {
			pt.y += this->zoom;
			px -= 2;
		}
	}

	*sub = px;
	return pt;
}

/**
 * Compute base parameters of the smallmap such that tile (\a tx, \a ty) starts at pixel (\a x, \a y).
 * @param tx        Tile x coordinate.
 * @param ty        Tile y coordinate.
 * @param x         Non-negative horizontal position in the display where the tile starts.
 * @param y         Non-negative vertical position in the display where the tile starts.
 * @param sub [out] Value of #subscroll needed.
 * @return #scroll_x, #scroll_y values.
 */
Point SmallMapWindow::ComputeScroll(int tx, int ty, int x, int y, int *sub)
{
	assert(x >= 0 && y >= 0);

	int new_sub;
	Point tile_xy = PixelToTile(x, y, &new_sub, false);
	tx -= tile_xy.x;
	ty -= tile_xy.y;

	Point scroll;
	if (new_sub == 0) {
		*sub = 0;
		scroll.x = (tx + this->zoom) * TILE_SIZE;
		scroll.y = (ty - this->zoom) * TILE_SIZE;
	} else {
		*sub = 4 - new_sub;
		scroll.x = (tx + 2 * this->zoom) * TILE_SIZE;
		scroll.y = (ty - 2 * this->zoom) * TILE_SIZE;
	}
	return scroll;
}

/**
 * Initialize or change the zoom level.
 * @param change  Way to change the zoom level.
 * @param zoom_pt Position to keep fixed while zooming.
 * @pre \c *zoom_pt should contain a point in the smallmap display when zooming in or out.
 */
void SmallMapWindow::SetZoomLevel(ZoomLevelChange change, const Point *zoom_pt)
{
	static const int zoomlevels[] = {1, 2, 4, 6, 8}; // Available zoom levels. Bigger number means more zoom-out (further away).
	static const int MIN_ZOOM_INDEX = 0;
	static const int MAX_ZOOM_INDEX = lengthof(zoomlevels) - 1;

	int new_index, cur_index, sub;
	Point tile;
	switch (change) {
		case ZLC_INITIALIZE:
			cur_index = - 1; // Definitely different from new_index.
			new_index = MIN_ZOOM_INDEX;
			tile.x = tile.y = 0;
			break;

		case ZLC_ZOOM_IN:
		case ZLC_ZOOM_OUT:
			for (cur_index = MIN_ZOOM_INDEX; cur_index <= MAX_ZOOM_INDEX; cur_index++) {
				if (this->zoom == zoomlevels[cur_index]) break;
			}
			assert(cur_index <= MAX_ZOOM_INDEX);

			tile = this->PixelToTile(zoom_pt->x, zoom_pt->y, &sub);
			new_index = Clamp(cur_index + ((change == ZLC_ZOOM_IN) ? -1 : 1), MIN_ZOOM_INDEX, MAX_ZOOM_INDEX);
			break;

		default: NOT_REACHED();
	}

	if (new_index != cur_index) {
		this->zoom = zoomlevels[new_index];
		if (cur_index >= 0) {
			Point new_tile = this->PixelToTile(zoom_pt->x, zoom_pt->y, &sub);
			this->SetNewScroll(this->scroll_x + (tile.x - new_tile.x) * TILE_SIZE,
					this->scroll_y + (tile.y - new_tile.y) * TILE_SIZE, sub);
		} else if (this->map_type == SMT_LINKSTATS) {
			this->overlay->RebuildCache();
		}
		this->SetWidgetDisabledState(WID_SM_ZOOM_IN,  this->zoom == zoomlevels[MIN_ZOOM_INDEX]);
		this->SetWidgetDisabledState(WID_SM_ZOOM_OUT, this->zoom == zoomlevels[MAX_ZOOM_INDEX]);
		this->SetDirty();
	}
}

/**
 * Decide which colours to show to the user for a group of tiles.
 * @param ta Tile area to investigate.
 * @return Colours to display.
 */
inline uint32 SmallMapWindow::GetTileColours(const TileArea &ta) const
{
	int importance = 0;
	TileIndex tile = INVALID_TILE; // Position of the most important tile.
	TileType et = MP_VOID;         // Effective tile type at that position.

	TILE_AREA_LOOP(ti, ta) {
		TileType ttype = GetTileType(ti);

		switch (ttype) {
			case MP_TUNNELBRIDGE: {
				TransportType tt = GetTunnelBridgeTransportType(ti);

				switch (tt) {
					case TRANSPORT_RAIL: ttype = MP_RAILWAY; break;
					case TRANSPORT_ROAD: ttype = MP_ROAD;    break;
					default:             ttype = MP_WATER;   break;
				}
				break;
			}

			case MP_INDUSTRY:
				/* Special handling of industries while in "Industries" smallmap view. */
				if (this->map_type == SMT_INDUSTRY) {
					/* If industry is allowed to be seen, use its colour on the map.
					 * This has the highest priority above any value in _tiletype_importance. */
					IndustryType type = Industry::GetByTile(ti)->type;
					if (_legend_from_industries[_industry_to_list_pos[type]].show_on_map) {
						if (type == _smallmap_industry_highlight) {
							if (_smallmap_industry_highlight_state) return MKCOLOUR_XXXX(PC_WHITE);
						} else {
							return GetIndustrySpec(type)->map_colour * 0x01010101;
						}
					}
					/* Otherwise make it disappear */
					ttype = IsTileOnWater(ti) ? MP_WATER : MP_CLEAR;
				}
				break;

			default:
				break;
		}

		if (_tiletype_importance[ttype] > importance) {
			importance = _tiletype_importance[ttype];
			tile = ti;
			et = ttype;
		}
	}

	switch (this->map_type) {
		case SMT_CONTOUR:
			return GetSmallMapContoursPixels(tile, et);

		case SMT_VEHICLES:
			return GetSmallMapVehiclesPixels(tile, et);

		case SMT_INDUSTRY:
			return GetSmallMapIndustriesPixels(tile, et);

		case SMT_LINKSTATS:
			return GetSmallMapLinkStatsPixels(tile, et);

		case SMT_ROUTES:
			return GetSmallMapRoutesPixels(tile, et);

		case SMT_VEGETATION:
			return GetSmallMapVegetationPixels(tile, et);

		case SMT_OWNER:
			return GetSmallMapOwnerPixels(tile, et);

		default: NOT_REACHED();
	}
}

/**
 * Draws one column of tiles of the small map in a certain mode onto the screen buffer, skipping the shifted rows in between.
 *
 * @param dst Pointer to a part of the screen buffer to write to.
 * @param xc The X coordinate of the first tile in the column.
 * @param yc The Y coordinate of the first tile in the column
 * @param pitch Number of pixels to advance in the screen buffer each time a pixel is written.
 * @param reps Number of lines to draw
 * @param start_pos Position of first pixel to draw.
 * @param end_pos Position of last pixel to draw (exclusive).
 * @param blitter current blitter
 * @note If pixel position is below \c 0, skip drawing.
 */
void SmallMapWindow::DrawSmallMapColumn(void *dst, uint xc, uint yc, int pitch, int reps, int start_pos, int end_pos, Blitter *blitter) const
{
	void *dst_ptr_abs_end = blitter->MoveTo(_screen.dst_ptr, 0, _screen.height);
	uint min_xy = _settings_game.construction.freeform_edges ? 1 : 0;

	do {
		/* Check if the tile (xc,yc) is within the map range */
		if (xc >= MapMaxX() || yc >= MapMaxY()) continue;

		/* Check if the dst pointer points to a pixel inside the screen buffer */
		if (dst < _screen.dst_ptr) continue;
		if (dst >= dst_ptr_abs_end) continue;

		/* Construct tilearea covered by (xc, yc, xc + this->zoom, yc + this->zoom) such that it is within min_xy limits. */
		TileArea ta;
		if (min_xy == 1 && (xc == 0 || yc == 0)) {
			if (this->zoom == 1) continue; // The tile area is empty, don't draw anything.

			ta = TileArea(TileXY(max(min_xy, xc), max(min_xy, yc)), this->zoom - (xc == 0), this->zoom - (yc == 0));
		} else {
			ta = TileArea(TileXY(xc, yc), this->zoom, this->zoom);
		}
		ta.ClampToMap(); // Clamp to map boundaries (may contain MP_VOID tiles!).

		uint32 val = this->GetTileColours(ta);
		uint8 *val8 = (uint8 *)&val;
		int idx = max(0, -start_pos);
		for (int pos = max(0, start_pos); pos < end_pos; pos++) {
			blitter->SetPixel(dst, idx, 0, val8[idx]);
			idx++;
		}
	/* Switch to next tile in the column */
	} while (xc += this->zoom, yc += this->zoom, dst = blitter->MoveTo(dst, pitch, 0), --reps != 0);
}

/**
 * Adds vehicles to the smallmap.
 * @param dpi the part of the smallmap to be drawn into
 * @param blitter current blitter
 */
void SmallMapWindow::DrawVehicles(const DrawPixelInfo *dpi, Blitter *blitter) const
{
	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_EFFECT) continue;
		if (v->vehstatus & (VS_HIDDEN | VS_UNCLICKABLE)) continue;

		/* Remap into flat coordinates. */
		Point pt = this->RemapTile(v->x_pos / (int)TILE_SIZE, v->y_pos / (int)TILE_SIZE);

		int y = pt.y - dpi->top;
		if (!IsInsideMM(y, 0, dpi->height)) continue; // y is out of bounds.

		bool skip = false; // Default is to draw both pixels.
		int x = pt.x - this->subscroll - 3 - dpi->left; // Offset X coordinate.
		if (x < 0) {
			/* if x+1 is 0, that means we're on the very left edge,
			 * and should thus only draw a single pixel */
			if (++x != 0) continue;
			skip = true;
		} else if (x >= dpi->width - 1) {
			/* Check if we're at the very right edge, and if so draw only a single pixel */
			if (x != dpi->width - 1) continue;
			skip = true;
		}

		/* Calculate pointer to pixel and the colour */
		byte colour = (this->map_type == SMT_VEHICLES) ? _vehicle_type_colours[v->type] : PC_WHITE;

		/* And draw either one or two pixels depending on clipping */
		blitter->SetPixel(dpi->dst_ptr, x, y, colour);
		if (!skip) blitter->SetPixel(dpi->dst_ptr, x + 1, y, colour);
	}
}

/**
 * Adds town names to the smallmap.
 * @param dpi the part of the smallmap to be drawn into
 */
void SmallMapWindow::DrawTowns(const DrawPixelInfo *dpi) const
{
	const Town *t;
	FOR_ALL_TOWNS(t) {
		/* Remap the town coordinate */
		Point pt = this->RemapTile(TileX(t->xy), TileY(t->xy));
		int x = pt.x - this->subscroll - (t->cache.sign.width_small >> 1);
		int y = pt.y;

		/* Check if the town sign is within bounds */
		if (x + t->cache.sign.width_small > dpi->left &&
				x < dpi->left + dpi->width &&
				y + FONT_HEIGHT_SMALL > dpi->top &&
				y < dpi->top + dpi->height) {
			/* And draw it. */
			SetDParam(0, t->index);
			DrawString(x, x + t->cache.sign.width_small, y, STR_SMALLMAP_TOWN);
		}
	}
}

/**
 * Adds map indicators to the smallmap.
 */
void SmallMapWindow::DrawMapIndicators() const
{
	/* Find main viewport. */
	const ViewPort *vp = FindWindowById(WC_MAIN_WINDOW, 0)->viewport;

	Point upper_left_smallmap_coord  = TranslateXYToTileCoord(vp, vp->left, vp->top, false);
	Point lower_right_smallmap_coord = TranslateXYToTileCoord(vp, vp->left + vp->width - 1, vp->top + vp->height - 1, false);

	Point upper_left = this->RemapTile(upper_left_smallmap_coord.x / (int)TILE_SIZE, upper_left_smallmap_coord.y / (int)TILE_SIZE);
	upper_left.x -= this->subscroll;

	Point lower_right = this->RemapTile(lower_right_smallmap_coord.x / (int)TILE_SIZE, lower_right_smallmap_coord.y / (int)TILE_SIZE);
	lower_right.x -= this->subscroll;

	SmallMapWindow::DrawVertMapIndicator(upper_left.x, upper_left.y, lower_right.y);
	SmallMapWindow::DrawVertMapIndicator(lower_right.x, upper_left.y, lower_right.y);

	SmallMapWindow::DrawHorizMapIndicator(upper_left.x, lower_right.x, upper_left.y);
	SmallMapWindow::DrawHorizMapIndicator(upper_left.x, lower_right.x, lower_right.y);
}

/**
 * Draws the small map.
 *
 * Basically, the small map is draw column of pixels by column of pixels. The pixels
 * are drawn directly into the screen buffer. The final map is drawn in multiple passes.
 * The passes are:
 * <ol><li>The colours of tiles in the different modes.</li>
 * <li>Town names (optional)</li></ol>
 *
 * @param dpi pointer to pixel to write onto
 */
void SmallMapWindow::DrawSmallMap(DrawPixelInfo *dpi) const
{
	Blitter *blitter = BlitterFactory::GetCurrentBlitter();
	DrawPixelInfo *old_dpi;

	old_dpi = _cur_dpi;
	_cur_dpi = dpi;

	/* Clear it */
	GfxFillRect(dpi->left, dpi->top, dpi->left + dpi->width - 1, dpi->top + dpi->height - 1, PC_BLACK);

	/* Which tile is displayed at (dpi->left, dpi->top)? */
	int dx;
	Point tile = this->PixelToTile(dpi->left, dpi->top, &dx);
	int tile_x = this->scroll_x / (int)TILE_SIZE + tile.x;
	int tile_y = this->scroll_y / (int)TILE_SIZE + tile.y;

	void *ptr = blitter->MoveTo(dpi->dst_ptr, -dx - 4, 0);
	int x = - dx - 4;
	int y = 0;

	for (;;) {
		/* Distance from left edge */
		if (x >= -3) {
			if (x >= dpi->width) break; // Exit the loop.

			int end_pos = min(dpi->width, x + 4);
			int reps = (dpi->height - y + 1) / 2; // Number of lines.
			if (reps > 0) {
				this->DrawSmallMapColumn(ptr, tile_x, tile_y, dpi->pitch * 2, reps, x, end_pos, blitter);
			}
		}

		if (y == 0) {
			tile_y += this->zoom;
			y++;
			ptr = blitter->MoveTo(ptr, 0, 1);
		} else {
			tile_x -= this->zoom;
			y--;
			ptr = blitter->MoveTo(ptr, 0, -1);
		}
		ptr = blitter->MoveTo(ptr, 2, 0);
		x += 2;
	}

	/* Draw vehicles */
	if (this->map_type == SMT_CONTOUR || this->map_type == SMT_VEHICLES) this->DrawVehicles(dpi, blitter);

	/* Draw link stat overlay */
	if (this->map_type == SMT_LINKSTATS) this->overlay->Draw(dpi);

	/* Draw town names */
	if (this->show_towns) this->DrawTowns(dpi);

	/* Draw map indicators */
	this->DrawMapIndicators();

	_cur_dpi = old_dpi;
}

/**
 * Function to set up widgets depending on the information being shown on the smallmap.
 */
void SmallMapWindow::SetupWidgetData()
{
	StringID legend_tooltip;
	StringID enable_all_tooltip;
	StringID disable_all_tooltip;
	int plane;
	switch (this->map_type) {
		case SMT_INDUSTRY:
			legend_tooltip = STR_SMALLMAP_TOOLTIP_INDUSTRY_SELECTION;
			enable_all_tooltip = STR_SMALLMAP_TOOLTIP_ENABLE_ALL_INDUSTRIES;
			disable_all_tooltip = STR_SMALLMAP_TOOLTIP_DISABLE_ALL_INDUSTRIES;
			plane = 0;
			break;

		case SMT_OWNER:
			legend_tooltip = STR_SMALLMAP_TOOLTIP_COMPANY_SELECTION;
			enable_all_tooltip = STR_SMALLMAP_TOOLTIP_ENABLE_ALL_COMPANIES;
			disable_all_tooltip = STR_SMALLMAP_TOOLTIP_DISABLE_ALL_COMPANIES;
			plane = 0;
			break;

		case SMT_LINKSTATS:
			legend_tooltip = STR_SMALLMAP_TOOLTIP_CARGO_SELECTION;
			enable_all_tooltip = STR_SMALLMAP_TOOLTIP_ENABLE_ALL_CARGOS;
			disable_all_tooltip = STR_SMALLMAP_TOOLTIP_DISABLE_ALL_CARGOS;
			plane = 0;
			break;

		default:
			legend_tooltip = STR_NULL;
			enable_all_tooltip = STR_NULL;
			disable_all_tooltip = STR_NULL;
			plane = 1;
			break;
	}

	this->GetWidget<NWidgetCore>(WID_SM_LEGEND)->SetDataTip(STR_NULL, legend_tooltip);
	this->GetWidget<NWidgetCore>(WID_SM_ENABLE_ALL)->SetDataTip(STR_SMALLMAP_ENABLE_ALL, enable_all_tooltip);
	this->GetWidget<NWidgetCore>(WID_SM_DISABLE_ALL)->SetDataTip(STR_SMALLMAP_DISABLE_ALL, disable_all_tooltip);
	this->GetWidget<NWidgetStacked>(WID_SM_SELECT_BUTTONS)->SetDisplayedPlane(plane);
}

SmallMapWindow::SmallMapWindow(WindowDesc *desc, int window_number) : Window(desc), refresh(FORCE_REFRESH_PERIOD)
{
	_smallmap_industry_highlight = INVALID_INDUSTRYTYPE;
	this->overlay = new LinkGraphOverlay(this, WID_SM_MAP, 0, this->GetOverlayCompanyMask(), 1);
	this->InitNested(window_number);
	this->LowerWidget(this->map_type + WID_SM_CONTOUR);

	this->RebuildColourIndexIfNecessary();

	this->SetWidgetLoweredState(WID_SM_SHOW_HEIGHT, _smallmap_show_heightmap);

	this->SetWidgetLoweredState(WID_SM_TOGGLETOWNNAME, this->show_towns);

	this->SetupWidgetData();

	this->SetZoomLevel(ZLC_INITIALIZE, NULL);
	this->SmallMapCenterOnCurrentPos();
	this->SetOverlayCargoMask();
}

SmallMapWindow::~SmallMapWindow()
{
	delete this->overlay;
	this->BreakIndustryChainLink();
}

/**
 * Rebuilds the colour indices used for fast access to the smallmap contour colours based on the heightlevel.
 */
void SmallMapWindow::RebuildColourIndexIfNecessary()
{
	/* Rebuild colour indices if necessary. */
	if (SmallMapWindow::max_heightlevel == _settings_game.construction.max_heightlevel) return;

	for (uint n = 0; n < lengthof(_heightmap_schemes); n++) {
		/* The heights go from 0 up to and including maximum. */
		int heights = _settings_game.construction.max_heightlevel + 1;
		_heightmap_schemes[n].height_colours = ReallocT<uint32>(_heightmap_schemes[n].height_colours, heights);

		for (int z = 0; z < heights; z++) {
			size_t access_index = (_heightmap_schemes[n].colour_count * z) / heights;

			/* Choose colour by mapping the range (0..max heightlevel) on the complete colour table. */
			_heightmap_schemes[n].height_colours[z] = _heightmap_schemes[n].height_colours_base[access_index];
		}
	}

	SmallMapWindow::max_heightlevel = _settings_game.construction.max_heightlevel;
	BuildLandLegend();
}

/* virtual */ void SmallMapWindow::SetStringParameters(int widget) const
{
	switch (widget) {
		case WID_SM_CAPTION:
			SetDParam(0, STR_SMALLMAP_TYPE_CONTOURS + this->map_type);
			break;
	}
}

/* virtual */ void SmallMapWindow::OnInit()
{
	uint min_width = 0;
	this->min_number_of_columns = INDUSTRY_MIN_NUMBER_OF_COLUMNS;
	this->min_number_of_fixed_rows = lengthof(_linkstat_colours_in_legenda);
	for (uint i = 0; i < lengthof(_legend_table); i++) {
		uint height = 0;
		uint num_columns = 1;
		for (const LegendAndColour *tbl = _legend_table[i]; !tbl->end; ++tbl) {
			StringID str;
			if (i == SMT_INDUSTRY) {
				SetDParam(0, tbl->legend);
				SetDParam(1, IndustryPool::MAX_SIZE);
				str = STR_SMALLMAP_INDUSTRY;
			} else if (i == SMT_LINKSTATS) {
				SetDParam(0, tbl->legend);
				str = STR_SMALLMAP_LINKSTATS;
			} else if (i == SMT_OWNER) {
				if (tbl->company != INVALID_COMPANY) {
					if (!Company::IsValidID(tbl->company)) {
						/* Rebuild the owner legend. */
						BuildOwnerLegend();
						this->OnInit();
						return;
					}
					/* Non-fixed legend entries for the owner view. */
					SetDParam(0, tbl->company);
					str = STR_SMALLMAP_COMPANY;
				} else {
					str = tbl->legend;
				}
			} else {
				if (tbl->col_break) {
					this->min_number_of_fixed_rows = max(this->min_number_of_fixed_rows, height);
					height = 0;
					num_columns++;
				}
				height++;
				str = tbl->legend;
			}
			min_width = max(GetStringBoundingBox(str).width, min_width);
		}
		this->min_number_of_fixed_rows = max(this->min_number_of_fixed_rows, height);
		this->min_number_of_columns = max(this->min_number_of_columns, num_columns);
	}

	/* The width of a column is the minimum width of all texts + the size of the blob + some spacing */
	this->column_width = min_width + LEGEND_BLOB_WIDTH + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
}

/* virtual */ void SmallMapWindow::OnPaint()
{
	if (this->map_type == SMT_OWNER) {
		for (const LegendAndColour *tbl = _legend_table[this->map_type]; !tbl->end; ++tbl) {
			if (tbl->company != INVALID_COMPANY && !Company::IsValidID(tbl->company)) {
				/* Rebuild the owner legend. */
				BuildOwnerLegend();
				this->InvalidateData(1);
				break;
			}
		}
	}

	this->DrawWidgets();
}

/* virtual */ void SmallMapWindow::DrawWidget(const Rect &r, int widget) const
{
	switch (widget) {
		case WID_SM_MAP: {
			DrawPixelInfo new_dpi;
			if (!FillDrawPixelInfo(&new_dpi, r.left + 1, r.top + 1, r.right - r.left - 1, r.bottom - r.top - 1)) return;
			this->DrawSmallMap(&new_dpi);
			break;
		}

		case WID_SM_LEGEND: {
			uint columns = this->GetNumberColumnsLegend(r.right - r.left + 1);
			uint number_of_rows = this->GetNumberRowsLegend(columns);
			bool rtl = _current_text_dir == TD_RTL;
			uint y_org = r.top + WD_FRAMERECT_TOP;
			uint x = rtl ? r.right - this->column_width - WD_FRAMERECT_RIGHT : r.left + WD_FRAMERECT_LEFT;
			uint y = y_org;
			uint i = 0; // Row counter for industry legend.
			uint row_height = FONT_HEIGHT_SMALL;

			uint text_left  = rtl ? 0 : LEGEND_BLOB_WIDTH + WD_FRAMERECT_LEFT;
			uint text_right = this->column_width - 1 - (rtl ? LEGEND_BLOB_WIDTH + WD_FRAMERECT_RIGHT : 0);
			uint blob_left  = rtl ? this->column_width - 1 - LEGEND_BLOB_WIDTH : 0;
			uint blob_right = rtl ? this->column_width - 1 : LEGEND_BLOB_WIDTH;

			StringID string = STR_NULL;
			switch (this->map_type) {
				case SMT_INDUSTRY:
					string = STR_SMALLMAP_INDUSTRY;
					break;
				case SMT_LINKSTATS:
					string = STR_SMALLMAP_LINKSTATS;
					break;
				case SMT_OWNER:
					string = STR_SMALLMAP_COMPANY;
					break;
				default:
					break;
			}

			for (const LegendAndColour *tbl = _legend_table[this->map_type]; !tbl->end; ++tbl) {
				if (tbl->col_break || ((this->map_type == SMT_INDUSTRY || this->map_type == SMT_OWNER || this->map_type == SMT_LINKSTATS) && i++ >= number_of_rows)) {
					/* Column break needed, continue at top, COLUMN_WIDTH pixels
					 * (one "row") to the right. */
					x += rtl ? -(int)this->column_width : this->column_width;
					y = y_org;
					i = 1;
				}

				uint8 legend_colour = tbl->colour;

				switch (this->map_type) {
					case SMT_INDUSTRY:
						/* Industry name must be formatted, since it's not in tiny font in the specs.
						 * So, draw with a parameter and use the STR_SMALLMAP_INDUSTRY string, which is tiny font */
						SetDParam(0, tbl->legend);
						SetDParam(1, Industry::GetIndustryTypeCount(tbl->type));
						if (tbl->show_on_map && tbl->type == _smallmap_industry_highlight) {
							legend_colour = _smallmap_industry_highlight_state ? PC_WHITE : PC_BLACK;
						}
						FALLTHROUGH;

					case SMT_LINKSTATS:
						SetDParam(0, tbl->legend);
						FALLTHROUGH;

					case SMT_OWNER:
						if (this->map_type != SMT_OWNER || tbl->company != INVALID_COMPANY) {
							if (this->map_type == SMT_OWNER) SetDParam(0, tbl->company);
							if (!tbl->show_on_map) {
								/* Simply draw the string, not the black border of the legend colour.
								 * This will enforce the idea of the disabled item */
								DrawString(x + text_left, x + text_right, y, string, TC_GREY);
							} else {
								DrawString(x + text_left, x + text_right, y, string, TC_BLACK);
								GfxFillRect(x + blob_left, y + 1, x + blob_right, y + row_height - 1, PC_BLACK); // Outer border of the legend colour
							}
							break;
						}
						FALLTHROUGH;

					default:
						if (this->map_type == SMT_CONTOUR) SetDParam(0, tbl->height * TILE_HEIGHT_STEP);
						/* Anything that is not an industry or a company is using normal process */
						GfxFillRect(x + blob_left, y + 1, x + blob_right, y + row_height - 1, PC_BLACK);
						DrawString(x + text_left, x + text_right, y, tbl->legend);
						break;
				}
				GfxFillRect(x + blob_left + 1, y + 2, x + blob_right - 1, y + row_height - 2, legend_colour); // Legend colour

				y += row_height;
			}
		}
	}
}

/**
 * Select a new map type.
 * @param map_type New map type.
 */
void SmallMapWindow::SwitchMapType(SmallMapType map_type)
{
	this->RaiseWidget(this->map_type + WID_SM_CONTOUR);
	this->map_type = map_type;
	this->LowerWidget(this->map_type + WID_SM_CONTOUR);

	this->SetupWidgetData();

	if (map_type == SMT_LINKSTATS) this->overlay->RebuildCache();
	if (map_type != SMT_INDUSTRY) this->BreakIndustryChainLink();
	this->SetDirty();
}

/**
 * Get the number of rows in the legend from the number of columns. Those
 * are at least min_number_of_fixed_rows and possibly more if there are so
 * many cargoes, industry types or companies that they won't fit in the
 * available space.
 * @param columns Number of columns in the legend.
 * @return Number of rows needed for everything to fit in.
 */
inline uint SmallMapWindow::GetNumberRowsLegend(uint columns) const
{
	/* Reserve one column for link colours */
	uint num_rows_linkstats = CeilDiv(_smallmap_cargo_count, columns - 1);
	uint num_rows_others = CeilDiv(max(_smallmap_industry_count, _smallmap_company_count), columns);
	return max(this->min_number_of_fixed_rows, max(num_rows_linkstats, num_rows_others));
}

/**
 * Select and toggle a legend item. When CTRL is pressed, disable all other
 * items in the group defined by begin_legend_item and end_legend_item and
 * keep the clicked one enabled even if it was already enabled before. If
 * the other items in the group are all disabled already and CTRL is pressed
 * enable them instead.
 * @param click_pos the index of the item being selected
 * @param legend the legend from which we select
 * @param end_legend_item index one past the last item in the group to be inverted
 * @param begin_legend_item index of the first item in the group to be inverted
 */
void SmallMapWindow::SelectLegendItem(int click_pos, LegendAndColour *legend, int end_legend_item, int begin_legend_item)
{
	if (_ctrl_pressed) {
		/* Disable all, except the clicked one */
		bool changes = false;
		for (int i = begin_legend_item; i != end_legend_item; i++) {
			bool new_state = (i == click_pos);
			if (legend[i].show_on_map != new_state) {
				changes = true;
				legend[i].show_on_map = new_state;
			}
		}
		if (!changes) {
			/* Nothing changed? Then show all (again). */
			for (int i = begin_legend_item; i != end_legend_item; i++) {
				legend[i].show_on_map = true;
			}
		}
	} else {
		legend[click_pos].show_on_map = !legend[click_pos].show_on_map;
	}

	if (this->map_type == SMT_INDUSTRY) this->BreakIndustryChainLink();
}

/**
 * Set the link graph overlay cargo mask from the legend.
 */
void SmallMapWindow::SetOverlayCargoMask()
{
	CargoTypes cargo_mask = 0;
	for (int i = 0; i != _smallmap_cargo_count; ++i) {
		if (_legend_linkstats[i].show_on_map) SetBit(cargo_mask, _legend_linkstats[i].type);
	}
	this->overlay->SetCargoMask(cargo_mask);
}

/**
 * Determines the mouse position on the legend.
 * @param pt Mouse position.
 * @return Legend item under the mouse.
 */
int SmallMapWindow::GetPositionOnLegend(Point pt)
{
	const NWidgetBase *wi = this->GetWidget<NWidgetBase>(WID_SM_LEGEND);
	uint line = (pt.y - wi->pos_y - WD_FRAMERECT_TOP) / FONT_HEIGHT_SMALL;
	uint columns = this->GetNumberColumnsLegend(wi->current_x);
	uint number_of_rows = this->GetNumberRowsLegend(columns);
	if (line >= number_of_rows) return -1;

	bool rtl = _current_text_dir == TD_RTL;
	int x = pt.x - wi->pos_x;
	if (rtl) x = wi->current_x - x;
	uint column = (x - WD_FRAMERECT_LEFT) / this->column_width;

	return (column * number_of_rows) + line;
}

/* virtual */ void SmallMapWindow::OnMouseOver(Point pt, int widget)
{
	IndustryType new_highlight = INVALID_INDUSTRYTYPE;
	if (widget == WID_SM_LEGEND && this->map_type == SMT_INDUSTRY) {
		int industry_pos = GetPositionOnLegend(pt);
		if (industry_pos >= 0 && industry_pos < _smallmap_industry_count) {
			new_highlight = _legend_from_industries[industry_pos].type;
		}
	}
	if (new_highlight != _smallmap_industry_highlight) {
		_smallmap_industry_highlight = new_highlight;
		this->refresh = _smallmap_industry_highlight != INVALID_INDUSTRYTYPE ? BLINK_PERIOD : FORCE_REFRESH_PERIOD;
		_smallmap_industry_highlight_state = true;
		this->SetDirty();
	}
}

/* virtual */ void SmallMapWindow::OnClick(Point pt, int widget, int click_count)
{
	switch (widget) {
		case WID_SM_MAP: { // Map window
			/*
			 * XXX: scrolling with the left mouse button is done by subsequently
			 * clicking with the left mouse button; clicking once centers the
			 * large map at the selected point. So by unclicking the left mouse
			 * button here, it gets reclicked during the next inputloop, which
			 * would make it look like the mouse is being dragged, while it is
			 * actually being (virtually) clicked every inputloop.
			 */
			_left_button_clicked = false;

			const NWidgetBase *wid = this->GetWidget<NWidgetBase>(WID_SM_MAP);
			Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
			int sub;
			pt = this->PixelToTile(pt.x - wid->pos_x, pt.y - wid->pos_y, &sub);
			ScrollWindowTo(this->scroll_x + pt.x * TILE_SIZE, this->scroll_y + pt.y * TILE_SIZE, -1, w);

			this->SetDirty();
			break;
		}

		case WID_SM_ZOOM_IN:
		case WID_SM_ZOOM_OUT: {
			const NWidgetBase *wid = this->GetWidget<NWidgetBase>(WID_SM_MAP);
			Point pt = { (int)wid->current_x / 2, (int)wid->current_y / 2};
			this->SetZoomLevel((widget == WID_SM_ZOOM_IN) ? ZLC_ZOOM_IN : ZLC_ZOOM_OUT, &pt);
			if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
			break;
		}

		case WID_SM_CONTOUR:    // Show land contours
		case WID_SM_VEHICLES:   // Show vehicles
		case WID_SM_INDUSTRIES: // Show industries
		case WID_SM_LINKSTATS:  // Show route map
		case WID_SM_ROUTES:     // Show transport routes
		case WID_SM_VEGETATION: // Show vegetation
		case WID_SM_OWNERS:     // Show land owners
			this->SwitchMapType((SmallMapType)(widget - WID_SM_CONTOUR));
			if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
			break;

		case WID_SM_CENTERMAP: // Center the smallmap again
			this->SmallMapCenterOnCurrentPos();
			this->HandleButtonClick(WID_SM_CENTERMAP);
			if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
			break;

		case WID_SM_TOGGLETOWNNAME: // Toggle town names
			this->show_towns = !this->show_towns;
			this->SetWidgetLoweredState(WID_SM_TOGGLETOWNNAME, this->show_towns);

			this->SetDirty();
			if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
			break;

		case WID_SM_LEGEND: // Legend
			if (this->map_type == SMT_INDUSTRY || this->map_type == SMT_LINKSTATS || this->map_type == SMT_OWNER) {
				int click_pos = this->GetPositionOnLegend(pt);
				if (click_pos < 0) break;

				/* If industry type small map*/
				if (this->map_type == SMT_INDUSTRY) {
					/* If click on industries label, find right industry type and enable/disable it. */
					if (click_pos < _smallmap_industry_count) {
						this->SelectLegendItem(click_pos, _legend_from_industries, _smallmap_industry_count);
					}
				} else if (this->map_type == SMT_LINKSTATS) {
					if (click_pos < _smallmap_cargo_count) {
						this->SelectLegendItem(click_pos, _legend_linkstats, _smallmap_cargo_count);
						this->SetOverlayCargoMask();
					}
				} else if (this->map_type == SMT_OWNER) {
					if (click_pos < _smallmap_company_count) {
						this->SelectLegendItem(click_pos, _legend_land_owners, _smallmap_company_count, NUM_NO_COMPANY_ENTRIES);
					}
				}
				this->SetDirty();
			}
			break;

		case WID_SM_ENABLE_ALL:
		case WID_SM_DISABLE_ALL: {
			LegendAndColour *tbl = NULL;
			switch (this->map_type) {
				case SMT_INDUSTRY:
					tbl = _legend_from_industries;
					this->BreakIndustryChainLink();
					break;
				case SMT_OWNER:
					tbl = &(_legend_land_owners[NUM_NO_COMPANY_ENTRIES]);
					break;
				case SMT_LINKSTATS:
					tbl = _legend_linkstats;
					break;
				default:
					NOT_REACHED();
			}
			for (;!tbl->end && tbl->legend != STR_LINKGRAPH_LEGEND_UNUSED; ++tbl) {
				tbl->show_on_map = (widget == WID_SM_ENABLE_ALL);
			}
			if (this->map_type == SMT_LINKSTATS) this->SetOverlayCargoMask();
			this->SetDirty();
			break;
		}

		case WID_SM_SHOW_HEIGHT: // Enable/disable showing of heightmap.
			_smallmap_show_heightmap = !_smallmap_show_heightmap;
			this->SetWidgetLoweredState(WID_SM_SHOW_HEIGHT, _smallmap_show_heightmap);
			this->SetDirty();
			break;
	}
}

/**
 * Some data on this window has become invalid.
 * @param data Information about the changed data.
 * - data = 0: Displayed industries at the industry chain window have changed.
 * - data = 1: Companies have changed.
 * - data = 2: Cheat changing the maximum heightlevel has been used, rebuild our heightlevel-to-colour index
 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
 */
/* virtual */ void SmallMapWindow::OnInvalidateData(int data, bool gui_scope)
{
	if (!gui_scope) return;

	switch (data) {
		case 1:
			/* The owner legend has already been rebuilt. */
			this->ReInit();
			break;

		case 0: {
			extern std::bitset<NUM_INDUSTRYTYPES> _displayed_industries;
			if (this->map_type != SMT_INDUSTRY) this->SwitchMapType(SMT_INDUSTRY);

			for (int i = 0; i != _smallmap_industry_count; i++) {
				_legend_from_industries[i].show_on_map = _displayed_industries.test(_legend_from_industries[i].type);
			}
			break;
		}

		case 2:
			this->RebuildColourIndexIfNecessary();
			break;

		default: NOT_REACHED();
	}
	this->SetDirty();
}

/* virtual */ bool SmallMapWindow::OnRightClick(Point pt, int widget)
{
	if (widget != WID_SM_MAP || _scrolling_viewport) return false;

	_scrolling_viewport = true;
	return true;
}

/* virtual */ void SmallMapWindow::OnMouseWheel(int wheel)
{
	if (_settings_client.gui.scrollwheel_scrolling != 2) {
		const NWidgetBase *wid = this->GetWidget<NWidgetBase>(WID_SM_MAP);
		int cursor_x = _cursor.pos.x - this->left - wid->pos_x;
		int cursor_y = _cursor.pos.y - this->top  - wid->pos_y;
		if (IsInsideMM(cursor_x, 0, wid->current_x) && IsInsideMM(cursor_y, 0, wid->current_y)) {
			Point pt = {cursor_x, cursor_y};
			this->SetZoomLevel((wheel < 0) ? ZLC_ZOOM_IN : ZLC_ZOOM_OUT, &pt);
		}
	}
}

/* virtual */ void SmallMapWindow::OnTick()
{
	/* Update the window every now and then */
	if (--this->refresh != 0) return;

	if (this->map_type == SMT_LINKSTATS) {
		uint32 company_mask = this->GetOverlayCompanyMask();
		if (this->overlay->GetCompanyMask() != company_mask) {
			this->overlay->SetCompanyMask(company_mask);
		} else {
			this->overlay->RebuildCache();
		}
	}
	_smallmap_industry_highlight_state = !_smallmap_industry_highlight_state;

	this->refresh = _smallmap_industry_highlight != INVALID_INDUSTRYTYPE ? BLINK_PERIOD : FORCE_REFRESH_PERIOD;
	this->SetDirty();
}

/**
 * Set new #scroll_x, #scroll_y, and #subscroll values after limiting them such that the center
 * of the smallmap always contains a part of the map.
 * @param sx  Proposed new #scroll_x
 * @param sy  Proposed new #scroll_y
 * @param sub Proposed new #subscroll
 */
void SmallMapWindow::SetNewScroll(int sx, int sy, int sub)
{
	const NWidgetBase *wi = this->GetWidget<NWidgetBase>(WID_SM_MAP);
	Point hv = InverseRemapCoords(wi->current_x * ZOOM_LVL_BASE * TILE_SIZE / 2, wi->current_y * ZOOM_LVL_BASE * TILE_SIZE / 2);
	hv.x *= this->zoom;
	hv.y *= this->zoom;

	if (sx < -hv.x) {
		sx = -hv.x;
		sub = 0;
	}
	if (sx > (int)(MapMaxX() * TILE_SIZE) - hv.x) {
		sx = MapMaxX() * TILE_SIZE - hv.x;
		sub = 0;
	}
	if (sy < -hv.y) {
		sy = -hv.y;
		sub = 0;
	}
	if (sy > (int)(MapMaxY() * TILE_SIZE) - hv.y) {
		sy = MapMaxY() * TILE_SIZE - hv.y;
		sub = 0;
	}

	this->scroll_x = sx;
	this->scroll_y = sy;
	this->subscroll = sub;
	if (this->map_type == SMT_LINKSTATS) this->overlay->RebuildCache();
}

/* virtual */ void SmallMapWindow::OnScroll(Point delta)
{
	if (_settings_client.gui.scroll_mode == VSM_VIEWPORT_RMB_FIXED || _settings_client.gui.scroll_mode == VSM_MAP_RMB_FIXED) _cursor.fix_at = true;

	/* While tile is at (delta.x, delta.y)? */
	int sub;
	Point pt = this->PixelToTile(delta.x, delta.y, &sub);
	this->SetNewScroll(this->scroll_x + pt.x * TILE_SIZE, this->scroll_y + pt.y * TILE_SIZE, sub);

	this->SetDirty();
}

/**
 * Center the small map on the current center of the viewport.
 */
void SmallMapWindow::SmallMapCenterOnCurrentPos()
{
	const ViewPort *vp = FindWindowById(WC_MAIN_WINDOW, 0)->viewport;
	Point viewport_center = TranslateXYToTileCoord(vp, vp->left + vp->width / 2, vp->top + vp->height / 2);

	int sub;
	const NWidgetBase *wid = this->GetWidget<NWidgetBase>(WID_SM_MAP);
	Point sxy = this->ComputeScroll(viewport_center.x / (int)TILE_SIZE, viewport_center.y / (int)TILE_SIZE,
			max(0, (int)wid->current_x / 2 - 2), wid->current_y / 2, &sub);
	this->SetNewScroll(sxy.x, sxy.y, sub);
	this->SetDirty();
}

/**
 * Get the center of the given station as point on the screen in the smallmap window.
 * @param st Station to find in the smallmap.
 * @return Point with coordinates of the station.
 */
Point SmallMapWindow::GetStationMiddle(const Station *st) const
{
	int x = (st->rect.right + st->rect.left + 1) / 2;
	int y = (st->rect.bottom + st->rect.top + 1) / 2;
	Point ret = this->RemapTile(x, y);

	/* Same magic 3 as in DrawVehicles; that's where I got it from.
	 * No idea what it is, but without it the result looks bad.
	 */
	ret.x -= 3 + this->subscroll;
	return ret;
}

SmallMapWindow::SmallMapType SmallMapWindow::map_type = SMT_CONTOUR;
bool SmallMapWindow::show_towns = true;
int SmallMapWindow::max_heightlevel = -1;

/**
 * Custom container class for displaying smallmap with a vertically resizing legend panel.
 * The legend panel has a smallest height that depends on its width. Standard containers cannot handle this case.
 *
 * @note The container assumes it has two children, the first is the display, the second is the bar with legends and selection image buttons.
 *       Both children should be both horizontally and vertically resizable and horizontally fillable.
 *       The bar should have a minimal size with a zero-size legends display. Child padding is not supported.
 */
class NWidgetSmallmapDisplay : public NWidgetContainer {
	const SmallMapWindow *smallmap_window; ///< Window manager instance.
public:
	NWidgetSmallmapDisplay() : NWidgetContainer(NWID_VERTICAL)
	{
		this->smallmap_window = NULL;
	}

	virtual void SetupSmallestSize(Window *w, bool init_array)
	{
		NWidgetBase *display = this->head;
		NWidgetBase *bar = display->next;

		display->SetupSmallestSize(w, init_array);
		bar->SetupSmallestSize(w, init_array);

		this->smallmap_window = dynamic_cast<SmallMapWindow *>(w);
		assert(this->smallmap_window != NULL);
		this->smallest_x = max(display->smallest_x, bar->smallest_x + smallmap_window->GetMinLegendWidth());
		this->smallest_y = display->smallest_y + max(bar->smallest_y, smallmap_window->GetLegendHeight(smallmap_window->min_number_of_columns));
		this->fill_x = max(display->fill_x, bar->fill_x);
		this->fill_y = (display->fill_y == 0 && bar->fill_y == 0) ? 0 : min(display->fill_y, bar->fill_y);
		this->resize_x = max(display->resize_x, bar->resize_x);
		this->resize_y = min(display->resize_y, bar->resize_y);
	}

	virtual void AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool rtl)
	{
		this->pos_x = x;
		this->pos_y = y;
		this->current_x = given_width;
		this->current_y = given_height;

		NWidgetBase *display = this->head;
		NWidgetBase *bar = display->next;

		if (sizing == ST_SMALLEST) {
			this->smallest_x = given_width;
			this->smallest_y = given_height;
			/* Make display and bar exactly equal to their minimal size. */
			display->AssignSizePosition(ST_SMALLEST, x, y, display->smallest_x, display->smallest_y, rtl);
			bar->AssignSizePosition(ST_SMALLEST, x, y + display->smallest_y, bar->smallest_x, bar->smallest_y, rtl);
		}

		uint bar_height = max(bar->smallest_y, this->smallmap_window->GetLegendHeight(this->smallmap_window->GetNumberColumnsLegend(given_width - bar->smallest_x)));
		uint display_height = given_height - bar_height;
		display->AssignSizePosition(ST_RESIZE, x, y, given_width, display_height, rtl);
		bar->AssignSizePosition(ST_RESIZE, x, y + display_height, given_width, bar_height, rtl);
	}

	virtual NWidgetCore *GetWidgetFromPos(int x, int y)
	{
		if (!IsInsideBS(x, this->pos_x, this->current_x) || !IsInsideBS(y, this->pos_y, this->current_y)) return NULL;
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			NWidgetCore *widget = child_wid->GetWidgetFromPos(x, y);
			if (widget != NULL) return widget;
		}
		return NULL;
	}

	virtual void Draw(const Window *w)
	{
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) child_wid->Draw(w);
	}
};

/** Widget parts of the smallmap display. */
static const NWidgetPart _nested_smallmap_display[] = {
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_SM_MAP_BORDER),
		NWidget(WWT_INSET, COLOUR_BROWN, WID_SM_MAP), SetMinimalSize(346, 140), SetResize(1, 1), SetPadding(2, 2, 2, 2), EndContainer(),
	EndContainer(),
};

/** Widget parts of the smallmap legend bar + image buttons. */
static const NWidgetPart _nested_smallmap_bar[] = {
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_SM_LEGEND), SetResize(1, 1),
			NWidget(NWID_VERTICAL),
				/* Top button row. */
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_SM_ZOOM_IN),
							SetDataTip(SPR_IMG_ZOOMIN, STR_TOOLBAR_TOOLTIP_ZOOM_THE_VIEW_IN), SetFill(1, 1),
					NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_SM_CENTERMAP),
							SetDataTip(SPR_IMG_SMALLMAP, STR_SMALLMAP_CENTER), SetFill(1, 1),
					NWidget(WWT_IMGBTN, COLOUR_BROWN, WID_SM_BLANK),
							SetDataTip(SPR_DOT_SMALL, STR_NULL), SetFill(1, 1),
					NWidget(WWT_IMGBTN, COLOUR_BROWN, WID_SM_CONTOUR),
							SetDataTip(SPR_IMG_SHOW_COUNTOURS, STR_SMALLMAP_TOOLTIP_SHOW_LAND_CONTOURS_ON_MAP), SetFill(1, 1),
					NWidget(WWT_IMGBTN, COLOUR_BROWN, WID_SM_VEHICLES),
							SetDataTip(SPR_IMG_SHOW_VEHICLES, STR_SMALLMAP_TOOLTIP_SHOW_VEHICLES_ON_MAP), SetFill(1, 1),
					NWidget(WWT_IMGBTN, COLOUR_BROWN, WID_SM_INDUSTRIES),
							SetDataTip(SPR_IMG_INDUSTRY, STR_SMALLMAP_TOOLTIP_SHOW_INDUSTRIES_ON_MAP), SetFill(1, 1),
				EndContainer(),
				/* Bottom button row. */
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_SM_ZOOM_OUT),
							SetDataTip(SPR_IMG_ZOOMOUT, STR_TOOLBAR_TOOLTIP_ZOOM_THE_VIEW_OUT), SetFill(1, 1),
					NWidget(WWT_IMGBTN, COLOUR_BROWN, WID_SM_TOGGLETOWNNAME),
							SetDataTip(SPR_IMG_TOWN, STR_SMALLMAP_TOOLTIP_TOGGLE_TOWN_NAMES_ON_OFF), SetFill(1, 1),
					NWidget(WWT_IMGBTN, COLOUR_BROWN, WID_SM_LINKSTATS),
							SetDataTip(SPR_IMG_CARGOFLOW, STR_SMALLMAP_TOOLTIP_SHOW_LINK_STATS_ON_MAP), SetFill(1, 1),
					NWidget(WWT_IMGBTN, COLOUR_BROWN, WID_SM_ROUTES),
							SetDataTip(SPR_IMG_SHOW_ROUTES, STR_SMALLMAP_TOOLTIP_SHOW_TRANSPORT_ROUTES_ON), SetFill(1, 1),
					NWidget(WWT_IMGBTN, COLOUR_BROWN, WID_SM_VEGETATION),
							SetDataTip(SPR_IMG_PLANTTREES, STR_SMALLMAP_TOOLTIP_SHOW_VEGETATION_ON_MAP), SetFill(1, 1),
					NWidget(WWT_IMGBTN, COLOUR_BROWN, WID_SM_OWNERS),
							SetDataTip(SPR_IMG_COMPANY_GENERAL, STR_SMALLMAP_TOOLTIP_SHOW_LAND_OWNERS_ON_MAP), SetFill(1, 1),
				EndContainer(),
				NWidget(NWID_SPACER), SetResize(0, 1),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static NWidgetBase *SmallMapDisplay(int *biggest_index)
{
	NWidgetContainer *map_display = new NWidgetSmallmapDisplay;

	MakeNWidgets(_nested_smallmap_display, lengthof(_nested_smallmap_display), biggest_index, map_display);
	MakeNWidgets(_nested_smallmap_bar, lengthof(_nested_smallmap_bar), biggest_index, map_display);
	return map_display;
}


static const NWidgetPart _nested_smallmap_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_SM_CAPTION), SetDataTip(STR_SMALLMAP_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidgetFunction(SmallMapDisplay), // Smallmap display and legend bar + image buttons.
	/* Bottom button row and resize box. */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_SM_SELECT_BUTTONS),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SM_ENABLE_ALL), SetDataTip(STR_SMALLMAP_ENABLE_ALL, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SM_DISABLE_ALL), SetDataTip(STR_SMALLMAP_DISABLE_ALL, STR_NULL),
				NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_SM_SHOW_HEIGHT), SetDataTip(STR_SMALLMAP_SHOW_HEIGHT, STR_SMALLMAP_TOOLTIP_SHOW_HEIGHT),
				NWidget(WWT_PANEL, COLOUR_BROWN), SetFill(1, 0), SetResize(1, 0),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_BROWN), SetFill(1, 0), SetResize(1, 0),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

static WindowDesc _smallmap_desc(
	WDP_AUTO, "smallmap", 484, 314,
	WC_SMALLMAP, WC_NONE,
	0,
	_nested_smallmap_widgets, lengthof(_nested_smallmap_widgets)
);

/**
 * Show the smallmap window.
 */
void ShowSmallMap()
{
	AllocateWindowDescFront<SmallMapWindow>(&_smallmap_desc, 0);
}

/**
 * Scrolls the main window to given coordinates.
 * @param x x coordinate
 * @param y y coordinate
 * @param z z coordinate; -1 to scroll to terrain height
 * @param instant scroll instantly (meaningful only when smooth_scrolling is active)
 * @return did the viewport position change?
 */
bool ScrollMainWindowTo(int x, int y, int z, bool instant)
{
	bool res = ScrollWindowTo(x, y, z, FindWindowById(WC_MAIN_WINDOW, 0), instant);

	/* If a user scrolls to a tile (via what way what so ever) and already is on
	 * that tile (e.g.: pressed twice), move the smallmap to that location,
	 * so you directly see where you are on the smallmap. */

	if (res) return res;

	SmallMapWindow *w = dynamic_cast<SmallMapWindow*>(FindWindowById(WC_SMALLMAP, 0));
	if (w != NULL) w->SmallMapCenterOnCurrentPos();

	return res;
}
