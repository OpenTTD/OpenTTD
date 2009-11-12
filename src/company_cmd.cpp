/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file company_cmd.cpp Handling of companies. */

#include "stdafx.h"
#include "openttd.h"
#include "engine_base.h"
#include "company_func.h"
#include "company_gui.h"
#include "town.h"
#include "news_func.h"
#include "command_func.h"
#include "network/network.h"
#include "network/network_func.h"
#include "network/network_base.h"
#include "ai/ai.hpp"
#include "company_manager_face.h"
#include "group.h"
#include "window_func.h"
#include "strings_func.h"
#include "gfx_func.h"
#include "date_func.h"
#include "sound_func.h"
#include "autoreplace_func.h"
#include "autoreplace_gui.h"
#include "rail.h"
#include "sprite.h"
#include "core/pool_func.hpp"
#include "settings_func.h"

#include "table/strings.h"

CompanyByte _local_company;
CompanyByte _current_company;
/* NOSAVE: can be determined from company structs */
Colours _company_colours[MAX_COMPANIES];
CompanyManagerFace _company_manager_face; ///< for company manager face storage in openttd.cfg
uint _next_competitor_start;              ///< the number of ticks before the next AI is started
uint _cur_company_tick_index;             ///< used to generate a name for one company that doesn't have a name yet per tick

CompanyPool _company_pool("Company");
INSTANTIATE_POOL_METHODS(Company)

Company::Company(uint16 name_1, bool is_ai) :
	name_1(name_1),
	location_of_HQ(INVALID_TILE),
	is_ai(is_ai)
{
	for (uint j = 0; j < 4; j++) this->share_owners[j] = COMPANY_SPECTATOR;
	InvalidateWindowData(WC_PERFORMANCE_DETAIL, 0, INVALID_COMPANY);
}

Company::~Company()
{
	free(this->name);
	free(this->president_name);
	free(this->num_engines);

	if (CleaningPool()) return;

	DeleteCompanyWindows(this->index);
}

/**
 * Invalidating some stuff after removing item from the pool.
 * @param index index of deleted item
 */
void Company::PostDestructor(size_t index)
{
	InvalidateWindowData(WC_GRAPH_LEGEND, 0, (int)index);
	InvalidateWindowData(WC_PERFORMANCE_DETAIL, 0, (int)index);
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

#ifdef ENABLE_NETWORK
	/* Delete the chat window, if you were team chatting. */
	InvalidateWindowData(WC_SEND_NETWORK_MSG, DESTTYPE_TEAM, _local_company);
#endif

	_local_company = new_company;

	/* Delete any construction windows... */
	DeleteConstructionWindows();

	/* ... and redraw the whole screen. */
	MarkWholeScreenDirty();
}

uint16 GetDrawStringCompanyColour(CompanyID company)
{
	/* Get the colour for DrawString-subroutines which matches the colour
	 * of the company */
	if (!Company::IsValidID(company)) return _colour_gradient[COLOUR_WHITE][4] | IS_PALETTE_COLOUR;
	return (_colour_gradient[_company_colours[company]][4]) | IS_PALETTE_COLOUR;
}

void DrawCompanyIcon(CompanyID c, int x, int y)
{
	DrawSprite(SPR_COMPANY_ICON, COMPANY_SPRITE_COLOUR(c), x, y);
}

/**
 * Checks whether a company manager's face is a valid encoding.
 * Unused bits are not enforced to be 0.
 * @param cmf the fact to check
 * @return true if and only if the face is valid
 */
