#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "map.h"
#include "gui.h"
#include "window.h"
#include "gfx.h"
#include "viewport.h"
#include "player.h"
#include "vehicle.h"
#include "town.h"
#include "sound.h"

static const Widget _smallmap_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    13,     0,    10,     0,    13, STR_00C5,		STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    13,    11,   433,     0,    13, STR_00B0_MAP,STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    13,   434,   445,     0,    13, 0x0,         STR_STICKY_BUTTON},
{     WWT_IMGBTN,     RESIZE_RB,    13,     0,   445,    14,   257, 0x0,					STR_NULL},
{          WWT_6,     RESIZE_RB,    13,     2,   443,    16,   255, 0x0,					STR_NULL},
{     WWT_IMGBTN,   RESIZE_LRTB,    13,   380,   401,   258,   279, 0x2E2,				STR_0191_SHOW_LAND_CONTOURS_ON_MAP},
{     WWT_IMGBTN,   RESIZE_LRTB,    13,   402,   423,   258,   279, 0x2E3,				STR_0192_SHOW_VEHICLES_ON_MAP},
{     WWT_IMGBTN,   RESIZE_LRTB,    13,   424,   445,   258,   279, 0x2E5,				STR_0193_SHOW_INDUSTRIES_ON_MAP},
{     WWT_IMGBTN,   RESIZE_LRTB,    13,   380,   401,   280,   301, 0x2E4,				STR_0194_SHOW_TRANSPORT_ROUTES_ON},
{     WWT_IMGBTN,   RESIZE_LRTB,    13,   402,   423,   280,   301, 0x2E6,				STR_0195_SHOW_VEGETATION_ON_MAP},
{     WWT_IMGBTN,   RESIZE_LRTB,    13,   424,   445,   280,   301, 0x2E7,				STR_0196_SHOW_LAND_OWNERS_ON_MAP},
{     WWT_IMGBTN,   RESIZE_LRTB,    13,   358,   379,   258,   279, 0x0,					STR_NULL},
{     WWT_IMGBTN,   RESIZE_LRTB,    13,   358,   379,   280,   301, 0xFED,				STR_0197_TOGGLE_TOWN_NAMES_ON_OFF},
{     WWT_IMGBTN,    RESIZE_RTB,    13,     0,   357,   258,   301, 0x0,					STR_NULL},
{      WWT_PANEL,    RESIZE_RTB,    13,     0,   433,   302,   313, 0x0,					STR_NULL},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    13,   434,   445,   302,   313, 0x0,					STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static int _smallmap_type;
static bool _smallmap_show_towns = true;

#define MK(a,b) a,b
#define MKEND() 0xffff
#define MS(a,b) (a|0x100),b

static const uint16 _legend_land_contours[] = {
	MK(0x5A,STR_00F0_100M),
	MK(0x5C,STR_00F1_200M),
	MK(0x5E,STR_00F2_300M),
	MK(0x1F,STR_00F3_400M),
	MK(0x27,STR_00F4_500M),

	MS(0xD7,STR_00EB_ROADS),
	MK(0x0A,STR_00EC_RAILROADS),
	MK(0x98,STR_00ED_STATIONS_AIRPORTS_DOCKS),
	MK(0xB5,STR_00EE_BUILDINGS_INDUSTRIES),
	MK(0x0F,STR_00EF_VEHICLES),
	MKEND()
};

static const uint16 _legend_vehicles[] = {
	MK(0xB8,STR_00F5_TRAINS),
	MK(0xBF,STR_00F6_ROAD_VEHICLES),
	MK(0x98,STR_00F7_SHIPS),
	MK(0x0F,STR_00F8_AIRCRAFT),
	MS(0xD7,STR_00F9_TRANSPORT_ROUTES),
	MK(0xB5,STR_00EE_BUILDINGS_INDUSTRIES),
	MKEND()
};

static const uint16 _legend_industries_normal[] = {
	MK(0xD7,STR_00FA_COAL_MINE),
	MK(0xB8,STR_00FB_POWER_STATION),
	MK(0x56,STR_00FC_FOREST),
	MK(0xC2,STR_00FD_SAWMILL),
	MK(0xBF,STR_00FE_OIL_REFINERY),
	MK(0x0F,STR_0105_BANK),

	MS(0x30,STR_00FF_FARM),
	MK(0xAE,STR_0100_FACTORY),
	MK(0x98,STR_0102_OIL_WELLS),
	MK(0x37,STR_0103_IRON_ORE_MINE),
	MK(0x0A,STR_0104_STEEL_MILL),
	MKEND()
};

