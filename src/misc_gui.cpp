/* $Id$ */

/** @file misc_gui.cpp GUIs for a number of misc windows. */

#include "stdafx.h"
#include "openttd.h"
#include "heightmap.h"
#include "debug.h"
#include "landscape.h"
#include "newgrf.h"
#include "newgrf_text.h"
#include "saveload.h"
#include "tile_map.h"
#include "gui.h"
#include "window_gui.h"
#include "station_gui.h"
#include "textbuf_gui.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "station_func.h"
#include "command_func.h"
#include "player_func.h"
#include "player_base.h"
#include "town.h"
#include "network/network.h"
#include "variables.h"
#include "cheat_func.h"
#include "train.h"
#include "tgp.h"
#include "cargotype.h"
#include "player_face.h"
#include "strings_func.h"
#include "fileio.h"
#include "fios.h"
#include "tile_cmd.h"
#include "zoom_func.h"
#include "functions.h"
#include "window_func.h"
#include "date_func.h"
#include "sound_func.h"
#include "string_func.h"
#include "player_gui.h"
#include "settings_type.h"
#include "newgrf_cargo.h"
#include "rail_gui.h"
#include "tilehighlight_func.h"
#include "querystring_gui.h"

#include "table/sprites.h"
#include "table/strings.h"

/* Variables to display file lists */
FiosItem *_fios_list;
SaveLoadDialogMode _saveload_mode;


static bool _fios_path_changed;
static bool _savegame_sort_dirty;

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
	NULL
};

class LandInfoWindow : public Window {
	enum {
		LAND_INFO_LINES          =   7,
		LAND_INFO_LINE_BUFF_SIZE = 512,
	};

public:
	char landinfo_data[LAND_INFO_LINES][LAND_INFO_LINE_BUFF_SIZE];

	virtual void OnPaint()
	{
		DrawWindowWidgets(this);

		DoDrawStringCentered(140, 16, this->landinfo_data[0], TC_LIGHT_BLUE);
		DoDrawStringCentered(140, 27, this->landinfo_data[1], TC_FROMSTRING);
		DoDrawStringCentered(140, 38, this->landinfo_data[2], TC_FROMSTRING);
		DoDrawStringCentered(140, 49, this->landinfo_data[3], TC_FROMSTRING);
		DoDrawStringCentered(140, 60, this->landinfo_data[4], TC_FROMSTRING);
		if (!StrEmpty(this->landinfo_data[5])) DrawStringMultiCenter(140, 76, BindCString(this->landinfo_data[5]), this->width - 4);
		if (!StrEmpty(this->landinfo_data[6])) DoDrawStringCentered(140, 71, this->landinfo_data[6], TC_FROMSTRING);
	}

