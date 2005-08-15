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

static GRFFile *_cur_grffile;
GRFFile *_first_grffile;
int _grffile_count;
static int _cur_spriteid;
static int _cur_stage;

/* 32 * 8 = 256 flags. Apparently TTDPatch uses this many.. */
static uint32 _ttdpatch_flags[8];


typedef enum grfspec_feature {
	GSF_TRAIN,
	GSF_ROAD,
	GSF_SHIP,
	GSF_AIRCRAFT,
	GSF_STATION,
	GSF_BRIDGE,
	GSF_TOWNHOUSE,
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
	DEBUG(grf, export_severity) ("[%s][%s] %s", _cur_grffile->filename, severitystr[severity], buf);
}


#define check_length(real, wanted, where) \
do { \
	if (real < wanted) { \
		grfmsg(GMS_ERROR, "%s/%d: Invalid special sprite length %d (expected %d)!", \
		       where, _cur_spriteid - _cur_grffile->sprite_offset, real, wanted); \
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
		case 0x05: { /* Track type */
			FOR_EACH_OBJECT {
				uint8 tracktype = grf_load_byte(&buf);

				ei[i].railtype_climates &= 0xf;
				ei[i].railtype_climates |= tracktype << 4;
			}
		} break;
		case 0x08: { /* AI passenger service */
			/* TODO */
			FOR_EACH_OBJECT {
				grf_load_byte(&buf);
			}
			ret = true;
		} break;
		case 0x09: { /* Speed */
			FOR_EACH_OBJECT {
				uint16 speed = grf_load_word(&buf);

				rvi[i].max_speed = speed;
				dewagonize(speed, engine + i);
			}
		} break;
		case 0x0B: { /* Power */
			FOR_EACH_OBJECT {
				uint16 power = grf_load_word(&buf);

				if (rvi[i].flags & RVI_MULTIHEAD)
					power /= 2;

				rvi[i].power = power;
				dewagonize(power, engine + i);
			}
		} break;
		case 0x0D: { /* Running cost factor */
			FOR_EACH_OBJECT {
				uint8 runcostfact = grf_load_byte(&buf);

				rvi[i].running_cost_base = runcostfact;
				dewagonize(runcostfact, engine + i);
			}
		} break;
		case 0x0E: { /* Running cost base */
			FOR_EACH_OBJECT {
				uint32 base = grf_load_dword(&buf);

				switch (base) {
				case 0x4C30:
					rvi[i].engclass = 0;
					break;
				case 0x4C36:
					rvi[i].engclass = 1;
					break;
				case 0x4C3C:
					rvi[i].engclass = 2;
					break;
				}
				dewagonize(base, engine + i);
			}
		} break;
		case 0x12: { /* Sprite ID */
			FOR_EACH_OBJECT {
				uint8 spriteid = grf_load_byte(&buf);

				if (spriteid == 0xFD && rvi[i].image_index != 0xFD)
					_engine_original_sprites[engine + i] = rvi[i].image_index;
				rvi[i].image_index = spriteid;
			}
		} break;
		case 0x13: { /* Dual-headed */
			FOR_EACH_OBJECT {
				uint8 dual = grf_load_byte(&buf);

				if (dual != 0) {
					if (!(rvi[i].flags & RVI_MULTIHEAD)) // adjust power if needed
						rvi[i].power /= 2;
					rvi[i].flags |= RVI_MULTIHEAD;
				} else {
					if (rvi[i].flags & RVI_MULTIHEAD) // adjust power if needed
						rvi[i].power *= 2;
					rvi[i].flags &= ~RVI_MULTIHEAD;
				}
			}
		} break;
		case 0x14: { /* Cargo capacity */
			FOR_EACH_OBJECT {
				uint8 capacity = grf_load_byte(&buf);

				rvi[i].capacity = capacity;
			}
		} break;
		case 0x15: { /* Cargo type */
			FOR_EACH_OBJECT {
				uint8 ctype = grf_load_byte(&buf);

				rvi[i].cargo_type = ctype;
			}
		} break;
		case 0x16: { /* Weight */
			FOR_EACH_OBJECT {
				uint8 weight = grf_load_byte(&buf);

				rvi[i].weight = weight;
			}
		} break;
		case 0x17: { /* Cost factor */
			FOR_EACH_OBJECT {
				uint8 cfactor = grf_load_byte(&buf);

				rvi[i].base_cost = cfactor;
			}
		} break;
		case 0x18: { /* AI rank */
			/* TODO: _railveh_score should be merged to _rail_vehicle_info. */
			FOR_EACH_OBJECT {
				grf_load_byte(&buf);
			}
			ret = true;
		} break;
		case 0x19: { /* Engine traction type */
			/* TODO: What do the individual numbers mean?
			 * XXX: And in what base are they, in fact? --pasky */
			FOR_EACH_OBJECT {
				uint8 traction = grf_load_byte(&buf);
				int engclass;

				if (traction <= 0x07)
					engclass = 0;
				else if (traction <= 0x27)
					engclass = 1;
				else if (traction <= 0x31)
					engclass = 2;
				else
					break;

				rvi[i].engclass = engclass;
			}
		} break;
		case 0x1B: { /* Powered wagons power bonus */
			FOR_EACH_OBJECT {
				uint16 wag_power = grf_load_word(&buf);

				rvi[i].pow_wag_power = wag_power;
			}
		} break;
		case 0x1D: { /* Refit cargo */
			FOR_EACH_OBJECT {
				uint32 refit_mask = grf_load_dword(&buf);

				_engine_refit_masks[engine + i] = refit_mask;
			}
		} break;
		case 0x1E: { /* Callback */
			FOR_EACH_OBJECT {
				byte callbacks = grf_load_byte(&buf);

				rvi[i].callbackmask = callbacks;
			}
		} break;
		case 0x21: { /* Shorter vehicle */
			FOR_EACH_OBJECT {
				byte shorten_factor = grf_load_byte(&buf);

				rvi[i].shorten_factor = shorten_factor;
			}
		} break;
		case 0x22: { /* Visual effect */
			// see note in engine.h about rvi->visual_effect
			FOR_EACH_OBJECT {
				byte visual = grf_load_byte(&buf);

				rvi[i].visual_effect = visual;
			}
		} break;
		case 0x23: { /* Powered wagons weight bonus */
			FOR_EACH_OBJECT {
				byte wag_weight = grf_load_byte(&buf);

				rvi[i].pow_wag_weight = wag_weight;
			}
		} break;
		/* TODO */
		/* Fall-through for unimplemented one byte long properties. */
		case 0x1A:	/* Sort order */
		case 0x1C:	/* Refit cost */
		case 0x1F:	/* Tractive effort */
		case 0x20:	/* Air drag */
		case 0x24:	/* High byte of vehicle weight */
		case 0x25:	/* User-defined bit mask to set when checking veh. var. 42 */
		case 0x26:	/* Retire vehicle early */
			{
			/* TODO */
			FOR_EACH_OBJECT {
				grf_load_byte(&buf);
			}
			ret = true;
		}	break;
		default:
			ret = true;
	}
	*bufp = buf;
	return ret;
}

