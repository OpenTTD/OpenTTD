/* $Id$ */

/** @file misc_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "heightmap.h"
#include "debug.h"
#include "functions.h"
#include "landscape.h"
#include "newgrf.h"
#include "newgrf_text.h"
#include "saveload.h"
#include "strings.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "strings.h"
#include "table/tree_land.h"
#include "map.h"
#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "station.h"
#include "command.h"
#include "player.h"
#include "town.h"
#include "sound.h"
#include "network/network.h"
#include "string.h"
#include "variables.h"
#include "vehicle.h"
#include "train.h"
#include "tgp.h"
#include "settings.h"
#include "date.h"
#include "cargotype.h"
#include "player_face.h"
#include "fileio.h"

#include "fileio.h"
#include "fios.h"
/* Variables to display file lists */
FiosItem *_fios_list;
int _saveload_mode;


static bool _fios_path_changed;
static bool _savegame_sort_dirty;

enum {
	LAND_INFO_LINES          =   7,
	LAND_INFO_LINE_BUFF_SIZE = 512,
};

static char _landinfo_data[LAND_INFO_LINES][LAND_INFO_LINE_BUFF_SIZE];

static void LandInfoWndProc(Window *w, WindowEvent *e)
{
	if (e->event == WE_PAINT) {
		DrawWindowWidgets(w);

		DoDrawStringCentered(140, 16, _landinfo_data[0], TC_LIGHT_BLUE);
		DoDrawStringCentered(140, 27, _landinfo_data[1], TC_FROMSTRING);
		DoDrawStringCentered(140, 38, _landinfo_data[2], TC_FROMSTRING);
		DoDrawStringCentered(140, 49, _landinfo_data[3], TC_FROMSTRING);
		DoDrawStringCentered(140, 60, _landinfo_data[4], TC_FROMSTRING);
		if (_landinfo_data[5][0] != '\0') DrawStringMultiCenter(140, 76, BindCString(_landinfo_data[5]), w->width - 4);
		if (_landinfo_data[6][0] != '\0') DoDrawStringCentered(140, 71, _landinfo_data[6], TC_FROMSTRING);
	}
}

static const Widget _land_info_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                       STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   279,     0,    13, STR_01A3_LAND_AREA_INFORMATION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   279,    14,    92, 0x0,                            STR_NULL},
{    WIDGETS_END},
};

static const WindowDesc _land_info_desc = {
	WDP_AUTO, WDP_AUTO, 280, 93, 280, 93,
	WC_LAND_INFO, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_land_info_widgets,
	LandInfoWndProc
};

static void Place_LandInfo(TileIndex tile)
{
	Player *p;
	Window *w;
	Town *t;
	Money old_money;
	CommandCost costclear;
	AcceptedCargo ac;
	TileDesc td;
	StringID str;

	DeleteWindowById(WC_LAND_INFO, 0);

	w = AllocateWindowDesc(&_land_info_desc);
	WP(w, void_d).data = &_landinfo_data;

	p = GetPlayer(IsValidPlayer(_local_player) ? _local_player : PLAYER_FIRST);
	t = ClosestTownFromTile(tile, _patches.dist_local_authority);

	old_money = p->player_money;
	p->player_money = INT64_MAX;
	costclear = DoCommand(tile, 0, 0, 0, CMD_LANDSCAPE_CLEAR);
	p->player_money = old_money;

	/* Because build_date is not set yet in every TileDesc, we make sure it is empty */
	td.build_date = 0;
	GetAcceptedCargo(tile, ac);
	GetTileDesc(tile, &td);

	SetDParam(0, td.dparam[0]);
	GetString(_landinfo_data[0], td.str, lastof(_landinfo_data[0]));

	SetDParam(0, STR_01A6_N_A);
	if (td.owner != OWNER_NONE && td.owner != OWNER_WATER) GetNameOfOwner(td.owner, tile);
	GetString(_landinfo_data[1], STR_01A7_OWNER, lastof(_landinfo_data[1]));

	str = STR_01A4_COST_TO_CLEAR_N_A;
	if (CmdSucceeded(costclear)) {
		SetDParam(0, costclear.GetCost());
		str = STR_01A5_COST_TO_CLEAR;
	}
	GetString(_landinfo_data[2], str, lastof(_landinfo_data[2]));

	snprintf(_userstring, lengthof(_userstring), "0x%.4X", tile);
	SetDParam(0, TileX(tile));
	SetDParam(1, TileY(tile));
	SetDParam(2, TileHeight(tile));
	SetDParam(3, STR_SPEC_USERSTRING);
	GetString(_landinfo_data[3], STR_LANDINFO_COORDS, lastof(_landinfo_data[3]));

	SetDParam(0, STR_01A9_NONE);
	if (t != NULL && t->IsValid()) {
		SetDParam(0, STR_TOWN);
		SetDParam(1, t->index);
	}
	GetString(_landinfo_data[4], STR_01A8_LOCAL_AUTHORITY, lastof(_landinfo_data[4]));

	{
		char *p = GetString(_landinfo_data[5], STR_01CE_CARGO_ACCEPTED, lastof(_landinfo_data[5]));
		bool found = false;

		for (CargoID i = 0; i < NUM_CARGO; ++i) {
			if (ac[i] > 0) {
				/* Add a comma between each item. */
				if (found) {
					*p++ = ',';
					*p++ = ' ';
				}
				found = true;

				/* If the accepted value is less than 8, show it in 1/8:ths */
				if (ac[i] < 8) {
					SetDParam(0, ac[i]);
					SetDParam(1, GetCargo(i)->name);
					p = GetString(p, STR_01D1_8, lastof(_landinfo_data[5]));
				} else {
					p = GetString(p, GetCargo(i)->name, lastof(_landinfo_data[5]));
				}
			}
		}

		if (!found) _landinfo_data[5][0] = '\0';
	}

	if (td.build_date != 0) {
		SetDParam(0, td.build_date);
	GetString(_landinfo_data[6], STR_BUILD_DATE, lastof(_landinfo_data[6]));
	} else {
		_landinfo_data[6][0] = '\0';
	}

#if defined(_DEBUG)
#	define LANDINFOD_LEVEL 0
#else
#	define LANDINFOD_LEVEL 1
#endif
	DEBUG(misc, LANDINFOD_LEVEL, "TILE: %#x (%i,%i)", tile, TileX(tile), TileY(tile));
	DEBUG(misc, LANDINFOD_LEVEL, "type_height  = %#x", _m[tile].type_height);
	DEBUG(misc, LANDINFOD_LEVEL, "m1           = %#x", _m[tile].m1);
	DEBUG(misc, LANDINFOD_LEVEL, "m2           = %#x", _m[tile].m2);
	DEBUG(misc, LANDINFOD_LEVEL, "m3           = %#x", _m[tile].m3);
	DEBUG(misc, LANDINFOD_LEVEL, "m4           = %#x", _m[tile].m4);
	DEBUG(misc, LANDINFOD_LEVEL, "m5           = %#x", _m[tile].m5);
	DEBUG(misc, LANDINFOD_LEVEL, "m6           = %#x", _m[tile].m6);
	DEBUG(misc, LANDINFOD_LEVEL, "m7           = %#x", _me[tile].m7);
#undef LANDINFOD_LEVEL
}

void PlaceLandBlockInfo()
{
	if (_cursor.sprite == SPR_CURSOR_QUERY) {
		ResetObjectToPlace();
	} else {
		_place_proc = Place_LandInfo;
		SetObjectToPlace(SPR_CURSOR_QUERY, PAL_NONE, VHM_RECT, WC_MAIN_TOOLBAR, 0);
	}
}

static const char *credits[] = {
	/*************************************************************************
	 *                      maximum length of string which fits in window   -^*/
	"Original design by Chris Sawyer",
	"Original graphics by Simon Foster",
	"",
	"The OpenTTD team (in alphabetical order):",
	"  Jean-Francois Claeys (Belugas) - In training, not yet specialized",
	"  Bjarni Corfitzen (Bjarni) - MacOSX port, coder",
	"  Matthijs Kooijman (blathijs) - Pathfinder-guru",
	"  Victor Fischer (Celestar) - Programming everywhere you need him to",
	"  Tamás Faragó (Darkvater) - Lead coder",
	"  Loïc Guilloux (glx) - In training, not yet specialized",
	"  Jaroslav Mazanec (KUDr) - YAPG (Yet Another Pathfinder God) ;)",
	"  Jonathan Coome (Maedhros) - High priest of the newGRF Temple",
	"  Attila Bán (MiHaMiX) - WebTranslator, Nightlies, Wiki and bugtracker host",
	"  Owen Rudge (orudge) - Forum host, OS/2 port",
	"  Peter Nelson (peter1138) - Spiritual descendant from newgrf gods",
	"  Remko Bijker (Rubidium) - THE desync hunter",
	"  Christoph Mallon (Tron) - Programmer, code correctness police",
	"",
	"Retired Developers:",
	"  Ludvig Strigeus (ludde) - OpenTTD author, main coder (0.1 - 0.3.3)",
	"  Serge Paquet (vurlix) - Assistant project manager, coder (0.1 - 0.3.3)",
	"  Dominik Scherer (dominik81) - Lead programmer, GUI expert (0.3.0 - 0.3.6)",
	"  Patric Stout (TrueLight) - Programmer, webhoster (0.3 - pre0.6)",
	"",
	"Special thanks go out to:",
	"  Josef Drexler - For his great work on TTDPatch",
	"  Marcin Grzegorczyk - For his documentation of TTD internals",
	"  Petr Baudis (pasky) - Many patches, newgrf support",
	"  Stefan Meißner (sign_de) - For his work on the console",
	"  Simon Sasburg (HackyKid) - Many bugfixes he has blessed us with (and PBS)",
	"  Cian Duffy (MYOB) - BeOS port / manual writing",
	"  Christian Rosentreter (tokai) - MorphOS / AmigaOS port",
	"  Richard Kempton (richK) - additional airports, initial TGP implementation",
	"",
	"  Michael Blunck - Pre-Signals and Semaphores © 2003",
	"  George - Canal/Lock graphics © 2003-2004",
	"  David Dallaston - Tram tracks",
	"  Marcin Grzegorczyk - Foundations for Tracks on Slopes",
	"  All Translators - Who made OpenTTD a truly international game",
	"  Bug Reporters - Without whom OpenTTD would still be full of bugs!",
	"",
	"",
	"And last but not least:",
	"  Chris Sawyer - For an amazing game!"
};

