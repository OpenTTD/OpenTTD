/* $Id$ */

/** @file smallmap_gui.cpp GUI that shows a small map of the world with metadata like owner or height. */

#include "stdafx.h"
#include "clear_map.h"
#include "industry_map.h"
#include "station_map.h"
#include "landscape.h"
#include "window_gui.h"
#include "tree_map.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "town.h"
#include "blitter/factory.hpp"
#include "tunnelbridge_map.h"
#include "strings_func.h"
#include "zoom_func.h"
#include "core/endian_func.hpp"
#include "vehicle_base.h"
#include "sound_func.h"
#include "window_func.h"

#include "table/strings.h"
#include "table/sprites.h"

static const Widget _smallmap_widgets[] = {
{  WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_BROWN,     0,    10,     0,    13, STR_00C5,                 STR_018B_CLOSE_WINDOW},
{   WWT_CAPTION,  RESIZE_RIGHT,  COLOUR_BROWN,    11,   337,     0,    13, STR_00B0_MAP,             STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_STICKYBOX,     RESIZE_LR,  COLOUR_BROWN,   338,   349,     0,    13, 0x0,                      STR_STICKY_BUTTON},
{     WWT_PANEL,     RESIZE_RB,  COLOUR_BROWN,     0,   349,    14,   157, 0x0,                      STR_NULL},
{     WWT_INSET,     RESIZE_RB,  COLOUR_BROWN,     2,   347,    16,   155, 0x0,                      STR_NULL},
{     WWT_PANEL,    RESIZE_RTB,  COLOUR_BROWN,     0,   261,   158,   201, 0x0,                      STR_NULL},
{     WWT_PANEL,   RESIZE_LRTB,  COLOUR_BROWN,   262,   349,   158,   158, 0x0,                      STR_NULL},
{    WWT_IMGBTN,   RESIZE_LRTB,  COLOUR_BROWN,   284,   305,   158,   179, SPR_IMG_SHOW_COUNTOURS,   STR_0191_SHOW_LAND_CONTOURS_ON_MAP},
{    WWT_IMGBTN,   RESIZE_LRTB,  COLOUR_BROWN,   306,   327,   158,   179, SPR_IMG_SHOW_VEHICLES,    STR_0192_SHOW_VEHICLES_ON_MAP},
{    WWT_IMGBTN,   RESIZE_LRTB,  COLOUR_BROWN,   328,   349,   158,   179, SPR_IMG_INDUSTRY,         STR_0193_SHOW_INDUSTRIES_ON_MAP},
{    WWT_IMGBTN,   RESIZE_LRTB,  COLOUR_BROWN,   284,   305,   180,   201, SPR_IMG_SHOW_ROUTES,      STR_0194_SHOW_TRANSPORT_ROUTES_ON},
{    WWT_IMGBTN,   RESIZE_LRTB,  COLOUR_BROWN,   306,   327,   180,   201, SPR_IMG_PLANTTREES,       STR_0195_SHOW_VEGETATION_ON_MAP},
{    WWT_IMGBTN,   RESIZE_LRTB,  COLOUR_BROWN,   328,   349,   180,   201, SPR_IMG_COMPANY_GENERAL,  STR_0196_SHOW_LAND_OWNERS_ON_MAP},
{    WWT_IMGBTN,   RESIZE_LRTB,  COLOUR_BROWN,   262,   283,   158,   179, SPR_IMG_SMALLMAP,         STR_SMALLMAP_CENTER},
{    WWT_IMGBTN,   RESIZE_LRTB,  COLOUR_BROWN,   262,   283,   180,   201, SPR_IMG_TOWN,             STR_0197_TOGGLE_TOWN_NAMES_ON_OFF},
{     WWT_PANEL,    RESIZE_RTB,  COLOUR_BROWN,     0,   337,   202,   213, 0x0,                      STR_NULL},
{   WWT_TEXTBTN,     RESIZE_TB,  COLOUR_BROWN,     0,    99,   202,   213, STR_MESSAGES_ENABLE_ALL,  STR_NULL},
{   WWT_TEXTBTN,     RESIZE_TB,  COLOUR_BROWN,   100,   201,   202,   213, STR_MESSAGES_DISABLE_ALL, STR_NULL},
{ WWT_RESIZEBOX,   RESIZE_LRTB,  COLOUR_BROWN,   338,   349,   202,   213, 0x0,                      STR_RESIZE_BUTTON},
{  WIDGETS_END},
};

/* number of used industries */
static int _smallmap_industry_count;

/** Macro for ordinary entry of LegendAndColour */
#define MK(a, b) {a, b, INVALID_INDUSTRYTYPE, true, false, false}
/** Macro for end of list marker in arrays of LegendAndColour */
#define MKEND() {0, STR_NULL, INVALID_INDUSTRYTYPE, true, true, false}
/** Macro for break marker in arrays of LegendAndColour.
 * It will have valid data, though */
#define MS(a, b) {a, b, INVALID_INDUSTRYTYPE, true, false, true}

/** Structure for holding relevant data for legends in small map */
struct LegendAndColour {
	uint16 colour;     ///< colour of the item on the map
	StringID legend;   ///< string corresponding to the coloured item
	IndustryType type; ///< type of industry
	bool show_on_map;  ///< for filtering industries, if true is shown on map in colour
	bool end;          ///< this is the end of the list
	bool col_break;    ///< perform a break and go one collumn further
};

/** Legend text giving the colours to look for on the minimap */
static const LegendAndColour _legend_land_contours[] = {
	MK(0x5A, STR_00F0_100M),
	MK(0x5C, STR_00F1_200M),
	MK(0x5E, STR_00F2_300M),
	MK(0x1F, STR_00F3_400M),
	MK(0x27, STR_00F4_500M),

	MS(0xD7, STR_00EB_ROADS),
	MK(0x0A, STR_00EC_RAILROADS),
	MK(0x98, STR_00ED_STATIONS_AIRPORTS_DOCKS),
	MK(0xB5, STR_00EE_BUILDINGS_INDUSTRIES),
	MK(0x0F, STR_00EF_VEHICLES),
	MKEND()
};

static const LegendAndColour _legend_vehicles[] = {
	MK(0xB8, STR_00F5_TRAINS),
	MK(0xBF, STR_00F6_ROAD_VEHICLES),
	MK(0x98, STR_00F7_SHIPS),
	MK(0x0F, STR_00F8_AIRCRAFT),
	MS(0xD7, STR_00F9_TRANSPORT_ROUTES),
	MK(0xB5, STR_00EE_BUILDINGS_INDUSTRIES),
	MKEND()
};

