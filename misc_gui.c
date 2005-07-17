#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "spritecache.h"
#include "strings.h"
#include "table/sprites.h"
#include "table/strings.h"
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
#include "network.h"
#include "string.h"

#include "hal.h" // for file list

static bool _fios_path_changed;
static bool _savegame_sort_dirty;

bool _query_string_active;

typedef struct LandInfoData {
	Town *town;
	int32 costclear;
	AcceptedCargo ac;
	TileIndex tile;
	TileDesc td;
} LandInfoData;

static void LandInfoWndProc(Window *w, WindowEvent *e)
{
	LandInfoData *lid;
	StringID str;

	if (e->event == WE_PAINT) {
		int i;

		DrawWindowWidgets(w);

		lid = WP(w,void_d).data;

		SetDParam(0, lid->td.dparam[0]);
		DrawStringCentered(140, 16, lid->td.str, 13);

		SetDParam(0, STR_01A6_N_A);
		if (lid->td.owner != OWNER_NONE && lid->td.owner != OWNER_WATER)
			GetNameOfOwner(lid->td.owner, lid->tile);
		DrawStringCentered(140, 27, STR_01A7_OWNER, 0);

		str = STR_01A4_COST_TO_CLEAR_N_A;
		if (lid->costclear != CMD_ERROR) {
			SetDParam(0, lid->costclear);
			str = STR_01A5_COST_TO_CLEAR;
		}
		DrawStringCentered(140, 38, str, 0);

		snprintf(_userstring, USERSTRING_LEN, "%.4X", lid->tile);
		SetDParam(0, TileX(lid->tile));
		SetDParam(1, TileY(lid->tile));
		SetDParam(2, STR_SPEC_USERSTRING);
		DrawStringCentered(140, 49, STR_LANDINFO_COORDS, 0);

		SetDParam(0, STR_01A9_NONE);
		if (lid->town != NULL) {
			SetDParam(0, STR_TOWN);
			SetDParam(1, lid->town->index);
		}
		DrawStringCentered(140,60, STR_01A8_LOCAL_AUTHORITY, 0);

		{
			char buf[512];
			char *p = GetString(buf, STR_01CE_CARGO_ACCEPTED);
			bool found = false;

			for (i = 0; i < NUM_CARGO; ++i) {
				if (lid->ac[i] > 0) {
					// Add a comma between each item.
					if (found) { *p++ = ','; *p++ = ' '; }
					found = true;

					// If the accepted value is less than 8, show it in 1/8:ths
					if (lid->ac[i] < 8) {
						int32 argv[2] = {
							lid->ac[i],
							_cargoc.names_s[i]
						};
						p = GetStringWithArgs(p, STR_01D1_8, argv);
					} else {
						p = GetString(p, _cargoc.names_s[i]);
					}
				}
			}

			if (found)
				DrawStringMultiCenter(140, 76, BindCString(buf), 276);
		}

		if (lid->td.build_date != 0) {
			SetDParam(0,lid->td.build_date);
			DrawStringCentered(140,71, STR_BUILD_DATE, 0);
		}
	}
}

static const Widget _land_info_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,	STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   279,     0,    13, STR_01A3_LAND_AREA_INFORMATION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   279,    14,    92, 0x0,				STR_NULL},
{    WIDGETS_END},
};

static const WindowDesc _land_info_desc = {
	-1, -1, 280, 93,
	WC_LAND_INFO,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_land_info_widgets,
	LandInfoWndProc
};

static void Place_LandInfo(TileIndex tile)
{
	Player *p;
	static LandInfoData lid;
	Window *w;
	int64 old_money;

	DeleteWindowById(WC_LAND_INFO, 0);

	w = AllocateWindowDesc(&_land_info_desc);
	WP(w,void_d).data = &lid;

	lid.tile = tile;
	lid.town = ClosestTownFromTile(tile, _patches.dist_local_authority);

	if (_local_player >= MAX_PLAYERS)
		p = GetPlayer(0);
	else
		p = GetPlayer(_local_player);

	old_money = p->money64;
	p->money64 = p->player_money = 0x7fffffff;
	lid.costclear = DoCommandByTile(tile, 0, 0, 0, CMD_LANDSCAPE_CLEAR);
	p->money64 = old_money;
	UpdatePlayerMoney32(p);

	// Becuase build_date is not set yet in every TileDesc, we make sure it is empty
	lid.td.build_date = 0;

	GetAcceptedCargo(tile, lid.ac);
	GetTileDesc(tile, &lid.td);

	#if defined(_DEBUG)
		DEBUG(misc, 0) ("TILE: %#x (%i,%i)", tile, TileX(tile), TileY(tile));
		DEBUG(misc, 0) ("TILE: %d ", tile);
		DEBUG(misc, 0) ("_type_height = %#x", _m[tile].type_height);
		DEBUG(misc, 0) ("m2           = %#x", _m[tile].m2);
		DEBUG(misc, 0) ("m3           = %#x", _m[tile].m3);
		DEBUG(misc, 0) ("m4           = %#x", _m[tile].m4);
		DEBUG(misc, 0) ("m5           = %#x", _m[tile].m5);
		DEBUG(misc, 0) ("owner        = %#x", _m[tile].owner);
	#endif
}

void PlaceLandBlockInfo(void)
{
	if (_cursor.sprite == SPR_CURSOR_QUERY) {
		ResetObjectToPlace();
	} else {
		_place_proc = Place_LandInfo;
		SetObjectToPlace(SPR_CURSOR_QUERY, 1, 1, 0);
	}
}

static const char *credits[] = {
	/*************************************************************************
	 *                      maximum length of string which fits in window   -^*/
	"Original design by Chris Sawyer",
	"Original graphics by Simon Foster",
	"",
	"The OpenTTD team (in alphabetical order):",
	"  Matthijs Kooijman (blathijs) - Pathfinder-god",
	"  Bjarni Corfitzen (Bjarni) - MacOSX port, coder",
	"  Victor Fischer (Celestar) - Programming everywhere you need him to",
	"  Tamas Faragó (Darkvater) - Lead coder",
	"  Kerekes Miham (MiHaMiX) - Translator system, and Nightlies host",
	"  Owen Rudge (orudge) - Forum- and masterserver host, OS/2 port",
	"  Christoph Mallon (Tron) - Programmer, code correctness police",
	"  Patric Stout (TrueLight) - Coder, network guru, SVN- and website host",
	"",
	"Retired Developers:",
	"  Ludvig Strigeus (ludde) - OpenTTD author, main coder (0.1 - 0.3.3)",
	"  Serge Paquet (vurlix) - Assistant project manager, coder (0.1 - 0.3.3)",
	"  Dominik Scherer (dominik81) - Lead programmer, GUI expert (0.3.0 - 0.3.6)",
	"",
	"Special thanks go out to:",
	"  Josef Drexler - For his great work on TTDPatch",
	"  Marcin Grzegorczyk - For his documentation of TTD internals",
	"  Petr Baudis (pasky) - Many patches, newgrf support",
	"  Stefan Meißner (sign_de) - For his work on the console",
	"  Simon Sasburg (HackyKid) - Many bugfixes he has blessed us with (and PBS)",
	"  Cian Duffy (MYOB) - BeOS port / manual writing",
	"  Christian Rosentreter (tokaiz) - MorphOS / AmigaOS port",
	"",
	"  Michael Blunck - Pre-Signals and Semaphores © 2003",
	"  George - Canal/Lock graphics © 2003-2004",
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
	switch(e->event) {
	case WE_CREATE: /* Set up window counter and start position of scroller */
		WP(w, scroller_d).counter = 0;
		WP(w, scroller_d).height = w->height - 40;
		break;
	case WE_PAINT: {
		uint i;
		int y = WP(w, scroller_d).height;
		DrawWindowWidgets(w);

		// Show original copyright and revision version
		DrawStringCentered(210, 17, STR_00B6_ORIGINAL_COPYRIGHT, 0);
		DrawStringCentered(210, 17 + 10, STR_00B7_VERSION, 0);

		// Show all scrolling credits
		for (i = 0; i < lengthof(credits); i++) {
			if (y >= 50 && y < (w->height - 40)) {
				DoDrawString(credits[i], 10, y, 0x10);
			}
			y += 10;
		}

		// If the last text has scrolled start anew from the start
		if (y < 50) WP(w, scroller_d).height = w->height - 40;

		DrawStringMultiCenter(210, w->height - 15, STR_00BA_COPYRIGHT_OPENTTD, 398);
	}	break;
	case WE_MOUSELOOP: /* Timer to scroll the text and adjust the new top */
		if (WP(w, scroller_d).counter++ % 3 == 0) {
			WP(w, scroller_d).height--;
			SetWindowDirty(w);
		}
		break;
	}
}

