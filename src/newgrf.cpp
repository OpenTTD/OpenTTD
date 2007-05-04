/* $Id$ */

/** @file newgrf.cpp */

#include "stdafx.h"

#include <stdarg.h>

#include "openttd.h"
#include "debug.h"
#include "gfx.h"
#include "fileio.h"
#include "functions.h"
#include "engine.h"
#include "spritecache.h"
#include "station.h"
#include "sprite.h"
#include "newgrf.h"
#include "variables.h"
#include "string.h"
#include "table/strings.h"
#include "bridge.h"
#include "town.h"
#include "economy.h"
#include "newgrf_engine.h"
#include "vehicle.h"
#include "newgrf_text.h"
#include "table/sprites.h"
#include "fontcache.h"
#include "date.h"
#include "currency.h"
#include "landscape.h"
#include "sound.h"
#include "newgrf_config.h"
#include "newgrf_house.h"
#include "newgrf_sound.h"
#include "newgrf_spritegroup.h"
#include "helpers.hpp"
#include "table/town_land.h"
#include "cargotype.h"
#include "industry.h"

/* TTDPatch extended GRF format codec
 * (c) Petr Baudis 2004 (GPL'd)
 * Changes by Florian octo Forster are (c) by the OpenTTD development team.
 *
 * Contains portions of documentation by TTDPatch team.
 * Thanks especially to Josef Drexler for the documentation as well as a lot
 * of help at #tycoon. Also thanks to Michael Blunck for is GRF files which
 * served as subject to the initial testing of this codec. */


static int _skip_sprites; // XXX
static uint _file_index; // XXX
SpriteID _signal_base;
SpriteID _coast_base;

static GRFFile *_cur_grffile;
GRFFile *_first_grffile;
static SpriteID _cur_spriteid;
static GrfLoadingStage _cur_stage;
static uint32 _nfo_line;

static GRFConfig *_cur_grfconfig;

/* Miscellaneous GRF features, set by Action 0x0D, parameter 0x9E */
static byte _misc_grf_features = 0;

/* 32 * 8 = 256 flags. Apparently TTDPatch uses this many.. */
static uint32 _ttdpatch_flags[8];

/* Used by Action 0x06 to preload a pseudo sprite and modify its content */
static byte *_preload_sprite = NULL;

/* Set if any vehicle is loaded which uses 2cc (two company colours) */
bool _have_2cc = false;

/* Set if there are any newhouses loaded. */
bool _have_newhouses = false;


enum GrfDataType {
	GDT_SOUND,
};

static byte _grf_data_blocks;
static GrfDataType _grf_data_type;


enum grfspec_feature {
	GSF_TRAIN,
	GSF_ROAD,
	GSF_SHIP,
	GSF_AIRCRAFT,
	GSF_STATION,
	GSF_CANAL,
	GSF_BRIDGE,
	GSF_TOWNHOUSE,
	GSF_GLOBALVAR,
	GSF_INDUSTRYTILES,
	GSF_INDUSTRIES,
	GSF_CARGOS,
	GSF_SOUNDFX,
};


typedef void (*SpecialSpriteHandler)(byte *buf, int len);

static const uint _vehcounts[4] = {
	/* GSF_TRAIN */    NUM_TRAIN_ENGINES,
	/* GSF_ROAD */     NUM_ROAD_ENGINES,
	/* GSF_SHIP */     NUM_SHIP_ENGINES,
	/* GSF_AIRCRAFT */ NUM_AIRCRAFT_ENGINES
};

static const uint _vehshifts[4] = {
	/* GSF_TRAIN */    0,
	/* GSF_ROAD */     ROAD_ENGINES_INDEX,
	/* GSF_SHIP */     SHIP_ENGINES_INDEX,
	/* GSF_AIRCRAFT */ AIRCRAFT_ENGINES_INDEX,
};

enum {
	MAX_STATIONS = 256,
};

static uint16 cargo_allowed[TOTAL_NUM_ENGINES];
static uint16 cargo_disallowed[TOTAL_NUM_ENGINES];

/* Contains the GRF ID of the owner of a vehicle if it has been reserved */
static uint32 _grm_engines[TOTAL_NUM_ENGINES];

/** DEBUG() function dedicated to newGRF debugging messages
 * Function is essentialy the same as DEBUG(grf, severity, ...) with the
 * addition of file:line information when parsing grf files.
 * NOTE: for the above reason(s) grfmsg() should ONLY be used for
 * loading/parsing grf files, not for runtime debug messages as there
 * is no file information available during that time.
 * @param severity debugging severity level, see debug.h
 * @param str message in printf() format */
void CDECL grfmsg(int severity, const char *str, ...)
{
	char buf[1024];
	va_list va;

	va_start(va, str);
	vsnprintf(buf, sizeof(buf), str, va);
	va_end(va);

	DEBUG(grf, severity, "[%s:%d] %s", _cur_grfconfig->filename, _nfo_line, buf);
}

static inline bool check_length(int real, int wanted, const char *str)
{
	if (real >= wanted) return true;
	grfmsg(0, "%s: Invalid pseudo sprite length %d (expected %d)!", str, real, wanted);
	return false;
}

static inline byte grf_load_byte(byte **buf)
{
	return *(*buf)++;
}

static uint16 grf_load_word(byte **buf)
{
	uint16 val = grf_load_byte(buf);
	return val | (grf_load_byte(buf) << 8);
}

static uint16 grf_load_extended(byte** buf)
{
	uint16 val;
	val = grf_load_byte(buf);
	if (val == 0xFF) val = grf_load_word(buf);
	return val;
}

static uint32 grf_load_dword(byte **buf)
{
	uint32 val = grf_load_word(buf);
	return val | (grf_load_word(buf) << 16);
}

static uint32 grf_load_var(byte size, byte **buf)
{
	switch (size) {
		case 1: return grf_load_byte(buf);
		case 2: return grf_load_word(buf);
		case 4: return grf_load_dword(buf);
		default:
			NOT_REACHED();
			return 0;
	}
}

static const char *grf_load_string(byte **buf, size_t max_len)
{
	const char *string   = *(const char **)buf;
	size_t string_length = ttd_strnlen(string, max_len);

	if (string_length == max_len) {
		/* String was not NUL terminated, so make sure it is now. */
		(*buf)[string_length - 1] = '\0';
		grfmsg(7, "String was not terminated with a zero byte.");
	} else {
		/* Increase the string length to include the NUL byte. */
		string_length++;
	}
	*buf += string_length;

	return string;
}

static GRFFile *GetFileByGRFID(uint32 grfid)
{
	GRFFile *file;

	for (file = _first_grffile; file != NULL; file = file->next) {
		if (file->grfid == grfid) break;
	}
	return file;
}

static GRFFile *GetFileByFilename(const char *filename)
{
	GRFFile *file;

	for (file = _first_grffile; file != NULL; file = file->next) {
		if (strcmp(file->filename, filename) == 0) break;
	}
	return file;
}


/** Used when setting an object's property to map to the GRF's strings
 * while taking in consideration the "drift" between TTDPatch string system and OpenTTD's one
 * @param grfid Id of the grf file
 * @param str StringID that we want to have the equivalent in OoenTTD
 * @return the properly adjusted StringID
 */
static StringID MapGRFStringID(uint32 grfid, StringID str)
{
	/* 0xD0 and 0xDC stand for all the TextIDs in the range
	 * of 0xD000 (misc graphics texts) and 0xDC00 (misc persistent texts).
	 * These strings are unique to each grf file, and thus require to be used with the
	 * grfid in which they are declared */
	if (GB(str, 8, 8) == 0xD0 || GB(str, 8, 8) == 0xDC) {
		return GetGRFStringID(grfid, str);
	}

	/* We have some changes in our cargo strings, resulting in some missing. */
	if (str >= 0x006E && str <= 0x008D) return str - 0x20;
	if (str >= 0x008E && str <= 0x00AD) return str - 0x20;

	/* Map building names according to our lang file changes
	 * 0x200F = Tall Office Block, first house name in the original data, the one that TTDPatch stil uses
	 * 0x201F = Old houses is the last house name.
	 * OpenTTD does not have exactly the same order aymore, so, the code below allows
	 * to compensate for the difference */
	if (str >= 0x200F && str <= 0x201F) return str + (STR_200F_TALL_OFFICE_BLOCK - 0x200F);

	return str;
}

static uint8 MapDOSColour(uint8 colour)
{
	if (_use_dos_palette) return colour;

	if (colour < 10) {
		static uint8 dos_to_win_colour_map[] = { 0, 215, 216, 136, 88, 106, 32, 33, 40, 245 };
		return dos_to_win_colour_map[colour];
	}

	if (colour >= 245 && colour < 254) return colour - 28;

	return colour;
}


typedef bool (*VCI_Handler)(uint engine, int numinfo, int prop, byte **buf, int len);

#define FOR_EACH_OBJECT for (i = 0; i < numinfo; i++)

static void dewagonize(int condition, int engine)
{
	EngineInfo *ei = &_engine_info[engine];
	RailVehicleInfo *rvi = &_rail_vehicle_info[engine];

	if (condition != 0) {
		ei->unk2 &= ~0x80;
		if (rvi->railveh_type == RAILVEH_WAGON)
			rvi->railveh_type = RAILVEH_SINGLEHEAD;
	} else {
		ei->unk2 |= 0x80;
		rvi->railveh_type = RAILVEH_WAGON;
	}
}

static bool RailVehicleChangeInfo(uint engine, int numinfo, int prop, byte **bufp, int len)
{
	EngineInfo *ei = &_engine_info[engine];
	RailVehicleInfo *rvi = &_rail_vehicle_info[engine];
	byte *buf = *bufp;
	int i;
	bool ret = false;

	switch (prop) {
		case 0x05: // Track type
			FOR_EACH_OBJECT {
				uint8 tracktype = grf_load_byte(&buf);

				switch (tracktype) {
					case 0: rvi[i].railtype = rvi[i].engclass == 2 ? RAILTYPE_ELECTRIC : RAILTYPE_RAIL; break;
					case 1: rvi[i].railtype = RAILTYPE_MONO; break;
					case 2: rvi[i].railtype = RAILTYPE_MAGLEV; break;
					default:
						grfmsg(1, "RailVehicleChangeInfo: Invalid track type %d specified, ignoring", tracktype);
						break;
				}
			}
			break;

		case 0x08: // AI passenger service
			/* @todo missing feature */
			FOR_EACH_OBJECT grf_load_byte(&buf);
			ret = true;
			break;

		case 0x09: // Speed (1 unit is 1 kmh)
			FOR_EACH_OBJECT {
				uint16 speed = grf_load_word(&buf);
				if (speed == 0xFFFF) speed = 0;

				rvi[i].max_speed = speed;
			}
			break;

		case 0x0B: // Power
			FOR_EACH_OBJECT {
				uint16 power = grf_load_word(&buf);

				if (rvi[i].railveh_type == RAILVEH_MULTIHEAD) power /= 2;

				rvi[i].power = power;
				dewagonize(power, engine + i);
			}
			break;

		case 0x0D: // Running cost factor
			FOR_EACH_OBJECT {
				uint8 runcostfact = grf_load_byte(&buf);

				if (rvi[i].railveh_type == RAILVEH_MULTIHEAD) runcostfact /= 2;

				rvi[i].running_cost_base = runcostfact;
			}
			break;

		case 0x0E: // Running cost base
			FOR_EACH_OBJECT {
				uint32 base = grf_load_dword(&buf);

				switch (base) {
					case 0x4C30: rvi[i].running_cost_class = 0; break;
					case 0x4C36: rvi[i].running_cost_class = 1; break;
					case 0x4C3C: rvi[i].running_cost_class = 2; break;
					case 0: break; // Used by wagons
					default:
						grfmsg(1, "RailVehicleChangeInfo: Unsupported running cost base 0x%04X, ignoring", base);
						break;
				}
			}
			break;

		case 0x12: // Sprite ID
			FOR_EACH_OBJECT {
				uint8 spriteid = grf_load_byte(&buf);

				/* TTD sprite IDs point to a location in a 16bit array, but we use it
				 * as an array index, so we need it to be half the original value. */
				if (spriteid < 0xFD) spriteid >>= 1;

				rvi[i].image_index = spriteid;
			}
			break;

		case 0x13: // Dual-headed
			FOR_EACH_OBJECT {
				uint8 dual = grf_load_byte(&buf);

				if (dual != 0) {
					if (rvi[i].railveh_type != RAILVEH_MULTIHEAD) {
						// adjust power and running cost if needed
						rvi[i].power /= 2;
						rvi[i].running_cost_base /= 2;
					}
					rvi[i].railveh_type = RAILVEH_MULTIHEAD;
				} else {
					if (rvi[i].railveh_type == RAILVEH_MULTIHEAD) {
						// adjust power and running cost if needed
						rvi[i].power *= 2;
						rvi[i].running_cost_base *= 2;
					}
					rvi[i].railveh_type = rvi[i].power == 0 ?
						RAILVEH_WAGON : RAILVEH_SINGLEHEAD;
				}
			}
			break;

		case 0x14: // Cargo capacity
			FOR_EACH_OBJECT rvi[i].capacity = grf_load_byte(&buf);
			break;

		case 0x15: // Cargo type
			FOR_EACH_OBJECT {
				uint8 ctype = grf_load_byte(&buf);

				if (ctype < NUM_CARGO && HASBIT(_cargo_mask, ctype)) {
					rvi[i].cargo_type = ctype;
				} else {
					rvi[i].cargo_type = CT_INVALID;
					grfmsg(2, "RailVehicleChangeInfo: Invalid cargo type %d, using first refittable", ctype);
				}
			}
			break;

		case 0x16: // Weight
			FOR_EACH_OBJECT SB(rvi[i].weight, 0, 8, grf_load_byte(&buf));
			break;

		case 0x17: // Cost factor
			FOR_EACH_OBJECT rvi[i].base_cost = grf_load_byte(&buf);
			break;

		case 0x18: // AI rank
			FOR_EACH_OBJECT rvi[i].ai_rank = grf_load_byte(&buf);
			break;

		case 0x19: // Engine traction type
			/* What do the individual numbers mean?
			 * 0x00 .. 0x07: Steam
			 * 0x08 .. 0x27: Diesel
			 * 0x28 .. 0x31: Electric
			 * 0x32 .. 0x37: Monorail
			 * 0x38 .. 0x41: Maglev
			 */
			FOR_EACH_OBJECT {
				uint8 traction = grf_load_byte(&buf);
				int engclass;

				if (traction <= 0x07) {
					engclass = 0;
				} else if (traction <= 0x27) {
					engclass = 1;
				} else if (traction <= 0x31) {
					engclass = 2;
				} else if (traction <= 0x41) {
					engclass = 2;
				} else {
					break;
				}
				if (rvi[i].railtype == RAILTYPE_RAIL     && engclass == 2) rvi[i].railtype = RAILTYPE_ELECTRIC;
				if (rvi[i].railtype == RAILTYPE_ELECTRIC && engclass != 2) rvi[i].railtype = RAILTYPE_RAIL;

				rvi[i].engclass = engclass;
			}
			break;

		case 0x1A: // Alter purchase list sort order
			FOR_EACH_OBJECT {
				EngineID pos = grf_load_byte(&buf);

				if (pos < NUM_TRAIN_ENGINES) {
					AlterRailVehListOrder(engine + i, pos);
				} else {
					grfmsg(2, "RailVehicleChangeInfo: Invalid train engine ID %d, ignoring", pos);
				}
			}
			break;

		case 0x1B: // Powered wagons power bonus
			FOR_EACH_OBJECT rvi[i].pow_wag_power = grf_load_word(&buf);
			break;

		case 0x1C: // Refit cost
			FOR_EACH_OBJECT ei[i].refit_cost = grf_load_byte(&buf);
			break;

		case 0x1D: // Refit cargo
			FOR_EACH_OBJECT ei[i].refit_mask = grf_load_dword(&buf);
			break;

		case 0x1E: // Callback
			FOR_EACH_OBJECT ei[i].callbackmask = grf_load_byte(&buf);
			break;

		case 0x1F: // Tractive effort coefficient
			FOR_EACH_OBJECT rvi[i].tractive_effort = grf_load_byte(&buf);
			break;

		case 0x21: // Shorter vehicle
			FOR_EACH_OBJECT rvi[i].shorten_factor = grf_load_byte(&buf);
			break;

		case 0x22: // Visual effect
			/* see note in engine.h about rvi->visual_effect */
			FOR_EACH_OBJECT rvi[i].visual_effect = grf_load_byte(&buf);
			break;

		case 0x23: // Powered wagons weight bonus
			FOR_EACH_OBJECT rvi[i].pow_wag_weight = grf_load_byte(&buf);
			break;

		case 0x24: // High byte of vehicle weight
			FOR_EACH_OBJECT {
				byte weight = grf_load_byte(&buf);

				if (weight > 4) {
					grfmsg(2, "RailVehicleChangeInfo: Nonsensical weight of %d tons, ignoring", weight << 8);
				} else {
					SB(rvi[i].weight, 8, 8, weight);
				}
			}
			break;

		case 0x25: // User-defined bit mask to set when checking veh. var. 42
			FOR_EACH_OBJECT rvi[i].user_def_data = grf_load_byte(&buf);
			break;

		case 0x27: // Miscellaneous flags
			FOR_EACH_OBJECT {
				ei[i].misc_flags = grf_load_byte(&buf);
				if (HASBIT(ei[i].misc_flags, EF_USES_2CC)) _have_2cc = true;
			}
			break;

		case 0x28: // Cargo classes allowed
			FOR_EACH_OBJECT cargo_allowed[engine + i] = grf_load_word(&buf);
			break;

		case 0x29: // Cargo classes disallowed
			FOR_EACH_OBJECT cargo_disallowed[engine + i] = grf_load_word(&buf);
			break;

		case 0x2A: // Long format introduction date (days since year 0)
			FOR_EACH_OBJECT ei[i].base_intro = grf_load_dword(&buf);
			break;

		/* @todo air drag and retire vehicle early
		 * Fall-through for unimplemented one byte long properties. */
		case 0x20: // Air drag
		case 0x26: // Retire vehicle early
			FOR_EACH_OBJECT grf_load_byte(&buf);
			ret = true;
			break;

		default:
			ret = true;
			break;
	}
	*bufp = buf;
	return ret;
}