static const uint16 _legend_industries_hilly[] = {
	MK(0xD7,STR_00FA_COAL_MINE),
	MK(0xB8,STR_00FB_POWER_STATION),
	MK(0x56,STR_00FC_FOREST),
	MK(0x0A,STR_0106_PAPER_MILL),
	MK(0xBF,STR_00FE_OIL_REFINERY),
	MK(0x37,STR_0108_FOOD_PROCESSING_PLANT),
	MS(0x30,STR_00FF_FARM),

	MK(0xAE,STR_0101_PRINTING_WORKS),
	MK(0x98,STR_0102_OIL_WELLS),
	MK(0xC2,STR_0107_GOLD_MINE),
	MK(0x0F,STR_0105_BANK),
	MKEND()
};

static const uint16 _legend_industries_desert[] = {
	MK(0xBF,STR_00FE_OIL_REFINERY),
	MK(0x98,STR_0102_OIL_WELLS),
	MK(0x0F,STR_0105_BANK),
	MK(0xB8,STR_0109_DIAMOND_MINE),
	MK(0x37,STR_0108_FOOD_PROCESSING_PLANT),
	MK(0x0A,STR_010A_COPPER_ORE_MINE),
	MK(0x30,STR_00FF_FARM),
	MS(0x56,STR_010B_FRUIT_PLANTATION),

	MK(0x27,STR_010C_RUBBER_PLANTATION),
	MK(0x25,STR_010D_WATER_SUPPLY),
	MK(0xD0,STR_010E_WATER_TOWER),
	MK(0xAE,STR_0100_FACTORY),
	MK(0xC2,STR_010F_LUMBER_MILL),
	MKEND()
};

static const uint16 _legend_industries_candy[] = {
	MK(0x30,STR_0110_COTTON_CANDY_FOREST),
	MK(0xAE,STR_0111_CANDY_FACTORY),
	MK(0x27,STR_0112_BATTERY_FARM),
	MK(0x37,STR_0113_COLA_WELLS),
	MK(0xD0,STR_0114_TOY_SHOP),
	MK(0x0A,STR_0115_TOY_FACTORY),
	MS(0x25,STR_0116_PLASTIC_FOUNTAINS),

	MK(0xB8,STR_0117_FIZZY_DRINK_FACTORY),
	MK(0x98,STR_0118_BUBBLE_GENERATOR),
	MK(0xC2,STR_0119_TOFFEE_QUARRY),
	MK(0x0F,STR_011A_SUGAR_MINE),
	MKEND()
};

static const uint16 _legend_routes[] = {
	MK(0xD7,STR_00EB_ROADS),
	MK(0x0A,STR_00EC_RAILROADS),
	MK(0xB5,STR_00EE_BUILDINGS_INDUSTRIES),
	MS(0x56,STR_011B_RAILROAD_STATION),

	MK(0xC2,STR_011C_TRUCK_LOADING_BAY),
	MK(0xBF,STR_011D_BUS_STATION),
	MK(0xB8,STR_011E_AIRPORT_HELIPORT),
	MK(0x98,STR_011F_DOCK),
	MKEND()
};

static const uint16 _legend_vegetation[] = {
	MK(0x52,STR_0120_ROUGH_LAND),
	MK(0x54,STR_0121_GRASS_LAND),
	MK(0x37,STR_0122_BARE_LAND),
	MK(0x25,STR_0123_FIELDS),
	MK(0x57,STR_0124_TREES),
	MK(0xD0,STR_00FC_FOREST),
	MS(0x0A,STR_0125_ROCKS),

	MK(0xC2,STR_012A_DESERT),
	MK(0x98,STR_012B_SNOW),
	MK(0xD7,STR_00F9_TRANSPORT_ROUTES),
	MK(0xB5,STR_00EE_BUILDINGS_INDUSTRIES),
	MKEND()
};

static const uint16 _legend_land_owners[] = {
	MK(0xCA,STR_0126_WATER),
	MK(0x54,STR_0127_NO_OWNER),
	MK(0xB4,STR_0128_TOWNS),
	MK(0x20,STR_0129_INDUSTRIES),
	MKEND()
};
#undef MK
#undef MS
#undef MKEND