static const Widget _about_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,					STR_NULL},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   419,     0,    13, STR_015B_OPENTTD,	STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   419,    14,   271, 0x0,								STR_NULL},
{      WWT_FRAME,   RESIZE_NONE,    14,     5,   414,    40,   245, STR_NULL,					STR_NULL},
{    WIDGETS_END},
};

static const WindowDesc _about_desc = {
	WDP_CENTER, WDP_CENTER, 420, 272,
	WC_GAME_OPTIONS,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_about_widgets,
	AboutWindowProc
};


void ShowAboutWindow(void)
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	AllocateWindowDesc(&_about_desc);
}

static int _tree_to_plant;

static const uint32 _tree_sprites[] = {
	0x655,0x663,0x678,0x62B,0x647,0x639,0x64E,0x632,0x67F,0x68D,0x69B,0x6A9,
	0x6AF,0x6D2,0x6D9,0x6C4,0x6CB,0x6B6,0x6BD,0x6E0,
	0x72E,0x734,0x74A,0x74F,0x76B,0x78F,0x788,0x77B,0x75F,0x774,0x720,0x797,
	0x79E,0x30D87A5,0x30B87AC,0x7B3,0x7BA,0x30B87C1,0x30887C8,0x30A87CF,0x30B87D6
};

static void BuildTreesWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		int x,y;
		int i, count;

		DrawWindowWidgets(w);

		WP(w,tree_d).base = i = _tree_base_by_landscape[_opt.landscape];
		WP(w,tree_d).count = count = _tree_count_by_landscape[_opt.landscape];

		x = 18;
		y = 54;
		do {
			DrawSprite(_tree_sprites[i], x, y);
			x += 35;
			if (!(++i & 3)) {
				x -= 35*4;
				y += 47;
			}
		} while (--count);
	} break;

	case WE_CLICK: {
		int wid;

		switch(wid=e->click.widget) {
		case 0:
			ResetObjectToPlace();
			return;
		case 3: case 4: case 5: case 6:
		case 7: case 8: case 9: case 10:
		case 11:case 12: case 13: case 14:
			if ( (uint)(wid-3) >= (uint)WP(w,tree_d).count)
				return;

			if (HandlePlacePushButton(w, wid, SPR_CURSOR_TREE, 1, NULL))
				_tree_to_plant = WP(w,tree_d).base + wid - 3;
			break;

		case 15: // tree of random type.
			if (HandlePlacePushButton(w, 15, SPR_CURSOR_TREE, 1, NULL))
				_tree_to_plant = -1;
			break;

		case 16: /* place trees randomly over the landscape*/
			w->click_state |= 1 << 16;
			w->flags4 |= 5 << WF_TIMEOUT_SHL;
			SndPlayFx(SND_15_BEEP);
			PlaceTreesRandomly();
			MarkWholeScreenDirty();
			break;
		}
	} break;

	case WE_PLACE_OBJ:
		VpStartPlaceSizing(e->place.tile, VPM_X_AND_Y_LIMITED);
		VpSetPlaceSizingLimit(20);
		break;

	case WE_PLACE_DRAG:
		VpSelectTilesWithMethod(e->place.pt.x, e->place.pt.y, e->place.userdata);
		return;

	case WE_PLACE_MOUSEUP:
		if (e->click.pt.x != -1) {
			DoCommandP(e->place.tile, _tree_to_plant, e->place.starttile, NULL,
				CMD_PLANT_TREE | CMD_AUTO | CMD_MSG(STR_2805_CAN_T_PLANT_TREE_HERE));
		}
		break;

	case WE_TIMEOUT:
		UnclickSomeWindowButtons(w, 1<<16);
		break;

	case WE_ABORT_PLACE_OBJ:
		w->click_state = 0;
		SetWindowDirty(w);
		break;
	}
}

static const Widget _build_trees_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,				STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   142,     0,    13, STR_2802_TREES,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   142,    14,   170, 0x0,							STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,    16,    61, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,    16,    61, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,    16,    61, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,    16,    61, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,    63,   108, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,    63,   108, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,    63,   108, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,    63,   108, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,   110,   155, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,   110,   155, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,   110,   155, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,   110,   155, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{		WWT_CLOSEBOX,   RESIZE_NONE,    14,   2,   140,   157,   168, STR_TREES_RANDOM_TYPE, STR_TREES_RANDOM_TYPE_TIP},
{    WIDGETS_END},
};

static const WindowDesc _build_trees_desc = {
	497, 22, 143, 171,
	WC_BUILD_TREES, WC_SCEN_LAND_GEN,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_trees_widgets,
	BuildTreesWndProc
};

static const Widget _build_trees_scen_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,				STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   142,     0,    13, STR_2802_TREES,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   142,    14,   183, 0x0,							STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,    16,    61, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,    16,    61, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,    16,    61, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,    16,    61, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,    63,   108, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,    63,   108, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,    63,   108, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,    63,   108, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,   110,   155, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,   110,   155, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,   110,   155, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,   110,   155, 0x0,							STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{		WWT_CLOSEBOX,   RESIZE_NONE,    14,		 2,   140,   157,   168, STR_TREES_RANDOM_TYPE,	STR_TREES_RANDOM_TYPE_TIP},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     2,   140,   170,   181, STR_028A_RANDOM_TREES,	STR_028B_PLANT_TREES_RANDOMLY_OVER},
{    WIDGETS_END},
};

static const WindowDesc _build_trees_scen_desc = {
	-1, -1, 143, 184,
	WC_BUILD_TREES,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_trees_scen_widgets,
	BuildTreesWndProc
};


void ShowBuildTreesToolbar(void)
{
	AllocateWindowDescFront(&_build_trees_desc, 0);
}

void ShowBuildTreesScenToolbar(void)
{
	AllocateWindowDescFront(&_build_trees_scen_desc, 0);
}

static uint32 _errmsg_decode_params[20];
static StringID _errmsg_message_1, _errmsg_message_2;
static uint _errmsg_duration;


static const Widget _errmsg_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     4,     0,    10,     0,    13, STR_00C5,					STR_NULL},
{    WWT_CAPTION,   RESIZE_NONE,     4,    11,   239,     0,    13, STR_00B2_MESSAGE,	STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,     4,     0,   239,    14,    45, 0x0,								STR_NULL},
{    WIDGETS_END},
};

static const Widget _errmsg_face_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     4,     0,    10,     0,    13, STR_00C5,							STR_NULL},
{    WWT_CAPTION,   RESIZE_NONE,     4,    11,   333,     0,    13, STR_00B3_MESSAGE_FROM,	STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,     4,     0,   333,    14,   136, 0x0,										STR_NULL},
{   WIDGETS_END},
};

