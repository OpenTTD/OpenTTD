/* $Id$ */

/** @file smallmap_gui.cpp GUI that shows a small map of the world with metadata like owner or height. */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "clear_map.h"
#include "industry_map.h"
#include "industry.h"
#include "station_map.h"
#include "landscape.h"
#include "gui.h"
#include "window_gui.h"
#include "tree_map.h"
#include "tunnel_map.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "player_base.h"
#include "town.h"
#include "variables.h"
#include "blitter/factory.hpp"
#include "tunnelbridge_map.h"
#include "strings_func.h"
#include "zoom_func.h"
#include "core/endian_func.hpp"
#include "vehicle_base.h"
#include "sound_func.h"
#include "settings_type.h"
#include "window_func.h"

#include "table/strings.h"
#include "table/sprites.h"

static const Widget _smallmap_widgets[] = {
{  WWT_CLOSEBOX,   RESIZE_NONE,    13,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW},
{   WWT_CAPTION,  RESIZE_RIGHT,    13,    11,   337,     0,    13, STR_00B0_MAP,            STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_STICKYBOX,     RESIZE_LR,    13,   338,   349,     0,    13, 0x0,                     STR_STICKY_BUTTON},
{     WWT_PANEL,     RESIZE_RB,    13,     0,   349,    14,   157, 0x0,                     STR_NULL},
{     WWT_INSET,     RESIZE_RB,    13,     2,   347,    16,   155, 0x0,                     STR_NULL},
{    WWT_IMGBTN,   RESIZE_LRTB,    13,   284,   305,   158,   179, SPR_IMG_SHOW_COUNTOURS,  STR_0191_SHOW_LAND_CONTOURS_ON_MAP},
{    WWT_IMGBTN,   RESIZE_LRTB,    13,   306,   327,   158,   179, SPR_IMG_SHOW_VEHICLES,   STR_0192_SHOW_VEHICLES_ON_MAP},
{    WWT_IMGBTN,   RESIZE_LRTB,    13,   328,   349,   158,   179, SPR_IMG_INDUSTRY,        STR_0193_SHOW_INDUSTRIES_ON_MAP},
{    WWT_IMGBTN,   RESIZE_LRTB,    13,   284,   307,   180,   201, SPR_IMG_SHOW_ROUTES,     STR_0194_SHOW_TRANSPORT_ROUTES_ON},
{    WWT_IMGBTN,   RESIZE_LRTB,    13,   306,   327,   180,   201, SPR_IMG_PLANTTREES,      STR_0195_SHOW_VEGETATION_ON_MAP},
{    WWT_IMGBTN,   RESIZE_LRTB,    13,   328,   349,   180,   201, SPR_IMG_COMPANY_GENERAL, STR_0196_SHOW_LAND_OWNERS_ON_MAP},
{    WWT_IMGBTN,   RESIZE_LRTB,    13,   262,   283,   158,   179, SPR_IMG_SMALLMAP,        STR_SMALLMAP_CENTER},
{    WWT_IMGBTN,   RESIZE_LRTB,    13,   262,   283,   180,   201, SPR_IMG_TOWN,            STR_0197_TOGGLE_TOWN_NAMES_ON_OFF},
{     WWT_PANEL,    RESIZE_RTB,    13,     0,   261,   158,   201, 0x0,                     STR_NULL},
{     WWT_PANEL,   RESIZE_LRTB,    13,   262,   349,   202,   202, 0x0,                     STR_NULL},
{     WWT_PANEL,    RESIZE_RTB,    13,     0,   337,   202,   213, 0x0,                     STR_NULL},
{   WWT_TEXTBTN,     RESIZE_TB,    13,     0,    99,   202,   213, STR_MESSAGES_ENABLE_ALL, STR_NULL},
{   WWT_TEXTBTN,     RESIZE_TB,    13,   100,   201,   202,   213, STR_MESSAGES_DISABLE_ALL,STR_NULL},
{ WWT_RESIZEBOX,   RESIZE_LRTB,    13,   338,   349,   202,   213, 0x0,                     STR_RESIZE_BUTTON},
{  WIDGETS_END},
};

