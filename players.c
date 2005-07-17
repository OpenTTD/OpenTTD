/** @file,
 * sjdlfkasjdf
 */
#include "stdafx.h"
#include "openttd.h"
#include "string.h"
#include "strings.h"
#include "table/strings.h"
#include "map.h"
#include "player.h"
#include "town.h"
#include "vehicle.h"
#include "station.h"
#include "gfx.h"
#include "news.h"
#include "saveload.h"
#include "command.h"
#include "ai.h"
#include "sound.h"
#include "network.h"

extern void StartupEconomy(void);

static const SpriteID cheeks_table[4] = {
	0x325, 0x326,
	0x390, 0x3B0,
};

static const SpriteID mouth_table[3] = {
	0x34C, 0x34D, 0x34F
};

void DrawPlayerFace(uint32 face, int color, int x, int y)
{
	byte flag = 0;

	if ( (int32)face < 0)
		flag |= 1;
	if ((((((face >> 7) ^ face) >> 7) ^ face) & 0x8080000) == 0x8000000)
		flag |= 2;

	/* draw the gradient */
	DrawSprite( (color<<16) + 0x0307836A, x, y);

	/* draw the cheeks */
	DrawSprite(cheeks_table[flag&3], x, y);

	/* draw the chin */
	/* FIXME: real code uses -2 in zoomlevel 1 */
	{
		uint val = (face >> 4) & 3;
		if (!(flag & 2)) {
			DrawSprite(0x327 + (flag&1?0:val), x, y);
		} else {
			DrawSprite((flag&1?0x3B1:0x391) + (val>>1), x, y);
		}
	}
	/* draw the eyes */
	{
		uint val1 = (face >> 6)&15;
		uint val2 = (face >> 20)&7;
		uint32 high = 0x314<<16;

		if (val2 >= 6) {
			high = 0x30F<<16;
			if (val2 != 6)
				high = 0x30D<<16;
		}

		if (!(flag & 2)) {
			if (!(flag & 1)) {
				DrawSprite(high+((val1 * 12 >> 4) + 0x832B), x, y);
			} else {
				DrawSprite(high+(val1 + 0x8337), x, y);
			}
		} else {
			if (!(flag & 1)) {
				DrawSprite(high+((val1 * 11 >> 4) + 0x839A), x, y);
			} else {
				DrawSprite(high+(val1 + 0x83B8), x, y);
			}
		}
	}

	/* draw the mouth */
	{
		uint val = (face >> 10) & 63;
		uint val2;

		if (!(flag&1)) {
			val2 = ((val&0xF) * 15 >> 4);

			if (val2 < 3) {
				DrawSprite((flag&2 ? 0x397 : 0x367) + val2, x, y);
				/* skip the rest */
				goto skip_mouth;
			}

			val2 -= 3;
			if (flag & 2) {
				if (val2 > 8) val2 = 0;
				val2 += 0x3A5 - 0x35B;
			}
			DrawSprite(val2 + 0x35B, x, y);
		} else if (!(flag&2)) {
			DrawSprite(((val&0xF) * 10 >> 4) + 0x351, x, y);
		} else {
			DrawSprite(((val&0xF) * 9 >> 4) + 0x3C8, x, y);
		}

		val >>= 3;

		if (!(flag&2)) {
			if (!(flag&1)) {
				DrawSprite(0x349 + val, x, y);
			} else {
				DrawSprite( mouth_table[(val*3>>3)], x, y);
			}
		} else {
			if (!(flag&1)) {
				DrawSprite(0x393 + (val&3), x, y);
			} else {
				DrawSprite(0x3B3 + (val*5>>3), x, y);
			}
		}

		skip_mouth:;
	}


	/* draw the hair */
	{
		uint val = (face >> 16) & 15;
		if (!(flag&2)) {
			if (!(flag&1)) {
				DrawSprite(0x382 + (val*9>>4), x, y);
			} else {
				DrawSprite(0x38B + (val*5>>4), x, y);
			}
		} else {
			if (!(flag&1)) {
				DrawSprite(0x3D4 + (val*5>>4), x, y);
			} else {
				DrawSprite(0x3D9 + (val*5>>4), x, y);
			}
		}
	}

	/* draw the tie */
	{
		uint val = (face >> 20) & 0xFF;

		if (!(flag&1)) {
			DrawSprite(0x36B + ((val&3)*3>>2), x, y);
			DrawSprite(0x36E + ((val>>2)&3), x, y);
			DrawSprite(0x372 + ((val>>4)*6>>4), x, y);
		} else {
			DrawSprite(0x378 + ((val&3)*3>>2), x, y);
			DrawSprite(0x37B + ((val>>2)&3), x, y);

			val >>= 4;
			if (val < 3) {
				DrawSprite((flag&2 ? 0x3D1 : 0x37F) + val, x, y);
			}
		}
	}

	/* draw the glasses */
	{
		uint val = (face >> 28) & 7;

		if (!(flag&2)) {
			if (val<=1) {
				DrawSprite(0x347 + val, x, y);
			}
		} else {
			if (val<=1) {
				DrawSprite(0x3AE + val, x, y);
			}
		}
	}
}