static void ErrmsgWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT:
		COPY_IN_DPARAM(0, _errmsg_decode_params, lengthof(_errmsg_decode_params));
		DrawWindowWidgets(w);
		COPY_IN_DPARAM(0, _errmsg_decode_params, lengthof(_errmsg_decode_params));
		if (!IsWindowOfPrototype(w, _errmsg_face_widgets)) {
			DrawStringMultiCenter(
				120,
				(_errmsg_message_1 == INVALID_STRING_ID ? 25 : 15),
				_errmsg_message_2,
				238);
			if (_errmsg_message_1 != INVALID_STRING_ID)
				DrawStringMultiCenter(
					120,
					30,
					_errmsg_message_1,
					238);
		} else {
			Player *p = GetPlayer(GetDParamX(_errmsg_decode_params,2));
			DrawPlayerFace(p->face, p->player_color, 2, 16);

			DrawStringMultiCenter(
				214,
				(_errmsg_message_1 == INVALID_STRING_ID ? 65 : 45),
				_errmsg_message_2,
				238);
			if (_errmsg_message_1 != INVALID_STRING_ID)
				DrawStringMultiCenter(
					214,
					90,
					_errmsg_message_1,
					238);
		}
		break;

	case WE_MOUSELOOP:
		if (_right_button_down)
			DeleteWindow(w);
		break;
	case WE_4:
		if (!--_errmsg_duration)
			DeleteWindow(w);
		break;
	case WE_DESTROY: {
		SetRedErrorSquare(0);
		_switch_mode_errorstr = INVALID_STRING_ID;
		break;
		}

	case WE_KEYPRESS:
		if (e->keypress.keycode == WKC_SPACE) {
			// Don't continue.
			e->keypress.cont = false;
			DeleteWindow(w);
		}
		break;
	}
}

void ShowErrorMessage(StringID msg_1, StringID msg_2, int x, int y)
{
	Window *w;
	ViewPort *vp;
	Point pt;

	DeleteWindowById(WC_ERRMSG, 0);

	//assert(msg_2);
	if (msg_2 == 0) msg_2 = STR_EMPTY;

	_errmsg_message_1 = msg_1;
	_errmsg_message_2 = msg_2;
	COPY_OUT_DPARAM(_errmsg_decode_params, 0, lengthof(_errmsg_decode_params));
	_errmsg_duration = _patches.errmsg_duration;
	if (!_errmsg_duration)
		return;

	if (_errmsg_message_1 != STR_013B_OWNED_BY || GetDParamX(_errmsg_decode_params,2) >= 8) {

		if ( (x|y) != 0) {
			pt = RemapCoords2(x, y);
			for(w=_windows; w->window_class != WC_MAIN_WINDOW; w++) {}
			vp = w->viewport;

			// move x pos to opposite corner
			pt.x = ((pt.x - vp->virtual_left) >> vp->zoom) + vp->left;
			pt.x = (pt.x < (_screen.width >> 1)) ? _screen.width - 260 : 20;

			// move y pos to opposite corner
			pt.y = ((pt.y - vp->virtual_top) >> vp->zoom) + vp->top;
			pt.y = (pt.y < (_screen.height >> 1)) ? _screen.height - 80 : 100;

		} else {
			pt.x = (_screen.width - 240) >> 1;
			pt.y = (_screen.height - 46) >> 1;
		}
		w = AllocateWindow(pt.x, pt.y, 240, 46, ErrmsgWndProc, WC_ERRMSG, _errmsg_widgets);
	} else {
		if ( (x|y) != 0) {
			pt = RemapCoords2(x, y);
			for(w=_windows; w->window_class != WC_MAIN_WINDOW; w++) {}
			vp = w->viewport;
			pt.x = clamp(((pt.x - vp->virtual_left) >> vp->zoom) + vp->left - (334/2), 0, _screen.width - 334);
			pt.y = clamp(((pt.y - vp->virtual_top) >> vp->zoom) + vp->top - (137/2), 22, _screen.height - 137);
		} else {
			pt.x = (_screen.width - 334) >> 1;
			pt.y = (_screen.height - 137) >> 1;
		}
		w = AllocateWindow(pt.x, pt.y, 334, 137, ErrmsgWndProc, WC_ERRMSG, _errmsg_face_widgets);
	}

	w->desc_flags = WDF_STD_BTN | WDF_DEF_WIDGET;
}


void ShowEstimatedCostOrIncome(int32 cost, int x, int y)
{
	int msg = STR_0805_ESTIMATED_COST;

	if (cost < 0) {
		cost = -cost;
		msg = STR_0807_ESTIMATED_INCOME;
	}
	SetDParam(0, cost);
	ShowErrorMessage(-1, msg, x, y);
}

void ShowCostOrIncomeAnimation(int x, int y, int z, int32 cost)
{
	int msg;
	Point pt = RemapCoords(x,y,z);

	msg = STR_0801_COST;
	if (cost < 0) {
		cost = -cost;
		msg = STR_0803_INCOME;
	}
	SetDParam(0, cost);
	AddTextEffect(msg, pt.x, pt.y, 0x250);
}

void ShowFeederIncomeAnimation(int x, int y, int z, int32 cost)
{
	Point pt = RemapCoords(x,y,z);

	SetDParam(0, cost);
	AddTextEffect(STR_FEEDER, pt.x, pt.y, 0x250);
}

static Widget _tooltips_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   199,     0,    31, 0x0, STR_NULL},
{   WIDGETS_END},
};


static void TooltipsWndProc(Window *w, WindowEvent *e)
{

	switch(e->event) {
	case WE_PAINT: {
		GfxFillRect(0, 0, w->width - 1, w->height - 1, 0);
		GfxFillRect(1, 1, w->width - 2, w->height - 2, 0x44);
		DrawStringMultiCenter((w->width>>1), (w->height>>1)-5, WP(w,tooltips_d).string_id, 197);
		break;
	}
	case WE_MOUSELOOP:
		if (!_right_button_down)
			DeleteWindow(w);
		break;
	}
}

void GuiShowTooltips(StringID string_id)
{
	char buffer[512];
	Window *w;
	int right,bottom;
	int x,y;

	if (string_id == 0)
		return;

	w = FindWindowById(WC_TOOLTIPS, 0);
	if (w != NULL) {
		if (WP(w,tooltips_d).string_id == string_id)
			return;
		DeleteWindow(w);
	}

	GetString(buffer, string_id);

	right = GetStringWidth(buffer) + 4;

	bottom = 14;
	if (right > 200) {
		bottom += ((right - 4) / 176) * 10;
		right = 200;
	}

	_tooltips_widgets[0].right = right;
	_tooltips_widgets[0].bottom = bottom;

	y = _cursor.pos.y + 30;
	if (y < 22) y = 22;

	if (y > (_screen.height - 44) && (y-=52) > (_screen.height - 44))
		y = (_screen.height - 44);

	x = _cursor.pos.x - (right >> 1);
	if (x < 0) x = 0;
	if (x > (_screen.width - right)) x = _screen.width - right;

	w = AllocateWindow(x, y, right, bottom, TooltipsWndProc, WC_TOOLTIPS, _tooltips_widgets);
	WP(w,tooltips_d).string_id = string_id;
	w->flags4 &= ~WF_WHITE_BORDER_MASK;
}


static void DrawStationCoverageText(const AcceptedCargo accepts,
	int str_x, int str_y, uint mask)
{
	char *b = _userstring;
	int i;

	b = InlineString(b, STR_000D_ACCEPTS);

	for (i = 0; i != NUM_CARGO; i++, mask >>= 1) {
		if (accepts[i] >= 8 && mask & 1) {
			b = InlineString(b, _cargoc.names_s[i]);
			*b++ = ',';
			*b++ = ' ';
		}
	}

	if (b == &_userstring[3]) {
		b = InlineString(b, STR_00D0_NOTHING);
		*b++ = '\0';
	} else {
		b[-2] = '\0';
	}

	DrawStringMultiLine(str_x, str_y, STR_SPEC_USERSTRING, 144);
}

void DrawStationCoverageAreaText(int sx, int sy, uint mask, int rad) {
	int x = _thd.pos.x;
	int y = _thd.pos.y;
	uint accepts[NUM_CARGO];
	if (x != -1) {
		GetAcceptanceAroundTiles(accepts, TileVirtXY(x, y), _thd.size.x / 16, _thd.size.y / 16 , rad);
		DrawStationCoverageText(accepts, sx, sy, mask);
	}
}