static void AboutWindowProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_CREATE: // Set up window counter and start position of scroller
		WP(w, scroller_d).counter = 0;
		WP(w, scroller_d).height = w->height - 40;
		break;
	case WE_PAINT: {
		uint i;
		int y = WP(w, scroller_d).height;
		DrawWindowWidgets(w);

		/* Show original copyright and revision version */
		DrawStringCentered(210, 17, STR_00B6_ORIGINAL_COPYRIGHT, TC_FROMSTRING);
		DrawStringCentered(210, 17 + 10, STR_00B7_VERSION, TC_FROMSTRING);

		/* Show all scrolling credits */
		for (i = 0; i < lengthof(credits); i++) {
			if (y >= 50 && y < (w->height - 40)) {
				DoDrawString(credits[i], 10, y, TC_BLACK);
			}
			y += 10;
		}

		/* If the last text has scrolled start anew from the start */
		if (y < 50) WP(w, scroller_d).height = w->height - 40;

		DoDrawStringCentered(210, w->height - 25, "Website: http://www.openttd.org", TC_BLACK);
		DrawStringCentered(210, w->height - 15, STR_00BA_COPYRIGHT_OPENTTD, TC_FROMSTRING);
	} break;
	case WE_MOUSELOOP: // Timer to scroll the text and adjust the new top
		if (WP(w, scroller_d).counter++ % 3 == 0) {
			WP(w, scroller_d).height--;
			SetWindowDirty(w);
		}
		break;
	}
}

static const Widget _about_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,         STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   419,     0,    13, STR_015B_OPENTTD, STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   419,    14,   271, 0x0,              STR_NULL},
{      WWT_FRAME,   RESIZE_NONE,    14,     5,   414,    40,   245, STR_NULL,         STR_NULL},
{    WIDGETS_END},
};

static const WindowDesc _about_desc = {
	WDP_CENTER, WDP_CENTER, 420, 272, 420, 272,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_about_widgets,
	AboutWindowProc
};


void ShowAboutWindow()
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	AllocateWindowDesc(&_about_desc);
}

static int _tree_to_plant;

static const PalSpriteID _tree_sprites[] = {
	{ 0x655, PAL_NONE }, { 0x663, PAL_NONE }, { 0x678, PAL_NONE }, { 0x62B, PAL_NONE },
	{ 0x647, PAL_NONE }, { 0x639, PAL_NONE }, { 0x64E, PAL_NONE }, { 0x632, PAL_NONE },
	{ 0x67F, PAL_NONE }, { 0x68D, PAL_NONE }, { 0x69B, PAL_NONE }, { 0x6A9, PAL_NONE },
	{ 0x6AF, PAL_NONE }, { 0x6D2, PAL_NONE }, { 0x6D9, PAL_NONE }, { 0x6C4, PAL_NONE },
	{ 0x6CB, PAL_NONE }, { 0x6B6, PAL_NONE }, { 0x6BD, PAL_NONE }, { 0x6E0, PAL_NONE },
	{ 0x72E, PAL_NONE }, { 0x734, PAL_NONE }, { 0x74A, PAL_NONE }, { 0x74F, PAL_NONE },
	{ 0x76B, PAL_NONE }, { 0x78F, PAL_NONE }, { 0x788, PAL_NONE }, { 0x77B, PAL_NONE },
	{ 0x75F, PAL_NONE }, { 0x774, PAL_NONE }, { 0x720, PAL_NONE }, { 0x797, PAL_NONE },
	{ 0x79E, PAL_NONE }, { 0x7A5, PALETTE_TO_GREEN }, { 0x7AC, PALETTE_TO_RED }, { 0x7B3, PAL_NONE },
	{ 0x7BA, PAL_NONE }, { 0x7C1, PALETTE_TO_RED, }, { 0x7C8, PALETTE_TO_PALE_GREEN }, { 0x7CF, PALETTE_TO_YELLOW }, { 0x7D6, PALETTE_TO_RED }
};

static void BuildTreesWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		int x,y;
		int i, count;

		DrawWindowWidgets(w);

		WP(w,tree_d).base = i = _tree_base_by_landscape[_opt.landscape];
		WP(w,tree_d).count = count = _tree_count_by_landscape[_opt.landscape];

		x = 18;
		y = 54;
		do {
			DrawSprite(_tree_sprites[i].sprite, _tree_sprites[i].pal, x, y);
			x += 35;
			if (!(++i & 3)) {
				x -= 35 * 4;
				y += 47;
			}
		} while (--count);
	} break;

	case WE_CLICK: {
		int wid = e->we.click.widget;

		switch (wid) {
		case 0:
			ResetObjectToPlace();
			break;

		case 3: case 4: case 5: case 6:
		case 7: case 8: case 9: case 10:
		case 11:case 12: case 13: case 14:
			if (wid - 3 >= WP(w,tree_d).count) break;

			if (HandlePlacePushButton(w, wid, SPR_CURSOR_TREE, VHM_RECT, NULL))
				_tree_to_plant = WP(w,tree_d).base + wid - 3;
			break;

		case 15: // tree of random type.
			if (HandlePlacePushButton(w, 15, SPR_CURSOR_TREE, VHM_RECT, NULL))
				_tree_to_plant = -1;
			break;

		case 16: // place trees randomly over the landscape
			w->LowerWidget(16);
			w->flags4 |= 5 << WF_TIMEOUT_SHL;
			SndPlayFx(SND_15_BEEP);
			PlaceTreesRandomly();
			MarkWholeScreenDirty();
			break;
		}
	} break;

	case WE_PLACE_OBJ:
		VpStartPlaceSizing(e->we.place.tile, VPM_X_AND_Y_LIMITED, DDSP_PLANT_TREES);
		VpSetPlaceSizingLimit(20);
		break;

	case WE_PLACE_DRAG:
		VpSelectTilesWithMethod(e->we.place.pt.x, e->we.place.pt.y, e->we.place.select_method);
		return;

	case WE_PLACE_MOUSEUP:
		if (e->we.place.pt.x != -1 && e->we.place.select_proc == DDSP_PLANT_TREES) {
			DoCommandP(e->we.place.tile, _tree_to_plant, e->we.place.starttile, NULL,
				CMD_PLANT_TREE | CMD_MSG(STR_2805_CAN_T_PLANT_TREE_HERE));
		}
		break;

	case WE_TIMEOUT:
		w->RaiseWidget(16);
		break;

	case WE_ABORT_PLACE_OBJ:
		w->RaiseButtons();
		break;
	}
}

static const Widget _build_trees_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,              STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   142,     0,    13, STR_2802_TREES,        STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   142,    14,   170, 0x0,                   STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   140,   157,   168, STR_TREES_RANDOM_TYPE, STR_TREES_RANDOM_TYPE_TIP},
{    WIDGETS_END},
};

static const WindowDesc _build_trees_desc = {
	497, 22, 143, 171, 143, 171,
	WC_BUILD_TREES, WC_SCEN_LAND_GEN,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_trees_widgets,
	BuildTreesWndProc
};

static const Widget _build_trees_scen_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,              STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   142,     0,    13, STR_2802_TREES,        STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   142,    14,   183, 0x0,                   STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   140,   157,   168, STR_TREES_RANDOM_TYPE, STR_TREES_RANDOM_TYPE_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   140,   170,   181, STR_028A_RANDOM_TREES, STR_028B_PLANT_TREES_RANDOMLY_OVER},
{    WIDGETS_END},
};

static const WindowDesc _build_trees_scen_desc = {
	WDP_AUTO, WDP_AUTO, 143, 184, 143, 184,
	WC_BUILD_TREES, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_trees_scen_widgets,
	BuildTreesWndProc
};


void ShowBuildTreesToolbar()
{
	if (!IsValidPlayer(_current_player)) return;
	AllocateWindowDescFront(&_build_trees_desc, 0);
}

void ShowBuildTreesScenToolbar()
{
	AllocateWindowDescFront(&_build_trees_scen_desc, 0);
}

static uint64 _errmsg_decode_params[20];
static StringID _errmsg_message_1, _errmsg_message_2;
static uint _errmsg_duration;