enum { IND_OFFS = 6 };
static const uint16 * const _legend_table[] = {
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

#if defined(TTD_ALIGNMENT_4)
	static inline void WRITE_PIXELS(void *dst, uint32 val)
	{
		byte *d = (byte*)dst;
#	if defined(TTD_BIG_ENDIAN)
		d[0] = (byte)(val >> 24);
		d[1] = (byte)(val >> 16);
		d[2] = (byte)(val >> 8);
		d[3] = (byte)(val >> 0);
#	elif defined(TTD_LITTLE_ENDIAN)
		d[0] = (byte)(val >> 0);
		d[1] = (byte)(val >> 8);
		d[2] = (byte)(val >> 16);
		d[3] = (byte)(val >> 24);
#	endif
	}

/* need to use OR, otherwise we will overwrite the wrong pixels at the edges :( */
	static inline void WRITE_PIXELS_OR(void *dst, uint32 val)
	{
		byte *d = (byte*)dst;
#	if defined(TTD_BIG_ENDIAN)
		d[0] |= (byte)(val >> 24);
		d[1] |= (byte)(val >> 16);
		d[2] |= (byte)(val >> 8);
		d[3] |= (byte)(val >> 0);
#	elif defined(TTD_LITTLE_ENDIAN)
		d[0] |= (byte)(val >> 0);
		d[1] |= (byte)(val >> 8);
		d[2] |= (byte)(val >> 16);
		d[3] |= (byte)(val >> 24);
#	endif
	}
#else
#	define WRITE_PIXELS(dst, val)		*(uint32*)(dst) = (val);
#	define WRITE_PIXELS_OR(dst,val)	*(uint32*)(dst) |= (val);
#endif

#if defined(TTD_BIG_ENDIAN)
#	define MKCOLOR(x) BSWAP32(x)
#elif defined(TTD_LITTLE_ENDIAN)
#	define MKCOLOR(x) (x)
#endif

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

static const uint32 _smallmap_contours_andor[12][2] = {
	{MKCOLOR(0x00000000),MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0x000A0A00),MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00D7D700),MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00B5B500),MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00000000),MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0x98989898),MKCOLOR(0x00000000)},
	{MKCOLOR(0xCACACACA),MKCOLOR(0x00000000)},
	{MKCOLOR(0x00000000),MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0xB5B5B5B5),MKCOLOR(0x00000000)},
	{MKCOLOR(0x00000000),MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0x00B5B500),MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x000A0A00),MKCOLOR(0xFF0000FF)},
};

static const uint32 _smallmap_vehicles_andor[12][2] = {
	{MKCOLOR(0x00000000),MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0x00D7D700),MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00D7D700),MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00B5B500),MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00000000),MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0x00D7D700),MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0xCACACACA),MKCOLOR(0x00000000)},
	{MKCOLOR(0x00000000),MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0xB5B5B5B5),MKCOLOR(0x00000000)},
	{MKCOLOR(0x00000000),MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0x00B5B500),MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00D7D700),MKCOLOR(0xFF0000FF)},
};

static const uint32 _smallmap_vegetation_andor[12][2] = {
	{MKCOLOR(0x00000000),MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0x00D7D700),MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00D7D700),MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00B5B500),MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00575700),MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00D7D700),MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0xCACACACA),MKCOLOR(0x00000000)},
	{MKCOLOR(0x00000000),MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0xB5B5B5B5),MKCOLOR(0x00000000)},
	{MKCOLOR(0x00000000),MKCOLOR(0xFFFFFFFF)},
	{MKCOLOR(0x00B5B500),MKCOLOR(0xFF0000FF)},
	{MKCOLOR(0x00D7D700),MKCOLOR(0xFF0000FF)},
};

static inline uint32 GetSmallMapCountoursPixels(uint tile)
{
	uint t;

	t = TileType(tile);
	if (t == MP_TUNNELBRIDGE) {
		t = _map5[tile];
		if ((t & 0x80) == 0) t>>=1;
		if ((t & 6) == 0) {
			t = MP_RAILWAY;
		} else if ((t & 6) == 2) {
			t = MP_STREET;
		} else {
			t = MP_WATER;
		}
	}

	return (_map_height_bits[TileHeight(tile)] & _smallmap_contours_andor[t][1]) | _smallmap_contours_andor[t][0];
}

static void DrawSmallMapContours(byte *dst, uint xc, uint yc, int pitch, int reps, uint32 mask)
{
	do {
		if (xc < MapMaxX() && yc < MapMaxY())
		    if (dst > _screen.dst_ptr && dst < (_screen.dst_ptr + _screen.width * _screen.height - _screen.width) )
		        WRITE_PIXELS_OR( dst, GetSmallMapCountoursPixels(TILE_XY(xc,yc)) & mask );
	} while (xc++,yc++,dst+=pitch,--reps != 0);
}


static inline uint32 GetSmallMapVehiclesPixels(uint tile)
{
	uint t;

	t = TileType(tile);
	if (t == MP_TUNNELBRIDGE) {
		t = _map5[tile];
		if ((t & 0x80) == 0) t>>=1;
		if ((t & 6) == 0) {
			t = MP_RAILWAY;
		} else if ((t & 6) == 2) {
			t = MP_STREET;
		} else {
			t = MP_WATER;
		}
	}
	return (MKCOLOR(0x54545454) & _smallmap_vehicles_andor[t][1]) | _smallmap_vehicles_andor[t][0];
}


static void DrawSmallMapVehicles(byte *dst, uint xc, uint yc, int pitch, int reps, uint32 mask)
{
	do {
		if (xc < MapMaxX() && yc < MapMaxY())
			WRITE_PIXELS_OR( dst, GetSmallMapVehiclesPixels(TILE_XY(xc,yc)) & mask );
	} while (xc++,yc++,dst+=pitch,--reps != 0);
}