bool IsValidCompanyManagerFace(CompanyManagerFace cmf)
{
	if (!AreCompanyManagerFaceBitsValid(cmf, CMFV_GEN_ETHN, GE_WM)) return false;

	GenderEthnicity ge   = (GenderEthnicity)GetCompanyManagerFaceBits(cmf, CMFV_GEN_ETHN, GE_WM);
	bool has_moustache   = !HasBit(ge, GENDER_FEMALE) && GetCompanyManagerFaceBits(cmf, CMFV_HAS_MOUSTACHE,   ge) != 0;
	bool has_tie_earring = !HasBit(ge, GENDER_FEMALE) || GetCompanyManagerFaceBits(cmf, CMFV_HAS_TIE_EARRING, ge) != 0;
	bool has_glasses     = GetCompanyManagerFaceBits(cmf, CMFV_HAS_GLASSES, ge) != 0;

	if (!AreCompanyManagerFaceBitsValid(cmf, CMFV_EYE_COLOUR, ge)) return false;
	for (CompanyManagerFaceVariable cmfv = CMFV_CHEEKS; cmfv < CMFV_END; cmfv++) {
		switch (cmfv) {
			case CMFV_MOUSTACHE:   if (!has_moustache)   continue; break;
			case CMFV_LIPS:        // FALL THROUGH
			case CMFV_NOSE:        if (has_moustache)    continue; break;
			case CMFV_TIE_EARRING: if (!has_tie_earring) continue; break;
			case CMFV_GLASSES:     if (!has_glasses)     continue; break;
			default: break;
		}
		if (!AreCompanyManagerFaceBitsValid(cmf, cmfv, ge)) return false;
	}

	return true;
}

void InvalidateCompanyWindows(const Company *company)
{
	CompanyID cid = company->index;

	if (cid == _local_company) SetWindowDirty(WC_STATUS_BAR, 0);
	SetWindowDirty(WC_FINANCES, cid);
}

bool CheckCompanyHasMoney(CommandCost cost)
{
	if (cost.GetCost() > 0) {
		const Company *c = Company::GetIfValid(_current_company);
		if (c != NULL && cost.GetCost() > c->money) {
			SetDParam(0, cost.GetCost());
			_error_message = STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY;
			return false;
		}
	}
	return true;
}

static void SubtractMoneyFromAnyCompany(Company *c, CommandCost cost)
{
	if (cost.GetCost() == 0) return;
	assert(cost.GetExpensesType() != INVALID_EXPENSES);

	c->money -= cost.GetCost();
	c->yearly_expenses[0][cost.GetExpensesType()] += cost.GetCost();

	if (HasBit(1 << EXPENSES_TRAIN_INC    |
	           1 << EXPENSES_ROADVEH_INC  |
	           1 << EXPENSES_AIRCRAFT_INC |
	           1 << EXPENSES_SHIP_INC, cost.GetExpensesType())) {
		c->cur_economy.income -= cost.GetCost();
	} else if (HasBit(1 << EXPENSES_TRAIN_RUN    |
	                  1 << EXPENSES_ROADVEH_RUN  |
	                  1 << EXPENSES_AIRCRAFT_RUN |
	                  1 << EXPENSES_SHIP_RUN     |
	                  1 << EXPENSES_PROPERTY     |
	                  1 << EXPENSES_LOAN_INT, cost.GetExpensesType())) {
		c->cur_economy.expenses -= cost.GetCost();
	}

	InvalidateCompanyWindows(c);
}

void SubtractMoneyFromCompany(CommandCost cost)
{
	Company *c = Company::GetIfValid(_current_company);
	if (c != NULL) SubtractMoneyFromAnyCompany(c, cost);
}

void SubtractMoneyFromCompanyFract(CompanyID company, CommandCost cst)
{
	Company *c = Company::Get(company);
	byte m = c->money_fraction;
	Money cost = cst.GetCost();

	c->money_fraction = m - (byte)cost;
	cost >>= 8;
	if (c->money_fraction > m) cost++;
	if (cost != 0) SubtractMoneyFromAnyCompany(c, CommandCost(cst.GetExpensesType(), cost));
}

void GetNameOfOwner(Owner owner, TileIndex tile)
{
	SetDParam(2, owner);

	if (owner != OWNER_TOWN) {
		if (!Company::IsValidID(owner)) {
			SetDParam(0, STR_COMPANY_SOMEONE);
		} else {
			SetDParam(0, STR_COMPANY_NAME);
			SetDParam(1, owner);
		}
	} else {
		const Town *t = ClosestTownFromTile(tile, UINT_MAX);

		SetDParam(0, STR_TOWN_NAME);
		SetDParam(1, t->index);
	}
}


