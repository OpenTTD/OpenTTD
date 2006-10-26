/* $Id$ */

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
#include "economy.h"
#include "newgrf_engine.h"
#include "vehicle.h"
#include "newgrf_text.h"
#include "table/sprites.h"
#include "date.h"
#include "currency.h"
#include "sound.h"
#include "newgrf_sound.h"
#include "newgrf_spritegroup.h"

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
SpriteID _signal_base = 0;

static GRFFile *_cur_grffile;
GRFFile *_first_grffile;
GRFConfig *_first_grfconfig;
static SpriteID _cur_spriteid;
static int _cur_stage;
static uint32 _nfo_line;

/* Miscellaneous GRF features, set by Action 0x0D, parameter 0x9E */
static byte _misc_grf_features = 0;

/* 32 * 8 = 256 flags. Apparently TTDPatch uses this many.. */
static uint32 _ttdpatch_flags[8];

/* Used by Action 0x06 to preload a pseudo sprite and modify its content */
static byte *_preload_sprite = NULL;

/* Set if any vehicle is loaded which uses 2cc (two company colours) */
bool _have_2cc = false;


typedef enum GrfDataType {
	GDT_SOUND,
} GrfDataType;

static byte _grf_data_blocks;
static GrfDataType _grf_data_type;


typedef enum grfspec_feature {
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
} grfspec_feature;


typedef void (*SpecialSpriteHandler)(byte *buf, int len);

static const int _vehcounts[4] = {
	/* GSF_TRAIN */    NUM_TRAIN_ENGINES,
	/* GSF_ROAD */     NUM_ROAD_ENGINES,
	/* GSF_SHIP */     NUM_SHIP_ENGINES,
	/* GSF_AIRCRAFT */ NUM_AIRCRAFT_ENGINES
};