void CheckRedrawStationCoverage(Window *w)
{
	if (_thd.dirty&1) {
		_thd.dirty&=~1;
		SetWindowDirty(w);
	}
}


void UnclickSomeWindowButtons(Window *w, uint32 mask)
{
	uint32 x = w->click_state & mask;
	int i = 0;
	w->click_state ^= x;
	do {
		if (x&1) InvalidateWidget(w,i);
	} while(i++,x>>=1);
}


void UnclickWindowButtons(Window *w)
{
	bool sticky = false;
	if (w->desc_flags & WDF_STICKY_BUTTON && HASBIT(w->click_state, 2))	sticky = true;

	UnclickSomeWindowButtons(w, (uint32)-1);

	if (sticky) SETBIT(w->click_state, 2);
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

static void DelChar(Textbuf *tb)
{
	tb->width -= GetCharacterWidth((byte)tb->buf[tb->caretpos]);
	memmove(tb->buf + tb->caretpos, tb->buf + tb->caretpos + 1, tb->length - tb->caretpos);
	tb->length--;
}

/**
 * Delete a character from a textbuffer, either with 'Delete' or 'Backspace'
 * The character is delete from the position the caret is at
 * @param tb @Textbuf type to be changed
 * @param delmode Type of deletion, either @WKC_BACKSPACE or @WKC_DELETE
 * @return Return true on successfull change of Textbuf, or false otherwise
 */
bool DeleteTextBufferChar(Textbuf *tb, int delmode)
{
	if (delmode == WKC_BACKSPACE && tb->caretpos != 0) {
		tb->caretpos--;
		tb->caretxoffs -= GetCharacterWidth((byte)tb->buf[tb->caretpos]);

		DelChar(tb);
		return true;
	} else if (delmode == WKC_DELETE && tb->caretpos < tb->length) {
		DelChar(tb);
		return true;
	}

	return false;
}

/**
 * Delete every character in the textbuffer
 * @param tb @Textbuf buffer to be emptied
 */
void DeleteTextBufferAll(Textbuf *tb)
{
	memset(tb->buf, 0, tb->maxlength);
	tb->length = tb->width = 0;
	tb->caretpos = tb->caretxoffs = 0;
}

/**
 * Insert a character to a textbuffer. If maxlength is zero, we don't care about
 * the screenlength but only about the physical length of the string
 * @param tb @Textbuf type to be changed
 * @param key Character to be inserted
 * @return Return true on successfull change of Textbuf, or false otherwise
 */
bool InsertTextBufferChar(Textbuf *tb, byte key)
{
	const byte charwidth = GetCharacterWidth(key);
	if (tb->length < tb->maxlength && (tb->maxwidth == 0 || tb->width + charwidth <= tb->maxwidth)) {
		memmove(tb->buf + tb->caretpos + 1, tb->buf + tb->caretpos, (tb->length - tb->caretpos) + 1);
		tb->buf[tb->caretpos] = key;
		tb->length++;
		tb->width += charwidth;

		tb->caretpos++;
		tb->caretxoffs += charwidth;
		return true;
	}
	return false;
}

/**
 * Handle text navigation with arrow keys left/right.
 * This defines where the caret will blink and the next characer interaction will occur
 * @param tb @Textbuf type where navigation occurs
 * @param navmode Direction in which navigation occurs @WKC_LEFT, @WKC_RIGHT, @WKC_END, @WKC_HOME
 * @return Return true on successfull change of Textbuf, or false otherwise
 */
bool MoveTextBufferPos(Textbuf *tb, int navmode)
{
	switch (navmode) {
	case WKC_LEFT:
		if (tb->caretpos != 0) {
			tb->caretpos--;
			tb->caretxoffs -= GetCharacterWidth((byte)tb->buf[tb->caretpos]);
			return true;
		}
		break;
	case WKC_RIGHT:
		if (tb->caretpos < tb->length) {
			tb->caretxoffs += GetCharacterWidth((byte)tb->buf[tb->caretpos]);
			tb->caretpos++;
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
 * Update @Textbuf type with its actual physical character and screenlength
 * Get the count of characters in the string as well as the width in pixels.
 * Useful when copying in a larger amount of text at once
 * @param tb @Textbuf type which length is calculated
 */
void UpdateTextBufferSize(Textbuf *tb)
{
	char *buf;
	tb->length = 0;
	tb->width = 0;

	for (buf = tb->buf; *buf != '\0' && tb->length <= tb->maxlength; buf++) {
		tb->length++;
		tb->width += GetCharacterWidth((byte)*buf);
	}

	tb->caretpos = tb->length;
	tb->caretxoffs = tb->width;
}

int HandleEditBoxKey(Window *w, int wid, WindowEvent *we)
{
	we->keypress.cont = false;

	switch (we->keypress.keycode) {
	case WKC_ESC: return 2;
	case WKC_RETURN: case WKC_NUM_ENTER: return 1;
	case (WKC_CTRL | 'V'):
		if (InsertTextBufferClipboard(&WP(w, querystr_d).text))
			InvalidateWidget(w, wid);
		break;
	case (WKC_CTRL | 'U'):
		DeleteTextBufferAll(&WP(w, querystr_d).text);
		InvalidateWidget(w, wid);
		break;
	case WKC_BACKSPACE: case WKC_DELETE:
		if (DeleteTextBufferChar(&WP(w, querystr_d).text, we->keypress.keycode))
			InvalidateWidget(w, wid);
		break;
	case WKC_LEFT: case WKC_RIGHT: case WKC_END: case WKC_HOME:
		if (MoveTextBufferPos(&WP(w, querystr_d).text, we->keypress.keycode))
			InvalidateWidget(w, wid);
  	break;
	default:
		if (IsValidAsciiChar(we->keypress.ascii)) {
			if (InsertTextBufferChar(&WP(w, querystr_d).text, we->keypress.ascii))
				InvalidateWidget(w, wid);
		} else // key wasn't caught
			we->keypress.cont = true;
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

void HandleEditBox(Window *w, int wid)
{
	if (HandleCaret(&WP(w, querystr_d).text))
		InvalidateWidget(w, wid);
}

void DrawEditBox(Window *w, int wid)
{
	const Widget *wi = w->widget + wid;
	const Textbuf *tb = &WP(w,querystr_d).text;

	GfxFillRect(wi->left+1, wi->top+1, wi->right-1, wi->bottom-1, 215);
	DoDrawString(tb->buf, wi->left+2, wi->top+1, 8);
	if (tb->caret)
		DoDrawString("_", wi->left + 2 + tb->caretxoffs, wi->top + 1, 12);
}

static void QueryStringWndProc(Window *w, WindowEvent *e)
{
	static bool closed = false;
	switch (e->event) {
	case WE_CREATE:
		SETBIT(_no_scroll, SCROLL_EDIT);
		closed = false;
		break;

	case WE_PAINT:
		SetDParam(0, WP(w,querystr_d).caption);
		DrawWindowWidgets(w);

		DrawEditBox(w, 5);
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 3: DeleteWindow(w); break;
		case 4:
press_ok:;
			if (WP(w, querystr_d).orig != NULL &&
					strcmp(WP(w, querystr_d).text.buf, WP(w, querystr_d).orig) == 0) {
				DeleteWindow(w);
			} else {
				char *buf = WP(w,querystr_d).text.buf;
				WindowClass wnd_class = WP(w,querystr_d).wnd_class;
				WindowNumber wnd_num = WP(w,querystr_d).wnd_num;
				Window *parent;

				// Mask the edit-box as closed, so we don't send out a CANCEL
				closed = true;

				DeleteWindow(w);

				parent = FindWindowById(wnd_class, wnd_num);
				if (parent != NULL) {
					WindowEvent e;
					e.event = WE_ON_EDIT_TEXT;
					e.edittext.str = buf;
					parent->wndproc(parent, &e);
				}
			}
			break;
		}
		break;

	case WE_MOUSELOOP: {
		if (!FindWindowById(WP(w,querystr_d).wnd_class, WP(w,querystr_d).wnd_num)) {
			DeleteWindow(w);
			return;
		}
		HandleEditBox(w, 5);
	} break;

	case WE_KEYPRESS: {
		switch(HandleEditBoxKey(w, 5, e)) {
		case 1: // Return
			goto press_ok;
		case 2: // Escape
			DeleteWindow(w);
			break;
		}
	} break;

	case WE_DESTROY:
		// If the window is not closed yet, it means it still needs to send a CANCEL
		if (!closed) {
			Window *parent = FindWindowById(WP(w,querystr_d).wnd_class, WP(w,querystr_d).wnd_num);
			if (parent != NULL) {
				WindowEvent e;
				e.event = WE_ON_EDIT_TEXT_CANCEL;
				parent->wndproc(parent, &e);
			}
		}
		_query_string_active = false;
		CLRBIT(_no_scroll, SCROLL_EDIT);
		break;
	}
}

static const Widget _query_string_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,				STR_NULL},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   259,     0,    13, STR_012D,				STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   259,    14,    29, 0x0,							STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,   129,    30,    41, STR_012E_CANCEL,	STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   130,   259,    30,    41, STR_012F_OK,			STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     2,   257,    16,    27, 0x0,							STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _query_string_desc = {
	190, 219, 260, 42,
	WC_QUERY_STRING,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_query_string_widgets,
	QueryStringWndProc
};

static char _edit_str_buf[64];
static char _orig_str_buf[lengthof(_edit_str_buf)];

void ShowQueryString(StringID str, StringID caption, uint maxlen, uint maxwidth, WindowClass window_class, WindowNumber window_number)
{
	Window *w;
	uint realmaxlen = maxlen & ~0x1000;

	assert(realmaxlen < lengthof(_edit_str_buf));

	DeleteWindowById(WC_QUERY_STRING, 0);
	DeleteWindowById(WC_SAVELOAD, 0);

	w = AllocateWindowDesc(&_query_string_desc);

	GetString(_edit_str_buf, str);
	_edit_str_buf[realmaxlen] = '\0';

	if (maxlen & 0x1000) {
		WP(w, querystr_d).orig = NULL;
	} else {
		strcpy(_orig_str_buf, _edit_str_buf);
		WP(w, querystr_d).orig = _orig_str_buf;
	}

	w->click_state = 1 << 5;
	WP(w, querystr_d).caption = caption;
	WP(w, querystr_d).wnd_class = window_class;
	WP(w, querystr_d).wnd_num = window_number;
	WP(w, querystr_d).text.caret = false;
	WP(w, querystr_d).text.maxlength = realmaxlen - 1;
	WP(w, querystr_d).text.maxwidth = maxwidth;
	WP(w, querystr_d).text.buf = _edit_str_buf;
	UpdateTextBufferSize(&WP(w, querystr_d).text);

	_query_string_active = true;
}

static const Widget _load_dialog_1_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,						STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   256,     0,    13, STR_4001_LOAD_GAME,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,   127,    14,    25, STR_SORT_BY_NAME,		STR_SORT_ORDER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   128,   256,    14,    25, STR_SORT_BY_DATE,		STR_SORT_ORDER_TIP},
{     WWT_IMGBTN,  RESIZE_RIGHT,    14,     0,   256,    26,    47, 0x0,								STR_NULL},
{     WWT_IMGBTN,     RESIZE_RB,    14,     0,   256,    48,   293, 0x0,								STR_NULL},
{          WWT_6,     RESIZE_RB,    14,     2,   243,    50,   291, 0x0,								STR_400A_LIST_OF_DRIVES_DIRECTORIES},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   245,   256,    48,   281, 0x0,								STR_0190_SCROLL_BAR_SCROLLS_LIST},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   245,   256,   282,   293, 0x0,								STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _load_dialog_2_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,								STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   256,     0,    13, STR_0298_LOAD_SCENARIO,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,   127,    14,    25, STR_SORT_BY_NAME,				STR_SORT_ORDER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   128,   256,    14,    25, STR_SORT_BY_DATE,				STR_SORT_ORDER_TIP},
{     WWT_IMGBTN,  RESIZE_RIGHT,    14,     0,   256,    26,    47, 0x0,										STR_NULL},
{     WWT_IMGBTN,     RESIZE_RB,    14,     0,   256,    48,   293, 0x0,										STR_NULL},
{          WWT_6,     RESIZE_RB,    14,     2,   243,    50,   291, 0x0,										STR_400A_LIST_OF_DRIVES_DIRECTORIES},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   245,   256,    48,   281, 0x0,										STR_0190_SCROLL_BAR_SCROLLS_LIST},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   245,   256,   282,   293, 0x0,										STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _save_dialog_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,						STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   256,     0,    13, STR_4000_SAVE_GAME,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,   127,    14,    25, STR_SORT_BY_NAME,		STR_SORT_ORDER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   128,   256,    14,    25, STR_SORT_BY_DATE,		STR_SORT_ORDER_TIP},
{     WWT_IMGBTN,  RESIZE_RIGHT,    14,     0,   256,    26,    47, 0x0,								STR_NULL},
{     WWT_IMGBTN,     RESIZE_RB,    14,     0,   256,    48,   291, 0x0,								STR_NULL},
{          WWT_6,     RESIZE_RB,    14,     2,   243,    50,   290, 0x0,								STR_400A_LIST_OF_DRIVES_DIRECTORIES},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   245,   256,    48,   291, 0x0,								STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_IMGBTN,    RESIZE_RTB,    14,     0,   256,   292,   307, 0x0,								STR_NULL},
{     WWT_IMGBTN,    RESIZE_RTB,    14,     2,   254,   294,   305, 0x0,								STR_400B_CURRENTLY_SELECTED_NAME},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   127,   308,   319, STR_4003_DELETE,		STR_400C_DELETE_THE_CURRENTLY_SELECTED},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   128,   244,   308,   319, STR_4002_SAVE,			STR_400D_SAVE_THE_CURRENT_GAME_USING},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   245,   256,   308,   319, 0x0,								STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _save_dialog_scen_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,								STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   256,     0,    13, STR_0299_SAVE_SCENARIO, STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,   127,    14,    25, STR_SORT_BY_NAME,				STR_SORT_ORDER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   128,   256,    14,    25, STR_SORT_BY_DATE,				STR_SORT_ORDER_TIP},
{     WWT_IMGBTN,  RESIZE_RIGHT,    14,     0,   256,    26,    47, 0x0,										STR_NULL},
{     WWT_IMGBTN,     RESIZE_RB,    14,     0,   256,    48,   291, 0x0,										STR_NULL},
{          WWT_6,     RESIZE_RB,    14,     2,   243,    50,   290, 0x0,										STR_400A_LIST_OF_DRIVES_DIRECTORIES},
{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   245,   256,    48,   291, 0x0,										STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_IMGBTN,    RESIZE_RTB,    14,     0,   256,   292,   307, 0x0,										STR_NULL},
{     WWT_IMGBTN,    RESIZE_RTB,    14,     2,   254,   294,   305, 0x0,										STR_400B_CURRENTLY_SELECTED_NAME},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   127,   308,   319, STR_4003_DELETE,				STR_400C_DELETE_THE_CURRENTLY_SELECTED},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   128,   244,   308,   319, STR_4002_SAVE,					STR_400D_SAVE_THE_CURRENT_GAME_USING},
{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   245,   256,   308,   319, 0x0,										STR_RESIZE_BUTTON},
{   WIDGETS_END},
};