bool CheckOwnership(Owner owner)
{
	assert(owner < OWNER_END);

	if (owner == _current_company) return true;
	_error_message = STR_ERROR_OWNED_BY;
	GetNameOfOwner(owner, 0);
	return false;
}

bool CheckTileOwnership(TileIndex tile)
{
	Owner owner = GetTileOwner(tile);

	assert(owner < OWNER_END);

	if (owner == _current_company) return true;
	_error_message = STR_ERROR_OWNED_BY;

	/* no need to get the name of the owner unless we're the local company (saves some time) */
	if (IsLocalCompany()) GetNameOfOwner(owner, tile);
	return false;
}

static void GenerateCompanyName(Company *c)
{
	TileIndex tile;
	Town *t;
	StringID str;
	Company *cc;
	uint32 strp;
	/* Reserve space for extra unicode character. We need to do this to be able
	 * to detect too long company name. */
	char buffer[MAX_LENGTH_COMPANY_NAME_BYTES + MAX_CHAR_LENGTH];

	if (c->name_1 != STR_SV_UNNAMED) return;

	tile = c->last_build_coordinate;
	if (tile == 0) return;

	t = ClosestTownFromTile(tile, UINT_MAX);

	if (t->name == NULL && IsInsideMM(t->townnametype, SPECSTR_TOWNNAME_START, SPECSTR_TOWNNAME_LAST + 1)) {
		str = t->townnametype - SPECSTR_TOWNNAME_START + SPECSTR_PLAYERNAME_START;
		strp = t->townnameparts;

verify_name:;
		/* No companies must have this name already */
		FOR_ALL_COMPANIES(cc) {
			if (cc->name_1 == str && cc->name_2 == strp) goto bad_town_name;
		}

		GetString(buffer, str, lastof(buffer));
		if (strlen(buffer) >= MAX_LENGTH_COMPANY_NAME_BYTES) goto bad_town_name;

set_name:;
		c->name_1 = str;
		c->name_2 = strp;

		MarkWholeScreenDirty();

		if (c->is_ai) {
			CompanyNewsInformation *cni = MallocT<CompanyNewsInformation>(1);
			cni->FillData(c);
			SetDParam(0, STR_NEWS_COMPANY_LAUNCH_TITLE);
			SetDParam(1, STR_NEWS_COMPANY_LAUNCH_DESCRIPTION);
			SetDParamStr(2, cni->company_name);
			SetDParam(3, t->index);
			AddNewsItem(STR_MESSAGE_NEWS_FORMAT, NS_COMPANY_NEW, NR_TILE, c->last_build_coordinate, NR_NONE, UINT32_MAX, cni);
		}
		AI::BroadcastNewEvent(new AIEventCompanyNew(c->index), c->index);
		return;
	}
bad_town_name:;

	if (c->president_name_1 == SPECSTR_PRESIDENT_NAME) {
		str = SPECSTR_ANDCO_NAME;
		strp = c->president_name_2;
		goto set_name;
	} else {
		str = SPECSTR_ANDCO_NAME;
		strp = Random();
		goto verify_name;
	}
}

static const byte _colour_sort[COLOUR_END] = {2, 2, 3, 2, 3, 2, 3, 2, 3, 2, 2, 2, 3, 1, 1, 1};
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

static Colours GenerateCompanyColour()
{
	Colours colours[COLOUR_END];

	/* Initialize array */
	for (uint i = 0; i < COLOUR_END; i++) colours[i] = (Colours)i;

	/* And randomize it */
	for (uint i = 0; i < 100; i++) {
		uint r = Random();
		Swap(colours[GB(r, 0, 4)], colours[GB(r, 4, 4)]);
	}

	/* Bubble sort it according to the values in table 1 */
	for (uint i = 0; i < COLOUR_END; i++) {
		for (uint j = 1; j < COLOUR_END; j++) {
			if (_colour_sort[colours[j - 1]] < _colour_sort[colours[j]]) {
				Swap(colours[j - 1], colours[j]);
			}
		}
	};

	/* Move the colours that look similar to each company's colour to the side */
	Company *c;
	FOR_ALL_COMPANIES(c) {
		Colours pcolour = (Colours)c->colour;

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
				if (colours[i - 1] == similar) Swap(colours[i - 1], colours[i]);
			}
		}
	}

	/* Return the first available colour */
	for (uint i = 0; i < COLOUR_END; i++) {
		if (colours[i] != INVALID_COLOUR) return colours[i];
	}

	NOT_REACHED();
}