static const LegendAndColour _legend_routes[] = {
	MK(0xD7, STR_00EB_ROADS),
	MK(0x0A, STR_00EC_RAILROADS),
	MK(0xB5, STR_00EE_BUILDINGS_INDUSTRIES),
	MS(0x56, STR_011B_RAILROAD_STATION),

	MK(0xC2, STR_011C_TRUCK_LOADING_BAY),
	MK(0xBF, STR_011D_BUS_STATION),
	MK(0xB8, STR_011E_AIRPORT_HELIPORT),
	MK(0x98, STR_011F_DOCK),
	MKEND()
};

static const LegendAndColour _legend_vegetation[] = {
	MK(0x52, STR_0120_ROUGH_LAND),
	MK(0x54, STR_0121_GRASS_LAND),
	MK(0x37, STR_0122_BARE_LAND),
	MK(0x25, STR_0123_FIELDS),
	MK(0x57, STR_0124_TREES),
	MK(0xD0, STR_00FC_FOREST),
	MS(0x0A, STR_0125_ROCKS),

	MK(0xC2, STR_012A_DESERT),
	MK(0x98, STR_012B_SNOW),
	MK(0xD7, STR_00F9_TRANSPORT_ROUTES),
	MK(0xB5, STR_00EE_BUILDINGS_INDUSTRIES),
	MKEND()
};

static const LegendAndColour _legend_land_owners[] = {
	MK(0xCA, STR_0126_WATER),
	MK(0x54, STR_0127_NO_OWNER),
	MK(0xB4, STR_0128_TOWNS),
	MK(0x20, STR_0129_INDUSTRIES),
	MKEND()
};
#undef MK
#undef MS
#undef MKEND

/** Allow room for all industries, plus a terminator entry
 * This is required in order to have the indutry slots all filled up */
static LegendAndColour _legend_from_industries[NUM_INDUSTRYTYPES + 1];
/* For connecting industry type to position in industries list(small map legend) */
static uint _industry_to_list_pos[NUM_INDUSTRYTYPES];

/**
 * Fills an array for the industries legends.
 */
void BuildIndustriesLegend()
{
	const IndustrySpec *indsp;
	uint j = 0;

	/* Add each name */
	for (IndustryType i = 0; i < NUM_INDUSTRYTYPES; i++) {
		indsp = GetIndustrySpec(i);
		if (indsp->enabled) {
			_legend_from_industries[j].legend = indsp->name;
			_legend_from_industries[j].colour = indsp->map_colour;
			_legend_from_industries[j].type = i;
			_legend_from_industries[j].show_on_map = true;
			_legend_from_industries[j].col_break = false;
			_legend_from_industries[j].end = false;

			/* Store widget number for this industry type */
			_industry_to_list_pos[i] = j;
			j++;
		}
	}
	/* Terminate the list */
	_legend_from_industries[j].end = true;

	/* Store number of enabled industries */
	_smallmap_industry_count = j;
}

static const LegendAndColour * const _legend_table[] = {
	_legend_land_contours,
	_legend_vehicles,
	_legend_from_industries,
	_legend_routes,
	_legend_vegetation,
	_legend_land_owners,
};

#define MKCOLOUR(x) TO_LE32X(x)

/**
 * Height encodings; MAX_TILE_HEIGHT + 1 levels, from 0 to MAX_TILE_HEIGHT
 */
static const uint32 _map_height_bits[] = {
	MKCOLOUR(0x5A5A5A5A),
	MKCOLOUR(0x5A5B5A5B),
	MKCOLOUR(0x5B5B5B5B),
	MKCOLOUR(0x5B5C5B5C),
	MKCOLOUR(0x5C5C5C5C),
	MKCOLOUR(0x5C5D5C5D),
	MKCOLOUR(0x5D5D5D5D),
	MKCOLOUR(0x5D5E5D5E),
	MKCOLOUR(0x5E5E5E5E),
	MKCOLOUR(0x5E5F5E5F),
	MKCOLOUR(0x5F5F5F5F),
	MKCOLOUR(0x5F1F5F1F),
	MKCOLOUR(0x1F1F1F1F),
	MKCOLOUR(0x1F271F27),
	MKCOLOUR(0x27272727),
	MKCOLOUR(0x27272727),
};
assert_compile(lengthof(_map_height_bits) == MAX_TILE_HEIGHT + 1);

struct AndOr {
	uint32 mor;
	uint32 mand;
};

static inline uint32 ApplyMask(uint32 colour, const AndOr *mask)
{
	return (colour & mask->mand) | mask->mor;
}


static const AndOr _smallmap_contours_andor[] = {
	{MKCOLOUR(0x00000000), MKCOLOUR(0xFFFFFFFF)},
	{MKCOLOUR(0x000A0A00), MKCOLOUR(0xFF0000FF)},
	{MKCOLOUR(0x00D7D700), MKCOLOUR(0xFF0000FF)},
	{MKCOLOUR(0x00B5B500), MKCOLOUR(0xFF0000FF)},
	{MKCOLOUR(0x00000000), MKCOLOUR(0xFFFFFFFF)},
	{MKCOLOUR(0x98989898), MKCOLOUR(0x00000000)},
	{MKCOLOUR(0xCACACACA), MKCOLOUR(0x00000000)},
	{MKCOLOUR(0x00000000), MKCOLOUR(0xFFFFFFFF)},
	{MKCOLOUR(0xB5B5B5B5), MKCOLOUR(0x00000000)},
	{MKCOLOUR(0x00000000), MKCOLOUR(0xFFFFFFFF)},
	{MKCOLOUR(0x00B5B500), MKCOLOUR(0xFF0000FF)},
	{MKCOLOUR(0x000A0A00), MKCOLOUR(0xFF0000FF)},
};

static const AndOr _smallmap_vehicles_andor[] = {
	{MKCOLOUR(0x00000000), MKCOLOUR(0xFFFFFFFF)},
	{MKCOLOUR(0x00D7D700), MKCOLOUR(0xFF0000FF)},
	{MKCOLOUR(0x00D7D700), MKCOLOUR(0xFF0000FF)},
	{MKCOLOUR(0x00B5B500), MKCOLOUR(0xFF0000FF)},
	{MKCOLOUR(0x00000000), MKCOLOUR(0xFFFFFFFF)},
	{MKCOLOUR(0x00D7D700), MKCOLOUR(0xFF0000FF)},
	{MKCOLOUR(0xCACACACA), MKCOLOUR(0x00000000)},
	{MKCOLOUR(0x00000000), MKCOLOUR(0xFFFFFFFF)},
	{MKCOLOUR(0xB5B5B5B5), MKCOLOUR(0x00000000)},
	{MKCOLOUR(0x00000000), MKCOLOUR(0xFFFFFFFF)},
	{MKCOLOUR(0x00B5B500), MKCOLOUR(0xFF0000FF)},
	{MKCOLOUR(0x00D7D700), MKCOLOUR(0xFF0000FF)},
};

