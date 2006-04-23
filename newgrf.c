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
#include "bridge.h"
#include "economy.h"
#include "newgrf_engine.h"
#include "vehicle.h"
#include "newgrf_text.h"

#include "newgrf_spritegroup.h"

/* TTDPatch extended GRF format codec
 * (c) Petr Baudis 2004 (GPL'd)
 * Changes by Florian octo Forster are (c) by the OpenTTD development team.
 *
 * Contains portions of documentation by TTDPatch team.
 * Thanks especially to Josef Drexler for the documentation as well as a lot
 * of help at #tycoon. Also thanks to Michael Blunck for is GRF files which
 * served as subject to the initial testing of this codec. */


uint16 _custom_sprites_base;
static int _skip_sprites; // XXX
static uint _file_index; // XXX
extern int _traininfo_vehicle_pitch;
SpriteID _signal_base = 0;

static GRFFile *_cur_grffile;
GRFFile *_first_grffile;
GRFConfig *_first_grfconfig;
static int _cur_spriteid;
static int _cur_stage;
static uint32 _nfo_line;

/* 32 * 8 = 256 flags. Apparently TTDPatch uses this many.. */
static uint32 _ttdpatch_flags[8];


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
	uint16 val;
	byte *p = *buf;
	val = p[0];
	val |= p[1] << 8;
	*buf = p + 2;
	return val;
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
	uint32 val;
	byte *p = *buf;
	val = p[0];
	val |= p[1] << 8;
	val |= p[2] << 16;
	val |= p[3] << 24;
	*buf = p + 4;
	return val;
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

		case 0x1D: /* Refit cargo */
			FOR_EACH_OBJECT ei[i].refit_mask = grf_load_dword(&buf);
			break;

		case 0x1E: /* Callback */
			FOR_EACH_OBJECT rvi[i].callbackmask = grf_load_byte(&buf);
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

		case 0x27: /* Miscellaneous flags */
			FOR_EACH_OBJECT ei[i].misc_flags = grf_load_byte(&buf);
			break;

		case 0x28: /* Cargo classes allowed */
			FOR_EACH_OBJECT cargo_allowed[engine + i] = grf_load_word(&buf);
			break;

		case 0x29: /* Cargo classes disallowed */
			FOR_EACH_OBJECT cargo_disallowed[engine + i] = grf_load_word(&buf);
			break;

		/* TODO */
		/* Fall-through for unimplemented one byte long properties. */
		case 0x1C: /* Refit cost */
		case 0x1F: /* Tractive effort */
		case 0x20: /* Air drag */
		case 0x25: /* User-defined bit mask to set when checking veh. var. 42 */
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
			FOR_EACH_OBJECT rvi[i].callbackmask = grf_load_byte(&buf);
			break;

		case 0x1C: /* Miscellaneous flags */
			FOR_EACH_OBJECT ei[i].misc_flags = grf_load_byte(&buf);
			break;

		case 0x1D: /* Cargo classes allowed */
			FOR_EACH_OBJECT cargo_allowed[ROAD_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x1E: /* Cargo classes disallowed */
			FOR_EACH_OBJECT cargo_disallowed[ROAD_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x18: /* Tractive effort */
		case 0x19: /* Air drag */
		case 0x1A: /* Refit cost */
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
			FOR_EACH_OBJECT svi[i].callbackmask = grf_load_byte(&buf);
			break;

		case 0x17: /* Miscellaneous flags */
			FOR_EACH_OBJECT ei[i].misc_flags = grf_load_byte(&buf);
			break;

		case 0x18: /* Cargo classes allowed */
			FOR_EACH_OBJECT cargo_allowed[SHIP_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x19: /* Cargo classes disallowed */
			FOR_EACH_OBJECT cargo_disallowed[SHIP_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x13: /* Refit cost */
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
			FOR_EACH_OBJECT SB(avi[i].subtype, 0, 1, (grf_load_byte(&buf) != 0 ? 1 : 0));
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
			FOR_EACH_OBJECT avi[i].callbackmask = grf_load_byte(&buf);
			break;

		case 0x17: /* Miscellaneous flags */
			FOR_EACH_OBJECT ei[i].misc_flags = grf_load_byte(&buf);
			break;

		case 0x18: /* Cargo classes allowed */
			FOR_EACH_OBJECT cargo_allowed[AIRCRAFT_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x19: /* Cargo classes disallowed */
			FOR_EACH_OBJECT cargo_disallowed[AIRCRAFT_ENGINES_INDEX + engine + i] = grf_load_word(&buf);
			break;

		case 0x15: /* Refit cost */
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
	StationSpec *stat;
	byte *buf = *bufp;
	int i;
	bool ret = false;

	/* Allocate station specs if necessary */
	if (_cur_grffile->num_stations < stid + numinfo) {
		_cur_grffile->stations = realloc(_cur_grffile->stations, (stid + numinfo) * sizeof(*_cur_grffile->stations))
;

		while (_cur_grffile->num_stations < stid + numinfo) {
			memset(&_cur_grffile->stations[_cur_grffile->num_stations], 0, sizeof(*_cur_grffile->stations));
			_cur_grffile->num_stations++;
		}
	}

	stat = &_cur_grffile->stations[stid];

	switch (prop) {
		case 0x08: /* Class ID */
			FOR_EACH_OBJECT {
				uint32 classid;

				/* classid, for a change, is big-endian */
				classid = *(buf++) << 24;
				classid |= *(buf++) << 16;
				classid |= *(buf++) << 8;
				classid |= *(buf++);

				stat[i].sclass = AllocateStationClass(classid);
			}
			break;

		case 0x09: /* Define sprite layout */
			FOR_EACH_OBJECT {
				StationSpec *stat = &_cur_grffile->stations[stid + i];
				uint t;

				stat->tiles = grf_load_extended(&buf);
				stat->renderdata = calloc(stat->tiles, sizeof(*stat->renderdata));
				for (t = 0; t < stat->tiles; t++) {
					DrawTileSprites *dts = &stat->renderdata[t];
					uint seq_count = 0;
					PalSpriteID ground_sprite;

					ground_sprite = grf_load_dword(&buf);
					if (ground_sprite == 0) {
						static const DrawTileSeqStruct empty = {0x80, 0, 0, 0, 0, 0, 0};
						dts->seq = &empty;
						continue;
					}

					if (HASBIT(ground_sprite, 31)) {
						// Bit 31 indicates that we should use a custom sprite.
						dts->ground_sprite = GB(ground_sprite, 0, 15) - 0x42D;
						dts->ground_sprite += _cur_grffile->first_spriteset;
					} else {
						dts->ground_sprite = ground_sprite;
					}

					dts->seq = NULL;
					while (buf < *bufp + len) {
						DrawTileSeqStruct *dtss;

						// no relative bounding box support
						dts->seq = realloc((void*)dts->seq, ++seq_count * sizeof(DrawTileSeqStruct));
						dtss = (DrawTileSeqStruct*) &dts->seq[seq_count - 1];

						dtss->delta_x = grf_load_byte(&buf);
						if ((byte) dtss->delta_x == 0x80) break;
						dtss->delta_y = grf_load_byte(&buf);
						dtss->delta_z = grf_load_byte(&buf);
						dtss->width = grf_load_byte(&buf);
						dtss->height = grf_load_byte(&buf);
						dtss->unk = grf_load_byte(&buf);
						dtss->image = grf_load_dword(&buf) - 0x42d;
					}
				}
			}
			break;

		case 0x0A: /* Copy sprite layout */
			FOR_EACH_OBJECT {
				StationSpec *stat = &_cur_grffile->stations[stid + i];
				byte srcid = grf_load_byte(&buf);
				const StationSpec *srcstat = &_cur_grffile->stations[srcid];
				uint t;

				stat->tiles = srcstat->tiles;
				stat->renderdata = calloc(stat->tiles, sizeof(*stat->renderdata));
				for (t = 0; t < stat->tiles; t++) {
					DrawTileSprites *dts = &stat->renderdata[t];
					const DrawTileSprites *sdts = &srcstat->renderdata[t];
					DrawTileSeqStruct const *sdtss = sdts->seq;
					int seq_count = 0;

					dts->ground_sprite = sdts->ground_sprite;
					if (dts->ground_sprite == 0) {
						static const DrawTileSeqStruct empty = {0x80, 0, 0, 0, 0, 0, 0};
						dts->seq = &empty;
						continue;
					}

					dts->seq = NULL;
					while (1) {
						DrawTileSeqStruct *dtss;

						// no relative bounding box support
						dts->seq = realloc((void*)dts->seq, ++seq_count * sizeof(DrawTileSeqStruct));
						dtss = (DrawTileSeqStruct*) &dts->seq[seq_count - 1];
						*dtss = *sdtss;
						if ((byte) dtss->delta_x == 0x80) break;
						sdtss++;
					}
				}
			}
			break;

		case 0x0B: /* Callback mask */
			FOR_EACH_OBJECT stat[i].callbackmask = grf_load_byte(&buf);
			break;

		case 0x0C: /* Disallowed number of platforms */
			FOR_EACH_OBJECT stat[i].disallowed_platforms = grf_load_byte(&buf);
			break;

		case 0x0D: /* Disallowed platform lengths */
			FOR_EACH_OBJECT stat[i].disallowed_lengths = grf_load_byte(&buf);
			break;

		case 0x0E: /* Define custom layout */
			FOR_EACH_OBJECT {
				StationSpec *stat = &_cur_grffile->stations[stid + i];

				while (buf < *bufp + len) {
					byte length = grf_load_byte(&buf);
					byte number = grf_load_byte(&buf);
					StationLayout layout;
					uint l, p;

					if (length == 0 || number == 0) break;

					//debug("l %d > %d ?", length, stat->lengths);
					if (length > stat->lengths) {
						stat->platforms = realloc(stat->platforms, length);
						memset(stat->platforms + stat->lengths, 0, length - stat->lengths);

						stat->layouts = realloc(stat->layouts, length * sizeof(*stat->layouts));
						memset(stat->layouts + stat->lengths, 0,
						       (length - stat->lengths) * sizeof(*stat->layouts));

						stat->lengths = length;
					}
					l = length - 1; // index is zero-based

					//debug("p %d > %d ?", number, stat->platforms[l]);
					if (number > stat->platforms[l]) {
						stat->layouts[l] = realloc(stat->layouts[l],
						                               number * sizeof(**stat->layouts));
						// We expect NULL being 0 here, but C99 guarantees that.
						memset(stat->layouts[l] + stat->platforms[l], 0,
						       (number - stat->platforms[l]) * sizeof(**stat->layouts));

						stat->platforms[l] = number;
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
					free(stat->layouts[l][p]);
					stat->layouts[l][p] = layout;
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
			FOR_EACH_OBJECT stat[i].cargo_threshold = grf_load_word(&buf);
			break;

		case 0x11: /* Pylon placement */
			FOR_EACH_OBJECT stat[i].pylons = grf_load_byte(&buf);
			break;

		case 0x12: /* Cargo types for random triggers */
			FOR_EACH_OBJECT stat[i].cargo_triggers = grf_load_dword(&buf);
			break;

		case 0x13: /* General flags */
			FOR_EACH_OBJECT stat[i].flags = grf_load_byte(&buf);
			break;

		case 0x14: /* Overhead wire placement */
			FOR_EACH_OBJECT stat[i].wires = grf_load_byte(&buf);
			break;

		case 0x15: /* Blocked tiles */
			FOR_EACH_OBJECT stat[i].blocked = grf_load_byte(&buf);
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
			FOR_EACH_OBJECT _bridge[brid + i].avail_year = grf_load_byte(&buf);
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

		default:
			ret = true;
	}

	*bufp = buf;
	return ret;
}

/* Action 0x00 */
static void VehicleChangeInfo(byte *buf, int len)
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
		/* GSF_TRAIN */    RailVehicleChangeInfo,
		/* GSF_ROAD */     RoadVehicleChangeInfo,
		/* GSF_SHIP */     ShipVehicleChangeInfo,
		/* GSF_AIRCRAFT */ AircraftVehicleChangeInfo,
		/* GSF_STATION */  StationChangeInfo,
		/* GSF_CANAL */    NULL,
		/* GSF_BRIDGE */   BridgeChangeInfo,
		/* GSF_TOWNHOUSE */NULL,
		/* GSF_GLOBALVAR */GlobalVarChangeInfo,
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

	check_length(len, 6, "VehicleChangeInfo");
	feature = buf[1];
	numprops = buf[2];
	numinfo = buf[3];
	engine = buf[4];

	DEBUG(grf, 6) ("VehicleChangeInfo: Feature %d, %d properties, to apply to %d+%d",
	               feature, numprops, engine, numinfo);

	if (feature >= lengthof(handler) || handler[feature] == NULL) {
		grfmsg(GMS_WARN, "VehicleChangeInfo: Unsupported feature %d, skipping.", feature);
		return;
	}

	if (feature <= GSF_AIRCRAFT) {
		if (engine + numinfo > _vehcounts[feature]) {
			grfmsg(GMS_ERROR, "VehicleChangeInfo: Last engine ID %d out of bounds (max %d), skipping.", engine + numinfo, _vehcounts[feature]);
			return;
		}
		ei = &_engine_info[engine + _vehshifts[feature]];
	}

	buf += 5;

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
						FOR_EACH_OBJECT ei[i].base_intro = grf_load_word(&buf);
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
			grfmsg(GMS_NOTICE, "VehicleChangeInfo: Ignoring property 0x%02X (not implemented).", prop);
	}
}

#undef FOR_EACH_OBJECT

/**
 * Creates a spritegroup representing a callback result
 * @param value The value that was used to represent this callback result
 * @return A spritegroup representing that callback result
 */
static SpriteGroup* NewCallBackResultSpriteGroup(uint16 value)
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
static SpriteGroup* NewResultSpriteGroup(uint16 value, byte sprites)
{
	SpriteGroup *group = AllocateSpriteGroup();
	group->type = SGT_RESULT;
	group->g.result.result = value;
	group->g.result.sprites = sprites;
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

	if (feature != _cur_grffile->spriteset_feature) {
		grfmsg(GMS_WARN, "NewSpriteGroup: sprite set feature 0x%02X does not match action feature 0x%02X, skipping.",
				_cur_grffile->spriteset_feature, feature);
		/* Clear this group's reference */
		_cur_grffile->spritegroups[setid] = NULL;
		return;
	}

	switch (type) {
		/* Deterministic Sprite Group */
		case 0x81: // Self scope, byte
		case 0x82: // Parent scope, byte
		{
			DeterministicSpriteGroup *dg;
			uint16 groupid;
			int i;

			check_length(bufend - buf, 6, "NewSpriteGroup 0x81/0x82");

			group = AllocateSpriteGroup();
			group->type = SGT_DETERMINISTIC;
			dg = &group->g.determ;

			dg->var_scope = type == 0x82 ? VSG_SCOPE_PARENT : VSG_SCOPE_SELF;
			dg->variable = grf_load_byte(&buf);
			/* Variables 0x60 - 0x7F include an extra parameter */
			if (IS_BYTE_INSIDE(dg->variable, 0x60, 0x80))
				dg->parameter = grf_load_byte(&buf);

			dg->shift_num = grf_load_byte(&buf);
			dg->and_mask = grf_load_byte(&buf);
			dg->operation = dg->shift_num >> 6; /* w00t */
			dg->shift_num &= 0x3F;
			if (dg->operation != DSG_OP_NONE) {
				dg->add_val = grf_load_byte(&buf);
				dg->divmod_val = grf_load_byte(&buf);
			}

			/* (groupid & 0x8000) means this is callback result. */

			dg->num_ranges = grf_load_byte(&buf);
			dg->ranges = calloc(dg->num_ranges, sizeof(*dg->ranges));
			for (i = 0; i < dg->num_ranges; i++) {
				groupid = grf_load_word(&buf);
				if (HASBIT(groupid, 15)) {
					dg->ranges[i].group = NewCallBackResultSpriteGroup(groupid);
				} else if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
					grfmsg(GMS_WARN, "NewSpriteGroup(0x%02X:0x%02X): Groupid 0x%04X does not exist, leaving empty.", setid, type, groupid);
					dg->ranges[i].group = NULL;
				} else {
					dg->ranges[i].group = _cur_grffile->spritegroups[groupid];
				}

				dg->ranges[i].low = grf_load_byte(&buf);
				dg->ranges[i].high = grf_load_byte(&buf);
			}

			groupid = grf_load_word(&buf);
			if (HASBIT(groupid, 15)) {
				dg->default_group = NewCallBackResultSpriteGroup(groupid);
			} else if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
				grfmsg(GMS_WARN, "NewSpriteGroup(0x%02X:0x%02X): Groupid 0x%04X does not exist, leaving empty.", setid, type, groupid);
				dg->default_group = NULL;
			} else {
				dg->default_group = _cur_grffile->spritegroups[groupid];
			}

			break;
		}

		/* Randomized Sprite Group */
		case 0x80: // Self scope
		case 0x83: // Parent scope
		{
			RandomizedSpriteGroup *rg;
			int i;

			/* This stuff is getting actually evaluated in
			 * EvalRandomizedSpriteGroup(). */

			check_length(bufend - buf, 6, "NewSpriteGroup 0x80/0x83");

			group = AllocateSpriteGroup();
			group->type = SGT_RANDOMIZED;
			rg = &group->g.random;

			rg->var_scope = type == 0x83 ? VSG_SCOPE_PARENT : VSG_SCOPE_SELF;

			rg->triggers = grf_load_byte(&buf);
			rg->cmp_mode = rg->triggers & 0x80;
			rg->triggers &= 0x7F;

			rg->lowest_randbit = grf_load_byte(&buf);
			rg->num_groups = grf_load_byte(&buf);

			rg->groups = calloc(rg->num_groups, sizeof(*rg->groups));
			for (i = 0; i < rg->num_groups; i++) {
				uint16 groupid = grf_load_word(&buf);

				if (HASBIT(groupid, 15)) {
					rg->groups[i] = NewCallBackResultSpriteGroup(groupid);
				} else if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
					grfmsg(GMS_WARN, "NewSpriteGroup(0x%02X:0x%02X): Groupid 0x%04X does not exist, leaving empty.", setid, type, groupid);
					rg->groups[i] = NULL;
				} else {
					rg->groups[i] = _cur_grffile->spritegroups[groupid];
				}
			}

			break;
		}

		default:
		{
			RealSpriteGroup *rg;
			byte num_loaded  = type;
			byte num_loading = grf_load_byte(&buf);
			uint i;

			if (_cur_grffile->spriteset_start == 0) {
				grfmsg(GMS_ERROR, "NewSpriteGroup: No sprite set to work on! Skipping.");
				return;
			}

			if (_cur_grffile->first_spriteset == 0)
				_cur_grffile->first_spriteset = _cur_grffile->spriteset_start;

			group = AllocateSpriteGroup();
			group->type = SGT_REAL;
			rg = &group->g.real;

			rg->sprites_per_set = _cur_grffile->spriteset_numents;
			rg->loaded_count  = num_loaded;
			rg->loading_count = num_loading;

			rg->loaded  = calloc(rg->loaded_count,  sizeof(*rg->loaded));
			rg->loading = calloc(rg->loading_count, sizeof(*rg->loading));

			DEBUG(grf, 6) ("NewSpriteGroup: New SpriteGroup 0x%02hhx, %u views, %u loaded, %u loading, sprites %u - %u",
					setid, rg->sprites_per_set, rg->loaded_count, rg->loading_count,
					_cur_grffile->spriteset_start - _cur_grffile->sprite_offset,
					_cur_grffile->spriteset_start + (_cur_grffile->spriteset_numents * (num_loaded + num_loading)) - _cur_grffile->sprite_offset);

			for (i = 0; i < num_loaded; i++) {
				uint16 spriteset_id = grf_load_word(&buf);
				if (HASBIT(spriteset_id, 15)) {
					rg->loaded[i] = NewCallBackResultSpriteGroup(spriteset_id);
				} else {
					rg->loaded[i] = NewResultSpriteGroup(_cur_grffile->spriteset_start + spriteset_id * _cur_grffile->spriteset_numents, rg->sprites_per_set);
				}
				DEBUG(grf, 8) ("NewSpriteGroup: + rg->loaded[%i]  = %u (subset %u)", i, rg->loaded[i]->g.result.result, spriteset_id);
			}

			for (i = 0; i < num_loading; i++) {
				uint16 spriteset_id = grf_load_word(&buf);
				if (HASBIT(spriteset_id, 15)) {
					rg->loading[i] = NewCallBackResultSpriteGroup(spriteset_id);
				} else {
					rg->loading[i] = NewResultSpriteGroup(_cur_grffile->spriteset_start + spriteset_id * _cur_grffile->spriteset_numents, rg->sprites_per_set);
				}
				DEBUG(grf, 8) ("NewSpriteGroup: + rg->loading[%i] = %u (subset %u)", i, rg->loading[i]->g.result.result, spriteset_id);
			}

			break;
		}
	}

	_cur_grffile->spritegroups[setid] = group;
}

/* Action 0x03 */
static void NewVehicle_SpriteGroupMapping(byte *buf, int len)
{
	/* <03> <feature> <n-id> <ids>... <num-cid> [<cargo-type> <cid>]... <def-cid>
	 * id-list	:= [<id>] [id-list]
	 * cargo-list	:= <cargo-type> <cid> [cargo-list]
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

	check_length(len, 7, "VehicleMapSpriteGroup");
	feature = buf[1];
	idcount = buf[2] & 0x7F;
	wagover = (buf[2] & 0x80) == 0x80;
	check_length(len, 3 + idcount, "VehicleMapSpriteGroup");
	cidcount = buf[3 + idcount];
	check_length(len, 4 + idcount + cidcount * 3, "VehicleMapSpriteGroup");

	DEBUG(grf, 6) ("VehicleMapSpriteGroup: Feature %d, %d ids, %d cids, wagon override %d.",
			feature, idcount, cidcount, wagover);

	if (feature > GSF_STATION) {
		grfmsg(GMS_WARN, "VehicleMapSpriteGroup: Unsupported feature %d, skipping.", feature);
		return;
	}


	if (feature == GSF_STATION) {
		// We do things differently for stations.

		for (i = 0; i < idcount; i++) {
			uint8 stid = buf[3 + i];
			StationSpec *stat = &_cur_grffile->stations[stid];
			byte *bp = &buf[4 + idcount];

			for (c = 0; c < cidcount; c++) {
				uint8 ctype = grf_load_byte(&bp);
				uint16 groupid = grf_load_word(&bp);

				if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
					grfmsg(GMS_WARN, "VehicleMapSpriteGroup: Spriteset 0x%04X out of range 0x%X or empty, skipping.",
					       groupid, _cur_grffile->spritegroups_count);
					return;
				}

				if (ctype != 0xFF) {
					/* TODO: No support for any other cargo. */
					continue;
				}

				stat->spritegroup[1] = _cur_grffile->spritegroups[groupid];
			}
		}

		{
			byte *bp = buf + 4 + idcount + cidcount * 3;
			uint16 groupid = grf_load_word(&bp);

			if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
				grfmsg(GMS_WARN, "VehicleMapSpriteGroup: Spriteset 0x%04X out of range 0x%X or empty, skipping.",
				       groupid, _cur_grffile->spritegroups_count);
				return;
			}

			for (i = 0; i < idcount; i++) {
				uint8 stid = buf[3 + i];
				StationSpec *stat = &_cur_grffile->stations[stid];

				stat->spritegroup[0] = _cur_grffile->spritegroups[groupid];
				stat->grfid = _cur_grffile->grfid;
				stat->localidx = stid;
				SetCustomStation(stat);
			}
		}
		return;
	}


	/* If ``n-id'' (or ``idcount'') is zero, this is a ``feature
	 * callback''. I have no idea how this works, so we will ignore it for
	 * now.  --octo */
	if (idcount == 0) {
		grfmsg(GMS_NOTICE, "NewMapping: Feature callbacks not implemented yet.");
		return;
	}

	// FIXME: Tropicset contains things like:
	// 03 00 01 19 01 00 00 00 00 - this is missing one 00 at the end,
	// what should we exactly do with that? --pasky

	if (_cur_grffile->spriteset_start == 0 || _cur_grffile->spritegroups == 0) {
		grfmsg(GMS_WARN, "VehicleMapSpriteGroup: No sprite set to work on! Skipping.");
		return;
	}

	if (!wagover && last_engines_count != idcount) {
		last_engines = realloc(last_engines, idcount);
		last_engines_count = idcount;
	}

	if (wagover) {
		if (last_engines_count == 0) {
			grfmsg(GMS_ERROR, "VehicleMapSpriteGroup: WagonOverride: No engine to do override with.");
			return;
		}
		DEBUG(grf, 6) ("VehicleMapSpriteGroup: WagonOverride: %u engines, %u wagons.",
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

		DEBUG(grf, 7) ("VehicleMapSpriteGroup: [%d] Engine %d...", i, engine);

		for (c = 0; c < cidcount; c++) {
			uint8 ctype = grf_load_byte(&bp);
			uint16 groupid = grf_load_word(&bp);

			DEBUG(grf, 8) ("VehicleMapSpriteGroup: * [%d] Cargo type 0x%X, group id 0x%02X", c, ctype, groupid);

			if (groupid >= _cur_grffile->spritegroups_count || _cur_grffile->spritegroups[groupid] == NULL) {
				grfmsg(GMS_WARN, "VehicleMapSpriteGroup: Spriteset 0x%04X out of range 0x%X or empty, skipping.", groupid, _cur_grffile->spritegroups_count);
				return;
			}

			if (ctype == GC_INVALID) ctype = GC_PURCHASE;

			if (wagover) {
				// TODO: No multiple cargo types per vehicle yet. --pasky
				SetWagonOverrideSprites(engine, _cur_grffile->spritegroups[groupid], last_engines, last_engines_count);
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
				grfmsg(GMS_WARN, "VehicleMapSpriteGroup: Spriteset 0x%04X out of range 0x%X or empty, skipping.", groupid, _cur_grffile->spritegroups_count);
				return;
			}

			if (wagover) {
				// TODO: No multiple cargo types per vehicle yet. --pasky
				SetWagonOverrideSprites(engine, _cur_grffile->spritegroups[groupid], last_engines, last_engines_count);
			} else {
				SetCustomEngineSprites(engine, GC_DEFAULT, _cur_grffile->spritegroups[groupid]);
				last_engines[i] = engine;
			}
		}
	}
}