static void GeneratePresidentName(Company *c)
{
	for (;;) {
restart:;
		c->president_name_2 = Random();
		c->president_name_1 = SPECSTR_PRESIDENT_NAME;

		/* Reserve space for extra unicode character. We need to do this to be able
		 * to detect too long president name. */
		char buffer[MAX_LENGTH_PRESIDENT_NAME_BYTES + MAX_CHAR_LENGTH];
		SetDParam(0, c->index);
		GetString(buffer, STR_PRESIDENT_NAME, lastof(buffer));
		if (strlen(buffer) >= MAX_LENGTH_PRESIDENT_NAME_BYTES) continue;

		Company *cc;
		FOR_ALL_COMPANIES(cc) {
			if (c != cc) {
				/* Reserve extra space so even overlength president names can be compared. */
				char buffer2[MAX_LENGTH_PRESIDENT_NAME_BYTES + MAX_CHAR_LENGTH];
				SetDParam(0, cc->index);
				GetString(buffer2, STR_PRESIDENT_NAME, lastof(buffer2));
				if (strcmp(buffer2, buffer) == 0) goto restart;
			}
		}
		return;
	}
}

void ResetCompanyLivery(Company *c)
{
	for (LiveryScheme scheme = LS_BEGIN; scheme < LS_END; scheme++) {
		c->livery[scheme].in_use  = false;
		c->livery[scheme].colour1 = c->colour;
		c->livery[scheme].colour2 = c->colour;
	}
}

/**
 * Create a new company and sets all company variables default values
 *
 * @param is_ai is a ai company?
 * @param company CompanyID to use for the new company
 * @return the company struct
 */
Company *DoStartupNewCompany(bool is_ai, CompanyID company = INVALID_COMPANY)
{
	if (!Company::CanAllocateItem()) return NULL;

	/* we have to generate colour before this company is valid */
	Colours colour = GenerateCompanyColour();

	Company *c;
	if (company == INVALID_COMPANY) {
		c = new Company(STR_SV_UNNAMED, is_ai);
	} else {
		if (Company::IsValidID(company)) return NULL;
		c = new (company) Company(STR_SV_UNNAMED, is_ai);
	}

	c->colour = colour;

	ResetCompanyLivery(c);
	_company_colours[c->index] = (Colours)c->colour;

	c->money = c->current_loan = 100000;

	c->share_owners[0] = c->share_owners[1] = c->share_owners[2] = c->share_owners[3] = INVALID_OWNER;

	c->avail_railtypes = GetCompanyRailtypes(c->index);
	c->avail_roadtypes = GetCompanyRoadtypes(c->index);
	c->inaugurated_year = _cur_year;
	RandomCompanyManagerFaceBits(c->face, (GenderEthnicity)Random(), false); // create a random company manager face

	SetDefaultCompanySettings(c->index);

	GeneratePresidentName(c);

	SetWindowDirty(WC_GRAPH_LEGEND, 0);
	SetWindowDirty(WC_TOOLBAR_MENU, 0);
	SetWindowDirty(WC_CLIENT_LIST, 0);

	if (is_ai && (!_networking || _network_server)) AI::StartNew(c->index);

	c->num_engines = CallocT<uint16>(Engine::GetPoolSize());

	return c;
}

void StartupCompanies()
{
	_next_competitor_start = 0;
}