static bool RoadVehicleChangeInfo(uint engine, int numinfo, int prop, byte **bufp, int len)
{
	RoadVehicleInfo *rvi = &_road_vehicle_info[engine];
	byte *buf = *bufp;
	int i;
	bool ret = false;

	switch (prop) {
		case 0x08: { /* Speed */
			FOR_EACH_OBJECT {
				uint8 speed = grf_load_byte(&buf);

				rvi[i].max_speed = speed; // ?? units
			}
		} break;
		case 0x09: { /* Running cost factor */
			FOR_EACH_OBJECT {
				uint8 runcost = grf_load_byte(&buf);

				rvi[i].running_cost = runcost;
			}
		} break;
		case 0x0A: { /* Running cost base */
			/* TODO: I have no idea. --pasky */
			FOR_EACH_OBJECT {
				grf_load_dword(&buf);
			}
			ret = true;
		} break;
		case 0x0E: { /* Sprite ID */
			FOR_EACH_OBJECT {
				uint8 spriteid = grf_load_byte(&buf);

				if (spriteid == 0xFF)
					spriteid = 0xFD; // cars have different custom id in the GRF file

				// This is currently not used but there's no reason
				// in not having it here for the future.
				if (spriteid == 0xFD && rvi[i].image_index != 0xFD)
					_engine_original_sprites[ROAD_ENGINES_INDEX + engine + i] = rvi[i].image_index;

				rvi[i].image_index = spriteid;
			}
		} break;
		case 0x0F: { /* Cargo capacity */
			FOR_EACH_OBJECT {
				uint16 capacity = grf_load_byte(&buf);

				rvi[i].capacity = capacity;
			}
		} break;
		case 0x10: { /* Cargo type */
			FOR_EACH_OBJECT {
				uint8 cargo = grf_load_byte(&buf);

				rvi[i].cargo_type = cargo;
			}
		} break;
		case 0x11: { /* Cost factor */
			FOR_EACH_OBJECT {
				uint8 cost_factor = grf_load_byte(&buf);

				rvi[i].base_cost = cost_factor; // ?? is it base_cost?
			}
		} break;
		case 0x12: { /* SFX */
			FOR_EACH_OBJECT {
				uint8 sfx = grf_load_byte(&buf);

				rvi[i].sfx = sfx;
			}
		} break;
		case 0x13:      /* Power in 10hp */
		case 0x14:      /* Weight in 1/4 tons */
		case 0x15:      /* Speed in mph*0.8 */
			/* TODO: Support for road vehicles realistic power
			 * computations (called rvpower in TTDPatch) is just
			 * missing in OTTD yet. --pasky */
			FOR_EACH_OBJECT {
				grf_load_byte(&buf);
			}
			ret = true;
			break;
		case 0x16: { /* Cargos available for refitting */
			FOR_EACH_OBJECT {
				uint32 refit_mask = grf_load_dword(&buf);

				_engine_refit_masks[ROAD_ENGINES_INDEX + engine + i] = refit_mask;
			}
		} break;
		case 0x17:      /* Callback */
		case 0x18:      /* Tractive effort */
		case 0x19:      /* Air drag */
		case 0x1A:      /* Refit cost */
		case 0x1B:      /* Retire vehicle early */
			{
			/* TODO */
			FOR_EACH_OBJECT {
				grf_load_byte(&buf);
			}
			ret = true;
			break;
		}
		default:
			ret = true;
	}

	*bufp = buf;
	return ret;
}