static const AndOr _smallmap_vegetation_andor[] = {
	{MKCOLOUR(0x00000000), MKCOLOUR(0xFFFFFFFF)},
	{MKCOLOUR(0x00D7D700), MKCOLOUR(0xFF0000FF)},
	{MKCOLOUR(0x00D7D700), MKCOLOUR(0xFF0000FF)},
	{MKCOLOUR(0x00B5B500), MKCOLOUR(0xFF0000FF)},
	{MKCOLOUR(0x00575700), MKCOLOUR(0xFF0000FF)},
	{MKCOLOUR(0x00D7D700), MKCOLOUR(0xFF0000FF)},
	{MKCOLOUR(0xCACACACA), MKCOLOUR(0x00000000)},
	{MKCOLOUR(0x00000000), MKCOLOUR(0xFFFFFFFF)},
	{MKCOLOUR(0xB5B5B5B5), MKCOLOUR(0x00000000)},
	{MKCOLOUR(0x00000000), MKCOLOUR(0xFFFFFFFF)},
	{MKCOLOUR(0x00B5B500), MKCOLOUR(0xFF0000FF)},
	{MKCOLOUR(0x00D7D700), MKCOLOUR(0xFF0000FF)},
};

typedef uint32 GetSmallMapPixels(TileIndex tile); // typedef callthrough function

/**
 * Draws one column of the small map in a certain mode onto the screen buffer. This
 * function looks exactly the same for all types
 *
 * @param dst Pointer to a part of the screen buffer to write to.
 * @param xc The X coordinate of the first tile in the column.
 * @param yc The Y coordinate of the first tile in the column
 * @param pitch Number of pixels to advance in the screen buffer each time a pixel is written.
 * @param reps Number of lines to draw
 * @param mask ?
 * @param proc Pointer to the colour function
 * @see GetSmallMapPixels(TileIndex)
 */
static void DrawSmallMapStuff(void *dst, uint xc, uint yc, int pitch, int reps, uint32 mask, GetSmallMapPixels *proc)
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	void *dst_ptr_abs_end = blitter->MoveTo(_screen.dst_ptr, 0, _screen.height);
	void *dst_ptr_end = blitter->MoveTo(dst_ptr_abs_end, -4, 0);

	do {
		/* check if the tile (xc,yc) is within the map range */
		uint min_xy = _settings_game.construction.freeform_edges ? 1 : 0;
		if (IsInsideMM(xc, min_xy, MapMaxX()) && IsInsideMM(yc, min_xy, MapMaxY())) {
			/* check if the dst pointer points to a pixel inside the screen buffer */
			if (dst < _screen.dst_ptr) continue;
			if (dst >= dst_ptr_abs_end) continue;

			uint32 val = proc(TileXY(xc, yc)) & mask;
			uint8 *val8 = (uint8 *)&val;

			if (dst <= dst_ptr_end) {
				blitter->SetPixelIfEmpty(dst, 0, 0, val8[0]);
				blitter->SetPixelIfEmpty(dst, 1, 0, val8[1]);
				blitter->SetPixelIfEmpty(dst, 2, 0, val8[2]);
				blitter->SetPixelIfEmpty(dst, 3, 0, val8[3]);
			} else {
				/* It happens that there are only 1, 2 or 3 pixels left to fill, so in that special case, write till the end of the video-buffer */
				int i = 0;
				do {
					blitter->SetPixelIfEmpty(dst, 0, 0, val8[i]);
				} while (i++, dst = blitter->MoveTo(dst, 1, 0), dst < dst_ptr_abs_end);
			}
		}
	/* switch to next tile in the column */
	} while (xc++, yc++, dst = blitter->MoveTo(dst, pitch, 0), --reps != 0);
}


static inline TileType GetEffectiveTileType(TileIndex tile)
{
	TileType t = GetTileType(tile);

	if (t == MP_TUNNELBRIDGE) {
		TransportType tt = GetTunnelBridgeTransportType(tile);

		switch (tt) {
			case TRANSPORT_RAIL: t = MP_RAILWAY; break;
			case TRANSPORT_ROAD: t = MP_ROAD;    break;
			default:             t = MP_WATER;   break;
		}
	}
	return t;
}

/**
 * Return the colour a tile would be displayed with in the small map in mode "Contour".
 * @param tile The tile of which we would like to get the colour.
 * @return The colour of tile in the small map in mode "Contour"
 */
static inline uint32 GetSmallMapContoursPixels(TileIndex tile)
{
	TileType t = GetEffectiveTileType(tile);

	return
		ApplyMask(_map_height_bits[TileHeight(tile)], &_smallmap_contours_andor[t]);
}

/**
 * Return the colour a tile would be displayed with in the small map in mode "Vehicles".
 *
 * @param tile The tile of which we would like to get the colour.
 * @return The colour of tile in the small map in mode "Vehicles"
 */
static inline uint32 GetSmallMapVehiclesPixels(TileIndex tile)
{
	TileType t = GetEffectiveTileType(tile);

	return ApplyMask(MKCOLOUR(0x54545454), &_smallmap_vehicles_andor[t]);
}

/**
 * Return the colour a tile would be displayed with in the small map in mode "Industries".
 *
 * @param tile The tile of which we would like to get the colour.
 * @return The colour of tile in the small map in mode "Industries"
 */
static inline uint32 GetSmallMapIndustriesPixels(TileIndex tile)
{
	TileType t = GetEffectiveTileType(tile);

	if (t == MP_INDUSTRY) {
		/* If industry is allowed to be seen, use its colour on the map */
		if (_legend_from_industries[_industry_to_list_pos[GetIndustryByTile(tile)->type]].show_on_map) {
			return GetIndustrySpec(GetIndustryByTile(tile)->type)->map_colour * 0x01010101;
		} else {
			/* otherwise, return the colour of the clear tiles, which will make it disappear */
			return ApplyMask(MKCOLOUR(0x54545454), &_smallmap_vehicles_andor[MP_CLEAR]);
		}
	}

	return ApplyMask(MKCOLOUR(0x54545454), &_smallmap_vehicles_andor[t]);
}