void InvalidatePlayerWindows(Player *p)
{
	uint pid = p->index;
	if ( (byte)pid == _local_player)
		InvalidateWindow(WC_STATUS_BAR, 0);

	InvalidateWindow(WC_FINANCES, pid);
}

bool CheckPlayerHasMoney(int32 cost)
{
	if (cost > 0) {
		uint pid = _current_player;
		if (pid < MAX_PLAYERS && cost > GetPlayer(pid)->player_money) {
			SetDParam(0, cost);
			_error_message = STR_0003_NOT_ENOUGH_CASH_REQUIRES;
			return false;
		}
	}
	return true;
}

static void SubtractMoneyFromAnyPlayer(Player *p, int32 cost)
{
	p->money64 -= cost;
	UpdatePlayerMoney32(p);

	p->yearly_expenses[0][_yearly_expenses_type] += cost;

	if ( ( 1 << _yearly_expenses_type ) & (1<<7|1<<8|1<<9|1<<10))
		p->cur_economy.income -= cost;
	else if (( 1 << _yearly_expenses_type ) & (1<<2|1<<3|1<<4|1<<5|1<<6|1<<11))
		p->cur_economy.expenses -= cost;

	InvalidatePlayerWindows(p);
}

void SubtractMoneyFromPlayer(int32 cost)
{
	PlayerID pid = _current_player;
	if (pid < MAX_PLAYERS)
		SubtractMoneyFromAnyPlayer(GetPlayer(pid), cost);
}

void SubtractMoneyFromPlayerFract(byte player, int32 cost)
{
	Player *p = GetPlayer(player);
	byte m = p->player_money_fraction;
	p->player_money_fraction = m - (byte)cost;
	cost >>= 8;
	if (p->player_money_fraction > m)
		cost++;
	if (cost != 0)
		SubtractMoneyFromAnyPlayer(p, cost);
}

// the player_money field is kept as it is, but money64 contains the actual amount of money.
void UpdatePlayerMoney32(Player *p)
{
	if (p->money64 < -2000000000)
		p->player_money = -2000000000;
	else if (p->money64 > 2000000000)
		p->player_money = 2000000000;
	else
		p->player_money = (int32)p->money64;
}

void GetNameOfOwner(byte owner, TileIndex tile)
{
	SetDParam(2, owner);

	if (owner != OWNER_TOWN) {
		if (owner >= 8)
			SetDParam(0, STR_0150_SOMEONE);
		else {
			Player *p = GetPlayer(owner);
			SetDParam(0, p->name_1);
			SetDParam(1, p->name_2);
		}
	} else {
		Town *t = ClosestTownFromTile(tile, (uint)-1);
		SetDParam(0, STR_TOWN);
		SetDParam(1, t->index);
	}
}


bool CheckOwnership(byte owner)
{
	assert(owner <= OWNER_WATER);

	if (owner == _current_player)
		return true;
	_error_message = STR_013B_OWNED_BY;
	GetNameOfOwner(owner, 0);
	return false;
}

bool CheckTileOwnership(TileIndex tile)
{
	byte owner = GetTileOwner(tile);

	assert(owner <= OWNER_WATER);

	if (owner == _current_player)
		return true;
	_error_message = STR_013B_OWNED_BY;

	// no need to get the name of the owner unless we're the local player (saves some time)
	if (_current_player == _local_player)
		GetNameOfOwner(owner, tile);
	return false;
}

static void GenerateCompanyName(Player *p)
{
	TileIndex tile;
	Town *t;
	StringID str;
	Player *pp;
	uint32 strp;
	char buffer[100];

	if (p->name_1 != STR_SV_UNNAMED)
		return;

	tile = p->last_build_coordinate;
	if (tile == 0)
		return;

	t = ClosestTownFromTile(tile, (uint)-1);

	if (IS_INT_INSIDE(t->townnametype, SPECSTR_TOWNNAME_START, SPECSTR_TOWNNAME_LAST+1)) {
		str = t->townnametype - SPECSTR_TOWNNAME_START + SPECSTR_PLAYERNAME_START;
		strp = t->townnameparts;

verify_name:;
		// No player must have this name already
		FOR_ALL_PLAYERS(pp) {
			if (pp->name_1 == str && pp->name_2 == strp)
				goto bad_town_name;
		}

		GetString(buffer, str);
		if (strlen(buffer) >= 32 || GetStringWidth(buffer) >= 150)
			goto bad_town_name;

set_name:;
		p->name_1 = str;
		p->name_2 = strp;

		MarkWholeScreenDirty();

		if (!IS_HUMAN_PLAYER(p->index)) {
			SetDParam(0, t->index);
			AddNewsItem(p->index + (4 << 4), NEWS_FLAGS(NM_CALLBACK, NF_TILE, NT_COMPANY_INFO, DNC_BANKRUPCY), p->last_build_coordinate, 0);
		}
		return;
	}
bad_town_name:;

	if (p->president_name_1 == SPECSTR_PRESIDENT_NAME) {
		str = SPECSTR_ANDCO_NAME;
		strp = p->president_name_2;
		goto set_name;
	} else {
		str = SPECSTR_ANDCO_NAME;
		strp = Random();
		goto verify_name;
	}
}