static const Widget _errmsg_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     4,     0,    10,     0,    13, STR_00C5,         STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     4,    11,   239,     0,    13, STR_00B2_MESSAGE, STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,     4,     0,   239,    14,    45, 0x0,              STR_NULL},
{    WIDGETS_END},
};

static const Widget _errmsg_face_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     4,     0,    10,     0,    13, STR_00C5,              STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     4,    11,   333,     0,    13, STR_00B3_MESSAGE_FROM, STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,     4,     0,   333,    14,   136, 0x0,                   STR_NULL},
{   WIDGETS_END},
};

static void ErrmsgWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT:
		CopyInDParam(0, _errmsg_decode_params, lengthof(_errmsg_decode_params));
		DrawWindowWidgets(w);
		CopyInDParam(0, _errmsg_decode_params, lengthof(_errmsg_decode_params));

		/* If the error message comes from a NewGRF, we must use the text ref. stack reserved for error messages.
		 * If the message doesn't come from a NewGRF, it won't use the TTDP-style text ref. stack, so we won't hurt anything
		 */
		SwitchToErrorRefStack();
		RewindTextRefStack();

		if (!IsWindowOfPrototype(w, _errmsg_face_widgets)) {
			DrawStringMultiCenter(
				120,
				(_errmsg_message_1 == INVALID_STRING_ID ? 25 : 15),
				_errmsg_message_2,
				w->width - 2);
			if (_errmsg_message_1 != INVALID_STRING_ID)
				DrawStringMultiCenter(
					120,
					30,
					_errmsg_message_1,
					w->width - 2);
		} else {
			const Player *p = GetPlayer((PlayerID)GetDParamX(_errmsg_decode_params,2));
			DrawPlayerFace(p->face, p->player_color, 2, 16);

			DrawStringMultiCenter(
				214,
				(_errmsg_message_1 == INVALID_STRING_ID ? 65 : 45),
				_errmsg_message_2,
				w->width - 2);
			if (_errmsg_message_1 != INVALID_STRING_ID)
				DrawStringMultiCenter(
					214,
					90,
					_errmsg_message_1,
					w->width - 2);
		}

		/* Switch back to the normal text ref. stack for NewGRF texts */
		SwitchToNormalRefStack();
		break;

	case WE_MOUSELOOP:
		if (_right_button_down) DeleteWindow(w);
		break;

	case WE_4:
		if (--_errmsg_duration == 0) DeleteWindow(w);
		break;

	case WE_DESTROY:
		SetRedErrorSquare(0);
		_switch_mode_errorstr = INVALID_STRING_ID;
		break;

	case WE_KEYPRESS:
		if (e->we.keypress.keycode == WKC_SPACE) {
			/* Don't continue. */
			e->we.keypress.cont = false;
			DeleteWindow(w);
		}
		break;
	}
}

void ShowErrorMessage(StringID msg_1, StringID msg_2, int x, int y)
{
	Window *w;
	const ViewPort *vp;
	Point pt;

	DeleteWindowById(WC_ERRMSG, 0);

	//assert(msg_2);
	if (msg_2 == 0) msg_2 = STR_EMPTY;

	_errmsg_message_1 = msg_1;
	_errmsg_message_2 = msg_2;
	CopyOutDParam(_errmsg_decode_params, 0, lengthof(_errmsg_decode_params));
	_errmsg_duration = _patches.errmsg_duration;
	if (!_errmsg_duration) return;

	if (_errmsg_message_1 != STR_013B_OWNED_BY || GetDParamX(_errmsg_decode_params,2) >= 8) {

		if ( (x|y) != 0) {
			pt = RemapCoords2(x, y);
			vp = FindWindowById(WC_MAIN_WINDOW, 0)->viewport;

			/* move x pos to opposite corner */
			pt.x = UnScaleByZoom(pt.x - vp->virtual_left, vp->zoom) + vp->left;
			pt.x = (pt.x < (_screen.width >> 1)) ? _screen.width - 260 : 20;

			/* move y pos to opposite corner */
			pt.y = UnScaleByZoom(pt.y - vp->virtual_top, vp->zoom) + vp->top;
			pt.y = (pt.y < (_screen.height >> 1)) ? _screen.height - 80 : 100;

		} else {
			pt.x = (_screen.width - 240) >> 1;
			pt.y = (_screen.height - 46) >> 1;
		}
		w = AllocateWindow(pt.x, pt.y, 240, 46, ErrmsgWndProc, WC_ERRMSG, _errmsg_widgets);
	} else {
		if ( (x|y) != 0) {
			pt = RemapCoords2(x, y);
			vp = FindWindowById(WC_MAIN_WINDOW, 0)->viewport;
			pt.x = Clamp(UnScaleByZoom(pt.x - vp->virtual_left, vp->zoom) + vp->left - (334/2), 0, _screen.width - 334);
			pt.y = Clamp(UnScaleByZoom(pt.y - vp->virtual_top, vp->zoom) + vp->top - (137/2), 22, _screen.height - 137);
		} else {
			pt.x = (_screen.width - 334) >> 1;
			pt.y = (_screen.height - 137) >> 1;
		}
		w = AllocateWindow(pt.x, pt.y, 334, 137, ErrmsgWndProc, WC_ERRMSG, _errmsg_face_widgets);
	}

	w->desc_flags = WDF_STD_BTN | WDF_DEF_WIDGET;
}


void ShowEstimatedCostOrIncome(Money cost, int x, int y)
{
	StringID msg = STR_0805_ESTIMATED_COST;

	if (cost < 0) {
		cost = -cost;
		msg = STR_0807_ESTIMATED_INCOME;
	}
	SetDParam(0, cost);
	ShowErrorMessage(INVALID_STRING_ID, msg, x, y);
}

void ShowCostOrIncomeAnimation(int x, int y, int z, Money cost)
{
	StringID msg;
	Point pt = RemapCoords(x,y,z);

	msg = STR_0801_COST;
	if (cost < 0) {
		cost = -cost;
		msg = STR_0803_INCOME;
	}
	SetDParam(0, cost);
	AddTextEffect(msg, pt.x, pt.y, 0x250, TE_RISING);
}

void ShowFeederIncomeAnimation(int x, int y, int z, Money cost)
{
	Point pt = RemapCoords(x,y,z);

	SetDParam(0, cost);
	AddTextEffect(STR_FEEDER, pt.x, pt.y, 0x250, TE_RISING);
}

TextEffectID ShowFillingPercent(int x, int y, int z, uint8 percent, StringID string)
{
	Point pt = RemapCoords(x, y, z);

	assert(string != STR_NULL);

	SetDParam(0, percent);
	return AddTextEffect(string, pt.x, pt.y, 0xFFFF, TE_STATIC);
}

void UpdateFillingPercent(TextEffectID te_id, uint8 percent, StringID string)
{
	assert(string != STR_NULL);

	SetDParam(0, percent);
	UpdateTextEffect(te_id, string);
}

void HideFillingPercent(TextEffectID te_id)
{
	if (te_id != INVALID_TE_ID) RemoveTextEffect(te_id);
}

static const Widget _tooltips_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   199,     0,    31, 0x0, STR_NULL},
{   WIDGETS_END},
};


static void TooltipsWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			uint arg;
			GfxFillRect(0, 0, w->width - 1, w->height - 1, 0);
			GfxFillRect(1, 1, w->width - 2, w->height - 2, 0x44);

			for (arg = 0; arg < WP(w, tooltips_d).paramcount; arg++) {
				SetDParam(arg, WP(w, tooltips_d).params[arg]);
			}
			DrawStringMultiCenter((w->width >> 1), (w->height >> 1) - 5, WP(w, tooltips_d).string_id, w->width - 2);
			break;
		}

		case WE_MOUSELOOP:
			/* We can show tooltips while dragging tools. These are shown as long as
			 * we are dragging the tool. Normal tooltips work with rmb */
			if (WP(w, tooltips_d).paramcount == 0 ) {
				if (!_right_button_down) DeleteWindow(w);
			} else {
				if (!_left_button_down) DeleteWindow(w);
			}

			break;
	}
}

/** Shows a tooltip
 * @param str String to be displayed
 * @param paramcount number of params to deal with
 * @param params (optional) up to 5 pieces of additional information that may be
 * added to a tooltip; currently only supports parameters of {NUM} (integer) */
