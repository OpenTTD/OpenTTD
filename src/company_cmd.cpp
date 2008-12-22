/* $Id$ */

/** @file company_cmd.cpp Handling of companies. */

#include "stdafx.h"
#include "openttd.h"
#include "engine_func.h"
#include "engine_base.h"
#include "company_func.h"
#include "company_gui.h"
#include "town.h"
#include "news_func.h"
#include "saveload.h"
#include "command_func.h"
#include "network/network.h"
#include "network/network_func.h"
#include "variables.h"
#include "cheat_func.h"
#include "ai/ai.h"
#include "company_manager_face.h"
#include "group.h"
#include "window_func.h"
#include "tile_map.h"
#include "strings_func.h"
#include "gfx_func.h"
#include "functions.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "core/alloc_func.hpp"
#include "core/sort_func.hpp"
#include "autoreplace_func.h"
#include "autoreplace_gui.h"
#include "string_func.h"
#include "ai/default/default.h"
#include "ai/trolly/trolly.h"
#include "road_func.h"
#include "rail.h"
#include "sprite.h"
#include "debug.h"
#include "oldpool_func.h"

#include "table/strings.h"
#include "table/sprites.h"

CompanyByte _local_company;
CompanyByte _current_company;
/* NOSAVE: can be determined from company structs */
byte _company_colours[MAX_COMPANIES];
CompanyManagerFace _company_manager_face; ///< for company manager face storage in openttd.cfg
HighScore _highscore_table[5][5]; // 4 difficulty-settings (+ network); top 5

DEFINE_OLD_POOL_GENERIC(Company, Company)

Company::Company(uint16 name_1, bool is_ai) : name_1(name_1), is_ai(is_ai)
{
	for (uint j = 0; j < 4; j++) this->share_owners[j] = COMPANY_SPECTATOR;
}

Company::~Company()
{
	free(this->name);
	free(this->president_name);
	free(this->num_engines);

	if (CleaningPool()) return;

	DeleteCompanyWindows(this->index);
	this->name_1 = 0;
}

/**
 * Sets the local company and updates the patch settings that are set on a
 * per-company basis to reflect the core's state in the GUI.
 * @param new_company the new company
 * @pre IsValidCompanyID(new_company) || new_company == COMPANY_SPECTATOR || new_company == OWNER_NONE
 */
void SetLocalCompany(CompanyID new_company)
{
	/* company could also be COMPANY_SPECTATOR or OWNER_NONE */
	assert(IsValidCompanyID(new_company) || new_company == COMPANY_SPECTATOR || new_company == OWNER_NONE);

	_local_company = new_company;

	/* Do not update the patches if we are in the intro GUI */
	if (IsValidCompanyID(new_company) && _game_mode != GM_MENU) {
		const Company *c = GetCompany(new_company);
		_settings_client.gui.autorenew        = c->engine_renew;
		_settings_client.gui.autorenew_months = c->engine_renew_months;
		_settings_client.gui.autorenew_money  = c->engine_renew_money;
		InvalidateWindow(WC_GAME_OPTIONS, 0);
	}
}

bool IsHumanCompany(CompanyID company)
{
	return !GetCompany(company)->is_ai;
}


uint16 GetDrawStringCompanyColor(CompanyID company)
{
	/* Get the color for DrawString-subroutines which matches the color
	 * of the company */
	if (!IsValidCompanyID(company)) return _colour_gradient[COLOUR_WHITE][4] | IS_PALETTE_COLOR;
	return (_colour_gradient[_company_colours[company]][4]) | IS_PALETTE_COLOR;
}

void DrawCompanyIcon(CompanyID c, int x, int y)
{
	DrawSprite(SPR_PLAYER_ICON, COMPANY_SPRITE_COLOR(c), x, y);
}

/**
 * Converts an old company manager's face format to the new company manager's face format
 *
 * Meaning of the bits in the old face (some bits are used in several times):
 * - 4 and 5: chin
 * - 6 to 9: eyebrows
 * - 10 to 13: nose
 * - 13 to 15: lips (also moustache for males)
 * - 16 to 19: hair
 * - 20 to 22: eye color
 * - 20 to 27: tie, ear rings etc.
 * - 28 to 30: glasses
 * - 19, 26 and 27: race (bit 27 set and bit 19 equal to bit 26 = black, otherwise white)
 * - 31: gender (0 = male, 1 = female)
 *
 * @param face the face in the old format
 * @return the face in the new format
 */