#define COLOR_SWAP(i,j) do { byte t=colors[i];colors[i]=colors[j];colors[j]=t; } while(0)

static const byte _color_sort[16] = {2, 2, 3, 2, 3, 2, 3, 2, 3, 2, 2, 2, 3, 1, 1, 1};
static const byte _color_similar_1[16] = {8, 6, 255, 12,  255, 0, 1, 1, 0, 13,  11,  10, 3,   9,  15, 14};
static const byte _color_similar_2[16] = {5, 7, 255, 255, 255, 8, 7, 6, 5, 12, 255, 255, 9, 255, 255, 255};

static byte GeneratePlayerColor(void)
{
	byte colors[16], pcolor, t2;
	int i,j,n;
	uint32 r;
	Player *p;

	// Initialize array
	for(i=0; i!=16; i++)
		colors[i] = i;

	// And randomize it
	n = 100;
	do {
		r = Random();
		COLOR_SWAP(r & 0xF, (r >> 4) & 0xF);
	} while (--n);

	// Bubble sort it according to the values in table 1
	i = 16;
	do {
		for(j=0; j!=15; j++) {
			if (_color_sort[colors[j]] < _color_sort[colors[j+1]]) {
				COLOR_SWAP(j,j+1);
			}
		}
	} while (--i);

	// Move the colors that look similar to each player's color to the side
	FOR_ALL_PLAYERS(p) if (p->is_active) {
		pcolor = p->player_color;
		for(i=0; i!=16; i++) if (colors[i] == pcolor) {
			colors[i] = 0xFF;

			t2 = _color_similar_1[pcolor];
			if (t2 == 0xFF) break;
			for(i=0; i!=15; i++) {
				if (colors[i] == t2) {
					do COLOR_SWAP(i,i+1); while (++i != 15);
					break;
				}
			}

			t2 = _color_similar_2[pcolor];
			if (t2 == 0xFF) break;
			for(i=0; i!=15; i++) {
				if (colors[i] == t2) {
					do COLOR_SWAP(i,i+1); while (++i != 15);
					break;
				}
			}
			break;
		}
	}

	// Return the first available color
	i = 0;
	for(;;) {
		if (colors[i] != 0xFF)
			return colors[i];
		i++;
	}
}

static void GeneratePresidentName(Player *p)
{
	Player *pp;
	char buffer[100], buffer2[40];

	for(;;) {
restart:;

		p->president_name_2 = Random();
		p->president_name_1 = SPECSTR_PRESIDENT_NAME;

		SetDParam(0, p->president_name_2);
		GetString(buffer, p->president_name_1);
		if (strlen(buffer) >= 32 || GetStringWidth(buffer) >= 94)
			continue;

		FOR_ALL_PLAYERS(pp) {
			if (pp->is_active && p != pp) {
				SetDParam(0, pp->president_name_2);
				GetString(buffer2, pp->president_name_1);
				if (strcmp(buffer2, buffer) == 0)
					goto restart;
			}
		}
		return;
	}
}

extern int GetPlayerMaxRailtype(int p);

static Player *AllocatePlayer(void)
{
	Player *p;
	// Find a free slot
	FOR_ALL_PLAYERS(p) {
		if (!p->is_active) {
			int i = p->index;
			memset(p, 0, sizeof(Player));
			p->index = i;
			return p;
		}
	}
	return NULL;
}

Player *DoStartupNewPlayer(bool is_ai)
{
	Player *p;
	int index;

	p = AllocatePlayer();
	if (p == NULL) return NULL;

	index = p->index;

	// Make a color
	p->player_color = GeneratePlayerColor();
	_player_colors[index] = p->player_color;
	p->name_1 = STR_SV_UNNAMED;
	p->is_active = true;

	p->money64 = p->player_money = p->current_loan = 100000;

	p->is_ai = is_ai;
	p->ai.state = 5; /* AIS_WANT_NEW_ROUTE */
	p->share_owners[0] = p->share_owners[1] = p->share_owners[2] = p->share_owners[3] = 0xFF;

	p->max_railtype = GetPlayerMaxRailtype(index);
	p->inaugurated_year = _cur_year;
	p->face = Random();

	GeneratePresidentName(p);

	InvalidateWindow(WC_GRAPH_LEGEND, 0);
	InvalidateWindow(WC_TOOLBAR_MENU, 0);
	InvalidateWindow(WC_CLIENT_LIST, 0);

	return p;
}

void StartupPlayers(void)
{
	// The AI starts like in the setting with +2 month max
	_next_competitor_start = _opt.diff.competitor_start_time * 90 * DAY_TICKS + RandomRange(60 * DAY_TICKS) + 1;
}