void BuildFileList(void)
{
	_fios_path_changed = true;
	FiosFreeSavegameList();
	if (_saveload_mode == SLD_NEW_GAME || _saveload_mode == SLD_LOAD_SCENARIO || _saveload_mode == SLD_SAVE_SCENARIO) {
		_fios_list = FiosGetScenarioList(&_fios_num, _saveload_mode);
	} else
		_fios_list = FiosGetSavegameList(&_fios_num, _saveload_mode);
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
	DrawString(2, 37, str, 0);
	DoDrawStringTruncated(path, 2, 27, 16, maxw);
}

static void MakeSortedSaveGameList(void)
{
	/*	Directories are always above the files (FIOS_TYPE_DIR)
	 *	Drives (A:\ (windows only) are always under the files (FIOS_TYPE_DRIVE)
	 *	Only sort savegames/scenarios, not directories
	 */

	int i, sort_start, sort_end, s_amount;
	i = sort_start = sort_end = 0;

	while (i < _fios_num) {
		if (_fios_list[i].type == FIOS_TYPE_DIR || _fios_list[i].type == FIOS_TYPE_PARENT)
			sort_start++;

		if (_fios_list[i].type == FIOS_TYPE_DRIVE)
			sort_end++;

		i++;
	}

	s_amount = _fios_num - sort_start - sort_end;
	if (s_amount > 0)
		qsort(_fios_list + sort_start, s_amount, sizeof(FiosItem), compare_FiosItems);
}