/**
 * Return the colour a tile would be displayed with in the small map in mode "Routes".
 *
 * @param tile The tile of which we would like to get the colour.
 * @return The colour of tile  in the small map in mode "Routes"
 */
static inline uint32 GetSmallMapRoutesPixels(TileIndex tile)
{
	TileType t = GetEffectiveTileType(tile);
	uint32 bits;

	if (t == MP_STATION) {
		switch (GetStationType(tile)) {
			case STATION_RAIL:    bits = MKCOLOUR(0x56565656); break;
			case STATION_AIRPORT: bits = MKCOLOUR(0xB8B8B8B8); break;
			case STATION_TRUCK:   bits = MKCOLOUR(0xC2C2C2C2); break;
			case STATION_BUS:     bits = MKCOLOUR(0xBFBFBFBF); break;
			case STATION_DOCK:    bits = MKCOLOUR(0x98989898); break;
			default:              bits = MKCOLOUR(0xFFFFFFFF); break;
		}
	} else {
		/* ground colour */
		bits = ApplyMask(MKCOLOUR(0x54545454), &_smallmap_contours_andor[t]);
	}
	return bits;
}


static const uint32 _vegetation_clear_bits[] = {
	MKCOLOUR(0x54545454), ///< full grass
	MKCOLOUR(0x52525252), ///< rough land
	MKCOLOUR(0x0A0A0A0A), ///< rocks
	MKCOLOUR(0x25252525), ///< fields
	MKCOLOUR(0x98989898), ///< snow
	MKCOLOUR(0xC2C2C2C2), ///< desert
	MKCOLOUR(0x54545454), ///< unused
	MKCOLOUR(0x54545454), ///< unused
};

static inline uint32 GetSmallMapVegetationPixels(TileIndex tile)
{
	TileType t = GetEffectiveTileType(tile);
	uint32 bits;

	switch (t) {
		case MP_CLEAR:
			if (IsClearGround(tile, CLEAR_GRASS) && GetClearDensity(tile) < 3) {
				bits = MKCOLOUR(0x37373737);
			} else {
				bits = _vegetation_clear_bits[GetClearGround(tile)];
			}
			break;

		case MP_INDUSTRY:
			bits = GetIndustrySpec(GetIndustryByTile(tile)->type)->check_proc == CHECK_FOREST ? MKCOLOUR(0xD0D0D0D0) : MKCOLOUR(0xB5B5B5B5);
			break;

		case MP_TREES:
			if (GetTreeGround(tile) == TREE_GROUND_SNOW_DESERT) {
				bits = (_settings_game.game_creation.landscape == LT_ARCTIC) ? MKCOLOUR(0x98575798) : MKCOLOUR(0xC25757C2);
			} else {
				bits = MKCOLOUR(0x54575754);
			}
			break;

		default:
			bits = ApplyMask(MKCOLOUR(0x54545454), &_smallmap_vehicles_andor[t]);
			break;
	}

	return bits;
}


static uint32 _owner_colours[OWNER_END + 1];

/**
 * Return the colour a tile would be displayed with in the small map in mode "Owner".
 *
 * @param tile The tile of which we would like to get the colour.
 * @return The colour of tile in the small map in mode "Owner"
 */
static inline uint32 GetSmallMapOwnerPixels(TileIndex tile)
{
	Owner o;

	switch (GetTileType(tile)) {
		case MP_INDUSTRY: o = OWNER_END;          break;
		case MP_HOUSE:    o = OWNER_TOWN;         break;
		default:          o = GetTileOwner(tile); break;
		/* FIXME: For MP_ROAD there are multiple owners.
		 * GetTileOwner returns the rail owner (level crossing) resp. the owner of ROADTYPE_ROAD (normal road),
		 * even if there are no ROADTYPE_ROAD bits on the tile.
		 */
	}

	return _owner_colours[o];
}


static const uint32 _smallmap_mask_left[3] = {
	MKCOLOUR(0xFF000000),
	MKCOLOUR(0xFFFF0000),
	MKCOLOUR(0xFFFFFF00),
};

static const uint32 _smallmap_mask_right[] = {
	MKCOLOUR(0x000000FF),
	MKCOLOUR(0x0000FFFF),
	MKCOLOUR(0x00FFFFFF),
};

/* each tile has 4 x pixels and 1 y pixel */

static GetSmallMapPixels *_smallmap_draw_procs[] = {
	GetSmallMapContoursPixels,
	GetSmallMapVehiclesPixels,
	GetSmallMapIndustriesPixels,
	GetSmallMapRoutesPixels,
	GetSmallMapVegetationPixels,
	GetSmallMapOwnerPixels,
};

static const byte _vehicle_type_colours[6] = {
	184, 191, 152, 15, 215, 184
};


static void DrawVertMapIndicator(int x, int y, int x2, int y2)
{
	GfxFillRect(x, y,      x2, y + 3, 69);
	GfxFillRect(x, y2 - 3, x2, y2,    69);
}

static void DrawHorizMapIndicator(int x, int y, int x2, int y2)
{
	GfxFillRect(x,      y, x + 3, y2, 69);
	GfxFillRect(x2 - 3, y, x2,    y2, 69);
}

enum SmallMapWindowWidgets {
	SM_WIDGET_MAP_BORDER = 3,
	SM_WIDGET_MAP,
	SM_WIDGET_LEGEND,
	SM_WIDGET_BUTTONSPANEL,
	SM_WIDGET_CONTOUR,
	SM_WIDGET_VEHICLES,
	SM_WIDGET_INDUSTRIES,
	SM_WIDGET_ROUTES,
	SM_WIDGET_VEGETATION,
	SM_WIDGET_OWNERS,
	SM_WIDGET_CENTERMAP,
	SM_WIDGET_TOGGLETOWNNAME,
	SM_WIDGET_BOTTOMPANEL,
	SM_WIDGET_ENABLEINDUSTRIES,
	SM_WIDGET_DISABLEINDUSTRIES,
	SM_WIDGET_RESIZEBOX,
};

class SmallMapWindow : public Window
{
	enum SmallMapType {
		SMT_CONTOUR,
		SMT_VEHICLES,
		SMT_INDUSTRY,
		SMT_ROUTES,
		SMT_VEGETATION,
		SMT_OWNER,
	};

	static SmallMapType map_type;
	static bool show_towns;

	int32 scroll_x;
	int32 scroll_y;
	int32 subscroll;
	uint8 refresh;