void GuiShowTooltipsWithArgs(StringID str, uint paramcount, const uint64 params[])
{
	char buffer[512];
	BoundingRect br;
	Window *w;
	uint i;
	int x, y;

	DeleteWindowById(WC_TOOLTIPS, 0);

	/* We only show measurement tooltips with patch setting on */
	if (str == STR_NULL || (paramcount != 0 && !_patches.measure_tooltip)) return;

	for (i = 0; i != paramcount; i++) SetDParam(i, params[i]);
	GetString(buffer, str, lastof(buffer));

	br = GetStringBoundingBox(buffer);
	br.width += 6; br.height += 4; // increase slightly to have some space around the box

	/* Cut tooltip length to 200 pixels max, wrap to new line if longer */
	if (br.width > 200) {
		br.height += ((br.width - 4) / 176) * 10;
		br.width = 200;
	}

	/* Correctly position the tooltip position, watch out for window and cursor size
	 * Clamp value to below main toolbar and above statusbar. If tooltip would
	 * go below window, flip it so it is shown above the cursor */
	y = Clamp(_cursor.pos.y + _cursor.size.y + _cursor.offs.y + 5, 22, _screen.height - 12);
	if (y + br.height > _screen.height - 12) y = _cursor.pos.y + _cursor.offs.y - br.height - 5;
	x = Clamp(_cursor.pos.x - (br.width >> 1), 0, _screen.width - br.width);

	w = AllocateWindow(x, y, br.width, br.height, TooltipsWndProc, WC_TOOLTIPS, _tooltips_widgets);

	WP(w, tooltips_d).string_id = str;
	assert(sizeof(WP(w, tooltips_d).params[0]) == sizeof(params[0]));
	memcpy(WP(w, tooltips_d).params, params, sizeof(WP(w, tooltips_d).params[0]) * paramcount);
	WP(w, tooltips_d).paramcount = paramcount;

	w->flags4 &= ~WF_WHITE_BORDER_MASK; // remove white-border from tooltip
	w->widget[0].right = br.width;
	w->widget[0].bottom = br.height;
}


static void DrawStationCoverageText(const AcceptedCargo accepts,
	int str_x, int str_y, StationCoverageType sct)
{
	char *b = _userstring;
	bool first = true;

	b = InlineString(b, STR_000D_ACCEPTS);

	for (CargoID i = 0; i < NUM_CARGO; i++) {
		if (b >= lastof(_userstring) - 5) break;
		switch (sct) {
			case SCT_PASSENGERS_ONLY: if (!IsCargoInClass(i, CC_PASSENGERS)) continue; break;
			case SCT_NON_PASSENGERS_ONLY: if (IsCargoInClass(i, CC_PASSENGERS)) continue; break;
			case SCT_ALL: break;
			default: NOT_REACHED();
		}
		if (accepts[i] >= 8) {
			if (first) {
				first = false;
			} else {
				/* Add a comma if this is not the first item */
				*b++ = ',';
				*b++ = ' ';
			}
			b = InlineString(b, GetCargo(i)->name);
		}
	}

	/* If first is still true then no cargo is accepted */
	if (first) b = InlineString(b, STR_00D0_NOTHING);

	*b = '\0';
	DrawStringMultiLine(str_x, str_y, STR_SPEC_USERSTRING, 144);
}

void DrawStationCoverageAreaText(int sx, int sy, StationCoverageType sct, int rad)
{
	TileIndex tile = TileVirtXY(_thd.pos.x, _thd.pos.y);
	AcceptedCargo accepts;
	if (tile < MapSize()) {
		GetAcceptanceAroundTiles(accepts, tile, _thd.size.x / TILE_SIZE, _thd.size.y / TILE_SIZE , rad);
		DrawStationCoverageText(accepts, sx, sy, sct);
	}
}

void CheckRedrawStationCoverage(const Window *w)
{
	if (_thd.dirty & 1) {
		_thd.dirty &= ~1;
		SetWindowDirty(w);
	}
}

void SetVScrollCount(Window *w, int num)
{
	w->vscroll.count = num;
	num -= w->vscroll.cap;
	if (num < 0) num = 0;
	if (num < w->vscroll.pos) w->vscroll.pos = num;
}

void SetVScroll2Count(Window *w, int num)
{
	w->vscroll2.count = num;
	num -= w->vscroll2.cap;
	if (num < 0) num = 0;
	if (num < w->vscroll2.pos) w->vscroll2.pos = num;
}

void SetHScrollCount(Window *w, int num)
{
	w->hscroll.count = num;
	num -= w->hscroll.cap;
	if (num < 0) num = 0;
	if (num < w->hscroll.pos) w->hscroll.pos = num;
}

/* Delete a character at the caret position in a text buf.
 * If backspace is set, delete the character before the caret,
 * else delete the character after it. */
static void DelChar(Textbuf *tb, bool backspace)
{
	WChar c;
	uint width;
	size_t len;
	char *s = tb->buf + tb->caretpos;

	if (backspace) s = Utf8PrevChar(s);

	len = Utf8Decode(&c, s);
	width = GetCharacterWidth(FS_NORMAL, c);

	tb->width  -= width;
	if (backspace) {
		tb->caretpos   -= len;
		tb->caretxoffs -= width;
	}

	/* Move the remaining characters over the marker */
	memmove(s, s + len, tb->length - (s - tb->buf) - len + 1);
	tb->length -= len;
}

/**
 * Delete a character from a textbuffer, either with 'Delete' or 'Backspace'
 * The character is delete from the position the caret is at
 * @param tb Textbuf type to be changed
 * @param delmode Type of deletion, either WKC_BACKSPACE or WKC_DELETE
 * @return Return true on successful change of Textbuf, or false otherwise
 */
bool DeleteTextBufferChar(Textbuf *tb, int delmode)
{
	if (delmode == WKC_BACKSPACE && tb->caretpos != 0) {
		DelChar(tb, true);
		return true;
	} else if (delmode == WKC_DELETE && tb->caretpos < tb->length) {
		DelChar(tb, false);
		return true;
	}

	return false;
}

/**
 * Delete every character in the textbuffer
 * @param tb Textbuf buffer to be emptied
 */
void DeleteTextBufferAll(Textbuf *tb)
{
	memset(tb->buf, 0, tb->maxlength);
	tb->length = tb->width = 0;
	tb->caretpos = tb->caretxoffs = 0;
}

/**
 * Insert a character to a textbuffer. If maxwidth of the Textbuf is zero,
 * we don't care about the visual-length but only about the physical
 * length of the string
 * @param tb Textbuf type to be changed
 * @param key Character to be inserted
 * @return Return true on successful change of Textbuf, or false otherwise
 */
bool InsertTextBufferChar(Textbuf *tb, WChar key)
{
	const byte charwidth = GetCharacterWidth(FS_NORMAL, key);
	size_t len = Utf8CharLen(key);
	if (tb->length < (tb->maxlength - len) && (tb->maxwidth == 0 || tb->width + charwidth <= tb->maxwidth)) {
		memmove(tb->buf + tb->caretpos + len, tb->buf + tb->caretpos, tb->length - tb->caretpos + 1);
		Utf8Encode(tb->buf + tb->caretpos, key);
		tb->length += len;
		tb->width  += charwidth;

		tb->caretpos   += len;
		tb->caretxoffs += charwidth;
		return true;
	}
	return false;
}

/**
 * Handle text navigation with arrow keys left/right.
 * This defines where the caret will blink and the next characer interaction will occur
 * @param tb Textbuf type where navigation occurs
 * @param navmode Direction in which navigation occurs WKC_LEFT, WKC_RIGHT, WKC_END, WKC_HOME
 * @return Return true on successful change of Textbuf, or false otherwise
 */
bool MoveTextBufferPos(Textbuf *tb, int navmode)
{
	switch (navmode) {
	case WKC_LEFT:
		if (tb->caretpos != 0) {
			WChar c;
			const char *s = Utf8PrevChar(tb->buf + tb->caretpos);
			Utf8Decode(&c, s);
			tb->caretpos    = s - tb->buf; // -= (tb->buf + tb->caretpos - s)
			tb->caretxoffs -= GetCharacterWidth(FS_NORMAL, c);

			return true;
		}
		break;
	case WKC_RIGHT:
		if (tb->caretpos < tb->length) {
			WChar c;

			tb->caretpos   += Utf8Decode(&c, tb->buf + tb->caretpos);
			tb->caretxoffs += GetCharacterWidth(FS_NORMAL, c);

			return true;
		}
		break;
	case WKC_HOME:
		tb->caretpos = 0;
		tb->caretxoffs = 0;
		return true;
	case WKC_END:
		tb->caretpos = tb->length;
		tb->caretxoffs = tb->width;
		return true;
	}

	return false;
}

/**
 * Initialize the textbuffer by supplying it the buffer to write into
 * and the maximum length of this buffer
 * @param tb Textbuf type which is getting initialized
 * @param buf the buffer that will be holding the data for input
 * @param maxlength maximum length in characters of this buffer
 * @param maxwidth maximum length in pixels of this buffer. If reached, buffer
 * cannot grow, even if maxlength would allow because there is space. A length
 * of zero '0' means the buffer is only restricted by maxlength */
void InitializeTextBuffer(Textbuf *tb, const char *buf, uint16 maxlength, uint16 maxwidth)
{
	tb->buf = (char*)buf;
	tb->maxlength = maxlength;
	tb->maxwidth  = maxwidth;
	tb->caret = true;
	UpdateTextBufferSize(tb);
}

/**
 * Update Textbuf type with its actual physical character and screenlength
 * Get the count of characters in the string as well as the width in pixels.
 * Useful when copying in a larger amount of text at once
 * @param tb Textbuf type which length is calculated
 */
void UpdateTextBufferSize(Textbuf *tb)
{
	const char *buf = tb->buf;
	WChar c = Utf8Consume(&buf);

	tb->width = 0;
	tb->length = 0;

	for (; c != '\0' && tb->length < (tb->maxlength - 1); c = Utf8Consume(&buf)) {
		tb->width += GetCharacterWidth(FS_NORMAL, c);
		tb->length += Utf8CharLen(c);
	}

	tb->caretpos = tb->length;
	tb->caretxoffs = tb->width;
}