static void MaybeStartNewCompany()
{
#ifdef ENABLE_NETWORK
	if (_networking && Company::GetNumItems() >= _settings_client.network.max_companies) return;
#endif /* ENABLE_NETWORK */

	Company *c;

	/* count number of competitors */
	uint n = 0;
	FOR_ALL_COMPANIES(c) {
		if (c->is_ai) n++;
	}

	if (n < (uint)_settings_game.difficulty.max_no_competitors) {
		/* Send a command to all clients to start up a new AI.
		 * Works fine for Multiplayer and Singleplayer */
		DoCommandP(0, 1, INVALID_COMPANY, CMD_COMPANY_CTRL);
	}
}

void InitializeCompanies()
{
	_company_pool.CleanPool();
	_cur_company_tick_index = 0;
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
	static const int TAKE_OVER_TIMEOUT = 3 * 30 * DAY_TICKS / (MAX_COMPANIES - 1);

	assert(c->bankrupt_asked != 0);

	/* We're currently asking some company to buy 'us' */
	if (c->bankrupt_timeout != 0) {
		c->bankrupt_timeout -= MAX_COMPANIES;
		if (c->bankrupt_timeout > 0) return;
		c->bankrupt_timeout = 0;

		return;
	}

	/* Did we ask everyone for bankruptcy? If so, bail out. */
	if (c->bankrupt_asked == MAX_UVALUE(CompanyMask)) return;

	Company *c2, *best = NULL;
	int32 best_performance = -1;

	/* Ask the company with the highest performance history first */
	FOR_ALL_COMPANIES(c2) {
		if (c2->bankrupt_asked == 0 && // Don't ask companies going bankrupt themselves
				!HasBit(c->bankrupt_asked, c2->index) &&
				best_performance < c2->old_economy[1].performance_history) {
			best_performance = c2->old_economy[1].performance_history;
			best = c2;
		}
	}

	/* Asked all companies? */
	if (best_performance == -1) {
		c->bankrupt_asked = MAX_UVALUE(CompanyMask);
		return;
	}

	SetBit(c->bankrupt_asked, best->index);

	if (IsInteractiveCompany(best->index)) {
		c->bankrupt_timeout = TAKE_OVER_TIMEOUT;
		ShowBuyCompanyDialog(c->index);
		return;
	}

	if (best->is_ai) {
		AI::NewEvent(best->index, new AIEventCompanyAskMerger(c->index, ClampToI32(c->bankrupt_value)));
	}
}

void OnTick_Companies()
{
	if (_game_mode == GM_EDITOR) return;

	Company *c = Company::GetIfValid(_cur_company_tick_index);
	if (c != NULL) {
		if (c->name_1 != 0) GenerateCompanyName(c);
		if (c->bankrupt_asked != 0) HandleBankruptcyTakeover(c);
	}

	if (_next_competitor_start == 0) {
		_next_competitor_start = AI::GetStartNextTime() * DAY_TICKS;
	}

	if (AI::CanStartNew() && _game_mode != GM_MENU && --_next_competitor_start == 0) {
		MaybeStartNewCompany();
	}

	_cur_company_tick_index = (_cur_company_tick_index + 1) % MAX_COMPANIES;
}

void CompaniesYearlyLoop()
{
	Company *c;

	/* Copy statistics */
	FOR_ALL_COMPANIES(c) {
		memmove(&c->yearly_expenses[1], &c->yearly_expenses[0], sizeof(c->yearly_expenses) - sizeof(c->yearly_expenses[0]));
		memset(&c->yearly_expenses[0], 0, sizeof(c->yearly_expenses[0]));
		SetWindowDirty(WC_FINANCES, c->index);
	}

	if (_settings_client.gui.show_finances && _local_company != COMPANY_SPECTATOR) {
		ShowCompanyFinances(_local_company);
		c = Company::Get(_local_company);
		if (c->num_valid_stat_ent > 5 && c->old_economy[0].performance_history < c->old_economy[4].performance_history) {
			SndPlayFx(SND_01_BAD_YEAR);
		} else {
			SndPlayFx(SND_00_GOOD_YEAR);
		}
	}
}

