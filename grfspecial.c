#include "stdafx.h"

#include <stdarg.h>

#include "ttd.h"
#include "gfx.h"
#include "fileio.h"
#include "engine.h"

/* TTDPatch extended GRF format codec
 * (c) Petr Baudis 2004 (GPL'd)
 * Changes by Florian octo Forster are (c) by the OpenTTD development team.
 *
 * Contains portions of documentation by TTDPatch team.
 * Thanks especially to Josef Drexler for the documentation as well as a lot
 * of help at #tycoon. Also thanks to Michael Blunck for is GRF files which
 * served as subject to the initial testing of this codec. */

extern int _skip_sprites;

static const char *_cur_grffile;
static int _cur_spriteid;

typedef void (*SpecialSpriteHandler)(byte *buf, int len);

static const int _vehshifts[4] = {
	0,
	ROAD_ENGINES_INDEX,
	SHIP_ENGINES_INDEX,
	AIRCRAFT_ENGINES_INDEX,
};


enum grfmsg_severity {
	GMS_NOTICE,
	GMS_WARN,
	GMS_ERROR,
	GMS_FATAL,
};

static void CDECL grfmsg(enum grfmsg_severity severity, const char *str, ...)
{
	static const char * const severitystr[4] = { "Notice", "Warning", "Error", "Fatal" };
	char buf[1024];
	va_list va;

	va_start(va, str);
	vsprintf(buf, str, va);
	va_end(va);
	DEBUG(grf, 2) ("[%s][%s] %s", _cur_grffile, severitystr[severity], buf);
}


#define check_length(real, wanted, where) \
do { \
	if (real < wanted) { \
		grfmsg(GMS_ERROR, "%s: Invalid special sprite length %d (expected %d)!", where, real, wanted); \
		return; \
	} \
} while (0)