static bool RoadVehicleChangeInfo(uint engine, int numinfo, int prop, byte **bufp, int len)
{
	EngineInfo *ei = &_engine_info[ROAD_ENGINES_INDEX + engine];
	RoadVehicleInfo *rvi = &_road_vehicle_info[engine];
	byte *buf = *bufp;
	int i;
	bool ret = false;

	switch (prop) {
		case 0x08: // Speed (1 unit is 0.5 kmh)
			FOR_EACH_OBJECT rvi[i].max_speed = grf_load_byte(&buf);
			break;

		case 0x09: // Running cost factor
			FOR_EACH_OBJECT rvi[i].running_cost = grf_load_byte(&buf);
			break;

		case 0x0A: // Running cost base
			/* @todo : I have no idea. --pasky
			 * I THINK it is used for overriding the base cost of all road vehicle (_price.roadveh_base) --belugas */
			FOR_EACH_OBJECT grf_load_dword(&buf);
			ret = true;
			break;

		case 0x0E: // Sprite ID
			FOR_EACH_OBJECT {
				uint8 spriteid = grf_load_byte(&buf);

				/* cars have different custom id in the GRF file */
				if (spriteid == 0xFF) spriteid = 0xFD;

				if (spriteid < 0xFD) spriteid >>= 1;

				rvi[i].image_index = spriteid;
			}
			break;

		case 0x0F: // Cargo capacity
			FOR_EACH_OBJECT rvi[i].capacity = grf_load_byte(&buf);
			break;

		case 0x10: // Cargo type
			FOR_EACH_OBJECT {
				uint8 cargo = grf_load_byte(&buf);

				if (cargo < NUM_CARGO && HASBIT(_cargo_mask, cargo)) {
					rvi[i].cargo_type = cargo;
				} else {
					rvi[i].cargo_type = CT_INVALID;
					grfmsg(2, "RoadVehicleChangeInfo: Invalid cargo type %d, using first refittable", cargo);
				}
			}
			break;

		case 0x11: // Cost factor
			FOR_EACH_OBJECT rvi[i].base_cost = grf_load_byte(&buf); // ?? is it base_cost?
			break;

		case 0x12: // SFX
			FOR_EACH_OBJECT rvi[i].sfx = (SoundFx)grf_load_byte(&buf);
			break;

		case 0x13: // Power in 10hp
		case 0x14: // Weight in 1/4 tons
		case 0x15: // Speed in mph*0.8
			/* TODO: Support for road vehicles realistic power
			 * computations (called rvpower in TTDPatch) is just
			 * missing in OTTD yet. --pasky */
			FOR_EACH_OBJECT grf_load_byte(&buf);
			ret = true;
			break;

		case 0x16: // Cargos available for refitting
			FOR_EACH_OBJECT ei[i].refit_mask = grf_load_dword(&buf);
			break;

		case 0x17: // Callback mask
			FOR_EACH_OBJECT ei[i].callbackmask = grf_load_byte(&buf);
			break;

		case 0x1A: // Refit cost
			FOR_EACH_OBJECT ei[i].refit_cost = grf_load_byte(&buf);
			break;

		case 0x1C: // Miscellaneous flags
			FOR_EACH_OBJECT {
				ei[i].misc_flags = grf_load_byte(&buf);
				if (HASBIT(ei[i].misc_flags, EF_USES_2CC)) _have_2cc = true;
			}
			break;

		case 0x1D: // Cargo classes allowed
			FOR_EACH_OBJECT cargo_allowed[ROAD_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x1E: // Cargo classes disallowed
			FOR_EACH_OBJECT cargo_disallowed[ROAD_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x1F: // Long format introduction date (days since year 0)
			FOR_EACH_OBJECT ei[i].base_intro = grf_load_dword(&buf);
			break;

		case 0x18: // Tractive effort
		case 0x19: // Air drag
		case 0x1B: // Retire vehicle early
			/* @todo */
			FOR_EACH_OBJECT grf_load_byte(&buf);
			ret = true;
			break;

		default:
			ret = true;
			break;
	}

	*bufp = buf;
	return ret;
}

static bool ShipVehicleChangeInfo(uint engine, int numinfo, int prop, byte **bufp, int len)
{
	EngineInfo *ei = &_engine_info[SHIP_ENGINES_INDEX + engine];
	ShipVehicleInfo *svi = &_ship_vehicle_info[engine];
	byte *buf = *bufp;
	int i;
	bool ret = false;

	//printf("e %x prop %x?\n", engine, prop);
	switch (prop) {
		case 0x08: // Sprite ID
			FOR_EACH_OBJECT {
				uint8 spriteid = grf_load_byte(&buf);

				/* ships have different custom id in the GRF file */
				if (spriteid == 0xFF) spriteid = 0xFD;

				if (spriteid < 0xFD) spriteid >>= 1;

				svi[i].image_index = spriteid;
			}
			break;

		case 0x09: // Refittable
			FOR_EACH_OBJECT svi[i].refittable = (grf_load_byte(&buf) != 0);
			break;

		case 0x0A: // Cost factor
			FOR_EACH_OBJECT svi[i].base_cost = grf_load_byte(&buf); // ?? is it base_cost?
			break;

		case 0x0B: // Speed (1 unit is 0.5 kmh)
			FOR_EACH_OBJECT svi[i].max_speed = grf_load_byte(&buf);
			break;

		case 0x0C: // Cargo type
			FOR_EACH_OBJECT {
				uint8 cargo = grf_load_byte(&buf);

				if (cargo < NUM_CARGO && HASBIT(_cargo_mask, cargo)) {
					svi[i].cargo_type = cargo;
				} else {
					svi[i].cargo_type = CT_INVALID;
					grfmsg(2, "ShipVehicleChangeInfo: Invalid cargo type %d, using first refittable", cargo);
				}
			}
			break;

		case 0x0D: // Cargo capacity
			FOR_EACH_OBJECT svi[i].capacity = grf_load_word(&buf);
			break;

		case 0x0F: // Running cost factor
			FOR_EACH_OBJECT svi[i].running_cost = grf_load_byte(&buf);
			break;

		case 0x10: // SFX
			FOR_EACH_OBJECT svi[i].sfx = (SoundFx)grf_load_byte(&buf);
			break;

		case 0x11: // Cargos available for refitting
			FOR_EACH_OBJECT ei[i].refit_mask = grf_load_dword(&buf);
			break;

		case 0x12: // Callback mask
			FOR_EACH_OBJECT ei[i].callbackmask = grf_load_byte(&buf);
			break;

		case 0x13: // Refit cost
			FOR_EACH_OBJECT ei[i].refit_cost = grf_load_byte(&buf);
			break;

		case 0x17: // Miscellaneous flags
			FOR_EACH_OBJECT {
				ei[i].misc_flags = grf_load_byte(&buf);
				if (HASBIT(ei[i].misc_flags, EF_USES_2CC)) _have_2cc = true;
			}
			break;

		case 0x18: // Cargo classes allowed
			FOR_EACH_OBJECT cargo_allowed[SHIP_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x19: // Cargo classes disallowed
			FOR_EACH_OBJECT cargo_disallowed[SHIP_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x1A: // Long format introduction date (days since year 0)
			FOR_EACH_OBJECT ei[i].base_intro = grf_load_dword(&buf);
			break;

		case 0x14: // Ocean speed fraction
		case 0x15: // Canal speed fraction
		case 0x16: // Retire vehicle early
			/* @todo */
			FOR_EACH_OBJECT grf_load_byte(&buf);
			ret = true;
			break;

		default:
			ret = true;
			break;
	}

	*bufp = buf;
	return ret;
}

static bool AircraftVehicleChangeInfo(uint engine, int numinfo, int prop, byte **bufp, int len)
{
	EngineInfo *ei = &_engine_info[AIRCRAFT_ENGINES_INDEX + engine];
	AircraftVehicleInfo *avi = &_aircraft_vehicle_info[engine];
	byte *buf = *bufp;
	int i;
	bool ret = false;

	//printf("e %x prop %x?\n", engine, prop);
	switch (prop) {
		case 0x08: // Sprite ID
			FOR_EACH_OBJECT {
				uint8 spriteid = grf_load_byte(&buf);

				/* aircraft have different custom id in the GRF file */
				if (spriteid == 0xFF) spriteid = 0xFD;

				if (spriteid < 0xFD) spriteid >>= 1;

				avi[i].image_index = spriteid;
			}
			break;

		case 0x09: // Helicopter
			FOR_EACH_OBJECT {
				if (grf_load_byte(&buf) == 0) {
					avi[i].subtype = AIR_HELI;
				} else {
					SB(avi[i].subtype, 0, 1, 1); // AIR_CTOL
				}
			}
			break;

		case 0x0A: // Large
			FOR_EACH_OBJECT SB(avi[i].subtype, 1, 1, (grf_load_byte(&buf) != 0 ? 1 : 0)); // AIR_FAST
			break;

		case 0x0B: // Cost factor
			FOR_EACH_OBJECT avi[i].base_cost = grf_load_byte(&buf); // ?? is it base_cost?
			break;

		case 0x0C: // Speed (1 unit is 8 mph, we translate to 1 unit is 1 km/h)
			FOR_EACH_OBJECT avi[i].max_speed = (grf_load_byte(&buf) * 129) / 10;
			break;

		case 0x0D: // Acceleration
			FOR_EACH_OBJECT avi[i].acceleration = (grf_load_byte(&buf) * 129) / 10;
			break;

		case 0x0E: // Running cost factor
			FOR_EACH_OBJECT avi[i].running_cost = grf_load_byte(&buf);
			break;

		case 0x0F: // Passenger capacity
			FOR_EACH_OBJECT avi[i].passenger_capacity = grf_load_word(&buf);
			break;

		case 0x11: // Mail capacity
			FOR_EACH_OBJECT avi[i].mail_capacity = grf_load_byte(&buf);
			break;

		case 0x12: // SFX
			FOR_EACH_OBJECT avi[i].sfx = (SoundFx)grf_load_byte(&buf);
			break;

		case 0x13: // Cargos available for refitting
			FOR_EACH_OBJECT ei[i].refit_mask = grf_load_dword(&buf);
			break;

		case 0x14: // Callback mask
			FOR_EACH_OBJECT ei[i].callbackmask = grf_load_byte(&buf);
			break;

		case 0x15: // Refit cost
			FOR_EACH_OBJECT ei[i].refit_cost = grf_load_byte(&buf);
			break;

		case 0x17: // Miscellaneous flags
			FOR_EACH_OBJECT {
				ei[i].misc_flags = grf_load_byte(&buf);
				if (HASBIT(ei[i].misc_flags, EF_USES_2CC)) _have_2cc = true;
			}
			break;

		case 0x18: // Cargo classes allowed
			FOR_EACH_OBJECT cargo_allowed[AIRCRAFT_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x19: // Cargo classes disallowed
			FOR_EACH_OBJECT cargo_disallowed[AIRCRAFT_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x1A: // Long format introduction date (days since year 0)
			FOR_EACH_OBJECT ei[i].base_intro = grf_load_dword(&buf);
			break;

		case 0x16: // Retire vehicle early
			/* @todo */
			FOR_EACH_OBJECT grf_load_byte(&buf);
			ret = true;
			break;

		default:
			ret = true;
			break;
	}

	*bufp = buf;
	return ret;
}

static bool StationChangeInfo(uint stid, int numinfo, int prop, byte **bufp, int len)
{
	StationSpec **statspec;
	byte *buf = *bufp;
	int i;
	bool ret = false;

	if (stid + numinfo > MAX_STATIONS) {
		grfmsg(1, "StationChangeInfo: Station %u is invalid, max %u, ignoring", stid + numinfo, MAX_STATIONS);
		return false;
	}

	/* Allocate station specs if necessary */
	if (_cur_grffile->stations == NULL) _cur_grffile->stations = CallocT<StationSpec*>(MAX_STATIONS);

	statspec = &_cur_grffile->stations[stid];

	if (prop != 0x08) {
		/* Check that all stations we are modifying are defined. */
		FOR_EACH_OBJECT {
			if (statspec[i] == NULL) {
				grfmsg(2, "StationChangeInfo: Attempt to modify undefined station %u, ignoring", stid + i);
				return false;
			}
		}
	}

	switch (prop) {
		case 0x08: // Class ID
			FOR_EACH_OBJECT {
				/* Property 0x08 is special; it is where the station is allocated */
				if (statspec[i] == NULL) statspec[i] = CallocT<StationSpec>(1);

				/* Swap classid because we read it in BE meaning WAYP or DFLT */
				uint32 classid = grf_load_dword(&buf);
				statspec[i]->sclass = AllocateStationClass(BSWAP32(classid));
			}
			break;

		case 0x09: // Define sprite layout
			FOR_EACH_OBJECT {
				StationSpec *statspec = _cur_grffile->stations[stid + i];

				statspec->tiles = grf_load_extended(&buf);
				statspec->renderdata = CallocT<DrawTileSprites>(statspec->tiles);
				statspec->copied_renderdata = false;

				for (uint t = 0; t < statspec->tiles; t++) {
					DrawTileSprites *dts = &statspec->renderdata[t];
					uint seq_count = 0;

					dts->seq = NULL;
					dts->ground_sprite = grf_load_word(&buf);
					dts->ground_pal = grf_load_word(&buf);
					if (dts->ground_sprite == 0) continue;
					if (HASBIT(dts->ground_pal, 15)) {
						CLRBIT(dts->ground_pal, 15);
						SETBIT(dts->ground_sprite, SPRITE_MODIFIER_USE_OFFSET);
					}

					while (buf < *bufp + len) {
						DrawTileSeqStruct *dtss;

						/* no relative bounding box support */
						dts->seq = ReallocT((DrawTileSeqStruct*)dts->seq, ++seq_count);
						dtss = (DrawTileSeqStruct*) &dts->seq[seq_count - 1];

						dtss->delta_x = grf_load_byte(&buf);
						if ((byte) dtss->delta_x == 0x80) break;
						dtss->delta_y = grf_load_byte(&buf);
						dtss->delta_z = grf_load_byte(&buf);
						dtss->size_x = grf_load_byte(&buf);
						dtss->size_y = grf_load_byte(&buf);
						dtss->size_z = grf_load_byte(&buf);
						dtss->image = grf_load_word(&buf);
						dtss->pal = grf_load_word(&buf);

						/* Remap flags as ours collide */
						if (HASBIT(dtss->pal, 15)) {
							CLRBIT(dtss->pal, 15);
							SETBIT(dtss->image, SPRITE_MODIFIER_USE_OFFSET);
						}

						if (HASBIT(dtss->image, 15)) {
							CLRBIT(dtss->image, 15);
							SETBIT(dtss->image, PALETTE_MODIFIER_COLOR);
						}
						if (HASBIT(dtss->image, 14)) {
							CLRBIT(dtss->image, 14);
							SETBIT(dtss->image, PALETTE_MODIFIER_TRANSPARENT);
						}
					}
				}
			}
			break;

		case 0x0A: // Copy sprite layout
			FOR_EACH_OBJECT {
				StationSpec *statspec = _cur_grffile->stations[stid + i];
				byte srcid = grf_load_byte(&buf);
				const StationSpec *srcstatspec = _cur_grffile->stations[srcid];

				statspec->tiles = srcstatspec->tiles;
				statspec->renderdata = srcstatspec->renderdata;
				statspec->copied_renderdata = true;
			}
			break;

		case 0x0B: // Callback mask
			FOR_EACH_OBJECT statspec[i]->callbackmask = grf_load_byte(&buf);
			break;

		case 0x0C: // Disallowed number of platforms
			FOR_EACH_OBJECT statspec[i]->disallowed_platforms = grf_load_byte(&buf);
			break;

		case 0x0D: // Disallowed platform lengths
			FOR_EACH_OBJECT statspec[i]->disallowed_lengths = grf_load_byte(&buf);
			break;

		case 0x0E: // Define custom layout
			FOR_EACH_OBJECT {
				StationSpec *statspec = _cur_grffile->stations[stid + i];

				statspec->copied_layouts = false;

				while (buf < *bufp + len) {
					byte length = grf_load_byte(&buf);
					byte number = grf_load_byte(&buf);
					StationLayout layout;
					uint l, p;

					if (length == 0 || number == 0) break;

					//debug("l %d > %d ?", length, stat->lengths);
					if (length > statspec->lengths) {
						statspec->platforms = ReallocT(statspec->platforms, length);
						memset(statspec->platforms + statspec->lengths, 0, length - statspec->lengths);

						statspec->layouts = ReallocT(statspec->layouts, length);
						memset(statspec->layouts + statspec->lengths, 0,
						       (length - statspec->lengths) * sizeof(*statspec->layouts));

						statspec->lengths = length;
					}
					l = length - 1; // index is zero-based

					//debug("p %d > %d ?", number, stat->platforms[l]);
					if (number > statspec->platforms[l]) {
						statspec->layouts[l] = ReallocT(statspec->layouts[l], number);
						/* We expect NULL being 0 here, but C99 guarantees that. */
						memset(statspec->layouts[l] + statspec->platforms[l], 0,
						       (number - statspec->platforms[l]) * sizeof(**statspec->layouts));

						statspec->platforms[l] = number;
					}

					p = 0;
					layout = MallocT<byte>(length * number);
					for (l = 0; l < length; l++) {
						for (p = 0; p < number; p++) {
							layout[l * number + p] = grf_load_byte(&buf);
						}
					}

					l--;
					p--;
					free(statspec->layouts[l][p]);
					statspec->layouts[l][p] = layout;
				}
			}
			break;

		case 0x0F: // Copy custom layout
			FOR_EACH_OBJECT {
				StationSpec *statspec = _cur_grffile->stations[stid + i];
				byte srcid = grf_load_byte(&buf);
				const StationSpec *srcstatspec = _cur_grffile->stations[srcid];

				statspec->lengths   = srcstatspec->lengths;
				statspec->platforms = srcstatspec->platforms;
				statspec->layouts   = srcstatspec->layouts;
				statspec->copied_layouts = true;
			}
			break;

		case 0x10: // Little/lots cargo threshold
			FOR_EACH_OBJECT statspec[i]->cargo_threshold = grf_load_word(&buf);
			break;

		case 0x11: // Pylon placement
			FOR_EACH_OBJECT statspec[i]->pylons = grf_load_byte(&buf);
			break;

		case 0x12: // Cargo types for random triggers
			FOR_EACH_OBJECT statspec[i]->cargo_triggers = grf_load_dword(&buf);
			break;

		case 0x13: // General flags
			FOR_EACH_OBJECT statspec[i]->flags = grf_load_byte(&buf);
			break;

		case 0x14: // Overhead wire placement
			FOR_EACH_OBJECT statspec[i]->wires = grf_load_byte(&buf);
			break;

		case 0x15: // Blocked tiles
			FOR_EACH_OBJECT statspec[i]->blocked = grf_load_byte(&buf);
			break;

		case 0x16: // @todo Animation info
			FOR_EACH_OBJECT grf_load_word(&buf);
			ret = true;
			break;

		case 0x17: // @todo Animation speed
			FOR_EACH_OBJECT grf_load_byte(&buf);
			ret = true;
			break;

		case 0x18: // @todo Animation triggers
			FOR_EACH_OBJECT grf_load_word(&buf);
			ret = true;
			break;

		default:
			ret = true;
			break;
	}

	*bufp = buf;
	return ret;
}

static bool BridgeChangeInfo(uint brid, int numinfo, int prop, byte **bufp, int len)
{
	byte *buf = *bufp;
	int i;
	bool ret = false;

	switch (prop) {
		case 0x08: // Year of availability
			FOR_EACH_OBJECT _bridge[brid + i].avail_year = ORIGINAL_BASE_YEAR + grf_load_byte(&buf);
			break;

		case 0x09: // Minimum length
			FOR_EACH_OBJECT _bridge[brid + i].min_length = grf_load_byte(&buf);
			break;

		case 0x0A: // Maximum length
			FOR_EACH_OBJECT _bridge[brid + i].max_length = grf_load_byte(&buf);
			break;

		case 0x0B: // Cost factor
			FOR_EACH_OBJECT _bridge[brid + i].price = grf_load_byte(&buf);
			break;

		case 0x0C: // Maximum speed
			FOR_EACH_OBJECT _bridge[brid + i].speed = grf_load_word(&buf);
			break;

		case 0x0D: // Bridge sprite tables
			FOR_EACH_OBJECT {
				Bridge *bridge = &_bridge[brid + i];
				byte tableid = grf_load_byte(&buf);
				byte numtables = grf_load_byte(&buf);

				if (bridge->sprite_table == NULL) {
					/* Allocate memory for sprite table pointers and zero out */
					bridge->sprite_table = CallocT<PalSpriteID*>(7);
				}

				for (; numtables-- != 0; tableid++) {
					if (tableid >= 7) { // skip invalid data
						grfmsg(1, "BridgeChangeInfo: Table %d >= 7, skipping", tableid);
						for (byte sprite = 0; sprite < 32; sprite++) grf_load_dword(&buf);
						continue;
					}

					if (bridge->sprite_table[tableid] == NULL) {
						bridge->sprite_table[tableid] = MallocT<PalSpriteID>(32);
					}

					for (byte sprite = 0; sprite < 32; sprite++) {
						SpriteID image = grf_load_word(&buf);
						SpriteID pal   = grf_load_word(&buf);

						if (HASBIT(pal, 15)) {
							SETBIT(image, PALETTE_MODIFIER_TRANSPARENT);
						}

						/* Clear old color modifer bit */
						CLRBIT(image, 15);

						bridge->sprite_table[tableid][sprite].sprite = image;
						bridge->sprite_table[tableid][sprite].pal    = pal;
					}
				}
			}
			break;

		case 0x0E: // Flags; bit 0 - disable far pillars
			FOR_EACH_OBJECT _bridge[brid + i].flags = grf_load_byte(&buf);
			break;

		case 0x0F: // Long format year of availability (year since year 0)
			FOR_EACH_OBJECT _bridge[brid + i].avail_year = clamp(grf_load_dword(&buf), MIN_YEAR, MAX_YEAR);
			break;

		default:
			ret = true;
	}

	*bufp = buf;
	return ret;
}

static bool TownHouseChangeInfo(uint hid, int numinfo, int prop, byte **bufp, int len)
{
	HouseSpec **housespec;
	byte *buf = *bufp;
	int i;
	bool ret = false;

	if (hid + numinfo >= HOUSE_MAX) {
		grfmsg(1, "TownHouseChangeInfo: Too many houses loaded (%u), max (%u). Ignoring.", hid + numinfo, HOUSE_MAX-1);
		return false;
	}

	/* Allocate house specs if they haven't been allocated already. */
	if (_cur_grffile->housespec == NULL) {
		_cur_grffile->housespec = CallocT<HouseSpec*>(HOUSE_MAX);

		/* Reset any overrides that have been set. */
		ResetHouseOverrides();
	}

	housespec = &_cur_grffile->housespec[hid];

	if (prop != 0x08) {
		/* Check that all the houses being modified have been defined. */
		FOR_EACH_OBJECT {
			if (housespec[i] == NULL) {
				grfmsg(2, "TownHouseChangeInfo: Attempt to modify undefined house %u. Ignoring.", hid + i);
				return false;
			}
		}
	}

	switch (prop) {
		case 0x08: // Substitute building type, and definition of a new house
			FOR_EACH_OBJECT {
				byte subs_id = grf_load_byte(&buf);

				if (subs_id == 0xFF) {
					/* Instead of defining a new house, a substitute house id
					 * of 0xFF disables the old house with the current id. */
					_house_specs[hid + i].enabled = false;
					continue;
				} else if (subs_id >= NEW_HOUSE_OFFSET) {
					/* The substitute id must be one of the original houses. */
					grfmsg(2, "TownHouseChangeInfo: Attempt to use new house %u as substitute house for %u. Ignoring.", subs_id, hid + i);
					return false;
				}

				/* Allocate space for this house. */
				if (housespec[i] == NULL) housespec[i] = CallocT<HouseSpec>(1);

				memcpy(housespec[i], &_house_specs[subs_id], sizeof(_house_specs[subs_id]));

				housespec[i]->enabled = true;
				housespec[i]->local_id = hid + i;
				housespec[i]->substitute_id = subs_id;
				housespec[i]->grffile = _cur_grffile;
				housespec[i]->random_colour[0] = 0x04;  // those 4 random colours are the base colour
				housespec[i]->random_colour[1] = 0x08;  // for all new houses
				housespec[i]->random_colour[2] = 0x0C;  // they stand for red, blue, orange and green
				housespec[i]->random_colour[3] = 0x06;

				/* New houses do not (currently) expect to have a default start
				 * date before 1930, as this breaks the build date stuff. See
				 * FinaliseHouseArray() for more details. */
				if (housespec[i]->min_date < 1930) housespec[i]->min_date = 1930;
			}
			_have_newhouses = true;
			break;

		case 0x09: // Building flags
			FOR_EACH_OBJECT {
				byte state = grf_load_byte(&buf);
				housespec[i]->building_flags = (BuildingFlags)state;
			}
			break;

		case 0x0A: // Availability years
			FOR_EACH_OBJECT {
				uint16 years = grf_load_word(&buf);
				housespec[i]->min_date = GB(years, 0, 8) > 150 ? MAX_YEAR : ORIGINAL_BASE_YEAR + GB(years, 0, 8);
				housespec[i]->max_date = GB(years, 8, 8) > 150 ? MAX_YEAR : ORIGINAL_BASE_YEAR + GB(years, 8, 8);
			}
			break;

		case 0x0B: // Population
			FOR_EACH_OBJECT housespec[i]->population = grf_load_byte(&buf);
			break;

		case 0x0C: // Mail generation multiplier
			FOR_EACH_OBJECT housespec[i]->mail_generation = grf_load_byte(&buf);
			break;

		case 0x0D: // Passenger acceptance
		case 0x0E: // Mail acceptance
			FOR_EACH_OBJECT housespec[i]->cargo_acceptance[prop - 0x0D] = grf_load_byte(&buf);
			break;
		case 0x0F: // Goods/candy, food/fizzy drinks acceptance
			FOR_EACH_OBJECT {
				int8 goods = grf_load_byte(&buf);

				/* If value of goods is negative, it means in fact food or, if in toyland, fizzy_drink acceptance.
				 * Else, we have "standard" 3rd cargo type, goods or candy, for toyland once more */
				housespec[i]->accepts_cargo[2] = (goods >= 0) ? ((_opt.landscape == LT_TOYLAND) ? CT_CANDY : CT_GOODS) :
						((_opt.landscape == LT_TOYLAND) ? CT_FIZZY_DRINKS : CT_FOOD);

				housespec[i]->cargo_acceptance[2] = abs(goods); // but we do need positive value here
			}
			break;

		case 0x10: // Local authority rating decrease on removal
			FOR_EACH_OBJECT housespec[i]->remove_rating_decrease = grf_load_word(&buf);
			break;

		case 0x11: // Removal cost multiplier
			FOR_EACH_OBJECT housespec[i]->removal_cost = grf_load_byte(&buf);
			break;

		case 0x12: // Building name ID
			FOR_EACH_OBJECT housespec[i]->building_name = MapGRFStringID(_cur_grffile->grfid, grf_load_word(&buf));
			break;

		case 0x13: // Building availability mask
			FOR_EACH_OBJECT {
				uint16 avail = grf_load_word(&buf);
				housespec[i]->building_availability = (HouseZones)avail;
			}
			break;

		case 0x14: // House callback flags
			FOR_EACH_OBJECT housespec[i]->callback_mask = grf_load_byte(&buf);
			break;

		case 0x15: // House override byte
			FOR_EACH_OBJECT {
				byte override = grf_load_byte(&buf);

				/* The house being overridden must be an original house. */
				if (override >= NEW_HOUSE_OFFSET) {
					grfmsg(2, "TownHouseChangeInfo: Attempt to override new house %u with house id %u. Ignoring.", override, hid);
					return false;
				}

				AddHouseOverride(hid, override);
			}
			break;

		case 0x16: // Periodic refresh multiplier
			FOR_EACH_OBJECT housespec[i]->processing_time = grf_load_byte(&buf);
			break;

		case 0x17: // Four random colours to use
			FOR_EACH_OBJECT {
				uint j;
				for (j = 0; j < 4; j++) housespec[i]->random_colour[j] = grf_load_byte(&buf);
			}
			break;

		case 0x18: // Relative probability of appearing
			FOR_EACH_OBJECT housespec[i]->probability = grf_load_byte(&buf);
			break;

		case 0x19: // Extra flags
			FOR_EACH_OBJECT {
				byte flags = grf_load_byte(&buf);
				housespec[i]->extra_flags = (HouseExtraFlags)flags;
			}
			break;

		case 0x1A: // Animation frames
			FOR_EACH_OBJECT housespec[i]->animation_frames = grf_load_byte(&buf);
			break;

		case 0x1B: // Animation speed
			FOR_EACH_OBJECT housespec[i]->animation_speed = clamp(grf_load_byte(&buf), 2, 16);
			break;

		case 0x1C: // Class of the building type
			FOR_EACH_OBJECT housespec[i]->class_id = AllocateHouseClassID(grf_load_byte(&buf), _cur_grffile->grfid);
			break;

		case 0x1D: // Callback flags 2
			FOR_EACH_OBJECT housespec[i]->callback_mask |= (grf_load_byte(&buf) << 8);
			break;

		case 0x1E: // Accepted cargo types
			FOR_EACH_OBJECT {
				uint32 cargotypes = grf_load_dword(&buf);

				/* Check if the cargo types should not be changed */
				if (cargotypes == 0xFFFFFFFF) break;

				for (uint j = 0; j < 3; j++) {
					/* Get the cargo number from the 'list' */
					uint8 cargo_part = GB(cargotypes, 8 * j, 8);
					CargoID cargo = GetCargoTranslation(cargo_part, _cur_grffile);

					if (cargo == CT_INVALID) {
						/* Disable acceptance of invalid cargo type */
						housespec[i]->cargo_acceptance[j] = 0;
					} else {
						housespec[i]->accepts_cargo[j] = cargo;
					}
				}
			}
			break;

		default:
			ret = true;
			break;
	}

	*bufp = buf;
	return ret;
}

static bool GlobalVarChangeInfo(uint gvid, int numinfo, int prop, byte **bufp, int len)
{
	byte *buf = *bufp;
	int i;
	bool ret = false;

	switch (prop) {
		case 0x08: /* Cost base factor */
			FOR_EACH_OBJECT {
				byte factor = grf_load_byte(&buf);
				uint price = gvid + i;

				if (price < NUM_PRICES) {
					SetPriceBaseMultiplier(price, factor);
				} else {
					grfmsg(1, "GlobalVarChangeInfo: Price %d out of range, ignoring", price);
				}
			}
			break;

		case 0x09: /* Cargo translation table */
			/* This is loaded during the initialisation stage, so just skip it here. */
			/* Each entry is 4 bytes. */
			buf += numinfo * 4;
			break;

		case 0x0A: // Currency display names
			FOR_EACH_OBJECT {
				uint curidx = GetNewgrfCurrencyIdConverted(gvid + i);
				StringID newone = GetGRFStringID(_cur_grffile->grfid, grf_load_word(&buf));

				if ((newone != STR_UNDEFINED) && (curidx < NUM_CURRENCY)) {
					_currency_specs[curidx].name = newone;
				}
			}
			break;

		case 0x0B: // Currency multipliers
			FOR_EACH_OBJECT {
				uint curidx = GetNewgrfCurrencyIdConverted(gvid + i);
				uint32 rate = grf_load_dword(&buf);

				if (curidx < NUM_CURRENCY) {
					/* TTDPatch uses a multiple of 1000 for its conversion calculations,
					 * which OTTD does not. For this reason, divide grf value by 1000,
					 * to be compatible */
					_currency_specs[curidx].rate = rate / 1000;
				} else {
					grfmsg(1, "GlobalVarChangeInfo: Currency multipliers %d out of range, ignoring", curidx);
				}
			}
			break;

		case 0x0C: // Currency options
			FOR_EACH_OBJECT {
				uint curidx = GetNewgrfCurrencyIdConverted(gvid + i);
				uint16 options = grf_load_word(&buf);

				if (curidx < NUM_CURRENCY) {
					_currency_specs[curidx].separator = GB(options, 0, 8);
					/* By specifying only one bit, we prevent errors,
					 * since newgrf specs said that only 0 and 1 can be set for symbol_pos */
					_currency_specs[curidx].symbol_pos = GB(options, 8, 1);
				} else {
					grfmsg(1, "GlobalVarChangeInfo: Currency option %d out of range, ignoring", curidx);
				}
			}
			break;

		case 0x0D: // Currency prefix symbol
			FOR_EACH_OBJECT {
				uint curidx = GetNewgrfCurrencyIdConverted(gvid + i);
				uint32 tempfix = grf_load_dword(&buf);

				if (curidx < NUM_CURRENCY) {
					memcpy(_currency_specs[curidx].prefix,&tempfix,4);
					_currency_specs[curidx].prefix[4] = 0;
				} else {
					grfmsg(1, "GlobalVarChangeInfo: Currency symbol %d out of range, ignoring", curidx);
				}
			}
			break;

		case 0x0E: // Currency suffix symbol
			FOR_EACH_OBJECT {
				uint curidx = GetNewgrfCurrencyIdConverted(gvid + i);
				uint32 tempfix = grf_load_dword(&buf);

				if (curidx < NUM_CURRENCY) {
					memcpy(&_currency_specs[curidx].suffix, &tempfix, 4);
					_currency_specs[curidx].suffix[4] = 0;
				} else {
					grfmsg(1, "GlobalVarChangeInfo: Currency symbol %d out of range, ignoring", curidx);
				}
			}
			break;

		case 0x0F: //  Euro introduction dates
			FOR_EACH_OBJECT {
				uint curidx = GetNewgrfCurrencyIdConverted(gvid + i);
				Year year_euro = grf_load_word(&buf);

				if (curidx < NUM_CURRENCY) {
					_currency_specs[curidx].to_euro = year_euro;
				} else {
					grfmsg(1, "GlobalVarChangeInfo: Euro intro date %d out of range, ignoring", curidx);
				}
			}
			break;

		case 0x10: // 12 * 32 * B Snow line height table
			if (numinfo > 1 || IsSnowLineSet()) {
				grfmsg(1, "GlobalVarChangeInfo: The snowline can only be set once (%d)", numinfo);
			} else if (len < SNOW_LINE_MONTHS * SNOW_LINE_DAYS) {
				grfmsg(1, "GlobalVarChangeInfo: Not enough entries set in the snowline table (%d)", len);
			} else {
				byte table[SNOW_LINE_MONTHS][SNOW_LINE_DAYS];

				for (uint i = 0; i < SNOW_LINE_MONTHS; i++) {
					for (uint j = 0; j < SNOW_LINE_DAYS; j++) {
						table[i][j] = grf_load_byte(&buf);
					}
				}
				SetSnowLine(table);
			}
			break;

		default:
			ret = true;
	}

	*bufp = buf;
	return ret;
}

static bool CargoChangeInfo(uint cid, int numinfo, int prop, byte **bufp, int len)
{
	if (cid + numinfo > NUM_CARGO) {
		grfmsg(2, "CargoChangeInfo: Cargo type %d out of range (max %d)", cid + numinfo, NUM_CARGO - 1);
		return false;
	}

	CargoSpec *cs = &_cargo[cid];
	byte *buf = *bufp;
	int i;
	bool ret = false;

	switch (prop) {
		case 0x08: /* Bit number of cargo */
			FOR_EACH_OBJECT {
				cs[i].bitnum = grf_load_byte(&buf);
				cs[i].grfid = _cur_grffile->grfid;
				if (cs->IsValid()) {
					SETBIT(_cargo_mask, cid + i);
				} else {
					CLRBIT(_cargo_mask, cid + i);
				}
			}
			break;

		case 0x09: /* String ID for cargo type name */
			FOR_EACH_OBJECT cs[i].name = grf_load_word(&buf);
			break;

		case 0x0A: /* String for cargo name, plural */
			FOR_EACH_OBJECT cs[i].name_plural = grf_load_word(&buf);
			break;

		case 0x0B:
			/* String for units of cargo. This is different in OpenTTD to TTDPatch
			 * (e.g. 10 tonnes of coal) */
			FOR_EACH_OBJECT cs[i].units_volume = grf_load_word(&buf);
			break;

		case 0x0C: /* String for quantity of cargo (e.g. 10 tonnes of coal) */
			FOR_EACH_OBJECT cs[i].quantifier = grf_load_word(&buf);
			break;

		case 0x0D: /* String for two letter cargo abbreviation */
			FOR_EACH_OBJECT cs[i].abbrev = grf_load_word(&buf);
			break;

		case 0x0E: /* Sprite ID for cargo icon */
			FOR_EACH_OBJECT cs[i].sprite = grf_load_word(&buf);
			break;

		case 0x0F: /* Weight of one unit of cargo */
			FOR_EACH_OBJECT cs[i].weight = grf_load_byte(&buf);
			break;

		case 0x10: /* Used for payment calculation */
			FOR_EACH_OBJECT cs[i].transit_days[0] = grf_load_byte(&buf);
			break;

		case 0x11: /* Used for payment calculation */
			FOR_EACH_OBJECT cs[i].transit_days[1] = grf_load_byte(&buf);
			break;

		case 0x12: /* Base cargo price */
			FOR_EACH_OBJECT cs[i].initial_payment = grf_load_dword(&buf);
			break;

		case 0x13: /* Colour for station rating bars */
			FOR_EACH_OBJECT cs[i].rating_colour = MapDOSColour(grf_load_byte(&buf));
			break;

		case 0x14: /* Colour for cargo graph */
			FOR_EACH_OBJECT cs[i].legend_colour = MapDOSColour(grf_load_byte(&buf));
			break;

		case 0x15: /* Freight status */
			FOR_EACH_OBJECT cs[i].is_freight = grf_load_byte(&buf) != 0;
			break;

		case 0x16: /* Cargo classes */
			FOR_EACH_OBJECT cs[i].classes = grf_load_word(&buf);
			break;

		case 0x17: /* Cargo label */
			FOR_EACH_OBJECT {
				cs[i].label = grf_load_dword(&buf);
				cs[i].label = BSWAP32(cs[i].label);
			}
			break;

		case 0x18: /* Town growth substitute type */
			FOR_EACH_OBJECT {
				uint8 substitute_type = grf_load_byte(&buf);
				switch (substitute_type) {
					case 0x00: cs[i].town_effect = TE_PASSENGERS; break;
					case 0x02: cs[i].town_effect = TE_MAIL; break;
					case 0x05: cs[i].town_effect = TE_GOODS; break;
					case 0x09: cs[i].town_effect = TE_WATER; break;
					case 0x0B: cs[i].town_effect = TE_FOOD; break;
					default:
						grfmsg(1, "CargoChangeInfo: Unknown town growth substitute value %d, setting to none.", substitute_type);
					case 0xFF: cs[i].town_effect = TE_NONE; break;
				}
			}
			break;

		case 0x19: /* Town growth coefficient */
			FOR_EACH_OBJECT cs[i].multipliertowngrowth = grf_load_word(&buf);
			break;

		case 0x1A: /* Bitmask of callbacks to use */
			FOR_EACH_OBJECT cs[i].callback_mask = grf_load_byte(&buf);
			break;

		default:
			ret = true;
	}

	*bufp = buf;
	return ret;
}


static bool SoundEffectChangeInfo(uint sid, int numinfo, int prop, byte **bufp, int len)
{
	byte *buf = *bufp;
	int i;
	bool ret = false;

	if (_cur_grffile->sound_offset == 0) {
		grfmsg(1, "SoundEffectChangeInfo: No effects defined, skipping");
		return false;
	}

	switch (prop) {
		case 0x08: // Relative volume
			FOR_EACH_OBJECT {
				uint sound = sid + i + _cur_grffile->sound_offset - GetNumOriginalSounds();

				if (sound >= GetNumSounds()) {
					grfmsg(1, "SoundEffectChangeInfo: Sound %d not defined (max %d)", sound, GetNumSounds());
				} else {
					GetSound(sound)->volume = grf_load_byte(&buf);
				}
			}
			break;

		case 0x09: // Priority
			FOR_EACH_OBJECT {
				uint sound = sid + i + _cur_grffile->sound_offset - GetNumOriginalSounds();

				if (sound >= GetNumSounds()) {
					grfmsg(1, "SoundEffectChangeInfo: Sound %d not defined (max %d)", sound, GetNumSounds());
				} else {
					GetSound(sound)->priority = grf_load_byte(&buf);
				}
			}
			break;

		case 0x0A: // Override old sound
			FOR_EACH_OBJECT {
				uint sound = sid + i + _cur_grffile->sound_offset - GetNumOriginalSounds();
				uint orig_sound = grf_load_byte(&buf);

				if (sound >= GetNumSounds() || orig_sound >= GetNumSounds()) {
					grfmsg(1, "SoundEffectChangeInfo: Sound %d or %d not defined (max %d)", sound, orig_sound, GetNumSounds());
				} else {
					FileEntry *newfe = GetSound(sound);
					FileEntry *oldfe = GetSound(orig_sound);

					/* Literally copy the data of the new sound over the original */
					*oldfe = *newfe;
				}
			}
			break;

		default:
			ret = true;
	}

	*bufp = buf;
	return ret;
}

/* Action 0x00 */
static void FeatureChangeInfo(byte *buf, int len)
{
	byte *bufend = buf + len;
	uint i;

	/* <00> <feature> <num-props> <num-info> <id> (<property <new-info>)...
	 *
	 * B feature       0, 1, 2 or 3 for trains, road vehicles, ships or planes
	 *                 4 for defining new train station sets
	 * B num-props     how many properties to change per vehicle/station
	 * B num-info      how many vehicles/stations to change
	 * B id            ID of first vehicle/station to change, if num-info is
	 *                 greater than one, this one and the following
	 *                 vehicles/stations will be changed
	 * B property      what property to change, depends on the feature
	 * V new-info      new bytes of info (variable size; depends on properties) */
	/* TODO: Bridges, town houses. */

	static const VCI_Handler handler[] = {
		/* GSF_TRAIN */        RailVehicleChangeInfo,
		/* GSF_ROAD */         RoadVehicleChangeInfo,
		/* GSF_SHIP */         ShipVehicleChangeInfo,
		/* GSF_AIRCRAFT */     AircraftVehicleChangeInfo,
		/* GSF_STATION */      StationChangeInfo,
		/* GSF_CANAL */        NULL,
		/* GSF_BRIDGE */       BridgeChangeInfo,
		/* GSF_TOWNHOUSE */    TownHouseChangeInfo,
		/* GSF_GLOBALVAR */    GlobalVarChangeInfo,
		/* GSF_INDUSTRYTILES */NULL,
		/* GSF_INDUSTRIES */   NULL,
		/* GSF_CARGOS */       NULL, /* Cargo is handled during reservation */
		/* GSF_SOUNDFX */      SoundEffectChangeInfo,
	};

	EngineInfo *ei = NULL;

	if (len == 1) {
		grfmsg(8, "Silently ignoring one-byte special sprite 0x00");
		return;
	}

	if (!check_length(len, 6, "FeatureChangeInfo")) return;
	buf++;
	uint8 feature  = grf_load_byte(&buf);
	uint8 numprops = grf_load_byte(&buf);
	uint numinfo  = grf_load_byte(&buf);
	uint engine   = grf_load_byte(&buf);

	grfmsg(6, "FeatureChangeInfo: feature %d, %d properties, to apply to %d+%d",
	               feature, numprops, engine, numinfo);

	if (feature >= lengthof(handler) || handler[feature] == NULL) {
		grfmsg(1, "FeatureChangeInfo: Unsupported feature %d, skipping", feature);
		return;
	}

	if (feature <= GSF_AIRCRAFT) {
		if (engine + numinfo > _vehcounts[feature]) {
			grfmsg(0, "FeatureChangeInfo: Last engine ID %d out of bounds (max %d), skipping", engine + numinfo, _vehcounts[feature]);
			return;
		}
		ei = &_engine_info[engine + _vehshifts[feature]];
	}

	while (numprops-- && buf < bufend) {
		uint8 prop = grf_load_byte(&buf);
		bool ignoring = false;

		switch (feature) {
			case GSF_TRAIN:
			case GSF_ROAD:
			case GSF_SHIP:
			case GSF_AIRCRAFT:
				/* Common properties for vehicles */
				switch (prop) {
					case 0x00: // Introduction date
						FOR_EACH_OBJECT ei[i].base_intro = grf_load_word(&buf) + DAYS_TILL_ORIGINAL_BASE_YEAR;
						break;

					case 0x02: // Decay speed
						FOR_EACH_OBJECT SB(ei[i].unk2, 0, 7, grf_load_byte(&buf) & 0x7F);
						break;

					case 0x03: // Vehicle life
						FOR_EACH_OBJECT ei[i].lifelength = grf_load_byte(&buf);
						break;

					case 0x04: // Model life
						FOR_EACH_OBJECT ei[i].base_life = grf_load_byte(&buf);
						break;

					case 0x06: // Climates available
						FOR_EACH_OBJECT ei[i].climates = grf_load_byte(&buf);
						break;

					case 0x07: // Loading speed
						/* Hyronymus explained me what does
						 * this mean and insists on having a
						 * credit ;-). --pasky */
						FOR_EACH_OBJECT ei[i].load_amount = grf_load_byte(&buf);
						break;

					default:
						if (handler[feature](engine, numinfo, prop, &buf, bufend - buf)) {
							ignoring = true;
						}
						break;
				}
				break;

			default:
				if (handler[feature](engine, numinfo, prop, &buf, bufend - buf)) {
					ignoring = true;
				}
				break;
		}

		if (ignoring) grfmsg(2, "FeatureChangeInfo: Ignoring property 0x%02X (not implemented)", prop);
	}
}

/* Action 0x00 (GLS_SAFETYSCAN) */
static void SafeChangeInfo(byte *buf, int len)
{
	if (len == 1) {
		grfmsg(8, "Silently ignoring one-byte special sprite 0x00");
		return;
	}

	if (!check_length(len, 6, "SafeChangeInfo")) return;
	buf++;
	uint8 feature  = grf_load_byte(&buf);
	uint8 numprops = grf_load_byte(&buf);
	grf_load_byte(&buf);
	grf_load_byte(&buf);

	if (feature == GSF_BRIDGE && numprops == 1) {
		uint8 prop = grf_load_byte(&buf);
		/* Bridge property 0x0D is redefinition of sprite layout tables, which
		 * is considered safe. */
		if (prop == 0x0D) return;
	}

	SETBIT(_cur_grfconfig->flags, GCF_UNSAFE);

	/* Skip remainder of GRF */
	_skip_sprites = -1;
}

/* Action 0x00 (GLS_INIT) */
static void InitChangeInfo(byte *buf, int len)
{
	byte *bufend = buf + len;

	if (len == 1) {
		grfmsg(8, "Silently ignoring one-byte special sprite 0x00");
		return;
	}

	if (!check_length(len, 6, "InitChangeInfo")) return;
	buf++;
	uint8 feature  = grf_load_byte(&buf);
	uint8 numprops = grf_load_byte(&buf);
	uint8 numinfo  = grf_load_byte(&buf);
	uint8 index    = grf_load_byte(&buf);

	while (numprops-- && buf < bufend) {
		uint8 prop = grf_load_byte(&buf);

		switch (feature) {
			case GSF_GLOBALVAR:
				switch (prop) {
					case 0x09: // Cargo Translation Table
						if (index != 0) {
							grfmsg(1, "InitChangeInfo: Cargo translation table must start at zero");
							return;
						}

						free(_cur_grffile->cargo_list);
						_cur_grffile->cargo_max = numinfo;
						_cur_grffile->cargo_list = MallocT<CargoLabel>(numinfo);

						int i;
						FOR_EACH_OBJECT {
							CargoLabel cl = grf_load_dword(&buf);
							_cur_grffile->cargo_list[i] = BSWAP32(cl);
						}
						break;
				}
				break;
		}
	}
}

/* Action 0x00 (GLS_RESERVE) */
static void ReserveChangeInfo(byte *buf, int len)
{
	byte *bufend = buf + len;

	if (len == 1) {
		grfmsg(8, "Silently ignoring one-byte special sprite 0x00");
		return;
	}

	if (!check_length(len, 6, "InitChangeInfo")) return;
	buf++;
	uint8 feature  = grf_load_byte(&buf);

	if (feature != GSF_CARGOS) return;

	uint8 numprops = grf_load_byte(&buf);
	uint8 numinfo  = grf_load_byte(&buf);
	uint8 index    = grf_load_byte(&buf);

	while (numprops-- && buf < bufend) {
		uint8 prop = grf_load_byte(&buf);

		if (CargoChangeInfo(index, numinfo, prop, &buf, bufend - buf)) {
			grfmsg(2, "ReserveChangeInfo: Ignoring property 0x%02X (not implemented)", prop);
		}
	}
}

#undef FOR_EACH_OBJECT

/**
 * Creates a spritegroup representing a callback result
 * @param value The value that was used to represent this callback result
 * @return A spritegroup representing that callback result
 */
static const SpriteGroup* NewCallBackResultSpriteGroup(uint16 value)
{
	SpriteGroup *group = AllocateSpriteGroup();

	group->type = SGT_CALLBACK;

	/* Old style callback results have the highest byte 0xFF so signify it is a callback result
	 * New style ones only have the highest bit set (allows 15-bit results, instead of just 8) */
	if ((value >> 8) == 0xFF) {
		value &= ~0xFF00;
	} else {
		value &= ~0x8000;
	}

	group->g.callback.result = value;

	return group;
}

/**
 * Creates a spritegroup representing a sprite number result.
 * @param sprite The sprite number.
 * @param num_sprites The number of sprites per set.
 * @return A spritegroup representing the sprite number result.
 */
static const SpriteGroup* NewResultSpriteGroup(SpriteID sprite, byte num_sprites)
{
	SpriteGroup *group = AllocateSpriteGroup();
	group->type = SGT_RESULT;
	group->g.result.sprite = sprite;
	group->g.result.num_sprites = num_sprites;
	return group;
}

/* Action 0x01 */
static void NewSpriteSet(byte *buf, int len)
{
	/* <01> <feature> <num-sets> <num-ent>
	 *
	 * B feature       feature to define sprites for
	 *                 0, 1, 2, 3: veh-type, 4: train stations
	 * B num-sets      number of sprite sets
	 * E num-ent       how many entries per sprite set
	 *                 For vehicles, this is the number of different
	 *                         vehicle directions in each sprite set
	 *                         Set num-dirs=8, unless your sprites are symmetric.
	 *                         In that case, use num-dirs=4.
	 */

	if (!check_length(len, 4, "NewSpriteSet")) return;
	buf++;
	uint8 feature   = grf_load_byte(&buf);
	uint8 num_sets  = grf_load_byte(&buf);
	uint16 num_ents = grf_load_extended(&buf);

	_cur_grffile->spriteset_start = _cur_spriteid;
	_cur_grffile->spriteset_feature = feature;
	_cur_grffile->spriteset_numsets = num_sets;
	_cur_grffile->spriteset_numents = num_ents;

	grfmsg(7, "New sprite set at %d of type %d, consisting of %d sets with %d views each (total %d)",
		_cur_spriteid, feature, num_sets, num_ents, num_sets * num_ents
	);

	for (uint16 i = 0; i < num_sets * num_ents; i++) {
		LoadNextSprite(_cur_spriteid++, _file_index);
		_nfo_line++;
	}
}

/* Helper function to either create a callback or link to a previously
 * defined spritegroup. */
static const SpriteGroup* GetGroupFromGroupID(byte setid, byte type, uint16 groupid)
{
	if (HASBIT(groupid, 15)) return NewCallBackResultSpriteGroup(groupid);

	if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
		grfmsg(1, "GetGroupFromGroupID(0x%02X:0x%02X): Groupid 0x%04X does not exist, leaving empty", setid, type, groupid);
		return NULL;
	}

	return _cur_grffile->spritegroups[groupid];
}

/* Helper function to either create a callback or a result sprite group. */
static const SpriteGroup* CreateGroupFromGroupID(byte feature, byte setid, byte type, uint16 spriteid, uint16 num_sprites)
{
	if (HASBIT(spriteid, 15)) return NewCallBackResultSpriteGroup(spriteid);

	if (spriteid >= _cur_grffile->spriteset_numsets) {
		grfmsg(1, "CreateGroupFromGroupID(0x%02X:0x%02X): Sprite set %u invalid, max %u", setid, type, spriteid, _cur_grffile->spriteset_numsets);
		return NULL;
	}

	/* Check if the sprite is within range. This can fail if the Action 0x01
	 * is skipped, as TTDPatch mandates that Action 0x02s must be processed.
	 * We don't have that rule, but must live by the Patch... */
	if (_cur_grffile->spriteset_start + spriteid * num_sprites + num_sprites > _cur_spriteid) {
		grfmsg(1, "CreateGroupFromGroupID(0x%02X:0x%02X): Real Sprite IDs 0x%04X - 0x%04X do not (all) exist (max 0x%04X), leaving empty",
				setid, type,
				_cur_grffile->spriteset_start + spriteid * num_sprites,
				_cur_grffile->spriteset_start + spriteid * num_sprites + num_sprites - 1, _cur_spriteid - 1);
		return NULL;
	}

	if (feature != _cur_grffile->spriteset_feature) {
		grfmsg(1, "CreateGroupFromGroupID(0x%02X:0x%02X): Sprite set feature 0x%02X does not match action feature 0x%02X, skipping",
				_cur_grffile->spriteset_feature, feature);
		return NULL;
	}

	return NewResultSpriteGroup(_cur_grffile->spriteset_start + spriteid * num_sprites, num_sprites);
}

/* Action 0x02 */
static void NewSpriteGroup(byte *buf, int len)
{
	/* <02> <feature> <set-id> <type/num-entries> <feature-specific-data...>
	 *
	 * B feature       see action 1
	 * B set-id        ID of this particular definition
	 * B type/num-entries
	 *                 if 80 or greater, this is a randomized or variational
	 *                 list definition, see below
	 *                 otherwise it specifies a number of entries, the exact
	 *                 meaning depends on the feature
	 * V feature-specific-data (huge mess, don't even look it up --pasky) */
	SpriteGroup *group = NULL;
	byte *bufend = buf + len;

	if (!check_length(len, 5, "NewSpriteGroup")) return;
	buf++;

	uint8 feature = grf_load_byte(&buf);
	uint8 setid   = grf_load_byte(&buf);
	uint8 type    = grf_load_byte(&buf);

	if (setid >= _cur_grffile->spritegroups_count) {
		/* Allocate memory for new sprite group references. */
		_cur_grffile->spritegroups = ReallocT(_cur_grffile->spritegroups, setid + 1);
		/* Initialise new space to NULL */
		for (; _cur_grffile->spritegroups_count < (setid + 1); _cur_grffile->spritegroups_count++)
			_cur_grffile->spritegroups[_cur_grffile->spritegroups_count] = NULL;
	}

	switch (type) {
		/* Deterministic Sprite Group */
		case 0x81: // Self scope, byte
		case 0x82: // Parent scope, byte
		case 0x85: // Self scope, word
		case 0x86: // Parent scope, word
		case 0x89: // Self scope, dword
		case 0x8A: // Parent scope, dword
		{
			byte varadjust;
			byte varsize;

			/* Check we can load the var size parameter */
			if (!check_length(bufend - buf, 1, "NewSpriteGroup (Deterministic) (1)")) return;

			group = AllocateSpriteGroup();
			group->type = SGT_DETERMINISTIC;
			group->g.determ.var_scope = HASBIT(type, 1) ? VSG_SCOPE_PARENT : VSG_SCOPE_SELF;

			switch (GB(type, 2, 2)) {
				default: NOT_REACHED();
				case 0: group->g.determ.size = DSG_SIZE_BYTE;  varsize = 1; break;
				case 1: group->g.determ.size = DSG_SIZE_WORD;  varsize = 2; break;
				case 2: group->g.determ.size = DSG_SIZE_DWORD; varsize = 4; break;
			}

			if (!check_length(bufend - buf, 5 + varsize, "NewSpriteGroup (Deterministic) (2)")) return;

			/* Loop through the var adjusts. Unfortunately we don't know how many we have
			 * from the outset, so we shall have to keep reallocing. */
			do {
				DeterministicSpriteGroupAdjust *adjust;

				if (group->g.determ.num_adjusts > 0) {
					if (!check_length(bufend - buf, 2 + varsize + 3, "NewSpriteGroup (Deterministic) (3)")) return;
				}

				group->g.determ.num_adjusts++;
				group->g.determ.adjusts = ReallocT(group->g.determ.adjusts, group->g.determ.num_adjusts);

				adjust = &group->g.determ.adjusts[group->g.determ.num_adjusts - 1];

				/* The first var adjust doesn't have an operation specified, so we set it to add. */
				adjust->operation = group->g.determ.num_adjusts == 1 ? DSGA_OP_ADD : (DeterministicSpriteGroupAdjustOperation)grf_load_byte(&buf);
				adjust->variable  = grf_load_byte(&buf);
				if (adjust->variable == 0x7E) {
					/* Link subroutine group */
					adjust->subroutine = GetGroupFromGroupID(setid, type, grf_load_byte(&buf));
				} else {
					adjust->parameter = IS_BYTE_INSIDE(adjust->variable, 0x60, 0x80) ? grf_load_byte(&buf) : 0;
				}

				varadjust = grf_load_byte(&buf);
				adjust->shift_num = GB(varadjust, 0, 5);
				adjust->type      = (DeterministicSpriteGroupAdjustType)GB(varadjust, 6, 2);
				adjust->and_mask  = grf_load_var(varsize, &buf);

				if (adjust->type != DSGA_TYPE_NONE) {
					adjust->add_val    = grf_load_var(varsize, &buf);
					adjust->divmod_val = grf_load_var(varsize, &buf);
				} else {
					adjust->add_val    = 0;
					adjust->divmod_val = 0;
				}

				/* Continue reading var adjusts while bit 5 is set. */
			} while (HASBIT(varadjust, 5));

			group->g.determ.num_ranges = grf_load_byte(&buf);
			group->g.determ.ranges = CallocT<DeterministicSpriteGroupRange>(group->g.determ.num_ranges);

			if (!check_length(bufend - buf, 2 + (2 + 2 * varsize) * group->g.determ.num_ranges, "NewSpriteGroup (Deterministic)")) return;

			for (uint i = 0; i < group->g.determ.num_ranges; i++) {
				group->g.determ.ranges[i].group = GetGroupFromGroupID(setid, type, grf_load_word(&buf));
				group->g.determ.ranges[i].low   = grf_load_var(varsize, &buf);
				group->g.determ.ranges[i].high  = grf_load_var(varsize, &buf);
			}

			group->g.determ.default_group = GetGroupFromGroupID(setid, type, grf_load_word(&buf));
			break;
		}

		/* Randomized Sprite Group */
		case 0x80: // Self scope
		case 0x83: // Parent scope
		{
			if (!check_length(bufend - buf, 7, "NewSpriteGroup (Randomized) (1)")) return;

			group = AllocateSpriteGroup();
			group->type = SGT_RANDOMIZED;
			group->g.random.var_scope = HASBIT(type, 1) ? VSG_SCOPE_PARENT : VSG_SCOPE_SELF;

			uint8 triggers = grf_load_byte(&buf);
			group->g.random.triggers       = GB(triggers, 0, 7);
			group->g.random.cmp_mode       = HASBIT(triggers, 7) ? RSG_CMP_ALL : RSG_CMP_ANY;
			group->g.random.lowest_randbit = grf_load_byte(&buf);
			group->g.random.num_groups     = grf_load_byte(&buf);
			group->g.random.groups = CallocT<const SpriteGroup*>(group->g.random.num_groups);

			if (!check_length(bufend - buf, 2 * group->g.random.num_groups, "NewSpriteGroup (Randomized) (2)")) return;

			for (uint i = 0; i < group->g.random.num_groups; i++) {
				group->g.random.groups[i] = GetGroupFromGroupID(setid, type, grf_load_word(&buf));
			}

			break;
		}

		/* Neither a variable or randomized sprite group... must be a real group */
		default:
		{


			switch (feature) {
				case GSF_TRAIN:
				case GSF_ROAD:
				case GSF_SHIP:
				case GSF_AIRCRAFT:
				case GSF_STATION:
				case GSF_CARGOS:
				{
					byte sprites     = _cur_grffile->spriteset_numents;
					byte num_loaded  = type;
					byte num_loading = grf_load_byte(&buf);

					if (_cur_grffile->spriteset_start == 0) {
						grfmsg(0, "NewSpriteGroup: No sprite set to work on! Skipping");
						return;
					}

					if (!check_length(bufend - buf, 2 * num_loaded + 2 * num_loading, "NewSpriteGroup (Real) (1)")) return;

					group = AllocateSpriteGroup();
					group->type = SGT_REAL;

					group->g.real.num_loaded  = num_loaded;
					group->g.real.num_loading = num_loading;
					if (num_loaded  > 0) group->g.real.loaded = CallocT<const SpriteGroup*>(num_loaded);
					if (num_loading > 0) group->g.real.loading = CallocT<const SpriteGroup*>(num_loading);

					grfmsg(6, "NewSpriteGroup: New SpriteGroup 0x%02X, %u views, %u loaded, %u loading",
							setid, sprites, num_loaded, num_loading);

					for (uint i = 0; i < num_loaded; i++) {
						uint16 spriteid = grf_load_word(&buf);
						group->g.real.loaded[i] = CreateGroupFromGroupID(feature, setid, type, spriteid, sprites);
						grfmsg(8, "NewSpriteGroup: + rg->loaded[%i]  = subset %u", i, spriteid);
					}

					for (uint i = 0; i < num_loading; i++) {
						uint16 spriteid = grf_load_word(&buf);
						group->g.real.loading[i] = CreateGroupFromGroupID(feature, setid, type, spriteid, sprites);
						grfmsg(8, "NewSpriteGroup: + rg->loading[%i] = subset %u", i, spriteid);
					}

					break;
				}

				case GSF_TOWNHOUSE: {
					byte sprites     = _cur_grffile->spriteset_numents;
					byte num_sprites = max((uint8)1, type);
					uint i;

					group = AllocateSpriteGroup();
					group->type = SGT_TILELAYOUT;
					group->g.layout.num_sprites = sprites;
					group->g.layout.dts = CallocT<DrawTileSprites>(1);

					/* Groundsprite */
					group->g.layout.dts->ground_sprite = grf_load_word(&buf);
					group->g.layout.dts->ground_pal    = grf_load_word(&buf);
					/* Remap transparent/colour modifier bits */
					if (HASBIT(group->g.layout.dts->ground_sprite, 14)) {
						CLRBIT(group->g.layout.dts->ground_sprite, 14);
						SETBIT(group->g.layout.dts->ground_sprite, PALETTE_MODIFIER_TRANSPARENT);
					}
					if (HASBIT(group->g.layout.dts->ground_sprite, 15)) {
						CLRBIT(group->g.layout.dts->ground_sprite, 15);
						SETBIT(group->g.layout.dts->ground_sprite, PALETTE_MODIFIER_COLOR);
					}
					if (HASBIT(group->g.layout.dts->ground_pal, 14)) {
						CLRBIT(group->g.layout.dts->ground_pal, 14);
						SETBIT(group->g.layout.dts->ground_sprite, SPRITE_MODIFIER_OPAQUE);
					}
					if (HASBIT(group->g.layout.dts->ground_pal, 15)) {
						/* Bit 31 set means this is a custom sprite, so rewrite it to the
						 * last spriteset defined. */
						SpriteID sprite = _cur_grffile->spriteset_start + GB(group->g.layout.dts->ground_sprite, 0, 14) * sprites;
						SB(group->g.layout.dts->ground_sprite, 0, SPRITE_WIDTH, sprite);
						CLRBIT(group->g.layout.dts->ground_pal, 15);
					}

					group->g.layout.dts->seq = CallocT<DrawTileSeqStruct>(num_sprites + 1);

					for (i = 0; i < num_sprites; i++) {
						DrawTileSeqStruct *seq = (DrawTileSeqStruct*)&group->g.layout.dts->seq[i];

						seq->image = grf_load_word(&buf);
						seq->pal   = grf_load_word(&buf);
						seq->delta_x = grf_load_byte(&buf);
						seq->delta_y = grf_load_byte(&buf);

						if (HASBIT(seq->image, 14)) {
							CLRBIT(seq->image, 14);
							SETBIT(seq->image, PALETTE_MODIFIER_TRANSPARENT);
						}
						if (HASBIT(seq->image, 15)) {
							CLRBIT(seq->image, 15);
							SETBIT(seq->image, PALETTE_MODIFIER_COLOR);
						}
						if (HASBIT(seq->pal, 14)) {
							CLRBIT(seq->pal, 14);
							SETBIT(seq->image, SPRITE_MODIFIER_OPAQUE);
						}
						if (HASBIT(seq->pal, 15)) {
							/* Bit 31 set means this is a custom sprite, so rewrite it to the
							 * last spriteset defined. */
							SpriteID sprite = _cur_grffile->spriteset_start + GB(seq->image, 0, 14) * sprites;
							SB(seq->image, 0, SPRITE_WIDTH, sprite);
							CLRBIT(seq->pal, 15);
						}

						if (type > 0) {
							seq->delta_z = grf_load_byte(&buf);
							if ((byte)seq->delta_z == 0x80) continue;
						}

						seq->size_x = grf_load_byte(&buf);
						seq->size_y = grf_load_byte(&buf);
						seq->size_z = grf_load_byte(&buf);
					}

					/* Set the terminator value. */
					((DrawTileSeqStruct*)group->g.layout.dts->seq)[i].delta_x = (byte)0x80;

					break;
				}

				/* Loading of Tile Layout and Production Callback groups would happen here */
				default: grfmsg(1, "NewSpriteGroup: Unsupported feature %d, skipping", feature);
			}
		}
	}

	_cur_grffile->spritegroups[setid] = group;
}

static CargoID TranslateCargo(uint8 feature, uint8 ctype)
{
	/* Special cargo types for purchase list and stations */
	if (feature == GSF_STATION && ctype == 0xFE) return CT_DEFAULT_NA;
	if (ctype == 0xFF) return CT_PURCHASE;

	if (_cur_grffile->cargo_max == 0) {
		/* No cargo table, so use bitnum values */
		if (ctype >= 32) {
			grfmsg(1, "TranslateCargo: Cargo bitnum %d out of range (max 31), skipping.", ctype);
			return CT_INVALID;
		}

		for (CargoID c = 0; c < NUM_CARGO; c++) {
			const CargoSpec *cs = GetCargo(c);
			if (!cs->IsValid()) continue;

			if (cs->bitnum == ctype) {
				grfmsg(6, "TranslateCargo: Cargo bitnum %d mapped to cargo type %d.", ctype, c);
				return c;
			}
		}

		grfmsg(5, "TranslateCargo: Cargo bitnum %d not available in this climate, skipping.", ctype);
		return CT_INVALID;
	}

	/* Check if the cargo type is out of bounds of the cargo translation table */
	if (ctype >= _cur_grffile->cargo_max) {
		grfmsg(1, "TranslateCargo: Cargo type %d out of range (max %d), skipping.", ctype, _cur_grffile->cargo_max - 1);
		return CT_INVALID;
	}

	/* Look up the cargo label from the translation table */
	CargoLabel cl = _cur_grffile->cargo_list[ctype];
	if (cl == 0) {
		grfmsg(5, "TranslateCargo: Cargo type %d not available in this climate, skipping.", ctype);
		return CT_INVALID;
	}

	ctype = GetCargoIDByLabel(cl);
	if (ctype == CT_INVALID) {
		grfmsg(5, "TranslateCargo: Cargo '%c%c%c%c' unsupported, skipping.", GB(cl, 24, 8), GB(cl, 16, 8), GB(cl, 8, 8), GB(cl, 0, 8));
		return CT_INVALID;
	}

	grfmsg(6, "TranslateCargo: Cargo '%c%c%c%c' mapped to cargo type %d.", GB(cl, 24, 8), GB(cl, 16, 8), GB(cl, 8, 8), GB(cl, 0, 8), ctype);
	return ctype;
}


static void VehicleMapSpriteGroup(byte *buf, byte feature, uint8 idcount, uint8 cidcount, bool wagover)
{
	static byte *last_engines;
	static int last_engines_count;

	if (!wagover) {
		if (last_engines_count != idcount) {
			last_engines = ReallocT(last_engines, idcount);
			last_engines_count = idcount;
		}
	} else {
		if (last_engines_count == 0) {
			grfmsg(0, "VehicleMapSpriteGroup: WagonOverride: No engine to do override with");
			return;
		}

		grfmsg(6, "VehicleMapSpriteGroup: WagonOverride: %u engines, %u wagons",
				last_engines_count, idcount);
	}

	for (uint i = 0; i < idcount; i++) {
		uint8 engine_id = buf[3 + i];
		uint8 engine = engine_id + _vehshifts[feature];
		byte *bp = &buf[4 + idcount];

		if (engine_id > _vehcounts[feature]) {
			grfmsg(0, "Id %u for feature 0x%02X is out of bounds", engine_id, feature);
			return;
		}

		grfmsg(7, "VehicleMapSpriteGroup: [%d] Engine %d...", i, engine);

		for (uint c = 0; c < cidcount; c++) {
			uint8 ctype = grf_load_byte(&bp);
			uint16 groupid = grf_load_word(&bp);

			grfmsg(8, "VehicleMapSpriteGroup: * [%d] Cargo type 0x%X, group id 0x%02X", c, ctype, groupid);

			if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
				grfmsg(1, "VehicleMapSpriteGroup: Spriteset 0x%04X out of range 0x%X or empty, skipping", groupid, _cur_grffile->spritegroups_count);
				continue;
			}

			ctype = TranslateCargo(feature, ctype);
			if (ctype == CT_INVALID) continue;

			if (wagover) {
				SetWagonOverrideSprites(engine, ctype, _cur_grffile->spritegroups[groupid], last_engines, last_engines_count);
			} else {
				SetCustomEngineSprites(engine, ctype, _cur_grffile->spritegroups[groupid]);
				last_engines[i] = engine;
			}
		}
	}

	{
		byte *bp = &buf[4 + idcount + cidcount * 3];
		uint16 groupid = grf_load_word(&bp);

		grfmsg(8, "-- Default group id 0x%04X", groupid);

		for (uint i = 0; i < idcount; i++) {
			uint8 engine = buf[3 + i] + _vehshifts[feature];

			/* Don't tell me you don't love duplicated code! */
			if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
				grfmsg(1, "FeatureMapSpriteGroup: Spriteset 0x%04X out of range 0x%X or empty, skipping",
				       groupid, _cur_grffile->spritegroups_count);
				continue;
			}

			if (wagover) {
				/* If the ID for this action 3 is the same as the vehicle ID,
 * this indicates we have a helicopter rotor override. */
				if (feature == GSF_AIRCRAFT && engine == last_engines[i]) {
					SetRotorOverrideSprites(engine, _cur_grffile->spritegroups[groupid]);
				} else {
					/* TODO: No multiple cargo types per vehicle yet. --pasky */
					SetWagonOverrideSprites(engine, CT_DEFAULT, _cur_grffile->spritegroups[groupid], last_engines, last_engines_count);
				}
			} else {
				SetCustomEngineSprites(engine, CT_DEFAULT, _cur_grffile->spritegroups[groupid]);
				SetEngineGRF(engine, _cur_grffile);
				last_engines[i] = engine;
			}
		}
	}
}


static void StationMapSpriteGroup(byte *buf, uint8 idcount, uint8 cidcount)
{
	for (uint i = 0; i < idcount; i++) {
		uint8 stid = buf[3 + i];
		StationSpec *statspec = _cur_grffile->stations[stid];
		byte *bp = &buf[4 + idcount];

		for (uint c = 0; c < cidcount; c++) {
			uint8 ctype = grf_load_byte(&bp);
			uint16 groupid = grf_load_word(&bp);

			if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
				grfmsg(1, "StationMapSpriteGroup: Spriteset 0x%04X out of range 0x%X or empty, skipping",
				       groupid, _cur_grffile->spritegroups_count);
				continue;
			}

			ctype = TranslateCargo(GSF_STATION, ctype);
			if (ctype == CT_INVALID) continue;

			statspec->spritegroup[ctype] = _cur_grffile->spritegroups[groupid];
		}
	}

	{
		byte *bp = &buf[4 + idcount + cidcount * 3];
		uint16 groupid = grf_load_word(&bp);

		if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
			grfmsg(1, "StationMapSpriteGroup: Spriteset 0x%04X out of range 0x%X or empty, skipping",
			       groupid, _cur_grffile->spritegroups_count);
			return;
		}

		for (uint i = 0; i < idcount; i++) {
			uint8 stid = buf[3 + i];
			StationSpec *statspec = _cur_grffile->stations[stid];

			statspec->spritegroup[CT_DEFAULT] = _cur_grffile->spritegroups[groupid];
			statspec->grffile = _cur_grffile;
			statspec->localidx = stid;
			SetCustomStationSpec(statspec);
		}
	}
}


static void TownHouseMapSpriteGroup(byte *buf, uint8 idcount, uint8 cidcount)
{
	byte *bp = &buf[4 + idcount + cidcount * 3];
	uint16 groupid = grf_load_word(&bp);

	if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
		grfmsg(1, "TownHouseMapSpriteGroup: Spriteset 0x%04X out of range 0x%X or empty, skipping.",
		       groupid, _cur_grffile->spritegroups_count);
		return;
	}

	for (uint i = 0; i < idcount; i++) {
		uint8 hid = buf[3 + i];
		HouseSpec *hs = _cur_grffile->housespec[hid];

		if (hs == NULL) {
			grfmsg(1, "TownHouseMapSpriteGroup: Too many houses defined, skipping");
			return;
		}

		hs->spritegroup = _cur_grffile->spritegroups[groupid];
	}
}


