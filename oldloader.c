#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "town.h"
#include "industry.h"
#include "station.h"
#include "economy.h"
#include "player.h"
#include "engine.h"
#include "vehicle.h"

extern byte _name_array[512][32];
extern TileIndex _animated_tile_list[256];
extern uint16 _custom_sprites_base;

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

typedef struct {
	uint16 string_id;
	uint16 x;
	uint16 right;
	uint16 y;
	uint16 bottom;
	uint16 duration;
	uint32 params[2];
} GCC_PACK OldTextEffect;
assert_compile(sizeof(OldTextEffect) == 0x14);

typedef struct {
	uint16 xy;
	uint16 population;
	uint16 townnametype;
	uint32 townnameparts;
	byte grow_counter;
	byte sort_index;
	int16 sign_left, sign_top;
	byte namewidth_1, namewidth_2;
	uint16 flags12;
	uint16 radius[5];
	uint16 ratings[8];
	uint32 have_ratings;
	uint32 statues;
	uint16 num_houses;
	byte time_until_rebuild;
	byte growth_rate;
	uint16 new_max_pass, new_max_mail;
	uint16 new_act_pass, new_act_mail;
	uint16 max_pass, max_mail;
	uint16 act_pass, act_mail;
	byte pct_pass_transported, pct_mail_transported;
	uint16 new_act_food, new_act_water;
	uint16 act_food, act_water;
	byte road_build_months;
	byte fund_buildings_months;
	// unused bytes at the end of the Town Struct
	uint32 unk56;
	uint32 unk5A;
} GCC_PACK OldTown;
assert_compile(sizeof(OldTown) == 0x5E);

typedef struct {
	uint16 xy;
	uint32 town;
} GCC_PACK OldDepot;
assert_compile(sizeof(OldDepot) == 0x6);

typedef struct {
	uint32 price;
	uint16 frac;
} GCC_PACK OldPrice;
assert_compile(sizeof(OldPrice) == 0x6);

typedef struct {
	uint32 price;
	uint16 frac;
	uint16 unused;
} GCC_PACK OldPaymentRate;
assert_compile(sizeof(OldPaymentRate) == 8);

typedef struct {
	uint16 waiting_acceptance;
	byte days_since_pickup;
	byte rating;
	byte enroute_from;
	byte enroute_time;
	byte last_speed;
	byte last_age;
} GCC_PACK OldGoodsEntry;
assert_compile(sizeof(OldGoodsEntry) == 8);

typedef struct {
	uint16 xy;
	uint32 town;
	uint16 bus_tile, lorry_tile, train_tile, airport_tile, dock_tile;
	byte platforms;
	byte alpha_order_obsolete;	// alpha_order is obsolete since savegame format 4
	byte namewidth_1, namewidth_2;
	uint16 string_id;
	int16 sign_left, sign_top;
	uint16 had_vehicle_of_type;
	OldGoodsEntry goods[12];
	byte time_since_load, time_since_unload;
	byte delete_ctr;
	byte owner;
	byte facilities;
	byte airport_type;
	byte truck_stop_status, bus_stop_status;
	byte blocked_months_obsolete;
	byte unk85;
	uint16 airport_flags;
	uint16 last_vehicle;
	uint32 unk8A;
} GCC_PACK OldStation;
assert_compile(sizeof(OldStation) == 0x8E);

typedef struct {
	uint16 xy;
	uint32 town;
	byte width;
	byte height;
	byte produced_cargo[2];
	uint16 cargo_waiting[2];
	byte production_rate[2];
	byte accepts_cargo[3];
	byte prod_level;
	uint16 last_mo_production[2];
	uint16 last_mo_transported[2];
	byte pct_transported[2];
	uint16 total_production[2];
	uint16 total_transported[2];
	byte type;
	byte owner;
	byte color_map;
	byte last_prod_year;
	uint16 counter;
	byte was_cargo_delivered;
	byte nothing;
	uint32 unk2E;
	uint32 unk32;
} GCC_PACK OldIndustry;
assert_compile(sizeof(OldIndustry) == 0x36);

typedef struct {
	int32 cost[13];
} GCC_PACK OldPlayerExpenses;
assert_compile(sizeof(OldPlayerExpenses) == 0x34);

typedef struct {
	int32 income;
	int32 expenses;
	uint32 delivered_cargo;
	uint32 performance_history;
	uint32 company_value;
} GCC_PACK OldPlayerEconomy;
assert_compile(sizeof(OldPlayerEconomy) == 0x14);

typedef struct {
	uint16 spec_tile;
	uint16 use_tile;
	byte rand_rng;
	byte cur_rule;
	byte unk6;
	byte unk7;
	byte buildcmd_a;
	byte buildcmd_b;
	byte direction;
	byte cargo;
	byte unused[8];
} GCC_PACK OldAiBuildRec;
assert_compile(sizeof(OldAiBuildRec) == 0x14);

typedef struct {
	uint16 tile;
	byte data;
} GCC_PACK OldAiBannedTile;
assert_compile(sizeof(OldAiBannedTile) == 3);

typedef struct {
	uint16 name_1;
	uint32 name_2;
	uint32 face;
	uint16 pres_name_1;
	uint32 pres_name_2;
	uint32 money;
	uint32 loan;
	byte color;
	byte money_fract;
	byte quarters_of_bankrupcy;
	byte bankrupt_asked;
	uint32 bankrupt_value;
	uint16 bankrupt_timeout;
	uint32 cargo_types;
	OldPlayerExpenses expenses[3];
	OldPlayerEconomy economy[24 + 1];
	uint16 inaugurated_date;
	uint16 last_build_coordinate;
	byte num_valid_stat_ent;
	byte ai_state;
	byte unused;
	byte ai_state_mode;
	uint16 ai_state_counter;
	uint16 ai_timeout_counter;
	OldAiBuildRec ai_src, ai_dst, ai_mid1, ai_mid2;
	byte unused_2[20];
	byte ai_cargo_type;
	byte ai_num_wagons;
	byte ai_build_kind;
	byte ai_num_build_rec;
	byte ai_num_loco_to_build;
	byte ai_num_want_fullload;
	byte unused_3[14];
	uint16 ai_loco_id; // NOT USED
	uint16 ai_wagonlist[9];
	byte ai_order_list_blocks[20];
	uint16 ai_start_tile_a;
	uint16 ai_start_tile_b;
	uint16 ai_cur_tile_a;
	uint16 ai_cur_tile_b;
	byte ai_start_dir_a;
	byte ai_start_dir_b;
	byte ai_cur_dir_a;
	byte ai_cur_dir_b;
	byte ai_banned_tile_count;
	OldAiBannedTile banned_tiles[16];
	byte ai_railtype_to_use;
	byte ai_route_type_mask;
	byte block_preview;
	byte ai_tick;
	byte max_railtype;
	uint16 location_of_house;
	byte share_owners[4];
	uint32 unk3AA;
	uint32 unk3AE;
} GCC_PACK OldPlayer;
assert_compile(sizeof(OldPlayer) == 0x3B2);

typedef struct {
	byte track;
	byte force_proceed;
	uint16 crash_anim_pos;
	byte railtype;
} GCC_PACK OldVehicleRailUnion;
assert_compile(sizeof(OldVehicleRailUnion) == 5);

typedef struct {
	byte unk0;
	byte targetairport;
	uint16 crashed_counter;
	byte state;
} GCC_PACK OldVehicleAirUnion;
assert_compile(sizeof(OldVehicleAirUnion) == 5);

typedef struct {
	byte state;
	byte frame;
	uint16 unk2;
	byte overtaking;
	byte overtaking_ctr;
	uint16 crashed_ctr;
	byte reverse_ctr;
} GCC_PACK OldVehicleRoadUnion;
assert_compile(sizeof(OldVehicleRoadUnion) == 9);

typedef struct {
	uint16 unk0;
	byte unk2;
} GCC_PACK OldVehicleSpecialUnion;
assert_compile(sizeof(OldVehicleSpecialUnion) == 3);

