#include "stdafx.h"

#include <stdarg.h>

#include "ttd.h"
#include "gfx.h"
#include "fileio.h"
#include "engine.h"
#include "station.h"

/* TTDPatch extended GRF format codec
 * (c) Petr Baudis 2004 (GPL'd)
 * Changes by Florian octo Forster are (c) by the OpenTTD development team.
 *
 * Contains portions of documentation by TTDPatch team.
 * Thanks especially to Josef Drexler for the documentation as well as a lot
 * of help at #tycoon. Also thanks to Michael Blunck for is GRF files which
 * served as subject to the initial testing of this codec. */

extern int _skip_sprites;
extern int _replace_sprites_count[16];
extern int _replace_sprites_offset[16];

struct StationSpec {
	uint32 classid;
	byte tiles;
	DrawTileSprites renderdata[8];
};

struct GRFFile {
	char *filename;
	uint32 grfid;
	uint16 flags;
	uint16 sprite_offset;
	struct GRFFile *next;

	/* A sprite group contains all sprites of a given vehicle (or multiple
	 * vehicles) when carrying given cargo. It consists of several sprite
	 * sets.  Group ids are refered as "cargo id"s by TTDPatch
	 * documentation, contributing to the global confusion.
	 *
	 * A sprite set contains all sprites of a given vehicle carrying given
	 * cargo at a given *stage* - that is usually its load stage. Ie. you
	 * can have a spriteset for an empty wagon, wagon full of coal,
	 * half-filled wagon etc.  Each spriteset contains eight sprites (one
	 * per direction) or four sprites if the vehicle is symmetric. */

	int spriteset_start;
	int spriteset_numsets;
	int spriteset_numents;
	int spriteset_feature;

	int spritegroups_count;
	struct SpriteGroup *spritegroups;

	struct StationSpec stations[256];
};

static struct GRFFile *_cur_grffile, *_first_grffile;
static int _cur_spriteid;
static int _cur_stage;

static int32 _paramlist[0x7f];
static int _param_max;

/* 32 * 8 = 256 flags. Apparently TTDPatch uses this many.. */
static uint32 _ttdpatch_flags[8];


enum grfspec_feature {
	GSF_TRAIN,
	GSF_ROAD,
	GSF_SHIP,
	GSF_AIRCRAFT,
	GSF_STATION,
	GSF_BRIDGE,
	GSF_TOWNHOUSE,
};


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


enum grfmsg_severity {
	GMS_NOTICE,
	GMS_WARN,
	GMS_ERROR,
	GMS_FATAL,
};

static void CDECL grfmsg(enum grfmsg_severity severity, const char *str, ...)
{
	static const char * const severitystr[4] = { "Notice", "Warning", "Error", "Fatal" };
	int export_severity = 0;
	char buf[1024];
	va_list va;

	va_start(va, str);
	vsprintf(buf, str, va);
	va_end(va);

	export_severity = 2 - (severity == GMS_FATAL ? 2 : severity);
	DEBUG(grf, export_severity) ("[%s][%s] %s", _cur_grffile->filename, severitystr[severity], buf);
}


#define check_length(real, wanted, where) \
do { \
	if (real < wanted) { \
		grfmsg(GMS_ERROR, "%s: Invalid special sprite length %d (expected %d)!", where, real, wanted); \
		return; \
	} \
} while (0)


static byte INLINE grf_load_byte(byte **buf)
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

static uint16 grf_load_dword(byte **buf)
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


static struct GRFFile *GetFileByGRFID(uint32 grfid)
{
	struct GRFFile *file;

	file = _first_grffile;
	while ((file != NULL) && (file->grfid != grfid))
		file = file->next;

	return file;
}

static struct GRFFile *GetFileByFilename(const char *filename)
{
	struct GRFFile *file;

	file = _first_grffile;
	while ((file != NULL) && strcmp(file->filename, filename))
		file = file->next;

	return file;
}


typedef bool (*VCI_Handler)(uint engine, int numinfo, int prop, byte **buf, int len);