static void CargoMapSpriteGroup(byte *buf, uint8 idcount, uint8 cidcount)
{
	byte *bp = &buf[4 + idcount + cidcount * 3];
	uint16 groupid = grf_load_word(&bp);

	if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
		grfmsg(1, "CargoMapSpriteGroup: Spriteset 0x%04X out of range 0x%X or empty, skipping.",
		       groupid, _cur_grffile->spritegroups_count);
		return;
	}

	for (uint i = 0; i < idcount; i++) {
		CargoID cid = buf[3 + i];

		if (cid >= NUM_CARGO) {
			grfmsg(1, "CargoMapSpriteGroup: Cargo ID %d out of range, skipping");
			continue;
		}

		CargoSpec *cs = &_cargo[cid];
		cs->grfid = _cur_grffile->grfid;
		cs->group = _cur_grffile->spritegroups[groupid];
	}
}


/* Action 0x03 */
static void FeatureMapSpriteGroup(byte *buf, int len)
{
	/* <03> <feature> <n-id> <ids>... <num-cid> [<cargo-type> <cid>]... <def-cid>
	 * id-list    := [<id>] [id-list]
	 * cargo-list := <cargo-type> <cid> [cargo-list]
	 *
	 * B feature       see action 0
	 * B n-id          bits 0-6: how many IDs this definition applies to
	 *                 bit 7: if set, this is a wagon override definition (see below)
	 * B ids           the IDs for which this definition applies
	 * B num-cid       number of cargo IDs (sprite group IDs) in this definition
	 *                 can be zero, in that case the def-cid is used always
	 * B cargo-type    type of this cargo type (e.g. mail=2, wood=7, see below)
	 * W cid           cargo ID (sprite group ID) for this type of cargo
	 * W def-cid       default cargo ID (sprite group ID) */

	if (!check_length(len, 6, "FeatureMapSpriteGroup")) return;

	uint8 feature = buf[1];
	uint8 idcount = buf[2] & 0x7F;
	bool wagover = (buf[2] & 0x80) == 0x80;

	if (!check_length(len, 3 + idcount, "FeatureMapSpriteGroup")) return;

	/* If idcount is zero, this is a feature callback */
	if (idcount == 0) {
		grfmsg(2, "FeatureMapSpriteGroup: Feature callbacks not implemented yet");
		return;
	}

	uint8 cidcount = buf[3 + idcount];
	if (!check_length(len, 4 + idcount + cidcount * 3, "FeatureMapSpriteGroup")) return;

	grfmsg(6, "FeatureMapSpriteGroup: Feature %d, %d ids, %d cids, wagon override %d",
			feature, idcount, cidcount, wagover);

	if (_cur_grffile->spriteset_start == 0 || _cur_grffile->spritegroups == 0) {
		grfmsg(1, "FeatureMapSpriteGroup: No sprite set to work on! Skipping");
		return;
	}

	switch (feature) {
		case GSF_TRAIN:
		case GSF_ROAD:
		case GSF_SHIP:
		case GSF_AIRCRAFT:
			VehicleMapSpriteGroup(buf, feature, idcount, cidcount, wagover);
			return;

		case GSF_STATION:
			StationMapSpriteGroup(buf, idcount, cidcount);
			return;

		case GSF_TOWNHOUSE:
			TownHouseMapSpriteGroup(buf, idcount, cidcount);
			return;

		case GSF_CARGOS:
			CargoMapSpriteGroup(buf, idcount, cidcount);
			return;

		default:
			grfmsg(1, "FeatureMapSpriteGroup: Unsupported feature %d, skipping", feature);
			return;
	}
}