static int _smallmap_type;
static bool _smallmap_show_towns = true;
/* number of used industries */
static int _smallmap_industry_count;
/* number of industries per column*/
static uint _industries_per_column;

/** Macro for ordinary entry of LegendAndColor */
#define MK(a,b) {a, b, INVALID_INDUSTRYTYPE, true, false, false}
/** Macro for end of list marker in arrays of LegendAndColor */
#define MKEND() {0, STR_NULL, INVALID_INDUSTRYTYPE, true, true, false}
/** Macro for break marker in arrays of LegendAndColor.
 * It will have valid data, though */
#define MS(a,b) {a, b, INVALID_INDUSTRYTYPE, true, false, true}

/** Structure for holding relevant data for legends in small map */
struct LegendAndColour {
	uint16 colour;     ///< color of the item on the map
	StringID legend;   ///< string corresponding to the colored item
	IndustryType type; ///< type of industry
	bool show_on_map;  ///< for filtering industries, if true is shown on map in color
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
static LegendAndColour _legend_from_industries[NUM_INDUSTRYTYPES+1];
/* For connecting industry type to position in industries list(small map legend) */
static uint _industry_to_list_pos[NUM_INDUSTRYTYPES];

/**
 * Fills an array for the industries legends.
 */
void BuildIndustriesLegend()
{
	const IndustrySpec *indsp;
	uint j = 0;
	uint free_slot, diff;

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

	_industries_per_column = _smallmap_industry_count / 3;
	free_slot = _smallmap_industry_count % 3;

	/* recalculate column break for first two columns(i) */
	diff = 0;
	for (int i = 1; i <= 2; i++) {
		if (free_slot > 0) diff = diff + 1;
		_legend_from_industries[i * _industries_per_column + diff].col_break = true;
		if (free_slot > 0) free_slot--;
	}

}

static const LegendAndColour * const _legend_table[] = {
	_legend_land_contours,
	_legend_vehicles,
	_legend_from_industries,
	_legend_routes,
	_legend_vegetation,
	_legend_land_owners,
};

#define MKCOLOR(x) TO_LE32X(x)

/**
 * Height encodings; MAX_TILE_HEIGHT + 1 levels, from 0 to MAX_TILE_HEIGHT
 */
static const uint32 _map_height_bits[] = {
	MKCOLOR(0x5A5A5A5A),
	MKCOLOR(0x5A5B5A5B),
	MKCOLOR(0x5B5B5B5B),
	MKCOLOR(0x5B5C5B5C),
	MKCOLOR(0x5C5C5C5C),
	MKCOLOR(0x5C5D5C5D),
	MKCOLOR(0x5D5D5D5D),
	MKCOLOR(0x5D5E5D5E),
	MKCOLOR(0x5E5E5E5E),
	MKCOLOR(0x5E5F5E5F),
	MKCOLOR(0x5F5F5F5F),
	MKCOLOR(0x5F1F5F1F),
	MKCOLOR(0x1F1F1F1F),
	MKCOLOR(0x1F271F27),
	MKCOLOR(0x27272727),
	MKCOLOR(0x27272727),
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
	{MKCOLOR(0x00000000), MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0x000A0A00), MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00D7D700), MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00B5B500), MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00000000), MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0x98989898), MKCOLOR(0x00000000)},
	{MKCOLOR(0xCACACACA), MKCOLOR(0x00000000)},
	{MKCOLOR(0x00000000), MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0xB5B5B5B5), MKCOLOR(0x00000000)},
	{MKCOLOR(0x00000000), MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0x00B5B500), MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x000A0A00), MKCOLOR(0xFF0000FF)},
};