typedef struct {
	uint16 image_override;
	uint16 unk2;
} GCC_PACK OldVehicleDisasterUnion;
assert_compile(sizeof(OldVehicleDisasterUnion) == 4);

typedef struct {
	byte state;
} OldVehicleShipUnion;
assert_compile(sizeof(OldVehicleShipUnion) == 1);

typedef union {
	OldVehicleRailUnion rail;
	OldVehicleAirUnion air;
	OldVehicleRoadUnion road;
	OldVehicleSpecialUnion special;
	OldVehicleDisasterUnion disaster;
	OldVehicleShipUnion ship;
	byte pad[10];
} GCC_PACK OldVehicleUnion;
assert_compile(sizeof(OldVehicleUnion) == 10);

typedef struct {
	byte type;
	byte subtype;
	uint16 next_hash; // NOLOAD, calculated automatically.
	uint16 index;     // NOLOAD, calculated automatically.
	uint32 schedule_ptr;
	byte next_order, next_order_param;
	byte num_orders;
	byte cur_order_index;
	uint16 dest_tile;
	uint16 load_unload_time_rem;
	uint16 date_of_last_service;
	uint16 service_interval;
	byte last_station_visited;
	byte tick_counter;
	uint16 max_speed;
	uint16 x_pos, y_pos;
	byte z_pos;
	byte direction;
	byte x_offs, y_offs;
	byte sprite_width, sprite_height, z_height;
	byte owner;
	uint16 tile;
	uint16 cur_image;

	int16 left_coord, right_coord, top_coord, bottom_coord; // NOLOAD, calculated automatically.
	uint16 vehstatus;
	uint16 cur_speed;
	byte subspeed;
	byte acceleration;
	byte progress;
	byte cargo_type;
	uint16 capacity;
	uint16 number_of_pieces;
	byte source_of_pieces;
	byte days_in_transit;
	uint16 age_in_days, max_age_in_days;
	byte build_year;
	byte unitnumber;
	uint16 engine_type;
	byte spritenum;
	byte day_counter;
	byte breakdowns_since_last_service;
	byte breakdown_ctr, breakdown_delay, breakdown_chance;
	uint16 reliability, reliability_spd_dec;
	uint32 profit_this_year, profit_last_year;
	uint16 next_in_chain;
	uint32 value;
	uint16 string_id;
	OldVehicleUnion u;
	byte unused[20];
} GCC_PACK OldVehicle;
assert_compile(sizeof(OldVehicle) == 0x80);

typedef struct {
	byte name[32];
} GCC_PACK OldName;

typedef struct {
	uint16 text;
	int16 x,y,z;
	byte namewidth_1, namewidth_2;
	int16 sign_left, sign_top;
} GCC_PACK OldSign;
assert_compile(sizeof(OldSign) == 0xE);

typedef struct {
	uint16 player_avail;
	uint16 intro_date;
	uint16 age;
	uint16 reliability, reliability_spd_dec, reliability_start, reliability_max, reliability_final;
	uint16 duration_phase_1, duration_phase_2, duration_phase_3;
	byte lifelength;
	byte flags;
	byte preview_player;
	byte preview_wait;
	byte railtype;
	byte unk1B;
} GCC_PACK OldEngine;
assert_compile(sizeof(OldEngine) == 0x1C);

typedef struct {
	byte cargo_type;
	byte age;
	byte from;
	byte to;
} GCC_PACK OldSubsidy;
assert_compile(sizeof(OldSubsidy) == 4);

typedef struct {
	uint16 max_no_competitors;
	uint16 competitor_start_time;
	uint16 number_towns;
	uint16 number_industries;
	uint16 max_loan;
	uint16 initial_interest;
	uint16 vehicle_costs;
	uint16 competitor_speed;
	uint16 competitor_intelligence;
	uint16 vehicle_breakdowns;
	uint16 subsidy_multiplier;
	uint16 construction_cost;
	uint16 terrain_type;
	uint16 quantity_sea_lakes;
	uint16 economy;
	uint16 line_reverse_mode;
	uint16 disasters;
} GCC_PACK OldGameSettings;
assert_compile(sizeof(OldGameSettings) == 0x22);

typedef struct {
	uint16 date;
	uint16 date_fract;

	OldTextEffect te_list[30]; // NOLOAD: not so important.
	uint32 seed_1, seed_2;

	OldTown town_list[70];
	uint16 order_list[5000];

	uint16 animated_tile_list[256];
	uint32 ptr_to_next_order;
	OldDepot depots[255];

	uint32 cur_town_ptr;
	uint16 timer_counter;
	uint16 land_code; // NOLOAD: never needed in game
	uint16 age_cargo_skip_counter;
	uint16 tick_counter;
	uint16 cur_tileloop_tile;

	OldPrice prices[49];
	OldPaymentRate cargo_payment_rates[12];

	byte map_owner[256*256];
	byte map2[256*256];
	uint16 map3[256*256];
	byte map_extra[256*256/4];

	OldStation stations[250];
	OldIndustry industries[90];
	OldPlayer players[8];
	OldVehicle vehicles[850];
	OldName names[500];

	uint16 vehicle_position_hash[0x1000]; // NOLOAD, calculated automatically.

	OldSign signs[40];
	OldEngine engines[256];

	uint16 vehicle_id_ctr_day;
	OldSubsidy subsidies[8];

	uint16 next_competitor_start;

	uint16 saved_main_scrollpos_x, saved_main_scrollpos_y, saved_main_scrollpos_zoom;
	uint32 maximum_loan, maximum_loan_unround;
	uint16 economy_fluct;
	uint16 disaster_delay;

	//NOLOAD. These are calculated from InitializeLandscapeVariables
	uint16 cargo_names_s[12], cargo_names_p[12], cargo_names_long_s[12], cargo_names_long_p[12], cargo_names_short[12];
	uint16 cargo_sprites[12];

	uint16 engine_name_strings[256];

	//NOLOAD. These are calculated from InitializeLandscapeVariables
	uint16 railveh_by_cargo_1[12], railveh_by_cargo_2[12], railveh_by_cargo_3[12];
	uint16 roadveh_by_cargo_start[12];
	byte roadveh_by_cargo_count[12];
	uint16 ship_of_type_start[12];
	byte ship_of_type_count[12];

	byte human_player_1, human_player_2; //NOLOAD. Calculated automatically.
	byte station_tick_ctr;
	byte currency;
	byte use_kilometers;
	byte cur_player_tick_index;
	byte cur_year, cur_month;		//NOLOAD. Calculated automatically.
	byte player_colors[8];			//NOLOAD. Calculated automatically

	byte inflation_amount;
	byte inflation_amount_payment_rates;
	byte interest_rate;

	byte avail_aircraft;
	byte road_side;
	byte town_name_type;
	OldGameSettings game_diff;
	byte difficulty_level;
	byte landscape_type;
	byte trees_tick_ctr;
	byte vehicle_design_names;
	byte snow_line_height;

	byte new_industry_randtable[32]; // NOLOAD. Not needed due to different code design.

	//NOLOAD. Initialized by InitializeLandscapeVariables
	byte cargo_weights[12];
	byte transit_days_table_1[12];
	byte transit_days_table_2[12];

	byte map_type_and_height[256*256];
	byte map5[256*256];
} GCC_PACK OldMain;
assert_compile(sizeof(OldMain) == 487801 + 256*256*2);

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#define REMAP_TOWN_IDX(x) (x - (0x0459154 - 0x0458EF0)) / sizeof(OldTown)
#define REMAP_TOWN_PTR(x) DEREF_TOWN( REMAP_TOWN_IDX(x) )

#define REMAP_ORDER_IDX(x) (x - (0x045AB08 - 0x0458EF0)) / sizeof(uint16)

typedef struct LoadSavegameState {
	int8 mode;
	byte rep_char;

	size_t count;
	size_t buffer_count;
	FILE *fin;

	byte *buffer_cur;

	byte buffer[4096];

} LoadSavegameState;