static const byte _industry_smallmap_colors[175] = {
	215,215,215,215,215,215,215,184,
	184,184,184,194,194,194,194,194,
	 86, 86,191,191,191,191,191,191,
	152,152,152,152,152,152,152,152,
	152, 48, 48, 48, 48, 48, 48,174,
	174,174,174,174,174,174,174, 10,
	 10, 10, 10, 10, 10, 10, 10, 10,
	 10, 10, 15, 15, 55, 55, 55, 55,
	 10, 10, 10, 10, 10, 10, 10, 10,
	194,194,194,194,194,194,194,194,
	194,194,194,194,194,194,194,194,
	194, 15, 15,184,184,184,184,184,
	184,184,184,184, 55, 55, 55, 55,
	 55, 55, 55, 55, 55, 55, 55, 55,
	 55, 55, 55, 55, 86, 39, 37, 37,
	208,174,174,174,174,194,194,194,
	194, 48, 48,174,174,174,174, 39,
	 39, 55,208,208,208,208, 10, 10,
	 10, 10, 10, 10, 37, 37, 37, 37,
	 37, 37, 37, 37,184,184,184,184,
	152,152,152,152,194,194,194, 15,
	 15, 15, 15, 15, 15, 15, 15,
};

static inline uint32 GetSmallMapIndustriesPixels(uint tile)
{
	int t;

	t = TileType(tile);
	if (t == MP_INDUSTRY) {
		byte color = _industry_smallmap_colors[_map5[tile]];
		return color + (color << 8) + (color << 16) + (color << 24);
	} else {
		if (t == MP_TUNNELBRIDGE) {
			t = _map5[tile];
			if ((t & 0x80) == 0) t>>=1;
			if ((t & 6) == 0) {
				t = MP_RAILWAY;
			} else if ((t & 6) == 2) {
				t = MP_STREET;
			} else {
				t = MP_WATER;
			}
		}
		return ((MKCOLOR(0x54545454) & _smallmap_vehicles_andor[t][1]) | _smallmap_vehicles_andor[t][0]);
	}
}

static void DrawSmallMapIndustries(byte *dst, uint xc, uint yc, int pitch, int reps, uint32 mask)
{
	do {
		if (xc < MapMaxX() && yc < MapMaxY())
			WRITE_PIXELS_OR(dst, GetSmallMapIndustriesPixels(TILE_XY(xc,yc)) & mask);
	} while (xc++,yc++,dst+=pitch,--reps != 0);
}

static inline uint32 GetSmallMapRoutesPixels(uint tile)
{
	int t;
	uint32 bits;

	t = TileType(tile);
	if (t == MP_STATION) {
		byte m5 = _map5[tile];
		(bits = MKCOLOR(0x56565656), m5 < 8) ||			//   8 - railroad station (green)
		(bits = MKCOLOR(0xB8B8B8B8), m5 < 0x43) ||	//  67 - airport (red)
		(bits = MKCOLOR(0xC2C2C2C2), m5 < 0x47) ||	//  71 - truck loading bay (orange)
		(bits = MKCOLOR(0xBFBFBFBF), m5 < 0x4B) ||	//  75 - bus station (yellow)
		(bits = MKCOLOR(0x98989898), m5 < 0x53) ||	//  83 - docks (blue)
		(bits = MKCOLOR(0xB8B8B8B8), m5 < 0x73) ||	// 115 - airport (red) (new airports)
		(bits = MKCOLOR(0xFFFFFFFF), true);					// all others
	} else {
		if (t == MP_TUNNELBRIDGE) {
			t = _map5[tile];
			if ((t & 0x80) == 0) t>>=1;
			if ((t & 6) == 0) {
				t = MP_RAILWAY;
			} else if ((t & 6) == 2) {
				t = MP_STREET;
			} else {
				t = MP_WATER;
			}
		}
		// ground color
		bits = ((MKCOLOR(0x54545454) & _smallmap_contours_andor[t][1]) | _smallmap_contours_andor[t][0]);
	}
	return bits;
}

static void DrawSmallMapRoutes(byte *dst, uint xc, uint yc, int pitch, int reps, uint32 mask)
{
	do {
		if (xc < MapMaxX() && yc < MapMaxY())
			WRITE_PIXELS_OR(dst, GetSmallMapRoutesPixels(TILE_XY(xc,yc)) & mask);
	} while (xc++,yc++,dst+=pitch,--reps != 0);
}

static const uint32 _vegetation_clear_bits[4 + 7] = {
	MKCOLOR(0x37373737),
	MKCOLOR(0x37373737),
	MKCOLOR(0x37373737),
	MKCOLOR(0x54545454),

	MKCOLOR(0x52525252),
	MKCOLOR(0x0A0A0A0A),
	MKCOLOR(0x25252525),
	MKCOLOR(0x98989898),
	MKCOLOR(0xC2C2C2C2),
	MKCOLOR(0x54545454),
	MKCOLOR(0x54545454),
};