static void GenerateFileName(void)
{
	const Player *p;
	/* Check if we are not a specatator who wants to generate a name..
	    Let's use the name of player #0 for now. */
	if (_local_player < MAX_PLAYERS)
		p = GetPlayer(_local_player);
	else
		p = GetPlayer(0);

	SetDParam(0, p->name_1);
	SetDParam(1, p->name_2);
	SetDParam(2, _date);
	GetString(_edit_str_buf, STR_4004);
}

extern void StartupEngines(void);

static void SaveLoadDlgWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		int y,pos;
		const FiosItem *item;

		SetVScrollCount(w, _fios_num);
		DrawWindowWidgets(w);
		DrawFiosTexts(w->width);

		if (_savegame_sort_dirty) {
			_savegame_sort_dirty = false;
			MakeSortedSaveGameList();
		}

		GfxFillRect(w->widget[6].left + 1, w->widget[6].top + 1, w->widget[6].right, w->widget[6].bottom, 0xD7);
		DoDrawString(_savegame_sort_order & 1 ? "\xAA" : "\xA0", _savegame_sort_order <= 1 ? w->widget[3].right - 9 : w->widget[2].right - 9, 15, 0x10);

		y = w->widget[6].top + 1;
		pos = w->vscroll.pos;
		while (pos < _fios_num) {
			item = _fios_list + pos;
			DoDrawStringTruncated(item->title, 4, y, _fios_colors[item->type], w->width - 18);
			pos++;
			y+=10;
			if (y >= w->vscroll.cap*10+w->widget[6].top+1)
				break;
		}

		if (_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO) {
			DrawEditBox(w, 9);
		}
		break;
	}
	case WE_CLICK:
		switch(e->click.widget) {
		case 2: /* Sort save names by name */
			_savegame_sort_order = (_savegame_sort_order == 2) ? 3 : 2;
			_savegame_sort_dirty = true;
			SetWindowDirty(w);
			break;

		case 3: /* Sort save names by date */
			_savegame_sort_order = (_savegame_sort_order == 0) ? 1 : 0;
			_savegame_sort_dirty = true;
			SetWindowDirty(w);
			break;

		case 6: { /* Click the listbox */
			int y = (e->click.pt.y - w->widget[6].top - 1) / 10;
			char *name;
			const FiosItem *file;

			if (y < 0 || (y += w->vscroll.pos) >= w->vscroll.count)
				return;

			file = _fios_list + y;

			if ((name = FiosBrowseTo(file)) != NULL) {
				if (_saveload_mode == SLD_LOAD_GAME || _saveload_mode == SLD_LOAD_SCENARIO) {
					_switch_mode = (_game_mode == GM_EDITOR) ? SM_LOAD_SCENARIO : SM_LOAD;

					SetFiosType(file->type);
					ttd_strlcpy(_file_to_saveload.name, name, sizeof(_file_to_saveload.name));
					ttd_strlcpy(_file_to_saveload.title, file->title, sizeof(_file_to_saveload.title));

					DeleteWindow(w);
				} else {
					// SLD_SAVE_GAME, SLD_SAVE_SCENARIO copy clicked name to editbox
					ttd_strlcpy(WP(w, querystr_d).text.buf, file->title, WP(w, querystr_d).text.maxlength);
					UpdateTextBufferSize(&WP(w, querystr_d).text);
					InvalidateWidget(w, 9);
				}
			} else {
				// Changed directory, need repaint.
				SetWindowDirty(w);
				BuildFileList();
			}
			break;
		}
		case 10: case 11: /* Delete, Save game */
			break;
		}
		break;
	case WE_MOUSELOOP:
		HandleEditBox(w, 9);
		break;
	case WE_KEYPRESS:
		switch (HandleEditBoxKey(w, 9, e)) {
		case 1:
			HandleButtonClick(w, 11);
			break;
		}
		break;
	case WE_TIMEOUT:
		if (HASBIT(w->click_state, 10)) { /* Delete button clicked */
			FiosDelete(WP(w,querystr_d).text.buf);
			SetWindowDirty(w);
			BuildFileList();
			if (_saveload_mode == SLD_SAVE_GAME) {
				GenerateFileName(); /* Reset file name to current date */
				UpdateTextBufferSize(&WP(w, querystr_d).text);
			}
		} else if (HASBIT(w->click_state, 11)) { /* Save button clicked */
			_switch_mode = SM_SAVE;
			FiosMakeSavegameName(_file_to_saveload.name, WP(w,querystr_d).text.buf);

			/* In the editor set up the vehicle engines correctly (date might have changed) */
			if (_game_mode == GM_EDITOR) StartupEngines();
		}
		break;
	case WE_DESTROY:
		// pause is only used in single-player, non-editor mode, non menu mode
		if(!_networking && (_game_mode != GM_EDITOR) && (_game_mode != GM_MENU))
			DoCommandP(0, 0, 0, NULL, CMD_PAUSE);
		_query_string_active = false;
		FiosFreeSavegameList();
		CLRBIT(_no_scroll, SCROLL_SAVE);
		break;
	case WE_RESIZE: {
		/* Widget 2 and 3 have to go with halve speed, make it so obiwan */
		uint diff = e->sizing.diff.x / 2;
		w->widget[2].right += diff;
		w->widget[3].left  += diff;
		w->widget[3].right += e->sizing.diff.x;

		/* Same for widget 10 and 11 in save-dialog */
		if (_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO) {
			w->widget[10].right += diff;
			w->widget[11].left  += diff;
			w->widget[11].right += e->sizing.diff.x;
		}

		w->vscroll.cap += e->sizing.diff.y / 10;
		} break;
	}
}

static const WindowDesc _load_dialog_desc = {
	WDP_CENTER, WDP_CENTER, 257, 294,
	WC_SAVELOAD,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_load_dialog_1_widgets,
	SaveLoadDlgWndProc,
};

static const WindowDesc _load_dialog_scen_desc = {
	WDP_CENTER, WDP_CENTER, 257, 294,
	WC_SAVELOAD,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_load_dialog_2_widgets,
	SaveLoadDlgWndProc,
};

static const WindowDesc _save_dialog_desc = {
	WDP_CENTER, WDP_CENTER, 257, 320,
	WC_SAVELOAD,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_save_dialog_widgets,
	SaveLoadDlgWndProc,
};

static const WindowDesc _save_dialog_scen_desc = {
	WDP_CENTER, WDP_CENTER, 257, 320,
	WC_SAVELOAD,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_save_dialog_scen_widgets,
	SaveLoadDlgWndProc,
};

static const WindowDesc * const _saveload_dialogs[] = {
	&_load_dialog_desc,
	&_load_dialog_scen_desc,
	&_save_dialog_desc,
	&_save_dialog_scen_desc,
};

