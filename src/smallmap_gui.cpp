/* $Id$ */

/** @file smallmap_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "bridge_map.h"
#include "clear_map.h"
#include "industry_map.h"
#include "station_map.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "landscape.h"
#include "map.h"
#include "tile.h"
#include "gui.h"
#include "tree_map.h"
#include "tunnel_map.h"
#include "window.h"
#include "gfx.h"
#include "viewport.h"
#include "player.h"
#include "vehicle.h"
#include "town.h"
#include "sound.h"
#include "variables.h"

static const Widget _smallmap_widgets[] = {
{  WWT_CLOSEBOX,   RESIZE_NONE,    13,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW},
{   WWT_CAPTION,  RESIZE_RIGHT,    13,    11,   433,     0,    13, STR_00B0_MAP,            STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_STICKYBOX,     RESIZE_LR,    13,   434,   445,     0,    13, 0x0,                     STR_STICKY_BUTTON},
{     WWT_PANEL,     RESIZE_RB,    13,     0,   445,    14,   257, 0x0,                     STR_NULL},
{     WWT_INSET,     RESIZE_RB,    13,     2,   443,    16,   255, 0x0,                     STR_NULL},
{    WWT_IMGBTN,   RESIZE_LRTB,    13,   380,   401,   258,   279, SPR_IMG_SHOW_COUNTOURS,  STR_0191_SHOW_LAND_CONTOURS_ON_MAP},
{    WWT_IMGBTN,   RESIZE_LRTB,    13,   402,   423,   258,   279, SPR_IMG_SHOW_VEHICLES,   STR_0192_SHOW_VEHICLES_ON_MAP},
{    WWT_IMGBTN,   RESIZE_LRTB,    13,   424,   445,   258,   279, SPR_IMG_INDUSTRY,        STR_0193_SHOW_INDUSTRIES_ON_MAP},
{    WWT_IMGBTN,   RESIZE_LRTB,    13,   380,   401,   280,   301, SPR_IMG_SHOW_ROUTES,     STR_0194_SHOW_TRANSPORT_ROUTES_ON},
{    WWT_IMGBTN,   RESIZE_LRTB,    13,   402,   423,   280,   301, SPR_IMG_PLANTTREES,      STR_0195_SHOW_VEGETATION_ON_MAP},
{    WWT_IMGBTN,   RESIZE_LRTB,    13,   424,   445,   280,   301, SPR_IMG_COMPANY_GENERAL, STR_0196_SHOW_LAND_OWNERS_ON_MAP},
{    WWT_IMGBTN,   RESIZE_LRTB,    13,   358,   379,   258,   279, SPR_IMG_SMALLMAP,        STR_SMALLMAP_CENTER},
{    WWT_IMGBTN,   RESIZE_LRTB,    13,   358,   379,   280,   301, SPR_IMG_TOWN,            STR_0197_TOGGLE_TOWN_NAMES_ON_OFF},
{     WWT_PANEL,    RESIZE_RTB,    13,     0,   357,   258,   301, 0x0,                     STR_NULL},
{     WWT_PANEL,    RESIZE_RTB,    13,     0,   433,   302,   313, 0x0,                     STR_NULL},
{ WWT_RESIZEBOX,   RESIZE_LRTB,    13,   434,   445,   302,   313, 0x0,                     STR_RESIZE_BUTTON},
{  WIDGETS_END},
};

static int _smallmap_type;
static bool _smallmap_show_towns = true;

/** Macro for ordinary entry of LegendAndColor */
#define MK(a,b) {a, b, false, false}
/** Macro for end of list marker in arrays of LegendAndColor */
#define MKEND() {0, STR_NULL, true, false}
/** Macro for break marker in arrays of LegendAndColor.
 * It will have valid data, though */
#define MS(a,b) {a, b, false, true}