/** Change engine renewal parameters
 * @param tile unused
 * @param flags operation to perform
 * @param p1 packed data
 *   - bits 16-31 = engine group
 * @param p2 packed data
 *   - bits  0-15 = old engine type
 *   - bits 16-31 = new engine type
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdSetAutoReplace(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Company *c = Company::GetIfValid(_current_company);
	if (c == NULL) return CMD_ERROR;

	EngineID old_engine_type = GB(p2, 0, 16);
	EngineID new_engine_type = GB(p2, 16, 16);
	GroupID id_g = GB(p1, 16, 16);
	CommandCost cost;

	if (!Group::IsValidID(id_g) && !IsAllGroupID(id_g) && !IsDefaultGroupID(id_g)) return CMD_ERROR;
	if (new_engine_type != INVALID_ENGINE) {
		if (!CheckAutoreplaceValidity(old_engine_type, new_engine_type, _current_company)) return CMD_ERROR;

		cost = AddEngineReplacementForCompany(c, old_engine_type, new_engine_type, id_g, flags);
	} else {
		cost = RemoveEngineReplacementForCompany(c, old_engine_type, id_g, flags);
	}

	if (IsLocalCompany()) InvalidateAutoreplaceWindow(old_engine_type, id_g);

	return cost;
}

/**
 * Fill the CompanyNewsInformation struct with the required data.
 * @param c the current company.
 * @param other the other company.
 */
void CompanyNewsInformation::FillData(const Company *c, const Company *other)
{
	SetDParam(0, c->index);
	GetString(this->company_name, STR_COMPANY_NAME, lastof(this->company_name));

	if (other == NULL) {
		*this->other_company_name = '\0';
	} else {
		SetDParam(0, other->index);
		GetString(this->other_company_name, STR_COMPANY_NAME, lastof(this->other_company_name));
		c = other;
	}

	SetDParam(0, c->index);
	GetString(this->president_name, STR_PRESIDENT_NAME_MANAGER, lastof(this->president_name));

	this->colour = c->colour;
	this->face = c->face;

}

/** Control the companies: add, delete, etc.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 various functionality
 * - p1 = 0 - create a new company, Which company (network) it will be is in p2
 * - p1 = 1 - create a new AI company
 * - p1 = 2 - delete a company. Company is identified by p2
 * @param p2 various functionality, dictated by p1
 * - p1 = 0 - ClientID of the newly created client
 * - p1 = 1 - CompanyID to start AI (INVALID_COMPANY for first available)
 * - p1 = 2 - CompanyID of the that is getting deleted
 * @param text unused
 * @return the cost of this operation or an error
 *
 * @todo In the case of p1=0, create new company, the clientID of the new client is in parameter
 * p2. This parameter is passed in at function DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_COMMAND)
 * on the server itself. First of all this is unbelievably ugly; second of all, well,
 * it IS ugly! <b>Someone fix this up :)</b> So where to fix?@n
 * @arg - network_server.c:838 DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_COMMAND)@n
 * @arg - network_client.c:536 DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_MAP) from where the map has been received
 */