static byte INLINE grf_load_byte(byte **buf) {
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


typedef int (*VCI_Handler)(uint engine, int numinfo, int prop, byte **buf, int len);

#define foreach_engine for (i = 0; i < numinfo; i++)

static void dewagonize(int condition, int engine)
{
	EngineInfo *ei = &_engine_info[engine];
	RailVehicleInfo *rvi = &_rail_vehicle_info[engine];

	if (condition) {
		ei->unk2 &= ~0x80;
		rvi->flags &= ~2;
	} else {
		ei->unk2 |= 0x80;
		rvi->flags |= 2;
	}
}

static int RailVehicleChangeInfo(uint engine, int numinfo, int prop, byte **bufp, int len)
{
	EngineInfo *ei = &_engine_info[engine];
	RailVehicleInfo *rvi = &_rail_vehicle_info[engine];
	byte *buf = *bufp;
	int i;
	int ret = 0;

	switch (prop) {
		case 0x05:
		{	/* Track type */
			foreach_engine {
				uint8 tracktype = grf_load_byte(&buf);

				ei[i].railtype_climates &= 0xf;
				ei[i].railtype_climates |= tracktype << 4;
			}
			break;
		}
		case 0x08:
		{	/* AI passenger service */
			/* TODO */
			foreach_engine {
				grf_load_byte(&buf);
			}
			ret = 1;
			break;
		}
		case 0x09:
		{	/* Speed */
			foreach_engine {
				uint16 speed = grf_load_word(&buf);

				rvi[i].max_speed = speed;
				dewagonize(speed, engine + i);
			}
			break;
		}
		case 0x0b:
		{	/* Power */
			foreach_engine {
				uint16 power = grf_load_word(&buf);

				rvi[i].power = power;
				dewagonize(power, engine + i);
			}
			break;
		}
		case 0x0d:
		{	/* Running cost factor */
			foreach_engine {
				uint8 runcostfact = grf_load_byte(&buf);

				rvi[i].running_cost_base = runcostfact;
				dewagonize(runcostfact, engine + i);
			}
			break;
		}
		case 0x0e:
		{	/* Running cost base */
			foreach_engine {
				uint32 base = grf_load_dword(&buf);

				switch (base) {
				case 0x4c30:
					rvi[i].engclass = 0;
					break;
				case 0x4c36:
					rvi[i].engclass = 1;
					break;
				case 0x4c3c:
					rvi[i].engclass = 2;
					break;
				}
				dewagonize(base, engine + i);
			}
			break;
		}
		case 0x12:
		{	/* Sprite ID */
			foreach_engine {
				uint8 spriteid = grf_load_byte(&buf);

				if (spriteid == 0xfd && rvi[i].image_index != 0xfd)
					_engine_original_sprites[engine + i] = rvi[i].image_index;
				rvi[i].image_index = spriteid;
			}
			break;
		}
		case 0x13:
		{	/* Dual-headed */
			foreach_engine {
				uint8 dual = grf_load_byte(&buf);

				if (dual) {
					rvi[i].flags |= 1;
				} else {
					rvi[i].flags &= ~1;
				}
			}
			break;
		}
		case 0x14:
		{	/* Cargo capacity */
			foreach_engine {
				uint8 capacity = grf_load_byte(&buf);

				rvi[i].capacity = capacity;
			}
			break;
		}
		case 0x15:
		{	/* Cargo type */
			foreach_engine {
				uint8 ctype = grf_load_byte(&buf);

				rvi[i].cargo_type = ctype;
			}
			break;
		}
		case 0x16:
		{	/* Weight */
			foreach_engine {
				uint8 weight = grf_load_byte(&buf);

				rvi[i].weight = weight;
			}
			break;
		}
		case 0x17:
		{	/* Cost factor */
			foreach_engine {
				uint8 cfactor = grf_load_byte(&buf);

				rvi[i].base_cost = cfactor;
			}
			break;
		}
		case 0x18:
		{	/* AI rank */
			/* TODO: _railveh_score should be merged to _rail_vehicle_info. */
			foreach_engine {
				grf_load_byte(&buf);
			}
			ret = 1;
			break;
		}
		case 0x19:
		{	/* Engine traction type */
			/* TODO: What do the individual numbers mean?
			 * XXX: And in what base are they, in fact? --pasky */
			foreach_engine {
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
			break;
		}
		/* TODO */
		/* Fall-through for unimplemented four bytes long properties. */
		case 0x1d:	/* Refit cargo */
			foreach_engine {
				grf_load_word(&buf);
			}
		/* Fall-through for unimplemented two bytes long properties. */
		case 0x1b:	/* Powered wagons power bonus */
			foreach_engine {
				grf_load_byte(&buf);
			}
		/* Fall-through for unimplemented one byte long properties. */
		case 0x1a:	/* Sort order */
		case 0x1c:	/* Refit cost */
		case 0x1e:	/* Callback */
		case 0x1f:	/* Tractive effort */
		case 0x21:	/* Shorter tenders */
		case 0x22:	/* Visual */
		case 0x23:	/* Powered wagons weight bonus */
			/* TODO */
			foreach_engine {
				grf_load_byte(&buf);
			}
			ret = 1;
			break;
		default:
			ret = 1;
			break;
	}
	*bufp = buf;
	return ret;
}

static int ShipVehicleChangeInfo(uint engine, int numinfo, int prop, byte **bufp, int len)
{
	ShipVehicleInfo *svi = &_ship_vehicle_info[engine];
	byte *buf = *bufp;
	int i;
	int ret = 0;

	//printf("e %x prop %x?\n", engine, prop);
	switch (prop) {
		case 0x08:
		{	/* Sprite ID */
			foreach_engine {
				uint8 spriteid = grf_load_byte(&buf);

				if (spriteid == 0xff)
					spriteid = 0xfd; // ships have different custom id in the GRF file

				// This is currently not used but there's no reason
				// in not having it here for the future.
				if (spriteid == 0xfd
				    && svi[i].image_index != 0xfd)
					_engine_original_sprites[SHIP_ENGINES_INDEX
					                         + engine + i]
						= svi[i].image_index;
				svi[i].image_index = spriteid;
			}
			break;
		}
		case 0x09:
		{	/* Refittable */
			foreach_engine {
				uint8 refittable = grf_load_byte(&buf);

				svi[i].refittable = refittable;
			}
			break;
		}
		case 0x0a:
		{	/* Cost factor */
			foreach_engine {
				uint8 cost_factor = grf_load_byte(&buf);

				svi[i].base_cost = cost_factor; // ?? is it base_cost?
			}
			break;
		}
		case 0x0b:
		{	/* Speed */
			foreach_engine {
				uint8 speed = grf_load_byte(&buf);

				svi[i].max_speed = speed; // ?? units
			}
			break;
		}
		case 0x0c:
		{	/* Cargo type */

			foreach_engine {
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
			break;
		}
		case 0x0d:
		{	/* Cargo capacity */
			foreach_engine {
				uint16 capacity = grf_load_word(&buf);

				svi[i].capacity = capacity;
			}
			break;
		}
		case 0x0f:
		{	/* Running cost factor */
			foreach_engine {
				uint8 runcost = grf_load_byte(&buf);

				svi[i].running_cost = runcost;
			}
			break;
		}
		case 0x10:
		{	/* SFX */
			foreach_engine {
				uint8 sfx = grf_load_byte(&buf);

				svi[i].sfx = sfx;
			}
			break;
		}
		case 0x11:
		{	/* Cargos available for refitting */
			foreach_engine {
				uint32 refit_mask = grf_load_dword(&buf);

				_engine_refit_masks[SHIP_ENGINES_INDEX + engine + i] = refit_mask;
			}
			break;
		}
		case 0x12:
		{	/* Callback */
			/* TODO */
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
	/* TODO: Only trains and ships are supported for now. */

	static const VCI_Handler handler[5] = {
		RailVehicleChangeInfo,
		NULL,
		ShipVehicleChangeInfo,
		NULL,
		NULL,
	};
	uint8 feature;
	uint8 numprops;
	uint8 numinfo;
	byte engine;
	EngineInfo *ei;

	check_length(len, 6, "VehicleChangeInfo");
	feature = buf[1];
	numprops = buf[2];
	numinfo = buf[3];
	engine = buf[4];

	if (feature != 0 && feature != 2) {
		grfmsg(GMS_WARN, "VehicleChangeInfo: Unsupported vehicle type %x, skipping.", feature);
		return;
	}

	ei = &_engine_info[engine + _vehshifts[feature]];

	buf += 5;

	while (numprops-- && buf < bufend) {
		uint8 prop = grf_load_byte(&buf);

		switch (prop) {
		case 0x00: {
			/* Introduction date */
			foreach_engine {
				uint16 date = grf_load_word(&buf);

				ei[i].base_intro = date;
			}
			break;
		}
		case 0x02: {
			/* Decay speed */
			foreach_engine {
				uint8 decay = grf_load_byte(&buf);

				ei[i].unk2 &= 0x80;
				ei[i].unk2 |= decay & 0x7f;
			}
			break;
		}
		case 0x03: {
			/* Vehicle life */
			foreach_engine {
				uint8 life = grf_load_byte(&buf);

				ei[i].lifelength = life;
			}
			break;
		}
		case 0x04: {
			/* Model life */
			foreach_engine {
				uint8 life = grf_load_byte(&buf);

				ei[i].base_life = life;
			}
			break;
		}
		case 0x06: {
			/* Climates available */
			foreach_engine {
				uint8 climates = grf_load_byte(&buf);

				ei[i].railtype_climates &= 0xf0;
				ei[i].railtype_climates |= climates;
			}
			break;
		}
		case 0x07: { /* TODO */
			/* Loading speed */
			/* Hyronymus explained me what does
			 * this mean and insists on having a
			 * credit ;-). --pasky */
			/* TODO: This needs to be supported by
			 * LoadUnloadVehicle() first. */
			foreach_engine {
				grf_load_byte(&buf);
			}
			goto ignoring;
		}
		default:
		{
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


/* A sprite superset contains all sprites of a given vehicle (or multiple
 * vehicles) when carrying given cargo. It consists of several sprite sets.
 * Superset ids are refered as "cargo id"s by TTDPatch documentation,
 * contributing to the global confusion.
 *
 * A sprite set contains all sprites of a given vehicle carrying given cargo at
 * a given *stage* - that is usually its load stage. Ie. you can have a
 * spriteset for an empty wagon, wagon full of coal, half-filled wagon etc.
 * Each spriteset contains eight sprites (one per direction) or four sprites if
 * the vehicle is symmetric. */

static int _spriteset_start;
static int _spriteset_numsets;
static int _spriteset_numents;
static int _spriteset_feature;

static int _spritesset_count;
static struct SpriteSuperSet *_spritesset;

/* Action 0x01 */
static void SpriteNewSet(byte *buf, int len)
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

	check_length(len, 4, "SpriteNewSet");
	feature = buf[1];

	if (feature == 4) {
		_spriteset_start = 0;
		grfmsg(GMS_WARN, "SpriteNewSet: Stations unsupported, skipping.");
		return;
	}

	_spriteset_start = _cur_spriteid + 1;
	_spriteset_feature = feature;
	_spriteset_numsets = buf[2];
	_spriteset_numents = buf[3];
}

/* Action 0x02 */
static void SpriteNewSuperset(byte *buf, int len)
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
	/* TODO: Only trains supported now. No 0x80-types (ugh). */
	/* TODO: Also, empty sprites aren't handled for now. Need to investigate
	 * the "opacity" rules for these, that is which sprite to fall back to
	 * when. --pasky */
	uint8 feature;
	uint8 setid;
	uint8 numloaded;
	uint8 numloading;
	struct SpriteSuperSet *superset;
	int i;

	check_length(len, 5, "SpriteNewSuperset");
	feature = buf[1];
	setid = buf[2];
	numloaded = buf[3];
	numloading = buf[4];

	if (feature == 4) {
		grfmsg(GMS_WARN, "SpriteNewSuperset: Stations unsupported, skipping.");
		return;
	}

	if (numloaded == 0x81) {
		// XXX: This is _VERY_ ad hoc just to handle Dm3. And that is
		// a semi-futile ask because the great Patchman himself says
		// this is just buggy. It dereferences last (first) byte of
		// a schedule list pointer of the vehicle and if it's 0xff
		// it uses superset 01, otherwise it uses superset 00. Now
		// if _you_ understand _that_... We just assume it is never
		// 0xff and therefore go for superset 00. --pasky
		uint8 var = buf[4];
		//uint8 shiftnum = buf[5];
		//uint8 andmask = buf[6];
		uint8 nvar = buf[7];
		//uint32 val;
		uint16 def;

		grfmsg(GMS_WARN, "SpriteNewSuperset(0x81): Unsupported variable %x. Using default cid.", var);

		//val = (0xff << shiftnum) & andmask;

		//Go for the default.
		if (setid >= _spritesset_count) {
			_spritesset_count = setid + 1;
			_spritesset = realloc(_spritesset, _spritesset_count * sizeof(struct SpriteSuperSet));
		}
		buf += 8 + nvar * 4;
		def = grf_load_word(&buf);
		_spritesset[setid] = _spritesset[def];
		return;

	} else if (numloaded & 0x80) {
		grfmsg(GMS_WARN, "SpriteNewSuperset(0x%x): Unsupported special superset.", numloaded);
		return;
	}

	if (!_spriteset_start) {
		grfmsg(GMS_WARN, "SpriteNewSuperset: No sprite set to work on! Skipping.");
		return;
	}

	if (_spriteset_feature != feature) {
		grfmsg(GMS_WARN, "SpriteNewSuperset: Superset feature %x doesn't match set feature %x! Skipping.", feature, _spriteset_feature);
		return;
	}

	if (setid >= _spritesset_count) {
		_spritesset_count = setid + 1;
		_spritesset = realloc(_spritesset, _spritesset_count * sizeof(struct SpriteSuperSet));
	}
	superset = &_spritesset[setid];
	memset(superset, 0, sizeof(struct SpriteSuperSet));
	superset->sprites_per_set = _spriteset_numents;

	buf += 5;

	for (i = 0; buf < bufend && i < numloaded; i++) {
		uint16 spriteset_id = grf_load_word(&buf);

		if (_spritesset[setid].loaded_count > 16) {
			grfmsg(GMS_WARN, "SpriteNewSuperset: More than 16 sprites in superset %x, skipping.", setid);
			return;
		}
		superset->loaded[superset->loaded_count++]
			= _spriteset_start + spriteset_id * _spriteset_numents;
	}

	for (i = 0; buf < bufend && i < numloading; i++) {
		uint16 spriteset_id = grf_load_word(&buf);

		if (_spritesset[setid].loading_count > 16) {
			grfmsg(GMS_WARN, "SpriteNewSuperset: More than 16 sprites in superset %x, skipping.", setid);
			return;
		}
		superset->loading[superset->loading_count++] = _spriteset_start + spriteset_id * _spriteset_numents;
	}
}

/* Action 0x03 */
static void VehicleMapSpriteSuperset(byte *buf, int len)
{
	byte *bufend = buf + len;
	/* <03> <feature> <n-id> <ids>... <num-cid> [<cargo-type> <cid>]... <def-cid>
	 * id-list	:= [<id>] [id-list]
	 * cargo-list	:= <cargo-type> <cid> [cargo-list]
	 *
	 * B feature       see action 0
	 * B n-id          bits 0-6: how many IDs this definition applies to
	 *                 bit 7: if set, this is a wagon override definition (see below)
	 * B ids           the IDs for which this definition applies
	 * B num-cid       number of cargo IDs in this definition
	 *                 can be zero, in that case the def-cid is used always
	 * B cargo-type    type of this cargo type (e.g. mail=2, wood=7, see below)
	 * W cid           cargo ID for this type of cargo
	 * W def-cid       default cargo ID */
	/* TODO: Only trains supported now. */
	/* TODO: Multiple cargo support could be useful even for trains/cars -
	 * cargo id 0xff is used for showing images in the build train list. */

	static byte *last_engines;
	static int last_engines_count;
	uint8 feature;
	uint8 idcount;
	int wagover;
	uint8 cidcount;
	int c, i;

	check_length(len, 7, "VehicleMapSpriteSuperset");
	feature = buf[1];
	idcount = buf[2] & 0x7f;
	wagover = buf[2] & 0x80;
	cidcount = buf[3 + idcount];

	if (feature == 4) {
		grfmsg(GMS_WARN, "VehicleMapSpriteSuperset: Stations unsupported, skipping.");
		return;
	}

	// FIXME: Tropicset contains things like:
	// 03 00 01 19 01 00 00 00 00 - this is missing one 00 at the end,
	// what should we exactly do with that? --pasky

	if (!_spriteset_start || !_spritesset) {
		grfmsg(GMS_WARN, "VehicleMapSpriteSuperset: No sprite set to work on! Skipping.");
		return;
	}

	if (!wagover && last_engines_count != idcount) {
		last_engines = realloc(last_engines, idcount);
		last_engines_count = idcount;
	}

	for (i = 0; i < idcount; i++) {
		uint8 engine = buf[3 + i] + _vehshifts[feature];
		byte *bp = &buf[4 + idcount];

		for (c = 0; c < cidcount; c++) {
			uint8 ctype = grf_load_byte(&bp);
			uint16 supersetid = grf_load_word(&bp);

			if (supersetid >= _spritesset_count) {
				grfmsg(GMS_WARN, "VehicleMapSpriteSuperset: Spriteset %x out of range %x, skipping.", supersetid, _spritesset_count);
				return;
			}

			if (ctype == 0xff)
				ctype = CID_PURCHASE;

			if (wagover) {
				// TODO: No multiple cargo types per vehicle yet. --pasky
				SetWagonOverrideSprites(engine, &_spritesset[supersetid], last_engines, last_engines_count);
			} else {
				SetCustomEngineSprites(engine, ctype, &_spritesset[supersetid]);
				last_engines[i] = engine;
			}
		}
	}

	{
		byte *bp = buf + 4 + idcount + cidcount * 3;
		uint16 supersetid = grf_load_word(&bp);

		for (i = 0; i < idcount; i++) {
			uint8 engine = buf[3 + i] + _vehshifts[feature];

			// Don't tell me you don't love duplicated code!
			if (supersetid >= _spritesset_count) {
				grfmsg(GMS_WARN, "VehicleMapSpriteSuperset: Spriteset %x out of range %x, skipping.", supersetid, _spritesset_count);
				return;
			}

			if (wagover) {
				// TODO: No multiple cargo types per vehicle yet. --pasky
				SetWagonOverrideSprites(engine, &_spritesset[supersetid], last_engines, last_engines_count);
			} else {
				SetCustomEngineSprites(engine, CID_DEFAULT, &_spritesset[supersetid]);
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

	uint8 feature;
	uint8 lang;
	uint8 id;
	uint8 endid;

	check_length(len, 6, "VehicleNewName");
	feature = buf[1];
	lang = buf[2];
	id = buf[4] + _vehshifts[feature];
	endid = id + buf[3];

	if (lang & 0x80) {
		grfmsg(GMS_WARN, "VehicleNewName: No support for changing in-game texts. Skipping.");
		return;
	}

	if (!(lang & 3)) {
		/* XXX: If non-English name, silently skip it. */
		return;
	}

	buf += 5, len -= 5;
	for (; id < endid && len > 0; id++) {
		int ofs = strlen(buf) + 1;

		if (ofs < 128)
			SetCustomEngineName(id, buf);
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
	/* TODO: We only support few tests. */
	/* TODO: More params. More condition types. */
	uint8 param;
	uint8 paramsize;
	uint8 condtype;
	uint8 numsprites;
	int val, result;

	check_length(len, 6, "SkipIf");
	param = buf[1];
	paramsize = buf[2];
	condtype = buf[3];
	numsprites = buf[4 + paramsize];

	if (param == 0x83) {
		val = _opt.landscape;
	} else {
		grfmsg(GMS_WARN, "Unsupported param %x. Ignoring test.", param);
		return;
	}

	switch (condtype) {
		case 2: result = (buf[4] == val);
			break;
		case 3: result = (buf[4] != val);
			break;
		default:
			grfmsg(GMS_WARN, "Unsupported test %d. Ignoring.", condtype);
			return;
	}

	if (!result)
		return;

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
	uint32 grfid; /* this is de facto big endian - grf_load_dword() unsuitable */
	char *name;
	char *info;

	check_length(len, 9, "GRFInfo");
	version = buf[1];
	grfid = buf[2] << 24 | buf[3] << 16 | buf[4] << 8 | buf[5];
	name = buf + 6;
	info = name + strlen(name) + 1;
	DEBUG(grf, 1) ("[%s] Loaded GRFv%d set %08lx - %s:\n%s\n",
	               _cur_grffile, version, grfid, name, info);
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
	/* TODO */
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

	if (msgid == 0xff) {
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
	/* TODO */
}

static void GRFInhibit(byte *buf, int len)
{
	/* <0E> <num> <grfids...>
	 *
	 * B num           Number of GRFIDs that follow
	 * D grfids        GRFIDs of the files to deactivate */
	/* TODO */
}

/* Here we perform initial decoding of some special sprites (as are they
 * described at http://www.ttdpatch.net/src/newgrf.txt, but this is only a very
 * partial implementation yet; also, we ignore the stages stuff). */
/* XXX: We consider GRF files trusted. It would be trivial to exploit OTTD by
 * a crafted invalid GRF file. We should tell that to the user somehow, or
 * better make this more robust in the future. */

void DecodeSpecialSprite(const char *filename, int num, int spriteid)
{
#define NUM_ACTIONS 0xf
	static const SpecialSpriteHandler handlers[NUM_ACTIONS] = {
		/* 0x0 */ VehicleChangeInfo,
		/* 0x1 */ SpriteNewSet,
		/* 0x2 */ SpriteNewSuperset,
		/* 0x3 */ VehicleMapSpriteSuperset,
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
	byte action;
	byte *buf = malloc(num);
	int i;

	if (buf == NULL) error("DecodeSpecialSprite: Could not allocate memory");

	_cur_grffile = filename;
	_cur_spriteid = spriteid;

	for (i = 0; i != num; i++)
		buf[i] = FioReadByte();

	action = buf[0];
	if (action < NUM_ACTIONS) {
		handlers[action](buf, num);
	} else {
		grfmsg(GMS_WARN, "Unknown special sprite action %x, skipping.", action);
	}

	free(buf);
#undef NUM_ACTIONS
}