CompanyManagerFace ConvertFromOldCompanyManagerFace(uint32 face)
{
	CompanyManagerFace cmf = 0;
	GenderEthnicity ge = GE_WM;

	if (HasBit(face, 31)) SetBit(ge, GENDER_FEMALE);
	if (HasBit(face, 27) && (HasBit(face, 26) == HasBit(face, 19))) SetBit(ge, ETHNICITY_BLACK);

	SetCompanyManagerFaceBits(cmf, CMFV_GEN_ETHN,    ge, ge);
	SetCompanyManagerFaceBits(cmf, CMFV_HAS_GLASSES, ge, GB(face, 28, 3) <= 1);
	SetCompanyManagerFaceBits(cmf, CMFV_EYE_COLOUR,  ge, HasBit(ge, ETHNICITY_BLACK) ? 0 : ClampU(GB(face, 20, 3), 5, 7) - 5);
	SetCompanyManagerFaceBits(cmf, CMFV_CHIN,        ge, ScaleCompanyManagerFaceValue(CMFV_CHIN,     ge, GB(face,  4, 2)));
	SetCompanyManagerFaceBits(cmf, CMFV_EYEBROWS,    ge, ScaleCompanyManagerFaceValue(CMFV_EYEBROWS, ge, GB(face,  6, 4)));
	SetCompanyManagerFaceBits(cmf, CMFV_HAIR,        ge, ScaleCompanyManagerFaceValue(CMFV_HAIR,     ge, GB(face, 16, 4)));
	SetCompanyManagerFaceBits(cmf, CMFV_JACKET,      ge, ScaleCompanyManagerFaceValue(CMFV_JACKET,   ge, GB(face, 20, 2)));
	SetCompanyManagerFaceBits(cmf, CMFV_COLLAR,      ge, ScaleCompanyManagerFaceValue(CMFV_COLLAR,   ge, GB(face, 22, 2)));
	SetCompanyManagerFaceBits(cmf, CMFV_GLASSES,     ge, GB(face, 28, 1));

	uint lips = GB(face, 10, 4);
	if (!HasBit(ge, GENDER_FEMALE) && lips < 4) {
		SetCompanyManagerFaceBits(cmf, CMFV_HAS_MOUSTACHE, ge, true);
		SetCompanyManagerFaceBits(cmf, CMFV_MOUSTACHE,     ge, max(lips, 1U) - 1);
	} else {
		if (!HasBit(ge, GENDER_FEMALE)) {
			lips = lips * 15 / 16;
			lips -= 3;
			if (HasBit(ge, ETHNICITY_BLACK) && lips > 8) lips = 0;
		} else {
			lips = ScaleCompanyManagerFaceValue(CMFV_LIPS, ge, lips);
		}
		SetCompanyManagerFaceBits(cmf, CMFV_LIPS, ge, lips);

		uint nose = GB(face, 13, 3);
		if (ge == GE_WF) {
			nose = (nose * 3 >> 3) * 3 >> 2; // There is 'hole' in the nose sprites for females
		} else {
			nose = ScaleCompanyManagerFaceValue(CMFV_NOSE, ge, nose);
		}
		SetCompanyManagerFaceBits(cmf, CMFV_NOSE, ge, nose);
	}

	uint tie_earring = GB(face, 24, 4);
	if (!HasBit(ge, GENDER_FEMALE) || tie_earring < 3) { // Not all females have an earring
		if (HasBit(ge, GENDER_FEMALE)) SetCompanyManagerFaceBits(cmf, CMFV_HAS_TIE_EARRING, ge, true);
		SetCompanyManagerFaceBits(cmf, CMFV_TIE_EARRING, ge, HasBit(ge, GENDER_FEMALE) ? tie_earring : ScaleCompanyManagerFaceValue(CMFV_TIE_EARRING, ge, tie_earring / 2));
	}

	return cmf;
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
			case CMFV_LIPS:        /* FALL THROUGH */
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

	if (cid == _local_company) InvalidateWindow(WC_STATUS_BAR, 0);
	InvalidateWindow(WC_FINANCES, cid);
}