	LandInfoWindow(TileIndex tile) : Window(&_land_info_desc) {
		Player *p = GetPlayer(IsValidPlayer(_local_player) ? _local_player : PLAYER_FIRST);
		Town *t = ClosestTownFromTile(tile, _patches.dist_local_authority);

		Money old_money = p->player_money;
		p->player_money = INT64_MAX;
		CommandCost costclear = DoCommand(tile, 0, 0, 0, CMD_LANDSCAPE_CLEAR);
		p->player_money = old_money;

		/* Because build_date is not set yet in every TileDesc, we make sure it is empty */
		TileDesc td;
		AcceptedCargo ac;

		td.build_date = 0;
		GetAcceptedCargo(tile, ac);
		GetTileDesc(tile, &td);

		SetDParam(0, td.dparam[0]);
		GetString(this->landinfo_data[0], td.str, lastof(this->landinfo_data[0]));

		SetDParam(0, STR_01A6_N_A);
		if (td.owner != OWNER_NONE && td.owner != OWNER_WATER) GetNameOfOwner(td.owner, tile);
		GetString(this->landinfo_data[1], STR_01A7_OWNER, lastof(this->landinfo_data[1]));

		StringID str = STR_01A4_COST_TO_CLEAR_N_A;
		if (CmdSucceeded(costclear)) {
			SetDParam(0, costclear.GetCost());
			str = STR_01A5_COST_TO_CLEAR;
		}
		GetString(this->landinfo_data[2], str, lastof(this->landinfo_data[2]));

		snprintf(_userstring, lengthof(_userstring), "0x%.4X", tile);
		SetDParam(0, TileX(tile));
		SetDParam(1, TileY(tile));
		SetDParam(2, TileHeight(tile));
		SetDParam(3, STR_SPEC_USERSTRING);
		GetString(this->landinfo_data[3], STR_LANDINFO_COORDS, lastof(this->landinfo_data[3]));

		SetDParam(0, STR_01A9_NONE);
		if (t != NULL && t->IsValid()) {
			SetDParam(0, STR_TOWN);
			SetDParam(1, t->index);
		}
		GetString(this->landinfo_data[4], STR_01A8_LOCAL_AUTHORITY, lastof(this->landinfo_data[4]));

		char *strp = GetString(this->landinfo_data[5], STR_01CE_CARGO_ACCEPTED, lastof(this->landinfo_data[5]));
		bool found = false;

		for (CargoID i = 0; i < NUM_CARGO; ++i) {
			if (ac[i] > 0) {
				/* Add a comma between each item. */
				if (found) {
					*strp++ = ',';
					*strp++ = ' ';
				}
				found = true;

				/* If the accepted value is less than 8, show it in 1/8:ths */
				if (ac[i] < 8) {
					SetDParam(0, ac[i]);
					SetDParam(1, GetCargo(i)->name);
					strp = GetString(strp, STR_01D1_8, lastof(this->landinfo_data[5]));
				} else {
					strp = GetString(strp, GetCargo(i)->name, lastof(this->landinfo_data[5]));
				}
			}
		}
		if (!found) this->landinfo_data[5][0] = '\0';

		if (td.build_date != 0) {
			SetDParam(0, td.build_date);
			GetString(this->landinfo_data[6], STR_BUILD_DATE, lastof(this->landinfo_data[6]));
		} else {
			this->landinfo_data[6][0] = '\0';
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

		this->FindWindowPlacementAndResize(&_land_info_desc);
	}
};

static void Place_LandInfo(TileIndex tile)
{
	DeleteWindowById(WC_LAND_INFO, 0);
	new LandInfoWindow(tile);
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

struct scroller_d {
	int height;
	uint16 counter;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(scroller_d));

static const char *credits[] = {
	/*************************************************************************
	 *                      maximum length of string which fits in window   -^*/
	"Original design by Chris Sawyer",
	"Original graphics by Simon Foster",
	"",
	"The OpenTTD team (in alphabetical order):",
	"  Jean-Francois Claeys (Belugas) - GUI, newindustries and more",
	"  Bjarni Corfitzen (Bjarni) - MacOSX port, coder and vehicles",
	"  Matthijs Kooijman (blathijs) - Pathfinder-guru, pool rework",
	"  Loïc Guilloux (glx) - General coding",
	"  Christoph Elsenhans (frosch) - General coding",
	"  Jaroslav Mazanec (KUDr) - YAPG (Yet Another Pathfinder God) ;)",
	"  Jonathan Coome (Maedhros) - High priest of the newGRF Temple",
	"  Attila Bán (MiHaMiX) - WebTranslator, Nightlies, Wiki and bugtracker host",
	"  Owen Rudge (orudge) - Forum host, OS/2 port",
	"  Peter Nelson (peter1138) - Spiritual descendant from newGRF gods",
	"  Remko Bijker (Rubidium) - Lead coder and way more",
	"  Benedikt Brüggemeier (skidd13) - Bug fixer and code reworker",
	"  Zdenek Sojka (SmatZ) - Bug finder and fixer",
	"",
	"Inactive Developers:",
	"  Victor Fischer (Celestar) - Programming everywhere you need him to",
	"  Tamás Faragó (Darkvater) - Ex-Lead coder",
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
	"  Petr Baudis (pasky) - Many patches, newGRF support",
	"  Stefan Meißner (sign_de) - For his work on the console",
	"  Simon Sasburg (HackyKid) - Many bugfixes he has blessed us with",
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
			WP(w, scroller_d).counter = 5;
			WP(w, scroller_d).height = w->height - 40;
			break;

		case WE_PAINT: {
			int y = WP(w, scroller_d).height;
			DrawWindowWidgets(w);

			/* Show original copyright and revision version */
			DrawStringCentered(210, 17, STR_00B6_ORIGINAL_COPYRIGHT, TC_FROMSTRING);
			DrawStringCentered(210, 17 + 10, STR_00B7_VERSION, TC_FROMSTRING);

			/* Show all scrolling credits */
			for (uint i = 0; i < lengthof(credits); i++) {
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

		case WE_TICK: // Timer to scroll the text and adjust the new top
			if (--WP(w, scroller_d).counter == 0) {
				WP(w, scroller_d).counter = 5;
				WP(w, scroller_d).height--;
				w->SetDirty();
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
	new Window(&_about_desc);
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
				if (_errmsg_message_1 != INVALID_STRING_ID) {
					DrawStringMultiCenter(
						120,
						30,
						_errmsg_message_1,
						w->width - 2);
				}
			} else {
				const Player *p = GetPlayer((PlayerID)GetDParamX(_errmsg_decode_params,2));
				DrawPlayerFace(p->face, p->player_color, 2, 16);

				DrawStringMultiCenter(
					214,
					(_errmsg_message_1 == INVALID_STRING_ID ? 65 : 45),
					_errmsg_message_2,
					w->width - 2);
				if (_errmsg_message_1 != INVALID_STRING_ID) {
					DrawStringMultiCenter(
						214,
						90,
						_errmsg_message_1,
						w->width - 2);
				}
			}

			/* Switch back to the normal text ref. stack for NewGRF texts */
			SwitchToNormalRefStack();
			break;

		case WE_MOUSELOOP:
			if (_right_button_down) delete w;
			break;

		case WE_100_TICKS:
			if (--_errmsg_duration == 0) delete w;
			break;

		case WE_DESTROY:
			SetRedErrorSquare(0);
			extern StringID _switch_mode_errorstr;
			_switch_mode_errorstr = INVALID_STRING_ID;
			break;

		case WE_KEYPRESS:
			if (e->we.keypress.keycode == WKC_SPACE) {
				/* Don't continue. */
				e->we.keypress.cont = false;
				delete w;
			}
			break;
	}
}

void ShowErrorMessage(StringID msg_1, StringID msg_2, int x, int y)
{
	DeleteWindowById(WC_ERRMSG, 0);

	if (msg_2 == STR_NULL) msg_2 = STR_EMPTY;

	_errmsg_message_1 = msg_1;
	_errmsg_message_2 = msg_2;
	CopyOutDParam(_errmsg_decode_params, 0, lengthof(_errmsg_decode_params));
	_errmsg_duration = _patches.errmsg_duration;
	if (!_errmsg_duration) return;

	Point pt;
	const ViewPort *vp;
	Window *w;

	if (_errmsg_message_1 != STR_013B_OWNED_BY || GetDParamX(_errmsg_decode_params,2) >= 8) {
		if ((x | y) != 0) {
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
		w = new Window(pt.x, pt.y, 240, 46, ErrmsgWndProc, WC_ERRMSG, _errmsg_widgets);
	} else {
		if ((x | y) != 0) {
			pt = RemapCoords2(x, y);
			vp = FindWindowById(WC_MAIN_WINDOW, 0)->viewport;
			pt.x = Clamp(UnScaleByZoom(pt.x - vp->virtual_left, vp->zoom) + vp->left - (334/2), 0, _screen.width - 334);
			pt.y = Clamp(UnScaleByZoom(pt.y - vp->virtual_top, vp->zoom) + vp->top - (137/2), 22, _screen.height - 137);
		} else {
			pt.x = (_screen.width - 334) >> 1;
			pt.y = (_screen.height - 137) >> 1;
		}
		w = new Window(pt.x, pt.y, 334, 137, ErrmsgWndProc, WC_ERRMSG, _errmsg_face_widgets);
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
	Point pt = RemapCoords(x,y,z);
	StringID msg = STR_0801_COST;

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

struct TooltipsWindow : public Window
{
	StringID string_id;
	byte paramcount;
	uint64 params[5];

	TooltipsWindow(int x, int y, int width, int height, const Widget *widget,
								 StringID str, uint paramcount, const uint64 params[]) :
			Window(x, y, width, height, NULL, WC_TOOLTIPS, widget)
	{
		this->string_id = str;
		assert(sizeof(this->params[0]) == sizeof(params[0]));
		memcpy(this->params, params, sizeof(this->params[0]) * paramcount);
		this->paramcount = paramcount;

		this->flags4 &= ~WF_WHITE_BORDER_MASK; // remove white-border from tooltip
		this->widget[0].right = width;
		this->widget[0].bottom = height;

		FindWindowPlacementAndResize(width, height);
	}

	virtual void OnPaint()
	{
		GfxFillRect(0, 0, this->width - 1, this->height - 1, 0);
		GfxFillRect(1, 1, this->width - 2, this->height - 2, 0x44);

		for (uint arg = 0; arg < this->paramcount; arg++) {
			SetDParam(arg, this->params[arg]);
		}
		DrawStringMultiCenter((this->width >> 1), (this->height >> 1) - 5, this->string_id, this->width - 2);
	}

	virtual void OnMouseLoop()
	{
		/* We can show tooltips while dragging tools. These are shown as long as
		 * we are dragging the tool. Normal tooltips work with rmb */
		if (this->paramcount == 0 ) {
			if (!_right_button_down) delete this;
		} else {
			if (!_left_button_down) delete this;
		}
	}
};

/** Shows a tooltip
 * @param str String to be displayed
 * @param paramcount number of params to deal with
 * @param params (optional) up to 5 pieces of additional information that may be
 * added to a tooltip; currently only supports parameters of {NUM} (integer) */
void GuiShowTooltipsWithArgs(StringID str, uint paramcount, const uint64 params[])
{
	DeleteWindowById(WC_TOOLTIPS, 0);

	/* We only show measurement tooltips with patch setting on */
	if (str == STR_NULL || (paramcount != 0 && !_patches.measure_tooltip)) return;

	for (uint i = 0; i != paramcount; i++) SetDParam(i, params[i]);
	char buffer[512];
	GetString(buffer, str, lastof(buffer));

	Dimension br = GetStringBoundingBox(buffer);
	br.width += 6; br.height += 4; // increase slightly to have some space around the box

	/* Cut tooltip length to 200 pixels max, wrap to new line if longer */
	if (br.width > 200) {
		br.height += ((br.width - 4) / 176) * 10;
		br.width = 200;
	}

	/* Correctly position the tooltip position, watch out for window and cursor size
	 * Clamp value to below main toolbar and above statusbar. If tooltip would
	 * go below window, flip it so it is shown above the cursor */
	int y = Clamp(_cursor.pos.y + _cursor.size.y + _cursor.offs.y + 5, 22, _screen.height - 12);
	if (y + br.height > _screen.height - 12) y = _cursor.pos.y + _cursor.offs.y - br.height - 5;
	int x = Clamp(_cursor.pos.x - (br.width >> 1), 0, _screen.width - br.width);

	new TooltipsWindow(x, y, br.width, br.height, _tooltips_widgets, str, paramcount, params);
}


static int DrawStationCoverageText(const AcceptedCargo cargo,
	int str_x, int str_y, StationCoverageType sct, bool supplies)
{
	bool first = true;

	char *b = InlineString(_userstring, supplies ? STR_SUPPLIES : STR_000D_ACCEPTS);

	for (CargoID i = 0; i < NUM_CARGO; i++) {
		if (b >= lastof(_userstring) - (1 + 2 * 4)) break; // ',' or ' ' and two calls to Utf8Encode()
		switch (sct) {
			case SCT_PASSENGERS_ONLY: if (!IsCargoInClass(i, CC_PASSENGERS)) continue; break;
			case SCT_NON_PASSENGERS_ONLY: if (IsCargoInClass(i, CC_PASSENGERS)) continue; break;
			case SCT_ALL: break;
			default: NOT_REACHED();
		}
		if (cargo[i] >= (supplies ? 1U : 8U)) {
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

	/* Make sure we detect any buffer overflow */
	assert(b < endof(_userstring));

	return DrawStringMultiLine(str_x, str_y, STR_SPEC_USERSTRING, 144);
}

/**
 * Calculates and draws the accepted or supplied cargo around the selected tile(s)
 * @param sx x position where the string is to be drawn
 * @param sy y position where the string is to be drawn
 * @param sct which type of cargo is to be displayed (passengers/non-passengers)
 * @param rad radius around selected tile(s) to be searched
 * @param supplies if supplied cargos should be drawn, else accepted cargos
 * @return Returns the y value below the string that was drawn
 */
int DrawStationCoverageAreaText(int sx, int sy, StationCoverageType sct, int rad, bool supplies)
{
	TileIndex tile = TileVirtXY(_thd.pos.x, _thd.pos.y);
	AcceptedCargo cargo;
	if (tile < MapSize()) {
		if (supplies) {
			GetProductionAroundTiles(cargo, tile, _thd.size.x / TILE_SIZE, _thd.size.y / TILE_SIZE , rad);
		} else {
			GetAcceptanceAroundTiles(cargo, tile, _thd.size.x / TILE_SIZE, _thd.size.y / TILE_SIZE , rad);
		}
		return sy + DrawStationCoverageText(cargo, sx, sy, sct, supplies);
	}

	return sy;
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
	char *s = tb->buf + tb->caretpos;

	if (backspace) s = Utf8PrevChar(s);

	size_t len = Utf8Decode(&c, s);
	uint width = GetCharacterWidth(FS_NORMAL, c);

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

		default:
			break;
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

int QueryString::HandleEditBoxKey(Window *w, int wid, uint16 key, uint16 keycode, bool &cont)
{
	cont = false;

	switch (keycode) {
		case WKC_ESC: return 2;

		case WKC_RETURN: case WKC_NUM_ENTER: return 1;

		case (WKC_CTRL | 'V'):
			if (InsertTextBufferClipboard(&this->text)) w->InvalidateWidget(wid);
			break;

		case (WKC_CTRL | 'U'):
			DeleteTextBufferAll(&this->text);
			w->InvalidateWidget(wid);
			break;

		case WKC_BACKSPACE: case WKC_DELETE:
			if (DeleteTextBufferChar(&this->text, keycode)) w->InvalidateWidget(wid);
			break;

		case WKC_LEFT: case WKC_RIGHT: case WKC_END: case WKC_HOME:
			if (MoveTextBufferPos(&this->text, keycode)) w->InvalidateWidget(wid);
			break;

		default:
			if (IsValidChar(key, this->afilter)) {
				if (InsertTextBufferChar(&this->text, key)) w->InvalidateWidget(wid);
			} else { // key wasn't caught. Continue only if standard entry specified
				cont = (this->afilter == CS_ALPHANUMERAL);
			}
	}

	return 0;
}

void QueryString::HandleEditBox(Window *w, int wid)
{
	if (HandleCaret(&this->text)) w->InvalidateWidget(wid);
}

void QueryString::DrawEditBox(Window *w, int wid)
{
	const Widget *wi = &w->widget[wid];

	assert((wi->type & WWT_MASK) == WWT_EDITBOX);

	GfxFillRect(wi->left + 1, wi->top + 1, wi->right - 1, wi->bottom - 1, 215);

	DrawPixelInfo dpi;
	int delta;

	/* Limit the drawing of the string inside the widget boundaries */
	if (!FillDrawPixelInfo(&dpi,
			wi->left + 4,
			wi->top + 1,
			wi->right - wi->left - 4,
			wi->bottom - wi->top - 1)) {
		return;
	}

	DrawPixelInfo *old_dpi = _cur_dpi;
	_cur_dpi = &dpi;

	/* We will take the current widget length as maximum width, with a small
	 * space reserved at the end for the caret to show */
	const Textbuf *tb = &this->text;

	delta = (wi->right - wi->left) - tb->width - 10;
	if (delta > 0) delta = 0;

	if (tb->caretxoffs + delta < 0) delta = -tb->caretxoffs;

	DoDrawString(tb->buf, delta, 0, TC_YELLOW);
	if (tb->caret) DoDrawString("_", tb->caretxoffs + delta, 0, TC_WHITE);

	_cur_dpi = old_dpi;
}

int QueryStringBaseWindow::HandleEditBoxKey(int wid, uint16 key, uint16 keycode, bool &cont)
{
	return this->QueryString::HandleEditBoxKey(this, wid, key, keycode, cont);
}

void QueryStringBaseWindow::HandleEditBox(int wid)
{
	this->QueryString::HandleEditBox(this, wid);
}

void QueryStringBaseWindow::DrawEditBox(int wid)
{
	this->QueryString::DrawEditBox(this, wid);
}

enum QueryStringWidgets {
	QUERY_STR_WIDGET_TEXT = 3,
	QUERY_STR_WIDGET_CANCEL,
	QUERY_STR_WIDGET_OK
};


struct QueryStringWindow : public QueryStringBaseWindow
{
	Window *parent;

	QueryStringWindow(const WindowDesc *desc, Window *parent) : QueryStringBaseWindow(desc), parent(parent)
	{
		SetBit(_no_scroll, SCROLL_EDIT);

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		SetDParam(0, this->caption);
		DrawWindowWidgets(this);

		this->DrawEditBox(QUERY_STR_WIDGET_TEXT);
	}

	void OnOk()
	{
		if (this->orig == NULL || strcmp(this->text.buf, this->orig) != 0) {
			/* If the parent is NULL, the editbox is handled by general function
				* HandleOnEditText */
			if (this->parent != NULL) {
				this->parent->OnQueryTextFinished(this->text.buf);
			} else {
				HandleOnEditText(this->text.buf);
			}
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case QUERY_STR_WIDGET_TEXT:
				ShowOnScreenKeyboard(this, QUERY_STR_WIDGET_TEXT, QUERY_STR_WIDGET_CANCEL, QUERY_STR_WIDGET_OK);
				break;

			case QUERY_STR_WIDGET_OK:
				this->OnOk();
				/* Fallthrough */
			case QUERY_STR_WIDGET_CANCEL:
				delete this;
				break;
		}
	}

	virtual void OnMouseLoop()
	{
		this->HandleEditBox(QUERY_STR_WIDGET_TEXT);
	}

	virtual bool OnKeyPress(uint16 key, uint16 keycode)
	{
		bool cont;
		switch (this->HandleEditBoxKey(QUERY_STR_WIDGET_TEXT, key, keycode, cont)) {
			case 1: this->OnOk(); // Enter pressed, confirms change
			/* FALL THROUGH */
			case 2: delete this; break; // ESC pressed, closes window, abandons changes
		}
		return cont;
	}

	~QueryStringWindow()
	{
		if (!this->handled && this->parent != NULL) {
			this->handled = true;
			this->parent->OnQueryTextFinished(NULL);
		}
		ClrBit(_no_scroll, SCROLL_EDIT);
	}
};

static const Widget _query_string_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,        STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   259,     0,    13, STR_012D,        STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   259,    14,    29, 0x0,             STR_NULL},
{    WWT_EDITBOX,   RESIZE_NONE,    14,     2,   257,    16,    27, 0x0,             STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,   129,    30,    41, STR_012E_CANCEL, STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   130,   259,    30,    41, STR_012F_OK,     STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _query_string_desc = {
	190, 219, 260, 42, 260, 42,
	WC_QUERY_STRING, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_query_string_widgets,
	NULL
};

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
	uint realmaxlen = maxlen & ~0x1000;

	DeleteWindowById(WC_QUERY_STRING, 0);
	DeleteWindowById(WC_SAVELOAD, 0);

	QueryStringWindow *w = new QueryStringWindow(&_query_string_desc, parent);

	assert(realmaxlen < lengthof(w->edit_str_buf));

	GetString(w->edit_str_buf, str, lastof(w->edit_str_buf));
	w->edit_str_buf[realmaxlen - 1] = '\0';

	if (maxlen & 0x1000) {
		w->orig = NULL;
	} else {
		strecpy(w->orig_str_buf, w->edit_str_buf, lastof(w->orig_str_buf));
		w->orig = w->orig_str_buf;
	}

	w->LowerWidget(QUERY_STR_WIDGET_TEXT);
	w->caption = caption;
	w->afilter = afilter;
	InitializeTextBuffer(&w->text, w->edit_str_buf, realmaxlen, maxwidth);
}


enum QueryWidgets {
	QUERY_WIDGET_CAPTION = 1,
	QUERY_WIDGET_NO = 3,
	QUERY_WIDGET_YES
};

/**
 * Window used for asking the user a YES/NO question.
 */
struct QueryWindow : public Window {
	void (*proc)(Window*, bool); ///< callback function executed on closing of popup. Window* points to parent, bool is true if 'yes' clicked, false otherwise
	uint64 params[10];           ///< local copy of _decode_parameters
	StringID message;            ///< message shown for query window

	QueryWindow(const WindowDesc *desc, StringID caption, StringID message, Window *parent, void (*callback)(Window*, bool)) : Window(desc)
	{
		if (parent == NULL) parent = FindWindowById(WC_MAIN_WINDOW, 0);
		this->parent = parent;
		this->left = parent->left + (parent->width / 2) - (this->width / 2);
		this->top = parent->top + (parent->height / 2) - (this->height / 2);

		/* Create a backup of the variadic arguments to strings because it will be
		* overridden pretty often. We will copy these back for drawing */
		CopyOutDParam(this->params, 0, lengthof(this->params));
		this->widget[QUERY_WIDGET_CAPTION].data = caption;
		this->message    = message;
		this->proc       = callback;

		this->FindWindowPlacementAndResize(desc);
	}

	~QueryWindow()
	{
		if (this->proc != NULL) this->proc(this->parent, false);
	}

	virtual void OnPaint()
	{
		CopyInDParam(0, this->params, lengthof(this->params));
		DrawWindowWidgets(this);
		CopyInDParam(0, this->params, lengthof(this->params));

		DrawStringMultiCenter(this->width / 2, (this->height / 2) - 10, this->message, this->width - 2);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case QUERY_WIDGET_YES:
				if (this->proc != NULL) {
					this->proc(this->parent, true);
					this->proc = NULL;
				}
				/* Fallthrough */
			case QUERY_WIDGET_NO:
				delete this;
				break;
		}
	}

	virtual bool OnKeyPress(uint16 key, uint16 keycode)
	{
		/* ESC closes the window, Enter confirms the action */
		switch (keycode) {
			case WKC_RETURN:
			case WKC_NUM_ENTER:
				if (this->proc != NULL) {
					this->proc(this->parent, true);
					this->proc = NULL;
				}
				/* Fallthrough */
			case WKC_ESC:
				delete this;
				return false;
		}
		return true;
	}
};


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
	NULL
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
	new QueryWindow(&_query_desc, caption, message, parent, callback);
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
{    WWT_EDITBOX,    RESIZE_RTB,    14,     2,   254,   154,   165, STR_SAVE_OSKTITLE, STR_400B_CURRENTLY_SELECTED_NAME},
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

	/* Directories are always above the files (FIOS_TYPE_DIR)
	 * Drives (A:\ (windows only) are always under the files (FIOS_TYPE_DRIVE)
	 * Only sort savegames/scenarios, not directories
	 */
	for (int i = 0; i < _fios_num; i++) {
		switch (_fios_list[i].type) {
			case FIOS_TYPE_DIR:    sort_start++; break;
			case FIOS_TYPE_PARENT: sort_start++; break;
			case FIOS_TYPE_DRIVE:  sort_end++;   break;
			default: break;
		}
	}

	uint s_amount = _fios_num - sort_start - sort_end;
	if (s_amount > 0) {
		qsort(_fios_list + sort_start, s_amount, sizeof(FiosItem), compare_FiosItems);
	}
}

extern void StartupEngines();

struct SaveLoadWindow : public QueryStringBaseWindow {
	FiosItem o_dir;

	void GenerateFileName()
	{
		/* Check if we are not a spectator who wants to generate a name..
				Let's use the name of player #0 for now. */
		const Player *p = GetPlayer(IsValidPlayer(_local_player) ? _local_player : PLAYER_FIRST);

		SetDParam(0, p->index);
		SetDParam(1, _date);
		GetString(this->edit_str_buf, STR_4004, lastof(this->edit_str_buf));
		SanitizeFilename(this->edit_str_buf);
	}

	SaveLoadWindow(const WindowDesc *desc, SaveLoadDialogMode mode) : QueryStringBaseWindow(desc)
	{
		static const StringID saveload_captions[] = {
			STR_4001_LOAD_GAME,
			STR_0298_LOAD_SCENARIO,
			STR_4000_SAVE_GAME,
			STR_0299_SAVE_SCENARIO,
			STR_LOAD_HEIGHTMAP,
		};

		SetObjectToPlace(SPR_CURSOR_ZZZ, PAL_NONE, VHM_NONE, WC_MAIN_WINDOW, 0);
		SetBit(_no_scroll, SCROLL_SAVE);

		/* Use an array to define what will be the current file type being handled
		 * by current file mode */
		switch (mode) {
			case SLD_SAVE_GAME:     this->GenerateFileName(); break;
			case SLD_SAVE_SCENARIO: strcpy(this->edit_str_buf, "UNNAMED"); break;
			default:                break;
		}

		assert((uint)mode < lengthof(saveload_captions));

		this->widget[1].data = saveload_captions[mode];
		this->LowerWidget(7);

		this->afilter = CS_ALPHANUMERAL;
		InitializeTextBuffer(&this->text, this->edit_str_buf, lengthof(this->edit_str_buf), 240);

		/* pause is only used in single-player, non-editor mode, non-menu mode. It
		* will be unpaused in the WE_DESTROY event handler. */
		if (_game_mode != GM_MENU && !_networking && _game_mode != GM_EDITOR) {
			if (_pause_game >= 0) DoCommandP(0, 1, 0, NULL, CMD_PAUSE);
		}

		BuildFileList();

		ResetObjectToPlace();

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

		this->vscroll.cap = 10;
		this->resize.step_width = 2;
		this->resize.step_height = 10;

		this->FindWindowPlacementAndResize(desc);
	}

	virtual ~SaveLoadWindow()
	{
		/* pause is only used in single-player, non-editor mode, non menu mode */
		if (!_networking && _game_mode != GM_EDITOR && _game_mode != GM_MENU) {
			if (_pause_game >= 0) DoCommandP(0, 0, 0, NULL, CMD_PAUSE);
		}
		FiosFreeSavegameList();
		ClrBit(_no_scroll, SCROLL_SAVE);
	}

	virtual void OnPaint()
	{
		int pos;
		int y;

		SetVScrollCount(this, _fios_num);
		DrawWindowWidgets(this);
		DrawFiosTexts(this->width);

		if (_savegame_sort_dirty) {
			_savegame_sort_dirty = false;
			MakeSortedSaveGameList();
		}

		GfxFillRect(this->widget[7].left + 1, this->widget[7].top + 1, this->widget[7].right, this->widget[7].bottom, 0xD7);
		DrawSortButtonState(this, _savegame_sort_order & SORT_BY_NAME ? 2 : 3, _savegame_sort_order & SORT_DESCENDING ? SBS_DOWN : SBS_UP);

		y = this->widget[7].top + 1;
		for (pos = this->vscroll.pos; pos < _fios_num; pos++) {
			const FiosItem *item = _fios_list + pos;

			DoDrawStringTruncated(item->title, 4, y, _fios_colors[item->type], this->width - 18);
			y += 10;
			if (y >= this->vscroll.cap * 10 + this->widget[7].top + 1) break;
		}

		if (_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO) {
			this->DrawEditBox(10);
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case 2: // Sort save names by name
				_savegame_sort_order = (_savegame_sort_order == SORT_BY_NAME) ?
					SORT_BY_NAME | SORT_DESCENDING : SORT_BY_NAME;
				_savegame_sort_dirty = true;
				this->SetDirty();
				break;

			case 3: // Sort save names by date
				_savegame_sort_order = (_savegame_sort_order == SORT_BY_DATE) ?
					SORT_BY_DATE | SORT_DESCENDING : SORT_BY_DATE;
				_savegame_sort_dirty = true;
				this->SetDirty();
				break;

			case 6: // OpenTTD 'button', jumps to OpenTTD directory
				FiosBrowseTo(&o_dir);
				this->SetDirty();
				BuildFileList();
				break;

			case 7: { // Click the listbox
				int y = (pt.y - this->widget[widget].top - 1) / 10;
				char *name;
				const FiosItem *file;

				if (y < 0 || (y += this->vscroll.pos) >= this->vscroll.count) return;

				file = _fios_list + y;

				name = FiosBrowseTo(file);
				if (name != NULL) {
					if (_saveload_mode == SLD_LOAD_GAME || _saveload_mode == SLD_LOAD_SCENARIO) {
						_switch_mode = (_game_mode == GM_EDITOR) ? SM_LOAD_SCENARIO : SM_LOAD;

						SetFiosType(file->type);
						ttd_strlcpy(_file_to_saveload.name, name, sizeof(_file_to_saveload.name));
						ttd_strlcpy(_file_to_saveload.title, file->title, sizeof(_file_to_saveload.title));

						delete this;
					} else if (_saveload_mode == SLD_LOAD_HEIGHTMAP) {
						SetFiosType(file->type);
						ttd_strlcpy(_file_to_saveload.name, name, sizeof(_file_to_saveload.name));
						ttd_strlcpy(_file_to_saveload.title, file->title, sizeof(_file_to_saveload.title));

						delete this;
						ShowHeightmapLoad();
					} else {
						/* SLD_SAVE_GAME, SLD_SAVE_SCENARIO copy clicked name to editbox */
						ttd_strlcpy(this->text.buf, file->title, this->text.maxlength);
						UpdateTextBufferSize(&this->text);
						this->InvalidateWidget(10);
					}
				} else {
					/* Changed directory, need repaint. */
					this->SetDirty();
					BuildFileList();
				}
				break;
			}

			case 10: // edit box
				ShowOnScreenKeyboard(this, widget, 0, 0);
				break;

			case 11: case 12: // Delete, Save game
				break;
		}
	}

	virtual void OnMouseLoop()
	{
		if (_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO) {
			this->HandleEditBox(10);
		}
	}

	virtual bool OnKeyPress(uint16 key, uint16 keycode)
	{
		if (keycode == WKC_ESC) {
			delete this;
			return false;
		}

		bool cont = true;
		if ((_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO) &&
				this->HandleEditBoxKey(10, key, keycode, cont) == 1) { // Press Enter
			this->HandleButtonClick(12);
		}

		return cont;
	}

	virtual void OnTimeout()
	{
		/* This test protects against using widgets 11 and 12 which are only available
		 * in those two saveload mode  */
		if (!(_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO)) return;

		if (this->IsWidgetLowered(11)) { // Delete button clicked
			if (!FiosDelete(this->text.buf)) {
				ShowErrorMessage(INVALID_STRING_ID, STR_4008_UNABLE_TO_DELETE_FILE, 0, 0);
			} else {
				BuildFileList();
				/* Reset file name to current date on successful delete */
				if (_saveload_mode == SLD_SAVE_GAME) GenerateFileName();
			}

			UpdateTextBufferSize(&this->text);
			this->SetDirty();
		} else if (this->IsWidgetLowered(12)) { // Save button clicked
			_switch_mode = SM_SAVE;
			FiosMakeSavegameName(_file_to_saveload.name, this->text.buf, sizeof(_file_to_saveload.name));

			/* In the editor set up the vehicle engines correctly (date might have changed) */
			if (_game_mode == GM_EDITOR) StartupEngines();
		}
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		/* Widget 2 and 3 have to go with halve speed, make it so obiwan */
		uint diff = delta.x / 2;
		this->widget[2].right += diff;
		this->widget[3].left  += diff;
		this->widget[3].right += delta.x;

		/* Same for widget 11 and 12 in save-dialog */
		if (_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO) {
			this->widget[11].right += diff;
			this->widget[12].left  += diff;
			this->widget[12].right += delta.x;
		}

		this->vscroll.cap += delta.y / 10;
	}
};

static const WindowDesc _load_dialog_desc = {
	WDP_CENTER, WDP_CENTER, 257, 154, 257, 294,
	WC_SAVELOAD, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_load_dialog_widgets,
	NULL,
};

static const WindowDesc _save_dialog_desc = {
	WDP_CENTER, WDP_CENTER, 257, 180, 257, 320,
	WC_SAVELOAD, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_save_dialog_widgets,
	NULL,
};

/** These values are used to convert the file/operations mode into a corresponding file type.
 * So each entry, as expressed by the related comment, is based on the enum   */
static const FileType _file_modetotype[] = {
	FT_SAVEGAME,  ///< used for SLD_LOAD_GAME
	FT_SCENARIO,  ///< used for SLD_LOAD_SCENARIO
	FT_SAVEGAME,  ///< used for SLD_SAVE_GAME
	FT_SCENARIO,  ///< used for SLD_SAVE_SCENARIO
	FT_HEIGHTMAP, ///< used for SLD_LOAD_HEIGHTMAP
	FT_SAVEGAME,  ///< SLD_NEW_GAME
};

void ShowSaveLoadDialog(SaveLoadDialogMode mode)
{
	DeleteWindowById(WC_QUERY_STRING, 0);
	DeleteWindowById(WC_SAVELOAD, 0);

	const WindowDesc *sld;
	switch (mode) {
		case SLD_SAVE_GAME:
		case SLD_SAVE_SCENARIO:
			sld = &_save_dialog_desc; break;
		default:
			sld = &_load_dialog_desc; break;
	}

	_saveload_mode = mode;
	_file_to_saveload.filetype = _file_modetotype[mode];

	new SaveLoadWindow(sld, mode);
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