static void MaybeStartNewPlayer(void)
{
	uint n;
	Player *p;

	// count number of competitors
	n = 0;
	for(p=_players; p!=endof(_players); p++)
		if (p->is_active && p->is_ai)
			n++;

	// when there's a lot of computers in game, the probability that a new one starts is lower
	if (n < (uint)_opt.diff.max_no_competitors)
		if (n < (!_network_server) ? RandomRange(_opt.diff.max_no_competitors + 2) : InteractiveRandomRange(_opt.diff.max_no_competitors + 2))
		DoStartupNewPlayer(true);

	// The next AI starts like the difficulty setting said, with +2 month max
	_next_competitor_start = _opt.diff.competitor_start_time * 90 * DAY_TICKS + 1;
	_next_competitor_start += (!_network_server) ? RandomRange(60 * DAY_TICKS) : InteractiveRandomRange(60 * DAY_TICKS);
}

void InitializePlayers(void)
{
	int i;
	memset(_players, 0, sizeof(_players));
	for(i = 0; i != MAX_PLAYERS; i++)
		_players[i].index=i;
	_cur_player_tick_index = 0;
}

void OnTick_Players(void)
{
	Player *p;

	if (_game_mode == GM_EDITOR)
		return;

	p = GetPlayer(_cur_player_tick_index);
	_cur_player_tick_index = (_cur_player_tick_index + 1) % MAX_PLAYERS;
	if (p->name_1 != 0) GenerateCompanyName(p);

	if (!_networking && _game_mode != GM_MENU && !--_next_competitor_start) {
		MaybeStartNewPlayer();
	}
}

void RunOtherPlayersLoop(void)
{
	Player *p;

	_is_ai_player = true;

	FOR_ALL_PLAYERS(p) {
		if (p->is_active && p->is_ai) {
			_current_player = p->index;
			if (_patches.ainew_active)
				AiNewDoGameLoop(p);
			else
				AiDoGameLoop(p);
		}
	}

	_is_ai_player = false;
	_current_player = OWNER_NONE;
}

// index is the next parameter in _decode_parameters to set up
StringID GetPlayerNameString(byte player, byte index)
{
	if (IS_HUMAN_PLAYER(player) && player < MAX_PLAYERS) {
		SetDParam(index, player+1);
		return STR_7002_PLAYER;
	}
	return STR_EMPTY;
}

extern void ShowPlayerFinances(int player);

void PlayersYearlyLoop(void)
{
	Player *p;

	// Copy statistics
	FOR_ALL_PLAYERS(p) {
		if (p->is_active) {
			memmove(&p->yearly_expenses[1], &p->yearly_expenses[0], sizeof(p->yearly_expenses) - sizeof(p->yearly_expenses[0]));
			memset(&p->yearly_expenses[0], 0, sizeof(p->yearly_expenses[0]));
			InvalidateWindow(WC_FINANCES, p->index);
		}
	}

	if (_patches.show_finances && _local_player != OWNER_SPECTATOR) {
		ShowPlayerFinances(_local_player);
		p = GetPlayer(_local_player);
		if (p->num_valid_stat_ent > 5 && p->old_economy[0].performance_history < p->old_economy[4].performance_history) {
			SndPlayFx(SND_01_BAD_YEAR);
		} else {
			SndPlayFx(SND_00_GOOD_YEAR);
		}
	}
}

void DeletePlayerWindows(int pi)
{
	DeleteWindowById(WC_COMPANY, pi);
	DeleteWindowById(WC_FINANCES, pi);
	DeleteWindowById(WC_STATION_LIST, pi);
	/* The vehicle list windows also have station in the window_number
	 * A stationindex of -1 means the global vehicle list */
	DeleteWindowById(WC_TRAINS_LIST, (-1 << 16) | pi);
	DeleteWindowById(WC_ROADVEH_LIST, (-1 << 16) | pi);
	DeleteWindowById(WC_SHIPS_LIST, (-1 << 16) | pi);
	DeleteWindowById(WC_AIRCRAFT_LIST, (-1 << 16) | pi);
	DeleteWindowById(WC_BUY_COMPANY, pi);
}

static void DeletePlayerStuff(int pi)
{
	Player *p;

	DeletePlayerWindows(pi);
	p = GetPlayer(pi);
	DeleteName(p->name_1);
	DeleteName(p->president_name_1);
	p->name_1 = 0;
	p->president_name_1 = 0;
}

