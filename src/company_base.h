/* $Id$ */

/** @file company_base.h Definition of stuff that is very close to a company, like the company struct itself. */

#ifndef COMPANY_BASE_H
#define COMPANY_BASE_H

#include "company_type.h"
#include "core/pool_type.hpp"
#include "road_type.h"
#include "rail_type.h"
#include "date_type.h"
#include "engine_type.h"
#include "livery.h"
#include "autoreplace_type.h"
#include "economy_type.h"
#include "tile_type.h"
#include "settings_type.h"

struct CompanyEconomyEntry {
	Money income;
	Money expenses;
	int32 delivered_cargo;
	int32 performance_history; ///< company score (scale 0-1000)
	Money company_value;
};

typedef Pool<Company, CompanyByte, 1, MAX_COMPANIES> CompanyPool;
extern CompanyPool _company_pool;


struct Company : CompanyPool::PoolItem<&_company_pool> {
	Company(uint16 name_1 = 0, bool is_ai = false);
	~Company();

	uint32 name_2;
	uint16 name_1;
	char *name;

	uint16 president_name_1;
	uint32 president_name_2;
	char *president_name;

	CompanyManagerFace face;

	Money money;
	byte money_fraction;
	Money current_loan;

	byte colour;
	Livery livery[LS_END];
	RailTypes avail_railtypes;
	RoadTypes avail_roadtypes;
	byte block_preview;

	uint32 cargo_types; ///< which cargo types were transported the last year

	TileIndex location_of_HQ; ///< northern tile of HQ ; INVALID_TILE when there is none
	TileIndex last_build_coordinate;

	OwnerByte share_owners[4];

	Year inaugurated_year;
	byte num_valid_stat_ent;

	byte quarters_of_bankrupcy;
	CompanyMask bankrupt_asked; ///< which companies were asked about buying it?
	int16 bankrupt_timeout;
	Money bankrupt_value;

	bool is_ai;

	class AIInstance *ai_instance;
	class AIInfo *ai_info;

	Money yearly_expenses[3][EXPENSES_END];
	CompanyEconomyEntry cur_economy;
	CompanyEconomyEntry old_economy[MAX_HISTORY_MONTHS];
	EngineRenewList engine_renew_list; ///< Defined later
	CompanySettings settings;          ///< settings specific for each company
	uint16 *num_engines; ///< caches the number of engines of each type the company owns (no need to save this)

	static FORCEINLINE bool IsValidAiID(size_t index)
	{
		const Company *c = Company::GetIfValid(index);
		return c != NULL && c->is_ai;
	}

	static FORCEINLINE bool IsValidHumanID(size_t index)
	{
		const Company *c = Company::GetIfValid(index);
		return c != NULL && !c->is_ai;
	}

	static FORCEINLINE bool IsHumanID(size_t index)
	{
		return !Company::Get(index)->is_ai;
	}
};

#define FOR_ALL_COMPANIES_FROM(var, start) FOR_ALL_ITEMS_FROM(Company, company_index, var, start)
#define FOR_ALL_COMPANIES(var) FOR_ALL_COMPANIES_FROM(var, 0)

Money CalculateCompanyValue(const Company *c);

extern uint _next_competitor_start;
extern uint _cur_company_tick_index;

#endif /* COMPANY_BASE_H */
