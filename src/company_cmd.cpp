/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file company_cmd.cpp Handling of companies. */

#include "stdafx.h"
#include "company_base.h"
#include "company_func.h"
#include "company_gui.h"
#include "core/backup_type.hpp"
#include "town.h"
#include "news_func.h"
#include "command_func.h"
#include "network/network.h"
#include "network/network_func.h"
#include "network/network_base.h"
#include "network/network_admin.h"
#include "ai/ai.hpp"
#include "ai/ai_instance.hpp"
#include "ai/ai_config.hpp"
#include "company_manager_face.h"
#include "window_func.h"
#include "strings_func.h"
#include "sound_func.h"
#include "rail.h"
#include "core/pool_func.hpp"
#include "core/string_consumer.hpp"
#include "settings_func.h"
#include "vehicle_base.h"
#include "vehicle_func.h"
#include "smallmap_gui.h"
#include "game/game.hpp"
#include "goal_base.h"
#include "story_base.h"
#include "company_cmd.h"
#include "timer/timer.h"
#include "timer/timer_game_economy.h"
#include "timer/timer_game_tick.h"
#include "timer/timer_window.h"

#include "widgets/statusbar_widget.h"

#include "table/strings.h"
#include "table/company_face.h"

#include "safeguards.h"

void ClearEnginesHiddenFlagOfCompany(CompanyID cid);
void UpdateObjectColours(const Company *c);

CompanyID _local_company;   ///< Company controlled by the human player at this client. Can also be #COMPANY_SPECTATOR.
CompanyID _current_company; ///< Company currently doing an action.
TypedIndexContainer<std::array<Colours, MAX_COMPANIES>, CompanyID> _company_colours; ///< NOSAVE: can be determined from company structs.
std::string _company_manager_face; ///< for company manager face storage in openttd.cfg
uint _cur_company_tick_index;             ///< used to generate a name for one company that doesn't have a name yet per tick

CompanyPool _company_pool("Company"); ///< Pool of companies.
INSTANTIATE_POOL_METHODS(Company)

/**
 * Constructor.
 * @param name_1 Name of the company.
 * @param is_ai  A computer program is running for this company.
 */
Company::Company(StringID name_1, bool is_ai)
{
	this->name_1 = name_1;
	this->is_ai = is_ai;
	this->terraform_limit    = (uint32_t)_settings_game.construction.terraform_frame_burst << 16;
	this->clear_limit        = (uint32_t)_settings_game.construction.clear_frame_burst << 16;
	this->tree_limit         = (uint32_t)_settings_game.construction.tree_frame_burst << 16;
	this->build_object_limit = (uint32_t)_settings_game.construction.build_object_frame_burst << 16;

	InvalidateWindowData(WC_PERFORMANCE_DETAIL, 0, CompanyID::Invalid());
}

/** Destructor. */
Company::~Company()
{
	if (CleaningPool()) return;

	CloseCompanyWindows(this->index);
}

/**
 * Invalidating some stuff after removing item from the pool.
 * @param index index of deleted item
 */
void Company::PostDestructor(size_t index)
{
	InvalidateWindowData(WC_GRAPH_LEGEND, 0, (int)index);
	InvalidateWindowData(WC_PERFORMANCE_DETAIL, 0, (int)index);
	InvalidateWindowData(WC_COMPANY_LEAGUE, 0, 0);
	InvalidateWindowData(WC_LINKGRAPH_LEGEND, 0);
	/* If the currently shown error message has this company in it, then close it. */
	InvalidateWindowData(WC_ERRMSG, 0);
}

/**
 * Calculate the max allowed loan for this company.
 * @return the max loan amount.
 */
Money Company::GetMaxLoan() const
{
	if (this->max_loan == COMPANY_MAX_LOAN_DEFAULT) return _economy.max_loan;
	return this->max_loan;
}

/**
 * Sets the local company and updates the settings that are set on a
 * per-company basis to reflect the core's state in the GUI.
 * @param new_company the new company
 * @pre Company::IsValidID(new_company) || new_company == COMPANY_SPECTATOR || new_company == OWNER_NONE
 */
void SetLocalCompany(CompanyID new_company)
{
	/* company could also be COMPANY_SPECTATOR or OWNER_NONE */
	assert(Company::IsValidID(new_company) || new_company == COMPANY_SPECTATOR || new_company == OWNER_NONE);

	/* If actually changing to another company, several windows need closing */
	bool switching_company = _local_company != new_company;

	/* Delete the chat window, if you were team chatting. */
	if (switching_company) InvalidateWindowData(WC_SEND_NETWORK_MSG, DESTTYPE_TEAM, _local_company);

	assert(IsLocalCompany());

	_current_company = _local_company = new_company;

	if (switching_company) {
		InvalidateWindowClassesData(WC_COMPANY);
		/* Delete any construction windows... */
		CloseConstructionWindows();
	}

	/* ... and redraw the whole screen. */
	MarkWholeScreenDirty();
	InvalidateWindowClassesData(WC_SIGN_LIST, -1);
	InvalidateWindowClassesData(WC_GOALS_LIST);
	InvalidateWindowClassesData(WC_COMPANY_COLOUR, -1);
	ResetVehicleColourMap();
}

/**
 * Get the colour for DrawString-subroutines which matches the colour of the company.
 * @param company Company to get the colour of.
 * @return Colour of \a company.
 */
TextColour GetDrawStringCompanyColour(CompanyID company)
{
	if (!Company::IsValidID(company)) return GetColourGradient(COLOUR_WHITE, SHADE_NORMAL).ToTextColour();
	return GetColourGradient(_company_colours[company], SHADE_NORMAL).ToTextColour();
}

/**
 * Get the palette for recolouring with a company colour.
 * @param company Company to get the colour of.
 * @return Palette for recolouring.
 */
PaletteID GetCompanyPalette(CompanyID company)
{
	return GetColourPalette(_company_colours[company]);
}

/**
 * Draw the icon of a company.
 * @param c Company that needs its icon drawn.
 * @param x Horizontal coordinate of the icon.
 * @param y Vertical coordinate of the icon.
 */
void DrawCompanyIcon(CompanyID c, int x, int y)
{
	DrawSprite(SPR_COMPANY_ICON, GetCompanyPalette(c), x, y);
}

/**
 * Checks whether a company manager's face is a valid encoding.
 * Unused bits are not enforced to be 0.
 * @param cmf the fact to check
 * @return true if and only if the face is valid
 */
static bool IsValidCompanyManagerFace(CompanyManagerFace cmf)
{
	if (cmf.style >= GetNumCompanyManagerFaceStyles()) return false;

	/* Test if each enabled part is valid. */
	FaceVars vars = GetCompanyManagerFaceVars(cmf.style);
	for (uint var : SetBitIterator(GetActiveFaceVars(cmf, vars))) {
		if (!vars[var].IsValid(cmf)) return false;
	}

	return true;
}

static CompanyMask _dirty_company_finances{}; ///< Bitmask of company finances that should be marked dirty.

/**
 * Mark all finance windows owned by a company as needing a refresh.
 * The actual refresh is deferred until the end of the gameloop to reduce duplicated work.
 * @param company Company that changed, and needs its windows refreshed.
 */