static bool ShipVehicleChangeInfo(uint engine, int numinfo, int prop, byte **bufp, int len)
{
	ShipVehicleInfo *svi = &_ship_vehicle_info[engine];
	byte *buf = *bufp;
	int i;
	bool ret = false;

	//printf("e %x prop %x?\n", engine, prop);
	switch (prop) {
		case 0x08: {	/* Sprite ID */
			FOR_EACH_OBJECT {
				uint8 spriteid = grf_load_byte(&buf);

				if (spriteid == 0xFF)
					spriteid = 0xFD; // ships have different custom id in the GRF file

				// This is currently not used but there's no reason
				// in not having it here for the future.
				if (spriteid == 0xFD && svi[i].image_index != 0xFD)
					_engine_original_sprites[SHIP_ENGINES_INDEX + engine + i] = svi[i].image_index;

				svi[i].image_index = spriteid;
			}
		}	break;
		case 0x09: {	/* Refittable */
			FOR_EACH_OBJECT {
				uint8 refittable = grf_load_byte(&buf);

				svi[i].refittable = refittable;
			}
		}	break;
		case 0x0A: {	/* Cost factor */
			FOR_EACH_OBJECT {
				uint8 cost_factor = grf_load_byte(&buf);

				svi[i].base_cost = cost_factor; // ?? is it base_cost?
			}
		}	break;
		case 0x0B: {	/* Speed */
			FOR_EACH_OBJECT {
				uint8 speed = grf_load_byte(&buf);

				svi[i].max_speed = speed; // ?? units
			}
		}	break;
		case 0x0C: { /* Cargo type */
			FOR_EACH_OBJECT {
				uint8 cargo = grf_load_byte(&buf);

				// XXX: Need to consult this with patchman yet.
#if 0
				// Documentation claims this is already the
				// per-landscape cargo type id, but newships.grf
				// assume otherwise.
				cargo = local_cargo_id_ctype[cargo];
#endif
				svi[i].cargo_type = cargo;
			}
		}	break;
		case 0x0D: {	/* Cargo capacity */
			FOR_EACH_OBJECT {
				uint16 capacity = grf_load_word(&buf);

				svi[i].capacity = capacity;
			}
		}	break;
		case 0x0F: {	/* Running cost factor */
			FOR_EACH_OBJECT {
				uint8 runcost = grf_load_byte(&buf);

				svi[i].running_cost = runcost;
			}
		} break;
		case 0x10: {	/* SFX */
			FOR_EACH_OBJECT {
				uint8 sfx = grf_load_byte(&buf);

				svi[i].sfx = sfx;
			}
		}	break;
		case 0x11: {	/* Cargos available for refitting */
			FOR_EACH_OBJECT {
				uint32 refit_mask = grf_load_dword(&buf);

				_engine_refit_masks[SHIP_ENGINES_INDEX + engine + i] = refit_mask;
			}
		}	break;
		case 0x12: /* Callback */
		case 0x13: /* Refit cost */
		case 0x14: /* Ocean speed fraction */
		case 0x15: /* Canal speed fraction */
		case 0x16: /* Retire vehicle early */
		{
			/* TODO */
			FOR_EACH_OBJECT {
				grf_load_byte(&buf);
			}
			ret = true;
		}	break;
		default:
			ret = true;
	}

	*bufp = buf;
	return ret;
}

static bool AircraftVehicleChangeInfo(uint engine, int numinfo, int prop, byte **bufp, int len)
{
	AircraftVehicleInfo *avi = &_aircraft_vehicle_info[engine];
	byte *buf = *bufp;
	int i;
	bool ret = false;

	//printf("e %x prop %x?\n", engine, prop);
	switch (prop) {
		case 0x08: {	/* Sprite ID */
			FOR_EACH_OBJECT {
				uint8 spriteid = grf_load_byte(&buf);

				if (spriteid == 0xFF)
					spriteid = 0xFD; // ships have different custom id in the GRF file

				// This is currently not used but there's no reason
				// in not having it here for the future.
				if (spriteid == 0xFD && avi[i].image_index != 0xFD)
					_engine_original_sprites[AIRCRAFT_ENGINES_INDEX + engine + i] = avi[i].image_index;

				avi[i].image_index = spriteid;
			}
		}	break;
		case 0x09: {	/* Helicopter */
			FOR_EACH_OBJECT {
				uint8 heli = grf_load_byte(&buf);
				avi[i].subtype &= ~0x01; // remove old property
				avi[i].subtype |= (heli == 0) ? 0 : 1;
			}
		}	break;
		case 0x0A: {	/* Large */
			FOR_EACH_OBJECT {
				uint8 large = grf_load_byte(&buf);
				avi[i].subtype &= ~0x02; // remove old property
				avi[i].subtype |= (large == 1) ? 2 : 0;
			}
		}	break;
		case 0x0B: {	/* Cost factor */
			FOR_EACH_OBJECT {
				uint8 cost_factor = grf_load_byte(&buf);

				avi[i].base_cost = cost_factor; // ?? is it base_cost?
			}
		}	break;
		case 0x0C: {	/* Speed */
			FOR_EACH_OBJECT {
				uint8 speed = grf_load_byte(&buf);

				avi[i].max_speed = speed; // ?? units
			}
		}	break;
		case 0x0D: {	/* Acceleration */
			FOR_EACH_OBJECT {
				uint8 accel = grf_load_byte(&buf);

				avi[i].acceleration = accel;
			}
		} break;
		case 0x0E: {	/* Running cost factor */
			FOR_EACH_OBJECT {
				uint8 runcost = grf_load_byte(&buf);

				avi[i].running_cost = runcost;
			}
		} break;
		case 0x0F: {	/* Passenger capacity */
			FOR_EACH_OBJECT {
				uint16 capacity = grf_load_word(&buf);

				avi[i].passenger_capacity = capacity;
			}
		}	break;
		case 0x11: {	/* Mail capacity */
			FOR_EACH_OBJECT {
				uint8 capacity = grf_load_byte(&buf);

				avi[i].mail_capacity = capacity;
			}
		}	break;
		case 0x12: {	/* SFX */
			FOR_EACH_OBJECT {
				uint8 sfx = grf_load_byte(&buf);

				avi[i].sfx = sfx;
			}
		}	break;
		case 0x13: {	/* Cargos available for refitting */
			FOR_EACH_OBJECT {
				uint32 refit_mask = grf_load_dword(&buf);

				_engine_refit_masks[AIRCRAFT_ENGINES_INDEX + engine + i] = refit_mask;
			}
		}	break;
		case 0x14: /* Callback */
		case 0x15: /* Refit cost */
		case 0x16: /* Retire vehicle early */
		{
			/* TODO */
			FOR_EACH_OBJECT {
				grf_load_byte(&buf);
			}
			ret = true;
		}	break;
		default:
			ret = true;
	}

	*bufp = buf;
	return ret;
}