static LoadSavegameState *_cur_state;

static byte GetSavegameByteFromBuffer()
{
	LoadSavegameState *lss = _cur_state;

	if (lss->buffer_count == 0) {
		int count = fread(lss->buffer, 1, 4096, lss->fin) ;
/*		if (count == 0) {
			memset(lss->buffer, 0, sizeof(lss->buffer));
			count = 4096;
		}*/
		assert(count != 0);
		lss->buffer_count = count;
		lss->buffer_cur = lss->buffer;
	}

	lss->buffer_count--;
	return *lss->buffer_cur++;
}

static byte DecodeSavegameByte()
{
	LoadSavegameState *lss = _cur_state;
	int8 x;

	if (lss->mode < 0) {
		if (lss->count != 0) {
			lss->count--;
			return lss->rep_char;
		}
	} else if (lss->mode > 0) {
		if (lss->count != 0) {
			lss->count--;
			return GetSavegameByteFromBuffer();
		}
	}

	x = GetSavegameByteFromBuffer();
	if (x >= 0) {
		lss->count = x;
		lss->mode = 1;
		return GetSavegameByteFromBuffer();
	} else {
		lss->mode = -1;
		lss->count = -x;
		lss->rep_char = GetSavegameByteFromBuffer();
		return lss->rep_char;
	}
}

static void LoadSavegameBytes(void *p, size_t count)
{
	byte *ptr = (byte*)p;
	assert(count > 0);
	do {
		*ptr++ = DecodeSavegameByte();
	} while (--count);
}

extern uint32 GetOldTownName(uint32 townnameparts, byte old_town_name_type);

static void FixTown(Town *t, OldTown *o, int num, byte town_name_type)
{
	do {
		t->xy = o->xy;
		t->population = o->population;
		t->townnametype = o->townnametype;
		t->townnameparts = o->townnameparts;
		// Random TownNames
		if (IS_INT_INSIDE(o->townnametype, 0x20C1, 0x20C2 + 1)) {
			t->townnametype = SPECSTR_TOWNNAME_ENGLISH + town_name_type;
			if (o->xy)
				t->townnameparts = GetOldTownName(o->townnameparts, town_name_type);
		}

		t->grow_counter = o->grow_counter;
		t->flags12 = o->flags12;
		memcpy(t->ratings,o->ratings,sizeof(t->ratings));
		t->have_ratings = o->have_ratings;
		t->statues = o->statues;
		t->num_houses = o->num_houses;
		t->time_until_rebuild = o->time_until_rebuild;
		t->growth_rate = o->growth_rate;
		t->new_max_pass = o->new_max_pass;
		t->new_max_mail = o->new_max_mail;
		t->new_act_pass = o->new_act_pass;
		t->new_act_mail = o->new_act_mail;
		t->max_pass = o->max_pass;
		t->max_mail = o->max_mail;
		t->act_pass = o->act_pass;
		t->act_mail = o->act_mail;
		t->pct_pass_transported = o->pct_pass_transported;
		t->pct_mail_transported = o->pct_mail_transported;
		t->new_act_food = o->new_act_food;
		t->new_act_water = o->new_act_water;
		t->act_food = o->act_food;
		t->act_water = o->act_water;
		t->road_build_months = o->road_build_months;
		t->fund_buildings_months = o->fund_buildings_months;
	} while (t++,o++,--num);
}

static void FixIndustry(Industry *i, OldIndustry *o, int num)
{
	do {
		i->xy = o->xy;
		i->town = REMAP_TOWN_PTR(o->town);
		i->width = o->width;
		i->height = o->height;
		i->produced_cargo[0] = o->produced_cargo[0];
		i->produced_cargo[1] = o->produced_cargo[1];
		i->cargo_waiting[0] = o->cargo_waiting[0];
		i->cargo_waiting[1] = o->cargo_waiting[1];
		i->production_rate[0] = o->production_rate[0];
		i->production_rate[1] = o->production_rate[1];
		i->accepts_cargo[0] = o->accepts_cargo[0];
		i->accepts_cargo[1] = o->accepts_cargo[1];
		i->accepts_cargo[2] = o->accepts_cargo[2];
		i->prod_level = o->prod_level;
		i->last_mo_production[0] = o->last_mo_production[0];
		i->last_mo_production[1] = o->last_mo_production[1];

		i->last_mo_transported[0] = o->last_mo_transported[0];
		i->last_mo_transported[1] = o->last_mo_transported[1];
		i->last_mo_transported[2] = o->last_mo_transported[2];

		i->pct_transported[0] = o->pct_transported[0];
		i->pct_transported[1] = o->pct_transported[1];

		i->total_production[0] = o->total_production[0];
		i->total_production[1] = o->total_production[1];

		i->total_transported[0] = i->total_transported[0];
		i->total_transported[1] = i->total_transported[1];

		i->type = o->type;
		i->owner = o->owner;
		i->color_map = o->color_map;
		i->last_prod_year = o->last_prod_year;
		i->counter = o->counter;
		i->was_cargo_delivered = o->was_cargo_delivered;
	} while (i++,o++,--num);
}

static void FixGoodsEntry(GoodsEntry *g, OldGoodsEntry *o, int num)
{
	do {
		g->waiting_acceptance = o->waiting_acceptance;
		g->days_since_pickup = o->days_since_pickup;
		g->rating = o->rating;
		g->enroute_from = o->enroute_from;
		g->enroute_time = o->enroute_time;
		g->last_speed = o->last_speed;
		g->last_age = o->last_age;
	} while (g++,o++,--num);
}

static void FixStation(Station *s, OldStation *o, int num)
{
	do {
		s->xy = o->xy;
		s->town = REMAP_TOWN_PTR(o->town);
		s->bus_tile = o->bus_tile;
		s->lorry_tile = o->lorry_tile;
		s->train_tile = o->train_tile;
		s->airport_tile = o->airport_tile;
		s->dock_tile = o->dock_tile;

		if (o->train_tile) {
			int w = (o->platforms >> 3) & 0x7;
			int h = (o->platforms & 0x7);
			if (_map5[o->train_tile]&1) intswap(w,h);
			s->trainst_w = w;
			s->trainst_h = h;
		}

		s->string_id = RemapOldStringID(o->string_id);
		s->had_vehicle_of_type = o->had_vehicle_of_type;
		FixGoodsEntry(s->goods, o->goods, lengthof(o->goods));
		s->time_since_load = o->time_since_load;
		s->time_since_unload = o->time_since_unload;
		s->delete_ctr = o->delete_ctr;
		s->owner = o->owner;
		s->facilities = o->facilities;
		s->airport_type = o->airport_type;
		s->truck_stop_status = o->truck_stop_status;
		s->bus_stop_status = o->bus_stop_status;
		s->blocked_months_obsolete = o->blocked_months_obsolete;
		s->airport_flags = o->airport_flags;
		s->last_vehicle = o->last_vehicle;
	} while (s++,o++,--num);
}

static void FixDepot(Depot *n, OldDepot *o, int num)
{
	do {
		n->town_index = REMAP_TOWN_IDX(o->town);
		n->xy = o->xy;
	} while (n++,o++,--num);
}

