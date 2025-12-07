/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file company_base.h Definition of stuff that is very close to a company, like the company struct itself. */

#ifndef COMPANY_BASE_H
#define COMPANY_BASE_H

#include "road_type.h"
#include "livery.h"
#include "autoreplace_type.h"
#include "tile_type.h"
#include "timer/timer_game_economy.h"
#include "settings_type.h"
#include "group.h"

static const Money COMPANY_MAX_LOAN_DEFAULT = INT64_MIN;

/** Statistics about the economy. */
struct CompanyEconomyEntry {
	Money income = 0; ///< The amount of income.
	Money expenses = 0; ///< The amount of expenses.
	CargoArray delivered_cargo{}; ///< The amount of delivered cargo.
	int32_t performance_history = 0; ///< Company score (scale 0-1000)
	Money company_value = 0; ///< The value of the company.
};

struct CompanyInfrastructure {
	std::array<uint32_t, RAILTYPE_END> rail{}; ///< Count of company owned track bits for each rail type.
	std::array<uint32_t, ROADTYPE_END> road{}; ///< Count of company owned track bits for each road type.
	uint32_t signal = 0; ///< Count of company owned signals.
	uint32_t water = 0; ///< Count of company owned track bits for canals.
	uint32_t station = 0; ///< Count of company owned station tiles.
	uint32_t airport = 0; ///< Count of company owned airports.

	auto operator<=>(const CompanyInfrastructure &) const = default;

	/** Get total sum of all owned track bits. */
	uint32_t GetRailTotal() const
	{
		return std::accumulate(std::begin(this->rail), std::end(this->rail), 0U);
	}

	uint32_t GetRoadTramTotal(RoadTramType rtt) const;

	inline uint32_t GetRoadTotal() const { return GetRoadTramTotal(RTT_ROAD); }
	inline uint32_t GetTramTotal() const { return GetRoadTramTotal(RTT_TRAM); }
};

class FreeUnitIDGenerator {
public:
	UnitID NextID() const;
	UnitID UseID(UnitID index);
	void ReleaseID(UnitID index);

private:
	using BitmapStorage = size_t;
	static constexpr size_t BITMAP_SIZE = std::numeric_limits<BitmapStorage>::digits;

	std::vector<BitmapStorage> used_bitmap{};
};

typedef Pool<Company, CompanyID, 1> CompanyPool;
extern CompanyPool _company_pool;

/** Statically loadable part of Company pool item */
struct CompanyProperties {
	uint32_t name_2 = 0; ///< Parameter of #name_1.
	StringID name_1 = INVALID_STRING_ID; ///< Name of the company if the user did not change it.
	std::string name{}; ///< Name of the company if the user changed it.

	StringID president_name_1 = INVALID_STRING_ID; ///< Name of the president if the user did not change it.
	uint32_t president_name_2 = 0; ///< Parameter of #president_name_1
	std::string president_name{}; ///< Name of the president if the user changed it.

	NetworkAuthorizedKeys allow_list{}; ///< Public keys of clients that are allowed to join this company.

	CompanyManagerFace face{}; ///< Face description of the president.

	Money money = 0; ///< Money owned by the company.
	uint8_t money_fraction = 0; ///< Fraction of money of the company, too small to represent in #money.
	Money current_loan = 0; ///< Amount of money borrowed from the bank.
	Money max_loan = COMPANY_MAX_LOAN_DEFAULT; ///< Max allowed amount of the loan or COMPANY_MAX_LOAN_DEFAULT.

	Colours colour = COLOUR_BEGIN; ///< Company colour.

	uint8_t block_preview = 0; ///< Number of quarters that the company is not allowed to get new exclusive engine previews (see CompaniesGenStatistics).

	TileIndex location_of_HQ = INVALID_TILE; ///< Northern tile of HQ; #INVALID_TILE when there is none.
	TileIndex last_build_coordinate{}; ///< Coordinate of the last build thing by this company.

	TimerGameEconomy::Year inaugurated_year{}; ///< Economy year of starting the company.
	TimerGameCalendar::Year inaugurated_year_calendar{}; ///< Calendar year of starting the company. Used to display proper Inauguration year while in wallclock mode.

	uint8_t months_empty = 0; ///< NOSAVE: Number of months this company has not had a client in multiplayer.
	uint8_t months_of_bankruptcy = 0; ///< Number of months that the company is unable to pay its debts
	CompanyMask bankrupt_asked{}; ///< which companies were asked about buying it?
	int16_t bankrupt_timeout = 0; ///< If bigger than \c 0, amount of time to wait for an answer on an offer to buy this company.
	Money bankrupt_value = 0;

