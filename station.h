#ifndef STATION_H
#define STATION_H

#include "vehicle.h"

typedef struct GoodsEntry {
	uint16 waiting_acceptance;
	byte days_since_pickup;
	byte rating;
	byte enroute_from;
	byte enroute_time;
	byte last_speed;
	byte last_age;
} GoodsEntry;

struct Station {
	TileIndex xy;
	TileIndex bus_tile;
	TileIndex lorry_tile;
	TileIndex train_tile;
	TileIndex airport_tile;
	TileIndex dock_tile;
	Town *town;
	// alpha_order is obsolete since savegame format 4
	byte alpha_order_obsolete;
	uint16 string_id;

	ViewportSign sign;

	uint16 had_vehicle_of_type;

	byte time_since_load;
	byte time_since_unload;
	byte delete_ctr;
	byte owner;
	byte facilities;
	byte airport_type;
	byte truck_stop_status;
	byte bus_stop_status;
	byte blocked_months_obsolete;

	// trainstation width/height
	byte trainst_w, trainst_h;

	byte class_id; // custom graphics station class
	byte stat_id; // custom graphics station id in the @class_id class
	uint16 build_date;

	//uint16 airport_flags;
	uint32 airport_flags;
	uint16 index;

	VehicleID last_vehicle;
	GoodsEntry goods[NUM_CARGO];
};

enum {
	FACIL_TRAIN = 1,
	FACIL_TRUCK_STOP = 2,
	FACIL_BUS_STOP = 4,
	FACIL_AIRPORT = 8,
	FACIL_DOCK = 0x10,
};

enum {
//	HVOT_PENDING_DELETE = 1<<0, // not needed anymore
	HVOT_TRAIN = 1<<1,
	HVOT_BUS = 1 << 2,
	HVOT_TRUCK = 1 << 3,
	HVOT_AIRCRAFT = 1<<4,
	HVOT_SHIP = 1 << 5,
	HVOT_BUOY = 1 << 6
};

void ModifyStationRatingAround(TileIndex tile, byte owner, int amount, uint radius);

TileIndex GetStationTileForVehicle(Vehicle *v, Station *st);

void ShowStationViewWindow(int station);
void UpdateAllStationVirtCoord();

VARDEF Station _stations[250];
VARDEF bool _station_sort_dirty[MAX_PLAYERS];
VARDEF bool _global_station_sort_dirty;

#define DEREF_STATION(i) (&_stations[i])
#define FOR_ALL_STATIONS(st) for(st=_stations; st != endof(_stations); st++)


void GetProductionAroundTiles(uint *produced, uint tile, int w, int h);
void GetAcceptanceAroundTiles(uint *accepts, uint tile, int w, int h);
uint GetStationPlatforms(Station *st, uint tile);


typedef struct DrawTileSeqStruct {
	int8 delta_x;
	int8 delta_y;
	int8 delta_z;
	byte width,height;
	byte unk; // 'depth', just z-size; TODO: rename
	uint32 image;
} DrawTileSeqStruct;

typedef struct DrawTileSprites {
	SpriteID ground_sprite;
	DrawTileSeqStruct const *seq;
} DrawTileSprites;

#define foreach_draw_tile_seq(idx, list) for (idx = list; ((byte) idx->delta_x) != 0x80; idx++)

void SetCustomStation(uint32 classid, byte stid, DrawTileSprites *data, byte tiles);
DrawTileSprites *GetCustomStation(uint32 classid, byte stid);
int GetCustomStationsCount(uint32 classid);

#endif /* STATION_H */