void ShowSaveLoadDialog(int mode)
{
	Window *w;

	SetObjectToPlace(SPR_CURSOR_ZZZ, 0, 0, 0);
	DeleteWindowById(WC_QUERY_STRING, 0);
	DeleteWindowById(WC_SAVELOAD, 0);

	_saveload_mode = mode;
	SETBIT(_no_scroll, SCROLL_SAVE);

	switch (mode) {
	case SLD_SAVE_GAME:
		GenerateFileName();
		break;
	case SLD_SAVE_SCENARIO:
		strcpy(_edit_str_buf, "UNNAMED");
		break;
	}

	w = AllocateWindowDesc(_saveload_dialogs[mode]);
	w->vscroll.cap = 24;
	w->resize.step_width = 2;
	w->resize.step_height = 10;
	w->resize.height = w->height - 14 * 10; // Minimum of 10 items
	SETBIT(w->click_state, 6);
	WP(w,querystr_d).text.caret = false;
	WP(w,querystr_d).text.maxlength = lengthof(_edit_str_buf) - 1;
	WP(w,querystr_d).text.maxwidth = 240;
	WP(w,querystr_d).text.buf = _edit_str_buf;
	UpdateTextBufferSize(&WP(w, querystr_d).text);

	// pause is only used in single-player, non-editor mode, non-menu mode. It
	// will be unpaused in the WE_DESTROY event handler.
	if(_game_mode != GM_MENU && !_networking && _game_mode != GM_EDITOR)
		DoCommandP(0, 1, 0, NULL, CMD_PAUSE);

	BuildFileList();

	ResetObjectToPlace();
}

void RedrawAutosave(void)
{
	SetWindowDirty(FindWindowById(WC_STATUS_BAR, 0));
}

static const Widget _select_scenario_widgets[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,						STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,     7,    11,   256,     0,    13, STR_400E_SELECT_NEW_GAME_TYPE, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,  RESIZE_RIGHT,     7,     0,   256,    14,    25, 0x0,								STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     7,     0,   127,    14,    25, STR_SORT_BY_NAME,		STR_SORT_ORDER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     7,   128,   256,    14,    25, STR_SORT_BY_DATE,		STR_SORT_ORDER_TIP},
{     WWT_IMGBTN,     RESIZE_RB,     7,     0,   244,    26,   319, 0x0,								STR_NULL},
{          WWT_6,     RESIZE_RB,     7,     2,   243,    28,   317, 0x0,								STR_400F_SELECT_SCENARIO_GREEN_PRE},
{  WWT_SCROLLBAR,    RESIZE_LRB,     7,   245,   256,    26,   307, 0x0,								STR_0190_SCROLL_BAR_SCROLLS_LIST},
{  WWT_RESIZEBOX,   RESIZE_LRTB,     7,   245,   256,   308,   319, 0x0,								STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static void SelectScenarioWndProc(Window *w, WindowEvent *e) {
	const int list_start = 45;
	switch(e->event) {
	case WE_PAINT:
	{
		int y,pos;
		const FiosItem *item;

		if (_savegame_sort_dirty) {
			_savegame_sort_dirty = false;
			MakeSortedSaveGameList();
		}

		SetVScrollCount(w, _fios_num);

		DrawWindowWidgets(w);
		DoDrawString(_savegame_sort_order & 1 ? "\xAA" : "\xA0", _savegame_sort_order <= 1 ? w->widget[4].right - 9 : w->widget[3].right - 9, 15, 0x10);
		DrawString(4, 32, STR_4010_GENERATE_RANDOM_NEW_GAME, 9);

		y = list_start;
		pos = w->vscroll.pos;
		while (pos < _fios_num) {
			item = _fios_list + pos;
			DoDrawString(item->title, 4, y, _fios_colors[item->type] );
			pos++;
			y+=10;
			if (y >= w->vscroll.cap*10+list_start)
				break;
		}
	}
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 3: /* Sort scenario names by name */
			_savegame_sort_order = (_savegame_sort_order == 2) ? 3 : 2;
			_savegame_sort_dirty = true;
			SetWindowDirty(w);
			break;

		case 4: /* Sort scenario names by date */
			_savegame_sort_order = (_savegame_sort_order == 0) ? 1 : 0;
			_savegame_sort_dirty = true;
			SetWindowDirty(w);
			break;

		case 6: /* Click the listbox */
			if (e->click.pt.y < list_start)
				GenRandomNewGame(Random(), InteractiveRandom());
			else {
				char *name;
				int y = (e->click.pt.y - list_start) / 10;
				const FiosItem *file;

				if (y < 0 || (y += w->vscroll.pos) >= w->vscroll.count)
					return;

				file = _fios_list + y;

				if ((name = FiosBrowseTo(file)) != NULL) {
					SetFiosType(file->type);
					strcpy(_file_to_saveload.name, name);
					DeleteWindow(w);
					StartScenarioEditor(Random(), InteractiveRandom());
				}
			}
			break;
		}
	case WE_DESTROY:
		break;

	case WE_RESIZE: {
		/* Widget 3 and 4 have to go with halve speed, make it so obiwan */
		uint diff = e->sizing.diff.x / 2;
		w->widget[3].right += diff;
		w->widget[4].left  += diff;
		w->widget[4].right += e->sizing.diff.x;

		w->vscroll.cap += e->sizing.diff.y / 10;
		} break;
	}
}

void SetFiosType(const byte fiostype)
{
	switch (fiostype) {
	case FIOS_TYPE_FILE: case FIOS_TYPE_SCENARIO:
		_file_to_saveload.mode = SL_LOAD;
		break;
	case FIOS_TYPE_OLDFILE: case FIOS_TYPE_OLD_SCENARIO:
		_file_to_saveload.mode = SL_OLD_LOAD;
		break;
	default:
		_file_to_saveload.mode = SL_INVALID;
	}
}

static const WindowDesc _select_scenario_desc = {
	WDP_CENTER, WDP_CENTER, 257, 320,
	WC_SAVELOAD,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_select_scenario_widgets,
	SelectScenarioWndProc
};

void AskForNewGameToStart(void)
{
	Window *w;

	DeleteWindowById(WC_QUERY_STRING, 0);
	DeleteWindowById(WC_SAVELOAD, 0);

	_saveload_mode = SLD_NEW_GAME;
	BuildFileList();

	w = AllocateWindowDesc(&_select_scenario_desc);
	w->vscroll.cap = 27;
	w->resize.step_width = 2;
	w->resize.step_height = 10;
	w->resize.height = w->height - 10 * 17; // Minimum of 10 in the list
}

static int32 ClickMoneyCheat(int32 p1, int32 p2)
{
		DoCommandP(0, -10000000, 0, NULL, CMD_MONEY_CHEAT);
		return true;
}

// p1 player to set to, p2 is -1 or +1 (down/up)
static int32 ClickChangePlayerCheat(int32 p1, int32 p2)
{
	while(p1 >= 0 && p1 < MAX_PLAYERS) {
		if (_players[p1].is_active)	{
			_local_player = p1;
			MarkWholeScreenDirty();
			return _local_player;
		}
		p1 += p2;
	}

	return _local_player;
}

// p1 -1 or +1 (down/up)
static int32 ClickChangeClimateCheat(int32 p1, int32 p2)
{
	if(p1==-1) p1 = 3;
	if(p1==4) p1 = 0;
	_opt.landscape = p1;
	GfxLoadSprites();
	MarkWholeScreenDirty();
	return _opt.landscape;
}

extern void EnginesMonthlyLoop(void);

// p2 1 (increase) or -1 (decrease)
static int32 ClickChangeDateCheat(int32 p1, int32 p2)
{
	YearMonthDay ymd;
	ConvertDayToYMD(&ymd, _date);

	if((ymd.year==0 && p2==-1) || (ymd.year==170 && p2==1)) return _cur_year;

	SetDate(ConvertYMDToDay(_cur_year + p2, ymd.month, ymd.day));
	EnginesMonthlyLoop();
	SetWindowDirty(FindWindowById(WC_STATUS_BAR, 0));
	return _cur_year;
}

typedef int32 CheckButtonClick(int32, int32);