int HandleEditBoxKey(Window *w, querystr_d *string, int wid, WindowEvent *e)
{
	e->we.keypress.cont = false;

	switch (e->we.keypress.keycode) {
	case WKC_ESC: return 2;
	case WKC_RETURN: case WKC_NUM_ENTER: return 1;
	case (WKC_CTRL | 'V'):
		if (InsertTextBufferClipboard(&string->text))
			w->InvalidateWidget(wid);
		break;
	case (WKC_CTRL | 'U'):
		DeleteTextBufferAll(&string->text);
		w->InvalidateWidget(wid);
		break;
	case WKC_BACKSPACE: case WKC_DELETE:
		if (DeleteTextBufferChar(&string->text, e->we.keypress.keycode))
			w->InvalidateWidget(wid);
		break;
	case WKC_LEFT: case WKC_RIGHT: case WKC_END: case WKC_HOME:
		if (MoveTextBufferPos(&string->text, e->we.keypress.keycode))
			w->InvalidateWidget(wid);
		break;
	default:
		if (IsValidChar(e->we.keypress.key, string->afilter)) {
			if (InsertTextBufferChar(&string->text, e->we.keypress.key)) {
				w->InvalidateWidget(wid);
			}
		} else { // key wasn't caught. Continue only if standard entry specified
			e->we.keypress.cont = (string->afilter == CS_ALPHANUMERAL);
		}
	}

	return 0;
}

bool HandleCaret(Textbuf *tb)
{
	/* caret changed? */
	bool b = !!(_caret_timer & 0x20);

	if (b != tb->caret) {
		tb->caret = b;
		return true;
	}
	return false;
}

void HandleEditBox(Window *w, querystr_d *string, int wid)
{
	if (HandleCaret(&string->text)) w->InvalidateWidget(wid);
}

void DrawEditBox(Window *w, querystr_d *string, int wid)
{
	DrawPixelInfo dpi, *old_dpi;
	int delta;
	const Widget *wi = &w->widget[wid];
	const Textbuf *tb = &string->text;

	GfxFillRect(wi->left + 1, wi->top + 1, wi->right - 1, wi->bottom - 1, 215);

	/* Limit the drawing of the string inside the widget boundaries */
	if (!FillDrawPixelInfo(&dpi,
	      wi->left + 4,
	      wi->top + 1,
	      wi->right - wi->left - 4,
	      wi->bottom - wi->top - 1)
	) return;

	old_dpi = _cur_dpi;
	_cur_dpi = &dpi;

	/* We will take the current widget length as maximum width, with a small
	 * space reserved at the end for the caret to show */
	delta = (wi->right - wi->left) - tb->width - 10;
	if (delta > 0) delta = 0;

	if (tb->caretxoffs + delta < 0) delta = -tb->caretxoffs;

	DoDrawString(tb->buf, delta, 0, TC_YELLOW);
	if (tb->caret) DoDrawString("_", tb->caretxoffs + delta, 0, TC_WHITE);

	_cur_dpi = old_dpi;
}

enum QueryStringWidgets {
	QUERY_STR_WIDGET_TEXT = 3,
	QUERY_STR_WIDGET_CANCEL,
	QUERY_STR_WIDGET_OK
};


static void QueryStringWndProc(Window *w, WindowEvent *e)
{
	querystr_d *qs = &WP(w, querystr_d);

	switch (e->event) {
		case WE_CREATE:
			SetBit(_no_scroll, SCROLL_EDIT);
			break;

		case WE_PAINT:
			SetDParam(0, qs->caption);
			DrawWindowWidgets(w);

			DrawEditBox(w, qs, QUERY_STR_WIDGET_TEXT);
			break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case QUERY_STR_WIDGET_OK:
		press_ok:;
					if (qs->orig == NULL || strcmp(qs->text.buf, qs->orig) != 0) {
						Window *parent = w->parent;
						qs->handled = true;

						/* If the parent is NULL, the editbox is handled by general function
						 * HandleOnEditText */
						if (parent != NULL) {
							WindowEvent e;
							e.event = WE_ON_EDIT_TEXT;
							e.we.edittext.str = qs->text.buf;
							parent->wndproc(parent, &e);
						} else {
							HandleOnEditText(qs->text.buf);
						}
					}
					/* Fallthrough */
				case QUERY_STR_WIDGET_CANCEL:
					DeleteWindow(w);
					break;
			}
			break;

		case WE_MOUSELOOP:
			HandleEditBox(w, qs, QUERY_STR_WIDGET_TEXT);
			break;

		case WE_KEYPRESS:
			switch (HandleEditBoxKey(w, qs, QUERY_STR_WIDGET_TEXT, e)) {
				case 1: goto press_ok; // Enter pressed, confirms change
				case 2: DeleteWindow(w); break; // ESC pressed, closes window, abandons changes
			}
			break;

		case WE_DESTROY: // Call cancellation of query, if we have not handled it before
			if (!qs->handled && w->parent != NULL) {
				WindowEvent e;
				Window *parent = w->parent;

				qs->handled = true;
				e.event = WE_ON_EDIT_TEXT_CANCEL;
				parent->wndproc(parent, &e);
			}
			ClrBit(_no_scroll, SCROLL_EDIT);
			break;
		}
}

static const Widget _query_string_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,        STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   259,     0,    13, STR_012D,        STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   259,    14,    29, 0x0,             STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,   257,    16,    27, 0x0,             STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,   129,    30,    41, STR_012E_CANCEL, STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   130,   259,    30,    41, STR_012F_OK,     STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _query_string_desc = {
	190, 219, 260, 42, 260, 42,
	WC_QUERY_STRING, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_query_string_widgets,
	QueryStringWndProc
};

static char _edit_str_buf[64];

/** Show a query popup window with a textbox in it.
 * @param str StringID for the text shown in the textbox
 * @param caption StringID of text shown in caption of querywindow
 * @param maxlen maximum length in characters allowed. If bit 12 is set we
 * will not check the resulting string against to original string to return success
 * @param maxwidth maximum width in pixels allowed
 * @param parent pointer to a Window that will handle the events (ok/cancel) of this
 * window. If NULL, results are handled by global function HandleOnEditText
 * @param afilter filters out unwanted character input */
void ShowQueryString(StringID str, StringID caption, uint maxlen, uint maxwidth, Window *parent, CharSetFilter afilter)
{
	static char orig_str_buf[lengthof(_edit_str_buf)];
	Window *w;
	uint realmaxlen = maxlen & ~0x1000;

	assert(realmaxlen < lengthof(_edit_str_buf));

	DeleteWindowById(WC_QUERY_STRING, 0);
	DeleteWindowById(WC_SAVELOAD, 0);

	w = AllocateWindowDesc(&_query_string_desc);
	w->parent = parent;

	GetString(_edit_str_buf, str, lastof(_edit_str_buf));
	_edit_str_buf[realmaxlen - 1] = '\0';

	if (maxlen & 0x1000) {
		WP(w, querystr_d).orig = NULL;
	} else {
		strecpy(orig_str_buf, _edit_str_buf, lastof(orig_str_buf));
		WP(w, querystr_d).orig = orig_str_buf;
	}

	w->LowerWidget(QUERY_STR_WIDGET_TEXT);
	WP(w, querystr_d).caption = caption;
	WP(w, querystr_d).afilter = afilter;
	InitializeTextBuffer(&WP(w, querystr_d).text, _edit_str_buf, realmaxlen, maxwidth);
}


enum QueryWidgets {
	QUERY_WIDGET_CAPTION = 1,
	QUERY_WIDGET_NO = 3,
	QUERY_WIDGET_YES
};


struct query_d {
	void (*proc)(Window*, bool); ///< callback function executed on closing of popup. Window* points to parent, bool is true if 'yes' clicked, false otherwise
	uint64 params[10];           ///< local copy of _decode_parameters
	StringID message;            ///< message shown for query window
	bool calledback;             ///< has callback been executed already (internal usage for WE_DESTROY event)
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(query_d));


static void QueryWndProc(Window *w, WindowEvent *e)
{
	query_d *q = &WP(w, query_d);

	switch (e->event) {
		case WE_PAINT:
			CopyInDParam(0, q->params, lengthof(q->params));
			DrawWindowWidgets(w);
			CopyInDParam(0, q->params, lengthof(q->params));

			DrawStringMultiCenter(w->width / 2, (w->height / 2) - 10, q->message, w->width - 2);
			break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case QUERY_WIDGET_YES:
					q->calledback = true;
					if (q->proc != NULL) q->proc(w->parent, true);
					/* Fallthrough */
				case QUERY_WIDGET_NO:
					DeleteWindow(w);
					break;
				}
			break;

		case WE_KEYPRESS: // ESC closes the window, Enter confirms the action
			switch (e->we.keypress.keycode) {
				case WKC_RETURN:
				case WKC_NUM_ENTER:
					q->calledback = true;
					if (q->proc != NULL) q->proc(w->parent, true);
					/* Fallthrough */
				case WKC_ESC:
					e->we.keypress.cont = false;
					DeleteWindow(w);
					break;
			}
			break;

		case WE_DESTROY: // Call callback function (if any) on window close if not yet called
			if (!q->calledback && q->proc != NULL) {
				q->calledback = true;
				q->proc(w->parent, false);
			}
			break;
	}
}