static inline uint32 GetSmallMapVegetationPixels(uint tile)
{
	int i,t;
	uint32 bits;

	t = TileType(tile);
	if (t == MP_CLEAR) {
		i = (_map5[tile] & 0x1F) - 4;
		if (i >= 0) i = (i >> 2);
		bits = _vegetation_clear_bits[i + 4];
	} else if (t == MP_INDUSTRY) {
		bits = IS_BYTE_INSIDE(_map5[tile], 0x10, 0x12) ? MKCOLOR(0xD0D0D0D0) : MKCOLOR(0xB5B5B5B5);
	} else if (t == MP_TREES) {
		bits = MKCOLOR(0x54575754);
		if ((_map2[tile] & 0x30) == 0x20)
			bits = (_opt.landscape == LT_HILLY) ? MKCOLOR(0x98575798) : MKCOLOR(0xC25757C2);
	} else {
		if (t == MP_TUNNELBRIDGE) {
			t = _map5[tile];
			if ((t & 0x80) == 0) t>>=1;
			if ((t & 6) == 0) {
				t = MP_RAILWAY;
			} else if ((t & 6) == 2) {
				t = MP_STREET;
			} else {
				t = MP_WATER;
			}
		}
		bits = ((MKCOLOR(0x54545454) & _smallmap_vehicles_andor[t][1]) | _smallmap_vehicles_andor[t][0]);
	}

	return bits;
}


static void DrawSmallMapVegetation(byte *dst, uint xc, uint yc, int pitch, int reps, uint32 mask)
{
	do {
		if (xc < MapMaxX() && yc < MapMaxY())
			WRITE_PIXELS_OR(dst, GetSmallMapVegetationPixels(TILE_XY(xc,yc)) & mask);
	} while (xc++,yc++,dst+=pitch,--reps != 0);
}


static uint32 *_owner_colors;

static inline uint32 GetSmallMapOwnerPixels(uint tile)
{
	int t;

	t = TileType(tile);
	if (t == MP_HOUSE || _map_owner[tile] == OWNER_TOWN) {
		t = 0x80;
	} else if (t == MP_INDUSTRY) {
		t = 0xff;
	} else {
		t = _map_owner[tile];
	}

	return _owner_colors[t];
}


static void DrawSmallMapOwners(byte *dst, uint xc, uint yc, int pitch, int reps, uint32 mask)
{
	do {
		if (xc < MapMaxX() && yc < MapMaxY())
			WRITE_PIXELS_OR(dst, GetSmallMapOwnerPixels(TILE_XY(xc,yc)) & mask);
	} while (xc++,yc++,dst+=pitch,--reps != 0);
}


static const uint32 _smallmap_mask_left[3] = {
	MKCOLOR(0xFF000000),
	MKCOLOR(0xFFFF0000),
	MKCOLOR(0xFFFFFF00),
};

static const uint32 _smallmap_mask_right[4] = {
	MKCOLOR(0x00000000),
	MKCOLOR(0x000000FF),
	MKCOLOR(0x0000FFFF),
	MKCOLOR(0x00FFFFFF),
};

/* each tile has 4 x pixels and 1 y pixel */

typedef void SmallmapDrawProc(byte *dst, uint xc, uint yc, int pitch, int reps, uint32 mask);

static SmallmapDrawProc *_smallmap_draw_procs[] = {
	DrawSmallMapContours,
	DrawSmallMapVehicles,
	DrawSmallMapIndustries,
	DrawSmallMapRoutes,
	DrawSmallMapVegetation,
	DrawSmallMapOwners,
};

static const byte _vehicle_type_colors[6] = {
	184, 191, 152, 15, 215, 184
};

static inline uint32 dup_byte32(byte b) {
	return b + (b << 8) + (b << 16) + (b << 24);
}

static void DrawVertMapIndicator(int x, int y, int x2, int y2)
{
	GfxFillRect(x, y, x2, y + 3, 69);
	GfxFillRect(x, y2 - 3, x2, y2, 69);
}

static void DrawHorizMapIndicator(int x, int y, int x2, int y2)
{
	GfxFillRect(x, y, x + 3, y2, 69);
	GfxFillRect(x2 - 3, y, x2, y2, 69);
}