	static const int COLUMN_WIDTH = 119;
	static const int MIN_LEGEND_HEIGHT = 6 * 7;

public:
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
	 * @param w pointer to Window struct
	 * @param type type of map requested (vegetation, owners, routes, etc)
	 * @param show_towns true if the town names should be displayed, false if not.
	 */
	void DrawSmallMap(DrawPixelInfo *dpi)
	{
		Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
		DrawPixelInfo *old_dpi;
		int dx, dy, x, y, x2, y2;
		void *ptr;
		int tile_x;
		int tile_y;
		ViewPort *vp;

		old_dpi = _cur_dpi;
		_cur_dpi = dpi;

		/* clear it */
		GfxFillRect(dpi->left, dpi->top, dpi->left + dpi->width - 1, dpi->top + dpi->height - 1, 0);

		/* setup owner table */
		if (this->map_type == SMT_OWNER) {
			const Company *c;

			/* fill with some special colours */
			_owner_colours[OWNER_TOWN] = MKCOLOUR(0xB4B4B4B4);
			_owner_colours[OWNER_NONE] = MKCOLOUR(0x54545454);
			_owner_colours[OWNER_WATER] = MKCOLOUR(0xCACACACA);
			_owner_colours[OWNER_END]   = MKCOLOUR(0x20202020); // industry

			/* now fill with the company colours */
			FOR_ALL_COMPANIES(c) {
				_owner_colours[c->index] =
					_colour_gradient[c->colour][5] * 0x01010101;
			}
		}

		tile_x = this->scroll_x / TILE_SIZE;
		tile_y = this->scroll_y / TILE_SIZE;

		dx = dpi->left + this->subscroll;
		tile_x -= dx / 4;
		tile_y += dx / 4;
		dx &= 3;

		dy = dpi->top;
		tile_x += dy / 2;
		tile_y += dy / 2;

		if (dy & 1) {
			tile_x++;
			dx += 2;
			if (dx > 3) {
				dx -= 4;
				tile_x--;
				tile_y++;
			}
		}

		ptr = blitter->MoveTo(dpi->dst_ptr, -dx - 4, 0);
		x = - dx - 4;
		y = 0;

		for (;;) {
			uint32 mask = 0xFFFFFFFF;
			int reps;
			int t;

			/* distance from left edge */
			if (x < 0) {
				if (x < -3) goto skip_column;
				/* mask to use at the left edge */
				mask = _smallmap_mask_left[x + 3];
			}

			/* distance from right edge */
			t = dpi->width - x;
			if (t < 4) {
				if (t <= 0) break; // exit loop
				/* mask to use at the right edge */
				mask &= _smallmap_mask_right[t - 1];
			}

			/* number of lines */
			reps = (dpi->height - y + 1) / 2;
			if (reps > 0) {
				DrawSmallMapStuff(ptr, tile_x, tile_y, dpi->pitch * 2, reps, mask, _smallmap_draw_procs[this->map_type]);
			}

	skip_column:
			if (y == 0) {
				tile_y++;
				y++;
				ptr = blitter->MoveTo(ptr, 0, 1);
			} else {
				tile_x--;
				y--;
				ptr = blitter->MoveTo(ptr, 0, -1);
			}
			ptr = blitter->MoveTo(ptr, 2, 0);
			x += 2;
		}

		/* draw vehicles? */
		if (this->map_type == SMT_CONTOUR || this->map_type == SMT_VEHICLES) {
			Vehicle *v;
			bool skip;
			byte colour;

			FOR_ALL_VEHICLES(v) {
				if (v->type != VEH_EFFECT &&
						(v->vehstatus & (VS_HIDDEN | VS_UNCLICKABLE)) == 0) {
					/* Remap into flat coordinates. */
					Point pt = RemapCoords(
						v->x_pos / TILE_SIZE - this->scroll_x / TILE_SIZE, // divide each one separately because (a-b)/c != a/c-b/c in integer world
						v->y_pos / TILE_SIZE - this->scroll_y / TILE_SIZE, //    dtto
						0);
					x = pt.x;
					y = pt.y;

					/* Check if y is out of bounds? */
					y -= dpi->top;
					if (!IsInsideMM(y, 0, dpi->height)) continue;

					/* Default is to draw both pixels. */
					skip = false;

					/* Offset X coordinate */
					x -= this->subscroll + 3 + dpi->left;

					if (x < 0) {
						/* if x+1 is 0, that means we're on the very left edge,
						 *  and should thus only draw a single pixel */
						if (++x != 0) continue;
						skip = true;
					} else if (x >= dpi->width - 1) {
						/* Check if we're at the very right edge, and if so draw only a single pixel */
						if (x != dpi->width - 1) continue;
						skip = true;
					}

					/* Calculate pointer to pixel and the colour */
					colour = (this->map_type == SMT_VEHICLES) ? _vehicle_type_colours[v->type] : 0xF;

					/* And draw either one or two pixels depending on clipping */
					blitter->SetPixel(dpi->dst_ptr, x, y, colour);
					if (!skip) blitter->SetPixel(dpi->dst_ptr, x + 1, y, colour);
				}
			}
		}

		if (this->show_towns) {
			const Town *t;

			FOR_ALL_TOWNS(t) {
				/* Remap the town coordinate */
				Point pt = RemapCoords(
					(int)(TileX(t->xy) * TILE_SIZE - this->scroll_x) / TILE_SIZE,
					(int)(TileY(t->xy) * TILE_SIZE - this->scroll_y) / TILE_SIZE,
					0);
				x = pt.x - this->subscroll + 3 - (t->sign.width_2 >> 1);
				y = pt.y;

				/* Check if the town sign is within bounds */
				if (x + t->sign.width_2 > dpi->left &&
						x < dpi->left + dpi->width &&
						y + 6 > dpi->top &&
						y < dpi->top + dpi->height) {
					/* And draw it. */
					SetDParam(0, t->index);
					DrawString(x, y, STR_2056, TC_WHITE);
				}
			}
		}

		/* Draw map indicators */
		{
			Point pt;

			/* Find main viewport. */
			vp = FindWindowById(WC_MAIN_WINDOW, 0)->viewport;

			pt = RemapCoords(this->scroll_x, this->scroll_y, 0);

			x = vp->virtual_left - pt.x;
			y = vp->virtual_top - pt.y;
			x2 = (x + vp->virtual_width) / TILE_SIZE;
			y2 = (y + vp->virtual_height) / TILE_SIZE;
			x /= TILE_SIZE;
			y /= TILE_SIZE;

			x -= this->subscroll;
			x2 -= this->subscroll;

			DrawVertMapIndicator(x, y, x, y2);
			DrawVertMapIndicator(x2, y, x2, y2);

			DrawHorizMapIndicator(x, y, x2, y);
			DrawHorizMapIndicator(x, y2, x2, y2);
		}
		_cur_dpi = old_dpi;
	}