/* Action 0x04 */
static void FeatureNewName(byte *buf, int len)
{
	/* <04> <veh-type> <language-id> <num-veh> <offset> <data...>
	 *
	 * B veh-type      see action 0 (as 00..07, + 0A
	 *                 But IF veh-type = 48, then generic text
	 * B language-id   If bit 6 is set, This is the extended language scheme,
	                   with up to 64 language.
	                   Otherwise, it is a mapping where set bits have meaning
	                   0 = american, 1 = english, 2 = german, 3 = french, 4 = spanish
	                   Bit 7 set means this is a generic text, not a vehicle one (or else)
	 * B num-veh       number of vehicles which are getting a new name
	 * B/W offset      number of the first vehicle that gets a new name
	 *                 Byte : ID of vehicle to change
	 *                 Word : ID of string to change/add
	 * S data          new texts, each of them zero-terminated, after
	 *                 which the next name begins. */

	bool new_scheme = _cur_grffile->grf_version >= 7;

	if (!check_length(len, 6, "FeatureNewName")) return;
	buf++;
	uint8 feature  = grf_load_byte(&buf);
	uint8 lang     = grf_load_byte(&buf);
	uint8 num      = grf_load_byte(&buf);
	bool generic   = HASBIT(lang, 7);
	uint16 id      = generic ? grf_load_word(&buf) : grf_load_byte(&buf);

	CLRBIT(lang, 7);

	if (feature <= GSF_AIRCRAFT && id < _vehcounts[feature]) {
		id += _vehshifts[feature];
	}
	uint16 endid = id + num;

	grfmsg(6, "FeatureNewName: About to rename engines %d..%d (feature %d) in language 0x%02X",
	               id, endid, feature, lang);

	len -= generic ? 6 : 5;

	for (; id < endid && len > 0; id++) {
		const char *name   = grf_load_string(&buf, len);
		size_t name_length = strlen(name) + 1;

		len -= (int)name_length;

		if (name_length == 1) {
			grfmsg(7, "FeatureNewName: Can't add empty name");
		} else {
			grfmsg(8, "FeatureNewName: %d <- %s", id, name);

			switch (feature) {
				case GSF_TRAIN:
				case GSF_ROAD:
				case GSF_SHIP:
				case GSF_AIRCRAFT: {
					if (id < TOTAL_NUM_ENGINES) {
						StringID string = AddGRFString(_cur_grffile->grfid, id, lang, new_scheme, name, STR_8000_KIRBY_PAUL_TANK_STEAM + id);
						SetCustomEngineName(id, string);
					} else {
						AddGRFString(_cur_grffile->grfid, id, lang, new_scheme, name, id);
					}
					break;
				}

				case GSF_TOWNHOUSE:
				default:
					switch (GB(id, 8, 8)) {
						case 0xC4: // Station class name
							if (_cur_grffile->stations == NULL || _cur_grffile->stations[GB(id, 0, 8)] == NULL) {
								grfmsg(1, "FeatureNewName: Attempt to name undefined station 0x%X, ignoring", GB(id, 0, 8));
							} else {
								StationClassID sclass = _cur_grffile->stations[GB(id, 0, 8)]->sclass;
								SetStationClassName(sclass, AddGRFString(_cur_grffile->grfid, id, lang, new_scheme, name, STR_UNDEFINED));
							}
							break;

						case 0xC5: // Station name
							if (_cur_grffile->stations == NULL || _cur_grffile->stations[GB(id, 0, 8)] == NULL) {
								grfmsg(1, "FeatureNewName: Attempt to name undefined station 0x%X, ignoring", GB(id, 0, 8));
							} else {
								_cur_grffile->stations[GB(id, 0, 8)]->name = AddGRFString(_cur_grffile->grfid, id, lang, new_scheme, name, STR_UNDEFINED);
							}
							break;

						case 0xC9: { // House name
							if (_cur_grffile->housespec == NULL || _cur_grffile->housespec[GB(id, 0, 8)] == NULL) {
								grfmsg(1, "FeatureNewName: Attempt to name undefined house 0x%X, ignoring.", GB(id, 0, 8));
							} else {
								_cur_grffile->housespec[GB(id, 0, 8)]->building_name = AddGRFString(_cur_grffile->grfid, id, lang, new_scheme, name, STR_UNDEFINED);
							}
							break;
						}

						case 0xD0:
						case 0xDC:
							AddGRFString(_cur_grffile->grfid, id, lang, new_scheme, name, STR_UNDEFINED);
							break;

						default:
							grfmsg(7, "FeatureNewName: Unsupported ID (0x%04X)", id);
							break;
					}
					break;

#if 0
				case GSF_CANAL :
				case GSF_BRIDGE :
					AddGRFString(_cur_spriteid, id, lang, name);
					switch (GB(id, 8, 8)) {
						case 0xC9: // House name
						default:
							grfmsg(7, "FeatureNewName: Unsupported ID (0x%04X)", id);
					}
					break;

				case GSF_INDUSTRIES :
				case 0x48 :   // for generic strings
					AddGRFString(_cur_spriteid, id, lang, name);
					break;
				default :
					grfmsg(7, "FeatureNewName: Unsupported feature (0x%02X)", feature);
					break;
#endif
			}
		}
	}
}