static void FixVehicle(Vehicle *n, OldVehicle *o, int num)
{
	do {
		n->type = o->type;
		n->subtype = o->subtype;

		if (o->schedule_ptr == 0xFFFFFFFF || o->schedule_ptr == 0) {
			n->schedule_ptr = NULL;
		} else {
			n->schedule_ptr = _order_array + REMAP_ORDER_IDX(o->schedule_ptr);
			assert(n->schedule_ptr >= _order_array && n->schedule_ptr < _ptr_to_next_order);
		}

		n->next_order = o->next_order;
		n->next_order_param = o->next_order_param;
		n->num_orders = o->num_orders;
		n->cur_order_index = o->cur_order_index;
		n->dest_tile = o->dest_tile;
		n->load_unload_time_rem = o->load_unload_time_rem;
		n->date_of_last_service = o->date_of_last_service;
		n->service_interval = o->service_interval;
		n->last_station_visited = o->last_station_visited;
		n->tick_counter = o->tick_counter;
		n->max_speed = o->max_speed;
		n->x_pos = o->x_pos;
		n->y_pos = o->y_pos;
		n->z_pos = o->z_pos;
		n->direction = o->direction;
		n->x_offs = o->x_offs;
		n->y_offs = o->y_offs;
		n->sprite_width = o->sprite_width;
		n->sprite_height = o->sprite_height;
		n->z_height = o->z_height;
		n->owner = o->owner;
		n->tile = o->tile;
		n->cur_image = o->cur_image;
		if (o->cur_image >= 0x2000) // TTDPatch maps sprites from 0x2000 up.
			n->cur_image -= 0x2000 - _custom_sprites_base;

		n->vehstatus = o->vehstatus;
		n->cur_speed = o->cur_speed;
		n->subspeed = o->subspeed;
		n->acceleration = o->acceleration;
		n->progress = o->progress;
		n->cargo_type = o->cargo_type;
		n->cargo_cap = o->capacity;
		n->cargo_count = o->number_of_pieces;
		n->cargo_source = o->source_of_pieces;
		n->cargo_days = o->days_in_transit;
		n->age = o->age_in_days;
		n->max_age = o->max_age_in_days;
		n->build_year = o->build_year;
		n->unitnumber = o->unitnumber;
		n->engine_type = o->engine_type;
		switch (o->spritenum) {
			case 0xfd: n->spritenum = 0xfd; break;
			case 0xff: n->spritenum = 0xfe; break;
			default:   n->spritenum = o->spritenum >> 1; break;
		}
		n->day_counter = o->day_counter;
		n->breakdowns_since_last_service = o->breakdowns_since_last_service;
		n->breakdown_ctr = o->breakdown_ctr;
		n->breakdown_delay = o->breakdown_delay;
		n->breakdown_chance = o->breakdown_chance;
		n->reliability = o->reliability;
		n->reliability_spd_dec = o->reliability_spd_dec;
		n->profit_this_year = o->profit_this_year;
		n->profit_last_year = o->profit_last_year;
		n->next = o->next_in_chain == 0xffff ? NULL : &_vehicles[o->next_in_chain];
		n->value = o->value;
		n->string_id = RemapOldStringID(o->string_id);

		switch(o->type) {
		case VEH_Train:
			n->u.rail.track = o->u.rail.track;
			n->u.rail.force_proceed = o->u.rail.force_proceed;
			n->u.rail.crash_anim_pos = o->u.rail.crash_anim_pos;
			n->u.rail.railtype = o->u.rail.railtype;
			break;

		case VEH_Road:
			n->u.road.state = o->u.road.state;
			n->u.road.frame = o->u.road.frame;
			n->u.road.unk2 = o->u.road.unk2;
			n->u.road.overtaking = o->u.road.overtaking;
			n->u.road.overtaking_ctr = o->u.road.overtaking_ctr;
			n->u.road.crashed_ctr = o->u.road.crashed_ctr;
			n->u.road.reverse_ctr = o->u.road.reverse_ctr;
			break;
		case VEH_Ship:
			n->u.ship.state = o->u.ship.state;
			break;
		case VEH_Aircraft:
			n->u.air.crashed_counter = o->u.air.crashed_counter;
			n->u.air.pos = o->u.air.unk0;
			n->u.air.targetairport = o->u.air.targetairport;
			n->u.air.state = o->u.air.state;
			break;
		case VEH_Special:
			n->u.special.unk0 = o->u.special.unk0;
			n->u.special.unk2 = o->u.special.unk2;
			n->subtype = o->subtype >> 1;
			break;
		case VEH_Disaster:
			n->u.disaster.image_override = o->u.disaster.image_override;
			n->u.disaster.unk2 = o->u.disaster.unk2;
			break;
		}
	} while (n++,o++,--num);
}

static void FixSubsidy(Subsidy *n, OldSubsidy *o, int num)
{
	do {
		n->age = o->age;
		n->cargo_type = o->cargo_type;
		n->from = o->from;
		n->to = o->to;
	} while (n++,o++,--num);
}

static void FixEconomy(PlayerEconomyEntry *n, OldPlayerEconomy *o)
{
	n->company_value = o->company_value;
	n->delivered_cargo = o->delivered_cargo;
	n->income = -o->income;
	n->expenses = -o->expenses;
	n->performance_history = o->performance_history;
}

static void FixAiBuildRec(AiBuildRec *n, OldAiBuildRec *o)
{
	n->spec_tile = o->spec_tile;
	n->use_tile = o->use_tile;
	n->rand_rng = o->rand_rng;
	n->cur_building_rule = o->cur_rule;
	n->unk6 = o->unk6;
	n->unk7 = o->unk7;
	n->buildcmd_a = o->buildcmd_a;
	n->buildcmd_b = o->buildcmd_b;
	n->direction = o->direction;
	n->cargo = o->cargo;
}

static void FixPlayer(Player *n, OldPlayer *o, int num, byte town_name_type)
{
	int i, j;
	int x = 0;

	do {
		n->name_1 = RemapOldStringID(o->name_1);
		n->name_2 = o->name_2;
		/*	In every Old TTD(Patch) game Player1 (0) is human, and all others are AI
		 *	(Except in .SV2 savegames, which were 2 player games, but we are not fixing
		 *	that
		 */

		if (x == 0) {
			if (o->name_1 == 0) // if first player doesn't have a name, he is 'unnamed'
				n->name_1 = STR_SV_UNNAMED;
		} else {
			n->is_ai = 1;
		}

		if (o->name_1 != 0)
			n->is_active = true;

		n->face = o->face;
		n->president_name_1 = o->pres_name_1;
		n->president_name_2 = o->pres_name_2;

		n->money64 = n->player_money = o->money;
		n->current_loan = o->loan;

		// Correct money for scenario loading.
		// It's always 893288 pounds (and no loan), if not corrected
		if(o->money==0xda168)
			n->money64 = n->player_money = n->current_loan =100000;

		n->player_color = o->color;
		_player_colors[x] = o->color;
		x++;

		n->player_money_fraction = o->money_fract;
		n->quarters_of_bankrupcy = o->quarters_of_bankrupcy;
		n->bankrupt_asked = o->bankrupt_asked;
		n->bankrupt_value = o->bankrupt_value;
		n->bankrupt_timeout = o->bankrupt_timeout;
		n->cargo_types = o->cargo_types;

		for(i=0; i!=3; i++)
			for(j=0; j!=13; j++)
				n->yearly_expenses[i][j] = o->expenses[i].cost[j];

		FixEconomy(&n->cur_economy, &o->economy[0]);
		for(i=0; i!=24; i++) FixEconomy(&n->old_economy[i], &o->economy[i+1]);
		n->inaugurated_year = o->inaugurated_date - 1920;
		n->last_build_coordinate = o->last_build_coordinate;
		n->num_valid_stat_ent = o->num_valid_stat_ent;

		/*	Not good, since AI doesn't have a vehicle assigned as
		 *	in p->ai.cur_veh and thus will crash on certain actions.
		 *	Best is to set state to AiStateVehLoop (2)
		 *	n->ai.state = o->ai_state;
		 */
		n->ai.state = 2;
		n->ai.state_mode = o->ai_state_mode;
		n->ai.state_counter = o->ai_state_counter;
		n->ai.timeout_counter = o->ai_timeout_counter;
		n->ai.banned_tile_count = o->ai_banned_tile_count;
		n->ai.railtype_to_use = o->ai_railtype_to_use;

		FixAiBuildRec(&n->ai.src, &o->ai_src);
		FixAiBuildRec(&n->ai.dst, &o->ai_dst);
		FixAiBuildRec(&n->ai.mid1, &o->ai_mid1);
		FixAiBuildRec(&n->ai.mid2, &o->ai_mid2);

		n->ai.cargo_type = o->ai_cargo_type;
		n->ai.num_wagons = o->ai_num_wagons;
		n->ai.num_build_rec = o->ai_num_build_rec;
		n->ai.num_loco_to_build = o->ai_num_loco_to_build;
		n->ai.num_want_fullload = o->ai_num_want_fullload;

		for(i=0; i!=9; i++) n->ai.wagon_list[i] = o->ai_wagonlist[i];
		memcpy(n->ai.order_list_blocks, o->ai_order_list_blocks, 20);
		n->ai.start_tile_a = o->ai_start_tile_a;
		n->ai.start_tile_b = o->ai_start_tile_b;
		n->ai.cur_tile_a = o->ai_cur_tile_a;
		n->ai.cur_tile_b = o->ai_cur_tile_b;
		n->ai.start_dir_a = o->ai_start_dir_a;
		n->ai.start_dir_b = o->ai_start_dir_b;
		n->ai.cur_dir_a = o->ai_cur_dir_a;
		n->ai.cur_dir_b = o->ai_cur_dir_b;

		for(i=0; i!=16; i++) {
			n->ai.banned_tiles[i] = o->banned_tiles[i].tile;
			n->ai.banned_val[i] = o->banned_tiles[i].data;
		}

		n->ai.build_kind = o->ai_build_kind;
		n->ai.route_type_mask = o->ai_route_type_mask;
		n->ai.tick = o->ai_tick;

		n->block_preview = o->block_preview;
		n->max_railtype = (o->max_railtype == 0) ? 1 : o->max_railtype;
		n->location_of_house = o->location_of_house;
		if (o->location_of_house == 0xFFFF) n->location_of_house = 0;

		n->share_owners[0] = o->share_owners[0];
		n->share_owners[1] = o->share_owners[1];
		n->share_owners[2] = o->share_owners[2];
		n->share_owners[3] = o->share_owners[3];

		if (o->ai_state == 2) {
			n->ai.cur_veh = NULL;
		}
	} while (n++,o++,--num);
}