void InvalidateCompanyWindows(const Company *company)
{
	CompanyID cid = company->index;
	_dirty_company_finances.Set(cid);
}

/**
 * Refresh all company finance windows previously marked dirty.
 */
static const IntervalTimer<TimerWindow> invalidate_company_windows_interval(std::chrono::milliseconds(1), [](auto) {
	for (CompanyID cid : _dirty_company_finances) {
		if (cid == _local_company) SetWindowWidgetDirty(WC_STATUS_BAR, 0, WID_S_RIGHT);
		Window *w = FindWindowById(WC_FINANCES, cid);
		if (w != nullptr) {
			w->SetWidgetDirty(WID_CF_EXPS_PRICE3);
			w->SetWidgetDirty(WID_CF_OWN_VALUE);
			w->SetWidgetDirty(WID_CF_LOAN_VALUE);
			w->SetWidgetDirty(WID_CF_BALANCE_VALUE);
			w->SetWidgetDirty(WID_CF_MAXLOAN_VALUE);
		}
		SetWindowWidgetDirty(WC_COMPANY, cid, WID_C_DESC_COMPANY_VALUE);
	}
	_dirty_company_finances.Reset();
});

/**
 * Get the amount of money that a company has available, or INT64_MAX
 * if there is no such valid company.
 *
 * @param company Company to check
 * @return The available money of the company or INT64_MAX
 */
Money GetAvailableMoney(CompanyID company)
{
	if (_settings_game.difficulty.infinite_money) return INT64_MAX;
	if (!Company::IsValidID(company)) return INT64_MAX;
	return Company::Get(company)->money;
}

/**
 * This functions returns the money which can be used to execute a command.
 * This is either the money of the current company, or INT64_MAX if infinite money
 * is enabled or there is no such a company "at the moment" like the server itself.
 *
 * @return The available money of the current company or INT64_MAX
 */
Money GetAvailableMoneyForCommand()
{
	return GetAvailableMoney(_current_company);
}

/**
 * Verify whether the company can pay the bill.
 * @param[in,out] cost Money to pay, is changed to an error if the company does not have enough money.
 * @return Function returns \c true if the company has enough money or infinite money is enabled,
 * else it returns \c false.
 */
bool CheckCompanyHasMoney(CommandCost &cost)
{
	if (cost.GetCost() <= 0) return true;
	if (_settings_game.difficulty.infinite_money) return true;

	const Company *c = Company::GetIfValid(_current_company);
	if (c != nullptr && cost.GetCost() > c->money) {
		cost.MakeError(STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY);
		if (IsLocalCompany()) {
			cost.SetEncodedMessage(GetEncodedString(STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY, cost.GetCost()));
		}
		return false;
	}
	return true;
}

/**
 * Deduct costs of a command from the money of a company.
 * @param c Company to pay the bill.
 * @param cost Money to pay.
 */
static void SubtractMoneyFromAnyCompany(Company *c, const CommandCost &cost)
{
	if (cost.GetCost() == 0) return;
	assert(cost.GetExpensesType() != INVALID_EXPENSES);

	c->money -= cost.GetCost();
	c->yearly_expenses[0][cost.GetExpensesType()] += cost.GetCost();

	if (HasBit(1 << EXPENSES_TRAIN_REVENUE    |
	           1 << EXPENSES_ROADVEH_REVENUE  |
	           1 << EXPENSES_AIRCRAFT_REVENUE |
	           1 << EXPENSES_SHIP_REVENUE, cost.GetExpensesType())) {
		c->cur_economy.income -= cost.GetCost();
	} else if (HasBit(1 << EXPENSES_TRAIN_RUN    |
	                  1 << EXPENSES_ROADVEH_RUN  |
	                  1 << EXPENSES_AIRCRAFT_RUN |
	                  1 << EXPENSES_SHIP_RUN     |
	                  1 << EXPENSES_PROPERTY     |
	                  1 << EXPENSES_LOAN_INTEREST, cost.GetExpensesType())) {
		c->cur_economy.expenses -= cost.GetCost();
	}

	InvalidateCompanyWindows(c);
}

/**
 * Subtract money from the #_current_company, if the company is valid.
 * @param cost Money to pay.
 */
void SubtractMoneyFromCompany(const CommandCost &cost)
{
	Company *c = Company::GetIfValid(_current_company);
	if (c != nullptr) SubtractMoneyFromAnyCompany(c, cost);
}

/**
 * Subtract money from a company, including the money fraction.
 * @param company Company paying the bill.
 * @param cst     Cost of a command.
 */
void SubtractMoneyFromCompanyFract(CompanyID company, const CommandCost &cst)
{
	Company *c = Company::Get(company);
	uint8_t m = c->money_fraction;
	Money cost = cst.GetCost();

	c->money_fraction = m - (uint8_t)cost;
	cost >>= 8;
	if (c->money_fraction > m) cost++;
	if (cost != 0) SubtractMoneyFromAnyCompany(c, CommandCost(cst.GetExpensesType(), cost));
}

static constexpr void UpdateLandscapingLimit(uint32_t &limit, uint64_t per_64k_frames, uint64_t burst)
{
	limit = static_cast<uint32_t>(std::min<uint64_t>(limit + per_64k_frames, burst << 16));
}

/** Update the landscaping limits per company. */
void UpdateLandscapingLimits()
{
	for (Company *c : Company::Iterate()) {
		UpdateLandscapingLimit(c->terraform_limit,    _settings_game.construction.terraform_per_64k_frames,    _settings_game.construction.terraform_frame_burst);
		UpdateLandscapingLimit(c->clear_limit,        _settings_game.construction.clear_per_64k_frames,        _settings_game.construction.clear_frame_burst);
		UpdateLandscapingLimit(c->tree_limit,         _settings_game.construction.tree_per_64k_frames,         _settings_game.construction.tree_frame_burst);
		UpdateLandscapingLimit(c->build_object_limit, _settings_game.construction.build_object_per_64k_frames, _settings_game.construction.build_object_frame_burst);
	}
}

/**
 * Set the right DParams for STR_ERROR_OWNED_BY.
 * @param owner the owner to get the name of.
 * @param tile  optional tile to get the right town.
 * @pre if tile == 0, then owner can't be OWNER_TOWN.
 */
std::array<StringParameter, 2> GetParamsForOwnedBy(Owner owner, TileIndex tile)
{
	if (owner == OWNER_TOWN) {
		assert(tile != 0);
		const Town *t = ClosestTownFromTile(tile, UINT_MAX);
		return {STR_TOWN_NAME, t->index};
	}

	if (!Company::IsValidID(owner)) {
		return {STR_COMPANY_SOMEONE, std::monostate{}};
	}

	return {STR_COMPANY_NAME, owner};
}

/**
 * Check whether the current owner owns something.
 * If that isn't the case an appropriate error will be given.
 * @param owner the owner of the thing to check.
 * @param tile  optional tile to get the right town.
 * @pre if tile == 0 then the owner can't be OWNER_TOWN.
 * @return A succeeded command iff it's owned by the current company, else a failed command.
 */