/* Action 0x05 */
static void GraphicsNew(byte *buf, int len)
{
	/* <05> <graphics-type> <num-sprites> <other data...>
	 *
	 * B graphics-type What set of graphics the sprites define.
	 * E num-sprites   How many sprites are in this set?
	 * V other data    Graphics type specific data.  Currently unused. */
	/* TODO */

	SpriteID replace = 0;

	if (!check_length(len, 2, "GraphicsNew")) return;
	buf++;
	uint8 type = grf_load_byte(&buf);
	uint16 num = grf_load_extended(&buf);

	switch (type) {
		case 0x04: // Signal graphics
			if (num != 112 && num != 240) {
				grfmsg(1, "GraphicsNew: Signal graphics sprite count must be 112 or 240, skipping");
				return;
			}
			_signal_base = _cur_spriteid;
			break;

		case 0x05: // Catenary graphics
			if (num != 48) {
				grfmsg(1, "GraphicsNew: Catenary graphics sprite count must be 48, skipping");
				return;
			}
			replace = SPR_ELRAIL_BASE + 3;
			break;

		case 0x06: // Foundations
			if (num != 74) {
				grfmsg(1, "GraphicsNew: Foundation graphics sprite count must be 74, skipping");
				return;
			}
			replace = SPR_SLOPES_BASE;
			break;

		case 0x08: // Canal graphics
			if (num != 65) {
				grfmsg(1, "GraphicsNew: Canal graphics sprite count must be 65, skipping");
				return;
			}
			replace = SPR_CANALS_BASE + 5;
			break;

		case 0x0A: // 2CC colour maps
			if (num != 256) {
				grfmsg(1, "GraphicsNew: 2CC colour maps sprite count must be 256, skipping");
				return;
			}
			replace = SPR_2CCMAP_BASE;
			break;

		case 0x0D: // Coast graphics
			if (num != 16) {
				grfmsg(1, "GraphicsNew: Coast graphics sprite count must be 16, skipping");
				return;
			}
			_coast_base = _cur_spriteid;
			break;

		case 0x10: // New airport sprites
			if (num != 15) {
				grfmsg(1, "GraphicsNew: Airport graphics sprite count must be 15, skipping");
				return;
			}
			replace = SPR_AIRPORTX_BASE;
			break;

		case 0x11: // Road stop sprites
			if (num != 8) {
				grfmsg(1, "GraphicsNew: Road stop graphics sprite count must be 8, skipping");
				return;
			}
			replace = SPR_ROADSTOP_BASE;
			break;

		default:
			grfmsg(2, "GraphicsNew: Custom graphics (type 0x%02X) sprite block of length %u (unimplemented, ignoring)",
					type, num);
			return;
	}

	if (replace == 0) {
		grfmsg(2, "GraphicsNew: Loading %u sprites of type 0x%02X at SpriteID 0x%04X", num, type, _cur_spriteid);
	} else {
		grfmsg(2, "GraphicsNew: Replacing %u sprites of type 0x%02X at SpriteID 0x%04X", num, type, replace);
	}

	for (; num > 0; num--) {
		LoadNextSprite(replace == 0 ? _cur_spriteid++ : replace++, _file_index);
		_nfo_line++;
	}
}

/* Action 0x05 (SKIP) */
static void SkipAct5(byte *buf, int len)
{
	if (!check_length(len, 2, "SkipAct5")) return;
	buf++;

	/* Ignore type byte */
	grf_load_byte(&buf);

	/* Skip the sprites of this action */
	_skip_sprites = grf_load_extended(&buf);

	grfmsg(3, "SkipAct5: Skipping %d sprites", _skip_sprites);
}

static uint32 GetParamVal(byte param, uint32 *cond_val)
{
	switch (param) {
		case 0x81: // current year
			return clamp(_cur_year, ORIGINAL_BASE_YEAR, ORIGINAL_MAX_YEAR) - ORIGINAL_BASE_YEAR;

		case 0x83: // current climate, 0=temp, 1=arctic, 2=trop, 3=toyland
			return _opt.landscape;

		case 0x84: // GRF loading stage
			return (_cur_stage > GLS_INIT) | ((_cur_stage == GLS_ACTIVATION) << 9);

		case 0x85: // TTDPatch flags, only for bit tests
			if (cond_val == NULL) {
				/* Supported in Action 0x07 and 0x09, not 0x0D */
				return 0;
			} else {
				uint32 param_val = _ttdpatch_flags[*cond_val / 0x20];
				*cond_val %= 0x20;
				return param_val;
			}

		case 0x86: // road traffic side, bit 4 clear=left, set=right
			return _opt.road_side << 4;

		case 0x88: // GRF ID check
			return 0;

		case 0x8B: { // TTDPatch version
			uint major    = 2;
			uint minor    = 6;
			uint revision = 0; // special case: 2.0.1 is 2.0.10
			uint build    = 1168;
			return (major << 24) | (minor << 20) | (revision << 16) | build;
		}

		case 0x8D: // TTD Version, 00=DOS, 01=Windows
			return !_use_dos_palette;

		case 0x8E: // Y-offset for train sprites
			return _traininfo_vehicle_pitch;

		case 0x92: // Game mode
			return _game_mode;

		case 0x9A: // Always -1
			return UINT_MAX;

		case 0x9D: // TTD Platform, 00=TTDPatch, 01=OpenTTD
			return 1;

		case 0x9E: // Miscellaneous GRF features
			return _misc_grf_features;

		default:
			/* GRF Parameter */
			if (param < 0x80) return _cur_grffile->param[param];

			/* In-game variable. */
			grfmsg(1, "Unsupported in-game variable 0x%02X", param);
			return UINT_MAX;
	}
}