static void FixName(OldName *o, int num)
{
	int i;
	for(i=0; i!=num; i++) {
		memcpy(_name_array[i], o[i].name, sizeof(o[i].name));
	}
}

static void FixSign(SignStruct *n, OldSign *o, int num)
{
	do {
		n->str = o->text;
		n->x = o->x;
		n->y = o->y;
		n->z = o->z;
	} while (n++,o++,--num);
}

static void FixEngine(Engine *n, OldEngine *o, int num)
{
	int i = 0;

	do {
		n->player_avail = o->player_avail;
		n->intro_date = o->intro_date;
		n->age = o->age;
		if ((i >= 27 && i < 54) || (i >= 57 && i < 84) || (i >= 89 && i < 116))
			n->age = 0xffff;
		n->reliability = o->reliability;
		n->reliability_spd_dec = o->reliability_spd_dec;
		n->reliability_start = o->reliability_start;
		n->reliability_max = o->reliability_max;
		n->reliability_final = o->reliability_final;
		n->duration_phase_1 = o->duration_phase_1;
		n->duration_phase_2 = o->duration_phase_2;
		n->duration_phase_3 = o->duration_phase_3;
		n->lifelength = o->lifelength;
		n->flags = o->flags;
		n->preview_player = o->preview_player;
		n->preview_wait = o->preview_wait;
		n->railtype = o->railtype;
	} while (n++,o++,i++,--num);
}

static void FixGameDifficulty(GameDifficulty *n, OldGameSettings *o)
{
	n->max_no_competitors = o->max_no_competitors;
	n->competitor_start_time = o->competitor_start_time;
	n->number_towns = o->number_towns;
	n->number_industries = o->number_industries;
	n->max_loan = o->max_loan;
	n->initial_interest = o->initial_interest;
	n->vehicle_costs = o->vehicle_costs;
	n->competitor_speed = o->competitor_speed;
	n->competitor_intelligence = o->competitor_intelligence;
	n->vehicle_breakdowns = o->vehicle_breakdowns;
	n->subsidy_multiplier = o->subsidy_multiplier;
	n->construction_cost = o->construction_cost;
	n->terrain_type = o->terrain_type;
	n->quantity_sea_lakes = o->quantity_sea_lakes;
	n->economy = o->economy;
	n->line_reverse_mode = o->line_reverse_mode;
	n->disasters = o->disasters;
}

#ifdef TTD_BIG_ENDIAN
/*	This function fixes the endiannes issues on Big Endian machines.
 *	Obviously only uint16 (WORD) and uint32 (LONG WORD) 's are fixed
 *	since these are different on Big Endian machines. A single byte has
 *	the same ordening */