static const int _vehshifts[4] = {
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

/* Debugging messages policy:
 *
 * These should be the severities used for direct DEBUG() calls
 * (there is room for exceptions, but you have to have a good cause):
 *
 * 0..2 - dedicated to grfmsg()
 * 3
 * 4
 * 5
 * 6 - action handler entry reporting - one per action
 * 7 - basic action progress reporting - in loops, only single one allowed
 * 8 - more detailed progress reporting - less important stuff, in deep loops etc
 * 9 - extremely detailed progress reporting - detailed reports inside of deep loops and so
 */


typedef enum grfmsg_severity {
	GMS_NOTICE,
	GMS_WARN,
	GMS_ERROR,
	GMS_FATAL,
} grfmsg_severity;

static void CDECL grfmsg(grfmsg_severity severity, const char *str, ...)
{
	static const char* const severitystr[] = {
		"Notice",
		"Warning",
		"Error",
		"Fatal"
	};
	int export_severity = 0;
	char buf[1024];
	va_list va;

	va_start(va, str);
	vsnprintf(buf, sizeof(buf), str, va);
	va_end(va);

	export_severity = 2 - (severity == GMS_FATAL ? 2 : severity);
	DEBUG(grf, export_severity) ("[%s:%d][%s] %s", _cur_grffile->filename, _nfo_line, severitystr[severity], buf);
}


#define check_length(real, wanted, where) \
do { \
	if (real < wanted) { \
		grfmsg(GMS_ERROR, "%s: Invalid special sprite length %d (expected %d)!", \
		       where, real, wanted); \
		return; \
	} \
} while (0)


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


typedef bool (*VCI_Handler)(uint engine, int numinfo, int prop, byte **buf, int len);

#define FOR_EACH_OBJECT for (i = 0; i < numinfo; i++)

static void dewagonize(int condition, int engine)
{
	EngineInfo *ei = &_engine_info[engine];
	RailVehicleInfo *rvi = &_rail_vehicle_info[engine];

	if (condition != 0) {
		ei->unk2 &= ~0x80;
		rvi->flags &= ~2;
	} else {
		ei->unk2 |= 0x80;
		rvi->flags |= 2;
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
		case 0x05: /* Track type */
			FOR_EACH_OBJECT {
				uint8 tracktype = grf_load_byte(&buf);

				switch (tracktype) {
					case 0: ei[i].railtype = RAILTYPE_RAIL; break;
					case 1: ei[i].railtype = RAILTYPE_MONO; break;
					case 2: ei[i].railtype = RAILTYPE_MAGLEV; break;
					default:
						grfmsg(GMS_WARN, "RailVehicleChangeInfo: Invalid track type %d specified, ignoring.", tracktype);
						break;
				}
			}
			break;

		case 0x08: /* AI passenger service */
			/* TODO */
			FOR_EACH_OBJECT grf_load_byte(&buf);
			ret = true;
			break;

		case 0x09: /* Speed (1 unit is 1 kmh) */
			FOR_EACH_OBJECT {
				uint16 speed = grf_load_word(&buf);
				if (speed == 0xFFFF) speed = 0;

				rvi[i].max_speed = speed;
			}
			break;

		case 0x0B: /* Power */
			FOR_EACH_OBJECT {
				uint16 power = grf_load_word(&buf);

				if (rvi[i].flags & RVI_MULTIHEAD) power /= 2;

				rvi[i].power = power;
				dewagonize(power, engine + i);
			}
			break;

		case 0x0D: /* Running cost factor */
			FOR_EACH_OBJECT {
				uint8 runcostfact = grf_load_byte(&buf);

				if (rvi[i].flags & RVI_MULTIHEAD) runcostfact /= 2;

				rvi[i].running_cost_base = runcostfact;
			}
			break;

		case 0x0E: /* Running cost base */
			FOR_EACH_OBJECT {
				uint32 base = grf_load_dword(&buf);

				switch (base) {
					case 0x4C30: rvi[i].running_cost_class = 0; break;
					case 0x4C36: rvi[i].running_cost_class = 1; break;
					case 0x4C3C: rvi[i].running_cost_class = 2; break;
				}
			}
			break;

		case 0x12: /* Sprite ID */
			FOR_EACH_OBJECT {
				uint8 spriteid = grf_load_byte(&buf);

				/* TTD sprite IDs point to a location in a 16bit array, but we use it
				 * as an array index, so we need it to be half the original value. */
				if (spriteid < 0xFD) spriteid >>= 1;

				rvi[i].image_index = spriteid;
			}
			break;

		case 0x13: /* Dual-headed */
			FOR_EACH_OBJECT {
				uint8 dual = grf_load_byte(&buf);

				if (dual != 0) {
					if (!(rvi[i].flags & RVI_MULTIHEAD)) {
						// adjust power and running cost if needed
						rvi[i].power /= 2;
						rvi[i].running_cost_base /= 2;
					}
					rvi[i].flags |= RVI_MULTIHEAD;
				} else {
					if (rvi[i].flags & RVI_MULTIHEAD) {
						// adjust power and running cost if needed
						rvi[i].power *= 2;
						rvi[i].running_cost_base *= 2;
					}
					rvi[i].flags &= ~RVI_MULTIHEAD;
				}
			}
			break;

		case 0x14: /* Cargo capacity */
			FOR_EACH_OBJECT rvi[i].capacity = grf_load_byte(&buf);
			break;

		case 0x15: /* Cargo type */
			FOR_EACH_OBJECT {
				uint8 ctype = grf_load_byte(&buf);

				if (ctype < NUM_CARGO) {
					rvi[i].cargo_type = ctype;
				} else {
					grfmsg(GMS_NOTICE, "RailVehicleChangeInfo: Invalid cargo type %d, ignoring.", ctype);
				}
			}
			break;

		case 0x16: /* Weight */
			FOR_EACH_OBJECT SB(rvi[i].weight, 0, 8, grf_load_byte(&buf));
			break;

		case 0x17: /* Cost factor */
			FOR_EACH_OBJECT rvi[i].base_cost = grf_load_byte(&buf);
			break;

		case 0x18: /* AI rank */
			FOR_EACH_OBJECT rvi[i].ai_rank = grf_load_byte(&buf);
			break;

		case 0x19: /* Engine traction type */
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
					ei[i].railtype = RAILTYPE_ELECTRIC;
				} else if (traction <= 0x41) {
					engclass = 2;
				} else {
					break;
				}

				rvi[i].engclass = engclass;
			}
			break;

		case 0x1A: /* Alter purchase list sort order */
			FOR_EACH_OBJECT {
				EngineID pos = grf_load_byte(&buf);

				if (pos < NUM_TRAIN_ENGINES) {
					AlterRailVehListOrder(engine + i, pos);
				} else {
					grfmsg(GMS_NOTICE, "RailVehicleChangeInfo: Invalid train engine ID %d, ignoring.", pos);
				}
			}
			break;

		case 0x1B: /* Powered wagons power bonus */
			FOR_EACH_OBJECT rvi[i].pow_wag_power = grf_load_word(&buf);
			break;

		case 0x1C: /* Refit cost */
			FOR_EACH_OBJECT ei[i].refit_cost = grf_load_byte(&buf);
			break;

		case 0x1D: /* Refit cargo */
			FOR_EACH_OBJECT ei[i].refit_mask = grf_load_dword(&buf);
			break;

		case 0x1E: /* Callback */
			FOR_EACH_OBJECT ei[i].callbackmask = grf_load_byte(&buf);
			break;

		case 0x21: /* Shorter vehicle */
			FOR_EACH_OBJECT rvi[i].shorten_factor = grf_load_byte(&buf);
			break;

		case 0x22: /* Visual effect */
			// see note in engine.h about rvi->visual_effect
			FOR_EACH_OBJECT rvi[i].visual_effect = grf_load_byte(&buf);
			break;

		case 0x23: /* Powered wagons weight bonus */
			FOR_EACH_OBJECT rvi[i].pow_wag_weight = grf_load_byte(&buf);
			break;

		case 0x24: /* High byte of vehicle weight */
			FOR_EACH_OBJECT {
				byte weight = grf_load_byte(&buf);

				if (weight > 4) {
					grfmsg(GMS_NOTICE, "RailVehicleChangeInfo: Nonsensical weight of %d tons, ignoring.", weight << 8);
				} else {
					SB(rvi[i].weight, 8, 8, weight);
				}
			}
			break;

		case 0x25: /* User-defined bit mask to set when checking veh. var. 42 */
			FOR_EACH_OBJECT rvi[i].user_def_data = grf_load_byte(&buf);
			break;

		case 0x27: /* Miscellaneous flags */
			FOR_EACH_OBJECT {
				ei[i].misc_flags = grf_load_byte(&buf);
				if (HASBIT(ei[i].misc_flags, EF_USES_2CC)) _have_2cc = true;
			}
			break;

		case 0x28: /* Cargo classes allowed */
			FOR_EACH_OBJECT cargo_allowed[engine + i] = grf_load_word(&buf);
			break;

		case 0x29: /* Cargo classes disallowed */
			FOR_EACH_OBJECT cargo_disallowed[engine + i] = grf_load_word(&buf);
			break;

		/* TODO */
		/* Fall-through for unimplemented one byte long properties. */
		case 0x1F: /* Tractive effort */
		case 0x20: /* Air drag */
		case 0x26: /* Retire vehicle early */
			/* TODO */
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
		case 0x08: /* Speed (1 unit is 0.5 kmh) */
			FOR_EACH_OBJECT rvi[i].max_speed = grf_load_byte(&buf);
			break;

		case 0x09: /* Running cost factor */
			FOR_EACH_OBJECT rvi[i].running_cost = grf_load_byte(&buf);
			break;

		case 0x0A: /* Running cost base */
			/* TODO: I have no idea. --pasky */
			FOR_EACH_OBJECT grf_load_dword(&buf);
			ret = true;
			break;

		case 0x0E: /* Sprite ID */
			FOR_EACH_OBJECT {
				uint8 spriteid = grf_load_byte(&buf);

				// cars have different custom id in the GRF file
				if (spriteid == 0xFF) spriteid = 0xFD;

				if (spriteid < 0xFD) spriteid >>= 1;

				rvi[i].image_index = spriteid;
			}
			break;

		case 0x0F: /* Cargo capacity */
			FOR_EACH_OBJECT rvi[i].capacity = grf_load_byte(&buf);
			break;

		case 0x10: /* Cargo type */
			FOR_EACH_OBJECT {
				uint8 cargo = grf_load_byte(&buf);

				if (cargo < NUM_CARGO) {
					rvi[i].cargo_type = cargo;
				} else {
					grfmsg(GMS_NOTICE, "RoadVehicleChangeInfo: Invalid cargo type %d, ignoring.", cargo);
				}
			}
			break;

		case 0x11: /* Cost factor */
			FOR_EACH_OBJECT rvi[i].base_cost = grf_load_byte(&buf); // ?? is it base_cost?
			break;

		case 0x12: /* SFX */
			FOR_EACH_OBJECT rvi[i].sfx = grf_load_byte(&buf);
			break;

		case 0x13: /* Power in 10hp */
		case 0x14: /* Weight in 1/4 tons */
		case 0x15: /* Speed in mph*0.8 */
			/* TODO: Support for road vehicles realistic power
			 * computations (called rvpower in TTDPatch) is just
			 * missing in OTTD yet. --pasky */
			FOR_EACH_OBJECT grf_load_byte(&buf);
			ret = true;
			break;

		case 0x16: /* Cargos available for refitting */
			FOR_EACH_OBJECT ei[i].refit_mask = grf_load_dword(&buf);
			break;

		case 0x17: /* Callback mask */
			FOR_EACH_OBJECT ei[i].callbackmask = grf_load_byte(&buf);
			break;

		case 0x1A: /* Refit cost */
			FOR_EACH_OBJECT ei[i].refit_cost = grf_load_byte(&buf);
			break;

		case 0x1C: /* Miscellaneous flags */
			FOR_EACH_OBJECT {
				ei[i].misc_flags = grf_load_byte(&buf);
				if (HASBIT(ei[i].misc_flags, EF_USES_2CC)) _have_2cc = true;
			}
			break;

		case 0x1D: /* Cargo classes allowed */
			FOR_EACH_OBJECT cargo_allowed[ROAD_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x1E: /* Cargo classes disallowed */
			FOR_EACH_OBJECT cargo_disallowed[ROAD_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x18: /* Tractive effort */
		case 0x19: /* Air drag */
		case 0x1B: /* Retire vehicle early */
			/* TODO */
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
		case 0x08: /* Sprite ID */
			FOR_EACH_OBJECT {
				uint8 spriteid = grf_load_byte(&buf);

				// ships have different custom id in the GRF file
				if (spriteid == 0xFF) spriteid = 0xFD;

				if (spriteid < 0xFD) spriteid >>= 1;

				svi[i].image_index = spriteid;
			}
			break;

		case 0x09: /* Refittable */
			FOR_EACH_OBJECT svi[i].refittable = grf_load_byte(&buf);
			break;

		case 0x0A: /* Cost factor */
			FOR_EACH_OBJECT svi[i].base_cost = grf_load_byte(&buf); // ?? is it base_cost?
			break;

		case 0x0B: /* Speed (1 unit is 0.5 kmh) */
			FOR_EACH_OBJECT svi[i].max_speed = grf_load_byte(&buf);
			break;

		case 0x0C: /* Cargo type */
			FOR_EACH_OBJECT {
				uint8 cargo = grf_load_byte(&buf);

				// XXX: Need to consult this with patchman yet.
#if 0
				// Documentation claims this is already the
				// per-landscape cargo type id, but newships.grf
				// assume otherwise.
				cargo = local_cargo_id_ctype[cargo];
#endif
				if (cargo < NUM_CARGO) {
					svi[i].cargo_type = cargo;
				} else {
					grfmsg(GMS_NOTICE, "ShipVehicleChangeInfo: Invalid cargo type %d, ignoring.", cargo);
				}
			}
			break;

		case 0x0D: /* Cargo capacity */
			FOR_EACH_OBJECT svi[i].capacity = grf_load_word(&buf);
			break;

		case 0x0F: /* Running cost factor */
			FOR_EACH_OBJECT svi[i].running_cost = grf_load_byte(&buf);
			break;

		case 0x10: /* SFX */
			FOR_EACH_OBJECT svi[i].sfx = grf_load_byte(&buf);
			break;

		case 0x11: /* Cargos available for refitting */
			FOR_EACH_OBJECT ei[i].refit_mask = grf_load_dword(&buf);
			break;

		case 0x12: /* Callback mask */
			FOR_EACH_OBJECT ei[i].callbackmask = grf_load_byte(&buf);
			break;

		case 0x13: /* Refit cost */
			FOR_EACH_OBJECT ei[i].refit_cost = grf_load_byte(&buf);
			break;

		case 0x17: /* Miscellaneous flags */
			FOR_EACH_OBJECT {
				ei[i].misc_flags = grf_load_byte(&buf);
				if (HASBIT(ei[i].misc_flags, EF_USES_2CC)) _have_2cc = true;
			}
			break;

		case 0x18: /* Cargo classes allowed */
			FOR_EACH_OBJECT cargo_allowed[SHIP_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x19: /* Cargo classes disallowed */
			FOR_EACH_OBJECT cargo_disallowed[SHIP_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x14: /* Ocean speed fraction */
		case 0x15: /* Canal speed fraction */
		case 0x16: /* Retire vehicle early */
			/* TODO */
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
		case 0x08: /* Sprite ID */
			FOR_EACH_OBJECT {
				uint8 spriteid = grf_load_byte(&buf);

				// aircraft have different custom id in the GRF file
				if (spriteid == 0xFF) spriteid = 0xFD;

				if (spriteid < 0xFD) spriteid >>= 1;

				avi[i].image_index = spriteid;
			}
			break;

		case 0x09: /* Helicopter */
			FOR_EACH_OBJECT {
				if (grf_load_byte(&buf) == 0) {
					avi[i].subtype = 0;
				} else {
					SB(avi[i].subtype, 0, 1, 1);
				}
			}
			break;

		case 0x0A: /* Large */
			FOR_EACH_OBJECT SB(avi[i].subtype, 1, 1, (grf_load_byte(&buf) != 0 ? 1 : 0));
			break;

		case 0x0B: /* Cost factor */
			FOR_EACH_OBJECT avi[i].base_cost = grf_load_byte(&buf); // ?? is it base_cost?
			break;

		case 0x0C: /* Speed (1 unit is 8 mph) */
			FOR_EACH_OBJECT avi[i].max_speed = grf_load_byte(&buf);
			break;

		case 0x0D: /* Acceleration */
			FOR_EACH_OBJECT avi[i].acceleration = grf_load_byte(&buf);
			break;

		case 0x0E: /* Running cost factor */
			FOR_EACH_OBJECT avi[i].running_cost = grf_load_byte(&buf);
			break;

		case 0x0F: /* Passenger capacity */
			FOR_EACH_OBJECT avi[i].passenger_capacity = grf_load_word(&buf);
			break;

		case 0x11: /* Mail capacity */
			FOR_EACH_OBJECT avi[i].mail_capacity = grf_load_byte(&buf);
			break;

		case 0x12: /* SFX */
			FOR_EACH_OBJECT avi[i].sfx = grf_load_byte(&buf);
			break;

		case 0x13: /* Cargos available for refitting */
			FOR_EACH_OBJECT ei[i].refit_mask = grf_load_dword(&buf);
			break;

		case 0x14: /* Callback mask */
			FOR_EACH_OBJECT ei[i].callbackmask = grf_load_byte(&buf);
			break;

		case 0x15: /* Refit cost */
			FOR_EACH_OBJECT ei[i].refit_cost = grf_load_byte(&buf);
			break;

		case 0x17: /* Miscellaneous flags */
			FOR_EACH_OBJECT {
				ei[i].misc_flags = grf_load_byte(&buf);
				if (HASBIT(ei[i].misc_flags, EF_USES_2CC)) _have_2cc = true;
			}
			break;

		case 0x18: /* Cargo classes allowed */
			FOR_EACH_OBJECT cargo_allowed[AIRCRAFT_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x19: /* Cargo classes disallowed */
			FOR_EACH_OBJECT cargo_disallowed[AIRCRAFT_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x16: /* Retire vehicle early */
			/* TODO */
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
		grfmsg(GMS_WARN, "StationChangeInfo: Station %u is invalid, max %u, ignoring.", stid + numinfo, MAX_STATIONS);
		return false;
	}

	/* Allocate station specs if necessary */
	if (_cur_grffile->stations == NULL) _cur_grffile->stations = calloc(MAX_STATIONS, sizeof(*_cur_grffile->stations));

	statspec = &_cur_grffile->stations[stid];

	if (prop != 0x08) {
		/* Check that all stations we are modifying are defined. */
		FOR_EACH_OBJECT {
			if (statspec[i] == NULL) {
				grfmsg(GMS_NOTICE, "StationChangeInfo: Attempt to modify undefined station %u, ignoring.", stid + i);
				return false;
			}
		}
	}

	switch (prop) {
		case 0x08: /* Class ID */
			FOR_EACH_OBJECT {
				uint32 classid;

				/* Property 0x08 is special; it is where the station is allocated */
				if (statspec[i] == NULL) statspec[i] = calloc(1, sizeof(*statspec[i]));

				/* Swap classid because we read it in BE meaning WAYP or DFLT */
				classid = grf_load_dword(&buf);
				statspec[i]->sclass = AllocateStationClass(BSWAP32(classid));
			}
			break;

		case 0x09: /* Define sprite layout */
			FOR_EACH_OBJECT {
				StationSpec *statspec = _cur_grffile->stations[stid + i];
				uint t;

				statspec->tiles = grf_load_extended(&buf);
				statspec->renderdata = calloc(statspec->tiles, sizeof(*statspec->renderdata));
				statspec->copied_renderdata = false;

				for (t = 0; t < statspec->tiles; t++) {
					DrawTileSprites *dts = &statspec->renderdata[t];
					uint seq_count = 0;

					dts->seq = NULL;
					dts->ground_sprite = grf_load_dword(&buf);
					if (dts->ground_sprite == 0) continue;

					while (buf < *bufp + len) {
						DrawTileSeqStruct *dtss;

						// no relative bounding box support
						dts->seq = realloc((void*)dts->seq, ++seq_count * sizeof(DrawTileSeqStruct));
						dtss = (DrawTileSeqStruct*) &dts->seq[seq_count - 1];

						dtss->delta_x = grf_load_byte(&buf);
						if ((byte) dtss->delta_x == 0x80) break;
						dtss->delta_y = grf_load_byte(&buf);
						dtss->delta_z = grf_load_byte(&buf);
						dtss->size_x = grf_load_byte(&buf);
						dtss->size_y = grf_load_byte(&buf);
						dtss->size_z = grf_load_byte(&buf);
						dtss->image = grf_load_dword(&buf);

						/* Remap flags as ours collide */
						if (HASBIT(dtss->image, 31)) {
							CLRBIT(dtss->image, 31);
							SETBIT(dtss->image, 30);
						}
						if (HASBIT(dtss->image, 14)) {
							CLRBIT(dtss->image, 14);
							SETBIT(dtss->image, 31);
						}
					}
				}
			}
			break;

		case 0x0A: /* Copy sprite layout */
			FOR_EACH_OBJECT {
				StationSpec *statspec = _cur_grffile->stations[stid + i];
				byte srcid = grf_load_byte(&buf);
				const StationSpec *srcstatspec = _cur_grffile->stations[srcid];

				statspec->tiles = srcstatspec->tiles;
				statspec->renderdata = srcstatspec->renderdata;
				statspec->copied_renderdata = true;
			}
			break;

		case 0x0B: /* Callback mask */
			FOR_EACH_OBJECT statspec[i]->callbackmask = grf_load_byte(&buf);
			break;

		case 0x0C: /* Disallowed number of platforms */
			FOR_EACH_OBJECT statspec[i]->disallowed_platforms = grf_load_byte(&buf);
			break;

		case 0x0D: /* Disallowed platform lengths */
			FOR_EACH_OBJECT statspec[i]->disallowed_lengths = grf_load_byte(&buf);
			break;

		case 0x0E: /* Define custom layout */
			FOR_EACH_OBJECT {
				StationSpec *statspec = _cur_grffile->stations[stid + i];

				while (buf < *bufp + len) {
					byte length = grf_load_byte(&buf);
					byte number = grf_load_byte(&buf);
					StationLayout layout;
					uint l, p;

					if (length == 0 || number == 0) break;

					//debug("l %d > %d ?", length, stat->lengths);
					if (length > statspec->lengths) {
						statspec->platforms = realloc(statspec->platforms, length);
						memset(statspec->platforms + statspec->lengths, 0, length - statspec->lengths);

						statspec->layouts = realloc(statspec->layouts, length * sizeof(*statspec->layouts));
						memset(statspec->layouts + statspec->lengths, 0,
						       (length - statspec->lengths) * sizeof(*statspec->layouts));

						statspec->lengths = length;
					}
					l = length - 1; // index is zero-based

					//debug("p %d > %d ?", number, stat->platforms[l]);
					if (number > statspec->platforms[l]) {
						statspec->layouts[l] = realloc(statspec->layouts[l],
						                               number * sizeof(**statspec->layouts));
						// We expect NULL being 0 here, but C99 guarantees that.
						memset(statspec->layouts[l] + statspec->platforms[l], 0,
						       (number - statspec->platforms[l]) * sizeof(**statspec->layouts));

						statspec->platforms[l] = number;
					}

					p = 0;
					layout = malloc(length * number);
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

		case 0x0F: /* Copy custom layout */
			/* TODO */
			FOR_EACH_OBJECT {
				grf_load_byte(&buf);
			}
			ret = true;
			break;

		case 0x10: /* Little/lots cargo threshold */
			FOR_EACH_OBJECT statspec[i]->cargo_threshold = grf_load_word(&buf);
			break;

		case 0x11: /* Pylon placement */
			FOR_EACH_OBJECT statspec[i]->pylons = grf_load_byte(&buf);
			break;

		case 0x12: /* Cargo types for random triggers */
			FOR_EACH_OBJECT statspec[i]->cargo_triggers = grf_load_dword(&buf);
			break;

		case 0x13: /* General flags */
			FOR_EACH_OBJECT statspec[i]->flags = grf_load_byte(&buf);
			break;

		case 0x14: /* Overhead wire placement */
			FOR_EACH_OBJECT statspec[i]->wires = grf_load_byte(&buf);
			break;

		case 0x15: /* Blocked tiles */
			FOR_EACH_OBJECT statspec[i]->blocked = grf_load_byte(&buf);
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
		case 0x08: /* Year of availability */
			FOR_EACH_OBJECT _bridge[brid + i].avail_year = ORIGINAL_BASE_YEAR + grf_load_byte(&buf);
			break;

		case 0x09: /* Minimum length */
			FOR_EACH_OBJECT _bridge[brid + i].min_length = grf_load_byte(&buf);
			break;

		case 0x0A: /* Maximum length */
			FOR_EACH_OBJECT _bridge[brid + i].max_length = grf_load_byte(&buf);
			break;

		case 0x0B: /* Cost factor */
			FOR_EACH_OBJECT _bridge[brid + i].price = grf_load_byte(&buf);
			break;

		case 0x0C: /* Maximum speed */
			FOR_EACH_OBJECT _bridge[brid + i].speed = grf_load_word(&buf);
			break;

		case 0x0D: /* Bridge sprite tables */
			FOR_EACH_OBJECT {
				Bridge *bridge = &_bridge[brid + i];
				byte tableid = grf_load_byte(&buf);
				byte numtables = grf_load_byte(&buf);

				if (bridge->sprite_table == NULL) {
					/* Allocate memory for sprite table pointers and zero out */
					bridge->sprite_table = calloc(7, sizeof(*bridge->sprite_table));
				}

				for (; numtables-- != 0; tableid++) {
					byte sprite;

					if (tableid >= 7) { // skip invalid data
						grfmsg(GMS_WARN, "BridgeChangeInfo: Table %d >= 7, skipping.", tableid);
						for (sprite = 0; sprite < 32; sprite++) grf_load_dword(&buf);
						continue;
					}

					if (bridge->sprite_table[tableid] == NULL) {
						bridge->sprite_table[tableid] = malloc(32 * sizeof(**bridge->sprite_table));
					}

					for (sprite = 0; sprite < 32; sprite++)
						bridge->sprite_table[tableid][sprite] = grf_load_dword(&buf);
				}
			}
			break;

		case 0x0E: /* Flags; bit 0 - disable far pillars */
			FOR_EACH_OBJECT _bridge[brid + i].flags = grf_load_byte(&buf);
			break;

		default:
			ret = true;
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
					grfmsg(GMS_WARN, "GlobalVarChangeInfo: Price %d out of range, ignoring.", price);
				}
			}
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
					grfmsg(GMS_WARN, "GlobalVarChangeInfo: Currency multipliers %d out of range, ignoring.", curidx);
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
					grfmsg(GMS_WARN, "GlobalVarChangeInfo: Currency option %d out of range, ignoring.", curidx);
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
					grfmsg(GMS_WARN, "GlobalVarChangeInfo: Currency symbol %d out of range, ignoring.", curidx);
				}
			}
			break;

		case 0x0E: // Currency suffix symbol
			FOR_EACH_OBJECT {
				uint curidx = GetNewgrfCurrencyIdConverted(gvid + i);
				uint32 tempfix = grf_load_dword(&buf);

				if (curidx < NUM_CURRENCY) {
					memcpy(&_currency_specs[curidx].suffix,&tempfix,4);
					_currency_specs[curidx].suffix[4] = 0;
				} else {
					grfmsg(GMS_WARN, "GlobalVarChangeInfo: Currency symbol %d out of range, ignoring.", curidx);
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
					grfmsg(GMS_WARN, "GlobalVarChangeInfo: Euro intro date %d out of range, ignoring.", curidx);
				}
			}
			break;

		case 0x09: // Cargo translation table
		case 0x10: // 12 * 32 * B Snow line height table
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
		grfmsg(GMS_WARN, "SoundEffectChangeInfo: No effects defined, skipping.");
		return false;
	}

	switch (prop) {
		case 0x08: /* Relative volume */
			FOR_EACH_OBJECT {
				uint sound = sid + i + _cur_grffile->sound_offset - GetNumOriginalSounds();

				if (sound >= GetNumSounds()) {
					grfmsg(GMS_WARN, "SoundEffectChangeInfo: Sound %d not defined (max %d)", sound, GetNumSounds());
				} else {
					GetSound(sound)->volume = grf_load_byte(&buf);
				}
			}
			break;

		case 0x09: /* Priority */
			FOR_EACH_OBJECT {
				uint sound = sid + i + _cur_grffile->sound_offset - GetNumOriginalSounds();

				if (sound >= GetNumSounds()) {
					grfmsg(GMS_WARN, "SoundEffectChangeInfo: Sound %d not defined (max %d)", sound, GetNumSounds());
				} else {
					GetSound(sound)->priority = grf_load_byte(&buf);
				}
			}
			break;

		case 0x0A: /* Override old sound */
			FOR_EACH_OBJECT {
				uint sound = sid + i + _cur_grffile->sound_offset - GetNumOriginalSounds();
				uint orig_sound = grf_load_byte(&buf);

				if (sound >= GetNumSounds() || orig_sound >= GetNumSounds()) {
					grfmsg(GMS_WARN, "SoundEffectChangeInfo: Sound %d or %d not defined (max %d)", sound, orig_sound, GetNumSounds());
				} else {
					FileEntry *newfe = GetSound(sound);
					FileEntry *oldfe = GetSound(orig_sound);

					/* Literally copy the data of the new sound over the original */
					memcpy(oldfe, newfe, sizeof(*oldfe));
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
	int i;

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
		/* GSF_TOWNHOUSE */    NULL,
		/* GSF_GLOBALVAR */    GlobalVarChangeInfo,
		/* GSF_INDUSTRYTILES */NULL,
		/* GSF_INDUSTRIES */   NULL,
		/* GSF_CARGOS */       NULL,
		/* GSF_SOUNDFX */      SoundEffectChangeInfo,
	};

	uint8 feature;
	uint8 numprops;
	uint8 numinfo;
	byte engine;
	EngineInfo *ei = NULL;

	if (len == 1) {
		DEBUG(grf, 8) ("Silently ignoring one-byte special sprite 0x00.");
		return;
	}

	check_length(len, 6, "FeatureChangeInfo");
	buf++;
	feature  = grf_load_byte(&buf);
	numprops = grf_load_byte(&buf);
	numinfo  = grf_load_byte(&buf);
	engine   = grf_load_byte(&buf);

	DEBUG(grf, 6) ("FeatureChangeInfo: Feature %d, %d properties, to apply to %d+%d",
	               feature, numprops, engine, numinfo);

	if (feature >= lengthof(handler) || handler[feature] == NULL) {
		grfmsg(GMS_WARN, "FeatureChangeInfo: Unsupported feature %d, skipping.", feature);
		return;
	}

	if (feature <= GSF_AIRCRAFT) {
		if (engine + numinfo > _vehcounts[feature]) {
			grfmsg(GMS_ERROR, "FeatureChangeInfo: Last engine ID %d out of bounds (max %d), skipping.", engine + numinfo, _vehcounts[feature]);
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
					case 0x00: /* Introduction date */
						FOR_EACH_OBJECT ei[i].base_intro = grf_load_word(&buf) + DAYS_TILL_ORIGINAL_BASE_YEAR;
						break;

					case 0x02: /* Decay speed */
						FOR_EACH_OBJECT SB(ei[i].unk2, 0, 7, grf_load_byte(&buf) & 0x7F);
						break;

					case 0x03: /* Vehicle life */
						FOR_EACH_OBJECT ei[i].lifelength = grf_load_byte(&buf);
						break;

					case 0x04: /* Model life */
						FOR_EACH_OBJECT ei[i].base_life = grf_load_byte(&buf);
						break;

					case 0x06: /* Climates available */
						FOR_EACH_OBJECT ei[i].climates = grf_load_byte(&buf);
						break;

					case 0x07: /* Loading speed */
						/* TODO */
						/* Hyronymus explained me what does
						 * this mean and insists on having a
						 * credit ;-). --pasky */
						/* TODO: This needs to be supported by
						 * LoadUnloadVehicle() first. */
						FOR_EACH_OBJECT grf_load_byte(&buf);
						ignoring = true;
						break;

					default:
						if (handler[feature](engine, numinfo, prop, &buf, bufend - buf))
							ignoring = true;
						break;
				}
				break;

			default:
				if (handler[feature](engine, numinfo, prop, &buf, bufend - buf))
					ignoring = true;
				break;
		}

		if (ignoring)
			grfmsg(GMS_NOTICE, "FeatureChangeInfo: Ignoring property 0x%02X (not implemented).", prop);
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

	// Old style callback results have the highest byte 0xFF so signify it is a callback result
	// New style ones only have the highest bit set (allows 15-bit results, instead of just 8)
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
 * @param value The sprite number.
 * @param sprites The number of sprites per set.
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
	 *                 For stations, must be 12 (hex) for the eighteen
	 *                         different sprites that make up a station */
	/* TODO: No stations support. */
	uint8 feature;
	uint num_sets;
	uint num_ents;
	uint i;

	check_length(len, 4, "NewSpriteSet");
	buf++;
	feature  = grf_load_byte(&buf);
	num_sets = grf_load_byte(&buf);
	num_ents = grf_load_extended(&buf);

	_cur_grffile->spriteset_start = _cur_spriteid;
	_cur_grffile->spriteset_feature = feature;
	_cur_grffile->spriteset_numsets = num_sets;
	_cur_grffile->spriteset_numents = num_ents;

	DEBUG(grf, 7) (
		"New sprite set at %d of type %d, "
		"consisting of %d sets with %d views each (total %d)",
		_cur_spriteid, feature, num_sets, num_ents, num_sets * num_ents
	);

	for (i = 0; i < num_sets * num_ents; i++) {
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
		grfmsg(GMS_WARN, "NewSpriteGroup(0x%02X:0x%02X): Groupid 0x%04X does not exist, leaving empty.", setid, type, groupid);
		return NULL;
	}

	return _cur_grffile->spritegroups[groupid];
}

/* Helper function to either create a callback or a result sprite group. */
static const SpriteGroup* CreateGroupFromGroupID(byte feature, byte setid, byte type, uint16 spriteid, uint16 num_sprites)
{
	if (HASBIT(spriteid, 15)) return NewCallBackResultSpriteGroup(spriteid);

	if (spriteid >= _cur_grffile->spriteset_numsets) {
		grfmsg(GMS_WARN, "NewSpriteGroup(0x%02X:0x%02X): Sprite set %u invalid, max %u", setid, type, spriteid, _cur_grffile->spriteset_numsets);
		return NULL;
	}

	/* Check if the sprite is within range. This can fail if the Action 0x01
	 * is skipped, as TTDPatch mandates that Action 0x02s must be processed.
	 * We don't have that rule, but must live by the Patch... */
	if (_cur_grffile->spriteset_start + spriteid * num_sprites + num_sprites > _cur_spriteid) {
		grfmsg(GMS_WARN, "NewSpriteGroup(0x%02X:0x%02X): Real Sprite IDs 0x%04X - 0x%04X do not (all) exist (max 0x%04X), leaving empty.",
				setid, type,
				_cur_grffile->spriteset_start + spriteid * num_sprites,
				_cur_grffile->spriteset_start + spriteid * num_sprites + num_sprites - 1, _cur_spriteid - 1);
		return NULL;
	}

	if (feature != _cur_grffile->spriteset_feature) {
		grfmsg(GMS_WARN, "NewSpriteGroup(0x%02X:0x%02X): Sprite set feature 0x%02X does not match action feature 0x%02X, skipping.",
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
	uint8 feature;
	uint8 setid;
	uint8 type;
	SpriteGroup *group = NULL;
	byte *bufend = buf + len;

	check_length(len, 5, "NewSpriteGroup");
	buf++;

	feature = grf_load_byte(&buf);
	setid   = grf_load_byte(&buf);
	type    = grf_load_byte(&buf);

	if (setid >= _cur_grffile->spritegroups_count) {
		// Allocate memory for new sprite group references.
		_cur_grffile->spritegroups = realloc(_cur_grffile->spritegroups, (setid + 1) * sizeof(*_cur_grffile->spritegroups));
		// Initialise new space to NULL
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
			uint i;

			/* Check we can load the var size parameter */
			check_length(bufend - buf, 1, "NewSpriteGroup (Deterministic) (1)");

			group = AllocateSpriteGroup();
			group->type = SGT_DETERMINISTIC;
			group->g.determ.var_scope = HASBIT(type, 1) ? VSG_SCOPE_PARENT : VSG_SCOPE_SELF;

			switch (GB(type, 2, 2)) {
				default: NOT_REACHED();
				case 0: group->g.determ.size = DSG_SIZE_BYTE;  varsize = 1; break;
				case 1: group->g.determ.size = DSG_SIZE_WORD;  varsize = 2; break;
				case 2: group->g.determ.size = DSG_SIZE_DWORD; varsize = 4; break;
			}

			check_length(bufend - buf, 2 + (varsize * 3) + 2, "NewSpriteGroup (Deterministic) (2)");

			/* Loop through the var adjusts. Unfortunately we don't know how many we have
			 * from the outset, so we shall have to keep reallocing. */
			do {
				DeterministicSpriteGroupAdjust *adjust;

				if (group->g.determ.num_adjusts > 0) {
					check_length(bufend - buf, 2 + varsize + 3, "NewSpriteGroup (Deterministic) (3)");
				}

				group->g.determ.num_adjusts++;
				group->g.determ.adjusts = realloc(group->g.determ.adjusts, group->g.determ.num_adjusts * sizeof(*group->g.determ.adjusts));

				adjust = &group->g.determ.adjusts[group->g.determ.num_adjusts - 1];

				/* The first var adjust doesn't have an operation specified, so we set it to add. */
				adjust->operation = group->g.determ.num_adjusts == 1 ? DSGA_OP_ADD : grf_load_byte(&buf);
				adjust->variable  = grf_load_byte(&buf);
				adjust->parameter = IS_BYTE_INSIDE(adjust->variable, 0x60, 0x80) ? grf_load_byte(&buf) : 0;

				varadjust = grf_load_byte(&buf);
				adjust->shift_num = GB(varadjust, 0, 5);
				adjust->type      = GB(varadjust, 6, 2);
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
			group->g.determ.ranges = calloc(group->g.determ.num_ranges, sizeof(*group->g.determ.ranges));

			check_length(bufend - buf, 2 + (2 + 2 * varsize) * group->g.determ.num_ranges, "NewSpriteGroup (Deterministic)");

			for (i = 0; i < group->g.determ.num_ranges; i++) {
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
			byte triggers;
			uint i;

			check_length(bufend - buf, 7, "NewSpriteGroup (Randomized) (1)");

			group = AllocateSpriteGroup();
			group->type = SGT_RANDOMIZED;
			group->g.random.var_scope = HASBIT(type, 1) ? VSG_SCOPE_PARENT : VSG_SCOPE_SELF;

			triggers = grf_load_byte(&buf);
			group->g.random.triggers       = GB(triggers, 0, 7);
			group->g.random.cmp_mode       = HASBIT(triggers, 7) ? RSG_CMP_ALL : RSG_CMP_ANY;
			group->g.random.lowest_randbit = grf_load_byte(&buf);
			group->g.random.num_groups     = grf_load_byte(&buf);
			group->g.random.groups = calloc(group->g.random.num_groups, sizeof(*group->g.random.groups));

			check_length(bufend - buf, 2 * group->g.random.num_groups, "NewSpriteGroup (Randomized) (2)");

			for (i = 0; i < group->g.random.num_groups; i++) {
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
				{
					byte sprites     = _cur_grffile->spriteset_numents;
					byte num_loaded  = type;
					byte num_loading = grf_load_byte(&buf);
					uint i;

					if (_cur_grffile->spriteset_start == 0) {
						grfmsg(GMS_ERROR, "NewSpriteGroup: No sprite set to work on! Skipping.");
						return;
					}

					check_length(bufend - buf, 2 * num_loaded + 2 * num_loading, "NewSpriteGroup (Real) (1)");

					group = AllocateSpriteGroup();
					group->type = SGT_REAL;

					group->g.real.num_loaded  = num_loaded;
					group->g.real.num_loading = num_loading;
					if (num_loaded  > 0) group->g.real.loaded  = calloc(num_loaded,  sizeof(*group->g.real.loaded));
					if (num_loading > 0) group->g.real.loading = calloc(num_loading, sizeof(*group->g.real.loading));

					DEBUG(grf, 6) ("NewSpriteGroup: New SpriteGroup 0x%02X, %u views, %u loaded, %u loading",
							setid, sprites, num_loaded, num_loading);

					for (i = 0; i < num_loaded; i++) {
						uint16 spriteid = grf_load_word(&buf);
						group->g.real.loaded[i] = CreateGroupFromGroupID(feature, setid, type, spriteid, sprites);
						DEBUG(grf, 8) ("NewSpriteGroup: + rg->loaded[%i]  = subset %u", i, spriteid);
					}

					for (i = 0; i < num_loading; i++) {
						uint16 spriteid = grf_load_word(&buf);
						group->g.real.loading[i] = CreateGroupFromGroupID(feature, setid, type, spriteid, sprites);
						DEBUG(grf, 8) ("NewSpriteGroup: + rg->loading[%i] = subset %u", i, spriteid);
					}

					break;
				}

				/* Loading of Tile Layout and Production Callback groups would happen here */
				default:
					grfmsg(GMS_WARN, "NewSpriteGroup: Unsupported feature %d, skipping.", feature);
			}
		}
	}

	_cur_grffile->spritegroups[setid] = group;
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
	/* TODO: Bridges, town houses. */
	/* TODO: Multiple cargo support could be useful even for trains/cars -
	 * cargo id 0xff is used for showing images in the build train list. */

	static byte *last_engines;
	static int last_engines_count;
	uint8 feature;
	uint8 idcount;
	bool wagover;
	uint8 cidcount;
	int c, i;

	check_length(len, 6, "FeatureMapSpriteGroup");
	feature = buf[1];
	idcount = buf[2] & 0x7F;
	wagover = (buf[2] & 0x80) == 0x80;
	check_length(len, 3 + idcount, "FeatureMapSpriteGroup");

	/* If ``n-id'' (or ``idcount'') is zero, this is a ``feature
	 * callback''. */
	if (idcount == 0) {
		grfmsg(GMS_NOTICE, "FeatureMapSpriteGroup: Feature callbacks not implemented yet.");
		return;
	}

	cidcount = buf[3 + idcount];
	check_length(len, 4 + idcount + cidcount * 3, "FeatureMapSpriteGroup");

	DEBUG(grf, 6) ("FeatureMapSpriteGroup: Feature %d, %d ids, %d cids, wagon override %d.",
			feature, idcount, cidcount, wagover);

	if (feature > GSF_STATION) {
		grfmsg(GMS_WARN, "FeatureMapSpriteGroup: Unsupported feature %d, skipping.", feature);
		return;
	}


	if (feature == GSF_STATION) {
		// We do things differently for stations.

		for (i = 0; i < idcount; i++) {
			uint8 stid = buf[3 + i];
			StationSpec *statspec = _cur_grffile->stations[stid];
			byte *bp = &buf[4 + idcount];

			for (c = 0; c < cidcount; c++) {
				uint8 ctype = grf_load_byte(&bp);
				uint16 groupid = grf_load_word(&bp);

				if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
					grfmsg(GMS_WARN, "FeatureMapSpriteGroup: Spriteset 0x%04X out of range 0x%X or empty, skipping.",
					       groupid, _cur_grffile->spritegroups_count);
					return;
				}

				if (ctype == 0xFE) ctype = GC_DEFAULT_NA;
				if (ctype == 0xFF) ctype = GC_PURCHASE;

				statspec->spritegroup[ctype] = _cur_grffile->spritegroups[groupid];
			}
		}

		{
			byte *bp = buf + 4 + idcount + cidcount * 3;
			uint16 groupid = grf_load_word(&bp);

			if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
				grfmsg(GMS_WARN, "FeatureMapSpriteGroup: Spriteset 0x%04X out of range 0x%X or empty, skipping.",
				       groupid, _cur_grffile->spritegroups_count);
				return;
			}

			for (i = 0; i < idcount; i++) {
				uint8 stid = buf[3 + i];
				StationSpec *statspec = _cur_grffile->stations[stid];

				statspec->spritegroup[GC_DEFAULT] = _cur_grffile->spritegroups[groupid];
				statspec->grfid = _cur_grffile->grfid;
				statspec->localidx = stid;
				SetCustomStationSpec(statspec);
			}
		}
		return;
	}

	// FIXME: Tropicset contains things like:
	// 03 00 01 19 01 00 00 00 00 - this is missing one 00 at the end,
	// what should we exactly do with that? --pasky

	if (_cur_grffile->spriteset_start == 0 || _cur_grffile->spritegroups == 0) {
		grfmsg(GMS_WARN, "FeatureMapSpriteGroup: No sprite set to work on! Skipping.");
		return;
	}

	if (!wagover && last_engines_count != idcount) {
		last_engines = realloc(last_engines, idcount);
		last_engines_count = idcount;
	}

	if (wagover) {
		if (last_engines_count == 0) {
			grfmsg(GMS_ERROR, "FeatureMapSpriteGroup: WagonOverride: No engine to do override with.");
			return;
		}
		DEBUG(grf, 6) ("FeatureMapSpriteGroup: WagonOverride: %u engines, %u wagons.",
				last_engines_count, idcount);
	}


	for (i = 0; i < idcount; i++) {
		uint8 engine_id = buf[3 + i];
		uint8 engine = engine_id + _vehshifts[feature];
		byte *bp = &buf[4 + idcount];

		if (engine_id > _vehcounts[feature]) {
			grfmsg(GMS_ERROR, "Id %u for feature 0x%02X is out of bounds.", engine_id, feature);
			return;
		}

		DEBUG(grf, 7) ("FeatureMapSpriteGroup: [%d] Engine %d...", i, engine);

		for (c = 0; c < cidcount; c++) {
			uint8 ctype = grf_load_byte(&bp);
			uint16 groupid = grf_load_word(&bp);

			DEBUG(grf, 8) ("FeatureMapSpriteGroup: * [%d] Cargo type 0x%X, group id 0x%02X", c, ctype, groupid);

			if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
				grfmsg(GMS_WARN, "FeatureMapSpriteGroup: Spriteset 0x%04X out of range 0x%X or empty, skipping.", groupid, _cur_grffile->spritegroups_count);
				return;
			}

			if (ctype == GC_INVALID) ctype = GC_PURCHASE;

			if (wagover) {
				SetWagonOverrideSprites(engine, ctype, _cur_grffile->spritegroups[groupid], last_engines, last_engines_count);
			} else {
				SetCustomEngineSprites(engine, ctype, _cur_grffile->spritegroups[groupid]);
				last_engines[i] = engine;
			}
		}
	}

	{
		byte *bp = buf + 4 + idcount + cidcount * 3;
		uint16 groupid = grf_load_word(&bp);

		DEBUG(grf, 8) ("-- Default group id 0x%04X", groupid);

		for (i = 0; i < idcount; i++) {
			uint8 engine = buf[3 + i] + _vehshifts[feature];

			// Don't tell me you don't love duplicated code!
			if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
				grfmsg(GMS_WARN, "FeatureMapSpriteGroup: Spriteset 0x%04X out of range 0x%X or empty, skipping.", groupid, _cur_grffile->spritegroups_count);
				return;
			}

			if (wagover) {
				/* If the ID for this action 3 is the same as the vehicle ID,
				 * this indicates we have a helicopter rotor override. */
				if (feature == GSF_AIRCRAFT && engine == last_engines[i]) {
					SetRotorOverrideSprites(engine, _cur_grffile->spritegroups[groupid]);
				} else {
					// TODO: No multiple cargo types per vehicle yet. --pasky
					SetWagonOverrideSprites(engine, GC_DEFAULT, _cur_grffile->spritegroups[groupid], last_engines, last_engines_count);
				}
			} else {
				SetCustomEngineSprites(engine, GC_DEFAULT, _cur_grffile->spritegroups[groupid]);
				SetEngineGRF(engine, _cur_grffile);
				last_engines[i] = engine;
			}
		}
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
	/* TODO: No support for changing non-vehicle text. Perhaps we shouldn't
	 * implement it at all, but it could be useful for some "modpacks"
	 * (completely new scenarios changing all graphics and logically also
	 * factory names etc). We should then also support all languages (by
	 * name), not only the original four ones. --pasky
	 * All of the above are coming.  In Time.  Some sooner than others :)*/

	uint8 feature;
	uint8 lang;
	uint8 num;
	uint16 id;
	uint16 endid;
	const char* name;
	bool new_scheme = _cur_grffile->grf_version >= 7;

	check_length(len, 6, "FeatureNewName");
	buf++;
	feature  = grf_load_byte(&buf);
	lang     = grf_load_byte(&buf);
	num      = grf_load_byte(&buf);
	id       = (lang & 0x80) ? grf_load_word(&buf) : grf_load_byte(&buf);

	if (feature <= GSF_AIRCRAFT && id < _vehcounts[feature]) {
		id += _vehshifts[feature];
	}
	endid    = id + num;

	DEBUG(grf, 6) ("FeatureNewName: About to rename engines %d..%d (feature %d) in language 0x%02X.",
	               id, endid, feature, lang);

	name = (const char*)buf; /*transfer read value*/
	len -= (lang & 0x80) ? 6 : 5;
	for (; id < endid && len > 0; id++) {
		size_t ofs = strlen(name) + 1;

		if (ofs < 128) {
			DEBUG(grf, 8) ("FeatureNewName: %d <- %s", id, name);

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

				default:
					switch (GB(id, 8, 8)) {
						case 0xC4: /* Station class name */
							if (_cur_grffile->stations == NULL || _cur_grffile->stations[GB(id, 0, 8)] == NULL) {
								grfmsg(GMS_WARN, "FeatureNewName: Attempt to name undefined station 0x%X, ignoring.", GB(id, 0, 8));
							} else {
								StationClassID sclass = _cur_grffile->stations[GB(id, 0, 8)]->sclass;
								SetStationClassName(sclass, AddGRFString(_cur_grffile->grfid, id, lang, new_scheme, name, STR_UNDEFINED));
							}
							break;

						case 0xC5: /* Station name */
							if (_cur_grffile->stations == NULL || _cur_grffile->stations[GB(id, 0, 8)] == NULL) {
								grfmsg(GMS_WARN, "FeatureNewName: Attempt to name undefined station 0x%X, ignoring.", GB(id, 0, 8));
							} else {
								_cur_grffile->stations[GB(id, 0, 8)]->name = AddGRFString(_cur_grffile->grfid, id, lang, new_scheme, name, STR_UNDEFINED);
							}
							break;

						case 0xC9:
						case 0xD0:
						case 0xDC:
							AddGRFString(_cur_grffile->grfid, id, lang, new_scheme, name, STR_UNDEFINED);
							break;

						default:
							DEBUG(grf, 7) ("FeatureNewName: Unsupported ID (0x%04X)", id);
							break;
					}
					break;

#if 0
				case GSF_CANAL :
				case GSF_BRIDGE :
				case GSF_TOWNHOUSE :
					AddGRFString(_cur_spriteid, id, lang, name);
					switch (GB(id, 8,8)) {
						case 0xC9: /* House name */
						default:
							DEBUG(grf, 7) ("FeatureNewName: Unsupported ID (0x%04X)", id);
					}
					break;

				case GSF_INDUSTRIES :
				case 0x48 :   /* for generic strings */
					AddGRFString(_cur_spriteid, id, lang, name);
					break;
				default :
					DEBUG(grf,7) ("FeatureNewName: Unsupported feature (0x%02X)", feature);
					break;
#endif
			}
		} else {
			/* ofs is the string length + 1, so if the string is empty, ofs
			 * is 1 */
			if (ofs == 1) {
				DEBUG(grf, 7) ("FeatureNewName: Can't add empty name");
			} else {
				DEBUG(grf, 7) ("FeatureNewName: Too long a name (%d)", ofs);
			}
		}
		name += ofs;
		len -= (int)ofs;
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

	uint8 type;
	uint16 num;
	SpriteID replace = 0;

	check_length(len, 2, "GraphicsNew");
	buf++;
	type = grf_load_byte(&buf);
	num  = grf_load_extended(&buf);

	switch (type) {
		case 0x04: /* Signal graphics */
			if (num != 112 && num != 240) {
				grfmsg(GMS_WARN, "GraphicsNews: Signal graphics sprite count must be 112 or 240, skipping.");
				return;
			}
			_signal_base = _cur_spriteid;
			break;

		case 0x05: /* Catenary graphics */
			if (num != 48) {
				grfmsg(GMS_WARN, "GraphicsNews: Catenary graphics sprite count must be 48, skipping.");
				return;
			}
			replace = SPR_ELRAIL_BASE + 3;
			break;

		case 0x06: /* Foundations */
			if (num != 74) {
				grfmsg(GMS_WARN, "GraphicsNews: Foundation graphics sprite count must be 74, skipping.");
				return;
			}
			replace = SPR_SLOPES_BASE;
			break;

		case 0x08: /* Canal graphics */
			if (num != 65) {
				grfmsg(GMS_WARN, "GraphicsNews: Canal graphics sprite count must be 65, skipping.");
				return;
			}
			replace = SPR_CANALS_BASE + 5;
			break;

		default:
			grfmsg(GMS_NOTICE, "GraphicsNew: Custom graphics (type 0x%02X) sprite block of length %u (unimplemented, ignoring).\n",
					type, num);
			return;
	}

	if (replace == 0) {
		grfmsg(GMS_NOTICE, "GraphicsNew: Loading %u sprites of type 0x%02X at SpriteID 0x%04X", num, type, _cur_spriteid);
	} else {
		grfmsg(GMS_NOTICE, "GraphicsNew: Replacing %u sprites of type 0x%02X at SpriteID 0x%04X", num, type, replace);
	}

	for (; num > 0; num--) {
		LoadNextSprite(replace == 0 ? _cur_spriteid++ : replace++, _file_index);
		_nfo_line++;
	}
}

static uint32 GetParamVal(byte param, uint32 *cond_val)
{
	switch (param) {
		case 0x83: /* current climate, 0=temp, 1=arctic, 2=trop, 3=toyland */
			return _opt.landscape;

		case 0x84: /* .grf loading stage, 0=initialization, 1=activation */
			return _cur_stage;

		case 0x85: /* TTDPatch flags, only for bit tests */
			if (cond_val == NULL) {
				/* Supported in Action 0x07 and 0x09, not 0x0D */
				return 0;
			} else {
				uint32 param_val = _ttdpatch_flags[*cond_val / 0x20];
				*cond_val %= 0x20;
				return param_val;
			}

		case 0x86: /* road traffic side, bit 4 clear=left, set=right */
			return _opt.road_side << 4;

		case 0x88: /* GRF ID check */
			return 0;

		case 0x8B: { /* TTDPatch version */
			uint major    = 2;
			uint minor    = 0;
			uint revision = 10; // special case: 2.0.1 is 2.0.10
			uint build    = 73;
			return (major << 24) | (minor << 20) | (revision << 16) | (build * 10);
		}

		case 0x8D: /* TTD Version, 00=DOS, 01=Windows */
			return !_use_dos_palette;

		case 0x8E: /* Y-offset for train sprites */
			return _traininfo_vehicle_pitch;

		case 0x92: /* Game mode */
			return _game_mode;

		case 0x9D: /* TTD Platform, 00=TTDPatch, 01=OpenTTD */
			return 1;

		case 0x9E: /* Miscellaneous GRF features */
			return _misc_grf_features;

		default:
			/* GRF Parameter */
			if (param < 0x80) return _cur_grffile->param[param];

			/* In-game variable. */
			grfmsg(GMS_WARN, "Unsupported in-game variable 0x%02X.", param);
			return -1;
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
		_preload_sprite = malloc(num);
		FioReadBlock(_preload_sprite, num);
	}

	/* Reset the file position to the start of the next sprite */
	FioSeekTo(pos, SEEK_SET);

	if (type != 0xFF) {
		grfmsg(GMS_NOTICE, "CfgApply: Ignoring (next sprite is real, unsupported)");
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
			grfmsg(GMS_NOTICE, "CfgApply: Ignoring (param %d not set)", (param_num + (param_size - 1) / 4));
			break;
		}

		DEBUG(grf, 8) ("CfgApply: Applying %u bytes from parameter 0x%02X at offset 0x%04X", param_size, param_num, offset);

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
	uint8 param;
	uint8 paramsize;
	uint8 condtype;
	uint8 numsprites;
	uint32 param_val = 0;
	uint32 cond_val = 0;
	uint32 mask = 0;
	bool result;
	GRFLabel *label;
	GRFLabel *choice = NULL;

	check_length(len, 6, "SkipIf");
	buf++;
	param     = grf_load_byte(&buf);
	paramsize = grf_load_byte(&buf);
	condtype  = grf_load_byte(&buf);

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
		DEBUG(grf, 7) ("Param %d undefined, skipping test", param);
		return;
	}

	if (param == 0x88 && GetFileByGRFID(cond_val) == NULL) {
		DEBUG(grf, 7) ("GRFID 0x%08X unknown, skipping test", BSWAP32(cond_val));
		return;
	}

	param_val = GetParamVal(param, &cond_val);

	/* Apply parameter mask, only for GRF parameters. */
	if (param < 0x80) param_val &= mask;

	DEBUG(grf, 7) ("Test condtype %d, param 0x%08X, condval 0x%08X", condtype, param_val, cond_val);
	switch (condtype) {
		case 0: result = !!(param_val & (1 << cond_val));
			break;
		case 1: result = !(param_val & (1 << cond_val));
			break;
		/* TODO: For the following, make it to work with paramsize>1. */
		case 2: result = (param_val == cond_val);
			break;
		case 3: result = (param_val != cond_val);
			break;
		case 4: result = (param_val < cond_val);
			break;
		case 5: result = (param_val > cond_val);
			break;
		/* Tests 6 to 10 are only for param 0x88, GRFID checks */
		case 6: /* Is GRFID active? */
		case 9: /* GRFID is or will be active? */
			result = (GetFileByGRFID(cond_val)->flags & 1) == 1;
			break;
		case 7: /* Is GRFID non-active? */
		case 10: /* GRFID is not nor will be active */
			result = (GetFileByGRFID(cond_val)->flags & 1) == 0;
			break;
		case 8: /* GRFID is not but will be active? */
			result = 0;
			if ((GetFileByGRFID(cond_val)->flags & 1) == 1) {
				const GRFFile *file;
				for (file = _first_grffile; file != NULL; file = file->next) {
					if (file->grfid == cond_val) break;
					if (file == _cur_grffile) {
						result = 1;
						break;
					}
				}
			}
			break;
		default:
			grfmsg(GMS_WARN, "Unsupported test %d. Ignoring.", condtype);
			return;
	}

	if (!result) {
		grfmsg(GMS_NOTICE, "Not skipping sprites, test was false.");
		return;
	}

	numsprites = grf_load_byte(&buf);

	/* numsprites can be a GOTO label if it has been defined in the GRF
	 * file. The jump will always be the first matching label that follows
	 * the current nfo_line. If no matching label is found, the first matching
	 * label in the file is used. */
	for (label = _cur_grffile->label; label != NULL; label = label->next) {
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
		grfmsg(GMS_NOTICE, "Jumping to label 0x%0X at line %d, test was true.", choice->label, choice->nfo_line);
		FioSeekTo(choice->pos, SEEK_SET);
		_nfo_line = choice->nfo_line;
		return;
	}

	grfmsg(GMS_NOTICE, "Skipping %d sprites, test was true.", numsprites);
	_skip_sprites = numsprites;
	if (_skip_sprites == 0) {
		/* Zero means there are no sprites to skip, so
		 * we use -1 to indicate that all further
		 * sprites should be skipped. */
		_skip_sprites = -1;
	}
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
	/* TODO: Check version. (We should have own versioning done somehow.) */
	uint8 version;
	uint32 grfid;
	const char *name;
	const char *info;

	check_length(len, 8, "GRFInfo"); buf++;
	version = grf_load_byte(&buf);
	grfid = grf_load_dword(&buf);
	name = (const char*)buf;
	info = name + strlen(name) + 1;

	_cur_grffile->grfid = grfid;
	_cur_grffile->grf_version = version;
	_cur_grffile->flags |= 0x0001; /* set active flag */

	/* Do swap the GRFID for displaying purposes since people expect that */
	DEBUG(grf, 1) ("[%s] Loaded GRFv%d set %08lx - %s:\n%s",
	               _cur_grffile->filename, version, BSWAP32(grfid), name, info);
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
	uint8 num_sets;
	uint i;

	buf++; /* skip action byte */
	num_sets = grf_load_byte(&buf);

	for (i = 0; i < num_sets; i++) {
		uint8 num_sprites = grf_load_byte(&buf);
		uint16 first_sprite = grf_load_word(&buf);
		uint j;

		grfmsg(GMS_NOTICE,
			"SpriteReplace: [Set %d] Changing %d sprites, beginning with %d",
			i, num_sprites, first_sprite
		);

		for (j = 0; j < num_sprites; j++) {
			LoadNextSprite(first_sprite + j, _file_index); // XXX
			_nfo_line++;
		}
	}
}

/* Action 0x0B */
static void GRFError(byte *buf, int len)
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
	 * B parnum        see action 6, only used with built-in message 03 */
	/* TODO: For now we just show the message, sometimes incomplete and never translated. */

	static const char * const msgstr[4] = {
		"Requires at least pseudo-TTDPatch version %s.",
		"This file is for %s version of TTD.",
		"Designed to be used with %s.",
		"Invalid parameter %s.",
	};
	uint8 severity;
	uint8 msgid;

	check_length(len, 6, "GRFError");
	severity = buf[1];
	msgid = buf[3];

	// Undocumented TTDPatch feature.
	if ((severity & 0x80) == 0 && _cur_stage < 2) {
		DEBUG(grf, 7) ("Skipping non-fatal GRFError in stage 1");
		return;
	}
	severity &= 0x7F;

	if (msgid == 0xFF) {
		grfmsg(severity, "%s", buf+4);
	} else {
		grfmsg(severity, msgstr[msgid], buf+4);
	}
}

/* Action 0x0C */
static void GRFComment(byte *buf, int len)
{
	/* <0C> [<ignored...>]
	 *
	 * V ignored       Anything following the 0C is ignored */

	static char comment[256];
	if (len == 1) return;

	ttd_strlcpy(comment, (char*)(buf + 1), minu(sizeof(comment), len));
	grfmsg(GMS_NOTICE, "GRFComment: %s", comment);
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

	byte target;
	byte oper;
	uint32 src1;
	uint32 src2;
	uint32 data = 0;
	uint32 res;

	check_length(len, 5, "ParamSet");
	buf++;
	target = grf_load_byte(&buf);
	oper = grf_load_byte(&buf);
	src1 = grf_load_byte(&buf);
	src2 = grf_load_byte(&buf);

	if (len >= 8) data = grf_load_dword(&buf);

	/* You can add 80 to the operation to make it apply only if the target
	 * is not defined yet.  In this respect, a parameter is taken to be
	 * defined if any of the following applies:
	 * - it has been set to any value in the newgrf(w).cfg parameter list
	 * - it OR A PARAMETER WITH HIGHER NUMBER has been set to any value by
	 *   an earlier action D */
	if (oper & 0x80) {
		if (target < 0x80 && target < _cur_grffile->param_end) {
			DEBUG(grf, 7) ("Param %u already defined, skipping.", target);
			return;
		}

		oper &= 0x7F;
	}

	if (src2 == 0xFE) {
		if (GB(data, 0, 8) == 0xFF) {
			if (data == 0x0000FFFF) {
				/* Patch variables */
				grfmsg(GMS_NOTICE, "ParamSet: Reading Patch variables unsupported.");
				return;
			} else {
				/* GRF Resource Management */
				grfmsg(GMS_NOTICE, "ParamSet: GRF Resource Management unsupported.");
				return;
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

		case 0x07: /* Bitwise AND */
			res = src1 & src2;
			break;

		case 0x08: /* Bitwise OR */
			res = src1 | src2;
			break;

		case 0x09: /* Unsigned division */
			if (src2 == 0) {
				res = src1;
			} else {
				res = src1 / src2;
			}
			break;

		case 0x0A: /* Signed divison */
			if (src2 == 0) {
				res = src1;
			} else {
				res = (int32)src1 / (int32)src2;
			}
			break;

		case 0x0B: /* Unsigned modulo */
			if (src2 == 0) {
				res = src1;
			} else {
				res = src1 % src2;
			}
			break;

		case 0x0C: /* Signed modulo */
			if (src2 == 0) {
				res = src1;
			} else {
				res = (int32)src1 % (int32)src2;
			}
			break;

		default:
			grfmsg(GMS_ERROR, "ParamSet: Unknown operation %d, skipping.", oper);
			return;
	}

	switch (target) {
		case 0x8E: // Y-Offset for train sprites
			_traininfo_vehicle_pitch = res;
			break;

		// TODO implement
		case 0x8F: // Rail track type cost factors
		case 0x93: // Tile refresh offset to left
		case 0x94: // Tile refresh offset to right
		case 0x95: // Tile refresh offset upwards
		case 0x96: // Tile refresh offset downwards
		case 0x97: // Snow line height
		case 0x99: // Global ID offset
			DEBUG(grf, 7) ("ParamSet: Skipping unimplemented target 0x%02X", target);
			break;

		case 0x9E: /* Miscellaneous GRF features */
			_misc_grf_features = res;
			/* Set train list engine width */
			_traininfo_vehicle_width = HASBIT(res, 3) ? 32 : 29;
			break;

		default:
			if (target < 0x80) {
				_cur_grffile->param[target] = res;
				if (target + 1U > _cur_grffile->param_end) _cur_grffile->param_end = target + 1;
			} else {
				DEBUG(grf, 7) ("ParamSet: Skipping unknown target 0x%02X", target);
			}
			break;
	}
}

/* Action 0x0E */
static void GRFInhibit(byte *buf, int len)
{
	/* <0E> <num> <grfids...>
	 *
	 * B num           Number of GRFIDs that follow
	 * D grfids        GRFIDs of the files to deactivate */

	byte num;
	int i;

	check_length(len, 1, "GRFInhibit");
	buf++, len--;
	num = grf_load_byte(&buf); len--;
	check_length(len, 4 * num, "GRFInhibit");

	for (i = 0; i < num; i++) {
		uint32 grfid = grf_load_dword(&buf);
		GRFFile *file = GetFileByGRFID(grfid);

		/* Unset activation flag */
		if (file != NULL) {
			grfmsg(GMS_NOTICE, "GRFInhibit: Deactivating file ``%s''", file->filename);
			file->flags &= 0xFFFE;
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

	GRFLabel *label;

	check_length(len, 1, "DefineGotoLabel");
	buf++; len--;

	label = malloc(sizeof(*label));
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

	grfmsg(GMS_NOTICE, "DefineGotoLabel: GOTO target with label 0x%02X", label->label);
}

/* Action 0x11 */
static void GRFSound(byte *buf, int len)
{
	/* <11> <num>
	 *
	 * W num      Number of sound files that follow */

	uint16 num;

	check_length(len, 1, "GRFSound");
	buf++;
	num = grf_load_word(&buf);

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
		grfmsg(GMS_WARN, "ImportGRFSound: Source file not available.");
		return;
	}

	if (file->sound_offset + sound >= GetNumSounds()) {
		grfmsg(GMS_WARN, "ImportGRFSound: Sound effect %d is invalid.", sound);
		return;
	}

	grfmsg(GMS_NOTICE, "ImportGRFSound: Copying sound %d (%d) from file %X", sound, file->sound_offset + sound, grfid);

	memcpy(se, GetSound(file->sound_offset + sound), sizeof(*se));

	/* Reset volume and priority, which TTDPatch doesn't copy */
	se->volume   = 128;
	se->priority = 0;
}

/* 'Action 0xFE' */
static void GRFImportBlock(byte *buf, int len)
{
	if (_grf_data_blocks == 0) {
		grfmsg(GMS_NOTICE, "GRFImportBlock: Unexpected import block, skipping.");
		return;
	}

	buf++;

	_grf_data_blocks--;

	/* XXX 'Action 0xFE' isn't really specified. It is only mentioned for
	 * importing sounds, so this is probably all wrong... */
	if (grf_load_byte(&buf) != _grf_data_type) {
		grfmsg(GMS_WARN, "GRFImportBlock: Import type mismatch.");
	}

	switch (_grf_data_type) {
		case GDT_SOUND: ImportGRFSound(buf, len - 1); break;
		default: NOT_REACHED(); break;
	}
}

static void LoadGRFSound(byte *buf, int len)
{
	byte *buf_start = buf;
	FileEntry *se;

	/* Allocate a sound entry. This is done even if the data is not loaded
	 * so that the indices used elsewhere are still correct. */
	se = AllocateFileEntry();

	if (grf_load_dword(&buf) != 'FFIR') {
		grfmsg(GMS_WARN, "LoadGRFSound: Missing RIFF header");
		return;
	}

	/* Size of file -- we ignore this */
	grf_load_dword(&buf);

	if (grf_load_dword(&buf) != 'EVAW') {
		grfmsg(GMS_WARN, "LoadGRFSound: Invalid RIFF type");
		return;
	}

	for (;;) {
		uint32 tag  = grf_load_dword(&buf);
		uint32 size = grf_load_dword(&buf);

		switch (tag) {
			case ' tmf': /* 'fmt ' */
				/* Audio format, must be 1 (PCM) */
				if (grf_load_word(&buf) != 1) {
					grfmsg(GMS_WARN, "LoadGRFSound: Invalid audio format");
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

			case 'atad': /* 'data' */
				se->file_size    = size;
				se->file_offset  = FioGetPos() - (len - (buf - buf_start)) + 1;
				se->file_offset |= _file_index << 24;

				/* Set default volume and priority */
				se->volume = 0x80;
				se->priority = 0;

				grfmsg(GMS_NOTICE, "LoadGRFSound: channels %u, sample rate %u, bits per sample %u, length %u", se->channels, se->rate, se->bits_per_sample, size);
				return;

			default:
				se->file_size = 0;
				return;
		}
	}
}

/* 'Action 0xFF' */
static void GRFDataBlock(byte *buf, int len)
{
	byte name_len;
	const char *name;

	if (_grf_data_blocks == 0) {
		grfmsg(GMS_NOTICE, "GRFDataBlock: unexpected data block, skipping.");
		return;
	}

	buf++;
	name_len = grf_load_byte(&buf);
	name = (const char *)buf;
	buf += name_len + 1;

	grfmsg(GMS_NOTICE, "GRFDataBlock: block name '%s'...", name);

	_grf_data_blocks--;

	switch (_grf_data_type) {
		case GDT_SOUND: LoadGRFSound(buf, len - name_len - 2); break;
		default: NOT_REACHED(); break;
	}
}

static void InitializeGRFSpecial(void)
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
	                   |                                        (0 << 0x1C)  // gradualloading
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
	                   |                                        (0 << 0x0F)  // planespeed
	                   |                                        (0 << 0x10)  // moreindustriesperclimate - obsolete
	                   |                                        (0 << 0x11)  // moretoylandfeatures
	                   |                                        (1 << 0x12)  // newstations
	                   |                                        (0 << 0x13)  // tracktypecostdiff
	                   |                                        (0 << 0x14)  // manualconvert
	                   |       ((_patches.build_on_slopes ? 1 : 0) << 0x15)  // buildoncoasts
	                   |                                        (1 << 0x16)  // canals
	                   |                                        (1 << 0x17)  // newstartyear
	                   |                                        (0 << 0x18)  // freighttrains
	                   |                                        (0 << 0x19)  // newhouses
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
	                   |                                        (0 << 0x0B)  // newcargo
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
	                   |                                        (0 << 0x17); // articulatedrvs
}

static void ResetCustomStations(void)
{
	StationSpec *statspec;
	GRFFile *file;
	uint i;
	uint t;

	for (file = _first_grffile; file != NULL; file = file->next) {
		if (file->stations == NULL) continue;
		for (i = 0; i < MAX_STATIONS; i++) {
			if (file->stations[i] == NULL) continue;
			statspec = file->stations[i];

			/* Release renderdata, if it wasn't copied from another custom station spec  */
			if (!statspec->copied_renderdata) {
				for (t = 0; t < statspec->tiles; t++) {
					free((void*)statspec->renderdata[t].seq);
				}
				free(statspec->renderdata);
			}

			// TODO: Release platforms and layouts

			/* Release this station */
			free(statspec);
		}

		/* Free and reset the station data */
		free(file->stations);
		file->stations = NULL;
	}
}

static void ResetNewGRF(void)
{
	GRFFile *f, *next;

	for (f = _first_grffile; f != NULL; f = next) {
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
static void ResetNewGRFData(void)
{
	uint i;

	CleanUpStrings();

	// Copy/reset original engine info data
	memcpy(&_engine_info, &orig_engine_info, sizeof(orig_engine_info));
	memcpy(&_rail_vehicle_info, &orig_rail_vehicle_info, sizeof(orig_rail_vehicle_info));
	memcpy(&_ship_vehicle_info, &orig_ship_vehicle_info, sizeof(orig_ship_vehicle_info));
	memcpy(&_aircraft_vehicle_info, &orig_aircraft_vehicle_info, sizeof(orig_aircraft_vehicle_info));
	memcpy(&_road_vehicle_info, &orig_road_vehicle_info, sizeof(orig_road_vehicle_info));

	// Copy/reset original bridge info data
	// First, free sprite table data
	for (i = 0; i < MAX_BRIDGES; i++) {
		if (_bridge[i].sprite_table != NULL) {
			uint j;

			for (j = 0; j < 7; j++) free(_bridge[i].sprite_table[j]);
			free(_bridge[i].sprite_table);
		}
	}
	memcpy(&_bridge, &orig_bridge, sizeof(_bridge));

	// Reset refit/cargo class data
	memset(&cargo_allowed, 0, sizeof(cargo_allowed));
	memset(&cargo_disallowed, 0, sizeof(cargo_disallowed));

	// Unload sprite group data
	UnloadWagonOverrides();
	UnloadRotorOverrideSprites();
	UnloadCustomEngineSprites();
	UnloadCustomEngineNames();
	ResetEngineListOrder();

	// Reset price base data
	ResetPriceBaseMultipliers();

	/* Reset the curencies array */
	ResetCurrencies();

	// Reset station classes
	ResetStationClasses();
	ResetCustomStations();

	/* Reset NewGRF files */
	ResetNewGRF();

	// Add engine type to engine data. This is needed for the refit precalculation.
	AddTypeToEngines();

	/* Reset misc GRF features and train list display variables */
	_misc_grf_features = 0;
	_traininfo_vehicle_pitch = 0;
	_traininfo_vehicle_width = 29;
	_have_2cc = false;

	InitializeSoundPool();
	InitializeSpriteGroupPool();
}

/** Reset all NewGRFData that was used only while processing data */
static void ClearTemporaryNewGRFData(void)
{
	/* Clear the GOTO labels used for GRF processing */
	GRFLabel *l;
	for (l = _cur_grffile->label; l != NULL;) {
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

static void InitNewGRFFile(const GRFConfig *config, int sprite_offset)
{
	GRFFile *newfile;

	newfile = GetFileByFilename(config->filename);
	if (newfile != NULL) {
		/* We already loaded it once. */
		newfile->sprite_offset = sprite_offset;
		_cur_grffile = newfile;
		return;
	}

	newfile = calloc(1, sizeof(*newfile));

	if (newfile == NULL) error ("Out of memory");

	newfile->filename = strdup(config->filename);
	newfile->sprite_offset = sprite_offset;

	/* Copy the initial parameter list */
	assert(lengthof(newfile->param) == lengthof(config->param) && lengthof(config->param) == 0x80);
	newfile->param_end = config->num_params;
	memcpy(newfile->param, config->param, 0x80 * sizeof(newfile->param[0]));

	if (_first_grffile == NULL) {
		_cur_grffile = newfile;
		_first_grffile = newfile;
	} else {
		_cur_grffile->next = newfile;
		_cur_grffile = newfile;
	}
}

/**
 * Precalculate refit masks from cargo classes for all vehicles.
 */
static void CalculateRefitMasks(void)
{
	EngineID engine;

	for (engine = 0; engine < TOTAL_NUM_ENGINES; engine++) {
		uint32 mask = 0;
		uint32 not_mask = 0;
		uint32 xor_mask = _engine_info[engine].refit_mask;
		byte i;

		if (cargo_allowed[engine] != 0) {
			// Build up the list of cargo types from the set cargo classes.
			for (i = 0; i < lengthof(cargo_classes); i++) {
				if (HASBIT(cargo_allowed[engine], i)) mask |= cargo_classes[i];
				if (HASBIT(cargo_disallowed[engine], i)) not_mask |= cargo_classes[i];
			}
		} else {
			// Don't apply default refit mask to wagons or engines with no capacity
			if (xor_mask == 0 && (
						GetEngine(engine)->type != VEH_Train || (
							RailVehInfo(engine)->capacity != 0 &&
							!(RailVehInfo(engine)->flags & RVI_WAGON)
						)
					)) {
				xor_mask = _default_refitmasks[GetEngine(engine)->type - VEH_Train];
			}
		}
		_engine_info[engine].refit_mask = ((mask & ~not_mask) ^ xor_mask) & _landscape_global_cargo_mask[_opt.landscape];
	}
}

/* Here we perform initial decoding of some special sprites (as are they
 * described at http://www.ttdpatch.net/src/newgrf.txt, but this is only a very
 * partial implementation yet). */
/* XXX: We consider GRF files trusted. It would be trivial to exploit OTTD by
 * a crafted invalid GRF file. We should tell that to the user somehow, or
 * better make this more robust in the future. */
static void DecodeSpecialSprite(uint num, uint stage)
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
	static const uint32 action_mask[] = {0x10000, 0x0002FB40, 0x0000FFFF};

	static const SpecialSpriteHandler handlers[] = {
		/* 0x00 */ FeatureChangeInfo,
		/* 0x01 */ NewSpriteSet,
		/* 0x02 */ NewSpriteGroup,
		/* 0x03 */ FeatureMapSpriteGroup,
		/* 0x04 */ FeatureNewName,
		/* 0x05 */ GraphicsNew,
		/* 0x06 */ CfgApply,
		/* 0x07 */ SkipIf,
		/* 0x08 */ GRFInfo,
		/* 0x09 */ SkipIf,
		/* 0x0A */ SpriteReplace,
		/* 0x0B */ GRFError,
		/* 0x0C */ GRFComment,
		/* 0x0D */ ParamSet,
		/* 0x0E */ GRFInhibit,
		/* 0x0F */ NULL, // TODO implement
		/* 0x10 */ DefineGotoLabel,
		/* 0x11 */ GRFSound,
	};

	byte* buf;
	byte action;

	if (_preload_sprite == NULL) {
		/* No preloaded sprite to work with; allocate and read the
		 * pseudo sprite content. */
		buf = malloc(num);
		if (buf == NULL) error("DecodeSpecialSprite: Could not allocate memory");
		FioReadBlock(buf, num);
	} else {
		/* Use the preloaded sprite data. */
		buf = _preload_sprite;
		_preload_sprite = NULL;
		DEBUG(grf, 7) ("DecodeSpecialSprite: Using preloaded pseudo sprite data");

		/* Skip the real (original) content of this action. */
		FioSeekTo(num, SEEK_CUR);
	}

	action = buf[0];

	if (action == 0xFF) {
		DEBUG(grf, 7) ("Handling data block in stage %d", stage);
		GRFDataBlock(buf, num);
	} else if (action == 0xFE) {
		DEBUG(grf, 7) ("Handling import block in stage %d", stage);
		GRFImportBlock(buf, num);
	} else if (action >= lengthof(handlers)) {
		DEBUG(grf, 7) ("Skipping unknown action 0x%02X", action);
	} else if (!HASBIT(action_mask[stage], action)) {
		DEBUG(grf, 7) ("Skipping action 0x%02X in stage %d", action, stage);
	} else if (handlers[action] == NULL) {
		DEBUG(grf, 7) ("Skipping unsupported Action 0x%02X", action);
	} else {
		DEBUG(grf, 7) ("Handling action 0x%02X in stage %d", action, stage);
		handlers[action](buf, num);
	}
	free(buf);
}


static void LoadNewGRFFile(const char* filename, uint file_index, uint stage)
{
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
	if (stage != 0) {
		_cur_grffile = GetFileByFilename(filename);
		if (_cur_grffile == NULL) error("File ``%s'' lost in cache.\n", filename);
		if (stage > 1 && !(_cur_grffile->flags & 0x0001)) return;
	}

	FioOpenFile(file_index, filename);
	_file_index = file_index; // XXX

	DEBUG(grf, 7) ("Reading NewGRF-file '%s'", filename);

	/* Skip the first sprite; we don't care about how many sprites this
	 * does contain; newest TTDPatches and George's longvehicles don't
	 * neither, apparently. */
	if (FioReadWord() == 4 && FioReadByte() == 0xFF) {
		FioReadDword();
	} else {
		error("Custom .grf has invalid format.");
	}

	_skip_sprites = 0; // XXX
	_nfo_line = 0;

	while ((num = FioReadWord()) != 0) {
		byte type = FioReadByte();
		_nfo_line++;

		if (type == 0xFF) {
			if (_skip_sprites == 0) {
				DecodeSpecialSprite(num, stage);
				continue;
			} else {
				FioSkipBytes(num);
			}
		} else {
			if (_skip_sprites == 0) DEBUG(grf, 7) ("Skipping unexpected sprite");

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


void LoadNewGRF(uint load_index, uint file_index)
{
	uint stage;

	InitializeGRFSpecial();

	ResetNewGRFData();

	/* Load newgrf sprites
	 * in each loading stage, (try to) open each file specified in the config
	 * and load information from it. */
	for (stage = 0; stage <= 2; stage++) {
		uint slot = file_index;
		GRFConfig *c;

		_cur_stage = stage;
		_cur_spriteid = load_index;
		for (c = _first_grfconfig; c != NULL; c = c->next) {
			if (!FioCheckFileExists(c->filename)) {
				// TODO: usrerror()
				error("NewGRF file missing: %s", c->filename);
			}

			if (stage == 0) InitNewGRFFile(c, _cur_spriteid);
			LoadNewGRFFile(c->filename, slot++, stage);
			if (stage == 2) ClearTemporaryNewGRFData();
			DEBUG(spritecache, 2) ("Currently %i sprites are loaded", load_index);
		}
	}

	// Pre-calculate all refit masks after loading GRF files
	CalculateRefitMasks();
}