CommandCost CheckOwnership(Owner owner, TileIndex tile)
{
	assert(owner < OWNER_END);
	assert(owner != OWNER_TOWN || tile != 0);

	if (owner == _current_company) return CommandCost();

	CommandCost error{STR_ERROR_OWNED_BY};
	if (IsLocalCompany()) {
		auto params = GetParamsForOwnedBy(owner, tile);
		error.SetEncodedMessage(GetEncodedStringWithArgs(STR_ERROR_OWNED_BY, params));
		if (owner != OWNER_TOWN) error.SetErrorOwner(owner);
	}
	return error;
}

/**
 * Check whether the current owner owns the stuff on
 * the given tile.  If that isn't the case an
 * appropriate error will be given.
 * @param tile the tile to check.
 * @return A succeeded command iff it's owned by the current company, else a failed command.
 */
CommandCost CheckTileOwnership(TileIndex tile)
{
	return CheckOwnership(GetTileOwner(tile), tile);
}

/**
 * Generate the name of a company from the last build coordinate.
 * @param c Company to give a name.
 */
static void GenerateCompanyName(Company *c)
{
	if (c->name_1 != STR_SV_UNNAMED) return;
	if (c->last_build_coordinate == 0) return;

	Town *t = ClosestTownFromTile(c->last_build_coordinate, UINT_MAX);

	StringID str;
	uint32_t strp;
	std::string name;
	if (t->name.empty() && IsInsideMM(t->townnametype, SPECSTR_TOWNNAME_START, SPECSTR_TOWNNAME_END)) {
		str = t->townnametype - SPECSTR_TOWNNAME_START + SPECSTR_COMPANY_NAME_START;
		strp = t->townnameparts;

verify_name:;
		/* No companies must have this name already */
		for (const Company *cc : Company::Iterate()) {
			if (cc->name_1 == str && cc->name_2 == strp) goto bad_town_name;
		}

		name = GetString(str, strp);
		if (Utf8StringLength(name) >= MAX_LENGTH_COMPANY_NAME_CHARS) goto bad_town_name;

set_name:;
		c->name_1 = str;
		c->name_2 = strp;

		MarkWholeScreenDirty();
		AI::BroadcastNewEvent(new ScriptEventCompanyRenamed(c->index, name));
		Game::NewEvent(new ScriptEventCompanyRenamed(c->index, name));

		if (c->is_ai) {
			auto cni = std::make_unique<CompanyNewsInformation>(STR_NEWS_COMPANY_LAUNCH_TITLE, c);
			EncodedString headline = GetEncodedString(STR_NEWS_COMPANY_LAUNCH_DESCRIPTION, cni->company_name, t->index);
			AddNewsItem(std::move(headline),
				NewsType::CompanyInfo, NewsStyle::Company, {}, c->last_build_coordinate, {}, std::move(cni));
		}
		return;
	}
bad_town_name:;

	if (c->president_name_1 == SPECSTR_PRESIDENT_NAME) {
		str = SPECSTR_ANDCO_NAME;
		strp = c->president_name_2;
		name = GetString(str, strp);
		goto set_name;
	} else {
		str = SPECSTR_ANDCO_NAME;
		strp = Random();
		goto verify_name;
	}
}

/** Sorting weights for the company colours. */
static const uint8_t _colour_sort[COLOUR_END] = {2, 2, 3, 2, 3, 2, 3, 2, 3, 2, 2, 2, 3, 1, 1, 1};
/** Similar colours, so we can try to prevent same coloured companies. */
static const Colours _similar_colour[COLOUR_END][2] = {
	{ COLOUR_BLUE,       COLOUR_LIGHT_BLUE }, // COLOUR_DARK_BLUE
	{ COLOUR_GREEN,      COLOUR_DARK_GREEN }, // COLOUR_PALE_GREEN
	{ INVALID_COLOUR,    INVALID_COLOUR    }, // COLOUR_PINK
	{ COLOUR_ORANGE,     INVALID_COLOUR    }, // COLOUR_YELLOW
	{ INVALID_COLOUR,    INVALID_COLOUR    }, // COLOUR_RED
	{ COLOUR_DARK_BLUE,  COLOUR_BLUE       }, // COLOUR_LIGHT_BLUE
	{ COLOUR_PALE_GREEN, COLOUR_DARK_GREEN }, // COLOUR_GREEN
	{ COLOUR_PALE_GREEN, COLOUR_GREEN      }, // COLOUR_DARK_GREEN
	{ COLOUR_DARK_BLUE,  COLOUR_LIGHT_BLUE }, // COLOUR_BLUE
	{ COLOUR_BROWN,      COLOUR_ORANGE     }, // COLOUR_CREAM
	{ COLOUR_PURPLE,     INVALID_COLOUR    }, // COLOUR_MAUVE
	{ COLOUR_MAUVE,      INVALID_COLOUR    }, // COLOUR_PURPLE
	{ COLOUR_YELLOW,     COLOUR_CREAM      }, // COLOUR_ORANGE
	{ COLOUR_CREAM,      INVALID_COLOUR    }, // COLOUR_BROWN
	{ COLOUR_WHITE,      INVALID_COLOUR    }, // COLOUR_GREY
	{ COLOUR_GREY,       INVALID_COLOUR    }, // COLOUR_WHITE
};

/**
 * Generate a company colour.
 * @return Generated company colour.
 */
static Colours GenerateCompanyColour()
{
	Colours colours[COLOUR_END];

	/* Initialize array */
	for (uint i = 0; i < COLOUR_END; i++) colours[i] = static_cast<Colours>(i);

	/* And randomize it */
	for (uint i = 0; i < 100; i++) {
		uint r = Random();
		std::swap(colours[GB(r, 0, 4)], colours[GB(r, 4, 4)]);
	}

	/* Bubble sort it according to the values in table 1 */
	for (uint i = 0; i < COLOUR_END; i++) {
		for (uint j = 1; j < COLOUR_END; j++) {
			if (_colour_sort[colours[j - 1]] < _colour_sort[colours[j]]) {
				std::swap(colours[j - 1], colours[j]);
			}
		}
	}

	/* Move the colours that look similar to each company's colour to the side */
	for (const Company *c : Company::Iterate()) {
		Colours pcolour = c->colour;

		for (uint i = 0; i < COLOUR_END; i++) {
			if (colours[i] == pcolour) {
				colours[i] = INVALID_COLOUR;
				break;
			}
		}

		for (uint j = 0; j < 2; j++) {
			Colours similar = _similar_colour[pcolour][j];
			if (similar == INVALID_COLOUR) break;

			for (uint i = 1; i < COLOUR_END; i++) {
				if (colours[i - 1] == similar) std::swap(colours[i - 1], colours[i]);
			}
		}
	}

	/* Return the first available colour */
	for (uint i = 0; i < COLOUR_END; i++) {
		if (colours[i] != INVALID_COLOUR) return colours[i];
	}

	NOT_REACHED();
}

/**
 * Generate a random president name of a company.
 * @param c Company that needs a new president name.
 */
static void GeneratePresidentName(Company *c)
{
	for (;;) {
restart:;
		c->president_name_2 = Random();
		c->president_name_1 = SPECSTR_PRESIDENT_NAME;

		/* Reserve space for extra unicode character. We need to do this to be able
		 * to detect too long president name. */
		std::string name = GetString(STR_PRESIDENT_NAME, c->index);
		if (Utf8StringLength(name) >= MAX_LENGTH_PRESIDENT_NAME_CHARS) continue;

		for (const Company *cc : Company::Iterate()) {
			if (c != cc) {
				std::string other_name = GetString(STR_PRESIDENT_NAME, cc->index);
				if (name == other_name) goto restart;
			}
		}
		return;
	}
}