/* Action 0x06 */
static void CfgApply(byte *buf, int len)
{
	/* <06> <param-num> <param-size> <offset> ... <FF>
	 *
	 * B param-num     Number of parameter to substitute (First = "zero")
	 *                 Ignored if that parameter was not specified in newgrf.cfg
	 * B param-size    How many bytes to replace.  If larger than 4, the
	 *                 bytes of the following parameter are used.  In that
	 *                 case, nothing is applied unless *all* parameters
	 *                 were specified.
	 * B offset        Offset into data from beginning of next sprite
	 *                 to place where parameter is to be stored. */

	/* Preload the next sprite */
	uint32 pos = FioGetPos();
	uint16 num = FioReadWord();
	uint8 type = FioReadByte();

	/* Check if the sprite is a pseudo sprite. We can't operate on real sprites. */
	if (type == 0xFF) {
		_preload_sprite = MallocT<byte>(num);
		FioReadBlock(_preload_sprite, num);
	}

	/* Reset the file position to the start of the next sprite */
	FioSeekTo(pos, SEEK_SET);

	if (type != 0xFF) {
		grfmsg(2, "CfgApply: Ignoring (next sprite is real, unsupported)");
		return;
	}

	/* Now perform the Action 0x06 on our data. */
	buf++;

	for (;;) {
		uint i;
		uint param_num;
		uint param_size;
		uint offset;
		bool add_value;

		/* Read the parameter to apply. 0xFF indicates no more data to change. */
		param_num = grf_load_byte(&buf);
		if (param_num == 0xFF) break;

		/* Get the size of the parameter to use. If the size covers multiple
		 * double words, sequential parameter values are used. */
		param_size = grf_load_byte(&buf);

		/* Bit 7 of param_size indicates we should add to the original value
		 * instead of replacing it. */
		add_value  = HASBIT(param_size, 7);
		param_size = GB(param_size, 0, 7);

		/* Where to apply the data to within the pseudo sprite data. */
		offset     = grf_load_extended(&buf);

		/* If the parameter is a GRF parameter (not an internal variable) check
		 * if it (and all further sequential parameters) has been defined. */
		if (param_num < 0x80 && (param_num + (param_size - 1) / 4) >= _cur_grffile->param_end) {
			grfmsg(2, "CfgApply: Ignoring (param %d not set)", (param_num + (param_size - 1) / 4));
			break;
		}

		grfmsg(8, "CfgApply: Applying %u bytes from parameter 0x%02X at offset 0x%04X", param_size, param_num, offset);

		for (i = 0; i < param_size; i++) {
			uint32 value = GetParamVal(param_num + i / 4, NULL);

			if (add_value) {
				_preload_sprite[offset + i] += GB(value, (i % 4) * 8, 8);
			} else {
				_preload_sprite[offset + i] = GB(value, (i % 4) * 8, 8);
			}
		}
	}
}

/* Action 0x07 */
/* Action 0x09 */
static void SkipIf(byte *buf, int len)
{
	/* <07/09> <param-num> <param-size> <condition-type> <value> <num-sprites>
	 *
	 * B param-num
	 * B param-size
	 * B condition-type
	 * V value
	 * B num-sprites */
	/* TODO: More params. More condition types. */
	uint32 cond_val = 0;
	uint32 mask = 0;
	bool result;

	if (!check_length(len, 6, "SkipIf")) return;
	buf++;
	uint8 param     = grf_load_byte(&buf);
	uint8 paramsize = grf_load_byte(&buf);
	uint8 condtype  = grf_load_byte(&buf);

	if (condtype < 2) {
		/* Always 1 for bit tests, the given value should be ignored. */
		paramsize = 1;
	}

	switch (paramsize) {
		case 4: cond_val = grf_load_dword(&buf); mask = 0xFFFFFFFF; break;
		case 2: cond_val = grf_load_word(&buf);  mask = 0x0000FFFF; break;
		case 1: cond_val = grf_load_byte(&buf);  mask = 0x000000FF; break;
		default: break;
	}

	if (param < 0x80 && _cur_grffile->param_end <= param) {
		grfmsg(7, "SkipIf: Param %d undefined, skipping test", param);
		return;
	}

	uint32 param_val = GetParamVal(param, &cond_val);

	grfmsg(7, "SkipIf: Test condtype %d, param 0x%08X, condval 0x%08X", condtype, param_val, cond_val);

	if (param == 0x88) {
		/* GRF ID checks */

		const GRFConfig *c = GetGRFConfig(cond_val);

		if (condtype != 10 && c == NULL) {
			grfmsg(7, "SkipIf: GRFID 0x%08X unknown, skipping test", BSWAP32(cond_val));
			return;
		}

		switch (condtype) {
			/* Tests 6 to 10 are only for param 0x88, GRFID checks */
			case 6: // Is GRFID active?
				result = c->status == GCS_ACTIVATED;
				break;

			case 7: // Is GRFID non-active?
				result = c->status != GCS_ACTIVATED;
				break;

			case 8: // GRFID is not but will be active?
				result = c->status == GCS_INITIALISED;
				break;

			case 9: // GRFID is or will be active?
				result = c->status == GCS_ACTIVATED || c->status == GCS_INITIALISED;
				break;

			case 10: // GRFID is not nor will be active
				/* This is the only condtype that doesn't get ignored if the GRFID is not found */
				result = c == NULL || c->flags == GCS_DISABLED || c->status == GCS_NOT_FOUND;
				break;

			default: grfmsg(1, "SkipIf: Unsupported GRF test %d. Ignoring", condtype); return;
		}
	} else {
		/* Parameter or variable tests */
		switch (condtype) {
			case 0: result = !!(param_val & (1 << cond_val));
				break;
			case 1: result = !(param_val & (1 << cond_val));
				break;
			case 2: result = (param_val & mask) == cond_val;
				break;
			case 3: result = (param_val & mask) != cond_val;
				break;
			case 4: result = (param_val & mask) < cond_val;
				break;
			case 5: result = (param_val & mask) > cond_val;
				break;

			default: grfmsg(1, "SkipIf: Unsupported test %d. Ignoring", condtype); return;
		}
	}

	if (!result) {
		grfmsg(2, "SkipIf: Not skipping sprites, test was false");
		return;
	}

	uint8 numsprites = grf_load_byte(&buf);

	/* numsprites can be a GOTO label if it has been defined in the GRF
	 * file. The jump will always be the first matching label that follows
	 * the current nfo_line. If no matching label is found, the first matching
	 * label in the file is used. */
	GRFLabel *choice = NULL;
	for (GRFLabel *label = _cur_grffile->label; label != NULL; label = label->next) {
		if (label->label != numsprites) continue;

		/* Remember a goto before the current line */
		if (choice == NULL) choice = label;
		/* If we find a label here, this is definitely good */
		if (label->nfo_line > _nfo_line) {
			choice = label;
			break;
		}
	}

	if (choice != NULL) {
		grfmsg(2, "SkipIf: Jumping to label 0x%0X at line %d, test was true", choice->label, choice->nfo_line);
		FioSeekTo(choice->pos, SEEK_SET);
		_nfo_line = choice->nfo_line;
		return;
	}

	grfmsg(2, "SkipIf: Skipping %d sprites, test was true", numsprites);
	_skip_sprites = numsprites;
	if (_skip_sprites == 0) {
		/* Zero means there are no sprites to skip, so
		 * we use -1 to indicate that all further
		 * sprites should be skipped. */
		_skip_sprites = -1;

		/* If an action 8 hasn't been encountered yet, disable the grf. */
		if (_cur_stage != GLS_RESERVE && _cur_grfconfig->status != GCS_ACTIVATED) _cur_grfconfig->status = GCS_DISABLED;
	}
}


/* Action 0x08 (GLS_FILESCAN) */
static void ScanInfo(byte *buf, int len)
{
	if (!check_length(len, 8, "Info")) return;
	buf++;
	grf_load_byte(&buf);
	uint32 grfid  = grf_load_dword(&buf);

	_cur_grfconfig->grfid = grfid;

	/* GRF IDs starting with 0xFF are reserved for internal TTDPatch use */
	if (GB(grfid, 24, 8) == 0xFF) SETBIT(_cur_grfconfig->flags, GCF_SYSTEM);

	len -= 6;
	const char *name = grf_load_string(&buf, len);
	_cur_grfconfig->name = TranslateTTDPatchCodes(name);

	len -= strlen(name) + 1;
	if (len > 0) {
		const char *info = grf_load_string(&buf, len);
		_cur_grfconfig->info = TranslateTTDPatchCodes(info);
	}

	/* GLS_INFOSCAN only looks for the action 8, so we can skip the rest of the file */
	_skip_sprites = -1;
}

/* Action 0x08 */
static void GRFInfo(byte *buf, int len)
{
	/* <08> <version> <grf-id> <name> <info>
	 *
	 * B version       newgrf version, currently 06
	 * 4*B grf-id      globally unique ID of this .grf file
	 * S name          name of this .grf set
	 * S info          string describing the set, and e.g. author and copyright */

	if (!check_length(len, 8, "GRFInfo")) return;
	buf++;
	uint8 version    = grf_load_byte(&buf);
	uint32 grfid     = grf_load_dword(&buf);
	const char *name = grf_load_string(&buf, len - 6);

	_cur_grffile->grfid = grfid;
	_cur_grffile->grf_version = version;
	_cur_grfconfig->status = _cur_stage < GLS_ACTIVATION ? GCS_INITIALISED : GCS_ACTIVATED;

	/* Do swap the GRFID for displaying purposes since people expect that */
	DEBUG(grf, 1, "GRFInfo: Loaded GRFv%d set %08lX - %s", version, BSWAP32(grfid), name);
}

/* Action 0x0A */
static void SpriteReplace(byte *buf, int len)
{
	/* <0A> <num-sets> <set1> [<set2> ...]
	 * <set>: <num-sprites> <first-sprite>
	 *
	 * B num-sets      How many sets of sprites to replace.
	 * Each set:
	 * B num-sprites   How many sprites are in this set
	 * W first-sprite  First sprite number to replace */

	buf++; // skip action byte
	uint8 num_sets = grf_load_byte(&buf);

	for (uint i = 0; i < num_sets; i++) {
		uint8 num_sprites = grf_load_byte(&buf);
		uint16 first_sprite = grf_load_word(&buf);

		grfmsg(2, "SpriteReplace: [Set %d] Changing %d sprites, beginning with %d",
			i, num_sprites, first_sprite
		);

		for (uint j = 0; j < num_sprites; j++) {
			LoadNextSprite(first_sprite + j, _file_index); // XXX
			_nfo_line++;
		}
	}
}

/* Action 0x0A (SKIP) */
static void SkipActA(byte *buf, int len)
{
	buf++;
	uint8 num_sets = grf_load_byte(&buf);

	for (uint i = 0; i < num_sets; i++) {
		/* Skip the sprites this replaces */
		_skip_sprites += grf_load_byte(&buf);
		/* But ignore where they go */
		grf_load_word(&buf);
	}

	grfmsg(3, "SkipActA: Skipping %d sprites", _skip_sprites);
}

/* Action 0x0B */
static void GRFLoadError(byte *buf, int len)
{
	/* <0B> <severity> <language-id> <message-id> [<message...> 00] [<data...>] 00 [<parnum>]
	 *
	 * B severity      00: notice, contine loading grf file
	 *                 01: warning, continue loading grf file
	 *                 02: error, but continue loading grf file, and attempt
	 *                     loading grf again when loading or starting next game
	 *                 03: error, abort loading and prevent loading again in
	 *                     the future (only when restarting the patch)
	 * B language-id   see action 4, use 1F for built-in error messages
	 * B message-id    message to show, see below
	 * S message       for custom messages (message-id FF), text of the message
	 *                 not present for built-in messages.
	 * V data          additional data for built-in (or custom) messages
	 * B parnum        parameter numbers to be shown in the message (maximum of 2) */

	static const StringID msgstr[] = {
		STR_NEWGRF_ERROR_VERSION_NUMBER,
		STR_NEWGRF_ERROR_DOS_OR_WINDOWS,
		STR_NEWGRF_ERROR_UNSET_SWITCH,
		STR_NEWGRF_ERROR_INVALID_PARAMETER,
		STR_NEWGRF_ERROR_LOAD_BEFORE,
		STR_NEWGRF_ERROR_LOAD_AFTER
	};

	static const StringID sevstr[] = {
		STR_NEWGRF_ERROR_MSG_INFO,
		STR_NEWGRF_ERROR_MSG_WARNING,
		STR_NEWGRF_ERROR_MSG_ERROR,
		STR_NEWGRF_ERROR_MSG_FATAL
	};

	/* AddGRFString expects the string to be referred to by an id in the newgrf
	 * file. Errors messages are never referred to however, so invent ids that
	 * are unlikely to be reached in a newgrf file so they don't overwrite
	 * anything else. */
	enum {
		MESSAGE_STRING_ID = MAX_UVALUE(StringID) - 1,
		MESSAGE_DATA_ID   = MAX_UVALUE(StringID)
	};

	if (!check_length(len, 6, "GRFLoadError")) return;

	/* For now we can only show one message per newgrf file. */
	if (_cur_grfconfig->error != NULL) return;

	buf++; // Skip the action byte.
	byte severity   = grf_load_byte(&buf);
	byte lang       = grf_load_byte(&buf);
	byte message_id = grf_load_byte(&buf);
	len -= 4;

	/* Skip the error until the activation stage unless bit 7 of the severity
	 * is set. */
	if (!HASBIT(severity, 7) && _cur_stage < GLS_ACTIVATION) {
		grfmsg(7, "GRFLoadError: Skipping non-fatal GRFLoadError in stage 1");
		return;
	}
	CLRBIT(severity, 7);

	if (severity >= lengthof(sevstr)) {
		grfmsg(7, "GRFLoadError: Invalid severity id %d. Setting to 2 (non-fatal error).", severity);
		severity = 2;
	} else if (severity == 3) {
		/* This is a fatal error, so make sure the GRF is deactivated and no
		 * more of it gets loaded. */
		_cur_grfconfig->status = GCS_DISABLED;

		_skip_sprites = -1;
	}

	if (message_id >= lengthof(msgstr) && message_id != 0xFF) {
		grfmsg(7, "GRFLoadError: Invalid message id.");
		return;
	}

	if (len <= 1) {
		grfmsg(7, "GRFLoadError: No message data supplied.");
		return;
	}

	bool new_scheme = _cur_grffile->grf_version >= 7;
	GRFError *error = CallocT<GRFError>(1);

	error->severity = sevstr[severity];

	if (message_id == 0xFF) {
		/* This is a custom error message. */
		const char *message = grf_load_string(&buf, len);
		len -= (strlen(message) + 1);

		error->message = AddGRFString(_cur_grffile->grfid, MESSAGE_STRING_ID, lang, new_scheme, message, STR_UNDEFINED);
	} else {
		error->message = msgstr[message_id];
	}

	if (len > 0) {
		const char *data = grf_load_string(&buf, len);
		len -= (strlen(data) + 1);

		error->data = AddGRFString(_cur_grffile->grfid, MESSAGE_DATA_ID, lang, new_scheme, data, STR_UNDEFINED);
	}

	/* Only two parameter numbers can be used in the string. */
	uint i = 0;
	for (; i < 2 && len > 0; i++) {
		error->param_number[i] = grf_load_byte(&buf);
		len--;
	}
	error->num_params = i;

	_cur_grfconfig->error = error;
}

/* Action 0x0C */
static void GRFComment(byte *buf, int len)
{
	/* <0C> [<ignored...>]
	 *
	 * V ignored       Anything following the 0C is ignored */

	if (len == 1) return;

	int text_len = len - 1;
	const char *text = (const char*)(buf + 1);
	grfmsg(2, "GRFComment: %.*s", text_len, text);
}

/* Action 0x0D (GLS_SAFETYSCAN) */
static void SafeParamSet(byte *buf, int len)
{
	if (!check_length(len, 5, "SafeParamSet")) return;
	buf++;
	uint8 target = grf_load_byte(&buf);

	/* Only writing GRF parameters is considered safe */
	if (target < 0x80) return;

	/* GRM could be unsafe, but as here it can only happen after other GRFs
	 * are loaded, it should be okay. If the GRF tried to use the slots it
	 * reserved, it would be marked unsafe anyway. GRM for (e.g. bridge)
	 * sprites  is considered safe. */

	SETBIT(_cur_grfconfig->flags, GCF_UNSAFE);

	/* Skip remainder of GRF */
	_skip_sprites = -1;
}


static uint32 GetPatchVariable(uint8 param)
{
	switch (param) {
		/* start year - 1920 */
		case 0x0B: return _cur_year - ORIGINAL_BASE_YEAR;
		/* freight trains weight factor */
		case 0x0E: return _patches.freight_trains;
		/* empty wagon speed increase */
		case 0x0F: return 0;
		/* plane speed factor */
		case 0x10: return 4;
		/* 2CC colormap base sprite */
		case 0x11: return SPR_2CCMAP_BASE;

		default:
			grfmsg(2, "ParamSet: Unknown Patch variable 0x%02X.", param);
			return 0;
	}
}


/* Action 0x0D */
static void ParamSet(byte *buf, int len)
{
	/* <0D> <target> <operation> <source1> <source2> [<data>]
	 *
	 * B target        parameter number where result is stored
	 * B operation     operation to perform, see below
	 * B source1       first source operand
	 * B source2       second source operand
	 * D data          data to use in the calculation, not necessary
	 *                 if both source1 and source2 refer to actual parameters
	 *
	 * Operations
	 * 00      Set parameter equal to source1
	 * 01      Addition, source1 + source2
	 * 02      Subtraction, source1 - source2
	 * 03      Unsigned multiplication, source1 * source2 (both unsigned)
	 * 04      Signed multiplication, source1 * source2 (both signed)
	 * 05      Unsigned bit shift, source1 by source2 (source2 taken to be a
	 *         signed quantity; left shift if positive and right shift if
	 *         negative, source1 is unsigned)
	 * 06      Signed bit shift, source1 by source2
	 *         (source2 like in 05, and source1 as well)
	 */

	if (!check_length(len, 5, "ParamSet")) return;
	buf++;
	uint8 target = grf_load_byte(&buf);
	uint8 oper   = grf_load_byte(&buf);
	uint32 src1  = grf_load_byte(&buf);
	uint32 src2  = grf_load_byte(&buf);

	uint32 data = 0;
	if (len >= 8) data = grf_load_dword(&buf);

	/* You can add 80 to the operation to make it apply only if the target
	 * is not defined yet.  In this respect, a parameter is taken to be
	 * defined if any of the following applies:
	 * - it has been set to any value in the newgrf(w).cfg parameter list
	 * - it OR A PARAMETER WITH HIGHER NUMBER has been set to any value by
	 *   an earlier action D */
	if (HASBIT(oper, 7)) {
		if (target < 0x80 && target < _cur_grffile->param_end) {
			grfmsg(7, "ParamSet: Param %u already defined, skipping", target);
			return;
		}

		oper = GB(oper, 0, 7);
	}

	if (src2 == 0xFE) {
		if (GB(data, 0, 8) == 0xFF) {
			if (data == 0x0000FFFF) {
				/* Patch variables */
				src1 = GetPatchVariable(src1);
			} else {
				/* GRF Resource Management */
				if (_cur_stage != GLS_ACTIVATION) {
					/* Ignore GRM during initialization */
					src1 = 0;
				} else {
					uint8  op      = src1;
					uint8  feature = GB(data, 8, 8);
					uint16 count   = GB(data, 16, 16);

					switch (feature) {
						case 0x00: // Trains
						case 0x01: // Road Vehicles
						case 0x02: // Ships
						case 0x03: // Aircraft
						{
							uint start = 0;
							uint size  = 0;
							uint shift = _vehshifts[feature];

							if (op == 6) {
								/* Return GRFID of set that reserved ID */
								src1 = _grm_engines[shift + _cur_grffile->param[target]];
								break;
							}

							/* With an operation of 2 or 3, we want to reserve a specific block of IDs */
							if (op == 2 || op == 3) start = _cur_grffile->param[target];

							for (uint i = start; i < _vehcounts[feature]; i++) {
								if (_grm_engines[shift + i] == 0) {
									size++;
								} else {
									if (op == 2 || op == 3) break;
									start = i + 1;
									size = 0;
								}

								if (size == count) break;
							}

							if (size == count) {
								/* Got the slot... */
								if (op == 0 || op == 3) {
									grfmsg(2, "ParamSet: GRM: Reserving %d vehicles at %d", count, start);
									for (uint i = 0; i < count; i++) _grm_engines[shift + start + i] = _cur_grffile->grfid;
								}
								src1 = start;
							} else {
								/* Unable to allocate */
								if (op != 4 && op != 5) {
									/* Deactivate GRF */
									grfmsg(0, "ParamSet: GRM: Unable to allocate %d vehicles, deactivating", count);
									_cur_grfconfig->status = GCS_DISABLED;

									_skip_sprites = -1;
									return;
								}

								grfmsg(1, "ParamSet: GRM: Unable to allocate %d vehicles", count);
								src1 = UINT_MAX;
							}
							break;
						}

						case 0x08: // General sprites
							switch (op) {
								case 0:
									/* Check if the allocated sprites will fit below the original sprite limit */
									if (_cur_spriteid + count >= 16384) {
										grfmsg(0, "ParamSet: GRM: Unable to allocate %d sprites; try changing NewGRF order", count);
										_cur_grfconfig->status = GCS_DISABLED;

										_skip_sprites = -1;
										return;
									}

									/* 'Reserve' space at the current sprite ID */
									src1 = _cur_spriteid;
									_cur_spriteid += count;
									break;

								case 1:
									src1 = _cur_spriteid;
									break;

								default:
									grfmsg(1, "ParamSet: GRM: Unsupported operation %d for general sprites", op);
									return;
							}
							break;

						default: grfmsg(1, "ParamSet: GRM: Unsupported feature 0x%X", feature); return;
					}
				}
			}
		} else {
			/* Read another GRF File's parameter */
			const GRFFile *file = GetFileByGRFID(data);
			if (file == NULL || src1 >= file->param_end) {
				src1 = 0;
			} else {
				src1 = file->param[src1];
			}
		}
	} else {
		/* The source1 and source2 operands refer to the grf parameter number
		 * like in action 6 and 7.  In addition, they can refer to the special
		 * variables available in action 7, or they can be FF to use the value
		 * of <data>.  If referring to parameters that are undefined, a value
		 * of 0 is used instead.  */
		src1 = (src1 == 0xFF) ? data : GetParamVal(src1, NULL);
		src2 = (src2 == 0xFF) ? data : GetParamVal(src2, NULL);
	}

	/* TODO: You can access the parameters of another GRF file by using
	 * source2=FE, source1=the other GRF's parameter number and data=GRF
	 * ID.  This is only valid with operation 00 (set).  If the GRF ID
	 * cannot be found, a value of 0 is used for the parameter value
	 * instead. */

	uint32 res;
	switch (oper) {
		case 0x00:
			res = src1;
			break;

		case 0x01:
			res = src1 + src2;
			break;

		case 0x02:
			res = src1 - src2;
			break;

		case 0x03:
			res = src1 * src2;
			break;

		case 0x04:
			res = (int32)src1 * (int32)src2;
			break;

		case 0x05:
			if ((int32)src2 < 0) {
				res = src1 >> -(int32)src2;
			} else {
				res = src1 << src2;
			}
			break;

		case 0x06:
			if ((int32)src2 < 0) {
				res = (int32)src1 >> -(int32)src2;
			} else {
				res = (int32)src1 << src2;
			}
			break;

		case 0x07: // Bitwise AND
			res = src1 & src2;
			break;

		case 0x08: // Bitwise OR
			res = src1 | src2;
			break;

		case 0x09: // Unsigned division
			if (src2 == 0) {
				res = src1;
			} else {
				res = src1 / src2;
			}
			break;

		case 0x0A: // Signed divison
			if (src2 == 0) {
				res = src1;
			} else {
				res = (int32)src1 / (int32)src2;
			}
			break;

		case 0x0B: // Unsigned modulo
			if (src2 == 0) {
				res = src1;
			} else {
				res = src1 % src2;
			}
			break;

		case 0x0C: // Signed modulo
			if (src2 == 0) {
				res = src1;
			} else {
				res = (int32)src1 % (int32)src2;
			}
			break;

		default: grfmsg(0, "ParamSet: Unknown operation %d, skipping", oper); return;
	}

	switch (target) {
		case 0x8E: // Y-Offset for train sprites
			_traininfo_vehicle_pitch = res;
			break;

		/* @todo implement */
		case 0x8F: // Rail track type cost factors
		case 0x93: // Tile refresh offset to left
		case 0x94: // Tile refresh offset to right
		case 0x95: // Tile refresh offset upwards
		case 0x96: // Tile refresh offset downwards
		case 0x97: // Snow line height
		case 0x99: // Global ID offset
			grfmsg(7, "ParamSet: Skipping unimplemented target 0x%02X", target);
			break;

		case 0x9E: // Miscellaneous GRF features
			_misc_grf_features = res;
			/* Set train list engine width */
			_traininfo_vehicle_width = HasGrfMiscBit(GMB_TRAIN_WIDTH_32_PIXELS) ? 32 : 29;
			break;

		default:
			if (target < 0x80) {
				_cur_grffile->param[target] = res;
				if (target + 1U > _cur_grffile->param_end) _cur_grffile->param_end = target + 1;
			} else {
				grfmsg(7, "ParamSet: Skipping unknown target 0x%02X", target);
			}
			break;
	}
}