static const Widget _query_widgets[] = {
{  WWT_CLOSEBOX, RESIZE_NONE,  4,   0,  10,   0,  13, STR_00C5,        STR_018B_CLOSE_WINDOW},
{   WWT_CAPTION, RESIZE_NONE,  4,  11, 209,   0,  13, STR_NULL,        STR_NULL},
{     WWT_PANEL, RESIZE_NONE,  4,   0, 209,  14,  81, 0x0, /*OVERRIDE*/STR_NULL},
{WWT_PUSHTXTBTN, RESIZE_NONE,  3,  20,  90,  62,  73, STR_00C9_NO,     STR_NULL},
{WWT_PUSHTXTBTN, RESIZE_NONE,  3, 120, 190,  62,  73, STR_00C8_YES,    STR_NULL},
{   WIDGETS_END },
};

static const WindowDesc _query_desc = {
	WDP_CENTER, WDP_CENTER, 210, 82, 210, 82,
	WC_CONFIRM_POPUP_QUERY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_UNCLICK_BUTTONS | WDF_DEF_WIDGET | WDF_MODAL,
	_query_widgets,
	QueryWndProc
};

/** Show a modal confirmation window with standard 'yes' and 'no' buttons
 * The window is aligned to the centre of its parent.
 * NOTE: You cannot use BindCString as parameter for this window!
 * @param caption string shown as window caption
 * @param message string that will be shown for the window
 * @param parent pointer to parent window, if this pointer is NULL the parent becomes
 * the main window WC_MAIN_WINDOW
 * @param callback callback function pointer to set in the window descriptor*/
void ShowQuery(StringID caption, StringID message, Window *parent, void (*callback)(Window*, bool))
{
	Window *w = AllocateWindowDesc(&_query_desc);
	if (w == NULL) return;

	if (parent == NULL) parent = FindWindowById(WC_MAIN_WINDOW, 0);
	w->parent = parent;
	w->left = parent->left + (parent->width / 2) - (w->width / 2);
	w->top = parent->top + (parent->height / 2) - (w->height / 2);

	/* Create a backup of the variadic arguments to strings because it will be
	 * overridden pretty often. We will copy these back for drawing */
	CopyOutDParam(WP(w, query_d).params, 0, lengthof(WP(w, query_d).params));
	w->widget[QUERY_WIDGET_CAPTION].data = caption;
	WP(w, query_d).message    = message;
	WP(w, query_d).proc       = callback;
	WP(w, query_d).calledback = false;
}


static const Widget _load_dialog_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,         STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   256,     0,    13, STR_NULL,         STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,   127,    14,    25, STR_SORT_BY_NAME, STR_SORT_ORDER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   128,   256,    14,    25, STR_SORT_BY_DATE, STR_SORT_ORDER_TIP},
{      WWT_PANEL,  RESIZE_RIGHT,    14,     0,   256,    26,    47, 0x0,              STR_NULL},
{      WWT_PANEL,     RESIZE_RB,    14,     0,   256,    48,   153, 0x0,              STR_NULL},
{ WWT_PUSHIMGBTN,     RESIZE_LR,    14,   245,   256,    48,    59, SPR_HOUSE_ICON,   STR_SAVELOAD_HOME_BUTTON},
{      WWT_INSET,     RESIZE_RB,    14,     2,   243,    50,   151, 0x0,              STR_400A_LIST_OF_DRIVES_DIRECTORIES},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   245,   256,    60,   141, 0x0,              STR_0190_SCROLL_BAR_SCROLLS_LIST},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   245,   256,   142,   153, 0x0,              STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _save_dialog_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,         STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   256,     0,    13, STR_NULL,         STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,   127,    14,    25, STR_SORT_BY_NAME, STR_SORT_ORDER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   128,   256,    14,    25, STR_SORT_BY_DATE, STR_SORT_ORDER_TIP},
{      WWT_PANEL,  RESIZE_RIGHT,    14,     0,   256,    26,    47, 0x0,              STR_NULL},
{      WWT_PANEL,     RESIZE_RB,    14,     0,   256,    48,   151, 0x0,              STR_NULL},
{ WWT_PUSHIMGBTN,     RESIZE_LR,    14,   245,   256,    48,    59, SPR_HOUSE_ICON,   STR_SAVELOAD_HOME_BUTTON},
{      WWT_INSET,     RESIZE_RB,    14,     2,   243,    50,   150, 0x0,              STR_400A_LIST_OF_DRIVES_DIRECTORIES},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   245,   256,    60,   151, 0x0,              STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,    RESIZE_RTB,    14,     0,   256,   152,   167, 0x0,              STR_NULL},
{      WWT_PANEL,    RESIZE_RTB,    14,     2,   254,   154,   165, 0x0,              STR_400B_CURRENTLY_SELECTED_NAME},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   127,   168,   179, STR_4003_DELETE,  STR_400C_DELETE_THE_CURRENTLY_SELECTED},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   128,   244,   168,   179, STR_4002_SAVE,    STR_400D_SAVE_THE_CURRENT_GAME_USING},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   245,   256,   168,   179, 0x0,              STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

/* Colors for fios types */
const TextColour _fios_colors[] = {
	TC_LIGHT_BLUE, TC_DARK_GREEN,  TC_DARK_GREEN, TC_ORANGE, TC_LIGHT_BROWN,
	TC_ORANGE,     TC_LIGHT_BROWN, TC_ORANGE,     TC_ORANGE, TC_YELLOW
};

void BuildFileList()
{
	_fios_path_changed = true;
	FiosFreeSavegameList();

	switch (_saveload_mode) {
		case SLD_NEW_GAME:
		case SLD_LOAD_SCENARIO:
		case SLD_SAVE_SCENARIO:
			_fios_list = FiosGetScenarioList(_saveload_mode); break;
		case SLD_LOAD_HEIGHTMAP:
			_fios_list = FiosGetHeightmapList(_saveload_mode); break;

		default: _fios_list = FiosGetSavegameList(_saveload_mode); break;
	}
}

static void DrawFiosTexts(uint maxw)
{
	static const char *path = NULL;
	static StringID str = STR_4006_UNABLE_TO_READ_DRIVE;
	static uint32 tot = 0;

	if (_fios_path_changed) {
		str = FiosGetDescText(&path, &tot);
		_fios_path_changed = false;
	}

	if (str != STR_4006_UNABLE_TO_READ_DRIVE) SetDParam(0, tot);
	DrawString(2, 37, str, TC_FROMSTRING);
	DoDrawStringTruncated(path, 2, 27, TC_BLACK, maxw);
}

static void MakeSortedSaveGameList()
{
	uint sort_start = 0;
	uint sort_end = 0;
	uint s_amount;
	int i;

	/* Directories are always above the files (FIOS_TYPE_DIR)
	 * Drives (A:\ (windows only) are always under the files (FIOS_TYPE_DRIVE)
	 * Only sort savegames/scenarios, not directories
	 */
	for (i = 0; i < _fios_num; i++) {
		switch (_fios_list[i].type) {
			case FIOS_TYPE_DIR:    sort_start++; break;
			case FIOS_TYPE_PARENT: sort_start++; break;
			case FIOS_TYPE_DRIVE:  sort_end++;   break;
		}
	}

	s_amount = _fios_num - sort_start - sort_end;
	if (s_amount > 0)
		qsort(_fios_list + sort_start, s_amount, sizeof(FiosItem), compare_FiosItems);
}

static void GenerateFileName()
{
	/* Check if we are not a specatator who wants to generate a name..
	    Let's use the name of player #0 for now. */
	const Player *p = GetPlayer(IsValidPlayer(_local_player) ? _local_player : PLAYER_FIRST);

	SetDParam(0, p->index);
	SetDParam(1, _date);
	GetString(_edit_str_buf, STR_4004, lastof(_edit_str_buf));
	SanitizeFilename(_edit_str_buf);
}

extern void StartupEngines();