static void DrawSmallMap(DrawPixelInfo *dpi, Window *w, int type, bool show_towns)
{
	DrawPixelInfo *old_dpi;
	int dx,dy, x, y, x2, y2;
	byte *ptr;
	uint tile_x, tile_y;
	uint32 mask;
	int t;
	int reps;
	SmallmapDrawProc *proc;
	ViewPort *vp;

	old_dpi = _cur_dpi;
	_cur_dpi = dpi;

	/* clear it */
	GfxFillRect(dpi->left, dpi->top, dpi->left + dpi->width - 1, dpi->top + dpi->height - 1, 0);

	/* setup owner table */
	if (type == 5) {
		Player *p;
		uint32 *tbl;

		/* setup owners ptr */
		_owner_colors = tbl = alloca(256 * sizeof(*_owner_colors));

		/* clear the town colors */
		memset(tbl + 128, 0xB4, 128 * sizeof(*_owner_colors));

		/* fill with some special colors */
		tbl[0x10] = MKCOLOR(0x54545454);
		tbl[0x11] = MKCOLOR(0xCACACACA);
		tbl[0xff] = MKCOLOR(0x20202020); /* industry */

		/* now fill with the player colors */
		FOR_ALL_PLAYERS(p) {
			if (p->is_active) {
				tbl[p->index] = dup_byte32(GetSpritePtr(0x307 + p->player_color)[0xCB]);
			}
		}
	}

	tile_x = (int)WP(w,smallmap_d).scroll_x >> 4;
	tile_y = (int)WP(w,smallmap_d).scroll_y >> 4;

	dx = dpi->left + WP(w,smallmap_d).subscroll;
	tile_x -= (dx >> 2);
	tile_y += (dx >> 2);
	dx &= 3;

	dy = dpi->top;
	tile_x += (dy >> 1);
	tile_y += (dy >> 1);

	if (dy & 1) {
		tile_x++;
		dx += 2;
		if (dx > 3) {
			dx -= 4;
			tile_x--;
			tile_y++;
		}
	}

	proc = _smallmap_draw_procs[type];

	ptr = dpi->dst_ptr - dx - 4;
	x = - dx - 4;
	y = 0;

	for(;;) {
		mask = (uint32)-1;

		/* distance from left edge */
		if (x < 0) {
			if (x < -3) goto skip_column;
			/* mask to use at the left edge */
			mask = _smallmap_mask_left[x + 3];
		}

		/* distance from right edge */
		t = dpi->width - x;
		if (t < 4) {
			if (t < 0)
				break; /* exit loop */
			/* mask to use at the right edge */
			mask &= _smallmap_mask_right[t];
		}

		/* number of lines */
		reps = ((dpi->height - y + 1) >> 1);
		if (reps > 0) {
//			assert(ptr >= dpi->dst_ptr);
			proc(ptr, tile_x, tile_y, dpi->pitch*2, reps, mask);
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
			if (v->type != 0 && v->type != 0x14 && (v->vehstatus & (VS_HIDDEN|VS_UNCLICKABLE)) == 0) {
				// Remap into flat coordinates.
				Point pt = RemapCoords(
					(int)(v->x_pos - WP(w,smallmap_d).scroll_x) >> 4,
					(int)(v->y_pos - WP(w,smallmap_d).scroll_y) >> 4,
					0);
				x = pt.x;
				y = pt.y;

				// Check if y is out of bounds?
				y -= dpi->top;
				if ((uint)y >= (uint)dpi->height)
					continue;

				// Default is to draw both pixels.
				skip = false;

				// Offset X coordinate
				x -= WP(w,smallmap_d).subscroll + 3 + dpi->left;

				if (x < 0) {
					// if x+1 is 0, that means we're on the very left edge,
					//  and should thus only draw a single pixel
					if (++x != 0)
						continue;
					skip = true;
				} else if (x >= dpi->width - 1) {
					// Check if we're at the very right edge, and if so draw only a single pixel
					if (x != dpi->width - 1)
						continue;
					skip = true;
				}

				// Calculate pointer to pixel and the color
				ptr = dpi->dst_ptr + y * dpi->pitch + x;
				color = (type == 1) ? _vehicle_type_colors[v->type-0x10] : 0xF;

				// And draw either one or two pixels depending on clipping
				ptr[0] = color;
				if (!skip)
					ptr[1] = color;
			}
		}
	}

	if (show_towns) {
		Town *t;
		FOR_ALL_TOWNS(t) {
			if (t->xy != 0) {
				// Remap the town coordinate
				Point pt = RemapCoords(
					(int)(TileX(t->xy) * 16 - WP(w,smallmap_d).scroll_x) >> 4,
					(int)(TileY(t->xy) * 16 - WP(w,smallmap_d).scroll_y) >> 4,
					0);
				x = pt.x - WP(w,smallmap_d).subscroll + 3 - (t->sign.width_2 >> 1);
				y = pt.y;

				// Check if the town sign is within bounds
				if (x + t->sign.width_2 > dpi->left &&
						x < dpi->left + dpi->width &&
						y + 6 > dpi->top &&
						y < dpi->top + dpi->height) {
					// And draw it.
					SetDParam(0, t->index);
					DrawString(x, y, STR_2056, 12);
				}
			}
		}
	}

	// Draw map indicators
	{
		Point pt;

		// Find main viewport.
		vp = FindWindowById(WC_MAIN_WINDOW,0)->viewport;

		pt = RemapCoords(
			WP(w,smallmap_d).scroll_x,
			WP(w,smallmap_d).scroll_y,
			0);

		x = vp->virtual_left - pt.x;
		y = vp->virtual_top - pt.y;
		x2 = (x + vp->virtual_width) >> 4;
		y2 = (y + vp->virtual_height) >> 4;
		x >>= 4;
		y >>= 4;

		x -= WP(w,smallmap_d).subscroll;
		x2 -= WP(w,smallmap_d).subscroll;

		DrawVertMapIndicator(x, y, x, y2);
		DrawVertMapIndicator(x2, y, x2, y2);

		DrawHorizMapIndicator(x, y, x2, y);
		DrawHorizMapIndicator(x, y2, x2, y2);
	}
	_cur_dpi = old_dpi;
}