/**
 * Reset the livery schemes to the company's primary colour.
 * This is used on loading games without livery information and on new company start up.
 * @param c Company to reset.
 */
void ResetCompanyLivery(Company *c)
{
	for (LiveryScheme scheme = LS_BEGIN; scheme < LS_END; scheme++) {
		c->livery[scheme].in_use.Reset();
		c->livery[scheme].colour1 = c->colour;
		c->livery[scheme].colour2 = c->colour;
	}

	for (Group *g : Group::Iterate()) {
		if (g->owner == c->index) {
			g->livery.in_use.Reset();
			g->livery.colour1 = c->colour;
			g->livery.colour2 = c->colour;
		}
	}
}

/**
 * Create a new company and sets all company variables default values
 *
 * @param is_ai is an AI company?
 * @param company CompanyID to use for the new company
 * @return the company struct
 */
Company *DoStartupNewCompany(bool is_ai, CompanyID company = CompanyID::Invalid())
{
	if (!Company::CanAllocateItem()) return nullptr;

	/* we have to generate colour before this company is valid */
	Colours colour = GenerateCompanyColour();

	Company *c;
	if (company == CompanyID::Invalid()) {
		c = new Company(STR_SV_UNNAMED, is_ai);
	} else {
		if (Company::IsValidID(company)) return nullptr;
		c = new (company) Company(STR_SV_UNNAMED, is_ai);
	}

	c->colour = colour;

	ResetCompanyLivery(c);
	_company_colours[c->index] = c->colour;

	/* Scale the initial loan based on the inflation rounded down to the loan interval. The maximum loan has already been inflation adjusted. */
	c->money = c->current_loan = std::min<int64_t>((INITIAL_LOAN * _economy.inflation_prices >> 16) / LOAN_INTERVAL * LOAN_INTERVAL, _economy.max_loan);

	c->avail_railtypes = GetCompanyRailTypes(c->index);
	c->avail_roadtypes = GetCompanyRoadTypes(c->index);
	c->inaugurated_year = TimerGameEconomy::year;
	c->inaugurated_year_calendar = TimerGameCalendar::year;

	/* If starting a player company in singleplayer and a favourite company manager face is selected, choose it. Otherwise, use a random face.
	 * In a network game, we'll choose the favourite face later in CmdCompanyCtrl to sync it to all clients. */
	bool randomise_face = true;
	if (!_company_manager_face.empty() && !is_ai && !_networking) {
		auto cmf = ParseCompanyManagerFaceCode(_company_manager_face);
		if (cmf.has_value()) {
			randomise_face = false;
			c->face = std::move(*cmf);
		}
	}
	if (randomise_face) RandomiseCompanyManagerFace(c->face, _random);

	SetDefaultCompanySettings(c->index);
	ClearEnginesHiddenFlagOfCompany(c->index);

	GeneratePresidentName(c);

	SetWindowDirty(WC_GRAPH_LEGEND, 0);
	InvalidateWindowData(WC_CLIENT_LIST, 0);
	InvalidateWindowData(WC_LINKGRAPH_LEGEND, 0);
	BuildOwnerLegend();
	InvalidateWindowData(WC_SMALLMAP, 0, 1);

	if (is_ai && (!_networking || _network_server)) AI::StartNew(c->index);

	AI::BroadcastNewEvent(new ScriptEventCompanyNew(c->index), c->index);
	Game::NewEvent(new ScriptEventCompanyNew(c->index));

	return c;
}

/** Start a new competitor company if possible. */
TimeoutTimer<TimerGameTick> _new_competitor_timeout({ TimerGameTick::Priority::COMPETITOR_TIMEOUT, 0 }, []() {
	if (_game_mode == GM_MENU || !AI::CanStartNew()) return;
	if (_networking && Company::GetNumItems() >= _settings_client.network.max_companies) return;
	if (_settings_game.difficulty.competitors_interval == 0) return;

	/* count number of competitors */
	uint8_t n = 0;
	for (const Company *c : Company::Iterate()) {
		if (c->is_ai) n++;
	}

	if (n >= _settings_game.difficulty.max_no_competitors) return;

	/* Send a command to all clients to start up a new AI.
	 * Works fine for Multiplayer and Singleplayer */
	Command<CMD_COMPANY_CTRL>::Post(CCA_NEW_AI, CompanyID::Invalid(), CRR_NONE, INVALID_CLIENT_ID);
});

/** Start of a new game. */
void StartupCompanies()
{
	/* Ensure the timeout is aborted, so it doesn't fire based on information of the last game. */
	_new_competitor_timeout.Abort();
}

/** Initialize the pool of companies. */
void InitializeCompanies()
{
	_cur_company_tick_index = 0;
}

/**
 * Can company \a cbig buy company \a csmall without exceeding vehicle limits?
 * @param cbig   Company buying \a csmall.
 * @param csmall Company getting bought.
 * @return Return \c true if it is allowed.
 */
bool CheckTakeoverVehicleLimit(CompanyID cbig, CompanyID csmall)
{
	const Company *c1 = Company::Get(cbig);
	const Company *c2 = Company::Get(csmall);

	/* Do the combined vehicle counts stay within the limits? */
	return c1->group_all[VEH_TRAIN].num_vehicle + c2->group_all[VEH_TRAIN].num_vehicle <= _settings_game.vehicle.max_trains &&
		c1->group_all[VEH_ROAD].num_vehicle     + c2->group_all[VEH_ROAD].num_vehicle     <= _settings_game.vehicle.max_roadveh &&
		c1->group_all[VEH_SHIP].num_vehicle     + c2->group_all[VEH_SHIP].num_vehicle     <= _settings_game.vehicle.max_ships &&
		c1->group_all[VEH_AIRCRAFT].num_vehicle + c2->group_all[VEH_AIRCRAFT].num_vehicle <= _settings_game.vehicle.max_aircraft;
}

/**
 * Handle the bankruptcy take over of a company.
 * Companies going bankrupt will ask the other companies in order of their
 * performance rating, so better performing companies get the 'do you want to
 * merge with Y' question earlier. The question will then stay till either the
 * company has gone bankrupt or got merged with a company.
 *
 * @param c the company that is going bankrupt.
 */