/** Control the players: add, delete, etc.
 * @param x,y unused
 * @param p1 various functionality
 * - p1 = 0 - create a new player, Which player (network) it will be is in p2
 * - p1 = 1 - create a new AI player
 * - p1 = 2 - delete a player. Player is identified by p2
 * - p1 = 3 - merge two companies together. Player to merge #1 with player #2. Identified by p2
 * @param p2 various functionality, dictated by p1
 * - p1 = 0 - ClientID of the newly created player
 * - p1 = 2 - PlayerID of the that is getting deleted
 * - p1 = 3 - #1 p2 = (bit  0-15) - player to merge (p2 & 0xFFFF)
 *          - #2 p2 = (bit 16-31) - player to be merged into ((p2>>16)&0xFFFF)
 * @todo In the case of p1=0, create new player, the clientID of the new player is in parameter
 * p2. This parameter is passed in at function DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_COMMAND)
 * on the server itself. First of all this is unbelievably ugly; second of all, well,
 * it IS ugly! <b>Someone fix this up :)</b> So where to fix?@n
 * @arg - network_server.c:838 DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_COMMAND)@n
 * @arg - network_client.c:536 DEF_CLIENT_RECEIVE_COMMAND(PACKET_SERVER_MAP) from where the map has been received
 */
int32 CmdPlayerCtrl(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (flags & DC_EXEC) _current_player = OWNER_NONE;

	switch (p1) {
	case 0: { /* Create a new player */
		Player *p;
		PlayerID pid = p2;

		if (!(flags & DC_EXEC) || pid >= MAX_PLAYERS) return 0;

		p = DoStartupNewPlayer(false);

#ifdef ENABLE_NETWORK
		if (_networking && !_network_server && _local_player == OWNER_SPECTATOR)
			/* In case we are a client joining a server... */
			DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
#endif /* ENABLE_NETWORK */

		if (p != NULL) {
			if (_local_player == OWNER_SPECTATOR) {
				/* Check if we do not want to be a spectator in network */
				if (!_networking ||  (_network_server && !_network_dedicated) || _network_playas != OWNER_SPECTATOR) {
					_local_player = p->index;
					MarkWholeScreenDirty();
				}
			}
#ifdef ENABLE_NETWORK
			if (_network_server) {
				/* XXX - UGLY! p2 (pid) is mis-used to fetch the client-id, done at server-side
				 * in network_server.c:838, function DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_COMMAND) */
				NetworkClientInfo *ci = &_network_client_info[pid];
				ci->client_playas = p->index + 1;
				NetworkUpdateClientInfo(ci->client_index);

				if (ci->client_playas != 0 && ci->client_playas <= MAX_PLAYERS) {
					PlayerID player_backup = _local_player;
					_network_player_info[p->index].months_empty = 0;

					/* XXX - When a client joins, we automatically set its name to the
					 * player's name (for some reason). As it stands now only the server
					 * knows the client's name, so it needs to send out a "broadcast" to
					 * do this. To achieve this we send a network command. However, it
					 * uses _local_player to execute the command as.  To prevent abuse
					 * (eg. only yourself can change your name/company), we 'cheat' by
					 * impersonation _local_player as the server. Not the best solution;
					 * but it works.
					 * TODO: Perhaps this could be improved by when the client is ready
					 * with joining to let it send itself the command, and not the server?
					 * For example in network_client.c:534? */
					_cmd_text = ci->client_name;
					_local_player = ci->client_playas - 1;
					NetworkSend_Command(0, 0, 0, CMD_CHANGE_PRESIDENT_NAME, NULL);
					_local_player = player_backup;
				}
			}
		} else if (_network_server) {
				/* XXX - UGLY! p2 (pid) is mis-used to fetch the client-id, done at server-side
				* in network_server.c:838, function DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_COMMAND) */
			NetworkClientInfo *ci = &_network_client_info[pid];
			ci->client_playas = OWNER_SPECTATOR;
			NetworkUpdateClientInfo(ci->client_index);
		}
#else
		}
#endif /* ENABLE_NETWORK */
	} break;

	case 1: /* Make a new AI player */
		if (!(flags & DC_EXEC)) return 0;

		DoStartupNewPlayer(true);
		break;

	case 2: { /* Delete a player */
		Player *p;

		if (p2 >= MAX_PLAYERS) return CMD_ERROR;

		if (!(flags & DC_EXEC)) return 0;

		p = GetPlayer(p2);

		/* Only allow removal of HUMAN companies */
		if (IS_HUMAN_PLAYER(p->index)) {
			/* Delete any open window of the company */
			DeletePlayerWindows(p->index);

			/* Show the bankrupt news */
			SetDParam(0, p->name_1);
			SetDParam(1, p->name_2);
			AddNewsItem( (StringID)(p->index + 16*3), NEWS_FLAGS(NM_CALLBACK, 0, NT_COMPANY_INFO, DNC_BANKRUPCY),0,0);

			/* Remove the company */
			ChangeOwnershipOfPlayerItems(p->index, OWNER_SPECTATOR);
			p->money64 = p->player_money = 100000000; // XXX - wtf?
			p->is_active = false;
		}
	} break;

	case 3: { /* Merge a company (#1) into another company (#2), elimination company #1 */
		PlayerID pid_old = p2 & 0xFFFF;
		PlayerID pid_new = (p2 >> 16) & 0xFFFF;

		if (pid_old >= MAX_PLAYERS || pid_new >= MAX_PLAYERS) return CMD_ERROR;

		if (!(flags & DC_EXEC)) return CMD_ERROR;

		ChangeOwnershipOfPlayerItems(pid_old, pid_new);
		DeletePlayerStuff(pid_old);
	} break;
	default: return CMD_ERROR;
	}

	return 0;
}