bool CheckCompanyHasMoney(CommandCost cost)
{
	if (cost.GetCost() > 0) {
		CompanyID company = _current_company;
		if (IsValidCompanyID(company) && cost.GetCost() > GetCompany(company)->money) {
			SetDParam(0, cost.GetCost());
			_error_message = STR_0003_NOT_ENOUGH_CASH_REQUIRES;
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
	CompanyID cid = _current_company;

	if (IsValidCompanyID(cid)) SubtractMoneyFromAnyCompany(GetCompany(cid), cost);
}

void SubtractMoneyFromCompanyFract(CompanyID company, CommandCost cst)
{
	Company *c = GetCompany(company);
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
		if (!IsValidCompanyID(owner)) {
			SetDParam(0, STR_0150_SOMEONE);
		} else {
			SetDParam(0, STR_COMPANY_NAME);
			SetDParam(1, owner);
		}
	} else {
		const Town *t = ClosestTownFromTile(tile, UINT_MAX);

		SetDParam(0, STR_TOWN);
		SetDParam(1, t->index);
	}
}


bool CheckOwnership(Owner owner)
{
	assert(owner < OWNER_END);

	if (owner == _current_company) return true;
	_error_message = STR_013B_OWNED_BY;
	GetNameOfOwner(owner, 0);
	return false;
}

bool CheckTileOwnership(TileIndex tile)
{
	Owner owner = GetTileOwner(tile);

	assert(owner < OWNER_END);

	if (owner == _current_company) return true;
	_error_message = STR_013B_OWNED_BY;

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
	char buffer[100];

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

		if (!IsHumanCompany(c->index)) {
			CompanyNewsInformation *cni = MallocT<CompanyNewsInformation>(1);
			cni->FillData(c);
			SetDParam(0, STR_705E_NEW_TRANSPORT_COMPANY_LAUNCHED);
			SetDParam(1, STR_705F_STARTS_CONSTRUCTION_NEAR);
			SetDParamStr(2, cni->company_name);
			SetDParam(3, t->index);
			AddNewsItem(STR_02B6, NS_COMPANY_NEW, c->last_build_coordinate, 0, cni);
		}
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

static byte GenerateCompanyColour()
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

	/* Move the colors that look similar to each company's color to the side */
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

	/* Return the first available color */
	for (uint i = 0; i < COLOUR_END; i++) {
		if (colours[i] != INVALID_COLOUR) return colours[i];
	}

	NOT_REACHED();
}

static void GeneratePresidentName(Company *c)
{
	Company *cc;
	char buffer[100], buffer2[40];

	for (;;) {
restart:;

		c->president_name_2 = Random();
		c->president_name_1 = SPECSTR_PRESIDENT_NAME;

		SetDParam(0, c->index);
		GetString(buffer, STR_PRESIDENT_NAME, lastof(buffer));
		if (strlen(buffer) >= 32 || GetStringBoundingBox(buffer).width >= 94)
			continue;

		FOR_ALL_COMPANIES(cc) {
			if (c != cc) {
				SetDParam(0, cc->index);
				GetString(buffer2, STR_PRESIDENT_NAME, lastof(buffer2));
				if (strcmp(buffer2, buffer) == 0)
					goto restart;
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
 * @return the company struct
 */
Company *DoStartupNewCompany(bool is_ai)
{
	if (!Company::CanAllocateItem()) return NULL;

	Company *c = new Company(STR_SV_UNNAMED, is_ai);

	memset(&_companies_ai[c->index], 0, sizeof(CompanyAI));
	memset(&_companies_ainew[c->index], 0, sizeof(CompanyAiNew));

	/* Make a color */
	c->colour = GenerateCompanyColour();
	ResetCompanyLivery(c);
	_company_colours[c->index] = c->colour;

	c->money = c->current_loan = 100000;

	_companies_ai[c->index].state = 5; // AIS_WANT_NEW_ROUTE
	c->share_owners[0] = c->share_owners[1] = c->share_owners[2] = c->share_owners[3] = INVALID_OWNER;

	c->avail_railtypes = GetCompanyRailtypes(c->index);
	c->avail_roadtypes = GetCompanyRoadtypes(c->index);
	c->inaugurated_year = _cur_year;
	RandomCompanyManagerFaceBits(c->face, (GenderEthnicity)Random(), false); // create a random company manager face

	/* Engine renewal settings */
	c->engine_renew_list = NULL;
	c->renew_keep_length = false;
	c->engine_renew = _settings_client.gui.autorenew;
	c->engine_renew_months = _settings_client.gui.autorenew_months;
	c->engine_renew_money = _settings_client.gui.autorenew_money;

	GeneratePresidentName(c);

	InvalidateWindow(WC_GRAPH_LEGEND, 0);
	InvalidateWindow(WC_TOOLBAR_MENU, 0);
	InvalidateWindow(WC_CLIENT_LIST, 0);

	if (is_ai && (!_networking || _network_server) && _ai.enabled)
		AI_StartNewAI(c->index);

	c->num_engines = CallocT<uint16>(GetEnginePoolSize());

	return c;
}

void StartupCompanies()
{
	/* The AI starts like in the setting with +2 month max */
	_next_competitor_start = _settings_game.difficulty.competitor_start_time * 90 * DAY_TICKS + RandomRange(60 * DAY_TICKS) + 1;
}

static void MaybeStartNewCompany()
{
	uint n;
	Company *c;

	/* count number of competitors */
	n = 0;
	FOR_ALL_COMPANIES(c) {
		if (c->is_ai) n++;
	}

	/* when there's a lot of computers in game, the probability that a new one starts is lower */
	if (n < (uint)_settings_game.difficulty.max_no_competitors &&
			n < (_network_server ?
				InteractiveRandomRange(_settings_game.difficulty.max_no_competitors + 2) :
				RandomRange(_settings_game.difficulty.max_no_competitors + 2)
			)) {
		/* Send a command to all clients to start up a new AI.
		 * Works fine for Multiplayer and Singleplayer */
		DoCommandP(0, 1, 0, NULL, CMD_COMPANY_CTRL);
	}

	/* The next AI starts like the difficulty setting said, with +2 month max */
	_next_competitor_start = _settings_game.difficulty.competitor_start_time * 90 * DAY_TICKS + 1;
	_next_competitor_start += _network_server ? InteractiveRandomRange(60 * DAY_TICKS) : RandomRange(60 * DAY_TICKS);
}

void InitializeCompanies()
{
	_Company_pool.CleanPool();
	_Company_pool.AddBlockToPool();
	_cur_company_tick_index = 0;
}

void OnTick_Companies()
{
	if (_game_mode == GM_EDITOR) return;

	if (IsValidCompanyID((CompanyID)_cur_company_tick_index)) {
		Company *c = GetCompany((CompanyID)_cur_company_tick_index);
		if (c->name_1 != 0) GenerateCompanyName(c);

		if (AI_AllowNewAI() && _game_mode != GM_MENU && !--_next_competitor_start) {
			MaybeStartNewCompany();
		}
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
		InvalidateWindow(WC_FINANCES, c->index);
	}

	if (_settings_client.gui.show_finances && _local_company != COMPANY_SPECTATOR) {
		ShowCompanyFinances(_local_company);
		c = GetCompany(_local_company);
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
 * @param p1 bits 0-3 command
 * - p1 = 0 - change auto renew bool
 * - p1 = 1 - change auto renew months
 * - p1 = 2 - change auto renew money
 * - p1 = 3 - change auto renew array
 * - p1 = 4 - change bool, months & money all together
 * - p1 = 5 - change renew_keep_length
 * @param p2 value to set
 * if p1 = 0, then:
 * - p2 = enable engine renewal
 * if p1 = 1, then:
 * - p2 = months left before engine expires to replace it
 * if p1 = 2, then
 * - p2 = minimum amount of money available
 * if p1 = 3, then:
 * - p1 bits 16-31 = engine group
 * - p2 bits  0-15 = old engine type
 * - p2 bits 16-31 = new engine type
 * if p1 = 4, then:
 * - p1 bit     15 = enable engine renewal
 * - p1 bits 16-31 = months left before engine expires to replace it
 * - p2 bits  0-31 = minimum amount of money available
 * if p1 = 5, then
 * - p2 = enable renew_keep_length
 */
CommandCost CmdSetAutoReplace(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (!IsValidCompanyID(_current_company)) return CMD_ERROR;

	Company *c = GetCompany(_current_company);
	switch (GB(p1, 0, 3)) {
		case 0:
			if (c->engine_renew == HasBit(p2, 0)) return CMD_ERROR;

			if (flags & DC_EXEC) {
				c->engine_renew = HasBit(p2, 0);
				if (IsLocalCompany()) {
					_settings_client.gui.autorenew = c->engine_renew;
					InvalidateWindow(WC_GAME_OPTIONS, 0);
				}
			}
			break;

		case 1:
			if (Clamp((int16)p2, -12, 12) != (int16)p2) return CMD_ERROR;
			if (c->engine_renew_months == (int16)p2) return CMD_ERROR;

			if (flags & DC_EXEC) {
				c->engine_renew_months = (int16)p2;
				if (IsLocalCompany()) {
					_settings_client.gui.autorenew_months = c->engine_renew_months;
					InvalidateWindow(WC_GAME_OPTIONS, 0);
				}
			}
			break;

		case 2:
			if (ClampU(p2, 0, 2000000) != p2) return CMD_ERROR;
			if (c->engine_renew_money == p2) return CMD_ERROR;

			if (flags & DC_EXEC) {
				c->engine_renew_money = p2;
				if (IsLocalCompany()) {
					_settings_client.gui.autorenew_money = c->engine_renew_money;
					InvalidateWindow(WC_GAME_OPTIONS, 0);
				}
			}
			break;

		case 3: {
			EngineID old_engine_type = GB(p2, 0, 16);
			EngineID new_engine_type = GB(p2, 16, 16);
			GroupID id_g = GB(p1, 16, 16);
			CommandCost cost;

			if (!IsValidGroupID(id_g) && !IsAllGroupID(id_g) && !IsDefaultGroupID(id_g)) return CMD_ERROR;
			if (new_engine_type != INVALID_ENGINE) {
				if (!CheckAutoreplaceValidity(old_engine_type, new_engine_type, _current_company)) return CMD_ERROR;

				cost = AddEngineReplacementForCompany(c, old_engine_type, new_engine_type, id_g, flags);
			} else {
				cost = RemoveEngineReplacementForCompany(c, old_engine_type, id_g, flags);
			}

			if (IsLocalCompany()) InvalidateAutoreplaceWindow(old_engine_type, id_g);

			return cost;
		}

		case 4:
			if (Clamp((int16)GB(p1, 16, 16), -12, 12) != (int16)GB(p1, 16, 16)) return CMD_ERROR;
			if (ClampU(p2, 0, 2000000) != p2) return CMD_ERROR;

			if (flags & DC_EXEC) {
				c->engine_renew = HasBit(p1, 15);
				c->engine_renew_months = (int16)GB(p1, 16, 16);
				c->engine_renew_money = p2;

				if (IsLocalCompany()) {
					_settings_client.gui.autorenew = c->engine_renew;
					_settings_client.gui.autorenew_months = c->engine_renew_months;
					_settings_client.gui.autorenew_money = c->engine_renew_money;
					InvalidateWindow(WC_GAME_OPTIONS, 0);
				}
			}
			break;

		case 5:
			if (c->renew_keep_length == HasBit(p2, 0)) return CMD_ERROR;

			if (flags & DC_EXEC) {
				c->renew_keep_length = HasBit(p2, 0);
				if (IsLocalCompany()) {
					InvalidateWindow(WC_REPLACE_VEHICLE, VEH_TRAIN);
				}
			}
		break;
	}

	return CommandCost();
}

/**
 * Fill the CompanyNewsInformation struct with the required data.
 * @param p the current company.
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
	GetString(this->president_name, STR_7058_PRESIDENT, lastof(this->president_name));

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
 * - p1 = 3 - merge two companies together. merge #1 with #2. Identified by p2
 * @param p2 various functionality, dictated by p1
 * - p1 = 0 - ClientID of the newly created client
 * - p1 = 2 - CompanyID of the that is getting deleted
 * - p1 = 3 - #1 p2 = (bit  0-15) - company to merge (p2 & 0xFFFF)
 *          - #2 p2 = (bit 16-31) - company to be merged into ((p2>>16)&0xFFFF)
 * @todo In the case of p1=0, create new company, the clientID of the new client is in parameter
 * p2. This parameter is passed in at function DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_COMMAND)
 * on the server itself. First of all this is unbelievably ugly; second of all, well,
 * it IS ugly! <b>Someone fix this up :)</b> So where to fix?@n
 * @arg - network_server.c:838 DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_COMMAND)@n
 * @arg - network_client.c:536 DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_MAP) from where the map has been received
 */
CommandCost CmdCompanyCtrl(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (flags & DC_EXEC) _current_company = OWNER_NONE;

	InvalidateWindowData(WC_COMPANY_LEAGUE, 0, 0);

	switch (p1) {
		case 0: { /* Create a new company */
			/* This command is only executed in a multiplayer game */
			if (!_networking) return CMD_ERROR;

#ifdef ENABLE_NETWORK

			/* Joining Client:
			* _local_company: COMPANY_SPECTATOR
			* _network_playas/cid = requested company/clientid
			*
			* Other client(s)/server:
			* _local_company/_network_playas: what they play as
			* cid = requested company/company of joining client */
			ClientIndex cid = (ClientIndex)p2;

			/* Has the network client a correct ClientIndex? */
			if (!(flags & DC_EXEC)) return CommandCost();
			if (cid >= MAX_CLIENT_INFO) return CommandCost();

			/* Delete multiplayer progress bar */
			DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);

			Company *c = DoStartupNewCompany(false);

			/* A new company could not be created, revert to being a spectator */
			if (c == NULL) {
				if (_network_server) {
					NetworkClientInfo *ci = NetworkFindClientInfoFromIndex(cid);
					ci->client_playas = COMPANY_SPECTATOR;
					NetworkUpdateClientInfo(ci->client_id);
				} else if (_local_company == COMPANY_SPECTATOR) {
					_network_playas = COMPANY_SPECTATOR;
				}
				break;
			}

			/* This is the joining client who wants a new company */
			if (_local_company != _network_playas && _network_playas == c->index) {
				assert(_local_company == COMPANY_SPECTATOR);
				SetLocalCompany(c->index);
				if (!StrEmpty(_settings_client.network.default_company_pass)) {
					char *password = _settings_client.network.default_company_pass;
					NetworkChangeCompanyPassword(1, &password);
				}

				_current_company = _local_company;

				/* Now that we have a new company, broadcast our autorenew settings to
				* all clients so everything is in sync */
				NetworkSend_Command(0,
					(_settings_client.gui.autorenew << 15 ) | (_settings_client.gui.autorenew_months << 16) | 4,
					_settings_client.gui.autorenew_money,
					CMD_SET_AUTOREPLACE,
					NULL
				);

				MarkWholeScreenDirty();
			}

			if (_network_server) {
				/* XXX - UGLY! p2 (pid) is mis-used to fetch the client-id, done at
				* server-side in network_server.c:838, function
				* DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_COMMAND) */
				NetworkClientInfo *ci = NetworkFindClientInfoFromIndex(cid);
				ci->client_playas = c->index;
				NetworkUpdateClientInfo(ci->client_id);

				if (IsValidCompanyID(ci->client_playas)) {
					CompanyID company_backup = _local_company;
					_network_company_states[c->index].months_empty = 0;

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
					_cmd_text = ci->client_name;
					_local_company = ci->client_playas;
					NetworkSend_Command(0, 0, 0, CMD_RENAME_PRESIDENT, NULL);
					_local_company = company_backup;
				}
			}
#endif /* ENABLE_NETWORK */
		} break;

		case 1: /* Make a new AI company */
			if (!(flags & DC_EXEC)) return CommandCost();

			DoStartupNewCompany(true);
			break;

		case 2: { /* Delete a company */
			Company *c;

			if (!IsValidCompanyID((CompanyID)p2)) return CMD_ERROR;

			if (!(flags & DC_EXEC)) return CommandCost();

			c = GetCompany((CompanyID)p2);

			/* Only allow removal of HUMAN companies */
			if (IsHumanCompany(c->index)) {
				/* Delete any open window of the company */
				DeleteCompanyWindows(c->index);

				CompanyNewsInformation *cni = MallocT<CompanyNewsInformation>(1);
				cni->FillData(c);

				/* Show the bankrupt news */
				SetDParam(0, STR_705C_BANKRUPT);
				SetDParam(1, STR_705D_HAS_BEEN_CLOSED_DOWN_BY);
				SetDParamStr(2, cni->company_name);
				AddNewsItem(STR_02B6, NS_COMPANY_BANKRUPT, 0, 0, cni);

				/* Remove the company */
				ChangeOwnershipOfCompanyItems(c->index, INVALID_OWNER);

				delete c;
			}
		} break;

		case 3: { /* Merge a company (#1) into another company (#2), elimination company #1 */
			CompanyID cid_old = (CompanyID)GB(p2,  0, 16);
			CompanyID cid_new = (CompanyID)GB(p2, 16, 16);

			if (!IsValidCompanyID(cid_old) || !IsValidCompanyID(cid_new)) return CMD_ERROR;

			if (!(flags & DC_EXEC)) return CMD_ERROR;

			ChangeOwnershipOfCompanyItems(cid_old, cid_new);
			delete GetCompany(cid_old);
		} break;

		default: return CMD_ERROR;
	}

	return CommandCost();
}

static const StringID _endgame_perf_titles[] = {
	STR_0213_BUSINESSMAN,
	STR_0213_BUSINESSMAN,
	STR_0213_BUSINESSMAN,
	STR_0213_BUSINESSMAN,
	STR_0213_BUSINESSMAN,
	STR_0214_ENTREPRENEUR,
	STR_0214_ENTREPRENEUR,
	STR_0215_INDUSTRIALIST,
	STR_0215_INDUSTRIALIST,
	STR_0216_CAPITALIST,
	STR_0216_CAPITALIST,
	STR_0217_MAGNATE,
	STR_0217_MAGNATE,
	STR_0218_MOGUL,
	STR_0218_MOGUL,
	STR_0219_TYCOON_OF_THE_CENTURY
};

StringID EndGameGetPerformanceTitleFromValue(uint value)
{
	value = minu(value / 64, lengthof(_endgame_perf_titles) - 1);

	return _endgame_perf_titles[value];
}

/** Save the highscore for the company */
int8 SaveHighScoreValue(const Company *c)
{
	HighScore *hs = _highscore_table[_settings_game.difficulty.diff_level];
	uint i;
	uint16 score = c->old_economy[0].performance_history;

	/* Exclude cheaters from the honour of being in the highscore table */
	if (CheatHasBeenUsed()) return -1;

	for (i = 0; i < lengthof(_highscore_table[0]); i++) {
		/* You are in the TOP5. Move all values one down and save us there */
		if (hs[i].score <= score) {
			/* move all elements one down starting from the replaced one */
			memmove(&hs[i + 1], &hs[i], sizeof(HighScore) * (lengthof(_highscore_table[0]) - i - 1));
			SetDParam(0, c->index);
			SetDParam(1, c->index);
			GetString(hs[i].company, STR_HIGHSCORE_NAME, lastof(hs[i].company)); // get manager/company name string
			hs[i].score = score;
			hs[i].title = EndGameGetPerformanceTitleFromValue(score);
			return i;
		}
	}

	return -1; // too bad; we did not make it into the top5
}

/** Sort all companies given their performance */
static int CDECL HighScoreSorter(const Company* const *a, const Company* const *b)
{
	return (*b)->old_economy[0].performance_history - (*a)->old_economy[0].performance_history;
}

/* Save the highscores in a network game when it has ended */
#define LAST_HS_ITEM lengthof(_highscore_table) - 1
int8 SaveHighScoreValueNetwork()
{
	const Company *c;
	const Company *cl[MAX_COMPANIES];
	uint count = 0;
	int8 company = -1;

	/* Sort all active companies with the highest score first */
	FOR_ALL_COMPANIES(c) cl[count++] = c;

	GSortT(cl, count, &HighScoreSorter);

	{
		uint i;

		memset(_highscore_table[LAST_HS_ITEM], 0, sizeof(_highscore_table[0]));

		/* Copy over Top5 companies */
		for (i = 0; i < lengthof(_highscore_table[LAST_HS_ITEM]) && i < count; i++) {
			HighScore* hs = &_highscore_table[LAST_HS_ITEM][i];

			SetDParam(0, cl[i]->index);
			SetDParam(1, cl[i]->index);
			GetString(hs->company, STR_HIGHSCORE_NAME, lastof(hs->company)); // get manager/company name string
			hs->score = cl[i]->old_economy[0].performance_history;
			hs->title = EndGameGetPerformanceTitleFromValue(hs->score);

			/* get the ranking of the local company */
			if (cl[i]->index == _local_company) company = i;
		}
	}

	/* Add top5 companys to highscore table */
	return company;
}

/** Save HighScore table to file */
void SaveToHighScore()
{
	FILE *fp = fopen(_highscore_file, "wb");

	if (fp != NULL) {
		uint i;
		HighScore *hs;

		for (i = 0; i < LAST_HS_ITEM; i++) { // don't save network highscores
			for (hs = _highscore_table[i]; hs != endof(_highscore_table[i]); hs++) {
				/* First character is a command character, so strlen will fail on that */
				byte length = min(sizeof(hs->company), StrEmpty(hs->company) ? 0 : (int)strlen(&hs->company[1]) + 1);

				if (fwrite(&length, sizeof(length), 1, fp)       != 1 || // write away string length
						fwrite(hs->company, length, 1, fp)           >  1 || // Yes... could be 0 bytes too
						fwrite(&hs->score, sizeof(hs->score), 1, fp) != 1 ||
						fwrite("  ", 2, 1, fp)                       != 1) { // XXX - placeholder for hs->title, not saved anymore; compatibility
					DEBUG(misc, 1, "Could not save highscore.");
					i = LAST_HS_ITEM;
					break;
				}
			}
		}
		fclose(fp);
	}
}

/** Initialize the highscore table to 0 and if any file exists, load in values */
void LoadFromHighScore()
{
	FILE *fp = fopen(_highscore_file, "rb");

	memset(_highscore_table, 0, sizeof(_highscore_table));

	if (fp != NULL) {
		uint i;
		HighScore *hs;

		for (i = 0; i < LAST_HS_ITEM; i++) { // don't load network highscores
			for (hs = _highscore_table[i]; hs != endof(_highscore_table[i]); hs++) {
				byte length;
				if (fread(&length, sizeof(length), 1, fp)       !=  1 ||
						fread(hs->company, length, 1, fp)           >   1 || // Yes... could be 0 bytes too
						fread(&hs->score, sizeof(hs->score), 1, fp) !=  1 ||
						fseek(fp, 2, SEEK_CUR)                      == -1) { // XXX - placeholder for hs->title, not saved anymore; compatibility
					DEBUG(misc, 1, "Highscore corrupted");
					i = LAST_HS_ITEM;
					break;
				}
				*lastof(hs->company) = '\0';
				hs->title = EndGameGetPerformanceTitleFromValue(hs->score);
			}
		}
		fclose(fp);
	}

	/* Initialize end of game variable (when to show highscore chart) */
	_settings_client.gui.ending_year = 2051;
}

/* Save/load of companies */
static const SaveLoad _company_desc[] = {
	    SLE_VAR(Company, name_2,          SLE_UINT32),
	    SLE_VAR(Company, name_1,          SLE_STRINGID),
	SLE_CONDSTR(Company, name,            SLE_STR, 0,                       84, SL_MAX_VERSION),

	    SLE_VAR(Company, president_name_1, SLE_UINT16),
	    SLE_VAR(Company, president_name_2, SLE_UINT32),
	SLE_CONDSTR(Company, president_name,  SLE_STR, 0,                       84, SL_MAX_VERSION),

	    SLE_VAR(Company, face,            SLE_UINT32),

	/* money was changed to a 64 bit field in savegame version 1. */
	SLE_CONDVAR(Company, money,                 SLE_VAR_I64 | SLE_FILE_I32,  0, 0),
	SLE_CONDVAR(Company, money,                 SLE_INT64,                   1, SL_MAX_VERSION),

	SLE_CONDVAR(Company, current_loan,          SLE_VAR_I64 | SLE_FILE_I32,  0, 64),
	SLE_CONDVAR(Company, current_loan,          SLE_INT64,                  65, SL_MAX_VERSION),

	    SLE_VAR(Company, colour,                SLE_UINT8),
	    SLE_VAR(Company, money_fraction,        SLE_UINT8),
	SLE_CONDVAR(Company, avail_railtypes,       SLE_UINT8,                   0, 57),
	    SLE_VAR(Company, block_preview,         SLE_UINT8),

	SLE_CONDVAR(Company, cargo_types,           SLE_FILE_U16 | SLE_VAR_U32,  0, 93),
	SLE_CONDVAR(Company, cargo_types,           SLE_UINT32,                 94, SL_MAX_VERSION),
	SLE_CONDVAR(Company, location_of_HQ,        SLE_FILE_U16 | SLE_VAR_U32,  0,  5),
	SLE_CONDVAR(Company, location_of_HQ,        SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Company, last_build_coordinate, SLE_FILE_U16 | SLE_VAR_U32,  0,  5),
	SLE_CONDVAR(Company, last_build_coordinate, SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Company, inaugurated_year,      SLE_FILE_U8  | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(Company, inaugurated_year,      SLE_INT32,                  31, SL_MAX_VERSION),

	    SLE_ARR(Company, share_owners,          SLE_UINT8, 4),

	    SLE_VAR(Company, num_valid_stat_ent,    SLE_UINT8),

	    SLE_VAR(Company, quarters_of_bankrupcy, SLE_UINT8),
	    SLE_VAR(Company, bankrupt_asked,        SLE_UINT8),
	    SLE_VAR(Company, bankrupt_timeout,      SLE_INT16),
	SLE_CONDVAR(Company, bankrupt_value,        SLE_VAR_I64 | SLE_FILE_I32,  0, 64),
	SLE_CONDVAR(Company, bankrupt_value,        SLE_INT64,                  65, SL_MAX_VERSION),

	/* yearly expenses was changed to 64-bit in savegame version 2. */
	SLE_CONDARR(Company, yearly_expenses,       SLE_FILE_I32 | SLE_VAR_I64, 3 * 13, 0, 1),
	SLE_CONDARR(Company, yearly_expenses,       SLE_INT64, 3 * 13,                  2, SL_MAX_VERSION),

	SLE_CONDVAR(Company, is_ai,                 SLE_BOOL, 2, SL_MAX_VERSION),
	SLE_CONDNULL(1, 4, 99),

	/* Engine renewal settings */
	SLE_CONDNULL(512, 16, 18),
	SLE_CONDREF(Company, engine_renew_list,     REF_ENGINE_RENEWS,          19, SL_MAX_VERSION),
	SLE_CONDVAR(Company, engine_renew,          SLE_BOOL,                   16, SL_MAX_VERSION),
	SLE_CONDVAR(Company, engine_renew_months,   SLE_INT16,                  16, SL_MAX_VERSION),
	SLE_CONDVAR(Company, engine_renew_money,    SLE_UINT32,                 16, SL_MAX_VERSION),
	SLE_CONDVAR(Company, renew_keep_length,     SLE_BOOL,                    2, SL_MAX_VERSION), // added with 16.1, but was blank since 2

	/* reserve extra space in savegame here. (currently 63 bytes) */
	SLE_CONDNULL(63, 2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _company_economy_desc[] = {
	/* these were changed to 64-bit in savegame format 2 */
	SLE_CONDVAR(CompanyEconomyEntry, income,              SLE_FILE_I32 | SLE_VAR_I64, 0, 1),
	SLE_CONDVAR(CompanyEconomyEntry, income,              SLE_INT64,                  2, SL_MAX_VERSION),
	SLE_CONDVAR(CompanyEconomyEntry, expenses,            SLE_FILE_I32 | SLE_VAR_I64, 0, 1),
	SLE_CONDVAR(CompanyEconomyEntry, expenses,            SLE_INT64,                  2, SL_MAX_VERSION),
	SLE_CONDVAR(CompanyEconomyEntry, company_value,       SLE_FILE_I32 | SLE_VAR_I64, 0, 1),
	SLE_CONDVAR(CompanyEconomyEntry, company_value,       SLE_INT64,                  2, SL_MAX_VERSION),

	    SLE_VAR(CompanyEconomyEntry, delivered_cargo,     SLE_INT32),
	    SLE_VAR(CompanyEconomyEntry, performance_history, SLE_INT32),

	SLE_END()
};

static const SaveLoad _company_livery_desc[] = {
	SLE_CONDVAR(Livery, in_use,  SLE_BOOL,  34, SL_MAX_VERSION),
	SLE_CONDVAR(Livery, colour1, SLE_UINT8, 34, SL_MAX_VERSION),
	SLE_CONDVAR(Livery, colour2, SLE_UINT8, 34, SL_MAX_VERSION),
	SLE_END()
};

static void SaveLoad_PLYR(Company *c)
{
	int i;

	SlObject(c, _company_desc);

	/* Write AI? */
	if (!IsHumanCompany(c->index)) {
		SaveLoad_AI(c->index);
	}

	/* Write economy */
	SlObject(&c->cur_economy, _company_economy_desc);

	/* Write old economy entries. */
	for (i = 0; i < c->num_valid_stat_ent; i++) {
		SlObject(&c->old_economy[i], _company_economy_desc);
	}

	/* Write each livery entry. */
	int num_liveries = CheckSavegameVersion(63) ? LS_END - 4 : (CheckSavegameVersion(85) ? LS_END - 2: LS_END);
	for (i = 0; i < num_liveries; i++) {
		SlObject(&c->livery[i], _company_livery_desc);
	}

	if (num_liveries < LS_END) {
		/* We want to insert some liveries somewhere in between. This means some have to be moved. */
		memmove(&c->livery[LS_FREIGHT_WAGON], &c->livery[LS_PASSENGER_WAGON_MONORAIL], (LS_END - LS_FREIGHT_WAGON) * sizeof(c->livery[0]));
		c->livery[LS_PASSENGER_WAGON_MONORAIL] = c->livery[LS_MONORAIL];
		c->livery[LS_PASSENGER_WAGON_MAGLEV]   = c->livery[LS_MAGLEV];
	}

	if (num_liveries == LS_END - 4) {
		/* Copy bus/truck liveries over to trams */
		c->livery[LS_PASSENGER_TRAM] = c->livery[LS_BUS];
		c->livery[LS_FREIGHT_TRAM]   = c->livery[LS_TRUCK];
	}
}

static void Save_PLYR()
{
	Company *c;
	FOR_ALL_COMPANIES(c) {
		SlSetArrayIndex(c->index);
		SlAutolength((AutolengthProc*)SaveLoad_PLYR, c);
	}
}

static void Load_PLYR()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		Company *c = new (index) Company();
		SaveLoad_PLYR(c);
		_company_colours[index] = c->colour;

		/* This is needed so an AI is attached to a loaded AI */
		if (c->is_ai && (!_networking || _network_server) && _ai.enabled) {
			/* Clear the memory of the new AI, otherwise we might be doing wrong things. */
			memset(&_companies_ainew[index], 0, sizeof(CompanyAiNew));
			AI_StartNewAI(c->index);
		}
	}
}

extern const ChunkHandler _company_chunk_handlers[] = {
	{ 'PLYR', Save_PLYR, Load_PLYR, CH_ARRAY | CH_LAST},
};