static void HandleBankruptcyTakeover(Company *c)
{
	/* Amount of time out for each company to take over a company;
	 * Timeout is a quarter (3 months of 30 days) divided over the
	 * number of companies. The minimum number of days in a quarter
	 * is 90: 31 in January, 28 in February and 31 in March.
	 * Note that the company going bankrupt can't buy itself. */
	static const int TAKE_OVER_TIMEOUT = 3 * 30 * Ticks::DAY_TICKS / (MAX_COMPANIES - 1);

	assert(c->bankrupt_asked.Any());

	/* We're currently asking some company to buy 'us' */
	if (c->bankrupt_timeout != 0) {
		c->bankrupt_timeout -= MAX_COMPANIES;
		if (c->bankrupt_timeout > 0) return;
		c->bankrupt_timeout = 0;

		return;
	}

	/* Did we ask everyone for bankruptcy? If so, bail out. */
	if (c->bankrupt_asked.All()) return;

	Company *best = nullptr;
	int32_t best_performance = -1;

	/* Ask the company with the highest performance history first */
	for (Company *c2 : Company::Iterate()) {
		if (c2->bankrupt_asked.None() && // Don't ask companies going bankrupt themselves
				!c->bankrupt_asked.Test(c2->index) &&
				best_performance < c2->old_economy[1].performance_history &&
				CheckTakeoverVehicleLimit(c2->index, c->index)) {
			best_performance = c2->old_economy[1].performance_history;
			best = c2;
		}
	}

	/* Asked all companies? */
	if (best_performance == -1) {
		c->bankrupt_asked.Set();
		return;
	}

	c->bankrupt_asked.Set(best->index);

	c->bankrupt_timeout = TAKE_OVER_TIMEOUT;

	AI::NewEvent(best->index, new ScriptEventCompanyAskMerger(c->index, c->bankrupt_value));
	if (IsInteractiveCompany(best->index)) {
		ShowBuyCompanyDialog(c->index, false);
	}
}

/** Called every tick for updating some company info. */
void OnTick_Companies()
{
	if (_game_mode == GM_EDITOR) return;

	Company *c = Company::GetIfValid(_cur_company_tick_index);
	if (c != nullptr) {
		if (c->name_1 != 0) GenerateCompanyName(c);
		if (c->bankrupt_asked.Any()) HandleBankruptcyTakeover(c);
	}

	if (_new_competitor_timeout.HasFired() && _game_mode != GM_MENU && AI::CanStartNew()) {
		int32_t timeout = _settings_game.difficulty.competitors_interval * 60 * Ticks::TICKS_PER_SECOND;
		/* If the interval is zero, start as many competitors as needed then check every ~10 minutes if a company went bankrupt and needs replacing. */
		if (timeout == 0) {
			/* count number of competitors */
			uint8_t num_ais = 0;
			for (const Company *cc : Company::Iterate()) {
				if (cc->is_ai) num_ais++;
			}

			size_t num_companies = Company::GetNumItems();
			for (auto i = 0; i < _settings_game.difficulty.max_no_competitors; i++) {
				if (_networking && num_companies++ >= _settings_client.network.max_companies) break;
				if (num_ais++ >= _settings_game.difficulty.max_no_competitors) break;
				Command<CMD_COMPANY_CTRL>::Post(CCA_NEW_AI, CompanyID::Invalid(), CRR_NONE, INVALID_CLIENT_ID);
			}
			timeout = 10 * 60 * Ticks::TICKS_PER_SECOND;
		}
		/* Randomize a bit when the AI is actually going to start; ranges from 87.5% .. 112.5% of indicated value. */
		timeout += ScriptObject::GetRandomizer(OWNER_NONE).Next(timeout / 4) - timeout / 8;

		_new_competitor_timeout.Reset({ TimerGameTick::Priority::COMPETITOR_TIMEOUT, static_cast<uint>(std::max(1, timeout)) });
	}

	_cur_company_tick_index = (_cur_company_tick_index + 1) % MAX_COMPANIES;
}

/**
 * A year has passed, update the economic data of all companies, and perhaps show the
 * financial overview window of the local company.
 */
static const IntervalTimer<TimerGameEconomy> _economy_companies_yearly({TimerGameEconomy::YEAR, TimerGameEconomy::Priority::COMPANY}, [](auto)
{
	/* Copy statistics */
	for (Company *c : Company::Iterate()) {
		/* Move expenses to previous years. */
		std::rotate(std::rbegin(c->yearly_expenses), std::rbegin(c->yearly_expenses) + 1, std::rend(c->yearly_expenses));
		c->yearly_expenses[0].fill(0);
		InvalidateWindowData(WC_FINANCES, c->index);
	}

	if (_settings_client.gui.show_finances && _local_company != COMPANY_SPECTATOR) {
		ShowCompanyFinances(_local_company);
		Company *c = Company::Get(_local_company);
		if (c->num_valid_stat_ent > 5 && c->old_economy[0].performance_history < c->old_economy[4].performance_history) {
			if (_settings_client.sound.new_year) SndPlayFx(SND_01_BAD_YEAR);
		} else {
			if (_settings_client.sound.new_year) SndPlayFx(SND_00_GOOD_YEAR);
		}
	}
});

/**
 * Fill the CompanyNewsInformation struct with the required data.
 * @param c the current company.
 * @param other the other company (use \c nullptr if not relevant).
 */
CompanyNewsInformation::CompanyNewsInformation(StringID title, const Company *c, const Company *other)
{
	this->company_name = GetString(STR_COMPANY_NAME, c->index);

	if (other != nullptr) {
		this->other_company_name = GetString(STR_COMPANY_NAME, other->index);
		c = other;
	}

	this->president_name = GetString(STR_PRESIDENT_NAME_MANAGER, c->index);

	this->title = title;
	this->colour = c->colour;
	this->face = c->face;

}

/**
 * Called whenever company related information changes in order to notify admins.
 * @param company The company data changed of.
 */
void CompanyAdminUpdate(const Company *company)
{
	if (_network_server) NetworkAdminCompanyUpdate(company);
}

/**
 * Called whenever a company is removed in order to notify admins.
 * @param company_id The company that was removed.
 * @param reason     The reason the company was removed.
 */
void CompanyAdminRemove(CompanyID company_id, CompanyRemoveReason reason)
{
	if (_network_server) NetworkAdminCompanyRemove(company_id, (AdminCompanyRemoveReason)reason);
}

/**
 * Control the companies: add, delete, etc.
 * @param flags operation to perform
 * @param cca action to perform
 * @param company_id company to perform the action on
 * @param client_id ClientID
 * @return the cost of this operation or an error
 */