CommandCost CmdCompanyCtrl(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (flags & DC_EXEC) _current_company = OWNER_NONE;

	InvalidateWindowData(WC_COMPANY_LEAGUE, 0, 0);

	switch (p1) {
		case 0: { // Create a new company
			/* This command is only executed in a multiplayer game */
			if (!_networking) return CMD_ERROR;

#ifdef ENABLE_NETWORK

			/* Joining Client:
			 * _local_company: COMPANY_SPECTATOR
			 * cid = clientid
			 *
			 * Other client(s)/server:
			 * _local_company: what they play as
			 * cid = requested company/company of joining client */
			ClientID cid = (ClientID)p2;

			/* Has the network client a correct ClientIndex? */
			if (!(flags & DC_EXEC)) return CommandCost();
			NetworkClientInfo *ci = NetworkFindClientInfoFromClientID(cid);
			if (ci == NULL) return CommandCost();

			/* Delete multiplayer progress bar */
			DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);

			Company *c = DoStartupNewCompany(false);

			/* A new company could not be created, revert to being a spectator */
			if (c == NULL) {
				if (_network_server) {
					ci->client_playas = COMPANY_SPECTATOR;
					NetworkUpdateClientInfo(ci->client_id);
				}
				break;
			}

			/* This is the client (or non-dedicated server) who wants a new company */
			if (cid == _network_own_client_id) {
				assert(_local_company == COMPANY_SPECTATOR);
				SetLocalCompany(c->index);
				if (!StrEmpty(_settings_client.network.default_company_pass)) {
					char *password = _settings_client.network.default_company_pass;
					NetworkChangeCompanyPassword(1, &password);
				}

				_current_company = _local_company;

				/* Now that we have a new company, broadcast our company settings to
				 * all clients so everything is in sync */
				SyncCompanySettings();

				MarkWholeScreenDirty();
			}

			if (_network_server) {
				/* XXX - UGLY! p2 (pid) is mis-used to fetch the client-id, done at
				 * server side in network_server.c:838, function
				 * DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_COMMAND) */
				CompanyID old_playas = ci->client_playas;
				ci->client_playas = c->index;
				NetworkUpdateClientInfo(ci->client_id);

				if (Company::IsValidID(ci->client_playas)) {
					CompanyID company_backup = _local_company;
					_network_company_states[c->index].months_empty = 0;
					_network_company_states[c->index].password[0] = '\0';
					NetworkServerUpdateCompanyPassworded(ci->client_playas, false);

					/* XXX - When a client joins, we automatically set its name to the
					 * client's name (for some reason). As it stands now only the server
					 * knows the client's name, so it needs to send out a "broadcast" to
					 * do this. To achieve this we send a network command. However, it
					 * uses _local_company to execute the command as.  To prevent abuse
					 * (eg. only yourself can change your name/company), we 'cheat' by
					 * impersonation _local_company as the server. Not the best solution;
					 * but it works.
					 * TODO: Perhaps this could be improved by when the client is ready
					 * with joining to let it send itself the command, and not the server?
					 * For example in network_client.c:534? */
					_local_company = ci->client_playas;
					NetworkSend_Command(0, 0, 0, CMD_RENAME_PRESIDENT, NULL, ci->client_name);
					_local_company = company_backup;
				}

				/* Announce new company on network, if the client was a SPECTATOR before */
				if (old_playas == COMPANY_SPECTATOR) {
					NetworkServerSendChat(NETWORK_ACTION_COMPANY_NEW, DESTTYPE_BROADCAST, 0, "", ci->client_id, ci->client_playas + 1);
				}
			}
#endif /* ENABLE_NETWORK */
		} break;

		case 1: // Make a new AI company
			if (!(flags & DC_EXEC)) return CommandCost();

			if (p2 != INVALID_COMPANY && (p2 >= MAX_COMPANIES || Company::IsValidID(p2))) return CMD_ERROR;
			DoStartupNewCompany(true, (CompanyID)p2);
			break;

		case 2: { // Delete a company
			Company *c = Company::GetIfValid(p2);
			if (c == NULL) return CMD_ERROR;

			if (!(flags & DC_EXEC)) return CommandCost();

			/* Delete any open window of the company */
			DeleteCompanyWindows(c->index);
			CompanyNewsInformation *cni = MallocT<CompanyNewsInformation>(1);
			cni->FillData(c);

			/* Show the bankrupt news */
			SetDParam(0, STR_NEWS_COMPANY_BANKRUPT_TITLE);
			SetDParam(1, STR_NEWS_COMPANY_BANKRUPT_DESCRIPTION);
			SetDParamStr(2, cni->company_name);
			AddCompanyNewsItem(STR_MESSAGE_NEWS_FORMAT, NS_COMPANY_BANKRUPT, cni);

			/* Remove the company */
			ChangeOwnershipOfCompanyItems(c->index, INVALID_OWNER);
			if (c->is_ai) AI::Stop(c->index);

			CompanyID c_index = c->index;
			delete c;
			AI::BroadcastNewEvent(new AIEventCompanyBankrupt(c_index));
		} break;

		default: return CMD_ERROR;
	}

	return CommandCost();
}