static void FixEndianness(OldMain *m)
{
	int i;
	m->date				= BSWAP16(m->date);
	m->date_fract = BSWAP16(m->date_fract);
	m->seed_1			= BSWAP32(m->seed_1);
	m->seed_2			= BSWAP32(m->seed_2);

	/* ----------- TOWNS ----------- */
	for (i = 0; i < 70; i++) {														// OldTown town_list[70];
		int j;
		m->town_list[i].xy							= BSWAP16(m->town_list[i].xy);
		m->town_list[i].population			= BSWAP16(m->town_list[i].population);
		m->town_list[i].townnametype		= BSWAP16(m->town_list[i].townnametype);
		m->town_list[i].townnameparts		= BSWAP32(m->town_list[i].townnameparts);
		m->town_list[i].sign_left				= BSWAP16(m->town_list[i].sign_left);
		m->town_list[i].sign_top				= BSWAP16(m->town_list[i].sign_top);
		m->town_list[i].flags12					= BSWAP16(m->town_list[i].flags12);
		for (j = 0; j < 5; j++)															// uint16 radius[5];
			m->town_list[i].radius[j]			= BSWAP16(m->town_list[i].radius[j]);
		for (j = 0; j < 8; j++)															// uint16 ratings[8];
			m->town_list[i].ratings[j]		= BSWAP16(m->town_list[i].ratings[j]);
		m->town_list[i].have_ratings		= BSWAP32(m->town_list[i].have_ratings);
		m->town_list[i].statues					= BSWAP32(m->town_list[i].statues);
		m->town_list[i].num_houses			= BSWAP16(m->town_list[i].num_houses);
		m->town_list[i].new_max_pass		= BSWAP16(m->town_list[i].new_max_pass);
		m->town_list[i].new_max_mail		= BSWAP16(m->town_list[i].new_max_mail);
		m->town_list[i].new_act_pass		= BSWAP16(m->town_list[i].new_act_pass);
		m->town_list[i].new_act_mail		= BSWAP16(m->town_list[i].new_act_mail);
		m->town_list[i].max_pass				= BSWAP16(m->town_list[i].max_pass);
		m->town_list[i].max_mail				= BSWAP16(m->town_list[i].max_mail);
		m->town_list[i].act_pass				= BSWAP16(m->town_list[i].act_pass);
		m->town_list[i].act_mail				= BSWAP16(m->town_list[i].act_mail);
		m->town_list[i].new_act_food		= BSWAP16(m->town_list[i].new_act_food);
		m->town_list[i].new_act_water		= BSWAP16(m->town_list[i].new_act_water);
		m->town_list[i].act_food				= BSWAP16(m->town_list[i].act_food);
		m->town_list[i].act_water				= BSWAP16(m->town_list[i].act_water);
		m->town_list[i].unk56						= BSWAP32(m->town_list[i].unk56);
		m->town_list[i].unk5A						= BSWAP32(m->town_list[i].unk5A);
	}

	/* ----------- ORDER LIST ----------- */
	for (i = 0; i < 5000; i++)														// uint16 order_list[5000];
		m->order_list[i]							= BSWAP16(m->order_list[i]);

	/* ----------- ANIMATED TILE LIST ----------- */
	for (i = 0; i < 256; i++)															// uint16 animated_tile_list[256];
		m->animated_tile_list[i]			= BSWAP16(m->animated_tile_list[i]);

	m->ptr_to_next_order						= BSWAP32(m->ptr_to_next_order);

	/* ----------- DEPOTS ----------- */
	for (i = 0; i < 255; i++) {														// OldDepot depots[255];
		m->depots[i].xy								= BSWAP16(m->depots[i].xy);
		m->depots[i].town							= BSWAP32(m->depots[i].town);
	}

	m->cur_town_ptr									= BSWAP32(m->cur_town_ptr);
	m->timer_counter								= BSWAP16(m->timer_counter);
	m->land_code										= BSWAP16(m->land_code);
	m->age_cargo_skip_counter				= BSWAP16(m->age_cargo_skip_counter);
	m->tick_counter									= BSWAP16(m->tick_counter);
	m->cur_tileloop_tile						= BSWAP16(m->cur_tileloop_tile);

	/* ----------- PRICES ----------- */
	for (i = 0; i < 49; i++) {														// OldPrice prices[49];
		m->prices[i].price						= BSWAP32(m->prices[i].price);
		m->prices[i].frac							= BSWAP16(m->prices[i].frac);
	}

	/* ----------- CARGO PAYMENT RATES ----------- */
	for (i = 0; i < 12; i++) {														// OldPaymentRate cargo_payment_rates[12];
		m->cargo_payment_rates[i].price		= BSWAP32(m->cargo_payment_rates[i].price);
		m->cargo_payment_rates[i].frac		= BSWAP16(m->cargo_payment_rates[i].frac);
		m->cargo_payment_rates[i].unused	= BSWAP16(m->cargo_payment_rates[i].unused);
	}

	/* ----------- MAP3 ----------- */
	for (i = 0; i < (256*256); i++)												// uint16 map3[256*256];
		m->map3[i] = BSWAP16(m->map3[i]);

	/* ----------- STATIONS ----------- */
	for (i = 0; i < 250; i++) {														// OldStation stations[250];
		int j;
		m->stations[i].xy							= BSWAP16(m->stations[i].xy);
		m->stations[i].town						= BSWAP32(m->stations[i].town);
		m->stations[i].bus_tile				= BSWAP16(m->stations[i].bus_tile);
		m->stations[i].lorry_tile			= BSWAP16(m->stations[i].lorry_tile);
		m->stations[i].train_tile			= BSWAP16(m->stations[i].train_tile);
		m->stations[i].airport_tile		= BSWAP16(m->stations[i].airport_tile);
		m->stations[i].dock_tile			= BSWAP16(m->stations[i].dock_tile);
		m->stations[i].string_id			= BSWAP16(m->stations[i].string_id);
		m->stations[i].sign_left			= BSWAP16(m->stations[i].sign_left);
		m->stations[i].sign_top				= BSWAP16(m->stations[i].sign_top);
		m->stations[i].had_vehicle_of_type						= BSWAP16(m->stations[i].had_vehicle_of_type);
		for (j = 0; j < 12; j++)														// OldGoodsEntry goods[12];
			m->stations[i].goods[j].waiting_acceptance	= BSWAP16(m->stations[i].goods[j].waiting_acceptance);
		m->stations[i].airport_flags	= BSWAP16(m->stations[i].airport_flags);
		m->stations[i].last_vehicle		= BSWAP16(m->stations[i].last_vehicle);
		m->stations[i].unk8A					= BSWAP32(m->stations[i].unk8A);
	}

	/* ----------- INDUSTRIES ----------- */
	for (i = 0; i < 90; i++) {														// OldIndustry industries[90];
		m->industries[i].xy											= BSWAP16(m->industries[i].xy);
		m->industries[i].town										= BSWAP32(m->industries[i].town);
		m->industries[i].cargo_waiting[0]				= BSWAP16(m->industries[i].cargo_waiting[0]);
		m->industries[i].cargo_waiting[1]				= BSWAP16(m->industries[i].cargo_waiting[1]);

		m->industries[i].last_mo_production[0]	= BSWAP16(m->industries[i].last_mo_production[0]);
		m->industries[i].last_mo_production[1]	= BSWAP16(m->industries[i].last_mo_production[1]);

		m->industries[i].last_mo_transported[0]	= BSWAP16(m->industries[i].last_mo_transported[0]);
		m->industries[i].last_mo_transported[1]	= BSWAP16(m->industries[i].last_mo_transported[1]);

		m->industries[i].total_production[0]		= BSWAP16(m->industries[i].total_production[0]);
		m->industries[i].total_production[1]		= BSWAP16(m->industries[i].total_production[1]);

		m->industries[i].total_transported[0]		= BSWAP16(m->industries[i].total_transported[0]);
		m->industries[i].total_transported[1]		= BSWAP16(m->industries[i].total_transported[1]);
		m->industries[i].counter								= BSWAP16(m->industries[i].counter);
		m->industries[i].unk2E									= BSWAP32(m->industries[i].unk2E);
		m->industries[i].unk32									= BSWAP32(m->industries[i].unk32);
	}

	/* ----------- PLAYERS ----------- */
	for (i = 0; i < 8; i++) {															// OldPlayer players[8];
		int j, k;
		m->players[i].name_1						= BSWAP16(m->players[i].name_1);
		m->players[i].name_2						= BSWAP32(m->players[i].name_2);
		m->players[i].face							= BSWAP32(m->players[i].face);
		m->players[i].pres_name_1				= BSWAP16(m->players[i].pres_name_1);
		m->players[i].pres_name_2				= BSWAP32(m->players[i].pres_name_2);
		m->players[i].money							= BSWAP32(m->players[i].money);
		m->players[i].loan							= BSWAP32(m->players[i].loan);
		m->players[i].bankrupt_value		= BSWAP32(m->players[i].bankrupt_value);
		m->players[i].bankrupt_timeout	= BSWAP16(m->players[i].bankrupt_timeout);
		m->players[i].cargo_types				= BSWAP32(m->players[i].cargo_types);

		for (j = 0; j < 3; j++) {														// OldPlayerExpenses expenses[3];
			for (k = 0; k < 13; k++)
				m->players[i].expenses[j].cost[k]	= BSWAP32(m->players[i].expenses[j].cost[k]);
		}

		for (j = 0; j < (24 + 1); j++) {										// OldPlayerEconomy economy[24 + 1];
			m->players[i].economy->income								= BSWAP32(m->players[i].economy->income);
			m->players[i].economy->expenses							= BSWAP32(m->players[i].economy->expenses);
			m->players[i].economy->delivered_cargo			= BSWAP32(m->players[i].economy->delivered_cargo);
			m->players[i].economy->performance_history	= BSWAP32(m->players[i].economy->performance_history);
			m->players[i].economy->company_value				= BSWAP32(m->players[i].economy->company_value);
		}

		m->players[i].inaugurated_date			= BSWAP16(m->players[i].inaugurated_date);
		m->players[i].last_build_coordinate	= BSWAP16(m->players[i].last_build_coordinate);
		m->players[i].ai_state_counter			= BSWAP16(m->players[i].ai_state_counter);
		m->players[i].ai_timeout_counter		= BSWAP16(m->players[i].ai_timeout_counter);

		// OldAiBuildRec ai_src, ai_dst, ai_mid1, ai_mid2;
		m->players[i].ai_src.spec_tile			= BSWAP16(m->players[i].ai_src.spec_tile);
		m->players[i].ai_src.use_tile				= BSWAP16(m->players[i].ai_src.use_tile);
		m->players[i].ai_dst.spec_tile			= BSWAP16(m->players[i].ai_dst.spec_tile);
		m->players[i].ai_dst.use_tile				= BSWAP16(m->players[i].ai_dst.use_tile);
		m->players[i].ai_mid1.spec_tile			= BSWAP16(m->players[i].ai_mid1.spec_tile);
		m->players[i].ai_mid1.use_tile			= BSWAP16(m->players[i].ai_mid1.use_tile);
		m->players[i].ai_mid2.spec_tile			= BSWAP16(m->players[i].ai_mid2.spec_tile);
		m->players[i].ai_mid2.use_tile			= BSWAP16(m->players[i].ai_mid2.use_tile);

		m->players[i].ai_loco_id						= BSWAP16(m->players[i].ai_loco_id);

		for (j = 0; j < 9; j++)
			m->players[i].ai_wagonlist[j]			= BSWAP16(m->players[i].ai_wagonlist[j]);
		m->players[i].ai_start_tile_a				= BSWAP16(m->players[i].ai_start_tile_a);
		m->players[i].ai_start_tile_b				= BSWAP16(m->players[i].ai_start_tile_b);
		m->players[i].ai_cur_tile_a					= BSWAP16(m->players[i].ai_cur_tile_a);
		m->players[i].ai_cur_tile_b					= BSWAP16(m->players[i].ai_cur_tile_b);
		for (j = 0; j < 16; j++)														// OldAiBannedTile banned_tiles[16];
			m->players[i].banned_tiles[j].tile= BSWAP16(m->players[i].banned_tiles[j].tile);
		m->players[i].location_of_house			= BSWAP16(m->players[i].location_of_house);
		m->players[i].unk3AA								= BSWAP32(m->players[i].unk3AA);
		m->players[i].unk3AE								= BSWAP32(m->players[i].unk3AE);
	}

	/* ----------- VEHICLES ----------- */
	for (i = 0; i < 850; i++) {														// OldVehicle vehicles[850];
		m->vehicles[i].next_hash						= BSWAP16(m->vehicles[i].next_hash);
		m->vehicles[i].index								= BSWAP16(m->vehicles[i].index);
		m->vehicles[i].schedule_ptr					= BSWAP32(m->vehicles[i].schedule_ptr);
		m->vehicles[i].dest_tile						= BSWAP16(m->vehicles[i].dest_tile);
		m->vehicles[i].load_unload_time_rem	= BSWAP16(m->vehicles[i].load_unload_time_rem);
		m->vehicles[i].date_of_last_service	= BSWAP16(m->vehicles[i].date_of_last_service);
		m->vehicles[i].service_interval			= BSWAP16(m->vehicles[i].service_interval);
		m->vehicles[i].max_speed						= BSWAP16(m->vehicles[i].max_speed);
		m->vehicles[i].x_pos								= BSWAP16(m->vehicles[i].x_pos);
		m->vehicles[i].y_pos								= BSWAP16(m->vehicles[i].y_pos);
		m->vehicles[i].tile									= BSWAP16(m->vehicles[i].tile);
		m->vehicles[i].cur_image						= BSWAP16(m->vehicles[i].cur_image);
		m->vehicles[i].left_coord						= BSWAP16(m->vehicles[i].left_coord);
		m->vehicles[i].right_coord					= BSWAP16(m->vehicles[i].right_coord);
		m->vehicles[i].top_coord						= BSWAP16(m->vehicles[i].top_coord);
		m->vehicles[i].bottom_coord					= BSWAP16(m->vehicles[i].bottom_coord);
		m->vehicles[i].vehstatus						= BSWAP16(m->vehicles[i].vehstatus);
		m->vehicles[i].cur_speed						= BSWAP16(m->vehicles[i].cur_speed);
		m->vehicles[i].capacity							= BSWAP16(m->vehicles[i].capacity);
		m->vehicles[i].number_of_pieces			= BSWAP16(m->vehicles[i].number_of_pieces);
		m->vehicles[i].age_in_days					= BSWAP16(m->vehicles[i].age_in_days);
		m->vehicles[i].max_age_in_days			= BSWAP16(m->vehicles[i].max_age_in_days);
		m->vehicles[i].engine_type					= BSWAP16(m->vehicles[i].engine_type);
		m->vehicles[i].reliability					= BSWAP16(m->vehicles[i].reliability);
		m->vehicles[i].reliability_spd_dec	= BSWAP16(m->vehicles[i].reliability_spd_dec);
		m->vehicles[i].profit_this_year			= BSWAP32(m->vehicles[i].profit_this_year);
		m->vehicles[i].profit_last_year			= BSWAP32(m->vehicles[i].profit_last_year);
		m->vehicles[i].next_in_chain				= BSWAP16(m->vehicles[i].next_in_chain);
		m->vehicles[i].value								= BSWAP32(m->vehicles[i].value);
		m->vehicles[i].string_id						= BSWAP16(m->vehicles[i].string_id);

		// OldVehicleUnion u;
		switch (m->vehicles[i].type) {
		case VEH_Train:
			m->vehicles[i].u.rail.crash_anim_pos			= BSWAP16(m->vehicles[i].u.rail.crash_anim_pos);
			break;
		case VEH_Aircraft:
			m->vehicles[i].u.air.crashed_counter			= BSWAP16(m->vehicles[i].u.air.crashed_counter);
			break;
		case VEH_Road:
			m->vehicles[i].u.road.unk2								= BSWAP16(m->vehicles[i].u.road.unk2);
			m->vehicles[i].u.road.crashed_ctr					= BSWAP16(m->vehicles[i].u.road.crashed_ctr);
			break;
		case VEH_Special:
			m->vehicles[i].u.special.unk0							= BSWAP16(m->vehicles[i].u.special.unk0);
			break;
		case VEH_Disaster:
			m->vehicles[i].u.disaster.image_override	= BSWAP16(m->vehicles[i].u.disaster.image_override);
			m->vehicles[i].u.disaster.unk2						= BSWAP16(m->vehicles[i].u.disaster.unk2);
			break;
		}
	}

	/* ----------- SIGNS ----------- */
	for (i = 0; i < 40; i++) {														// OldSign signs[40];
		m->signs[i].text									= BSWAP16(m->signs[i].text);
		m->signs[i].x											= BSWAP16(m->signs[i].x);
		m->signs[i].y											= BSWAP16(m->signs[i].y);
		m->signs[i].z											= BSWAP16(m->signs[i].z);
		m->signs[i].sign_left							= BSWAP16(m->signs[i].sign_left);
		m->signs[i].sign_top							= BSWAP16(m->signs[i].sign_top);
	}

	/* ----------- ENGINES ----------- */
	for (i = 0; i < 256; i++) {														// OldEngine engines[256];
		m->engines[i].player_avail				= BSWAP16(m->engines[i].player_avail);
		m->engines[i].intro_date					= BSWAP16(m->engines[i].intro_date);
		m->engines[i].age									= BSWAP16(m->engines[i].age);
		m->engines[i].reliability					= BSWAP16(m->engines[i].reliability);
		m->engines[i].reliability_spd_dec	= BSWAP16(m->engines[i].reliability_spd_dec);
		m->engines[i].reliability_start		= BSWAP16(m->engines[i].reliability_start);
		m->engines[i].reliability_max			= BSWAP16(m->engines[i].reliability_max);
		m->engines[i].reliability_final		= BSWAP16(m->engines[i].reliability_final);
		m->engines[i].duration_phase_1		= BSWAP16(m->engines[i].duration_phase_1);
		m->engines[i].duration_phase_2		= BSWAP16(m->engines[i].duration_phase_2);
		m->engines[i].duration_phase_3		= BSWAP16(m->engines[i].duration_phase_3);
	}

	m->vehicle_id_ctr_day								= BSWAP16(m->vehicle_id_ctr_day);

	m->next_competitor_start						= BSWAP16(m->next_competitor_start);
	m->saved_main_scrollpos_x						= BSWAP16(m->saved_main_scrollpos_x);
	m->saved_main_scrollpos_y						= BSWAP16(m->saved_main_scrollpos_y);
	m->saved_main_scrollpos_zoom				= BSWAP16(m->saved_main_scrollpos_zoom);
	m->maximum_loan											= BSWAP32(m->maximum_loan);
	m->maximum_loan_unround							= BSWAP32(m->maximum_loan_unround);
	m->economy_fluct										= BSWAP16(m->economy_fluct);
	m->disaster_delay										= BSWAP16(m->disaster_delay);

	for (i = 0; i < 256; i++)															// uint16 engine_name_strings[256];
		m->engine_name_strings[i]					= BSWAP16(m->engine_name_strings[i]);

	/* ----------- GAME SETTINGS ----------- */
	m->game_diff.max_no_competitors			= BSWAP16(m->game_diff.max_no_competitors);
	m->game_diff.competitor_start_time	= BSWAP16(m->game_diff.competitor_start_time);
	m->game_diff.number_towns						= BSWAP16(m->game_diff.number_towns);
	m->game_diff.number_industries			= BSWAP16(m->game_diff.number_industries);
	m->game_diff.max_loan								= BSWAP16(m->game_diff.max_loan);
	m->game_diff.initial_interest				= BSWAP16(m->game_diff.initial_interest);
	m->game_diff.vehicle_costs					= BSWAP16(m->game_diff.vehicle_costs);
	m->game_diff.competitor_speed				= BSWAP16(m->game_diff.competitor_speed);
	m->game_diff.competitor_intelligence= BSWAP16(m->game_diff.competitor_intelligence);
	m->game_diff.vehicle_breakdowns			= BSWAP16(m->game_diff.vehicle_breakdowns);
	m->game_diff.subsidy_multiplier			= BSWAP16(m->game_diff.subsidy_multiplier);
	m->game_diff.construction_cost			= BSWAP16(m->game_diff.construction_cost);
	m->game_diff.terrain_type						= BSWAP16(m->game_diff.terrain_type);
	m->game_diff.quantity_sea_lakes			= BSWAP16(m->game_diff.quantity_sea_lakes);
	m->game_diff.economy								= BSWAP16(m->game_diff.economy);
	m->game_diff.line_reverse_mode			= BSWAP16(m->game_diff.line_reverse_mode);
	m->game_diff.disasters							= BSWAP16(m->game_diff.disasters);
}
#endif