CommandCost CmdCompanyCtrl(DoCommandFlags flags, CompanyCtrlAction cca, CompanyID company_id, CompanyRemoveReason reason, ClientID client_id)
{
	InvalidateWindowData(WC_COMPANY_LEAGUE, 0, 0);

	switch (cca) {
		case CCA_NEW: { // Create a new company
			/* This command is only executed in a multiplayer game */
			if (!_networking) return CMD_ERROR;

			/* Has the network client a correct ClientID? */
			if (!flags.Test(DoCommandFlag::Execute)) return CommandCost();

			NetworkClientInfo *ci = NetworkClientInfo::GetByClientID(client_id);

			/* Delete multiplayer progress bar */
			CloseWindowById(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_JOIN);

			Company *c = DoStartupNewCompany(false);

			/* A new company could not be created, revert to being a spectator */
			if (c == nullptr) {
				/* We check for "ci != nullptr" as a client could have left by
				 * the time we execute this command. */
				if (_network_server && ci != nullptr) {
					ci->client_playas = COMPANY_SPECTATOR;
					NetworkUpdateClientInfo(ci->client_id);
				}
				break;
			}

			NetworkAdminCompanyNew(c);
			NetworkServerNewCompany(c, ci);

			/* This is the client (or non-dedicated server) who wants a new company */
			if (client_id == _network_own_client_id) {
				assert(_local_company == COMPANY_SPECTATOR);
				SetLocalCompany(c->index);

				/*
				 * If a favourite company manager face is selected, choose it. Otherwise, use a random face.
				 * Because this needs to be synchronised over the network, only the client knows
				 * its configuration and we are currently in the execution of a command, we have
				 * to circumvent the normal ::Post logic for commands and just send the command.
				 */
				if (!_company_manager_face.empty()) {
					auto cmf = ParseCompanyManagerFaceCode(_company_manager_face);
					if (cmf.has_value()) {
						Command<CMD_SET_COMPANY_MANAGER_FACE>::SendNet(STR_NULL, c->index, cmf->style, cmf->bits);
					}
				}

				/* Now that we have a new company, broadcast our company settings to
				 * all clients so everything is in sync */
				SyncCompanySettings();

				MarkWholeScreenDirty();
			}
			break;
		}

		case CCA_NEW_AI: { // Make a new AI company
			if (company_id != CompanyID::Invalid() && company_id >= MAX_COMPANIES) return CMD_ERROR;

			/* For network games, company deletion is delayed. */
			if (!_networking && company_id != CompanyID::Invalid() && Company::IsValidID(company_id)) return CMD_ERROR;

			if (!flags.Test(DoCommandFlag::Execute)) return CommandCost();

			/* For network game, just assume deletion happened. */
			assert(company_id == CompanyID::Invalid() || !Company::IsValidID(company_id));

			Company *c = DoStartupNewCompany(true, company_id);
			if (c != nullptr) {
				NetworkAdminCompanyNew(c);
				NetworkServerNewCompany(c, nullptr);
			}
			break;
		}

		case CCA_DELETE: { // Delete a company
			if (reason >= CRR_END) return CMD_ERROR;

			/* We can't delete the last existing company in singleplayer mode. */
			if (!_networking && Company::GetNumItems() == 1) return CMD_ERROR;

			Company *c = Company::GetIfValid(company_id);
			if (c == nullptr) return CMD_ERROR;

			if (!flags.Test(DoCommandFlag::Execute)) return CommandCost();

			/* Show the bankrupt news */
			auto cni = std::make_unique<CompanyNewsInformation>(STR_NEWS_COMPANY_BANKRUPT_TITLE, c);
			EncodedString headline = GetEncodedString(STR_NEWS_COMPANY_BANKRUPT_DESCRIPTION, cni->company_name);
			AddCompanyNewsItem(std::move(headline), std::move(cni));

			/* Remove the company */
			ChangeOwnershipOfCompanyItems(c->index, INVALID_OWNER);
			if (c->is_ai) AI::Stop(c->index);

			CompanyID c_index = c->index;
			delete c;
			AI::BroadcastNewEvent(new ScriptEventCompanyBankrupt(c_index));
			Game::NewEvent(new ScriptEventCompanyBankrupt(c_index));
			CompanyAdminRemove(c_index, (CompanyRemoveReason)reason);

			if (StoryPage::GetNumItems() == 0 || Goal::GetNumItems() == 0) InvalidateWindowData(WC_MAIN_TOOLBAR, 0);
			InvalidateWindowData(WC_CLIENT_LIST, 0);

			break;
		}

		default: return CMD_ERROR;
	}

	InvalidateWindowClassesData(WC_GAME_OPTIONS);
	InvalidateWindowClassesData(WC_SCRIPT_SETTINGS);
	InvalidateWindowClassesData(WC_SCRIPT_LIST);

	return CommandCost();
}

static bool ExecuteAllowListCtrlAction(CompanyAllowListCtrlAction action, Company *c, const std::string &public_key)
{
	switch (action) {
		case CALCA_ADD:
			return c->allow_list.Add(public_key);

		case CALCA_REMOVE:
			return c->allow_list.Remove(public_key);

		default:
			NOT_REACHED();
	}
}

/**
 * Add or remove the given public key to the allow list of this company.
 * @param flags Operation to perform.
 * @param action The action to perform.
 * @param public_key The public key of the client to add or remove.
 * @return The cost of this operation or an error.
 */
CommandCost CmdCompanyAllowListCtrl(DoCommandFlags flags, CompanyAllowListCtrlAction action, const std::string &public_key)
{
	Company *c = Company::GetIfValid(_current_company);
	if (c == nullptr) return CMD_ERROR;

	/* The public key length includes the '\0'. */
	if (public_key.size() != NETWORK_PUBLIC_KEY_LENGTH - 1) return CMD_ERROR;

	switch (action) {
		case CALCA_ADD:
		case CALCA_REMOVE:
			break;

		default:
			return CMD_ERROR;
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		if (ExecuteAllowListCtrlAction(action, c, public_key)) {
			InvalidateWindowData(WC_CLIENT_LIST, 0);
			SetWindowDirty(WC_COMPANY, _current_company);
		}
	}

	return CommandCost();
}

/**
 * Change the company manager's face.
 * @param flags operation to perform
 * @param style The style of the company manager face.
 * @param bits The bits of company manager face.
 * @return the cost of this operation or an error
 */
CommandCost CmdSetCompanyManagerFace(DoCommandFlags flags, uint style, uint32_t bits)
{
	CompanyManagerFace tmp_face{style, bits, {}};
	if (!IsValidCompanyManagerFace(tmp_face)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		CompanyManagerFace &cmf = Company::Get(_current_company)->face;
		SetCompanyManagerFaceStyle(cmf, style);
		cmf.bits = tmp_face.bits;

		MarkWholeScreenDirty();
	}
	return CommandCost();
}

/**
 * Update liveries for a company. This is called when the LS_DEFAULT scheme is changed, to update schemes with colours
 * set to default.
 * @param c Company to update.
 */
void UpdateCompanyLiveries(Company *c)
{
	for (int i = 1; i < LS_END; i++) {
		if (!c->livery[i].in_use.Test(Livery::Flag::Primary)) c->livery[i].colour1 = c->livery[LS_DEFAULT].colour1;
		if (!c->livery[i].in_use.Test(Livery::Flag::Secondary)) c->livery[i].colour2 = c->livery[LS_DEFAULT].colour2;
	}
	UpdateCompanyGroupLiveries(c);
}

/**
 * Change the company's company-colour
 * @param flags operation to perform
 * @param scheme scheme to set
 * @param primary set first/second colour
 * @param colour new colour for vehicles, property, etc.
 * @return the cost of this operation or an error
 */