/** Structure for holding relevant data for legends in small map */
struct LegendAndColour {
	uint16 colour;     ///< color of the item on the map
	StringID legend;   ///< string corresponding to the colored item
	bool end;         ///< this is the end of the list
	bool col_break;   ///< perform a break and go one collumn further
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

static const LegendAndColour _legend_industries_normal[] = {
	MK(0xD7, STR_00FA_COAL_MINE),
	MK(0xB8, STR_00FB_POWER_STATION),
	MK(0x56, STR_00FC_FOREST),
	MK(0xC2, STR_00FD_SAWMILL),
	MK(0xBF, STR_00FE_OIL_REFINERY),
	MK(0x0F, STR_0105_BANK),

	MS(0x30, STR_00FF_FARM),
	MK(0xAE, STR_0100_FACTORY),
	MK(0x98, STR_0102_OIL_WELLS),
	MK(0x37, STR_0103_IRON_ORE_MINE),
	MK(0x0A, STR_0104_STEEL_MILL),
	MKEND()
};

static const LegendAndColour _legend_industries_hilly[] = {
	MK(0xD7, STR_00FA_COAL_MINE),
	MK(0xB8, STR_00FB_POWER_STATION),
	MK(0x56, STR_00FC_FOREST),
	MK(0x0A, STR_0106_PAPER_MILL),
	MK(0xBF, STR_00FE_OIL_REFINERY),
	MK(0x37, STR_0108_FOOD_PROCESSING_PLANT),
	MS(0x30, STR_00FF_FARM),

	MK(0xAE, STR_0101_PRINTING_WORKS),
	MK(0x98, STR_0102_OIL_WELLS),
	MK(0xC2, STR_0107_GOLD_MINE),
	MK(0x0F, STR_0105_BANK),
	MKEND()
};

static const LegendAndColour _legend_industries_desert[] = {
	MK(0xBF, STR_00FE_OIL_REFINERY),
	MK(0x98, STR_0102_OIL_WELLS),
	MK(0x0F, STR_0105_BANK),
	MK(0xB8, STR_0109_DIAMOND_MINE),
	MK(0x37, STR_0108_FOOD_PROCESSING_PLANT),
	MK(0x0A, STR_010A_COPPER_ORE_MINE),
	MK(0x30, STR_00FF_FARM),
	MS(0x56, STR_010B_FRUIT_PLANTATION),

	MK(0x27, STR_010C_RUBBER_PLANTATION),
	MK(0x25, STR_010D_WATER_SUPPLY),
	MK(0xD0, STR_010E_WATER_TOWER),
	MK(0xAE, STR_0100_FACTORY),
	MK(0xC2, STR_010F_LUMBER_MILL),
	MKEND()
};

static const LegendAndColour _legend_industries_candy[] = {
	MK(0x30, STR_0110_COTTON_CANDY_FOREST),
	MK(0xAE, STR_0111_CANDY_FACTORY),
	MK(0x27, STR_0112_BATTERY_FARM),
	MK(0x37, STR_0113_COLA_WELLS),
	MK(0xD0, STR_0114_TOY_SHOP),
	MK(0x0A, STR_0115_TOY_FACTORY),
	MS(0x25, STR_0116_PLASTIC_FOUNTAINS),

	MK(0xB8, STR_0117_FIZZY_DRINK_FACTORY),
	MK(0x98, STR_0118_BUBBLE_GENERATOR),
	MK(0xC2, STR_0119_TOFFEE_QUARRY),
	MK(0x0F, STR_011A_SUGAR_MINE),
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


enum { IND_OFFS = 6 };  ///< allow to "jump" to the industries corresponding to the landscape

static const LegendAndColour * const _legend_table[] = {
	_legend_land_contours,
	_legend_vehicles,
	NULL,
	_legend_routes,
	_legend_vegetation,
	_legend_land_owners,

	_legend_industries_normal,
	_legend_industries_hilly,
	_legend_industries_desert,
	_legend_industries_candy,
};

#if defined(OTTD_ALIGNMENT)
	static inline void WRITE_PIXELS(Pixel* d, uint32 val)
	{
#	if defined(TTD_BIG_ENDIAN)
		d[0] = GB(val, 24, 8);
		d[1] = GB(val, 16, 8);
		d[2] = GB(val,  8, 8);
		d[3] = GB(val,  0, 8);
#	elif defined(TTD_LITTLE_ENDIAN)
		d[0] = GB(val,  0, 8);
		d[1] = GB(val,  8, 8);
		d[2] = GB(val, 16, 8);
		d[3] = GB(val, 24, 8);
#	endif
	}

/* need to use OR, otherwise we will overwrite the wrong pixels at the edges :( */
	static inline void WRITE_PIXELS_OR(Pixel *d, uint32 val)
	{
#	if defined(TTD_BIG_ENDIAN)
		d[0] |= GB(val, 24, 8);
		d[1] |= GB(val, 16, 8);
		d[2] |= GB(val,  8, 8);
		d[3] |= GB(val,  0, 8);
#	elif defined(TTD_LITTLE_ENDIAN)
		d[0] |= GB(val,  0, 8);
		d[1] |= GB(val,  8, 8);
		d[2] |= GB(val, 16, 8);
		d[3] |= GB(val, 24, 8);
#	endif
	}
#else
#	define WRITE_PIXELS(dst, val)   *(uint32*)(dst) = (val);
#	define WRITE_PIXELS_OR(dst,val) *(uint32*)(dst) |= (val);
#endif

#define MKCOLOR(x) TO_LE32X(x)

/* Height encodings; 16 levels XXX - needs updating for more/finer heights! */
static const uint32 _map_height_bits[16] = {
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
static void DrawSmallMapStuff(Pixel *dst, uint xc, uint yc, int pitch, int reps, uint32 mask, GetSmallMapPixels *proc)
{
	Pixel *dst_ptr_end = _screen.dst_ptr + _screen.width * _screen.height - _screen.width;

	do {
		/* check if the tile (xc,yc) is within the map range */
		if (xc < MapMaxX() && yc < MapMaxY()) {
			/* check if the dst pointer points to a pixel inside the screen buffer */
			if (dst > _screen.dst_ptr && dst < dst_ptr_end)
				WRITE_PIXELS_OR(dst, proc(TileXY(xc, yc)) & mask);
		}
	/* switch to next tile in the column */
	} while (xc++, yc++, dst += pitch, --reps != 0);
}


static inline TileType GetEffectiveTileType(TileIndex tile)
{
	TileType t = GetTileType(tile);

	if (t == MP_TUNNELBRIDGE) {
		TransportType tt;

		if (IsTunnel(tile)) {
			tt = GetTunnelTransportType(tile);
		} else {
			tt = GetBridgeTransportType(tile);
		}
		switch (tt) {
			case TRANSPORT_RAIL: t = MP_RAILWAY; break;
			case TRANSPORT_ROAD: t = MP_STREET;  break;
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
		return GetIndustrySpec(GetIndustryByTile(tile)->type)->map_colour * 0x01010101;
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
			bits = GetIndustryType(tile) == IT_FOREST ? MKCOLOR(0xD0D0D0D0) : MKCOLOR(0xB5B5B5B5);
			break;

		case MP_TREES:
			if (GetTreeGround(tile) == TREE_GROUND_SNOW_DESERT) {
				bits = (_opt.landscape == LT_ARCTIC) ? MKCOLOR(0x98575798) : MKCOLOR(0xC25757C2);
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

/**
 * Draws the small map.
 *
 * Basically, the small map is draw column of pixels by column of pixels. The pixels
 * are drawn directly into the screen buffer. The final map is drawn in multiple passes.
 * The passes are:
 * <ol><li>The colors of tiles in the different modes.</li>
 * <li>Town names (optional)</li>
 *
 * @param dpi pointer to pixel to write onto
 * @param w pointer to Window struct
 * @param type type of map requested (vegetation, owners, routes, etc)
 * @param show_towns true if the town names should be displayed, false if not.
 */
static void DrawSmallMap(DrawPixelInfo *dpi, Window *w, int type, bool show_towns)
{
	DrawPixelInfo *old_dpi;
	int dx,dy, x, y, x2, y2;
	Pixel *ptr;
	int tile_x;
	int tile_y;
	ViewPort *vp;

	old_dpi = _cur_dpi;
	_cur_dpi = dpi;

	/* clear it */
	GfxFillRect(dpi->left, dpi->top, dpi->left + dpi->width - 1, dpi->top + dpi->height - 1, 0);

	/* setup owner table */
	if (type == 5) {
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

	tile_x = WP(w,smallmap_d).scroll_x / TILE_SIZE;
	tile_y = WP(w,smallmap_d).scroll_y / TILE_SIZE;

	dx = dpi->left + WP(w,smallmap_d).subscroll;
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

	ptr = dpi->dst_ptr - dx - 4;
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
//			assert(ptr >= dpi->dst_ptr);
			DrawSmallMapStuff(ptr, tile_x, tile_y, dpi->pitch * 2, reps, mask, _smallmap_draw_procs[type]);
		}

skip_column:
		if (y == 0) {
			tile_y++;
			y++;
			ptr += dpi->pitch;
		} else {
			tile_x--;
			y--;
			ptr -= dpi->pitch;
		}
		ptr += 2;
		x += 2;
	}

	/* draw vehicles? */
	if (type == 0 || type == 1) {
		Vehicle *v;
		bool skip;
		byte color;

		FOR_ALL_VEHICLES(v) {
			if (v->type != VEH_SPECIAL &&
					(v->vehstatus & (VS_HIDDEN | VS_UNCLICKABLE)) == 0) {
				/* Remap into flat coordinates. */
				Point pt = RemapCoords(
					v->x_pos / TILE_SIZE - WP(w,smallmap_d).scroll_x / TILE_SIZE, // divide each one separately because (a-b)/c != a/c-b/c in integer world
					v->y_pos / TILE_SIZE - WP(w,smallmap_d).scroll_y / TILE_SIZE, //    dtto
					0);
				x = pt.x;
				y = pt.y;

				/* Check if y is out of bounds? */
				y -= dpi->top;
				if (!IS_INT_INSIDE(y, 0, dpi->height)) continue;

				/* Default is to draw both pixels. */
				skip = false;

				/* Offset X coordinate */
				x -= WP(w,smallmap_d).subscroll + 3 + dpi->left;

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
				ptr = dpi->dst_ptr + y * dpi->pitch + x;
				color = (type == 1) ? _vehicle_type_colors[v->type] : 0xF;

				/* And draw either one or two pixels depending on clipping */
				ptr[0] = color;
				if (!skip) ptr[1] = color;
			}
		}
	}

	if (show_towns) {
		const Town *t;

		FOR_ALL_TOWNS(t) {
			/* Remap the town coordinate */
			Point pt = RemapCoords(
				(int)(TileX(t->xy) * TILE_SIZE - WP(w, smallmap_d).scroll_x) / TILE_SIZE,
				(int)(TileY(t->xy) * TILE_SIZE - WP(w, smallmap_d).scroll_y) / TILE_SIZE,
				0);
			x = pt.x - WP(w,smallmap_d).subscroll + 3 - (t->sign.width_2 >> 1);
			y = pt.y;

			/* Check if the town sign is within bounds */
			if (x + t->sign.width_2 > dpi->left &&
					x < dpi->left + dpi->width &&
					y + 6 > dpi->top &&
					y < dpi->top + dpi->height) {
				/* And draw it. */
				SetDParam(0, t->index);
				DrawString(x, y, STR_2056, 12);
			}
		}
	}

	/* Draw map indicators */
	{
		Point pt;

		/* Find main viewport. */
		vp = FindWindowById(WC_MAIN_WINDOW,0)->viewport;

		pt = RemapCoords(WP(w, smallmap_d).scroll_x, WP(w, smallmap_d).scroll_y, 0);

		x = vp->virtual_left - pt.x;
		y = vp->virtual_top - pt.y;
		x2 = (x + vp->virtual_width) / TILE_SIZE;
		y2 = (y + vp->virtual_height) / TILE_SIZE;
		x /= TILE_SIZE;
		y /= TILE_SIZE;

		x -= WP(w,smallmap_d).subscroll;
		x2 -= WP(w,smallmap_d).subscroll;

		DrawVertMapIndicator(x, y, x, y2);
		DrawVertMapIndicator(x2, y, x2, y2);

		DrawHorizMapIndicator(x, y, x2, y);
		DrawHorizMapIndicator(x, y2, x2, y2);
	}
	_cur_dpi = old_dpi;
}

void SmallMapCenterOnCurrentPos(Window *w)
{
	int x, y;
	ViewPort *vp;
	vp = FindWindowById(WC_MAIN_WINDOW, 0)->viewport;

	x  = ((vp->virtual_width  - (w->widget[4].right  - w->widget[4].left) * TILE_SIZE) / 2 + vp->virtual_left) / 4;
	y  = ((vp->virtual_height - (w->widget[4].bottom - w->widget[4].top ) * TILE_SIZE) / 2 + vp->virtual_top ) / 2 - TILE_SIZE * 2;
	WP(w, smallmap_d).scroll_x = (y - x) & ~0xF;
	WP(w, smallmap_d).scroll_y = (x + y) & ~0xF;
	SetWindowDirty(w);
}

static void SmallMapWindowProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			const LegendAndColour *tbl;
			int x, y, y_org;
			DrawPixelInfo new_dpi;

			/* draw the window */
			SetDParam(0, STR_00E5_CONTOURS + _smallmap_type);
			DrawWindowWidgets(w);

			/* draw the legend */
			tbl = _legend_table[(_smallmap_type != 2) ? _smallmap_type : (_opt.landscape + IND_OFFS)];
			x = 4;
			y_org = w->height - 44 - 11;
			y = y_org;
			for (;;) {
				GfxFillRect(x,     y + 1, x + 8, y + 5, 0);
				GfxFillRect(x + 1, y + 2, x + 7, y + 4, tbl->colour);
				DrawString(x + 11, y, tbl->legend, 0);

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

			if (!FillDrawPixelInfo(&new_dpi, 3, 17, w->width - 28 + 22, w->height - 64 - 11))
				return;

			DrawSmallMap(&new_dpi, w, _smallmap_type, _smallmap_show_towns);
		} break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case 4: { // Map window
					Window *w2 = FindWindowById(WC_MAIN_WINDOW, 0);
					Point pt;

					/*
					 * XXX: scrolling with the left mouse button is done by subsequently
					 * clicking with the left mouse button; clicking once centers the
					 * large map at the selected point. So by unclicking the left mouse
					 * button here, it gets reclicked during the next inputloop, which
					 * would make it look like the mouse is being dragged, while it is
					 * actually being (virtually) clicked every inputloop.
					 */
					_left_button_clicked = false;

					pt = RemapCoords(WP(w,smallmap_d).scroll_x, WP(w,smallmap_d).scroll_y, 0);
					WP(w2, vp_d).scrollpos_x = pt.x + ((_cursor.pos.x - w->left + 2) << 4) - (w2->viewport->virtual_width >> 1);
					WP(w2, vp_d).scrollpos_y = pt.y + ((_cursor.pos.y - w->top - 16) << 4) - (w2->viewport->virtual_height >> 1);

					SetWindowDirty(w);
				} break;

				case 5:  // Show land contours
				case 6:  // Show vehicles
				case 7:  // Show industries
				case 8:  // Show transport routes
				case 9:  // Show vegetation
				case 10: // Show land owners
					RaiseWindowWidget(w, _smallmap_type + 5);
					_smallmap_type = e->we.click.widget - 5;
					LowerWindowWidget(w, _smallmap_type + 5);

					SetWindowDirty(w);
					SndPlayFx(SND_15_BEEP);
					break;

				case 11: // Center the smallmap again
					SmallMapCenterOnCurrentPos(w);

					SetWindowDirty(w);
					SndPlayFx(SND_15_BEEP);
					break;

				case 12: // Toggle town names
					ToggleWidgetLoweredState(w, 12);
					_smallmap_show_towns = IsWindowWidgetLowered(w, 12);

					SetWindowDirty(w);
					SndPlayFx(SND_15_BEEP);
					break;
				}
			break;

		case WE_RCLICK:
			if (e->we.click.widget == 4) {
				if (_scrolling_viewport) return;
				_scrolling_viewport = true;
				_cursor.delta.x = 0;
				_cursor.delta.y = 0;
			}
			break;

		case WE_MOUSELOOP:
			/* update the window every now and then */
			if ((++w->vscroll.pos & 0x1F) == 0) SetWindowDirty(w);
			break;

		case WE_SCROLL: {
			int x;
			int y;
			int sub;
			int hx;
			int hy;
			int hvx;
			int hvy;

			_cursor.fix_at = true;

			x = WP(w, smallmap_d).scroll_x;
			y = WP(w, smallmap_d).scroll_y;

			sub = WP(w, smallmap_d).subscroll + e->we.scroll.delta.x;

			x -= (sub >> 2) << 4;
			y += (sub >> 2) << 4;
			sub &= 3;

			x += (e->we.scroll.delta.y >> 1) << 4;
			y += (e->we.scroll.delta.y >> 1) << 4;

			if (e->we.scroll.delta.y & 1) {
				x += TILE_SIZE;
				sub += 2;
				if (sub > 3) {
					sub -= 4;
					x -= TILE_SIZE;
					y += TILE_SIZE;
				}
			}

			hx = (w->widget[4].right  - w->widget[4].left) / 2;
			hy = (w->widget[4].bottom - w->widget[4].top ) / 2;
			hvx = hx * -4 + hy * 8;
			hvy = hx *  4 + hy * 8;
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

			WP(w, smallmap_d).scroll_x = x;
			WP(w, smallmap_d).scroll_y = y;
			WP(w, smallmap_d).subscroll = sub;

			SetWindowDirty(w);
		} break;
	}
}

static const WindowDesc _smallmap_desc = {
	WDP_AUTO, WDP_AUTO, 446, 314,
	WC_SMALLMAP, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_smallmap_widgets,
	SmallMapWindowProc
};

void ShowSmallMap()
{
	Window *w;

	w = AllocateWindowDescFront(&_smallmap_desc, 0);
	if (w == NULL) return;

	LowerWindowWidget(w, _smallmap_type + 5);
	SetWindowWidgetLoweredState(w, 12, _smallmap_show_towns);
	w->resize.width = 350;
	w->resize.height = 250;

	SmallMapCenterOnCurrentPos(w);
}

/* Extra ViewPort Window Stuff */
static const Widget _extra_view_port_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                         STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   287,     0,    13, STR_EXTRA_VIEW_PORT_TITLE,        STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   288,   299,     0,    13, 0x0,                              STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_RB,    14,     0,   299,    14,   233, 0x0,                              STR_NULL},
{      WWT_INSET,     RESIZE_RB,    14,     2,   297,    16,   231, 0x0,                              STR_NULL},
{ WWT_PUSHIMGBTN,     RESIZE_TB,    14,     0,    21,   234,   255, SPR_IMG_ZOOMIN,                   STR_017F_ZOOM_THE_VIEW_IN},
{ WWT_PUSHIMGBTN,     RESIZE_TB,    14,    22,    43,   234,   255, SPR_IMG_ZOOMOUT,                  STR_0180_ZOOM_THE_VIEW_OUT},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,    44,   171,   234,   255, STR_EXTRA_VIEW_MOVE_MAIN_TO_VIEW, STR_EXTRA_VIEW_MOVE_MAIN_TO_VIEW_TT},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   172,   298,   234,   255, STR_EXTRA_VIEW_MOVE_VIEW_TO_MAIN, STR_EXTRA_VIEW_MOVE_VIEW_TO_MAIN_TT},
{      WWT_PANEL,    RESIZE_RTB,    14,   299,   299,   234,   255, 0x0,                              STR_NULL},
{      WWT_PANEL,    RESIZE_RTB,    14,     0,   287,   256,   267, 0x0,                              STR_NULL},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   288,   299,   256,   267, 0x0,                              STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static void ExtraViewPortWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_CREATE: /* Disable zoom in button */
		DisableWindowWidget(w, 5);
		break;

	case WE_PAINT:
		// set the number in the title bar
		SetDParam(0, w->window_number + 1);

		DrawWindowWidgets(w);
		DrawWindowViewport(w);
		break;

	case WE_CLICK:
		switch (e->we.click.widget) {
			case 5: DoZoomInOutWindow(ZOOM_IN,  w); break;
			case 6: DoZoomInOutWindow(ZOOM_OUT, w); break;

		case 7: { /* location button (move main view to same spot as this view) 'Paste Location' */
			Window *w2 = FindWindowById(WC_MAIN_WINDOW, 0);
			int x = WP(w, vp_d).scrollpos_x; // Where is the main looking at
			int y = WP(w, vp_d).scrollpos_y;

			/* set this view to same location. Based on the center, adjusting for zoom */
			WP(w2, vp_d).scrollpos_x =  x - (w2->viewport->virtual_width -  w->viewport->virtual_width) / 2;
			WP(w2, vp_d).scrollpos_y =  y - (w2->viewport->virtual_height - w->viewport->virtual_height) / 2;
		} break;

		case 8: { /* inverse location button (move this view to same spot as main view) 'Copy Location' */
			const Window *w2 = FindWindowById(WC_MAIN_WINDOW, 0);
			int x = WP(w2, const vp_d).scrollpos_x;
			int y = WP(w2, const vp_d).scrollpos_y;

			WP(w, vp_d).scrollpos_x =  x + (w2->viewport->virtual_width -  w->viewport->virtual_width) / 2;
			WP(w, vp_d).scrollpos_y =  y + (w2->viewport->virtual_height - w->viewport->virtual_height) / 2;
		} break;
		}
		break;

	case WE_RESIZE:
		w->viewport->width          += e->we.sizing.diff.x;
		w->viewport->height         += e->we.sizing.diff.y;
		w->viewport->virtual_width  += e->we.sizing.diff.x;
		w->viewport->virtual_height += e->we.sizing.diff.y;
		break;

		case WE_SCROLL: {
			ViewPort *vp = IsPtInWindowViewport(w, _cursor.pos.x, _cursor.pos.y);

			if (vp == NULL) {
				_cursor.fix_at = false;
				_scrolling_viewport = false;
			}

			WP(w, vp_d).scrollpos_x += e->we.scroll.delta.x << vp->zoom;
			WP(w, vp_d).scrollpos_y += e->we.scroll.delta.y << vp->zoom;
		} break;

		case WE_MOUSEWHEEL:
			ZoomInOrOutToCursorWindow(e->we.wheel.wheel < 0, w);
			break;


		case WE_MESSAGE:
			/* Only handle zoom message if intended for us (msg ZOOM_IN/ZOOM_OUT) */
			if (e->we.message.wparam != w->window_number) break;
			HandleZoomMessage(w, w->viewport, 5, 6);
			break;
	}
}

static const WindowDesc _extra_view_port_desc = {
	WDP_AUTO, WDP_AUTO, 300, 268,
	WC_EXTRA_VIEW_PORT, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_extra_view_port_widgets,
	ExtraViewPortWndProc
};

void ShowExtraViewPortWindow()
{
	Window *w, *v;
	int i = 0;

	/* find next free window number for extra viewport */
	while (FindWindowById(WC_EXTRA_VIEW_PORT, i) != NULL) i++;

	w = AllocateWindowDescFront(&_extra_view_port_desc, i);
	if (w != NULL) {
		int x, y;
		/* the main window with the main view */
		v = FindWindowById(WC_MAIN_WINDOW, 0);
		/* New viewport start ats (zero,zero) */
		AssignWindowViewport(w, 3, 17, 294, 214, 0 , 0);

		/* center on same place as main window (zoom is maximum, no adjustment needed) */
		x = WP(v, vp_d).scrollpos_x;
		y = WP(v, vp_d).scrollpos_y;
		WP(w, vp_d).scrollpos_x = x + (v->viewport->virtual_width  - (294)) / 2;
		WP(w, vp_d).scrollpos_y = y + (v->viewport->virtual_height - (214)) / 2;
	}
}