/* Action 0x0E (GLS_SAFETYSCAN) */
static void SafeGRFInhibit(byte *buf, int len)
{
	/* <0E> <num> <grfids...>
	 *
	 * B num           Number of GRFIDs that follow
	 * D grfids        GRFIDs of the files to deactivate */

	if (!check_length(len, 2, "GRFInhibit")) return;
	buf++;
	uint8 num = grf_load_byte(&buf);
	if (!check_length(len, 2 + 4 * num, "GRFInhibit")) return;

	for (uint i = 0; i < num; i++) {
		uint32 grfid = grf_load_dword(&buf);

		/* GRF is unsafe it if tries to deactivate other GRFs */
		if (grfid != _cur_grfconfig->grfid) {
			SETBIT(_cur_grfconfig->flags, GCF_UNSAFE);

			/* Skip remainder of GRF */
			_skip_sprites = -1;

			return;
		}
	}
}

/* Action 0x0E */
static void GRFInhibit(byte *buf, int len)
{
	/* <0E> <num> <grfids...>
	 *
	 * B num           Number of GRFIDs that follow
	 * D grfids        GRFIDs of the files to deactivate */

	if (!check_length(len, 2, "GRFInhibit")) return;
	buf++;
	uint8 num = grf_load_byte(&buf);
	if (!check_length(len, 2 + 4 * num, "GRFInhibit")) return;

	for (uint i = 0; i < num; i++) {
		uint32 grfid = grf_load_dword(&buf);
		GRFConfig *file = GetGRFConfig(grfid);

		/* Unset activation flag */
		if (file != NULL && file != _cur_grfconfig) {
			grfmsg(2, "GRFInhibit: Deactivating file '%s'", file->filename);
			file->status = GCS_DISABLED;
		}
	}
}

/* Action 0x10 */
static void DefineGotoLabel(byte *buf, int len)
{
	/* <10> <label> [<comment>]
	 *
	 * B label      The label to define
	 * V comment    Optional comment - ignored */

	if (!check_length(len, 1, "DefineGotoLabel")) return;
	buf++; len--;

	GRFLabel *label = MallocT<GRFLabel>(1);
	label->label    = grf_load_byte(&buf);
	label->nfo_line = _nfo_line;
	label->pos      = FioGetPos();
	label->next     = NULL;

	/* Set up a linked list of goto targets which we will search in an Action 0x7/0x9 */
	if (_cur_grffile->label == NULL) {
		_cur_grffile->label = label;
	} else {
		/* Attach the label to the end of the list */
		GRFLabel *l;
		for (l = _cur_grffile->label; l->next != NULL; l = l->next);
		l->next = label;
	}

	grfmsg(2, "DefineGotoLabel: GOTO target with label 0x%02X", label->label);
}

/* Action 0x11 */
static void GRFSound(byte *buf, int len)
{
	/* <11> <num>
	 *
	 * W num      Number of sound files that follow */

	if (!check_length(len, 1, "GRFSound")) return;
	buf++;
	uint16 num = grf_load_word(&buf);

	_grf_data_blocks = num;
	_grf_data_type   = GDT_SOUND;

	if (_cur_grffile->sound_offset == 0) _cur_grffile->sound_offset = GetNumSounds();
}

static void ImportGRFSound(byte *buf, int len)
{
	const GRFFile *file;
	FileEntry *se = AllocateFileEntry();
	uint32 grfid = grf_load_dword(&buf);
	uint16 sound = grf_load_word(&buf);

	file = GetFileByGRFID(grfid);
	if (file == NULL || file->sound_offset == 0) {
		grfmsg(1, "ImportGRFSound: Source file not available");
		return;
	}

	if (file->sound_offset + sound >= GetNumSounds()) {
		grfmsg(1, "ImportGRFSound: Sound effect %d is invalid", sound);
		return;
	}

	grfmsg(2, "ImportGRFSound: Copying sound %d (%d) from file %X", sound, file->sound_offset + sound, grfid);

	*se = *GetSound(file->sound_offset + sound);

	/* Reset volume and priority, which TTDPatch doesn't copy */
	se->volume   = 128;
	se->priority = 0;
}

/* 'Action 0xFE' */
static void GRFImportBlock(byte *buf, int len)
{
	if (_grf_data_blocks == 0) {
		grfmsg(2, "GRFImportBlock: Unexpected import block, skipping");
		return;
	}

	buf++;

	_grf_data_blocks--;

	/* XXX 'Action 0xFE' isn't really specified. It is only mentioned for
	 * importing sounds, so this is probably all wrong... */
	if (grf_load_byte(&buf) != _grf_data_type) {
		grfmsg(1, "GRFImportBlock: Import type mismatch");
	}

	switch (_grf_data_type) {
		case GDT_SOUND: ImportGRFSound(buf, len - 1); break;
		default: NOT_REACHED(); break;
	}
}

static void LoadGRFSound(byte *buf, int len)
{
	byte *buf_start = buf;

	/* Allocate a sound entry. This is done even if the data is not loaded
	 * so that the indices used elsewhere are still correct. */
	FileEntry *se = AllocateFileEntry();

	if (grf_load_dword(&buf) != BSWAP32('RIFF')) {
		grfmsg(1, "LoadGRFSound: Missing RIFF header");
		return;
	}

	/* Size of file -- we ignore this */
	grf_load_dword(&buf);

	if (grf_load_dword(&buf) != BSWAP32('WAVE')) {
		grfmsg(1, "LoadGRFSound: Invalid RIFF type");
		return;
	}

	for (;;) {
		uint32 tag  = grf_load_dword(&buf);
		uint32 size = grf_load_dword(&buf);

		switch (tag) {
			case ' tmf': // 'fmt '
				/* Audio format, must be 1 (PCM) */
				if (grf_load_word(&buf) != 1) {
					grfmsg(1, "LoadGRFSound: Invalid audio format");
					return;
				}
				se->channels = grf_load_word(&buf);
				se->rate = grf_load_dword(&buf);
				grf_load_dword(&buf);
				grf_load_word(&buf);
				se->bits_per_sample = grf_load_word(&buf);

				/* Consume any extra bytes */
				for (; size > 16; size--) grf_load_byte(&buf);
				break;

			case 'atad': // 'data'
				se->file_size    = size;
				se->file_offset  = FioGetPos() - (len - (buf - buf_start)) + 1;
				se->file_offset |= _file_index << 24;

				/* Set default volume and priority */
				se->volume = 0x80;
				se->priority = 0;

				grfmsg(2, "LoadGRFSound: channels %u, sample rate %u, bits per sample %u, length %u", se->channels, se->rate, se->bits_per_sample, size);
				return;

			default:
				se->file_size = 0;
				return;
		}
	}
}

/* Action 0x12 */
static void LoadFontGlyph(byte *buf, int len)
{
	/* <12> <num_def> <font_size> <num_char> <base_char>
	 *
	 * B num_def      Number of definitions
	 * B font_size    Size of font (0 = normal, 1 = small, 2 = large)
	 * B num_char     Number of consecutive glyphs
	 * W base_char    First character index */

	buf++; len--;
	if (!check_length(len, 1, "LoadFontGlyph")) return;

	uint8 num_def = grf_load_byte(&buf);

	if (!check_length(len, 1 + num_def * 4, "LoadFontGlyph")) return;

	for (uint i = 0; i < num_def; i++) {
		FontSize size    = (FontSize)grf_load_byte(&buf);
		uint8  num_char  = grf_load_byte(&buf);
		uint16 base_char = grf_load_word(&buf);

		grfmsg(7, "LoadFontGlyph: Loading %u glyph(s) at 0x%04X for size %u", num_char, base_char, size);

		for (uint c = 0; c < num_char; c++) {
			SetUnicodeGlyph(size, base_char + c, _cur_spriteid);
			LoadNextSprite(_cur_spriteid++, _file_index);
			_nfo_line++;
		}
	}
}

/* Action 0x13 */
static void TranslateGRFStrings(byte *buf, int len)
{
	/* <13> <grfid> <num-ent> <offset> <text...>
	 *
	 * 4*B grfid     The GRFID of the file whose texts are to be translated
	 * B   num-ent   Number of strings
	 * W   offset    First text ID
	 * S   text...   Zero-terminated strings */

	buf++; len--;
	if (!check_length(len, 7, "TranslateGRFString")) return;

	uint32 grfid = grf_load_dword(&buf);
	const GRFConfig *c = GetGRFConfig(grfid);
	if (c == NULL || (c->status != GCS_INITIALISED && c->status != GCS_ACTIVATED)) {
		grfmsg(7, "TranslateGRFStrings: GRFID 0x%08x unknown, skipping action 13", BSWAP32(grfid));
		return;
	}

	if (c->status == GCS_INITIALISED) {
		/* If the file is not active but will be activated later, give an error
		 * and disable this file. */
		GRFError *error = CallocT<GRFError>(1);
		error->message  = STR_NEWGRF_ERROR_LOAD_AFTER;
		error->data     = STR_NEWGRF_ERROR_AFTER_TRANSLATED_FILE;
		error->severity = STR_NEWGRF_ERROR_MSG_FATAL;

		if (_cur_grfconfig->error != NULL) free(_cur_grfconfig->error);
		_cur_grfconfig->error = error;

		_cur_grfconfig->status = GCS_DISABLED;
		_skip_sprites = -1;
		return;
	}

	byte num_strings = grf_load_byte(&buf);
	uint16 first_id  = grf_load_word(&buf);

	if (!((first_id >= 0xD000 && first_id + num_strings <= 0xD3FF) || (first_id >= 0xDC00 && first_id + num_strings <= 0xDCFF))) {
		grfmsg(7, "TranslateGRFStrings: Attempting to set out-of-range string IDs in action 13 (first: 0x%4X, number: 0x%2X)", first_id, num_strings);
		return;
	}

	len -= 7;

	for (uint i = 0; i < num_strings && len > 0; i++) {
		const char *string   = grf_load_string(&buf, len);
		size_t string_length = strlen(string) + 1;

		len -= (int)string_length;

		if (string_length == 1) {
			grfmsg(7, "TranslateGRFString: Ignoring empty string.");
			continue;
		}

		/* Since no language id is supplied this string has to be added as a
		 * generic string, thus the language id of 0x7F. For this to work
		 * new_scheme has to be true as well. A language id of 0x7F will be
		 * overridden by a non-generic id, so this will not change anything if
		 * a string has been provided specifically for this language. */
		AddGRFString(grfid, first_id + i, 0x7F, true, string, STR_UNDEFINED);
	}
}

/* 'Action 0xFF' */
static void GRFDataBlock(byte *buf, int len)
{
	if (_grf_data_blocks == 0) {
		grfmsg(2, "GRFDataBlock: unexpected data block, skipping");
		return;
	}

	buf++;
	uint8 name_len = grf_load_byte(&buf);
	const char *name = (const char *)buf;
	buf += name_len + 1;

	grfmsg(2, "GRFDataBlock: block name '%s'...", name);

	_grf_data_blocks--;

	switch (_grf_data_type) {
		case GDT_SOUND: LoadGRFSound(buf, len - name_len - 2); break;
		default: NOT_REACHED(); break;
	}
}


/* Used during safety scan on unsafe actions */
static void GRFUnsafe(byte *buf, int len)
{
	SETBIT(_cur_grfconfig->flags, GCF_UNSAFE);

	/* Skip remainder of GRF */
	_skip_sprites = -1;
}


static void InitializeGRFSpecial()
{
	_ttdpatch_flags[0] =  ((_patches.always_small_airport ? 1 : 0) << 0x0C)  // keepsmallairport
	                   |                                        (1 << 0x0D)  // newairports
	                   |                                        (1 << 0x0E)  // largestations
	                   |           ((_patches.longbridges ? 1 : 0) << 0x0F)  // longbridges
	                   |                                        (0 << 0x10)  // loadtime
	                   |                                        (1 << 0x12)  // presignals
	                   |                                        (1 << 0x13)  // extpresignals
	                   | ((_patches.never_expire_vehicles ? 1 : 0) << 0x16)  // enginespersist
	                   |                                        (1 << 0x1B)  // multihead
	                   |                                        (1 << 0x1D)  // lowmemory
	                   |                                        (1 << 0x1E); // generalfixes

	_ttdpatch_flags[1] =                                        (0 << 0x07)  // moreairports - based on units of noise
	                   |        ((_patches.mammoth_trains ? 1 : 0) << 0x08)  // mammothtrains
	                   |                                        (1 << 0x09)  // trainrefit
	                   |                                        (0 << 0x0B)  // subsidiaries
	                   |       ((_patches.gradual_loading ? 1 : 0) << 0x0C)  // gradualloading
	                   |                                        (1 << 0x12)  // unifiedmaglevmode - set bit 0 mode. Not revelant to OTTD
	                   |                                        (1 << 0x13)  // unifiedmaglevmode - set bit 1 mode
	                   |                                        (1 << 0x14)  // bridgespeedlimits
	                   |                                        (1 << 0x16)  // eternalgame
	                   |                                        (1 << 0x17)  // newtrains
	                   |                                        (1 << 0x18)  // newrvs
	                   |                                        (1 << 0x19)  // newships
	                   |                                        (1 << 0x1A)  // newplanes
	                   |           ((_patches.signal_side ? 1 : 0) << 0x1B)  // signalsontrafficside
	                   |                                        (1 << 0x1C); // electrifiedrailway

	_ttdpatch_flags[2] =                                        (1 << 0x01)  // loadallgraphics - obsolote
	                   |                                        (1 << 0x03)  // semaphores
	                   |                                        (0 << 0x0B)  // enhancedgui
	                   |                                        (0 << 0x0C)  // newagerating
	                   |       ((_patches.build_on_slopes ? 1 : 0) << 0x0D)  // buildonslopes
	                   |         ((_patches.full_load_any ? 1 : 0) << 0x0E)  // fullloadany
	                   |                                        (0 << 0x0F)  // planespeed
	                   |                                        (0 << 0x10)  // moreindustriesperclimate - obsolete
	                   |                                        (0 << 0x11)  // moretoylandfeatures
	                   |                                        (1 << 0x12)  // newstations
	                   |                                        (0 << 0x13)  // tracktypecostdiff
	                   |                                        (1 << 0x14)  // manualconvert
	                   |       ((_patches.build_on_slopes ? 1 : 0) << 0x15)  // buildoncoasts
	                   |                                        (1 << 0x16)  // canals
	                   |                                        (1 << 0x17)  // newstartyear
	                   |    ((_patches.freight_trains > 1 ? 1 : 0) << 0x18)  // freighttrains
	                   |                                        (1 << 0x19)  // newhouses
	                   |                                        (1 << 0x1A)  // newbridges
	                   |                                        (0 << 0x1B)  // newtownnames
	                   |                                        (0 << 0x1C)  // moreanimations
	                   |    ((_patches.wagon_speed_limits ? 1 : 0) << 0x1D)  // wagonspeedlimits
	                   |                                        (1 << 0x1E)  // newshistory
	                   |                                        (0 << 0x1F); // custombridgeheads

	_ttdpatch_flags[3] =                                        (0 << 0x00)  // newcargodistribution
	                   |                                        (1 << 0x01)  // windowsnap
	                   |                                        (0 << 0x02)  // townbuildnoroad
	                   |                                        (0 << 0x03)  // pathbasedsignalling. To enable if ever pbs is back
	                   |                                        (0 << 0x04)  // aichoosechance
	                   |                                        (1 << 0x05)  // resolutionwidth
	                   |                                        (1 << 0x06)  // resolutionheight
	                   |                                        (0 << 0x07)  // newindustries
	                   |                                        (0 << 0x08)  // fifoloading
	                   |                                        (0 << 0x09)  // townroadbranchprob
	                   |                                        (0 << 0x0A)  // tempsnowline
	                   |                                        (1 << 0x0B)  // newcargo
	                   |                                        (1 << 0x0C)  // enhancemultiplayer
	                   |                                        (1 << 0x0D)  // onewayroads
	                   |   ((_patches.nonuniform_stations ? 1 : 0) << 0x0E)  // irregularstations
	                   |                                        (1 << 0x0F)  // statistics
	                   |                                        (1 << 0x10)  // newsounds
	                   |                                        (1 << 0x11)  // autoreplace
	                   |                                        (1 << 0x12)  // autoslope
	                   |                                        (0 << 0x13)  // followvehicle
	                   |                                        (0 << 0x14)  // trams
	                   |                                        (0 << 0x15)  // enhancetunnels
	                   |                                        (0 << 0x16)  // shortrvs
	                   |                                        (0 << 0x17)  // articulatedrvs
	                   |                                        (1 << 0x1E); // variablerunningcosts
}

static void ResetCustomStations()
{
	for (GRFFile *file = _first_grffile; file != NULL; file = file->next) {
		if (file->stations == NULL) continue;
		for (uint i = 0; i < MAX_STATIONS; i++) {
			if (file->stations[i] == NULL) continue;
			StationSpec *statspec = file->stations[i];

			/* Release renderdata, if it wasn't copied from another custom station spec  */
			if (!statspec->copied_renderdata) {
				for (uint t = 0; t < statspec->tiles; t++) {
					free((void*)statspec->renderdata[t].seq);
				}
				free(statspec->renderdata);
			}

			/* Release platforms and layouts */
			if (!statspec->copied_layouts) {
				for (uint l = 0; l < statspec->lengths; l++) {
					for (uint p = 0; p < statspec->platforms[l]; p++) {
						free(statspec->layouts[l][p]);
					}
					free(statspec->layouts[l]);
				}
				free(statspec->layouts);
				free(statspec->platforms);
			}

			/* Release this station */
			free(statspec);
		}

		/* Free and reset the station data */
		free(file->stations);
		file->stations = NULL;
	}
}

static void ResetCustomHouses()
{
	GRFFile *file;
	uint i;

	for (file = _first_grffile; file != NULL; file = file->next) {
		if (file->housespec == NULL) continue;
		for (i = 0; i < HOUSE_MAX; i++) free(file->housespec[i]);

		free(file->housespec);
		file->housespec = NULL;
	}
}

static void ResetNewGRF()
{
	GRFFile *next;

	for (GRFFile *f = _first_grffile; f != NULL; f = next) {
		next = f->next;

		free(f->filename);
		free(f);
	}

	_first_grffile = NULL;
	_cur_grffile   = NULL;
}

/**
 * Reset all NewGRF loaded data
 * TODO
 */