CommandCost CmdSetCompanyColour(DoCommandFlags flags, LiveryScheme scheme, bool primary, Colours colour)
{
	if (scheme >= LS_END || (colour >= COLOUR_END && colour != INVALID_COLOUR)) return CMD_ERROR;

	/* Default scheme can't be reset to invalid. */
	if (scheme == LS_DEFAULT && colour == INVALID_COLOUR) return CMD_ERROR;

	Company *c = Company::Get(_current_company);

	/* Ensure no two companies have the same primary colour */
	if (scheme == LS_DEFAULT && primary) {
		for (const Company *cc : Company::Iterate()) {
			if (cc != c && cc->colour == colour) return CMD_ERROR;
		}
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		if (primary) {
			if (scheme != LS_DEFAULT) c->livery[scheme].in_use.Set(Livery::Flag::Primary, colour != INVALID_COLOUR);
			if (colour == INVALID_COLOUR) colour = c->livery[LS_DEFAULT].colour1;
			c->livery[scheme].colour1 = colour;

			/* If setting the first colour of the default scheme, adjust the
			 * original and cached company colours too. */
			if (scheme == LS_DEFAULT) {
				UpdateCompanyLiveries(c);
				_company_colours[_current_company] = colour;
				c->colour = colour;
				CompanyAdminUpdate(c);
			}
		} else {
			if (scheme != LS_DEFAULT) c->livery[scheme].in_use.Set(Livery::Flag::Secondary, colour != INVALID_COLOUR);
			if (colour == INVALID_COLOUR) colour = c->livery[LS_DEFAULT].colour2;
			c->livery[scheme].colour2 = colour;

			if (scheme == LS_DEFAULT) {
				UpdateCompanyLiveries(c);
			}
		}

		if (c->livery[scheme].in_use.Any({Livery::Flag::Primary, Livery::Flag::Secondary})) {
			/* If enabling a scheme, set the default scheme to be in use too */
			c->livery[LS_DEFAULT].in_use.Set(Livery::Flag::Primary);
		} else {
			/* Else loop through all schemes to see if any are left enabled.
			 * If not, disable the default scheme too. */
			c->livery[LS_DEFAULT].in_use.Reset({Livery::Flag::Primary, Livery::Flag::Secondary});
			for (scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
				if (c->livery[scheme].in_use.Any({Livery::Flag::Primary, Livery::Flag::Secondary})) {
					c->livery[LS_DEFAULT].in_use.Set(Livery::Flag::Primary);
					break;
				}
			}
		}

		ResetVehicleColourMap();
		MarkWholeScreenDirty();

		/* All graph related to companies use the company colour. */
		InvalidateWindowData(WC_INCOME_GRAPH, 0);
		InvalidateWindowData(WC_OPERATING_PROFIT, 0);
		InvalidateWindowData(WC_DELIVERED_CARGO, 0);
		InvalidateWindowData(WC_PERFORMANCE_HISTORY, 0);
		InvalidateWindowData(WC_COMPANY_VALUE, 0);
		InvalidateWindowData(WC_LINKGRAPH_LEGEND, 0);
		/* The smallmap owner view also stores the company colours. */
		BuildOwnerLegend();
		InvalidateWindowData(WC_SMALLMAP, 0, 1);

		/* Company colour data is indirectly cached. */
		for (Vehicle *v : Vehicle::Iterate()) {
			if (v->owner == _current_company) v->InvalidateNewGRFCache();
		}

		UpdateObjectColours(c);
	}
	return CommandCost();
}

/**
 * Is the given name in use as name of a company?
 * @param name Name to search.
 * @return \c true if the name us unique (that is, not in use), else \c false.
 */
static bool IsUniqueCompanyName(const std::string &name)
{
	for (const Company *c : Company::Iterate()) {
		if (!c->name.empty() && c->name == name) return false;
	}

	return true;
}

/**
 * Change the name of the company.
 * @param flags operation to perform
 * @param text the new name or an empty string when resetting to the default
 * @return the cost of this operation or an error
 */
CommandCost CmdRenameCompany(DoCommandFlags flags, const std::string &text)
{
	bool reset = text.empty();

	if (!reset) {
		if (Utf8StringLength(text) >= MAX_LENGTH_COMPANY_NAME_CHARS) return CMD_ERROR;
		if (!IsUniqueCompanyName(text)) return CommandCost(STR_ERROR_NAME_MUST_BE_UNIQUE);
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		Company *c = Company::Get(_current_company);
		if (reset) {
			c->name.clear();
		} else {
			c->name = text;
		}
		MarkWholeScreenDirty();
		CompanyAdminUpdate(c);

		std::string new_name = GetString(STR_COMPANY_NAME, c->index);
		AI::BroadcastNewEvent(new ScriptEventCompanyRenamed(c->index, new_name));
		Game::NewEvent(new ScriptEventCompanyRenamed(c->index, new_name));
	}

	return CommandCost();
}

/**
 * Is the given name in use as president name of a company?
 * @param name Name to search.
 * @return \c true if the name us unique (that is, not in use), else \c false.
 */
static bool IsUniquePresidentName(const std::string &name)
{
	for (const Company *c : Company::Iterate()) {
		if (!c->president_name.empty() && c->president_name == name) return false;
	}

	return true;
}

/**
 * Change the name of the president.
 * @param flags operation to perform
 * @param text the new name or an empty string when resetting to the default
 * @return the cost of this operation or an error
 */
CommandCost CmdRenamePresident(DoCommandFlags flags, const std::string &text)
{
	bool reset = text.empty();

	if (!reset) {
		if (Utf8StringLength(text) >= MAX_LENGTH_PRESIDENT_NAME_CHARS) return CMD_ERROR;
		if (!IsUniquePresidentName(text)) return CommandCost(STR_ERROR_NAME_MUST_BE_UNIQUE);
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		Company *c = Company::Get(_current_company);

		if (reset) {
			c->president_name.clear();
		} else {
			c->president_name = text;

			if (c->name_1 == STR_SV_UNNAMED && c->name.empty()) {
				Command<CMD_RENAME_COMPANY>::Do(DoCommandFlag::Execute, text + " Transport");
			}
		}

		InvalidateWindowClassesData(WC_COMPANY, 1);
		MarkWholeScreenDirty();
		CompanyAdminUpdate(c);

		std::string new_name = GetString(STR_PRESIDENT_NAME, c->index);
		AI::BroadcastNewEvent(new ScriptEventPresidentRenamed(c->index, new_name));
		Game::NewEvent(new ScriptEventPresidentRenamed(c->index, new_name));
	}

	return CommandCost();
}

/**
 * Get the service interval for the given company and vehicle type.
 * @param c The company, or nullptr for client-default settings.
 * @param type The vehicle type to get the interval for.
 * @return The service interval.
 */
int CompanyServiceInterval(const Company *c, VehicleType type)
{
	const VehicleDefaultSettings *vds = (c == nullptr) ? &_settings_client.company.vehicle : &c->settings.vehicle;
	switch (type) {
		default: NOT_REACHED();
		case VEH_TRAIN:    return vds->servint_trains;
		case VEH_ROAD:     return vds->servint_roadveh;
		case VEH_AIRCRAFT: return vds->servint_aircraft;
		case VEH_SHIP:     return vds->servint_ships;
	}
}

/**
 * Get total sum of all owned road bits.
 * @param rtt RoadTramType to get total for.
 * @return Combined total road road bits.
 */
uint32_t CompanyInfrastructure::GetRoadTramTotal(RoadTramType rtt) const
{
	uint32_t total = 0;
	for (RoadType rt : GetMaskForRoadTramType(rtt)) {
		total += this->road[rt];
	}
	return total;
}

/**
 * Transfer funds (money) from one company to another.
 * To prevent abuse in multiplayer games you can only send money to other
 * companies if you have paid off your loan (either explicitly, or implicitly
 * given the fact that you have more money than loan).
 * @param flags operation to perform
 * @param money the amount of money to transfer; max 20.000.000
 * @param dest_company the company to transfer the money to
 * @return the cost of this operation or an error
 */