static bool StationChangeInfo(uint stid, int numinfo, int prop, byte **bufp, int len)
{
	byte *buf = *bufp;
	int i;
	int ret = 0;

	/* This is one single huge TODO. It doesn't handle anything more than
	 * just waypoints for now. */

	//printf("sci %d %d [0x%02x]\n", stid, numinfo, prop);
	switch (prop) {
		case 0x08:
		{	/* Class ID */
			FOR_EACH_OBJECT {
				StationSpec *stat = &_cur_grffile->stations[stid + i];
				uint32 classid;

				/* classid, for a change, is always little-endian */
				classid = *(buf++) << 24;
				classid |= *(buf++) << 16;
				classid |= *(buf++) << 8;
				classid |= *(buf++);

				switch (classid) {
					case 'DFLT':
						stat->sclass = STAT_CLASS_DFLT;
						break;
					case 'WAYP':
						stat->sclass = STAT_CLASS_WAYP;
						break;
					default:
						/* TODO: No support for custom
						 * classes for now, so stuff
						 * everything to the single
						 * default one. --pasky */
						stat->sclass = STAT_CLASS_DFLT;
						//stat->sclass = STAT_CLASS_CUSTOM;
						break;
				}
			}
			break;
		}
		case 0x09:
		{	/* Define sprite layout */
			FOR_EACH_OBJECT {
				StationSpec *stat = &_cur_grffile->stations[stid + i];
				int t;

				stat->tiles = grf_load_byte(&buf);
				for (t = 0; t < stat->tiles; t++) {
					DrawTileSprites *dts = &stat->renderdata[t];
					int seq_count = 0;

					if (t >= 8) {
						grfmsg(GMS_WARN, "StationChangeInfo: Sprite %d>=8, skipping.", t);
						grf_load_dword(&buf); // at least something
						continue;
					}

					dts->ground_sprite = grf_load_dword(&buf);
					if (!dts->ground_sprite) {
						static const DrawTileSeqStruct empty = {0x80, 0, 0, 0, 0, 0, 0};
						dts->seq = &empty;
						continue;
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
		}
		case 0x0a:
		{	/* Copy sprite layout */
			FOR_EACH_OBJECT {
				StationSpec *stat = &_cur_grffile->stations[stid + i];
				byte srcid = grf_load_byte(&buf);
				StationSpec *srcstat = &_cur_grffile->stations[srcid];
				int t;

				stat->tiles = srcstat->tiles;
				for (t = 0; t < stat->tiles; t++) {
					DrawTileSprites *dts = &stat->renderdata[t];
					DrawTileSprites *sdts = &srcstat->renderdata[t];
					DrawTileSeqStruct const *sdtss = sdts->seq;
					int seq_count = 0;

					dts->ground_sprite = sdts->ground_sprite;
					if (!dts->ground_sprite) {
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
		}
		case 0x0b:
		{	/* Callback */
			/* TODO */
			FOR_EACH_OBJECT {
				grf_load_byte(&buf);
			}
			ret = 1;
			break;
		}
		case 0x0C:
		{	/* Platforms number */
			FOR_EACH_OBJECT {
				StationSpec *stat = &_cur_grffile->stations[stid + i];

				stat->allowed_platforms = ~grf_load_byte(&buf);
			}
			break;
		}
		case 0x0D:
		{	/* Platforms length */
			FOR_EACH_OBJECT {
				StationSpec *stat = &_cur_grffile->stations[stid + i];

				stat->allowed_lengths = ~grf_load_byte(&buf);
			}
			break;
		}
		case 0x0e:
		{	/* Define custom layout */
			FOR_EACH_OBJECT {
				StationSpec *stat = &_cur_grffile->stations[stid + i];

				while (buf < *bufp + len) {
					byte length = grf_load_byte(&buf);
					byte number = grf_load_byte(&buf);
					StationLayout layout;
					int l, p;

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
					for (l = 0; l < length; l++)
						for (p = 0; p < number; p++)
							layout[l * number + p] = grf_load_byte(&buf);

					l--;
					p--;
					assert(p >= 0);
					free(stat->layouts[l][p]);
					stat->layouts[l][p] = layout;
				}
			}
			break;
		}
		case 0x0f:
		{	/* Copy custom layout */
			/* TODO */
			FOR_EACH_OBJECT {
				grf_load_byte(&buf);
			}
			ret = 1;
			break;
		}
		case 0x10:
		{	/* Little/lots cargo threshold */
			/* TODO */
			FOR_EACH_OBJECT {
				grf_load_word(&buf);
			}
			ret = 1;
			break;
		}
		case 0x11:
		{	/* Pylon placement */
			/* TODO; makes sense only for electrified tracks */
			FOR_EACH_OBJECT {
				grf_load_word(&buf);
			}
			ret = 1;
			break;
		}
		case 0x12:
		{	/* Cargo types for random triggers */
			/* TODO */
			FOR_EACH_OBJECT {
				grf_load_dword(&buf);
			}
			ret = 1;
			break;
		}
		default:
			ret = 1;
			break;
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

	static const VCI_Handler handler[7] = {
		/* GSF_TRAIN */    RailVehicleChangeInfo,
		/* GSF_ROAD */     RoadVehicleChangeInfo,
		/* GSF_SHIP */     ShipVehicleChangeInfo,
		/* GSF_AIRCRAFT */ AircraftVehicleChangeInfo,
		/* GSF_STATION */  StationChangeInfo,
		/* GSF_BRIDGE */   NULL,
		/* GSF_TOWNHOUSE */NULL,
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

	if (feature > GSF_STATION) {
		grfmsg(GMS_WARN, "VehicleChangeInfo: Unsupported feature %d, skipping.", feature);
		return;
	}

	if (feature != GSF_STATION)
		ei = &_engine_info[engine + _vehshifts[feature]];
	/* XXX - Should there not be a check to see if 'ei' is NULL
	    when it is used in the switch below?? -- TrueLight */

	buf += 5;

	while (numprops-- && buf < bufend) {
		uint8 prop = grf_load_byte(&buf);

		if (feature == GSF_STATION)
			// stations don't share those common properties
			goto run_handler;

		switch (prop) {
		case 0x00: { /* Introduction date */
			FOR_EACH_OBJECT {
				uint16 date = grf_load_word(&buf);

				ei[i].base_intro = date;
			}
		}	break;
		case 0x02: { /* Decay speed */
			FOR_EACH_OBJECT {
				uint8 decay = grf_load_byte(&buf);

				ei[i].unk2 &= 0x80;
				ei[i].unk2 |= decay & 0x7f;
			}
		}	break;
		case 0x03: { /* Vehicle life */
			FOR_EACH_OBJECT {
				uint8 life = grf_load_byte(&buf);

				ei[i].lifelength = life;
			}
		}	break;
		case 0x04: { /* Model life */
			FOR_EACH_OBJECT {
				uint8 life = grf_load_byte(&buf);

				ei[i].base_life = life;
			}
		}	break;
		case 0x06: { /* Climates available */
			FOR_EACH_OBJECT {
				uint8 climates = grf_load_byte(&buf);

				ei[i].railtype_climates &= 0xf0;
				ei[i].railtype_climates |= climates;
			}
		}	break;
		case 0x07: { /* Loading speed */
			/* TODO */
			/* Hyronymus explained me what does
			 * this mean and insists on having a
			 * credit ;-). --pasky */
			/* TODO: This needs to be supported by
			 * LoadUnloadVehicle() first. */
			FOR_EACH_OBJECT {
				grf_load_byte(&buf);
			}
			goto ignoring;
		}
		default: {
run_handler:
			if (handler[feature](engine, numinfo, prop, &buf, bufend - buf)) {
ignoring:
				grfmsg(GMS_NOTICE, "VehicleChangeInfo: Ignoring property %x (not implemented).", prop);
			}
			break;
		}
		}
	}
}

#undef FOR_EACH_OBJECT

/**
 * Creates a spritegroup representing a callback result
 * @param value The value that was used to represent this callback result
 * @return A spritegroup representing that callback result
 */
SpriteGroup NewCallBackResult(uint16 value)
{
	SpriteGroup group;

	group.type = SGT_CALLBACK;

	// Old style callback results have the highest byte 0xFF so signify it is a callback result
	// New style ones only have the highest bit set (allows 15-bit results, instead of just 8)
	if ((value >> 8) == 0xFF)
		value &= 0xFF;
	else
		value &= ~0x8000;

	group.g.callback.result = value;

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
	 * B num-ent       how many entries per sprite set
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
	feature = buf[1];
	num_sets = buf[2];
	num_ents = buf[3];

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
	}
}

/* Action 0x02 */
static void NewSpriteGroup(byte *buf, int len)
{
	byte *bufend = buf + len;

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
	/* TODO: No 0x80-types (ugh). */
	/* TODO: Also, empty sprites aren't handled for now. Need to investigate
	 * the "opacity" rules for these, that is which sprite to fall back to
	 * when. --pasky */
	uint8 feature;
	uint8 setid;
	/* XXX: For stations, these two are "little cargo" and "lotsa cargo" sets. */
	uint8 numloaded;
	uint8 numloading;
	SpriteGroup *group;
	RealSpriteGroup *rg;
	byte *loaded_ptr;
	byte *loading_ptr;
	int i;

	check_length(len, 5, "NewSpriteGroup");
	feature = buf[1];
	setid = buf[2];
	numloaded = buf[3];
	numloading = buf[4];

	if (numloaded == 0x81 || numloaded == 0x82) {
		DeterministicSpriteGroup *dg;
		uint16 groupid;
		int i;

		// Ok, this is gonna get a little wild, so hold your breath...

		/* This stuff is getting actually evaluated in
		 * EvalDeterministicSpriteGroup(). */

		buf += 4; len -= 4;
		check_length(len, 6, "NewSpriteGroup 0x81/0x82");

		if (setid >= _cur_grffile->spritegroups_count) {
			_cur_grffile->spritegroups_count = setid + 1;
			_cur_grffile->spritegroups = realloc(_cur_grffile->spritegroups, _cur_grffile->spritegroups_count * sizeof(*_cur_grffile->spritegroups));
		}

		group = &_cur_grffile->spritegroups[setid];
		memset(group, 0, sizeof(*group));
		group->type = SGT_DETERMINISTIC;
		dg = &group->g.determ;

		/* XXX: We don't free() anything, assuming that if there was
		 * some action here before, it got associated by action 3.
		 * We should perhaps keep some refcount? --pasky */

		dg->var_scope = numloaded == 0x82 ? VSG_SCOPE_PARENT : VSG_SCOPE_SELF;
		dg->variable = grf_load_byte(&buf);

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
			if (groupid & 0x8000) {
				dg->ranges[i].group = NewCallBackResult(groupid);
			} else if (groupid >= _cur_grffile->spritegroups_count) {
				/* This doesn't exist for us. */
				grf_load_word(&buf); // skip range
				i--; dg->num_ranges--;
				continue;
			} else {
			/* XXX: If multiple surreal sets attach a surreal
			 * set this way, we are in trouble. */
				dg->ranges[i].group = _cur_grffile->spritegroups[groupid];
			}

			dg->ranges[i].low = grf_load_byte(&buf);
			dg->ranges[i].high = grf_load_byte(&buf);
		}

		groupid = grf_load_word(&buf);
		if (groupid & 0x8000) {
			dg->default_group = malloc(sizeof(*dg->default_group));
			*dg->default_group = NewCallBackResult(groupid);
		} else if (groupid >= _cur_grffile->spritegroups_count) {
			/* This spritegroup stinks. */
			free(dg->ranges), dg->ranges = NULL;
			grfmsg(GMS_WARN, "NewSpriteGroup(%02x:0x%x): Default groupid %04x is cargo callback or unknown, ignoring spritegroup.", setid, numloaded, groupid);
			return;
		} else {
			dg->default_group = malloc(sizeof(*dg->default_group));
			memcpy(dg->default_group, &_cur_grffile->spritegroups[groupid], sizeof(*dg->default_group));
		}

		return;

	} else if (numloaded == 0x80 || numloaded == 0x83) {
		RandomizedSpriteGroup *rg;
		int i;

		/* This stuff is getting actually evaluated in
		 * EvalRandomizedSpriteGroup(). */

		buf += 4;
		len -= 4;
		check_length(len, 6, "NewSpriteGroup 0x80/0x83");

		if (setid >= _cur_grffile->spritegroups_count) {
			_cur_grffile->spritegroups_count = setid + 1;
			_cur_grffile->spritegroups = realloc(_cur_grffile->spritegroups, _cur_grffile->spritegroups_count * sizeof(*_cur_grffile->spritegroups));
		}

		group = &_cur_grffile->spritegroups[setid];
		memset(group, 0, sizeof(*group));
		group->type = SGT_RANDOMIZED;
		rg = &group->g.random;

		/* XXX: We don't free() anything, assuming that if there was
		 * some action here before, it got associated by action 3.
		 * We should perhaps keep some refcount? --pasky */

		rg->var_scope = numloaded == 0x83 ? VSG_SCOPE_PARENT : VSG_SCOPE_SELF;

		rg->triggers = grf_load_byte(&buf);
		rg->cmp_mode = rg->triggers & 0x80;
		rg->triggers &= 0x7F;

		rg->lowest_randbit = grf_load_byte(&buf);
		rg->num_groups = grf_load_byte(&buf);

		rg->groups = calloc(rg->num_groups, sizeof(*rg->groups));
		for (i = 0; i < rg->num_groups; i++) {
			uint16 groupid = grf_load_word(&buf);

			if (groupid & 0x8000 || groupid >= _cur_grffile->spritegroups_count) {
				/* This doesn't exist for us. */
				i--;
				rg->num_groups--;
				continue;
			}
			/* XXX: If multiple surreal sets attach a surreal
			 * set this way, we are in trouble. */
			rg->groups[i] = _cur_grffile->spritegroups[groupid];
		}

		return;
	}

	if (!_cur_grffile->spriteset_start) {
		grfmsg(GMS_ERROR, "NewSpriteGroup: No sprite set to work on! Skipping.");
		return;
	}

	if (_cur_grffile->spriteset_feature != feature) {
		grfmsg(GMS_ERROR, "NewSpriteGroup: Group feature %x doesn't match set feature %x! Playing it risky and trying to use it anyway.", feature, _cur_grffile->spriteset_feature);
		// return; // XXX: we can't because of MB's newstats.grf --pasky
	}

	check_length(bufend - buf, 5, "NewSpriteGroup");
	buf += 5;
	check_length(bufend - buf, 2 * numloaded, "NewSpriteGroup");
	loaded_ptr = buf;
	loading_ptr = buf + 2 * numloaded;

	if (numloaded > 16) {
		grfmsg(GMS_WARN, "NewSpriteGroup: More than 16 sprites in group %x, skipping the rest.", setid);
		numloaded = 16;
	}
	if (numloading > 16) {
		grfmsg(GMS_WARN, "NewSpriteGroup: More than 16 sprites in group %x, skipping the rest.", setid);
		numloading = 16;
	}

	if (setid >= _cur_grffile->spritegroups_count) {
		_cur_grffile->spritegroups_count = setid + 1;
		_cur_grffile->spritegroups = realloc(_cur_grffile->spritegroups, _cur_grffile->spritegroups_count * sizeof(*_cur_grffile->spritegroups));
	}
	group = &_cur_grffile->spritegroups[setid];
	memset(group, 0, sizeof(*group));
	group->type = SGT_REAL;
	rg = &group->g.real;

	rg->sprites_per_set = _cur_grffile->spriteset_numents;
	rg->loaded_count  = numloaded;
	rg->loading_count = numloading;

	DEBUG(grf, 6) ("NewSpriteGroup: New SpriteGroup 0x%02hhx, %u views, %u loaded, %u loading, sprites %u - %u",
			setid, rg->sprites_per_set, rg->loaded_count, rg->loading_count,
			_cur_grffile->spriteset_start - _cur_grffile->sprite_offset,
			_cur_grffile->spriteset_start + (_cur_grffile->spriteset_numents * (numloaded + numloading)) - _cur_grffile->sprite_offset);

	for (i = 0; i < numloaded; i++) {
		uint16 spriteset_id = grf_load_word(&loaded_ptr);
		rg->loaded[i] = _cur_grffile->spriteset_start + spriteset_id * _cur_grffile->spriteset_numents;
		DEBUG(grf, 8) ("NewSpriteGroup: + rg->loaded[%i]  = %u (subset %u)", i, rg->loaded[i], spriteset_id);
	}

	for (i = 0; i < numloading; i++) {
		uint16 spriteset_id = grf_load_word(&loading_ptr);
		rg->loading[i] = _cur_grffile->spriteset_start + spriteset_id * _cur_grffile->spriteset_numents;
		DEBUG(grf, 8) ("NewSpriteGroup: + rg->loading[%i] = %u (subset %u)", i, rg->loading[i], spriteset_id);
	}
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

				if (groupid >= _cur_grffile->spritegroups_count) {
					grfmsg(GMS_WARN, "VehicleMapSpriteGroup: Spriteset %x out of range %x, skipping.",
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

			if (groupid >= _cur_grffile->spritegroups_count) {
				grfmsg(GMS_WARN, "VehicleMapSpriteGroup: Spriteset %x out of range %x, skipping.",
				       groupid, _cur_grffile->spritegroups_count);
				return;
			}

			for (i = 0; i < idcount; i++) {
				uint8 stid = buf[3 + i];
				StationSpec *stat = &_cur_grffile->stations[stid];

				stat->spritegroup[0] = _cur_grffile->spritegroups[groupid];
				stat->grfid = _cur_grffile->grfid;
				SetCustomStation(stid, stat);
				stat->sclass = STAT_CLASS_NONE;
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

	if (!_cur_grffile->spriteset_start || !_cur_grffile->spritegroups) {
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
			grfmsg(GMS_ERROR, "Id %u for feature %x is out of bounds.",
					engine_id, feature);
			return;
		}

		DEBUG(grf, 7) ("VehicleMapSpriteGroup: [%d] Engine %d...", i, engine);

		for (c = 0; c < cidcount; c++) {
			uint8 ctype = grf_load_byte(&bp);
			uint16 groupid = grf_load_word(&bp);

			DEBUG(grf, 8) ("VehicleMapSpriteGroup: * [%d] Cargo type %x, group id %x", c, ctype, groupid);

			if (groupid >= _cur_grffile->spritegroups_count) {
				grfmsg(GMS_WARN, "VehicleMapSpriteGroup: Spriteset %x out of range %x, skipping.", groupid, _cur_grffile->spritegroups_count);
				return;
			}

			if (ctype == GC_INVALID) ctype = GC_PURCHASE;

			if (wagover) {
				// TODO: No multiple cargo types per vehicle yet. --pasky
				SetWagonOverrideSprites(engine, &_cur_grffile->spritegroups[groupid], last_engines, last_engines_count);
			} else {
				SetCustomEngineSprites(engine, ctype, &_cur_grffile->spritegroups[groupid]);
				last_engines[i] = engine;
			}
		}
	}

	{
		byte *bp = buf + 4 + idcount + cidcount * 3;
		uint16 groupid = grf_load_word(&bp);

		DEBUG(grf, 8) ("-- Default group id %x", groupid);

		for (i = 0; i < idcount; i++) {
			uint8 engine = buf[3 + i] + _vehshifts[feature];

			// Don't tell me you don't love duplicated code!
			if (groupid >= _cur_grffile->spritegroups_count) {
				grfmsg(GMS_WARN, "VehicleMapSpriteGroup: Spriteset %x out of range %x, skipping.", groupid, _cur_grffile->spritegroups_count);
				return;
			}

			if (wagover) {
				// TODO: No multiple cargo types per vehicle yet. --pasky
				SetWagonOverrideSprites(engine, &_cur_grffile->spritegroups[groupid], last_engines, last_engines_count);
			} else {
				SetCustomEngineSprites(engine, GC_DEFAULT, &_cur_grffile->spritegroups[groupid]);
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
	 * B veh-type      see action 0
	 * B language-id   language ID with bit 7 cleared (see below)
	 * B num-veh       number of vehicles which are getting a new name
	 * B offset        number of the first vehicle that gets a new name
	 * S data          new texts, each of them zero-terminated, after
	 *                 which the next name begins. */
	/* TODO: No support for changing non-vehicle text. Perhaps we shouldn't
	 * implement it at all, but it could be useful for some "modpacks"
	 * (completely new scenarios changing all graphics and logically also
	 * factory names etc). We should then also support all languages (by
	 * name), not only the original four ones. --pasky */
	/* TODO: Support for custom station class/type names. */

	uint8 feature;
	uint8 lang;
	uint8 id;
	uint8 endid;
	const char* name;

	check_length(len, 6, "VehicleNewName");
	feature = buf[1];
	lang = buf[2];
	id = buf[4] + _vehshifts[feature];
	endid = id + buf[3];

	DEBUG(grf, 6) ("VehicleNewName: About to rename engines %d..%d (feature %d) in language 0x%x.",
	               id, endid, feature, lang);

	if (lang & 0x80) {
		grfmsg(GMS_WARN, "VehicleNewName: No support for changing in-game texts. Skipping.");
		return;
	}

	if (!(lang & 3)) {
		/* XXX: If non-English name, silently skip it. */
		DEBUG(grf, 7) ("VehicleNewName: Skipping non-English name.");
		return;
	}

	name = (const char*)(buf + 5);
	len -= 5;
	for (; id < endid && len > 0; id++) {
		int ofs = strlen(name) + 1;

		if (ofs < 128) {
			DEBUG(grf, 8) ("VehicleNewName: %d <- %s", id, name);
			SetCustomEngineName(id, name);
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
	 * B num-sprites   How many sprites are in this set?
	 * V other data    Graphics type specific data.  Currently unused. */
	/* TODO */

	uint8 type;
	uint8 num;

	check_length(len, 2, "GraphicsNew");
	type = buf[0];
	num = buf[1];

	grfmsg(GMS_NOTICE, "GraphicsNew: Custom graphics (type %x) sprite block of length %d (unimplemented, ignoring).\n",
	       type, num);
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
			param_val = 1;
			break;
		case 0x8E:
			param_val = _traininfo_vehicle_pitch;
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

	DEBUG(grf, 7) ("Test condtype %d, param %x, condval %x", condtype, param_val, cond_val);
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

	check_length(len, 9, "GRFInfo");
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
	if ((severity & 0x80) == 0 && _cur_stage == 0)
		return;
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


static void InitializeGRFSpecial(void)
{
	/* FIXME: We should rather reflect reality in _ttdpatch_flags[]. */

	_ttdpatch_flags[0] = (1 << 0x1B); /* multihead */
	_ttdpatch_flags[1] = (1 << 0x08)  /* mammothtrains */
	                   | (1 << 0x0B)  /* subsidiaries */
	                   | (1 << 0x14)  /* bridgespeedlimits */
	                   | (1 << 0x16)  /* eternalgame */
	                   | (1 << 0x17)  /* newtrains */
	                   | (1 << 0x18)  /* newrvs */
	                   | (1 << 0x19)  /* newships */
	                   | (1 << 0x1A)  /* newplanes */
			               | (1 << 0x1B); /* signalsontrafficside */
	                   /* Uncomment following if you want to fool the GRF file.
	                    * Some GRF files will refuse to load without this
	                    * but you can still squeeze something from them even
	                    * without the support - i.e. USSet. --pasky */
			               //| (1 << 0x1C); /* electrifiedrailway */

	_ttdpatch_flags[2] = (1 << 0x0D)  /* buildonslopes */
	                   | (1 << 0x16)  /* canals */
	                   | (1 << 0x17); /* newstartyear */
}

static void InitNewGRFFile(const char* filename, int sprite_offset)
{
	GRFFile *newfile;

	newfile = GetFileByFilename(filename);
	if (newfile != NULL) {
		/* We already loaded it once. */
		newfile->sprite_offset = sprite_offset;
		_cur_grffile = newfile;
		return;
	}

	newfile = calloc(1, sizeof(*newfile));

	if (newfile == NULL)
		error ("Out of memory");

	newfile->filename = strdup(filename);
	newfile->sprite_offset = sprite_offset;

	if (_first_grffile == NULL) {
		_cur_grffile = newfile;
		_first_grffile = newfile;
	} else {
		_cur_grffile->next = newfile;
		_cur_grffile = newfile;
	}
}


/* Here we perform initial decoding of some special sprites (as are they
 * described at http://www.ttdpatch.net/src/newgrf.txt, but this is only a very
 * partial implementation yet). */
/* XXX: We consider GRF files trusted. It would be trivial to exploit OTTD by
 * a crafted invalid GRF file. We should tell that to the user somehow, or
 * better make this more robust in the future. */
static void DecodeSpecialSprite(const char* filename, uint num, uint stage)
{
	/* XXX: There is a difference between staged loading in TTDPatch and
	 * here.  In TTDPatch, for some reason actions 1 and 2 are carried out
	 * during stage 0, whilst action 3 is carried out during stage 1 (to
	 * "resolve" cargo IDs... wtf). This is a little problem, because cargo
	 * IDs are valid only within a given set (action 1) block, and may be
	 * overwritten after action 3 associates them. But overwriting happens
	 * in an earlier stage than associating, so...  We just process actions
	 * 1 and 2 in stage 1 now, let's hope that won't get us into problems.
	 * --pasky */
	uint32 action_mask = (stage == 0) ? 0x0001FF40 : 0x0001FFBF;
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
		/* 0x10 */ NULL  // TODO implement
	};

	byte* buf = malloc(num);
	byte action;

	if (buf == NULL) error("DecodeSpecialSprite: Could not allocate memory");

	FioReadBlock(buf, num);
	action = buf[0];

	if (action >= lengthof(handlers)) {
		DEBUG(grf, 7) ("Skipping unknown action 0x%02X", action);
		free(buf);
		return;
	}

	if (!HASBIT(action_mask, action)) {
		DEBUG(grf, 7) ("Skipping action 0x%02X in stage %d", action, stage);
		free(buf);
		return;
	}

	if (handlers[action] == NULL) {
		DEBUG(grf, 7) ("Skipping unsupported Action 0x%02X", action);
		free(buf);
		return;
	}

	DEBUG(grf, 7) ("Handling action 0x%02X in stage %d", action, stage);
	handlers[action](buf, num);
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
		if (!(_cur_grffile->flags & 0x0001)) return;
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

	while ((num = FioReadWord()) != 0) {
		byte type = FioReadByte();

		if (type == 0xFF) {
			if (_skip_sprites == 0) {
				DecodeSpecialSprite(filename, num, stage);
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
	static bool initialized = false; // XXX yikes
	uint stage;

	if (!initialized) {
		InitializeGRFSpecial();
		initialized = true;
	}

	/* Load newgrf sprites
	 * in each loading stage, (try to) open each file specified in the config
	 * and load information from it. */
	_custom_sprites_base = load_index;
	for (stage = 0; stage < 2; stage++) {
		uint j;

		_cur_stage = stage;
		_cur_spriteid = load_index;
		for (j = 0; j != lengthof(_newgrf_files) && _newgrf_files[j] != NULL; j++) {
			if (!FiosCheckFileExists(_newgrf_files[j])) {
				// TODO: usrerror()
				error("NewGRF file missing: %s", _newgrf_files[j]);
			}
			if (stage == 0) InitNewGRFFile(_newgrf_files[j], _cur_spriteid);
			LoadNewGRFFile(_newgrf_files[j], file_index++, stage); // XXX different file indices in both stages?
			DEBUG(spritecache, 2) ("Currently %i sprites are loaded", load_index);
		}
	}
}