static void ResetNewGRFData()
{
	CleanUpStrings();

	/* Copy/reset original engine info data */
	memcpy(&_engine_info, &orig_engine_info, sizeof(orig_engine_info));
	memcpy(&_rail_vehicle_info, &orig_rail_vehicle_info, sizeof(orig_rail_vehicle_info));
	memcpy(&_ship_vehicle_info, &orig_ship_vehicle_info, sizeof(orig_ship_vehicle_info));
	memcpy(&_aircraft_vehicle_info, &orig_aircraft_vehicle_info, sizeof(orig_aircraft_vehicle_info));
	memcpy(&_road_vehicle_info, &orig_road_vehicle_info, sizeof(orig_road_vehicle_info));

	/* Copy/reset original bridge info data
	 * First, free sprite table data */
	for (uint i = 0; i < MAX_BRIDGES; i++) {
		if (_bridge[i].sprite_table != NULL) {
			for (uint j = 0; j < 7; j++) free(_bridge[i].sprite_table[j]);
			free(_bridge[i].sprite_table);
		}
	}
	memcpy(&_bridge, &orig_bridge, sizeof(_bridge));

	/* Reset refit/cargo class data */
	memset(&cargo_allowed, 0, sizeof(cargo_allowed));
	memset(&cargo_disallowed, 0, sizeof(cargo_disallowed));

	/* Reset GRM reservations */
	memset(&_grm_engines, 0, sizeof(_grm_engines));

	/* Unload sprite group data */
	UnloadWagonOverrides();
	UnloadRotorOverrideSprites();
	UnloadCustomEngineSprites();
	UnloadCustomEngineNames();
	ResetEngineListOrder();

	/* Reset price base data */
	ResetPriceBaseMultipliers();

	/* Reset the curencies array */
	ResetCurrencies();

	/* Reset the house array */
	ResetCustomHouses();
	ResetHouses();

	/* Reset station classes */
	ResetStationClasses();
	ResetCustomStations();

	/* Reset the snowline table. */
	ClearSnowLine();

	/* Reset NewGRF files */
	ResetNewGRF();

	/* Add engine type to engine data. This is needed for the refit precalculation. */
	AddTypeToEngines();

	/* Set up the default cargo types */
	SetupCargoForClimate(_opt.landscape);

	/* Reset misc GRF features and train list display variables */
	_misc_grf_features = 0;
	_traininfo_vehicle_pitch = 0;
	_traininfo_vehicle_width = 29;
	_have_2cc = false;
	_have_newhouses = false;
	_signal_base = 0;
	_coast_base = 0;

	InitializeSoundPool();
	InitializeSpriteGroupPool();
}

/** Reset all NewGRFData that was used only while processing data */
static void ClearTemporaryNewGRFData()
{
	/* Clear the GOTO labels used for GRF processing */
	for (GRFLabel *l = _cur_grffile->label; l != NULL;) {
		GRFLabel *l2 = l->next;
		free(l);
		l = l2;
	}
	_cur_grffile->label = NULL;

	/* Clear the list of spritegroups */
	free(_cur_grffile->spritegroups);
	_cur_grffile->spritegroups = NULL;
	_cur_grffile->spritegroups_count = 0;
}

static void BuildCargoTranslationMap()
{
	memset(_cur_grffile->cargo_map, 0xFF, sizeof(_cur_grffile->cargo_map));

	for (CargoID c = 0; c < NUM_CARGO; c++) {
		const CargoSpec *cs = GetCargo(c);
		if (!cs->IsValid()) continue;

		if (_cur_grffile->cargo_max == 0) {
			/* Default translation table, so just a straight mapping to bitnum */
			_cur_grffile->cargo_map[c] = cs->bitnum;
		} else {
			/* Check the translation table for this cargo's label */
			for (uint i = 0; i < _cur_grffile->cargo_max; i++) {
				if (cs->label == _cur_grffile->cargo_list[i]) {
					_cur_grffile->cargo_map[c] = i;
					break;
				}
			}
		}
	}
}

static void InitNewGRFFile(const GRFConfig *config, int sprite_offset)
{
	GRFFile *newfile = GetFileByFilename(config->filename);
	if (newfile != NULL) {
		/* We already loaded it once. */
		newfile->sprite_offset = sprite_offset;
		_cur_grffile = newfile;
		return;
	}

	newfile = CallocT<GRFFile>(1);

	if (newfile == NULL) error ("Out of memory");

	newfile->filename = strdup(config->full_path);
	newfile->sprite_offset = sprite_offset;

	/* Copy the initial parameter list */
	assert(lengthof(newfile->param) == lengthof(config->param) && lengthof(config->param) == 0x80);
	newfile->param_end = config->num_params;
	memcpy(newfile->param, config->param, sizeof(newfile->param));

	if (_first_grffile == NULL) {
		_cur_grffile = newfile;
		_first_grffile = newfile;
	} else {
		_cur_grffile->next = newfile;
		_cur_grffile = newfile;
	}
}


/** List of what cargo labels are refittable for the given the vehicle-type.
 * Only currently active labels are applied. */
static const CargoLabel _default_refitmasks_rail[] = {
	'PASS', 'COAL', 'MAIL', 'LVST', 'GOOD', 'GRAI', 'WHEA', 'MAIZ', 'WOOD',
	'IORE', 'STEL', 'VALU', 'GOLD', 'DIAM', 'PAPR', 'FOOD', 'FRUT', 'CORE',
	'WATR', 'SUGR', 'TOYS', 'BATT', 'SWET', 'TOFF', 'COLA', 'CTCD', 'BUBL',
	'PLST', 'FZDR',
	0 };

static const CargoLabel _default_refitmasks_road[] = {
	0 };

static const CargoLabel _default_refitmasks_ships[] = {
	'COAL', 'MAIL', 'LVST', 'GOOD', 'GRAI', 'WHEA', 'MAIZ', 'WOOD', 'IORE',
	'STEL', 'VALU', 'GOLD', 'DIAM', 'PAPR', 'FOOD', 'FRUT', 'CORE', 'WATR',
	'RUBR', 'SUGR', 'TOYS', 'BATT', 'SWET', 'TOFF', 'COLA', 'CTCD', 'BUBL',
	'PLST', 'FZDR',
	0 };

static const CargoLabel _default_refitmasks_aircraft[] = {
	'PASS', 'MAIL', 'GOOD', 'VALU', 'GOLD', 'DIAM', 'FOOD', 'FRUT', 'SUGR',
	'TOYS', 'BATT', 'SWET', 'TOFF', 'COLA', 'CTCD', 'BUBL', 'PLST', 'FZDR',
	0 };

static const CargoLabel *_default_refitmasks[] = {
	_default_refitmasks_rail,
	_default_refitmasks_road,
	_default_refitmasks_ships,
	_default_refitmasks_aircraft,
};


/**
 * Precalculate refit masks from cargo classes for all vehicles.
 */
static void CalculateRefitMasks()
{
	for (EngineID engine = 0; engine < TOTAL_NUM_ENGINES; engine++) {
		uint32 mask = 0;
		uint32 not_mask = 0;
		uint32 xor_mask = 0;

		if (_engine_info[engine].refit_mask != 0) {
			const GRFFile *file = GetEngineGRF(engine);
			if (file != NULL && file->cargo_max != 0) {
				/* Apply cargo translation table to the refit mask */
				uint num_cargo = min(32, file->cargo_max);
				for (uint i = 0; i < num_cargo; i++) {
					if (!HASBIT(_engine_info[engine].refit_mask, i)) continue;

					CargoID c = GetCargoIDByLabel(file->cargo_list[i]);
					if (c == CT_INVALID) continue;

					SETBIT(xor_mask, c);
				}
			} else {
				/* No cargo table, so use the cargo bitnum values */
				for (CargoID c = 0; c < NUM_CARGO; c++) {
					const CargoSpec *cs = GetCargo(c);
					if (!cs->IsValid()) continue;

					if (HASBIT(_engine_info[engine].refit_mask, cs->bitnum)) SETBIT(xor_mask, c);
				}
			}
		}

		if (cargo_allowed[engine] != 0) {
			/* Build up the list of cargo types from the set cargo classes. */
			for (CargoID i = 0; i < NUM_CARGO; i++) {
				const CargoSpec *cs = GetCargo(i);
				if (cargo_allowed[engine]    & cs->classes) SETBIT(mask,     i);
				if (cargo_disallowed[engine] & cs->classes) SETBIT(not_mask, i);
			}
		} else {
			/* Don't apply default refit mask to wagons or engines with no capacity */
			if (xor_mask == 0 && (
						GetEngine(engine)->type != VEH_TRAIN || (
							RailVehInfo(engine)->capacity != 0 &&
							RailVehInfo(engine)->railveh_type != RAILVEH_WAGON
						)
					)) {
				const CargoLabel *cl = _default_refitmasks[GetEngine(engine)->type];
				for (uint i = 0;; i++) {
					if (cl[i] == 0) break;

					CargoID cargo = GetCargoIDByLabel(cl[i]);
					if (cargo == CT_INVALID) continue;

					SETBIT(xor_mask, cargo);
				}
			}
		}
		_engine_info[engine].refit_mask = ((mask & ~not_mask) ^ xor_mask) & _cargo_mask;

		/* Check if this engine's cargo type is valid. If not, set to the first refittable
		 * cargo type. Apparently cargo_type isn't a common property... */
		switch (GetEngine(engine)->type) {
			case VEH_TRAIN: {
				RailVehicleInfo *rvi = &_rail_vehicle_info[engine];
				if (rvi->cargo_type == CT_INVALID) rvi->cargo_type = FindFirstRefittableCargo(engine);
				if (rvi->cargo_type == CT_INVALID) _engine_info[engine].climates = 0;
				break;
			}
			case VEH_ROAD: {
				RoadVehicleInfo *rvi = &_road_vehicle_info[engine - ROAD_ENGINES_INDEX];
				if (rvi->cargo_type == CT_INVALID) rvi->cargo_type = FindFirstRefittableCargo(engine);
				if (rvi->cargo_type == CT_INVALID) _engine_info[engine].climates = 0;
				break;
			}
			case VEH_SHIP: {
				ShipVehicleInfo *svi = &_ship_vehicle_info[engine - SHIP_ENGINES_INDEX];
				if (svi->cargo_type == CT_INVALID) svi->cargo_type = FindFirstRefittableCargo(engine);
				if (svi->cargo_type == CT_INVALID) _engine_info[engine].climates = 0;
				break;
			}
		}
	}
}

/** Add all new houses to the house array. House properties can be set at any
 * time in the GRF file, so we can only add a house spec to the house array
 * after the file has finished loading. We also need to check the dates, due to
 * the TTDPatch behaviour described below that we need to emulate. */
static void FinaliseHouseArray()
{
	/* If there are no houses with start dates before 1930, then all houses
	 * with start dates of 1930 have them reset to 0. This is in order to be
	 * compatible with TTDPatch, where if no houses have start dates before
	 * 1930 and the date is before 1930, the game pretends that this is 1930.
	 * If there have been any houses defined with start dates before 1930 then
	 * the dates are left alone. */
	bool reset_dates = true;

	for (GRFFile *file = _first_grffile; file != NULL; file = file->next) {
		if (file->housespec == NULL) continue;

		for (int i = 0; i < HOUSE_MAX; i++) {
			HouseSpec *hs = file->housespec[i];
			if (hs != NULL) {
				SetHouseSpec(hs);
				if (hs->min_date < 1930) reset_dates = false;
			}
		}
	}

	if (reset_dates) {
		for (int i = NEW_HOUSE_OFFSET; i < HOUSE_MAX; i++) {
			HouseSpec *hs = GetHouseSpecs(i);

			if (hs->enabled && hs->min_date == 1930) hs->min_date = 0;
		}
	}
}


/** Each cargo string needs to be mapped from TTDPatch to OpenTTD string IDs.
 * This is done after loading so that strings from Action 4 will be mapped
 * properly. */
static void MapNewCargoStrings()
{
	for (CargoID c = 0; c < NUM_CARGO; c++) {
		CargoSpec *cs = &_cargo[c];
		/* Don't map if the cargo is unavailable or not from NewGRF */
		if (!cs->IsValid() || cs->grfid == 0) continue;

		cs->name         = MapGRFStringID(cs->grfid, cs->name);
		cs->name_plural  = MapGRFStringID(cs->grfid, cs->name_plural);
		cs->units_volume = MapGRFStringID(cs->grfid, cs->units_volume);
		cs->quantifier   = MapGRFStringID(cs->grfid, cs->quantifier);
		cs->abbrev       = MapGRFStringID(cs->grfid, cs->abbrev);
	}
}


/* Here we perform initial decoding of some special sprites (as are they
 * described at http://www.ttdpatch.net/src/newgrf.txt, but this is only a very
 * partial implementation yet). */
/* XXX: We consider GRF files trusted. It would be trivial to exploit OTTD by
 * a crafted invalid GRF file. We should tell that to the user somehow, or
 * better make this more robust in the future. */
static void DecodeSpecialSprite(uint num, GrfLoadingStage stage)
{
	/* XXX: There is a difference between staged loading in TTDPatch and
	 * here.  In TTDPatch, for some reason actions 1 and 2 are carried out
	 * during stage 1, whilst action 3 is carried out during stage 2 (to
	 * "resolve" cargo IDs... wtf). This is a little problem, because cargo
	 * IDs are valid only within a given set (action 1) block, and may be
	 * overwritten after action 3 associates them. But overwriting happens
	 * in an earlier stage than associating, so...  We just process actions
	 * 1 and 2 in stage 2 now, let's hope that won't get us into problems.
	 * --pasky */
	/* We need a pre-stage to set up GOTO labels of Action 0x10 because the grf
	 * is not in memory and scanning the file every time would be too expensive.
	 * In other stages we skip action 0x10 since it's already dealt with. */
	static const SpecialSpriteHandler handlers[][GLS_END] = {
		/* 0x00 */ { NULL,     SafeChangeInfo, NULL,       InitChangeInfo, ReserveChangeInfo, FeatureChangeInfo, },
		/* 0x01 */ { NULL,     GRFUnsafe, NULL,            NULL,           NULL,              NewSpriteSet, },
		/* 0x02 */ { NULL,     GRFUnsafe, NULL,            NULL,           NULL,              NewSpriteGroup, },
		/* 0x03 */ { NULL,     GRFUnsafe, NULL,            NULL,           NULL,              FeatureMapSpriteGroup, },
		/* 0x04 */ { NULL,     NULL,      NULL,            NULL,           NULL,              FeatureNewName, },
		/* 0x05 */ { SkipAct5, SkipAct5,  SkipAct5,        SkipAct5,       SkipAct5,          GraphicsNew, },
		/* 0x06 */ { NULL,     NULL,      NULL,            CfgApply,       CfgApply,          CfgApply, },
		/* 0x07 */ { NULL,     NULL,      NULL,            NULL,           SkipIf,            SkipIf, },
		/* 0x08 */ { ScanInfo, NULL,      NULL,            GRFInfo,        NULL,              GRFInfo, },
		/* 0x09 */ { NULL,     NULL,      NULL,            SkipIf,         SkipIf,            SkipIf, },
		/* 0x0A */ { SkipActA, SkipActA,  SkipActA,        SkipActA,       SkipActA,          SpriteReplace, },
		/* 0x0B */ { NULL,     NULL,      NULL,            GRFLoadError,   GRFLoadError,      GRFLoadError, },
		/* 0x0C */ { NULL,     NULL,      NULL,            GRFComment,     NULL,              GRFComment, },
		/* 0x0D */ { NULL,     SafeParamSet, NULL,         ParamSet,       ParamSet,          ParamSet, },
		/* 0x0E */ { NULL,     SafeGRFInhibit, NULL,       GRFInhibit,     GRFInhibit,        GRFInhibit, },
		/* 0x0F */ { NULL,     NULL,      NULL,            NULL,           NULL,              NULL, },
		/* 0x10 */ { NULL,     NULL,      DefineGotoLabel, NULL,           NULL,              NULL, },
		/* 0x11 */ { NULL,     GRFUnsafe, NULL,            NULL,           NULL,              GRFSound, },
		/* 0x12 */ { NULL,     NULL,      NULL,            NULL,           NULL,              LoadFontGlyph, },
		/* 0x13 */ { NULL,     NULL,      NULL,            NULL,           NULL,              TranslateGRFStrings, },
	};

	byte* buf;

	if (_preload_sprite == NULL) {
		/* No preloaded sprite to work with; allocate and read the
		 * pseudo sprite content. */
		buf = MallocT<byte>(num);
		if (buf == NULL) error("DecodeSpecialSprite: Could not allocate memory");
		FioReadBlock(buf, num);
	} else {
		/* Use the preloaded sprite data. */
		buf = _preload_sprite;
		_preload_sprite = NULL;
		grfmsg(7, "DecodeSpecialSprite: Using preloaded pseudo sprite data");

		/* Skip the real (original) content of this action. */
		FioSeekTo(num, SEEK_CUR);
	}

	byte action = buf[0];

	if (action == 0xFF) {
		grfmsg(7, "DecodeSpecialSprite: Handling data block in stage %d", stage);
		GRFDataBlock(buf, num);
	} else if (action == 0xFE) {
		grfmsg(7, "DecodeSpecialSprite: andling import block in stage %d", stage);
		GRFImportBlock(buf, num);
	} else if (action >= lengthof(handlers)) {
		grfmsg(7, "DecodeSpecialSprite: Skipping unknown action 0x%02X", action);
	} else if (handlers[action][stage] == NULL) {
		grfmsg(7, "DecodeSpecialSprite: Skipping action 0x%02X in stage %d", action, stage);
	} else {
		grfmsg(7, "DecodeSpecialSprite: Handling action 0x%02X in stage %d", action, stage);
		handlers[action][stage](buf, num);
	}
	free(buf);
}


void LoadNewGRFFile(GRFConfig *config, uint file_index, GrfLoadingStage stage)
{
	const char *filename = config->full_path;
	uint16 num;

	/* A .grf file is activated only if it was active when the game was
	 * started.  If a game is loaded, only its active .grfs will be
	 * reactivated, unless "loadallgraphics on" is used.  A .grf file is
	 * considered active if its action 8 has been processed, i.e. its
	 * action 8 hasn't been skipped using an action 7.
	 *
	 * During activation, only actions 0, 1, 2, 3, 4, 5, 7, 8, 9, 0A and 0B are
	 * carried out.  All others are ignored, because they only need to be
	 * processed once at initialization.  */
	if (stage != GLS_FILESCAN && stage != GLS_SAFETYSCAN && stage != GLS_LABELSCAN) {
		_cur_grffile = GetFileByFilename(filename);
		if (_cur_grffile == NULL) error("File '%s' lost in cache.\n", filename);
		if (stage == GLS_ACTIVATION && config->status != GCS_INITIALISED) return;
	}

	FioOpenFile(file_index, filename);
	_file_index = file_index; // XXX

	_cur_grfconfig = config;

	DEBUG(grf, 2, "LoadNewGRFFile: Reading NewGRF-file '%s'", filename);

	/* Skip the first sprite; we don't care about how many sprites this
	 * does contain; newest TTDPatches and George's longvehicles don't
	 * neither, apparently. */
	if (FioReadWord() == 4 && FioReadByte() == 0xFF) {
		FioReadDword();
	} else {
		DEBUG(grf, 7, "LoadNewGRFFile: Custom .grf has invalid format");
		return;
	}

	_skip_sprites = 0; // XXX
	_nfo_line = 0;

	while ((num = FioReadWord()) != 0) {
		byte type = FioReadByte();
		_nfo_line++;

		if (type == 0xFF) {
			if (_skip_sprites == 0) {
				DecodeSpecialSprite(num, stage);

				/* Stop all processing if we are to skip the remaining sprites */
				if (_skip_sprites == -1) break;

				continue;
			} else {
				FioSkipBytes(num);
			}
		} else {
			if (_skip_sprites == 0) grfmsg(7, "LoadNewGRFFile: Skipping unexpected sprite");

			FioSkipBytes(7);
			num -= 8;

			if (type & 2) {
				FioSkipBytes(num);
			} else {
				while (num > 0) {
					int8 i = FioReadByte();
					if (i >= 0) {
						num -= i;
						FioSkipBytes(i);
					} else {
						i = -(i >> 3);
						num -= i;
						FioReadByte();
					}
				}
			}
		}

		if (_skip_sprites > 0) _skip_sprites--;
	}
}

void InitDepotWindowBlockSizes();

static void AfterLoadGRFs()
{
	/* Pre-calculate all refit masks after loading GRF files. */
	CalculateRefitMasks();

	/* Set the block size in the depot windows based on vehicle sprite sizes */
	InitDepotWindowBlockSizes();

	/* Add all new houses to the house array. */
	FinaliseHouseArray();

	/* Create dynamic list of industry legends for smallmap_gui.cpp */
	BuildIndustriesLegend();

	/* Map cargo strings. This is a separate step because cargos are
	 * loaded before strings... */
	MapNewCargoStrings();
}

void LoadNewGRF(uint load_index, uint file_index)
{
	InitializeGRFSpecial();

	ResetNewGRFData();

	/* Load newgrf sprites
	 * in each loading stage, (try to) open each file specified in the config
	 * and load information from it. */
	for (GrfLoadingStage stage = GLS_LABELSCAN; stage <= GLS_ACTIVATION; stage++) {
		uint slot = file_index;

		_cur_stage = stage;
		_cur_spriteid = load_index;
		for (GRFConfig *c = _grfconfig; c != NULL; c = c->next) {
			if (c->status == GCS_DISABLED || c->status == GCS_NOT_FOUND) continue;

			/* @todo usererror() */
			if (!FileExists(c->full_path)) error("NewGRF file is missing '%s'", c->filename);

			if (stage == GLS_LABELSCAN) InitNewGRFFile(c, _cur_spriteid);
			LoadNewGRFFile(c, slot++, stage);
			if (stage == GLS_ACTIVATION) {
				ClearTemporaryNewGRFData();
				BuildCargoTranslationMap();
				DEBUG(sprite, 2, "LoadNewGRF: Currently %i sprites are loaded", _cur_spriteid);
			}
		}
	}

	/* Call any functions that should be run after GRFs have been loaded. */
	AfterLoadGRFs();
}

bool HasGrfMiscBit(GrfMiscBit bit)
{
	return HASBIT(_misc_grf_features, bit);
}