	uint32_t terraform_limit = 0; ///< Amount of tileheights we can (still) terraform (times 65536).
	uint32_t clear_limit = 0; ///< Amount of tiles we can (still) clear (times 65536).
	uint32_t tree_limit = 0; ///< Amount of trees we can (still) plant (times 65536).
	uint32_t build_object_limit = 0; ///< Amount of tiles we can (still) build objects on (times 65536). Also applies to buying land.

	/**
	 * If \c true, the company is (also) controlled by the computer (a NoAI program).
	 * @note It is possible that the user is also participating in such a company.
	 */
	bool is_ai = false;

	std::array<Expenses, 3> yearly_expenses{}; ///< Expenses of the company for the last three years.
	CompanyEconomyEntry cur_economy{}; ///< Economic data of the company of this quarter.
	std::array<CompanyEconomyEntry, MAX_HISTORY_QUARTERS> old_economy{}; ///< Economic data of the company of the last #MAX_HISTORY_QUARTERS quarters.
	uint8_t num_valid_stat_ent = 0; ///< Number of valid statistical entries in #old_economy.

	std::array<Livery, LS_END> livery{};

	EngineRenewList engine_renew_list = nullptr; ///< Engine renewals of this company.
	CompanySettings settings{}; ///< settings specific for each company
};

struct Company : CompanyProperties, CompanyPool::PoolItem<&_company_pool> {
	Company(StringID name_1 = {}, bool is_ai = false);
	~Company();

	RailTypes avail_railtypes{}; ///< Rail types available to this company.
	RoadTypes avail_roadtypes{}; ///< Road types available to this company.

	std::unique_ptr<class AIInstance> ai_instance{};
	class AIInfo *ai_info = nullptr;
	std::unique_ptr<class AIConfig> ai_config{};

	std::array<GroupStatistics, VEH_COMPANY_END> group_all{}; ///< NOSAVE: Statistics for the ALL_GROUP group.
	std::array<GroupStatistics, VEH_COMPANY_END> group_default{};  ///< NOSAVE: Statistics for the DEFAULT_GROUP group.

	CompanyInfrastructure infrastructure{}; ///< NOSAVE: Counts of company owned infrastructure.

	std::array<FreeUnitIDGenerator, VEH_COMPANY_END> freeunits{};
	FreeUnitIDGenerator freegroups{};

	Money GetMaxLoan() const;

	/**
	 * Is this company a valid company, controlled by the computer (a NoAI program)?
	 * @param index Index in the pool.
	 * @return \c true if it is a valid, computer controlled company, else \c false.
	 */
	static inline bool IsValidAiID(auto index)
	{
		const Company *c = Company::GetIfValid(index);
		return c != nullptr && c->is_ai;
	}

	/**
	 * Is this company a valid company, not controlled by a NoAI program?
	 * @param index Index in the pool.
	 * @return \c true if it is a valid, human controlled company, else \c false.
	 * @note If you know that \a index refers to a valid company, you can use #IsHumanID() instead.
	 */
	static inline bool IsValidHumanID(auto index)
	{
		const Company *c = Company::GetIfValid(index);
		return c != nullptr && !c->is_ai;
	}

	/**
	 * Is this company a company not controlled by a NoAI program?
	 * @param index Index in the pool.
	 * @return \c true if it is a human controlled company, else \c false.
	 * @pre \a index must be a valid CompanyID.
	 * @note If you don't know whether \a index refers to a valid company, you should use #IsValidHumanID() instead.
	 */
	static inline bool IsHumanID(auto index)
	{
		return !Company::Get(index)->is_ai;
	}

	/**
	 * Get offset for recolour palette of specific company.
	 * @param livery_scheme Scheme to use for recolour.
	 * @param use_secondary Specify whether to add secondary colour offset to the result.
	 * @return palette offset.
	 */
	inline uint8_t GetCompanyRecolourOffset(LiveryScheme livery_scheme, bool use_secondary = true) const
	{
		const Livery &l = this->livery[livery_scheme];
		return use_secondary ? l.colour1 + l.colour2 * 16 : l.colour1;
	}

	static void PostDestructor(size_t index);
};

Money CalculateCompanyValue(const Company *c, bool including_loan = true);
Money CalculateHostileTakeoverValue(const Company *c);

extern uint _cur_company_tick_index;

#endif /* COMPANY_BASE_H */