static const StringID _endgame_performance_titles[16] = {
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
	STR_0219_TYCOON_OF_THE_CENTURY,
};

StringID EndGameGetPerformanceTitleFromValue(uint value)
{
	return _endgame_performance_titles[minu(value, 1000) >> 6];
}

/* Return true if any cheat has been used, false otherwise */
static bool CheatHasBeenUsed(void)
{
	const Cheat* cht = (Cheat*) &_cheats;
	const Cheat* cht_last = &cht[sizeof(_cheats) / sizeof(Cheat)];

	for (; cht != cht_last; cht++) {
		if (cht->been_used)
			return true;
	}

	return false;
}

/* Save the highscore for the player */
int8 SaveHighScoreValue(const Player *p)
{
	HighScore *hs = _highscore_table[_opt.diff_level];
	uint i;
	uint16 score = p->old_economy[0].performance_history;

	/* Exclude cheaters from the honour of being in the highscore table */
	if (CheatHasBeenUsed())
		return -1;

	for (i = 0; i < lengthof(_highscore_table[0]); i++) {
		/* You are in the TOP5. Move all values one down and save us there */
		if (hs[i].score <= score) {
			char buf[sizeof(hs[i].company)];

			// move all elements one down starting from the replaced one
			memmove(&hs[i + 1], &hs[i], sizeof(HighScore) * (lengthof(_highscore_table[0]) - i - 1));
			SetDParam(0, p->president_name_1);
			SetDParam(1, p->president_name_2);
			SetDParam(2, p->name_1);
			SetDParam(3, p->name_2);
			GetString(buf, STR_HIGHSCORE_NAME); // get manager/company name string
			ttd_strlcpy(hs[i].company, buf, sizeof(buf));
			hs[i].score = score;
			hs[i].title = EndGameGetPerformanceTitleFromValue(score);
			return i;
		}
	}

	return -1; // too bad; we did not make it into the top5
}

/* Sort all players given their performance */
static int CDECL HighScoreSorter(const void *a, const void *b)
{
	const Player *pa = *(const Player* const*)a;
	const Player *pb = *(const Player* const*)b;

	return pb->old_economy[0].performance_history - pa->old_economy[0].performance_history;
}

/* Save the highscores in a network game when it has ended */
#define LAST_HS_ITEM lengthof(_highscore_table) - 1
int8 SaveHighScoreValueNetwork(void)
{
	Player *p, *player_sort[MAX_PLAYERS];
	size_t count = 0;
	int8 player = -1;

	/* Sort all active players with the highest score first */
	FOR_ALL_PLAYERS(p) {
		if (p->is_active)
			player_sort[count++] = p;
	}
	qsort(player_sort, count, sizeof(player_sort[0]), HighScoreSorter);

	{
		HighScore *hs;
		Player* const *p_cur = &player_sort[0];
		uint8 i;

		memset(_highscore_table[LAST_HS_ITEM], 0, sizeof(_highscore_table[0]));

		/* Copy over Top5 companies */
		for (i = 0; i < lengthof(_highscore_table[LAST_HS_ITEM]) && i < (uint8)count; i++) {
			char buf[sizeof(_highscore_table[0]->company)];

			hs = &_highscore_table[LAST_HS_ITEM][i];
			SetDParam(0, (*p_cur)->president_name_1);
			SetDParam(1, (*p_cur)->president_name_2);
			SetDParam(2, (*p_cur)->name_1);
			SetDParam(3, (*p_cur)->name_2);
			GetString(buf, STR_HIGHSCORE_NAME); // get manager/company name string

			ttd_strlcpy(hs->company, buf, sizeof(buf));
			hs->score = (*p_cur)->old_economy[0].performance_history;
			hs->title = EndGameGetPerformanceTitleFromValue(hs->score);

			// get the ranking of the local player
			if ((*p_cur)->index == (int8)_local_player)
				player = i;

			p_cur++;
		}
	}

	/* Add top5 players to highscore table */
	return player;
}

/* Save HighScore table to file */
void SaveToHighScore(void)
{
	FILE *fp = fopen(_highscore_file, "w");

	if (fp != NULL) {
		uint i;
		HighScore *hs;

		for (i = 0; i < LAST_HS_ITEM; i++) { // don't save network highscores
			for (hs = _highscore_table[i]; hs != endof(_highscore_table[i]); hs++) {
				/* First character is a command character, so strlen will fail on that */
				byte length = min(sizeof(hs->company), (hs->company[0] == '\0') ? 0 : strlen(&hs->company[1]) + 1);

				fwrite(&length, sizeof(length), 1, fp); // write away string length
				fwrite(hs->company, length, 1, fp);
				fwrite(&hs->score, sizeof(hs->score), 1, fp);
				fwrite(&hs->title, sizeof(hs->title), 1, fp);
			}
		}
		fclose(fp);
	}
}