static void SaveLoadDlgWndProc(Window *w, WindowEvent *e)
{
	static FiosItem o_dir;

	switch (e->event) {
	case WE_CREATE: // Set up OPENTTD button
		w->vscroll.cap = 10;
		w->resize.step_width = 2;
		w->resize.step_height = 10;

		o_dir.type = FIOS_TYPE_DIRECT;
		switch (_saveload_mode) {
			case SLD_SAVE_GAME:
			case SLD_LOAD_GAME:
				FioGetDirectory(o_dir.name, lengthof(o_dir.name), SAVE_DIR);
				break;

			case SLD_SAVE_SCENARIO:
			case SLD_LOAD_SCENARIO:
				FioGetDirectory(o_dir.name, lengthof(o_dir.name), SCENARIO_DIR);
				break;

			case SLD_LOAD_HEIGHTMAP:
				FioGetDirectory(o_dir.name, lengthof(o_dir.name), HEIGHTMAP_DIR);
				break;

			default:
				ttd_strlcpy(o_dir.name, _personal_dir, lengthof(o_dir.name));
		}
		break;

	case WE_PAINT: {
		int pos;
		int y;

		SetVScrollCount(w, _fios_num);
		DrawWindowWidgets(w);
		DrawFiosTexts(w->width);

		if (_savegame_sort_dirty) {
			_savegame_sort_dirty = false;
			MakeSortedSaveGameList();
		}

		GfxFillRect(w->widget[7].left + 1, w->widget[7].top + 1, w->widget[7].right, w->widget[7].bottom, 0xD7);
		DoDrawString(
			_savegame_sort_order & SORT_DESCENDING ? DOWNARROW : UPARROW,
			_savegame_sort_order & SORT_BY_NAME ? w->widget[2].right - 9 : w->widget[3].right - 9,
			15, TC_BLACK
		);

		y = w->widget[7].top + 1;
		for (pos = w->vscroll.pos; pos < _fios_num; pos++) {
			const FiosItem *item = _fios_list + pos;

			DoDrawStringTruncated(item->title, 4, y, _fios_colors[item->type], w->width - 18);
			y += 10;
			if (y >= w->vscroll.cap * 10 + w->widget[7].top + 1) break;
		}

		if (_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO) {
			DrawEditBox(w, &WP(w, querystr_d), 10);
		}
		break;
	}

	case WE_CLICK:
		switch (e->we.click.widget) {
		case 2: // Sort save names by name
			_savegame_sort_order = (_savegame_sort_order == SORT_BY_NAME) ?
				SORT_BY_NAME | SORT_DESCENDING : SORT_BY_NAME;
			_savegame_sort_dirty = true;
			SetWindowDirty(w);
			break;

		case 3: // Sort save names by date
			_savegame_sort_order = (_savegame_sort_order == SORT_BY_DATE) ?
				SORT_BY_DATE | SORT_DESCENDING : SORT_BY_DATE;
			_savegame_sort_dirty = true;
			SetWindowDirty(w);
			break;

		case 6: // OpenTTD 'button', jumps to OpenTTD directory
			FiosBrowseTo(&o_dir);
			SetWindowDirty(w);
			BuildFileList();
			break;

		case 7: { // Click the listbox
			int y = (e->we.click.pt.y - w->widget[e->we.click.widget].top - 1) / 10;
			char *name;
			const FiosItem *file;

			if (y < 0 || (y += w->vscroll.pos) >= w->vscroll.count) return;

			file = _fios_list + y;

			name = FiosBrowseTo(file);
			if (name != NULL) {
				if (_saveload_mode == SLD_LOAD_GAME || _saveload_mode == SLD_LOAD_SCENARIO) {
					_switch_mode = (_game_mode == GM_EDITOR) ? SM_LOAD_SCENARIO : SM_LOAD;

					SetFiosType(file->type);
					ttd_strlcpy(_file_to_saveload.name, name, sizeof(_file_to_saveload.name));
					ttd_strlcpy(_file_to_saveload.title, file->title, sizeof(_file_to_saveload.title));

					DeleteWindow(w);
				} else if (_saveload_mode == SLD_LOAD_HEIGHTMAP) {
					SetFiosType(file->type);
					ttd_strlcpy(_file_to_saveload.name, name, sizeof(_file_to_saveload.name));
					ttd_strlcpy(_file_to_saveload.title, file->title, sizeof(_file_to_saveload.title));

					DeleteWindow(w);
					ShowHeightmapLoad();
				} else {
					/* SLD_SAVE_GAME, SLD_SAVE_SCENARIO copy clicked name to editbox */
					ttd_strlcpy(WP(w, querystr_d).text.buf, file->title, WP(w, querystr_d).text.maxlength);
					UpdateTextBufferSize(&WP(w, querystr_d).text);
					w->InvalidateWidget(10);
				}
			} else {
				/* Changed directory, need repaint. */
				SetWindowDirty(w);
				BuildFileList();
			}
			break;
		}

		case 11: case 12: // Delete, Save game
			break;
		}
		break;
	case WE_MOUSELOOP:
		if (_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO) {
			HandleEditBox(w, &WP(w, querystr_d), 10);
		}
		break;
	case WE_KEYPRESS:
		if (e->we.keypress.keycode == WKC_ESC) {
			DeleteWindow(w);
			return;
		}

		if (_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO) {
			if (HandleEditBoxKey(w, &WP(w, querystr_d), 10, e) == 1) // Press Enter
					HandleButtonClick(w, 12);
		}
		break;
	case WE_TIMEOUT:
		/* This test protects against using widgets 11 and 12 which are only available
		 * in those two saveload mode  */
		if (!(_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO)) break;

		if (w->IsWidgetLowered(11)) { // Delete button clicked
			if (!FiosDelete(WP(w, querystr_d).text.buf)) {
				ShowErrorMessage(INVALID_STRING_ID, STR_4008_UNABLE_TO_DELETE_FILE, 0, 0);
			} else {
				BuildFileList();
				/* Reset file name to current date on successful delete */
				if (_saveload_mode == SLD_SAVE_GAME) GenerateFileName();
			}

			UpdateTextBufferSize(&WP(w, querystr_d).text);
			SetWindowDirty(w);
		} else if (w->IsWidgetLowered(12)) { // Save button clicked
			_switch_mode = SM_SAVE;
			FiosMakeSavegameName(_file_to_saveload.name, WP(w, querystr_d).text.buf, sizeof(_file_to_saveload.name));

			/* In the editor set up the vehicle engines correctly (date might have changed) */
			if (_game_mode == GM_EDITOR) StartupEngines();
		}
		break;
	case WE_DESTROY:
		/* pause is only used in single-player, non-editor mode, non menu mode */
		if (!_networking && _game_mode != GM_EDITOR && _game_mode != GM_MENU) {
			DoCommandP(0, 0, 0, NULL, CMD_PAUSE);
		}
		FiosFreeSavegameList();
		ClrBit(_no_scroll, SCROLL_SAVE);
		break;
	case WE_RESIZE: {
		/* Widget 2 and 3 have to go with halve speed, make it so obiwan */
		uint diff = e->we.sizing.diff.x / 2;
		w->widget[2].right += diff;
		w->widget[3].left  += diff;
		w->widget[3].right += e->we.sizing.diff.x;

		/* Same for widget 11 and 12 in save-dialog */
		if (_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO) {
			w->widget[11].right += diff;
			w->widget[12].left  += diff;
			w->widget[12].right += e->we.sizing.diff.x;
		}

		w->vscroll.cap += e->we.sizing.diff.y / 10;
		} break;
	}
}

static const WindowDesc _load_dialog_desc = {
	WDP_CENTER, WDP_CENTER, 257, 154, 257, 294,
	WC_SAVELOAD, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_load_dialog_widgets,
	SaveLoadDlgWndProc,
};

static const WindowDesc _save_dialog_desc = {
	WDP_CENTER, WDP_CENTER, 257, 180, 257, 320,
	WC_SAVELOAD, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_save_dialog_widgets,
	SaveLoadDlgWndProc,
};

void ShowSaveLoadDialog(int mode)
{
	static const StringID saveload_captions[] = {
		STR_4001_LOAD_GAME,
		STR_0298_LOAD_SCENARIO,
		STR_4000_SAVE_GAME,
		STR_0299_SAVE_SCENARIO,
		STR_4011_LOAD_HEIGHTMAP,
	};

	Window *w;
	const WindowDesc *sld = &_save_dialog_desc;


	SetObjectToPlace(SPR_CURSOR_ZZZ, PAL_NONE, VHM_NONE, WC_MAIN_WINDOW, 0);
	DeleteWindowById(WC_QUERY_STRING, 0);
	DeleteWindowById(WC_SAVELOAD, 0);

	_saveload_mode = mode;
	SetBit(_no_scroll, SCROLL_SAVE);

	switch (mode) {
		case SLD_SAVE_GAME:     GenerateFileName(); break;
		case SLD_SAVE_SCENARIO: strcpy(_edit_str_buf, "UNNAMED"); break;
		default:                sld = &_load_dialog_desc; break;
	}

	assert((uint)mode < lengthof(saveload_captions));
	w = AllocateWindowDesc(sld);
	w->widget[1].data = saveload_captions[mode];
	w->LowerWidget(7);

	WP(w, querystr_d).afilter = CS_ALPHANUMERAL;
	InitializeTextBuffer(&WP(w, querystr_d).text, _edit_str_buf, lengthof(_edit_str_buf), 240);

	/* pause is only used in single-player, non-editor mode, non-menu mode. It
	 * will be unpaused in the WE_DESTROY event handler. */
	if (_game_mode != GM_MENU && !_networking && _game_mode != GM_EDITOR) {
		DoCommandP(0, 1, 0, NULL, CMD_PAUSE);
	}

	BuildFileList();

	ResetObjectToPlace();
}

void RedrawAutosave()
{
	SetWindowDirty(FindWindowById(WC_STATUS_BAR, 0));
}

void SetFiosType(const byte fiostype)
{
	switch (fiostype) {
		case FIOS_TYPE_FILE:
		case FIOS_TYPE_SCENARIO:
			_file_to_saveload.mode = SL_LOAD;
			break;

		case FIOS_TYPE_OLDFILE:
		case FIOS_TYPE_OLD_SCENARIO:
			_file_to_saveload.mode = SL_OLD_LOAD;
			break;

#ifdef WITH_PNG
		case FIOS_TYPE_PNG:
			_file_to_saveload.mode = SL_PNG;
			break;
#endif /* WITH_PNG */

		case FIOS_TYPE_BMP:
			_file_to_saveload.mode = SL_BMP;
			break;

		default:
			_file_to_saveload.mode = SL_INVALID;
			break;
	}
}