// loader for old style savegames
bool LoadOldSaveGame(const char *file)
{
	LoadSavegameState lss;
	OldMain *m;
	int i;

	_cur_state = &lss;
	memset(&lss, 0, sizeof(lss));

	lss.fin = fopen(file, "rb");
	if (lss.fin == NULL) return false;

	/*	B - byte 8bit					(1)
	 *	W - word 16bit				(2 bytes)
	 *	L - 'long' word 32bit	(4 bytes)
	 */
	fseek(lss.fin, 49, SEEK_SET); // 47B TITLE, W Checksum - Total 49

	/*	Load the file into memory
	 *	Game Data 0x77179 + L4 (256x256) + L5 (256x256)
	 */
	m = (OldMain *)malloc(sizeof(OldMain));
	LoadSavegameBytes(m, sizeof(OldMain));
	#ifdef TTD_BIG_ENDIAN
	FixEndianness(m);
	#endif

	// copy sections of it to our datastructures.
	memcpy(_map_owner, m->map_owner, sizeof(_map_owner));
	memcpy(_map2, m->map2, sizeof(_map2));
	memcpy(_map_type_and_height, m->map_type_and_height, sizeof(_map_type_and_height));
	memcpy(_map5, m->map5, sizeof(_map5));
	for(i=0; i!=256*256; i++) {
		_map3_lo[i] = m->map3[i] & 0xFF;
		_map3_hi[i] = m->map3[i] >> 8;
	}
	memcpy(_map_extra_bits, m->map_extra, sizeof(_map_extra_bits));

	// go through the tables and see if we can find any ttdpatch presignals. Then convert those to our format.
	for(i=0; i!=256*256; i++) {
		if (IS_TILETYPE(i, MP_RAILWAY) && (_map5[i] & 0xC0) == 0x40) {
			// this byte is always zero in real ttd.
			if (_map3_hi[i]) {
				// convert ttdpatch presignal format to openttd presignal format.
				_map3_hi[i] = (_map3_hi[i] >> 1) & 7;
			}
		}
	}

	memcpy(_order_array, m->order_list, sizeof(m->order_list));
	_ptr_to_next_order = _order_array + REMAP_ORDER_IDX(m->ptr_to_next_order);

	FixTown(_towns, m->town_list, lengthof(m->town_list), m->town_name_type);
	FixIndustry(_industries, m->industries, lengthof(m->industries));
	FixStation(_stations, m->stations, lengthof(m->stations));

	FixDepot(_depots, m->depots, lengthof(m->depots));
	FixVehicle(_vehicles, m->vehicles, lengthof(m->vehicles));
	FixSubsidy(_subsidies, m->subsidies, lengthof(m->subsidies));

	FixPlayer(_players, m->players, lengthof(m->players), m->town_name_type);
	FixName(m->names, lengthof(m->names));
	FixSign(_sign_list, m->signs, lengthof(m->signs));
	FixEngine(_engines, m->engines, lengthof(m->engines));

	_opt.diff_level = m->difficulty_level;
	_opt.currency = m->currency;
	_opt.kilometers = m->use_kilometers;
	_opt.town_name = m->town_name_type;
	_opt.landscape = m->landscape_type & 0xf;
	_opt.snow_line = m->snow_line_height;
	_opt.autosave = 0;
	_opt.road_side = m->road_side;
	FixGameDifficulty(&_opt.diff, &m->game_diff);

	// Load globals
	_date = m->date;
	_date_fract = m->date_fract;
	_tick_counter = m->tick_counter;
	_vehicle_id_ctr_day = m->vehicle_id_ctr_day;
	_age_cargo_skip_counter = m->age_cargo_skip_counter;
	_avail_aircraft = m->avail_aircraft;
	_cur_tileloop_tile = m->cur_tileloop_tile;
	_disaster_delay = m->disaster_delay;
	_station_tick_ctr = m->station_tick_ctr;
	_random_seeds[0][0] = m->seed_1;
	_random_seeds[0][1] = m->seed_2;
	_cur_town_ctr = REMAP_TOWN_IDX(m->cur_town_ptr);
	_cur_player_tick_index = m->cur_player_tick_index;
	_next_competitor_start = m->next_competitor_start;
	_trees_tick_ctr = m->trees_tick_ctr;
	_saved_scrollpos_x = m->saved_main_scrollpos_x;
	_saved_scrollpos_y = m->saved_main_scrollpos_y;
	_saved_scrollpos_zoom = m->saved_main_scrollpos_zoom;

	// Load economy stuff
	_economy.max_loan = m->maximum_loan;
	_economy.max_loan_unround = m->maximum_loan_unround;
	_economy.fluct = m->economy_fluct;
	_economy.interest_rate = m->interest_rate;
	_economy.infl_amount = m->inflation_amount;
	_economy.infl_amount_pr = m->inflation_amount_payment_rates;

	memcpy(_animated_tile_list, m->animated_tile_list, sizeof(m->animated_tile_list));
	memcpy(_engine_name_strings, m->engine_name_strings, sizeof(m->engine_name_strings));

	for(i=0; i!=lengthof(m->prices); i++) {
		((uint32*)&_price)[i] = m->prices[i].price;
		_price_frac[i] = m->prices[i].frac;
	}

	for(i=0; i!=lengthof(m->cargo_payment_rates); i++) {
		_cargo_payment_rates[i] = -(int32)m->cargo_payment_rates[i].price;
		_cargo_payment_rates_frac[i] = m->cargo_payment_rates[i].frac;
	}

	free(m);
	fclose(lss.fin);
	return true;
}

void GetOldSaveGameName(char *title, const char *file)
{
	FILE *f;

	f = fopen(file, "rb");
	title[0] = 0;
	title[48] = 0;

	if (!f) return;
	if (fread(title, 1, 48, f) != 48) title[0] = 0;
	fclose(f);
}

void GetOldScenarioGameName(char *title, const char *file)
{
	GetOldSaveGameName(title, file);
}