/* Initialize the highscore table to 0 and if any file exists, load in values */
void LoadFromHighScore(void)
{
	FILE *fp = fopen(_highscore_file, "r");

	memset(_highscore_table, 0, sizeof(_highscore_table));

	if (fp != NULL) {
		uint i;
		HighScore *hs;

		for (i = 0; i < LAST_HS_ITEM; i++) { // don't load network highscores
			for (hs = _highscore_table[i]; hs != endof(_highscore_table[i]); hs++) {
				byte length;
				fread(&length, sizeof(length), 1, fp);

				fread(hs->company, 1, length, fp);
				fread(&hs->score, sizeof(hs->score), 1, fp);
				fread(&hs->title, sizeof(hs->title), 1, fp);
			}
		}
		fclose(fp);
	}

	/* Initialize end of game variable (when to show highscore chart) */
	 _patches.ending_date = 2051;
}

// Save/load of players
static const SaveLoad _player_desc[] = {
	SLE_VAR(Player,name_2,					SLE_UINT32),
	SLE_VAR(Player,name_1,					SLE_STRINGID),

	SLE_VAR(Player,president_name_1,SLE_UINT16),
	SLE_VAR(Player,president_name_2,SLE_UINT32),

	SLE_VAR(Player,face,						SLE_UINT32),

	// money was changed to a 64 bit field in savegame version 1.
	SLE_CONDVAR(Player,money64,			SLE_VAR_I64 | SLE_FILE_I32, 0, 0),
	SLE_CONDVAR(Player,money64,			SLE_INT64, 1, 255),

	SLE_VAR(Player,current_loan,		SLE_INT32),

	SLE_VAR(Player,player_color,		SLE_UINT8),
	SLE_VAR(Player,player_money_fraction,SLE_UINT8),
	SLE_VAR(Player,max_railtype,		SLE_UINT8),
	SLE_VAR(Player,block_preview,		SLE_UINT8),

	SLE_VAR(Player,cargo_types,			SLE_UINT16),
	SLE_CONDVAR(Player, location_of_house,     SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Player, location_of_house,     SLE_UINT32, 6, 255),
	SLE_CONDVAR(Player, last_build_coordinate, SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Player, last_build_coordinate, SLE_UINT32, 6, 255),
	SLE_VAR(Player,inaugurated_year,SLE_UINT8),

	SLE_ARR(Player,share_owners,		SLE_UINT8, 4),

	SLE_VAR(Player,num_valid_stat_ent,SLE_UINT8),

	SLE_VAR(Player,quarters_of_bankrupcy,SLE_UINT8),
	SLE_VAR(Player,bankrupt_asked,	SLE_UINT8),
	SLE_VAR(Player,bankrupt_timeout,SLE_INT16),
	SLE_VAR(Player,bankrupt_value,	SLE_INT32),

	// yearly expenses was changed to 64-bit in savegame version 2.
	SLE_CONDARR(Player,yearly_expenses,	SLE_FILE_I32|SLE_VAR_I64, 3*13, 0, 1),
	SLE_CONDARR(Player,yearly_expenses,	SLE_INT64, 3*13, 2, 255),

	SLE_CONDVAR(Player,is_ai,			SLE_UINT8, 2, 255),
	SLE_CONDVAR(Player,is_active,	SLE_UINT8, 4, 255),

	// reserve extra space in savegame here. (currently 64 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U64 | SLE_VAR_NULL, 8, 2, 255),

	SLE_END()
};

static const SaveLoad _player_economy_desc[] = {
	// these were changed to 64-bit in savegame format 2
	SLE_CONDVAR(PlayerEconomyEntry,income,							SLE_INT32, 0, 1),
	SLE_CONDVAR(PlayerEconomyEntry,expenses,						SLE_INT32, 0, 1),
	SLE_CONDVAR(PlayerEconomyEntry,company_value,				SLE_INT32, 0, 1),
	SLE_CONDVAR(PlayerEconomyEntry,income,	SLE_FILE_I64 | SLE_VAR_I32, 2, 255),
	SLE_CONDVAR(PlayerEconomyEntry,expenses,SLE_FILE_I64 | SLE_VAR_I32, 2, 255),
	SLE_CONDVAR(PlayerEconomyEntry,company_value,SLE_FILE_I64 | SLE_VAR_I32, 2, 255),

	SLE_VAR(PlayerEconomyEntry,delivered_cargo,			SLE_INT32),
	SLE_VAR(PlayerEconomyEntry,performance_history,	SLE_INT32),

	SLE_END()
};