	void SmallMapCenterOnCurrentPos()
	{
		int x, y;
		ViewPort *vp;
		vp = FindWindowById(WC_MAIN_WINDOW, 0)->viewport;

		x  = ((vp->virtual_width  - (this->widget[SM_WIDGET_MAP].right  - this->widget[SM_WIDGET_MAP].left) * TILE_SIZE) / 2 + vp->virtual_left) / 4;
		y  = ((vp->virtual_height - (this->widget[SM_WIDGET_MAP].bottom - this->widget[SM_WIDGET_MAP].top ) * TILE_SIZE) / 2 + vp->virtual_top ) / 2 - TILE_SIZE * 2;
		this->scroll_x = (y - x) & ~0xF;
		this->scroll_y = (x + y) & ~0xF;
		this->SetDirty();
	}

	void ResizeLegend()
	{
		Widget *legend = &this->widget[SM_WIDGET_LEGEND];
		int rows = (legend->bottom - legend->top) - 1;
		int columns = (legend->right - legend->left) / COLUMN_WIDTH;
		int new_rows = (this->map_type == SMT_INDUSTRY) ? ((_smallmap_industry_count + columns - 1) / columns) * 6 : MIN_LEGEND_HEIGHT;

		new_rows = max(new_rows, MIN_LEGEND_HEIGHT);

		if (new_rows != rows) {
			this->SetDirty();

			/* The legend widget needs manual adjustment as by default
			 * it lays outside the filler widget's bounds. */
			legend->top--;
			/* Resize the filler widget, and move widgets below it. */
			ResizeWindowForWidget(this, SM_WIDGET_BUTTONSPANEL, 0, new_rows - rows);
			legend->top++;

			/* Resize map border widget so the window stays the same size */
			ResizeWindowForWidget(this, SM_WIDGET_MAP_BORDER, 0, rows - new_rows);
			/* Manually adjust the map widget as it lies completely within
			 * the map border widget */
			this->widget[SM_WIDGET_MAP].bottom += rows - new_rows;

			this->SetDirty();
		}
	}