static void SmallMapWindowProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		const uint16 *tbl;
		int x,y,y_org;
		DrawPixelInfo new_dpi;

		/* draw the window */
		SetDParam(0, STR_00E5_CONTOURS + _smallmap_type);
		DrawWindowWidgets(w);

		/* draw the legend */
		tbl = _legend_table[(_smallmap_type != 2) ? _smallmap_type : (_opt.landscape + IND_OFFS)];
		x = 4;
		y_org = w->height - 43 - 11;
		y = y_org;
		while (true) {
			GfxFillRect(x, y+1, x+8, y + 5, 0);
			GfxFillRect(x+1, y+2, x+7, y + 4, (byte)tbl[0]);
			DrawString(x+11, y, tbl[1], 0);

			tbl += 2;
			y += 6;

			if (tbl[0] == 0xFFFF) {
				break;
			} else if (tbl[0] & 0x100) {
				x += 123;
				y = y_org;
			}
		}

		if (!FillDrawPixelInfo(&new_dpi, NULL, 3, 17, w->width - 28 + 22, w->height - 64 - 11))
			return;

		DrawSmallMap(&new_dpi, w, _smallmap_type, _smallmap_show_towns);
	} break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 4: {/* main wnd */
			Window *w2;
			Point pt;

			_left_button_clicked = false;

			w2 = FindWindowById(WC_MAIN_WINDOW, 0);

			pt = RemapCoords(WP(w,smallmap_d).scroll_x, WP(w,smallmap_d).scroll_y, 0);
			WP(w2,vp_d).scrollpos_x = pt.x + ((_cursor.pos.x - w->left + 2) << 4) - (w2->viewport->virtual_width >> 1);
			WP(w2,vp_d).scrollpos_y = pt.y + ((_cursor.pos.y - w->top - 16) << 4) - (w2->viewport->virtual_height >> 1);
		} break;

		case 5: /* show land contours */
		case 6: /* show vehicles */
		case 7: /* show industries */
		case 8: /* show transport routes */
		case 9: /* show vegetation */
		case 10: /* show land owners */
			w->click_state &= ~(1<<5|1<<6|1<<7|1<<8|1<<9|1<<10);
			w->click_state |= 1 << e->click.widget;
			_smallmap_type = e->click.widget - 5;

			SetWindowDirty(w);
			SndPlayFx(SND_15_BEEP);
			break;

		case 12: /* toggle town names */
			w->click_state ^= (1 << 12);
			_smallmap_show_towns = (w->click_state >> 12) & 1;
			SetWindowDirty(w);
			SndPlayFx(SND_15_BEEP);
			break;
		}
		break;

	case WE_RCLICK:
		if (e->click.widget == 4) {
			if (_scrolling_viewport)
				return;
			_scrolling_viewport = true;
			_cursor.delta.x = 0;
			_cursor.delta.y = 0;
		}
		break;

	case WE_MOUSELOOP:
		/* update the window every now and then */
		if ((++w->vscroll.pos & 0x1F) == 0)
			SetWindowDirty(w);
		break;
	}
}

static const WindowDesc _smallmap_desc = {
	-1,-1, 446, 314,
	WC_SMALLMAP,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_smallmap_widgets,
	SmallMapWindowProc
};

void ShowSmallMap()
{
	Window *w;
	ViewPort *vp;
	int x,y;

	w = AllocateWindowDescFront(&_smallmap_desc, 0);
	if (w) {
		w->click_state = ((1<<5) << _smallmap_type) | (_smallmap_show_towns << 12);
		w->resize.width = 350;
		w->resize.height = 250;

		vp = FindWindowById(WC_MAIN_WINDOW, 0)->viewport;

		x =  (((vp->virtual_width - (220*32)) / 2) + vp->virtual_left) / 4;
		y = ((((vp->virtual_height- (120*32)) / 2) + vp->virtual_top ) / 2) - 32;
		WP(w,smallmap_d).scroll_x = (y-x) & ~0xF;
		WP(w,smallmap_d).scroll_y = (x+y) & ~0xF;
		WP(w,smallmap_d).subscroll = 0;
	}
}