typedef enum ce_type {
	CE_BOOL = 0,
	CE_UINT8 = 1,
	CE_INT16 = 2,
	CE_UINT16 = 3,
	CE_INT32 = 4,
	CE_BYTE = 5,
	CE_CLICK = 6,
} ce_type;

typedef struct CheatEntry {
	ce_type type;    // type of selector
	byte flags;		// selector flags
	StringID str; // string with descriptive text
	void *variable; // pointer to the variable
	bool *been_used; // has this cheat been used before?
	CheckButtonClick *click_proc; // procedure
	int16 min,max; // range for spinbox setting
	uint16 step;   // step for spinbox
} CheatEntry;

static int32 ReadCE(const CheatEntry*ce)
{
	switch(ce->type) {
	case CE_BOOL:   return *(bool*)ce->variable;
	case CE_UINT8:  return *(uint8*)ce->variable;
	case CE_INT16:  return *(int16*)ce->variable;
	case CE_UINT16: return *(uint16*)ce->variable;
	case CE_INT32:  return *(int32*)ce->variable;
	case CE_BYTE:   return *(byte*)ce->variable;
	case CE_CLICK:  return 0;
	default:
		NOT_REACHED();
	}

	/* useless, but avoids compiler warning this way */
	return 0;
}

static void WriteCE(const CheatEntry *ce, int32 val)
{
	switch(ce->type) {
	case CE_BOOL: *(bool*)ce->variable = (bool)val; break;
	case CE_BYTE: *(byte*)ce->variable = (byte)val; break;
	case CE_UINT8: *(uint8*)ce->variable = (uint8)val; break;
	case CE_INT16: *(int16*)ce->variable = (int16)val; break;
	case CE_UINT16: *(uint16*)ce->variable = (uint16)val; break;
	case CE_INT32: *(int32*)ce->variable = val; break;
	case CE_CLICK: break;
	default:
		NOT_REACHED();
	}
}


static const CheatEntry _cheats_ui[] = {
	{CE_CLICK, 0, STR_CHEAT_MONEY, 					&_cheats.money.value, 					&_cheats.money.been_used, 					&ClickMoneyCheat,					0, 0, 0},
	{CE_UINT8, 0, STR_CHEAT_CHANGE_PLAYER, 	&_local_player, 								&_cheats.switch_player.been_used,		&ClickChangePlayerCheat,	0, 11, 1},
	{CE_BOOL, 0, STR_CHEAT_EXTRA_DYNAMITE,	&_cheats.magic_bulldozer.value,	&_cheats.magic_bulldozer.been_used, NULL,											0, 0, 0},
	{CE_BOOL, 0, STR_CHEAT_CROSSINGTUNNELS,	&_cheats.crossing_tunnels.value,&_cheats.crossing_tunnels.been_used,NULL,											0, 0, 0},
	{CE_BOOL, 0, STR_CHEAT_BUILD_IN_PAUSE,	&_cheats.build_in_pause.value,	&_cheats.build_in_pause.been_used,	NULL,											0, 0, 0},
	{CE_BOOL, 0, STR_CHEAT_NO_JETCRASH,			&_cheats.no_jetcrash.value,			&_cheats.no_jetcrash.been_used,			NULL,											0, 0, 0},
	{CE_BOOL, 0, STR_CHEAT_SETUP_PROD,			&_cheats.setup_prod.value,			&_cheats.setup_prod.been_used,			NULL,											0, 0, 0},
	{CE_UINT8, 0, STR_CHEAT_SWITCH_CLIMATE, &_opt.landscape, 								&_cheats.switch_climate.been_used,	&ClickChangeClimateCheat,-1, 4, 1},
	{CE_UINT8, 0, STR_CHEAT_CHANGE_DATE,		&_cur_year,											&_cheats.change_date.been_used,			&ClickChangeDateCheat,	 -1, 1, 1},
};


static const Widget _cheat_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,		STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   399,     0,    13, STR_CHEATS,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   399,    14,   159, 0x0,					STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   399,    14,   159, 0x0,					STR_CHEATS_TIP},
{   WIDGETS_END},
};

extern void DrawPlayerIcon(int p, int x, int y);

static void CheatsWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		int clk = WP(w,def_d).data_1;
		const CheatEntry *ce = &_cheats_ui[0];
		int32 val;
		int x, y;
		int i;

		DrawWindowWidgets(w);

		DrawStringMultiCenter(200, 25, STR_CHEATS_WARNING, 350);

		x=0;
		y=45;

		for(i=0; i!=lengthof(_cheats_ui); i++,ce++) {

			DrawSprite((SPR_OPENTTD_BASE + ((*ce->been_used)?67:66)), x+5, y+2);

			if (ce->type == CE_BOOL) {
				DrawFrameRect(x+20, y+1, x+30+9, y+9, (*(bool*)ce->variable) ? 6 : 4, (*(bool*)ce->variable) ? FR_LOWERED : 0);
				SetDParam(0, *(bool*)ce->variable ? STR_CONFIG_PATCHES_ON : STR_CONFIG_PATCHES_OFF);

			}	else if (ce->type == CE_CLICK) {
				DrawFrameRect(x+20, y+1, x+30+9, y+9, 0, (WP(w,def_d).data_1 == i*2+1) ? FR_LOWERED : 0);
				if (i == 0)
					SetDParam64(0, (int64) 10000000);
				else
					SetDParam(0, false);

			} else {
				DrawFrameRect(x+20, y+1, x+20+9, y+9, 3, clk == i*2+1 ? FR_LOWERED : 0);
				DrawFrameRect(x+30, y+1, x+30+9, y+9, 3, clk == i*2+2 ? FR_LOWERED : 0);
				DrawStringCentered(x+25, y+1, STR_6819, 0);
				DrawStringCentered(x+35, y+1, STR_681A, 0);

				val = ReadCE(ce);

				// set correct string for switch climate cheat
				if(ce->str==STR_CHEAT_SWITCH_CLIMATE)
					val += STR_TEMPERATE_LANDSCAPE;

				SetDParam(0, val);

				// display date for change date cheat
				if(ce->str==STR_CHEAT_CHANGE_DATE)
					SetDParam(0, _date);

				// draw colored flag for change player cheat
				if(ce->str==STR_CHEAT_CHANGE_PLAYER)
					DrawPlayerIcon(_current_player, 156, y+2);

			}

		DrawString(50, y+1, ce->str, 0);

		y+=12;
		}
		break;
	}
	case WE_CLICK: {
			const CheatEntry *ce;
			uint btn = (e->click.pt.y - 46) / 12;
			int32 val, oval;
			uint x = e->click.pt.x;

			// not clicking a button?
			if(!IS_INT_INSIDE(x, 20, 40) || btn>=lengthof(_cheats_ui))
				break;

			ce = &_cheats_ui[btn];
			oval = val = ReadCE(ce);

			*ce->been_used = true;

			switch(ce->type) {
				case CE_BOOL:	{
					val ^= 1;
					if (ce->click_proc != NULL)
						ce->click_proc(val, 0);
					break;
				}

				case CE_CLICK:	{
					ce->click_proc(val, 0);
					WP(w,def_d).data_1 = btn * 2 + 1;
					break;
				}

				default:	{
					if (x >= 30) {
						//increase
						val += ce->step;
						if (val > ce->max) val = ce->max;
					} else {
						// decrease
						val -= ce->step;
						if (val < ce->min) val = ce->min;
					}

					// take whatever the function returns
					val = ce->click_proc(val, (x>=30) ? 1 : -1);

					if (val != oval)
						WP(w,def_d).data_1 = btn * 2 + 1 + ((x>=30) ? 1 : 0);

					break;
				}
			}

			if (val != oval) {
				WriteCE(ce, val);
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
	240, 22, 400, 160,
	WC_CHEATS,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_cheat_widgets,
	CheatsWndProc
};


void ShowCheatWindow(void)
{
	Window *w;

	DeleteWindowById(WC_CHEATS, 0);
	w = AllocateWindowDesc(&_cheats_desc);

	if (w)
		SetWindowDirty(w);
}
