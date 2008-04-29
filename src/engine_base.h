/* $Id$ */

/** @file engine_base.h Base class for engines. */

#ifndef ENGINE_BASE_H
#define ENGINE_BASE_H

#include "engine_type.h"
#include "oldpool.h"

DECLARE_OLD_POOL(Engine, Engine, 6, 10000)

struct Engine : PoolItem<Engine, EngineID, &_Engine_pool> {
	char *name;         ///< Custom name of engine
	Date intro_date;
	Date age;
	uint16 reliability;
	uint16 reliability_spd_dec;
	uint16 reliability_start, reliability_max, reliability_final;
	uint16 duration_phase_1, duration_phase_2, duration_phase_3;
	byte lifelength;
	byte flags;
	uint8 preview_player_rank;
	byte preview_wait;
	byte player_avail;
	uint8 image_index; ///< Original vehicle image index
	VehicleType type; ///< type, ie VEH_ROAD, VEH_TRAIN, etc.

	EngineInfo info;

	union {
		RailVehicleInfo rail;
		RoadVehicleInfo road;
		ShipVehicleInfo ship;
		AircraftVehicleInfo air;
	} u;

	/* NewGRF related data */
	const struct GRFFile *grffile;
	const struct SpriteGroup *group[NUM_CARGO + 2];
	uint16 internal_id;                             ///< ID within the GRF file
	uint16 overrides_count;
	struct WagonOverride *overrides;
	uint16 list_position;

	Engine();
	Engine(VehicleType type, EngineID base);
	~Engine();

	inline bool IsValid() const { return this->info.climates != 0; }
};

static inline bool IsEngineIndex(uint index)
{
	return index < GetEnginePoolSize();
}

#define FOR_ALL_ENGINES_FROM(e, start) for (e = GetEngine(start); e != NULL; e = (e->index + 1U < GetEnginePoolSize()) ? GetEngine(e->index + 1U) : NULL) if (e->IsValid())
#define FOR_ALL_ENGINES(e) FOR_ALL_ENGINES_FROM(e, 0)

#define FOR_ALL_ENGINES_OF_TYPE(e, engine_type) FOR_ALL_ENGINES(e) if (e->type == engine_type)

static inline const EngineInfo *EngInfo(EngineID e)
{
	return &GetEngine(e)->info;
}

static inline const RailVehicleInfo *RailVehInfo(EngineID e)
{
	return &GetEngine(e)->u.rail;
}

static inline const RoadVehicleInfo *RoadVehInfo(EngineID e)
{
	return &GetEngine(e)->u.road;
}

static inline const ShipVehicleInfo *ShipVehInfo(EngineID e)
{
	return &GetEngine(e)->u.ship;
}

static inline const AircraftVehicleInfo *AircraftVehInfo(EngineID e)
{
	return &GetEngine(e)->u.air;
}

#endif /* ENGINE_TYPE_H */