	SmallMapWindow(const WindowDesc *desc, int window_number) : Window(desc, window_number)
	{
		this->LowerWidget(this->map_type + SM_WIDGET_CONTOUR);
		this->SetWidgetLoweredState(SM_WIDGET_TOGGLETOWNNAME, this->show_towns);

		this->SmallMapCenterOnCurrentPos();
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		DrawPixelInfo new_dpi;

		/* Hide Enable all/Disable all buttons if is not industry type small map*/
		this->SetWidgetHiddenState(SM_WIDGET_ENABLEINDUSTRIES, this->map_type != SMT_INDUSTRY);
		this->SetWidgetHiddenState(SM_WIDGET_DISABLEINDUSTRIES, this->map_type != SMT_INDUSTRY);

		/* draw the window */
		SetDParam(0, STR_00E5_CONTOURS + this->map_type);
		this->DrawWidgets();

		const Widget *legend = &this->widget[SM_WIDGET_LEGEND];

		int y_org = legend->top + 1;
		int x = 4;
		int y = y_org;

		for (const LegendAndColour *tbl = _legend_table[this->map_type]; !tbl->end; ++tbl) {
			if (tbl->col_break || y >= legend->bottom) {
				/* Column break needed, continue at top, COLUMN_WIDTH pixels
				 * (one "row") to the right. */
				x += COLUMN_WIDTH;
				y = y_org;
			}

			if (this->map_type == SMT_INDUSTRY) {
				/* Industry name must be formated, since it's not in tiny font in the specs.
				 * So, draw with a parameter and use the STR_SMALLMAP_INDUSTRY string, which is tiny font.*/
				SetDParam(0, tbl->legend);
				assert(tbl->type < NUM_INDUSTRYTYPES);
				SetDParam(1, _industry_counts[tbl->type]);
				if (!tbl->show_on_map) {
					/* Simply draw the string, not the black border of the legend colour.
					 * This will enforce the idea of the disabled item */
					DrawString(x + 11, y, STR_SMALLMAP_INDUSTRY, TC_GREY);
				} else {
					DrawString(x + 11, y, STR_SMALLMAP_INDUSTRY, TC_BLACK);
					GfxFillRect(x, y + 1, x + 8, y + 5, 0); // outer border of the legend colour
				}
			} else {
				/* Anything that is not an industry is using normal process */
				GfxFillRect(x, y + 1, x + 8, y + 5, 0);
				DrawString(x + 11, y, tbl->legend, TC_FROMSTRING);
			}
			GfxFillRect(x + 1, y + 2, x + 7, y + 4, tbl->colour); // legend colour

			y += 6;
		}

		const Widget *wi = &this->widget[SM_WIDGET_MAP];
		if (!FillDrawPixelInfo(&new_dpi, wi->left + 1, wi->top + 1, wi->right - wi->left - 1, wi->bottom - wi->top - 1)) return;

		this->DrawSmallMap(&new_dpi);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case SM_WIDGET_MAP: { // Map window
				/*
				 * XXX: scrolling with the left mouse button is done by subsequently
				 * clicking with the left mouse button; clicking once centers the
				 * large map at the selected point. So by unclicking the left mouse
				 * button here, it gets reclicked during the next inputloop, which
				 * would make it look like the mouse is being dragged, while it is
				 * actually being (virtually) clicked every inputloop.
				 */
				_left_button_clicked = false;

				Point pt = RemapCoords(this->scroll_x, this->scroll_y, 0);
				Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
				w->viewport->follow_vehicle = INVALID_VEHICLE;
				w->viewport->dest_scrollpos_x = pt.x + ((_cursor.pos.x - this->left + 2) << 4) - (w->viewport->virtual_width >> 1);
				w->viewport->dest_scrollpos_y = pt.y + ((_cursor.pos.y - this->top - 16) << 4) - (w->viewport->virtual_height >> 1);

				this->SetDirty();
			} break;

			case SM_WIDGET_CONTOUR:    // Show land contours
			case SM_WIDGET_VEHICLES:   // Show vehicles
			case SM_WIDGET_INDUSTRIES: // Show industries
			case SM_WIDGET_ROUTES:     // Show transport routes
			case SM_WIDGET_VEGETATION: // Show vegetation
			case SM_WIDGET_OWNERS:     // Show land owners
				this->RaiseWidget(this->map_type + SM_WIDGET_CONTOUR);
				this->map_type = (SmallMapType)(widget - SM_WIDGET_CONTOUR);
				this->LowerWidget(this->map_type + SM_WIDGET_CONTOUR);

				this->ResizeLegend();

				this->SetDirty();
				SndPlayFx(SND_15_BEEP);
				break;

			case SM_WIDGET_CENTERMAP: // Center the smallmap again
				this->SmallMapCenterOnCurrentPos();

				this->SetDirty();
				SndPlayFx(SND_15_BEEP);
				break;

			case SM_WIDGET_TOGGLETOWNNAME: // Toggle town names
				this->show_towns = !this->show_towns;
				this->SetWidgetLoweredState(SM_WIDGET_TOGGLETOWNNAME, this->show_towns);

				this->SetDirty();
				SndPlayFx(SND_15_BEEP);
				break;

			case SM_WIDGET_LEGEND: // Legend
				/* if industry type small map*/
				if (this->map_type == SMT_INDUSTRY) {
					/* if click on industries label, find right industry type and enable/disable it */
					Widget *wi = &this->widget[SM_WIDGET_LEGEND]; // label panel
					uint column = (pt.x - 4) / COLUMN_WIDTH;
					uint line = (pt.y - wi->top - 2) / 6;
					int rows_per_column = (wi->bottom - wi->top) / 6;

					/* check if click is on industry label*/
					int industry_pos = (column * rows_per_column) + line;
					if (industry_pos < _smallmap_industry_count) {
						_legend_from_industries[industry_pos].show_on_map = !_legend_from_industries[industry_pos].show_on_map;
					}

					/* Raise the two buttons "all", as we have done a specific choice */
					this->RaiseWidget(SM_WIDGET_ENABLEINDUSTRIES);
					this->RaiseWidget(SM_WIDGET_DISABLEINDUSTRIES);
					this->SetDirty();
				}
				break;

			case SM_WIDGET_ENABLEINDUSTRIES: // Enable all industries
				for (int i = 0; i != _smallmap_industry_count; i++) {
					_legend_from_industries[i].show_on_map = true;
				}
				/* toggle appeareance indicating the choice */
				this->LowerWidget(SM_WIDGET_ENABLEINDUSTRIES);
				this->RaiseWidget(SM_WIDGET_DISABLEINDUSTRIES);
				this->SetDirty();
				break;

			case SM_WIDGET_DISABLEINDUSTRIES: // disable all industries
				for (int i = 0; i != _smallmap_industry_count; i++) {
					_legend_from_industries[i].show_on_map = false;
				}
				/* toggle appeareance indicating the choice */
				this->RaiseWidget(SM_WIDGET_ENABLEINDUSTRIES);
				this->LowerWidget(SM_WIDGET_DISABLEINDUSTRIES);
				this->SetDirty();
				break;
		}
	}

	virtual void OnRightClick(Point pt, int widget)
	{
		if (widget == SM_WIDGET_MAP) {
			if (_scrolling_viewport) return;
			_scrolling_viewport = true;
			_cursor.delta.x = 0;
			_cursor.delta.y = 0;
		}
	}

	virtual void OnTick()
	{
		/* update the window every now and then */
		if ((++this->refresh & 0x1F) == 0) this->SetDirty();
	}

	virtual void OnScroll(Point delta)
	{
		_cursor.fix_at = true;

		int x = this->scroll_x;
		int y = this->scroll_y;

		int sub = this->subscroll + delta.x;

		x -= (sub >> 2) << 4;
		y += (sub >> 2) << 4;
		sub &= 3;

		x += (delta.y >> 1) << 4;
		y += (delta.y >> 1) << 4;

		if (delta.y & 1) {
			x += TILE_SIZE;
			sub += 2;
			if (sub > 3) {
				sub -= 4;
				x -= TILE_SIZE;
				y += TILE_SIZE;
			}
		}

		int hx = (this->widget[SM_WIDGET_MAP].right  - this->widget[SM_WIDGET_MAP].left) / 2;
		int hy = (this->widget[SM_WIDGET_MAP].bottom - this->widget[SM_WIDGET_MAP].top ) / 2;
		int hvx = hx * -4 + hy * 8;
		int hvy = hx *  4 + hy * 8;
		if (x < -hvx) {
			x = -hvx;
			sub = 0;
		}
		if (x > (int)MapMaxX() * TILE_SIZE - hvx) {
			x = MapMaxX() * TILE_SIZE - hvx;
			sub = 0;
		}
		if (y < -hvy) {
			y = -hvy;
			sub = 0;
		}
		if (y > (int)MapMaxY() * TILE_SIZE - hvy) {
			y = MapMaxY() * TILE_SIZE - hvy;
			sub = 0;
		}

		this->scroll_x = x;
		this->scroll_y = y;
		this->subscroll = sub;

		this->SetDirty();
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		if (delta.x != 0 && this->map_type == SMT_INDUSTRY) this->ResizeLegend();
	}
};

SmallMapWindow::SmallMapType SmallMapWindow::map_type = SMT_CONTOUR;
bool SmallMapWindow::show_towns = true;

static const WindowDesc _smallmap_desc(
	WDP_AUTO, WDP_AUTO, 350, 214, 446, 314,
	WC_SMALLMAP, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_smallmap_widgets
);

void ShowSmallMap()
{
	AllocateWindowDescFront<SmallMapWindow>(&_smallmap_desc, 0);
}