/* Extra ViewPort Window Stuff */
static const Widget _extra_view_port_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,	STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   287,     0,    13, STR_EXTRA_VIEW_PORT_TITLE,				STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,    14,   288,   299,     0,    13, 0x0,       STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_RB,    14,     0,   299,    14,   233, 0x0,				STR_NULL},
{          WWT_6,     RESIZE_RB,    14,     2,   297,    16,   231, 0x0,				STR_NULL},
{      WWT_PANEL,     RESIZE_TB,    14,     0,    21,   234,   255, 0x2DF,			STR_017F_ZOOM_THE_VIEW_IN},
{      WWT_PANEL,     RESIZE_TB,    14,    22,    43,   234,   255, 0x2E0,			STR_0180_ZOOM_THE_VIEW_OUT},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,    44,   171,   234,   255, STR_EXTRA_VIEW_MOVE_MAIN_TO_VIEW,STR_EXTRA_VIEW_MOVE_MAIN_TO_VIEW_TT},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   172,   298,   234,   255, STR_EXTRA_VIEW_MOVE_VIEW_TO_MAIN,STR_EXTRA_VIEW_MOVE_VIEW_TO_MAIN_TT},
{      WWT_PANEL,    RESIZE_RTB,    14,   299,   299,   234,   255, 0x0,				STR_NULL},
{      WWT_PANEL,    RESIZE_RTB,    14,     0,   287,   256,   267, 0x0,				STR_NULL},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   288,   299,   256,   267, 0x0,				STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static void ExtraViewPortWndProc(Window *w, WindowEvent *e)
{
	ViewPort *vp = w->viewport;

	switch(e->event) {
	case WE_PAINT: {
		// set the number in the title bar
		SetDParam(0, (w->window_number+1));

		DrawWindowWidgets(w);
		DrawWindowViewport(w);
	}	break;
	case WE_CLICK: {
		switch(e->click.widget) {
		case 5: { /* zoom in */
			DoZoomInOutWindow(ZOOM_IN, w);
		} break;

		case 6: { /* zoom out */
			DoZoomInOutWindow(ZOOM_OUT, w);
		} break;

		case 7: { /* location button (move main view to same spot as this view) */
			Window * w2 = FindWindowById(WC_MAIN_WINDOW, 0);
			int x = WP(w,vp_d).scrollpos_x; // Where is the main looking at
			int y = WP(w,vp_d).scrollpos_y;

			// set this view to same location. Based on the center, adjusting for zoom
			WP(w2,vp_d).scrollpos_x =  x - (w2->viewport->virtual_width -  (w->viewport->virtual_width  << vp->zoom)) / 2;
			WP(w2,vp_d).scrollpos_y =  y - (w2->viewport->virtual_height - (w->viewport->virtual_height << vp->zoom)) / 2;
		} break;
		case 8: { /* inverse location button (move this view to same spot as main view) */
			Window * w2 = FindWindowById(WC_MAIN_WINDOW, 0);
			int x = WP(w2,vp_d).scrollpos_x;
			int y = WP(w2,vp_d).scrollpos_y;

			WP(w,vp_d).scrollpos_x =  x + (w2->viewport->virtual_width -  (w->viewport->virtual_width  << vp->zoom)) / 2;
			WP(w,vp_d).scrollpos_y =  y + (w2->viewport->virtual_height - (w->viewport->virtual_height << vp->zoom)) / 2;
		} break;
		}
	} break;
	case WE_RESIZE: {
		w->viewport->width  += e->sizing.diff.x;
		w->viewport->height += e->sizing.diff.y;

		w->viewport->virtual_width  += e->sizing.diff.x;
		w->viewport->virtual_height += e->sizing.diff.y;
	} break;
	}
}

static const WindowDesc _extra_view_port_desc = {
	-1,-1, 300, 268,
	WC_EXTRA_VIEW_PORT,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_extra_view_port_widgets,
	ExtraViewPortWndProc
};

void ShowExtraViewPortWindow()
{
	Window *w, *v;
	int i = 0;

	// find next free window number for extra viewport
	while (FindWindowById(WC_EXTRA_VIEW_PORT, i) ) {
		i++;
	}

	w = AllocateWindowDescFront(&_extra_view_port_desc, i);
	if (w) {
		int x, y;
		// disable zoom in button
		w->disabled_state = (1 << 4);
		// the main window with the main view
		v = FindWindowById(WC_MAIN_WINDOW, 0);
		// New viewport start ats (zero,zero)
		AssignWindowViewport(w, 3, 17, 294, 214, 0 , 0);

		// center on same place as main window (zoom is maximum, no adjustment needed)
		x = WP(v,vp_d).scrollpos_x;
		y = WP(v,vp_d).scrollpos_y;
		WP(w,vp_d).scrollpos_x = x + (v->viewport->virtual_width  - (294)) / 2;
		WP(w,vp_d).scrollpos_y = y + (v->viewport->virtual_height - (214)) / 2;
	}
}