static const SaveLoad _player_ai_desc[] = {
	SLE_VAR(PlayerAI,state,							SLE_UINT8),
	SLE_VAR(PlayerAI,tick,							SLE_UINT8),
	SLE_CONDVAR(PlayerAI,state_counter, SLE_FILE_U16 | SLE_VAR_U32, 0, 12),
	SLE_CONDVAR(PlayerAI,state_counter, SLE_UINT32, 13, 255),
	SLE_VAR(PlayerAI,timeout_counter,		SLE_UINT16),

	SLE_VAR(PlayerAI,state_mode,				SLE_UINT8),
	SLE_VAR(PlayerAI,banned_tile_count,	SLE_UINT8),
	SLE_VAR(PlayerAI,railtype_to_use,		SLE_UINT8),

	SLE_VAR(PlayerAI,cargo_type,				SLE_UINT8),
	SLE_VAR(PlayerAI,num_wagons,				SLE_UINT8),
	SLE_VAR(PlayerAI,build_kind,				SLE_UINT8),
	SLE_VAR(PlayerAI,num_build_rec,			SLE_UINT8),
	SLE_VAR(PlayerAI,num_loco_to_build,	SLE_UINT8),
	SLE_VAR(PlayerAI,num_want_fullload,	SLE_UINT8),

	SLE_VAR(PlayerAI,route_type_mask,		SLE_UINT8),

	SLE_CONDVAR(PlayerAI, start_tile_a, SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(PlayerAI, start_tile_a, SLE_UINT32, 6, 255),
	SLE_CONDVAR(PlayerAI, cur_tile_a,   SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(PlayerAI, cur_tile_a,   SLE_UINT32, 6, 255),
	SLE_VAR(PlayerAI,start_dir_a,				SLE_UINT8),
	SLE_VAR(PlayerAI,cur_dir_a,					SLE_UINT8),

	SLE_CONDVAR(PlayerAI, start_tile_b, SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(PlayerAI, start_tile_b, SLE_UINT32, 6, 255),
	SLE_CONDVAR(PlayerAI, cur_tile_b,   SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(PlayerAI, cur_tile_b,   SLE_UINT32, 6, 255),
	SLE_VAR(PlayerAI,start_dir_b,				SLE_UINT8),
	SLE_VAR(PlayerAI,cur_dir_b,					SLE_UINT8),

	SLE_REF(PlayerAI,cur_veh,						REF_VEHICLE),

	SLE_ARR(PlayerAI,wagon_list,				SLE_UINT16, 9),
	SLE_ARR(PlayerAI,order_list_blocks,	SLE_UINT8, 20),
	SLE_ARR(PlayerAI,banned_tiles,			SLE_UINT16, 16),

	SLE_CONDARR(NullStruct,null,SLE_FILE_U64 | SLE_VAR_NULL, 8, 2, 255),
	SLE_END()
};

static const SaveLoad _player_ai_build_rec_desc[] = {
	SLE_CONDVAR(AiBuildRec,spec_tile, SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(AiBuildRec,spec_tile, SLE_UINT32, 6, 255),
	SLE_CONDVAR(AiBuildRec,use_tile,  SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(AiBuildRec,use_tile,  SLE_UINT32, 6, 255),
	SLE_VAR(AiBuildRec,rand_rng,			SLE_UINT8),
	SLE_VAR(AiBuildRec,cur_building_rule,SLE_UINT8),
	SLE_VAR(AiBuildRec,unk6,					SLE_UINT8),
	SLE_VAR(AiBuildRec,unk7,					SLE_UINT8),
	SLE_VAR(AiBuildRec,buildcmd_a,		SLE_UINT8),
	SLE_VAR(AiBuildRec,buildcmd_b,		SLE_UINT8),
	SLE_VAR(AiBuildRec,direction,			SLE_UINT8),
	SLE_VAR(AiBuildRec,cargo,					SLE_UINT8),
	SLE_END()
};

static void SaveLoad_PLYR(Player *p) {
	int i;

	SlObject(p, _player_desc);

	// Write AI?
	if (!IS_HUMAN_PLAYER(p->index)) {
		SlObject(&p->ai, _player_ai_desc);
		for(i=0; i!=p->ai.num_build_rec; i++)
			SlObject(&p->ai.src + i, _player_ai_build_rec_desc);
	}

	// Write economy
	SlObject(&p->cur_economy, _player_economy_desc);

	// Write old economy entries.
	{
		PlayerEconomyEntry *pe;
		for(i=p->num_valid_stat_ent,pe=p->old_economy; i!=0; i--,pe++)
			SlObject(pe, _player_economy_desc);
	}
}

static void Save_PLYR(void)
{
	Player *p;
	FOR_ALL_PLAYERS(p) {
		if (p->is_active) {
			SlSetArrayIndex(p->index);
			SlAutolength((AutolengthProc*)SaveLoad_PLYR, p);
		}
	}
}

static void Load_PLYR(void)
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		Player *p = GetPlayer(index);
		p->is_ai = (index != 0);
		SaveLoad_PLYR(p);
		_player_colors[index] = p->player_color;
		UpdatePlayerMoney32(p);
	}
}

const ChunkHandler _player_chunk_handlers[] = {
	{ 'PLYR', Save_PLYR, Load_PLYR, CH_ARRAY | CH_LAST},
};