static const AndOr _smallmap_vehicles_andor[] = {
	{MKCOLOR(0x00000000), MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0x00D7D700), MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00D7D700), MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00B5B500), MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00000000), MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0x00D7D700), MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0xCACACACA), MKCOLOR(0x00000000)},
	{MKCOLOR(0x00000000), MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0xB5B5B5B5), MKCOLOR(0x00000000)},
	{MKCOLOR(0x00000000), MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0x00B5B500), MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00D7D700), MKCOLOR(0xFF0000FF)},
};

static const AndOr _smallmap_vegetation_andor[] = {
	{MKCOLOR(0x00000000), MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0x00D7D700), MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00D7D700), MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00B5B500), MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00575700), MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00D7D700), MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0xCACACACA), MKCOLOR(0x00000000)},
	{MKCOLOR(0x00000000), MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0xB5B5B5B5), MKCOLOR(0x00000000)},
	{MKCOLOR(0x00000000), MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0x00B5B500), MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00D7D700), MKCOLOR(0xFF0000FF)},
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
		if (xc < MapMaxX() && yc < MapMaxY()) {
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
 * Return the color a tile would be displayed with in the small map in mode "Contour".
 * @param tile The tile of which we would like to get the color.
 * @return The color of tile in the small map in mode "Contour"
 */
static inline uint32 GetSmallMapContoursPixels(TileIndex tile)
{
	TileType t = GetEffectiveTileType(tile);

	return
		ApplyMask(_map_height_bits[TileHeight(tile)], &_smallmap_contours_andor[t]);
}

/**
 * Return the color a tile would be displayed with in the small map in mode "Vehicles".
 *
 * @param tile The tile of which we would like to get the color.
 * @return The color of tile in the small map in mode "Vehicles"
 */
static inline uint32 GetSmallMapVehiclesPixels(TileIndex tile)
{
	TileType t = GetEffectiveTileType(tile);

	return ApplyMask(MKCOLOR(0x54545454), &_smallmap_vehicles_andor[t]);
}

/**
 * Return the color a tile would be displayed with in the small map in mode "Industries".
 *
 * @param tile The tile of which we would like to get the color.
 * @return The color of tile in the small map in mode "Industries"
 */
static inline uint32 GetSmallMapIndustriesPixels(TileIndex tile)
{
	TileType t = GetEffectiveTileType(tile);

	if (t == MP_INDUSTRY) {
		/* If industry is allowed to be seen, use its color on the map */
		if (_legend_from_industries[_industry_to_list_pos[GetIndustryByTile(tile)->type]].show_on_map) {
			return GetIndustrySpec(GetIndustryByTile(tile)->type)->map_colour * 0x01010101;
		} else {
			/* otherwise, return the color of the clear tiles, which will make it disappear */
			return ApplyMask(MKCOLOR(0x54545454), &_smallmap_vehicles_andor[MP_CLEAR]);
		}
	}

	return ApplyMask(MKCOLOR(0x54545454), &_smallmap_vehicles_andor[t]);
}

/**
 * Return the color a tile would be displayed with in the small map in mode "Routes".
 *
 * @param tile The tile of which we would like to get the color.
 * @return The color of tile  in the small map in mode "Routes"
 */
static inline uint32 GetSmallMapRoutesPixels(TileIndex tile)
{
	TileType t = GetEffectiveTileType(tile);
	uint32 bits;

	if (t == MP_STATION) {
		switch (GetStationType(tile)) {
			case STATION_RAIL:    bits = MKCOLOR(0x56565656); break;
			case STATION_AIRPORT: bits = MKCOLOR(0xB8B8B8B8); break;
			case STATION_TRUCK:   bits = MKCOLOR(0xC2C2C2C2); break;
			case STATION_BUS:     bits = MKCOLOR(0xBFBFBFBF); break;
			case STATION_DOCK:    bits = MKCOLOR(0x98989898); break;
			default:              bits = MKCOLOR(0xFFFFFFFF); break;
		}
	} else {
		/* ground color */
		bits = ApplyMask(MKCOLOR(0x54545454), &_smallmap_contours_andor[t]);
	}
	return bits;
}


static const uint32 _vegetation_clear_bits[] = {
	MKCOLOR(0x54545454), ///< full grass
	MKCOLOR(0x52525252), ///< rough land
	MKCOLOR(0x0A0A0A0A), ///< rocks
	MKCOLOR(0x25252525), ///< fields
	MKCOLOR(0x98989898), ///< snow
	MKCOLOR(0xC2C2C2C2), ///< desert
	MKCOLOR(0x54545454), ///< unused
	MKCOLOR(0x54545454), ///< unused
};

static inline uint32 GetSmallMapVegetationPixels(TileIndex tile)
{
	TileType t = GetEffectiveTileType(tile);
	uint32 bits;

	switch (t) {
		case MP_CLEAR:
			if (IsClearGround(tile, CLEAR_GRASS) && GetClearDensity(tile) < 3) {
				bits = MKCOLOR(0x37373737);
			} else {
				bits = _vegetation_clear_bits[GetClearGround(tile)];
			}
			break;

		case MP_INDUSTRY:
			bits = GetIndustrySpec(GetIndustryByTile(tile)->type)->check_proc == CHECK_FOREST ? MKCOLOR(0xD0D0D0D0) : MKCOLOR(0xB5B5B5B5);
			break;

		case MP_TREES:
			if (GetTreeGround(tile) == TREE_GROUND_SNOW_DESERT) {
				bits = (_settings.game_creation.landscape == LT_ARCTIC) ? MKCOLOR(0x98575798) : MKCOLOR(0xC25757C2);
			} else {
				bits = MKCOLOR(0x54575754);
			}
			break;

		default:
			bits = ApplyMask(MKCOLOR(0x54545454), &_smallmap_vehicles_andor[t]);
			break;
	}

	return bits;
}


static uint32 _owner_colors[OWNER_END + 1];

/**
 * Return the color a tile would be displayed with in the small map in mode "Owner".
 *
 * @param tile The tile of which we would like to get the color.
 * @return The color of tile in the small map in mode "Owner"
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

	return _owner_colors[o];
}


static const uint32 _smallmap_mask_left[3] = {
	MKCOLOR(0xFF000000),
	MKCOLOR(0xFFFF0000),
	MKCOLOR(0xFFFFFF00),
};

static const uint32 _smallmap_mask_right[] = {
	MKCOLOR(0x000000FF),
	MKCOLOR(0x0000FFFF),
	MKCOLOR(0x00FFFFFF),
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

static const byte _vehicle_type_colors[6] = {
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
	SM_WIDGET_MAP = 4,
	SM_WIDGET_CONTOUR,
	SM_WIDGET_VEHICLES,
	SM_WIDGET_INDUSTRIES,
	SM_WIDGET_ROUTES,
	SM_WIDGET_VEGETATION,
	SM_WIDGET_OWNERS,
	SM_WIDGET_CENTERMAP,
	SM_WIDGET_TOGGLETOWNNAME,
	SM_WIDGET_LEGEND,
	SM_WIDGET_BUTTONSPANEL,
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
		SMT_OWNER = 5,
	};

	enum {
		BASE_NB_PER_COLUMN = 6,
	};

	int32 scroll_x;
	int32 scroll_y;
	int32 subscroll;

public:
	/**
	 * Draws the small map.
	 *
	 * Basically, the small map is draw column of pixels by column of pixels. The pixels
	 * are drawn directly into the screen buffer. The final map is drawn in multiple passes.
	 * The passes are:
	 * <ol><li>The colors of tiles in the different modes.</li>
	 * <li>Town names (optional)</li></ol>
	 *
	 * @param dpi pointer to pixel to write onto
	 * @param w pointer to Window struct
	 * @param type type of map requested (vegetation, owners, routes, etc)
	 * @param show_towns true if the town names should be displayed, false if not.
	 */
	void DrawSmallMap(DrawPixelInfo *dpi, int type, bool show_towns)
	{
		Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
		DrawPixelInfo *old_dpi;
		int dx,dy, x, y, x2, y2;
		void *ptr;
		int tile_x;
		int tile_y;
		ViewPort *vp;

		old_dpi = _cur_dpi;
		_cur_dpi = dpi;

		/* clear it */
		GfxFillRect(dpi->left, dpi->top, dpi->left + dpi->width - 1, dpi->top + dpi->height - 1, 0);

		/* setup owner table */
		if (type == SMT_OWNER) {
			const Player *p;

			/* fill with some special colors */
			_owner_colors[OWNER_TOWN] = MKCOLOR(0xB4B4B4B4);
			_owner_colors[OWNER_NONE] = MKCOLOR(0x54545454);
			_owner_colors[OWNER_WATER] = MKCOLOR(0xCACACACA);
			_owner_colors[OWNER_END]   = MKCOLOR(0x20202020); /* industry */

			/* now fill with the player colors */
			FOR_ALL_PLAYERS(p) {
				if (p->is_active) {
					_owner_colors[p->index] =
						_colour_gradient[p->player_color][5] * 0x01010101;
				}
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
				if (t <= 0) break; /* exit loop */
				/* mask to use at the right edge */
				mask &= _smallmap_mask_right[t - 1];
			}

			/* number of lines */
			reps = (dpi->height - y + 1) / 2;
			if (reps > 0) {
				DrawSmallMapStuff(ptr, tile_x, tile_y, dpi->pitch * 2, reps, mask, _smallmap_draw_procs[type]);
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
		if (type == SMT_CONTOUR || type == SMT_VEHICLES) {
			Vehicle *v;
			bool skip;
			byte color;

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

					/* Calculate pointer to pixel and the color */
					color = (type == SMT_VEHICLES) ? _vehicle_type_colors[v->type] : 0xF;

					/* And draw either one or two pixels depending on clipping */
					blitter->SetPixel(dpi->dst_ptr, x, y, color);
					if (!skip) blitter->SetPixel(dpi->dst_ptr, x + 1, y, color);
				}
			}
		}

		if (show_towns) {
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
			vp = FindWindowById(WC_MAIN_WINDOW,0)->viewport;

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

	SmallMapWindow(const WindowDesc *desc, int window_number) : Window(desc, window_number)
	{
		/* Resize the window to fit industries list */
		if (_industries_per_column > BASE_NB_PER_COLUMN) {
			uint diff = ((_industries_per_column - BASE_NB_PER_COLUMN) * BASE_NB_PER_COLUMN) + 1;

			this->height = this->height + diff;

			Widget *wi = &this->widget[SM_WIDGET_LEGEND]; // label panel
			wi->bottom = wi->bottom + diff;

			wi = &this->widget[SM_WIDGET_BUTTONSPANEL]; // filler panel under smallmap buttons
			wi->bottom = wi->bottom + diff - 1;

			/* Change widget position
			 * - footer panel
			 * - enable all industry
			 * - disable all industry
			 * - resize window button
			 */
			for (uint i = SM_WIDGET_BOTTOMPANEL; i <= SM_WIDGET_RESIZEBOX; i++) {
				wi           = &this->widget[i];
				wi->top      = wi->top + diff;
				wi->bottom   = wi->bottom + diff;
			}
		}

		this->LowerWidget(_smallmap_type + SMT_OWNER);
		this->SetWidgetLoweredState(SM_WIDGET_TOGGLETOWNNAME, _smallmap_show_towns);

		this->SmallMapCenterOnCurrentPos();
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		const LegendAndColour *tbl;
		int x, y, y_org;
		uint diff;
		DrawPixelInfo new_dpi;

		/* Hide Enable all/Disable all buttons if is not industry type small map*/
		this->SetWidgetHiddenState(SM_WIDGET_ENABLEINDUSTRIES, _smallmap_type != SMT_INDUSTRY);
		this->SetWidgetHiddenState(SM_WIDGET_DISABLEINDUSTRIES, _smallmap_type != SMT_INDUSTRY);

		/* draw the window */
		SetDParam(0, STR_00E5_CONTOURS + _smallmap_type);
		this->DrawWidgets();

		tbl = _legend_table[_smallmap_type];

		/* difference in window size */
		diff = (_industries_per_column > BASE_NB_PER_COLUMN) ? ((_industries_per_column - BASE_NB_PER_COLUMN) * BASE_NB_PER_COLUMN) + 1 : 0;

		x = 4;
		y_org = this->height - 44 - 11 - diff;
		y = y_org;

		for (;;) {
			if (_smallmap_type == SMT_INDUSTRY) {
				/* Industry name must be formated, since it's not in tiny font in the specs.
				 * So, draw with a parameter and use the STR_SMALLMAP_INDUSTRY string, which is tiny font.*/
				SetDParam(0, tbl->legend);
				assert(tbl->type < NUM_INDUSTRYTYPES);
				SetDParam(1, _industry_counts[tbl->type]);
				if (!tbl->show_on_map) {
					/* Simply draw the string, not the black border of the legend color.
					 * This will enforce the idea of the disabled item */
					DrawString(x + 11, y, STR_SMALLMAP_INDUSTRY, TC_GREY);
				} else {
					DrawString(x + 11, y, STR_SMALLMAP_INDUSTRY, TC_BLACK);
					GfxFillRect(x, y + 1, x + 8, y + 5, 0); // outer border of the legend color
				}
			} else {
				/* Anything that is not an industry is using normal process */
				GfxFillRect(x, y + 1, x + 8, y + 5, 0);
				DrawString(x + 11, y, tbl->legend, TC_FROMSTRING);
			}
			GfxFillRect(x + 1, y + 2, x + 7, y + 4, tbl->colour); // legend color

			tbl += 1;
			y += 6;

			if (tbl->end) { // end of the list
				break;
			} else if (tbl->col_break) {
				/*  break asked, continue at top, 123 pixels (one "row") to the right */
				x += 123;
				y = y_org;
			}
		}

		if (!FillDrawPixelInfo(&new_dpi, 3, 17, this->width - 28 + 22, this->height - 64 - 11 - diff)) return;

		this->DrawSmallMap(&new_dpi, _smallmap_type, _smallmap_show_towns);
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
				this->RaiseWidget(_smallmap_type + SM_WIDGET_CONTOUR);
				_smallmap_type = widget - SM_WIDGET_CONTOUR;
				this->LowerWidget(_smallmap_type + SM_WIDGET_CONTOUR);

				this->SetDirty();
				SndPlayFx(SND_15_BEEP);
				break;

			case SM_WIDGET_CENTERMAP: // Center the smallmap again
				this->SmallMapCenterOnCurrentPos();

				this->SetDirty();
				SndPlayFx(SND_15_BEEP);
				break;

			case SM_WIDGET_TOGGLETOWNNAME: // Toggle town names
				this->ToggleWidgetLoweredState(SM_WIDGET_TOGGLETOWNNAME);
				_smallmap_show_towns = this->IsWidgetLowered(SM_WIDGET_TOGGLETOWNNAME);

				this->SetDirty();
				SndPlayFx(SND_15_BEEP);
				break;

			case SM_WIDGET_LEGEND: // Legend
				/* if industry type small map*/
				if (_smallmap_type == SMT_INDUSTRY) {
					/* if click on industries label, find right industry type and enable/disable it */
					Widget *wi = &this->widget[SM_WIDGET_LEGEND]; // label panel
					uint column = (pt.x - 4) / 123;
					uint line = (pt.y - wi->top - 2) / 6;
					uint free = _smallmap_industry_count % 3;

					if (column <= 3) {
						/* check if click is on industry label*/
						uint industry_pos = 0;
						for (uint i = 0; i <= column; i++) {
							uint diff = (free > 0) ? 1 : 0;
							uint max_column_lines = _industries_per_column + diff;

							if (i < column) industry_pos = industry_pos + _industries_per_column + diff;

							if (i == column && line <= max_column_lines - 1) {
								industry_pos = industry_pos + line;
								_legend_from_industries[industry_pos].show_on_map = !_legend_from_industries[industry_pos].show_on_map;
							}
							if( free > 0) free--;
						}
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
		if ((++this->vscroll.pos & 0x1F) == 0) this->SetDirty();
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
};

static const WindowDesc _smallmap_desc = {
	WDP_AUTO, WDP_AUTO, 350, 214, 446, 314,
	WC_SMALLMAP, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_smallmap_widgets,
};

void ShowSmallMap()
{
	AllocateWindowDescFront<SmallMapWindow>(&_smallmap_desc, 0);
}

/* Extra ViewPort Window Stuff */
static const Widget _extra_view_port_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,    0,   13, STR_00C5,                         STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   287,    0,   13, STR_EXTRA_VIEW_PORT_TITLE,        STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   288,   299,    0,   13, 0x0,                              STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_RB,    14,     0,   299,   14,   33, 0x0,                              STR_NULL},
{      WWT_INSET,     RESIZE_RB,    14,     2,   297,   16,   31, 0x0,                              STR_NULL},
{ WWT_PUSHIMGBTN,     RESIZE_TB,    14,     0,    21,   34,   55, SPR_IMG_ZOOMIN,                   STR_017F_ZOOM_THE_VIEW_IN},
{ WWT_PUSHIMGBTN,     RESIZE_TB,    14,    22,    43,   34,   55, SPR_IMG_ZOOMOUT,                  STR_0180_ZOOM_THE_VIEW_OUT},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,    44,   171,   34,   55, STR_EXTRA_VIEW_MOVE_MAIN_TO_VIEW, STR_EXTRA_VIEW_MOVE_MAIN_TO_VIEW_TT},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   172,   298,   34,   55, STR_EXTRA_VIEW_MOVE_VIEW_TO_MAIN, STR_EXTRA_VIEW_MOVE_VIEW_TO_MAIN_TT},
{      WWT_PANEL,    RESIZE_RTB,    14,   299,   299,   34,   55, 0x0,                              STR_NULL},
{      WWT_PANEL,    RESIZE_RTB,    14,     0,   287,   56,   67, 0x0,                              STR_NULL},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   288,   299,   56,   67, 0x0,                              STR_RESIZE_BUTTON},
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
		ViewPort *vp = IsPtInWindowViewport(this, _cursor.pos.x, _cursor.pos.y);

		if (vp == NULL) {
			_cursor.fix_at = false;
			_scrolling_viewport = false;
		}

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

static const WindowDesc _extra_view_port_desc = {
	WDP_AUTO, WDP_AUTO, 300, 68, 300, 268,
	WC_EXTRA_VIEW_PORT, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_extra_view_port_widgets,
};

void ShowExtraViewPortWindow(TileIndex tile)
{
	int i = 0;

	/* find next free window number for extra viewport */
	while (FindWindowById(WC_EXTRA_VIEW_PORT, i) != NULL) i++;

	new ExtraViewportWindow(&_extra_view_port_desc, i, tile);
}

bool ScrollMainWindowTo(int x, int y, bool instant)
{
	bool res = ScrollWindowTo(x, y, FindWindowById(WC_MAIN_WINDOW, 0), instant);

	/* If a user scrolls to a tile (via what way what so ever) and already is on
	 *  that tile (e.g.: pressed twice), move the smallmap to that location,
	 *  so you directly see where you are on the smallmap. */

	if (res) return res;

	SmallMapWindow *w = dynamic_cast<SmallMapWindow*>(FindWindowById(WC_SMALLMAP, 0));
	if (w != NULL) w->SmallMapCenterOnCurrentPos();

	return res;
}