/* Extra ViewPort Window Stuff */
static const Widget _extra_view_port_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,    0,   13, STR_00C5,                         STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,  COLOUR_GREY,    11,   287,    0,   13, STR_EXTRA_VIEW_PORT_TITLE,        STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,  COLOUR_GREY,   288,   299,    0,   13, 0x0,                              STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_RB,  COLOUR_GREY,     0,   299,   14,   33, 0x0,                              STR_NULL},
{      WWT_INSET,     RESIZE_RB,  COLOUR_GREY,     2,   297,   16,   31, 0x0,                              STR_NULL},
{ WWT_PUSHIMGBTN,     RESIZE_TB,  COLOUR_GREY,     0,    21,   34,   55, SPR_IMG_ZOOMIN,                   STR_017F_ZOOM_THE_VIEW_IN},
{ WWT_PUSHIMGBTN,     RESIZE_TB,  COLOUR_GREY,    22,    43,   34,   55, SPR_IMG_ZOOMOUT,                  STR_0180_ZOOM_THE_VIEW_OUT},
{ WWT_PUSHTXTBTN,     RESIZE_TB,  COLOUR_GREY,    44,   171,   34,   55, STR_EXTRA_VIEW_MOVE_MAIN_TO_VIEW, STR_EXTRA_VIEW_MOVE_MAIN_TO_VIEW_TT},
{ WWT_PUSHTXTBTN,     RESIZE_TB,  COLOUR_GREY,   172,   298,   34,   55, STR_EXTRA_VIEW_MOVE_VIEW_TO_MAIN, STR_EXTRA_VIEW_MOVE_VIEW_TO_MAIN_TT},
{      WWT_PANEL,    RESIZE_RTB,  COLOUR_GREY,   299,   299,   34,   55, 0x0,                              STR_NULL},
{      WWT_PANEL,    RESIZE_RTB,  COLOUR_GREY,     0,   287,   56,   67, 0x0,                              STR_NULL},
{  WWT_RESIZEBOX,   RESIZE_LRTB,  COLOUR_GREY,   288,   299,   56,   67, 0x0,                              STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

class ExtraViewportWindow : public Window
{
	enum ExtraViewportWindowWidgets {
		EVW_CLOSE,
		EVW_CAPTION,
		EVW_STICKY,
		EVW_BACKGROUND,
		EVW_VIEWPORT,
		EVW_ZOOMIN,
		EVW_ZOOMOUT,
		EVW_MAIN_TO_VIEW,
		EVW_VIEW_TO_MAIN,
		EVW_SPACER1,
		EVW_SPACER2,
		EVW_RESIZE,
	};

public:
	ExtraViewportWindow(const WindowDesc *desc, int window_number, TileIndex tile) : Window(desc, window_number)
	{
		/* New viewport start at (zero,zero) */
		InitializeWindowViewport(this, 3, 17, this->widget[EVW_VIEWPORT].right - this->widget[EVW_VIEWPORT].left - 1, this->widget[EVW_VIEWPORT].bottom - this->widget[EVW_VIEWPORT].top - 1, 0, ZOOM_LVL_VIEWPORT);

		this->DisableWidget(EVW_ZOOMIN);
		this->FindWindowPlacementAndResize(desc);

		Point pt;
		if (tile == INVALID_TILE) {
			/* the main window with the main view */
			const Window *w = FindWindowById(WC_MAIN_WINDOW, 0);

			/* center on same place as main window (zoom is maximum, no adjustment needed) */
			pt.x = w->viewport->scrollpos_x + w->viewport->virtual_height / 2;
			pt.y = w->viewport->scrollpos_y + w->viewport->virtual_height / 2;
		} else {
			pt = RemapCoords(TileX(tile) * TILE_SIZE + TILE_SIZE / 2, TileY(tile) * TILE_SIZE + TILE_SIZE / 2, TileHeight(tile));
		}

		this->viewport->scrollpos_x = pt.x - ((this->widget[EVW_VIEWPORT].right - this->widget[EVW_VIEWPORT].left) - 1) / 2;
		this->viewport->scrollpos_y = pt.y - ((this->widget[EVW_VIEWPORT].bottom - this->widget[EVW_VIEWPORT].top) - 1) / 2;
		this->viewport->dest_scrollpos_x = this->viewport->scrollpos_x;
		this->viewport->dest_scrollpos_y = this->viewport->scrollpos_y;

	}

	virtual void OnPaint()
	{
		/* set the number in the title bar */
		SetDParam(0, this->window_number + 1);

		this->DrawWidgets();
		this->DrawViewport();
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case EVW_ZOOMIN: DoZoomInOutWindow(ZOOM_IN,  this); break;
			case EVW_ZOOMOUT: DoZoomInOutWindow(ZOOM_OUT, this); break;

			case EVW_MAIN_TO_VIEW: { // location button (move main view to same spot as this view) 'Paste Location'
				Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
				int x = this->viewport->scrollpos_x; // Where is the main looking at
				int y = this->viewport->scrollpos_y;

				/* set this view to same location. Based on the center, adjusting for zoom */
				w->viewport->dest_scrollpos_x =  x - (w->viewport->virtual_width -  this->viewport->virtual_width) / 2;
				w->viewport->dest_scrollpos_y =  y - (w->viewport->virtual_height - this->viewport->virtual_height) / 2;
				w->viewport->follow_vehicle   = INVALID_VEHICLE;
			} break;

			case EVW_VIEW_TO_MAIN: { // inverse location button (move this view to same spot as main view) 'Copy Location'
				const Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
				int x = w->viewport->scrollpos_x;
				int y = w->viewport->scrollpos_y;

				this->viewport->dest_scrollpos_x =  x + (w->viewport->virtual_width -  this->viewport->virtual_width) / 2;
				this->viewport->dest_scrollpos_y =  y + (w->viewport->virtual_height - this->viewport->virtual_height) / 2;
			} break;
		}
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		this->viewport->width          += delta.x;
		this->viewport->height         += delta.y;
		this->viewport->virtual_width  += delta.x;
		this->viewport->virtual_height += delta.y;
	}

	virtual void OnScroll(Point delta)
	{
		const ViewPort *vp = IsPtInWindowViewport(this, _cursor.pos.x, _cursor.pos.y);
		if (vp == NULL) return;

		this->viewport->scrollpos_x += ScaleByZoom(delta.x, vp->zoom);
		this->viewport->scrollpos_y += ScaleByZoom(delta.y, vp->zoom);
		this->viewport->dest_scrollpos_x = this->viewport->scrollpos_x;
		this->viewport->dest_scrollpos_y = this->viewport->scrollpos_y;
	}

	virtual void OnMouseWheel(int wheel)
	{
		ZoomInOrOutToCursorWindow(wheel < 0, this);
	}

	virtual void OnInvalidateData(int data = 0)
	{
		/* Only handle zoom message if intended for us (msg ZOOM_IN/ZOOM_OUT) */
		HandleZoomMessage(this, this->viewport, EVW_ZOOMIN, EVW_ZOOMOUT);
	}
};

static const WindowDesc _extra_view_port_desc(
	WDP_AUTO, WDP_AUTO, 300, 68, 300, 268,
	WC_EXTRA_VIEW_PORT, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_extra_view_port_widgets
);

void ShowExtraViewPortWindow(TileIndex tile)
{
	int i = 0;

	/* find next free window number for extra viewport */
	while (FindWindowById(WC_EXTRA_VIEW_PORT, i) != NULL) i++;

	new ExtraViewportWindow(&_extra_view_port_desc, i, tile);
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
	 *  that tile (e.g.: pressed twice), move the smallmap to that location,
	 *  so you directly see where you are on the smallmap. */

	if (res) return res;

	SmallMapWindow *w = dynamic_cast<SmallMapWindow*>(FindWindowById(WC_SMALLMAP, 0));
	if (w != NULL) w->SmallMapCenterOnCurrentPos();

	return res;
}