CommandCost CmdGiveMoney(DoCommandFlags flags, Money money, CompanyID dest_company)
{
	if (!_settings_game.economy.give_money) return CMD_ERROR;

	const Company *c = Company::Get(_current_company);
	CommandCost amount(EXPENSES_OTHER, std::min<Money>(money, 20000000LL));

	/* You can only transfer funds that is in excess of your loan */
	if (c->money - c->current_loan < amount.GetCost() || amount.GetCost() < 0) return CommandCost(STR_ERROR_INSUFFICIENT_FUNDS);
	if (!Company::IsValidID(dest_company)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		/* Add money to company */
		Backup<CompanyID> cur_company(_current_company, dest_company);
		SubtractMoneyFromCompany(CommandCost(EXPENSES_OTHER, -amount.GetCost()));
		cur_company.Restore();

		if (_networking) {
			std::string dest_company_name = GetString(STR_COMPANY_NAME, dest_company);
			std::string from_company_name = GetString(STR_COMPANY_NAME, _current_company);

			NetworkTextMessage(NETWORK_ACTION_GIVE_MONEY, GetDrawStringCompanyColour(_current_company), false, from_company_name, dest_company_name, amount.GetCost());
		}
	}

	/* Subtract money from local-company */
	return amount;
}

/**
 * Get the index of the first available company. It attempts,
 *  from first to last, and as soon as the attempt succeeds,
 *  to get the index of the company:
 *  1st - get the first existing human company.
 *  2nd - get the first non-existing company.
 *  3rd - get CompanyID::Begin().
 * @return the index of the first available company.
 */
CompanyID GetFirstPlayableCompanyID()
{
	for (Company *c : Company::Iterate()) {
		if (Company::IsHumanID(c->index)) {
			return c->index;
		}
	}

	if (Company::CanAllocateItem()) {
		for (CompanyID c = CompanyID::Begin(); c < MAX_COMPANIES; ++c) {
			if (!Company::IsValidID(c)) {
				return c;
			}
		}
	}

	return CompanyID::Begin();
}

static std::vector<FaceSpec> _faces; ///< All company manager face styles.

/**
 * Reset company manager face styles to default.
 */
void ResetFaces()
{
	_faces.clear();
	_faces.assign(std::begin(_original_faces), std::end(_original_faces));
}

/**
 * Get the number of company manager face styles.
 * @return Number of face styles.
 */
uint GetNumCompanyManagerFaceStyles()
{
	return static_cast<uint>(std::size(_faces));
}

/**
 * Get the definition of a company manager face style.
 * @param style_index Face style to get.
 * @return Definition of face style.
 */
const FaceSpec *GetCompanyManagerFaceSpec(uint style_index)
{
	if (style_index < GetNumCompanyManagerFaceStyles()) return &_faces[style_index];
	return nullptr;
}

/**
 * Find a company manager face style by label.
 * @param label Label to find.
 * @return Index of face style if label is found, otherwise std::nullopt.
 */
std::optional<uint> FindCompanyManagerFaceLabel(std::string_view label)
{
	auto it = std::ranges::find(_faces, label, &FaceSpec::label);
	if (it == std::end(_faces)) return std::nullopt;

	return static_cast<uint>(std::distance(std::begin(_faces), it));
}

/**
 * Get the face variables for a face style.
 * @param style_index Face style to get variables for.
 * @return Variables for the face style.
 */
FaceVars GetCompanyManagerFaceVars(uint style)
{
	const FaceSpec *spec = GetCompanyManagerFaceSpec(style);
	if (spec == nullptr) return {};
	return spec->GetFaceVars();
}

/**
 * Set a company face style.
 * Changes both the style index and the label.
 * @param cmf The CompanyManagerFace to change.
 * @param style The style to set.
 */
void SetCompanyManagerFaceStyle(CompanyManagerFace &cmf, uint style)
{
	const FaceSpec *spec = GetCompanyManagerFaceSpec(style);
	assert(spec != nullptr);

	cmf.style = style;
	cmf.style_label = spec->label;
}

/**
 * Completely randomise a company manager face, including style.
 * @note randomizer should passed be appropriate for server-side or client-side usage.
 * @param cmf The CompanyManagerFace to randomise.
 * @param randomizer The randomizer to use.
 */
void RandomiseCompanyManagerFace(CompanyManagerFace &cmf, Randomizer &randomizer)
{
	SetCompanyManagerFaceStyle(cmf, randomizer.Next(GetNumCompanyManagerFaceStyles()));
	RandomiseCompanyManagerFaceBits(cmf, GetCompanyManagerFaceVars(cmf.style), randomizer);
}

/**
 * Mask company manager face bits to ensure they are all within range.
 * @note Does not update the CompanyManagerFace itself. Unused bits are cleared.
 * @param cmf The CompanyManagerFace.
 * @param style The face variables.
 * @return The masked face bits.
 */
uint32_t MaskCompanyManagerFaceBits(const CompanyManagerFace &cmf, FaceVars vars)
{
	CompanyManagerFace face{};

	for (auto var : SetBitIterator(GetActiveFaceVars(cmf, vars))) {
		vars[var].SetBits(face, vars[var].GetBits(cmf));
	}

	return face.bits;
}

/**
 * Get a face code representation of a company manager face.
 * @param cmf The company manager face.
 * @return String containing face code.
 */
std::string FormatCompanyManagerFaceCode(const CompanyManagerFace &cmf)
{
	uint32_t masked_face_bits = MaskCompanyManagerFaceBits(cmf, GetCompanyManagerFaceVars(cmf.style));
	return fmt::format("{}:{}", cmf.style_label, masked_face_bits);
}

/**
 * Parse a face code into a company manager face.
 * @param str Face code to parse.
 * @return Company manager face, or std::nullopt if it could not be parsed.
 */
std::optional<CompanyManagerFace> ParseCompanyManagerFaceCode(std::string_view str)
{
	if (str.empty()) return std::nullopt;

	CompanyManagerFace cmf;
	StringConsumer consumer{str};
	if (consumer.FindChar(':') != StringConsumer::npos) {
		auto label = consumer.ReadUntilChar(':', StringConsumer::SKIP_ONE_SEPARATOR);

		/* Read numeric part and ensure it's valid. */
		auto bits = consumer.TryReadIntegerBase<uint32_t>(10, true);
		if (!bits.has_value() || consumer.AnyBytesLeft()) return std::nullopt;

		/* Ensure style label is valid. */
		auto style = FindCompanyManagerFaceLabel(label);
		if (!style.has_value()) return std::nullopt;

		SetCompanyManagerFaceStyle(cmf, *style);
		cmf.bits = *bits;
	} else {
		/* No ':' included, treat as numeric-only. This allows old-style codes to be entered. */
		auto bits = ParseInteger(str, 10, true);
		if (!bits.has_value()) return std::nullopt;

		/* Old codes use bits 0..1 to represent face style. These map directly to the default face styles. */
		SetCompanyManagerFaceStyle(cmf, GB(*bits, 0, 2));
		cmf.bits = *bits;
	}

	/* Force the face bits to be valid. */
	FaceVars vars = GetCompanyManagerFaceVars(cmf.style);
	ScaleAllCompanyManagerFaceBits(cmf, vars);
	cmf.bits = MaskCompanyManagerFaceBits(cmf, vars);

	return cmf;
}