#define FOR_EACH_ENGINE for (i = 0; i < numinfo; i++)

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
		case 0x05: {	/* Track type */
			FOR_EACH_ENGINE {
				uint8 tracktype = grf_load_byte(&buf);

				ei[i].railtype_climates &= 0xf;
				ei[i].railtype_climates |= tracktype << 4;
			}
		}	break;
		case 0x08: {	/* AI passenger service */
			/* TODO */
			FOR_EACH_ENGINE {
				grf_load_byte(&buf);
			}
			ret = true;
		}	break;
		case 0x09: {	/* Speed */
			FOR_EACH_ENGINE {
				uint16 speed = grf_load_word(&buf);

				rvi[i].max_speed = speed;
				dewagonize(speed, engine + i);
			}
		}	break;
		case 0x0B: {	/* Power */
			FOR_EACH_ENGINE {
				uint16 power = grf_load_word(&buf);

				rvi[i].power = power;
				dewagonize(power, engine + i);
			}
		}	break;
		case 0x0D: {	/* Running cost factor */
			FOR_EACH_ENGINE {
				uint8 runcostfact = grf_load_byte(&buf);

				rvi[i].running_cost_base = runcostfact;
				dewagonize(runcostfact, engine + i);
			}
		}	break;
		case 0x0E: {	/* Running cost base */
			FOR_EACH_ENGINE {
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
		}	break;
		case 0x12: {	/* Sprite ID */
			FOR_EACH_ENGINE {
				uint8 spriteid = grf_load_byte(&buf);

				if (spriteid == 0xFD && rvi[i].image_index != 0xFD)
					_engine_original_sprites[engine + i] = rvi[i].image_index;
				rvi[i].image_index = spriteid;
			}
		}	break;
		case 0x13: {	/* Dual-headed */
			FOR_EACH_ENGINE {
				uint8 dual = grf_load_byte(&buf);

				if (dual != 0) {
					rvi[i].flags |= 1;
				} else {
					rvi[i].flags &= ~1;
				}
			}
		}	break;
		case 0x14: {	/* Cargo capacity */
			FOR_EACH_ENGINE {
				uint8 capacity = grf_load_byte(&buf);

				rvi[i].capacity = capacity;
			}
		}	break;
		case 0x15: {	/* Cargo type */
			FOR_EACH_ENGINE {
				uint8 ctype = grf_load_byte(&buf);

				rvi[i].cargo_type = ctype;
			}
		}	break;
		case 0x16: {	/* Weight */
			FOR_EACH_ENGINE {
				uint8 weight = grf_load_byte(&buf);

				rvi[i].weight = weight;
			}
		}	break;
		case 0x17: {	/* Cost factor */
			FOR_EACH_ENGINE {
				uint8 cfactor = grf_load_byte(&buf);

				rvi[i].base_cost = cfactor;
			}
		}	break;
		case 0x18: {	/* AI rank */
			/* TODO: _railveh_score should be merged to _rail_vehicle_info. */
			FOR_EACH_ENGINE {
				grf_load_byte(&buf);
			}
			ret = true;
		}	break;
		case 0x19: { /* Engine traction type */
			/* TODO: What do the individual numbers mean?
			 * XXX: And in what base are they, in fact? --pasky */
			FOR_EACH_ENGINE {
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
		}	break;

		/* TODO */
		/* Fall-through for unimplemented four bytes long properties. */
		case 0x1D:	/* Refit cargo */
			FOR_EACH_ENGINE {
				grf_load_word(&buf);
			}
		/* Fall-through for unimplemented two bytes long properties. */
		case 0x1B:	/* Powered wagons power bonus */
			FOR_EACH_ENGINE {
				grf_load_byte(&buf);
			}
		/* Fall-through for unimplemented one byte long properties. */
		case 0x1A:	/* Sort order */
		case 0x1C:	/* Refit cost */
		case 0x1E:	/* Callback */
		case 0x1F:	/* Tractive effort */
		case 0x21:	/* Shorter tenders */
		case 0x22:	/* Visual */
		case 0x23: {/* Powered wagons weight bonus */
			/* TODO */
			FOR_EACH_ENGINE {
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
		case 0x08: {	/* Speed */
			FOR_EACH_ENGINE {
				uint8 speed = grf_load_byte(&buf);

				rvi[i].max_speed = speed; // ?? units
			}
		}	break;
		case 0x09: {	/* Running cost factor */
			FOR_EACH_ENGINE {
				uint8 runcost = grf_load_byte(&buf);

				rvi[i].running_cost = runcost;
			}
		}	break;
		case 0x0A:	/* Running cost base */
			/* TODO: I have no idea. --pasky */
			FOR_EACH_ENGINE {
				grf_load_byte(&buf);
			}
			ret = true;
			break;
		case 0x0E: {	/* Sprite ID */
			FOR_EACH_ENGINE {
				uint8 spriteid = grf_load_byte(&buf);

				if (spriteid == 0xFF)
					spriteid = 0xFD; // cars have different custom id in the GRF file

				// This is currently not used but there's no reason
				// in not having it here for the future.
				if (spriteid == 0xFD && rvi[i].image_index != 0xFD)
					_engine_original_sprites[ROAD_ENGINES_INDEX + engine + i] = rvi[i].image_index;

				rvi[i].image_index = spriteid;
			}
		}	break;
		case 0x0F: {	/* Cargo capacity */
			FOR_EACH_ENGINE {
				uint16 capacity = grf_load_word(&buf);

				rvi[i].capacity = capacity;
			}
		}	break;
		case 0x10: { /* Cargo type */
			FOR_EACH_ENGINE {
				uint8 cargo = grf_load_byte(&buf);

				rvi[i].cargo_type = cargo;
			}
		}	break;
		case 0x11: {	/* Cost factor */
			FOR_EACH_ENGINE {
				uint8 cost_factor = grf_load_byte(&buf);

				rvi[i].base_cost = cost_factor; // ?? is it base_cost?
			}
		}	break;
		case 0x12: {	/* SFX */
			FOR_EACH_ENGINE {
				uint8 sfx = grf_load_byte(&buf);

				rvi[i].sfx = sfx;
			}
		}	break;
		case 0x13:      /* Power in 10hp */
		case 0x14:      /* Weight in 1/4 tons */
		case 0x15:      /* Speed in mph*0.8 */
			/* TODO: Support for road vehicles realistic power
			 * computations (called rvpower in TTDPatch) is just
			 * missing in OTTD yet. --pasky */
			FOR_EACH_ENGINE {
				grf_load_byte(&buf);
			}
			ret = true;
			break;
		case 0x16: {	/* Cargos available for refitting */
			FOR_EACH_ENGINE {
				uint32 refit_mask = grf_load_dword(&buf);

				_engine_refit_masks[ROAD_ENGINES_INDEX + engine + i] = refit_mask;
			}
		}	break;
		case 0x17:      /* Callback */
		case 0x18:      /* Tractive effort */
			/* TODO */
			FOR_EACH_ENGINE {
				grf_load_byte(&buf);
			}
			ret = true;
			break;
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
			FOR_EACH_ENGINE {
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
			FOR_EACH_ENGINE {
				uint8 refittable = grf_load_byte(&buf);

				svi[i].refittable = refittable;
			}
		}	break;
		case 0x0A: {	/* Cost factor */
			FOR_EACH_ENGINE {
				uint8 cost_factor = grf_load_byte(&buf);

				svi[i].base_cost = cost_factor; // ?? is it base_cost?
			}
		}	break;
		case 0x0B: {	/* Speed */
			FOR_EACH_ENGINE {
				uint8 speed = grf_load_byte(&buf);

				svi[i].max_speed = speed; // ?? units
			}
		}	break;
		case 0x0C: { /* Cargo type */
			FOR_EACH_ENGINE {
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
			FOR_EACH_ENGINE {
				uint16 capacity = grf_load_word(&buf);

				svi[i].capacity = capacity;
			}
		}	break;
		case 0x0F: {	/* Running cost factor */
			FOR_EACH_ENGINE {
				uint8 runcost = grf_load_byte(&buf);

				svi[i].running_cost = runcost;
			}
		} break;
		case 0x10: {	/* SFX */
			FOR_EACH_ENGINE {
				uint8 sfx = grf_load_byte(&buf);

				svi[i].sfx = sfx;
			}
		}	break;
		case 0x11: {	/* Cargos available for refitting */
			FOR_EACH_ENGINE {
				uint32 refit_mask = grf_load_dword(&buf);

				_engine_refit_masks[SHIP_ENGINES_INDEX + engine + i] = refit_mask;
			}
		}	break;
		case 0x12: { /* Callback TODO */
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
			FOR_EACH_ENGINE {
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
			FOR_EACH_ENGINE {
				uint8 heli = grf_load_byte(&buf);

				avi[i].subtype = (heli == 0) ? 0 : 1;
			}
		}	break;
		case 0x0A: {	/* Large */
			FOR_EACH_ENGINE {
				uint8 large = grf_load_byte(&buf);

				avi[i].subtype = (large == 1) ? 3 : 1;
			}
		}	break;
		case 0x0B: {	/* Cost factor */
			FOR_EACH_ENGINE {
				uint8 cost_factor = grf_load_byte(&buf);

				avi[i].base_cost = cost_factor; // ?? is it base_cost?
			}
		}	break;
		case 0x0C: {	/* Speed */
			FOR_EACH_ENGINE {
				uint8 speed = grf_load_byte(&buf);

				avi[i].max_speed = speed; // ?? units
			}
		}	break;
		case 0x0D: {	/* Acceleration */
			FOR_EACH_ENGINE {
				uint8 accel = grf_load_byte(&buf);

				avi[i].acceleration = accel;
			}
		} break;
		case 0x0E: {	/* Running cost factor */
			FOR_EACH_ENGINE {
				uint8 runcost = grf_load_byte(&buf);

				avi[i].running_cost = runcost;
			}
		} break;
		case 0x0F: {	/* Passenger capacity */
			FOR_EACH_ENGINE {
				uint16 capacity = grf_load_word(&buf);

				avi[i].passanger_capacity = capacity;
			}
		}	break;
		case 0x11: {	/* Mail capacity */
			FOR_EACH_ENGINE {
				uint8 capacity = grf_load_byte(&buf);

				avi[i].mail_capacity = capacity;
			}
		}	break;
		case 0x12: {	/* SFX */
			FOR_EACH_ENGINE {
				uint8 sfx = grf_load_byte(&buf);

				avi[i].sfx = sfx;
			}
		}	break;
		case 0x13: {	/* Cargos available for refitting */
			FOR_EACH_ENGINE {
				uint32 refit_mask = grf_load_dword(&buf);

				_engine_refit_masks[AIRCRAFT_ENGINES_INDEX + engine + i] = refit_mask;
			}
		}	break;
		case 0x14: { /* Callback TODO */
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

	/* TODO: Differentiate between railtypes. This is missing in the new
	 * GRF file specification yet, though, so I need to agree on this with
	 * patchman yet. We just assume all the station stuff is for railtype 0
	 * (railroad) for now. --pasky */

	//printf("sci %d %d [0x%02x]\n", stid, numinfo, prop);
	switch (prop) {
		case 0x08:
		{	/* Class ID */
			FOR_EACH_ENGINE {
				struct StationSpec *stat = &_cur_grffile->stations[stid + i];

				/* classid, for a change, is always little-endian */
				stat->classid = *(buf++) << 24;
				stat->classid |= *(buf++) << 16;
				stat->classid |= *(buf++) << 8;
				stat->classid |= *(buf++);
			}
			break;
		}
		case 0x09:
		{	/* Define sprite layout */
			FOR_EACH_ENGINE {
				struct StationSpec *stat = &_cur_grffile->stations[stid + i];
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
						static const DrawTileSeqStruct empty = {0x80};
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
			FOR_EACH_ENGINE {
				struct StationSpec *stat = &_cur_grffile->stations[stid + i];
				byte srcid = grf_load_byte(&buf);
				struct StationSpec *srcstat = &_cur_grffile->stations[srcid];
				int t;

				stat->tiles = srcstat->tiles;
				for (t = 0; t < stat->tiles; t++) {
					DrawTileSprites *dts = &stat->renderdata[t];
					DrawTileSprites *sdts = &srcstat->renderdata[t];
					DrawTileSeqStruct const *sdtss = sdts->seq;
					int seq_count = 0;

					dts->ground_sprite = sdts->ground_sprite;
					if (!dts->ground_sprite) {
						static const DrawTileSeqStruct empty = {0x80};
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
			FOR_EACH_ENGINE {
				grf_load_byte(&buf);
			}
			ret = 1;
			break;
		}
		case 0x0c:
		{	/* Platforms number */
			/* TODO */
			FOR_EACH_ENGINE {
				grf_load_byte(&buf);
			}
			ret = 1;
			break;
		}
		case 0x0d:
		{	/* Platforms length */
			/* TODO */
			FOR_EACH_ENGINE {
				grf_load_byte(&buf);
			}
			ret = 1;
			break;
		}
		case 0x0e:
		{	/* Define custom layout */
			/* TODO */
			FOR_EACH_ENGINE {
				while (buf < *bufp + len) {
					byte length = grf_load_byte(&buf);
					byte number = grf_load_byte(&buf);
					int k = length * number;

					if (!length && !number) break;
					while (k--) grf_load_byte(&buf);
				}
			}
			ret = 1;
			break;
		}
		case 0x0f:
		{	/* Copy custom layout */
			/* TODO */
			FOR_EACH_ENGINE {
				grf_load_byte(&buf);
			}
			ret = 1;
			break;
		}
		case 0x10:
		{	/* Little/lots cargo threshold */
			/* TODO */
			FOR_EACH_ENGINE {
				grf_load_word(&buf);
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

#undef shift_buf


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
	EngineInfo *ei;

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

	buf += 5;

	while (numprops-- && buf < bufend) {
		uint8 prop = grf_load_byte(&buf);

		if (feature == 4)
			// stations don't share those common properties
			goto run_handler;

		switch (prop) {
		case 0x00: { /* Introduction date */
			FOR_EACH_ENGINE {
				uint16 date = grf_load_word(&buf);

				ei[i].base_intro = date;
			}
		}	break;
		case 0x02: { /* Decay speed */
			FOR_EACH_ENGINE {
				uint8 decay = grf_load_byte(&buf);

				ei[i].unk2 &= 0x80;
				ei[i].unk2 |= decay & 0x7f;
			}
		}	break;
		case 0x03: { /* Vehicle life */
			FOR_EACH_ENGINE {
				uint8 life = grf_load_byte(&buf);

				ei[i].lifelength = life;
			}
		}	break;
		case 0x04: { /* Model life */
			FOR_EACH_ENGINE {
				uint8 life = grf_load_byte(&buf);

				ei[i].base_life = life;
			}
		}	break;
		case 0x06: { /* Climates available */
			FOR_EACH_ENGINE {
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
			FOR_EACH_ENGINE {
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
#undef shift_buf
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

	check_length(len, 4, "NewSpriteSet");
	feature = buf[1];

	_cur_grffile->spriteset_start = _cur_spriteid + 1;
	_cur_grffile->spriteset_feature = feature;
	_cur_grffile->spriteset_numsets = buf[2];
	_cur_grffile->spriteset_numents = buf[3];
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
	struct SpriteGroup *group;
	byte *loaded_ptr;
	byte *loading_ptr;
	int i;

	check_length(len, 5, "NewSpriteGroup");
	feature = buf[1];
	setid = buf[2];
	numloaded = buf[3];
	numloading = buf[4];

	if (numloaded == 0x81) {
		/* XXX: This just goes for the default superset for now,
		 * straight and safe. --pasky */
		uint8 var = buf[4];
		//uint8 shiftnum = buf[5];
		//uint8 andmask = buf[6];
		uint8 nvar = buf[7];
		//uint32 val;
		uint16 def;

		grfmsg(GMS_WARN, "NewSpriteGroup(0x81): Unsupported variable %x. Using default cid.", var);

		//val = (0xff << shiftnum) & andmask;

		if (setid >= _cur_grffile->spritegroups_count) {
			_cur_grffile->spritegroups_count = setid + 1;
			_cur_grffile->spritegroups = realloc(_cur_grffile->spritegroups, _cur_grffile->spritegroups_count * sizeof(struct SpriteGroup));
		}
		buf += 8 + nvar * 4;
		def = grf_load_word(&buf);
		_cur_grffile->spritegroups[setid] = _cur_grffile->spritegroups[def];
		return;

	} else if (numloaded & 0x80) {
		grfmsg(GMS_WARN, "NewSpriteGroup(0x%x): Unsupported special group.", numloaded);
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
		_cur_grffile->spritegroups = realloc(_cur_grffile->spritegroups, _cur_grffile->spritegroups_count * sizeof(struct SpriteGroup));
	}
	group = &_cur_grffile->spritegroups[setid];
	memset(group, 0, sizeof(struct SpriteGroup));
	group->sprites_per_set = _cur_grffile->spriteset_numents;
	group->loaded_count  = numloaded;
	group->loading_count = numloading;

	DEBUG(grf, 6) ("NewSpriteGroup: New SpriteGroup 0x%02hhx, %u views, %u loaded, %u loading, sprites %u - %u",
			setid, group->sprites_per_set, group->loaded_count, group->loading_count,
			_cur_grffile->spriteset_start - _cur_grffile->sprite_offset,
			_cur_grffile->spriteset_start + (_cur_grffile->spriteset_numents * (numloaded + numloading)) - _cur_grffile->sprite_offset);

	for (i = 0; i < numloaded; i++) {
		uint16 spriteset_id = grf_load_word(&loaded_ptr);
		group->loaded[i] = _cur_grffile->spriteset_start + spriteset_id * _cur_grffile->spriteset_numents;
		DEBUG(grf, 8) ("NewSpriteGroup: + group->loaded[%i]  = %u (subset %u)", i, group->loaded[i], spriteset_id);
	}

	for (i = 0; i < numloading; i++) {
		uint16 spriteset_id = grf_load_word(&loading_ptr);
		group->loading[i] = _cur_grffile->spriteset_start + spriteset_id * _cur_grffile->spriteset_numents;
		DEBUG(grf, 8) ("NewSpriteGroup: + group->loading[%i] = %u (subset %u)", i, group->loading[i], spriteset_id);
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

		/* XXX: Currently we don't support cargo-specific images, so
		 * we go straight to the defaults. */
		byte *bp = buf + 4 + idcount + cidcount * 3;
		uint16 groupid = grf_load_word(&bp);

		for (i = 0; i < idcount; i++) {
			struct StationSpec *stat;
			uint8 stid = buf[3 + i];
			int j;

			if (groupid >= _cur_grffile->spritegroups_count) {
				grfmsg(GMS_WARN, "VehicleMapSpriteGroup: Spriteset %x out of range %x, skipping.",
				       groupid, _cur_grffile->spritegroups_count);
				return;
			}

			stat = &_cur_grffile->stations[stid];

			// relocate sprite indexes based on spriteset locations
			for (j = 0; j < stat->tiles; j++) {
				DrawTileSeqStruct *seq;

				foreach_draw_tile_seq(seq, (DrawTileSeqStruct*) stat->renderdata[j].seq) {
					seq->image += _cur_grffile->spritegroups[groupid].loading[0];
				}
			}
			/* FIXME: This means several GRF files defining new stations
			 * will override each other, but the stid should be GRF-specific
			 * instead! --pasky */
			SetCustomStation(stat->classid, stid, stat->renderdata, stat->tiles);
			stat->classid = 0;
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

			if (ctype == 0xFF)
				ctype = CID_PURCHASE;

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
				SetCustomEngineSprites(engine, CID_DEFAULT, &_cur_grffile->spritegroups[groupid]);
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

	buf += 5, len -= 5;
	for (; id < endid && len > 0; id++) {
		int ofs = strlen(buf) + 1;

		if (ofs < 128) {
			DEBUG(grf, 8) ("VehicleNewName: %d <- %s", id, buf);
			SetCustomEngineName(id, buf);
		} else {
			DEBUG(grf, 7) ("VehicleNewName: Too long a name (%d)", ofs);
		}
		buf += ofs, len -= ofs;
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
	int param_val = 0, cond_val = 0;
	bool result;

	check_length(len, 6, "SkipIf");
	param = buf[1];
	paramsize = buf[2];
	condtype = buf[3];

	if (condtype < 2) {
		/* Always 1 for bit tests, the given value should
		 * be ignored. */
		paramsize = 1;
	}

	buf += 4;
	if (paramsize == 4)
		cond_val = grf_load_dword(&buf);
	else if (paramsize == 2)
		cond_val = grf_load_word(&buf);
	else if (paramsize == 1)
		cond_val = grf_load_byte(&buf);

	switch (param) {
		case 0x83:
			param_val = _opt.landscape;
			break;
		case 0x84:
			param_val = _cur_stage;
			break;
		case 0x85:
			param_val = _ttdpatch_flags[cond_val / 0x20];
			cond_val %= 0x20;
			break;
		case 0x86:
			param_val = _opt.road_side << 4;
			break;
		case 0x88: {
			struct GRFFile *file;

			file = GetFileByGRFID(cond_val);
			param_val = (file != NULL);
		}	break;
		default:
			if (param >= 0x80) {
				/* In-game variable. */
				grfmsg(GMS_WARN, "Unsupported in-game variable %x. Ignoring test.", param);
			} else {
				/* Parameter. */
				param_val = _paramlist[param];
			}
			return;
	}

	switch (condtype) {
		case 0: result = (param_val & (1 << cond_val));
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
		case 6: result = param_val; /* GRFID is active (only for param-num=88) */
			break;
		case 7: result = !param_val; /* GRFID is not active (only for param-num=88) */
			break;
		default:
			grfmsg(GMS_WARN, "Unsupported test %d. Ignoring.", condtype);
			return;
	}

	if (result == 0) {
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
	char *name;
	char *info;

	check_length(len, 9, "GRFInfo");
	version = buf[1];
	/* this is de facto big endian - grf_load_dword() unsuitable */
	grfid = buf[2] << 24 | buf[3] << 16 | buf[4] << 8 | buf[5];
	name = buf + 6;
	info = name + strlen(name) + 1;

	_cur_grffile->grfid = grfid;
	_cur_grffile->flags |= 0x0001; /* set active flag */

	DEBUG(grf, 1) ("[%s] Loaded GRFv%d set %08lx - %s:\n%s\n",
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
	int i;

	buf++; /* skip action byte */
	num_sets = grf_load_byte(&buf);

	if (num_sets > 16) {
		grfmsg(GMS_ERROR, "SpriteReplace: Too many sets (%d), taking only the first 16!", num_sets);
	}

	for (i = 0; i < 16; i++) {
		if (i < num_sets) {
			uint8 num_sprites = grf_load_byte(&buf);
			uint16 first_sprite = grf_load_word(&buf);

			_replace_sprites_count[i] = num_sprites;
			_replace_sprites_offset[i] = first_sprite;
			grfmsg(GMS_NOTICE, "SpriteReplace: [Set %d] Changing %d sprites, beginning with %d",
					i, num_sprites, first_sprite);
		} else {
			_replace_sprites_count[i] = 0;
			_replace_sprites_offset[i] = 0;
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
	uint16 src1;
	uint16 src2;
	uint16 data = 0;

	check_length(len, 5, "ParamSet");
	target = grf_load_byte(&buf);
	oper = grf_load_byte(&buf);
	src1 = grf_load_byte(&buf);
	src2 = grf_load_byte(&buf);

	if (len >= 8)
		data = grf_load_dword(&buf);

	/* You can add 80 to the operation to make it apply only if the target
	 * is not defined yet.  In this respect, a parameter is taken to be
	 * defined if any of the following applies:
	 * - it has been set to any value in the newgrf(w).cfg parameter list
	 * - it OR A PARAMETER WITH HIGHER NUMBER has been set to any value by
	 *   an earlier action D */
	if (oper & 0x80) {
		if (_param_max < target)
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
		src1 = _param_max >= src1 ? _paramlist[src1] : 0;
	}

	if (src2 == 0xFF) {
		src2 = data;
	} else {
		src2 = _param_max >= src2 ? _paramlist[src2] : 0;
	}

	/* TODO: You can access the parameters of another GRF file by using
	 * source2=FE, source1=the other GRF's parameter number and data=GRF
	 * ID.  This is only valid with operation 00 (set).  If the GRF ID
	 * cannot be found, a value of 0 is used for the parameter value
	 * instead. */

	/* TODO: The target operand can also refer to the special variables
	 * from action 7, but at the moment the only variable that is valid to
	 * write is 8E. */

	if (_param_max < target)
		_param_max = target;

	/* FIXME: No checking for overflows. */
	switch (oper) {
		case 0x00:
			_paramlist[target] = src1;
			break;
		case 0x01:
			_paramlist[target] = src1 + src2;
			break;
		case 0x02:
			_paramlist[target] = src1 - src2;
			break;
		case 0x03:
			_paramlist[target] = ((uint32) src1) * ((uint32) src2);
			break;
		case 0x04:
			_paramlist[target] = ((int32) src1) * ((int32) src2);
			break;
		case 0x05:
			if (src2 & 0x8000) /* src2 is "negative" */
				_paramlist[target] = src1 >> -((int16) src2);
			else
				_paramlist[target] = src1 << src2;
			break;
		case 0x06:
			if (src2 & 0x8000) /* src2 is "negative" */
				_paramlist[target] = ((int16) src1) >> -((int16) src2);
			else
				_paramlist[target] = ((int16) src1) << src2;
			break;
		default:
			grfmsg(GMS_ERROR, "ParamSet: Unknown operation %d, skipping.", oper);
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
	num = grf_load_byte(&buf); len--;
	check_length(len, 4 * num, "GRFInhibit");

	for (i = 0; i < num; i++) {
		uint32 grfid = grf_load_dword(&buf);
		struct GRFFile *file = GetFileByGRFID(grfid);

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

	_ttdpatch_flags[1] = (1 << 0x08)	/* mammothtrains */
		| (1 << 0x0B)			/* subsidiaries */
		| (1 << 0x14)			/* bridgespeedlimits */
		| (1 << 0x16)			/* eternalgame */
		| (1 << 0x17)			/* newtrains */
		| (1 << 0x18)			/* newrvs */
		| (1 << 0x19)			/* newships */
		| (1 << 0x1A);		/* newplanes */

	_ttdpatch_flags[2] = (1 << 0x0D)	/* signalsontrafficside */
		| (1 << 0x16)			/* canals */
		| (1 << 0x17);		/* newstartyear */
}

void InitNewGRFFile(const char *filename, int sprite_offset)
{
	struct GRFFile *newfile;

	newfile = calloc(1, sizeof(struct GRFFile));

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

void DecodeSpecialSprite(const char *filename, int num, int spriteid, int stage)
{
#define NUM_ACTIONS 0xF
	static const SpecialSpriteHandler handlers[NUM_ACTIONS] = {
		/* 0x0 */ VehicleChangeInfo,
		/* 0x1 */ NewSpriteSet,
		/* 0x2 */ NewSpriteGroup,
		/* 0x3 */ NewVehicle_SpriteGroupMapping,
		/* 0x4 */ VehicleNewName,
		/* 0x5 */ GraphicsNew,
		/* 0x6 */ CfgApply,
		/* 0x7 */ SkipIf,
		/* 0x8 */ GRFInfo,
		/* 0x9 */ SkipIf,
		/* 0xa */ SpriteReplace,
		/* 0xb */ GRFError,
		/* 0xc */ GRFComment,
		/* 0xd */ ParamSet,
		/* 0xe */ GRFInhibit,
	};
	static int initialized;
	byte action;
	byte *buf = malloc(num);
	int i;

	if (buf == NULL) error("DecodeSpecialSprite: Could not allocate memory");

	if (initialized == 0) {
		InitializeGRFSpecial();
		initialized = 1;
	}

	_cur_stage = stage;
	_cur_spriteid = spriteid;

	for (i = 0; i != num; i++)
		buf[i] = FioReadByte();

	action = buf[0];

	/* XXX: There is a difference between staged loading in TTDPatch and
	 * here.  In TTDPatch, for some reason actions 1 and 2 are carried out
	 * during stage 0, whilst action 3 is carried out during stage 1 (to
	 * "resolve" cargo IDs... wtf). This is a little problem, because cargo
	 * IDs are valid only within a given set (action 1) block, and may be
	 * overwritten after action 3 associates them. But overwriting happens
	 * in an earlier stage than associating, so...  We just process actions
	 * 1 and 2 in stage 1 now, let's hope that won't get us into problems.
	 * --pasky */

	if (stage == 0) {
		/* During initialization, actions 0, 1, 2, 3, 4, 5 and 7 are ignored. */

		if ((action == 0x00) || (action == 0x01) || (action == 0x02) || (action == 0x03)
		    || (action == 0x04) || (action == 0x05) || (action == 0x07)) {
			DEBUG (grf, 7) ("DecodeSpecialSprite: Action: %x, Stage 0, Skipped", action);
			/* Do nothing. */

		} else if (action < NUM_ACTIONS) {
			DEBUG (grf, 7) ("DecodeSpecialSprite: Action: %x, Stage 0", action);
 			handlers[action](buf, num);

		} else {
			grfmsg(GMS_WARN, "Unknown special sprite action %x, skipping.", action);
		}

	} else if (stage == 1) {
		/* A .grf file is activated only if it was active when the game was
		 * started.  If a game is loaded, only its active .grfs will be
		 * reactivated, unless "loadallgraphics on" is used.  A .grf file is
		 * considered active if its action 8 has been processed, i.e. its
		 * action 8 hasn't been skipped using an action 7.
		 *
		 * During activation, only actions 0, 1, 2, 3, 4, 5, 7, 8, 9 and 0A are
		 * carried out.  All others are ignored, because they only need to be
		 * processed once at initialization.  */

		if ((_cur_grffile == NULL) || strcmp(_cur_grffile->filename, filename))
			_cur_grffile = GetFileByFilename(filename);

		if (_cur_grffile == NULL)
			error("File ``%s'' lost in cache.\n", filename);

		if (!(_cur_grffile->flags & 0x0001)) {
			DEBUG (grf, 7) ("DecodeSpecialSprite: Action: %x, Stage 1, Not activated", action);
			/* Do nothing. */

		} else if ((action == 0x00) || (action == 0x01) || (action == 0x02) || (action == 0x03)
			   || (action == 0x04) || (action == 0x05) || (action == 0x07) || (action == 0x08)
			   || (action == 0x09) || (action == 0x0A)) {
			DEBUG (grf, 7) ("DecodeSpecialSprite: Action: %x, Stage 1", action);
			handlers[action](buf, num);

		} else if (action < NUM_ACTIONS) {
			DEBUG (grf, 7) ("DecodeSpecialSprite: Action: %x, Stage 1, Skipped", action);
			/* Do nothing. */

		} else {
			grfmsg(GMS_WARN, "Unknown special sprite action %x, skipping.", action);
		}

	} else {
		error("Invalid stage %d", stage);
	}

	free(buf);
#undef NUM_ACTIONS
}