static int32 ClickMoneyCheat(int32 p1, int32 p2)
{
		DoCommandP(0, 10000000, 0, NULL, CMD_MONEY_CHEAT);
		return true;
}

/**
 * @param p1 player to set to
 * @param p2 is -1 or +1 (down/up)
 */
static int32 ClickChangePlayerCheat(int32 p1, int32 p2)
{
	while (IsValidPlayer((PlayerID)p1)) {
		if (_players[p1].is_active) {
			SetLocalPlayer((PlayerID)p1);

			MarkWholeScreenDirty();
			return _local_player;
		}
		p1 += p2;
	}

	return _local_player;
}

/**
 * @param p1 -1 or +1 (down/up)
 * @param p2 unused
 */
static int32 ClickChangeClimateCheat(int32 p1, int32 p2)
{
	if (p1 == -1) p1 = 3;
	if (p1 ==  4) p1 = 0;
	_opt.landscape = p1;
	ReloadNewGRFData();
	return _opt.landscape;
}

extern void EnginesMonthlyLoop();

/**
 * @param p1 unused
 * @param p2 1 (increase) or -1 (decrease)
 */
static int32 ClickChangeDateCheat(int32 p1, int32 p2)
{
	YearMonthDay ymd;
	ConvertDateToYMD(_date, &ymd);

	if ((ymd.year == MIN_YEAR && p2 == -1) || (ymd.year == MAX_YEAR && p2 == 1)) return _cur_year;

	SetDate(ConvertYMDToDate(_cur_year + p2, ymd.month, ymd.day));
	EnginesMonthlyLoop();
	SetWindowDirty(FindWindowById(WC_STATUS_BAR, 0));
	return _cur_year;
}

typedef int32 CheckButtonClick(int32, int32);

enum ce_flags_long
{
	CE_NONE = 0,
	CE_CLICK = 1 << 0,
	CE_END = 1 << 1,
};

/** Define basic enum properties */
template <> struct EnumPropsT<ce_flags_long> : MakeEnumPropsT<ce_flags_long, byte, CE_NONE, CE_END, CE_END> {};
typedef TinyEnumT<ce_flags_long> ce_flags;


struct CheatEntry {
	VarType type;          ///< type of selector
	ce_flags flags;        ///< selector flags
	StringID str;          ///< string with descriptive text
	void *variable;        ///< pointer to the variable
	bool *been_used;       ///< has this cheat been used before?
	CheckButtonClick *proc;///< procedure
	int16 min, max;        ///< range for spinbox setting
};

static const CheatEntry _cheats_ui[] = {
	{SLE_BOOL, {CE_CLICK}, STR_CHEAT_MONEY,          &_cheats.money.value,           &_cheats.money.been_used,           &ClickMoneyCheat,         0,  0},
	{SLE_UINT8, {CE_NONE}, STR_CHEAT_CHANGE_PLAYER,  &_local_player,                 &_cheats.switch_player.been_used,   &ClickChangePlayerCheat,  0, 11},
	{SLE_BOOL,  {CE_NONE}, STR_CHEAT_EXTRA_DYNAMITE, &_cheats.magic_bulldozer.value, &_cheats.magic_bulldozer.been_used, NULL,                     0,  0},
	{SLE_BOOL,  {CE_NONE}, STR_CHEAT_CROSSINGTUNNELS,&_cheats.crossing_tunnels.value,&_cheats.crossing_tunnels.been_used,NULL,                     0,  0},
	{SLE_BOOL,  {CE_NONE}, STR_CHEAT_BUILD_IN_PAUSE, &_cheats.build_in_pause.value,  &_cheats.build_in_pause.been_used,  NULL,                     0,  0},
	{SLE_BOOL,  {CE_NONE}, STR_CHEAT_NO_JETCRASH,    &_cheats.no_jetcrash.value,     &_cheats.no_jetcrash.been_used,     NULL,                     0,  0},
	{SLE_BOOL,  {CE_NONE}, STR_CHEAT_SETUP_PROD,     &_cheats.setup_prod.value,      &_cheats.setup_prod.been_used,      NULL,                     0,  0},
	{SLE_UINT8, {CE_NONE}, STR_CHEAT_SWITCH_CLIMATE, &_opt.landscape,                &_cheats.switch_climate.been_used,  &ClickChangeClimateCheat,-1,  4},
	{SLE_INT32, {CE_NONE}, STR_CHEAT_CHANGE_DATE,    &_cur_year,                     &_cheats.change_date.been_used,     &ClickChangeDateCheat,   -1,  1},
};


static const Widget _cheat_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,   STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   399,     0,    13, STR_CHEATS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   399,    14,   169, 0x0,        STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   399,    14,   169, 0x0,        STR_CHEATS_TIP},
{   WIDGETS_END},
};

static void CheatsWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		int clk = WP(w,def_d).data_1;
		int x, y;
		int i;

		DrawWindowWidgets(w);

		DrawStringMultiCenter(200, 25, STR_CHEATS_WARNING, w->width - 50);

		x = 0;
		y = 45;

		for (i = 0; i != lengthof(_cheats_ui); i++) {
			const CheatEntry *ce = &_cheats_ui[i];

			DrawSprite((*ce->been_used) ? SPR_BOX_CHECKED : SPR_BOX_EMPTY, PAL_NONE, x + 5, y + 2);

			switch (ce->type) {
			case SLE_BOOL: {
				bool on = (*(bool*)ce->variable);

				if (ce->flags & CE_CLICK) {
					DrawFrameRect(x + 20, y + 1, x + 30 + 9, y + 9, 0, (clk - (i * 2) == 1) ? FR_LOWERED : FR_NONE);
					if (i == 0) { // XXX - hack/hack for first element which is increase money. Told ya it's a mess
						SetDParam(0, 10000000);
					} else {
						SetDParam(0, false);
					}
				} else {
					DrawFrameRect(x + 20, y + 1, x + 30 + 9, y + 9, on ? 6 : 4, on ? FR_LOWERED : FR_NONE);
					SetDParam(0, on ? STR_CONFIG_PATCHES_ON : STR_CONFIG_PATCHES_OFF);
				}
			} break;
			default: {
				int32 val = (int32)ReadValue(ce->variable, ce->type);
				char buf[512];

				/* Draw [<][>] boxes for settings of an integer-type */
				DrawArrowButtons(x + 20, y, 3, clk - (i * 2), true, true);

				switch (ce->str) {
				/* Display date for change date cheat */
				case STR_CHEAT_CHANGE_DATE: SetDParam(0, _date); break;
				/* Draw colored flag for change player cheat */
				case STR_CHEAT_CHANGE_PLAYER:
					SetDParam(0, val);
					GetString(buf, STR_CHEAT_CHANGE_PLAYER, lastof(buf));
					DrawPlayerIcon(_current_player, 60 + GetStringBoundingBox(buf).width, y + 2);
					break;
				/* Set correct string for switch climate cheat */
				case STR_CHEAT_SWITCH_CLIMATE: val += STR_TEMPERATE_LANDSCAPE;
				/* Fallthrough */
				default: SetDParam(0, val);
				}
			} break;
			}

			DrawString(50, y + 1, ce->str, TC_FROMSTRING);

			y += 12;
		}
		break;
	}

	case WE_CLICK: {
			const CheatEntry *ce;
			uint btn = (e->we.click.pt.y - 46) / 12;
			int32 value, oldvalue;
			uint x = e->we.click.pt.x;

			/* not clicking a button? */
			if (!IsInsideMM(x, 20, 40) || btn >= lengthof(_cheats_ui)) break;

			ce = &_cheats_ui[btn];
			oldvalue = value = (int32)ReadValue(ce->variable, ce->type);

			*ce->been_used = true;

			switch (ce->type) {
			case SLE_BOOL:
				if (ce->flags & CE_CLICK) WP(w,def_d).data_1 = btn * 2 + 1;
				value ^= 1;
				if (ce->proc != NULL) ce->proc(value, 0);
				break;
			default: {
				/* Add a dynamic step-size to the scroller. In a maximum of
				 * 50-steps you should be able to get from min to max */
				uint16 step = ((ce->max - ce->min) / 20);
				if (step == 0) step = 1;

				/* Increase or decrease the value and clamp it to extremes */
				value += (x >= 30) ? step : -step;
				value = Clamp(value, ce->min, ce->max);

				/* take whatever the function returns */
				value = ce->proc(value, (x >= 30) ? 1 : -1);

				if (value != oldvalue) {
					WP(w,def_d).data_1 = btn * 2 + 1 + ((x >= 30) ? 1 : 0);
				}
			} break;
			}

			if (value != oldvalue) {
				WriteValue(ce->variable, ce->type, (int64)value);
				SetWindowDirty(w);
			}

			w->flags4 |= 5 << WF_TIMEOUT_SHL;

			SetWindowDirty(w);
		}
		break;
	case WE_TIMEOUT:
		WP(w,def_d).data_1 = 0;
		SetWindowDirty(w);
		break;
	}
}

static const WindowDesc _cheats_desc = {
	240, 22, 400, 170, 400, 170,
	WC_CHEATS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_cheat_widgets,
	CheatsWndProc
};


void ShowCheatWindow()
{
	DeleteWindowById(WC_CHEATS, 0);
	AllocateWindowDesc(&_cheats_desc);
}