/* Action 0x04 */
static void VehicleNewName(byte *buf, int len)
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
	 * TODO: Support for custom station class/type names.
	 * All of the above are coming.  In Time.  Some sooner than others :)*/

	uint8 feature;
	uint8 lang;
	uint8 num;
	uint16 id;
	uint16 endid;
	const char* name;

	check_length(len, 6, "VehicleNewName");
	buf++;
	feature  = grf_load_byte(&buf);
	lang     = grf_load_byte(&buf);
	num      = grf_load_byte(&buf);
	id       = (lang & 0x80) ? grf_load_word(&buf) : grf_load_byte(&buf);

	if (feature < GSF_AIRCRAFT+1) {
		id += _vehshifts[feature];
	}
	endid    = id + num;

	DEBUG(grf, 6) ("VehicleNewName: About to rename engines %d..%d (feature %d) in language 0x%02X.",
	               id, endid, feature, lang);

	name = (const char*)buf; /*transfer read value*/
	len -= (lang & 0x80) ? 6 : 5;
	for (; id < endid && len > 0; id++) {
		size_t ofs = strlen(name) + 1;

		if (ofs < 128) {
			DEBUG(grf, 8) ("VehicleNewName: %d <- %s", id, name);

			switch (feature) {
				case GSF_TRAIN:
				case GSF_ROAD:
				case GSF_SHIP:
				case GSF_AIRCRAFT: {
					StringID string = AddGRFString(_cur_grffile->grfid, id, lang, name);
					if (id < TOTAL_NUM_ENGINES) SetCustomEngineName(id, string);
					break;
				}

#if 0
				case GSF_STATION:
					switch (GB(id, 8, 8)) {
						case 0xC4: { /* Station class name */
							StationClassID sclass = _cur_grffile->stations[GB(id, 0, 8)].sclass;
							SetStationClassName(sclass, AddGRFString(_cur_grffile->grfid, id, lang, name));
							break;
						}

						case 0xC5:   /* Station name */
							_cur_grffile->stations[GB(id, 0, 8)].name = AddGRFString(_cur_grffile->grfid, id, lang, name);
							break;

						default:
							DEBUG(grf, 7) ("VehicleNewName: Unsupported ID (0x%04X)", id);
							break;
					}
					break;

				case GSF_CANAL :
				case GSF_BRIDGE :
				case GSF_TOWNHOUSE :
					AddGRFString(_cur_spriteid, id, lang, name);
					switch (GB(id, 8,8)) {
						case 0xC9: /* House name */
						default:
							DEBUG(grf, 7) ("VehicleNewName: Unsupported ID (0x%04X)", id);
					}
					break;

				case GSF_INDUSTRIES :
				case 0x48 :   /* for generic strings */
					AddGRFString(_cur_spriteid, id, lang, name);
					break;
				default :
					DEBUG(grf,7) ("VehicleNewName: Unsupported feature (0x%02X)", feature);
					break;
#endif
			}
		} else {
			DEBUG(grf, 7) ("VehicleNewName: Too long a name (%d)", ofs);
		}
		name += ofs;
		len -= ofs;
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

		default:
			grfmsg(GMS_NOTICE, "GraphicsNew: Custom graphics (type 0x%02X) sprite block of length %u (unimplemented, ignoring).\n",
					type, num);
			return;
	}

	grfmsg(GMS_NOTICE, "GraphicsNew: Loading %u sprites of type 0x%02X at SpriteID 0x%04X", num, type, _cur_spriteid);

	for (; num > 0; num--) {
		LoadNextSprite(_cur_spriteid++, _file_index);
		_nfo_line++;
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
	/* TODO */
	grfmsg(GMS_NOTICE, "CfgApply: Ignoring (not implemented).\n");
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
	bool result;
	GRFLabel *label;
	GRFLabel *choice = NULL;

	check_length(len, 6, "SkipIf");
	param = buf[1];
	paramsize = buf[2];
	condtype = buf[3];

	if (condtype < 2) {
		/* Always 1 for bit tests, the given value should be ignored. */
		paramsize = 1;
	}

	buf += 4;
	switch (paramsize) {
		case 4: cond_val = grf_load_dword(&buf); break;
		case 2: cond_val = grf_load_word(&buf);  break;
		case 1: cond_val = grf_load_byte(&buf);  break;
		default: break;
	}

	switch (param) {
		case 0x83:    /* current climate, 0=temp, 1=arctic, 2=trop, 3=toyland */
			param_val = _opt.landscape;
			break;
		case 0x84:    /* .grf loading stage, 0=initialization, 1=activation */
			param_val = _cur_stage;
			break;
		case 0x85:    /* TTDPatch flags, only for bit tests */
			param_val = _ttdpatch_flags[cond_val / 0x20];
			cond_val %= 0x20;
			break;
		case 0x86:    /* road traffic side, bit 4 clear=left, set=right */
			param_val = _opt.road_side << 4;
			break;
		case 0x88: {  /* see if specified GRFID is active */
			param_val = (GetFileByGRFID(cond_val) != NULL);
		}	break;

		case 0x8B: { /* TTDPatch version */
			uint major    = 2;
			uint minor    = 0;
			uint revision = 10; // special case: 2.0.1 is 2.0.10
			uint build    = 49;
			param_val = (major << 24) | (minor << 20) | (revision << 16) | (build * 10);
			break;
		}

		case 0x8D:    /* TTD Version, 00=DOS, 01=Windows */
			param_val = !_use_dos_palette;
			break;
		case 0x8E:
			param_val = _traininfo_vehicle_pitch;
			break;
		case 0x9D:    /* TTD Platform, 00=TTDPatch, 01=OpenTTD */
			param_val = 1;
			break;
		/* TODO */
		case 0x8F:    /* Track type cost multipliers */
		default:
			if (param < 0x80) {
				/* Parameter. */
				param_val = _cur_grffile->param[param];
			} else {
				/* In-game variable. */
				grfmsg(GMS_WARN, "Unsupported in-game variable 0x%02X. Ignoring test.", param);
				return;
			}
	}

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
		case 6: result = !!param_val; /* GRFID is active (only for param-num=88) */
			break;
		case 7: result = !param_val; /* GRFID is not active (only for param-num=88) */
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

	check_length(len, 8, "GRFInfo");
	version = buf[1];
	/* this is de facto big endian - grf_load_dword() unsuitable */
	grfid = buf[2] << 24 | buf[3] << 16 | buf[4] << 8 | buf[5];
	name = (const char*)(buf + 6);
	info = name + strlen(name) + 1;

	_cur_grffile->grfid = grfid;
	_cur_grffile->flags |= 0x0001; /* set active flag */

	DEBUG(grf, 1) ("[%s] Loaded GRFv%d set %08lx - %s:\n%s",
	               _cur_grffile->filename, version, grfid, name, info);
}

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
		if (_cur_grffile->param_end < target)
			oper &= 0x7F;
		else
			return;
	}

	/* The source1 and source2 operands refer to the grf parameter number
	 * like in action 6 and 7.  In addition, they can refer to the special
	 * variables available in action 7, or they can be FF to use the value
	 * of <data>.  If referring to parameters that are undefined, a value
	 * of 0 is used instead.  */
	if (src1 == 0xFF) {
		src1 = data;
	} else {
		src1 = _cur_grffile->param_end >= src1 ? _cur_grffile->param[src1] : 0;
	}

	if (src2 == 0xFF) {
		src2 = data;
	} else {
		src2 = _cur_grffile->param_end >= src2 ? _cur_grffile->param[src2] : 0;
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
			if ((int32)src2 < 0)
				res = src1 >> -(int32)src2;
			else
				res = src1 << src2;
			break;

		case 0x06:
			if ((int32)src2 < 0)
				res = (int32)src1 >> -(int32)src2;
			else
				res = (int32)src1 << src2;
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

static void InitializeGRFSpecial(void)
{
	/* FIXME: We should rather reflect reality in _ttdpatch_flags[]. */

	_ttdpatch_flags[0] = (_patches.always_small_airport ? (1 << 0x0C) : 0)  /* keepsmallairport */
	                   | (1 << 0x0E)  /* largestations */
	                   | (_patches.longbridges ? (1 << 0x0F) : 0)           /* longbridges */
	                   | (1 << 0x12)  /* presignals */
	                   | (1 << 0x13)  /* extpresignals */
	                   | (_patches.never_expire_vehicles ? (1 << 0x16) : 0) /* enginespersist */
	                   | (1 << 0x1B); /* multihead */
	_ttdpatch_flags[1] = (_patches.mammoth_trains ? (1 << 0x08) : 0)        /* mammothtrains */
	                   | (1 << 0x09)  /* trainrefit */
	                   | (1 << 0x14)  /* bridgespeedlimits */
	                   | (1 << 0x16)  /* eternalgame */
	                   | (1 << 0x17)  /* newtrains */
	                   | (1 << 0x18)  /* newrvs */
	                   | (1 << 0x19)  /* newships */
	                   | (1 << 0x1A)  /* newplanes */
	                   | (_patches.signal_side ? (1 << 0x1B) : 0)           /* signalsontrafficside */
	                   | (1 << 0x1C); /* electrifiedrailway */

	_ttdpatch_flags[2] = (_patches.build_on_slopes ? (1 << 0x0D) : 0)       /* buildonslopes */
	                   | (_patches.build_on_slopes ? (1 << 0x15) : 0)       /* buildoncoasts */
	                   | (1 << 0x16)  /* canals */
	                   | (1 << 0x17)  /* newstartyear */
	                   | (1 << 0x1A)  /* newbridges */
	                   | (_patches.wagon_speed_limits ? (1 << 0x1D) : 0);   /* wagonspeedlimits */
	_ttdpatch_flags[3] = (1 << 0x03)  /* pathbasedsignalling */
	                   | (1 << 0x0C)  /* enhancemultiplayer */
	                   | (1 << 0x0E)  /* irregularstations */
	                   | (1 << 0x10); /* autoreplace */
}

static void ResetCustomStations(void)
{
	GRFFile *file;
	uint i;

	for (file = _first_grffile; file != NULL; file = file->next) {
		for (i = 0; i < file->num_stations; i++) {
			if (file->stations[i].grfid != file->grfid) continue;

			// TODO: Release renderdata, platforms and layouts
		}

		/* Free and reset the station data */
		free(file->stations);
		file->stations = NULL;
		file->num_stations = 0;
	}
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
	UnloadCustomEngineSprites();
	UnloadCustomEngineNames();
	ResetEngineListOrder();

	// Reset price base data
	ResetPriceBaseMultipliers();

	// Reset station classes
	ResetStationClasses();
	ResetCustomStations();

	// Add engine type to engine data. This is needed for the refit precalculation.
	AddTypeToEngines();

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
	static const uint32 action_mask[] = {0x10000, 0x0000FB40, 0x0000FFBF};

	static const SpecialSpriteHandler handlers[] = {
		/* 0x00 */ VehicleChangeInfo,
		/* 0x01 */ NewSpriteSet,
		/* 0x02 */ NewSpriteGroup,
		/* 0x03 */ NewVehicle_SpriteGroupMapping,
		/* 0x04 */ VehicleNewName,
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
	};

	byte* buf = malloc(num);
	byte action;

	if (buf == NULL) error("DecodeSpecialSprite: Could not allocate memory");

	FioReadBlock(buf, num);
	action = buf[0];

	if (action >= lengthof(handlers)) {
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
	_custom_sprites_base = load_index;
	for (stage = 0; stage <= 2; stage++) {
		uint slot = file_index;
		GRFConfig *c;

		_cur_stage = stage;
		_cur_spriteid = load_index;
		for (c = _first_grfconfig; c != NULL; c = c->next) {
			if (!FiosCheckFileExists(c->filename)) {
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
