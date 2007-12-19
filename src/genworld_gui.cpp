/* $Id$ */

/** @file genworld_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "heightmap.h"
#include "functions.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "gfx.h"
#include "tile.h"
#include "strings.h"
#include "gfxinit.h"
#include "player.h"
#include "command.h"
#include "sound.h"
#include "variables.h"
#include "string.h"
#include "settings.h"
#include "debug.h"
#include "genworld.h"
#include "network/network.h"
#include "thread.h"
#include "date.h"
#include "newgrf_config.h"

/**
 * In what 'mode' the GenerateLandscapeWindowProc is.
 */
enum glwp_modes {
	GLWP_GENERATE,
	GLWP_HEIGHTMAP,
	GLWP_SCENARIO,
	GLWP_END
};

struct generate_d {
	uint widget_id;
	uint x;
	uint y;
	char name[64];
};

extern void SwitchMode(int new_mode);

static inline void SetNewLandscapeType(byte landscape)
{
	_opt_newgame.landscape = landscape;
	InvalidateWindowClasses(WC_SELECT_GAME);
	InvalidateWindowClasses(WC_GENERATE_LANDSCAPE);
}

enum GenerateLandscapeWindowWidgets {
	GLAND_TEMPERATE = 3,
	GLAND_ARCTIC,
	GLAND_TROPICAL,
	GLAND_TOYLAND,

	GLAND_MAPSIZE_X_TEXT,
	GLAND_MAPSIZE_X_PULLDOWN,
	GLAND_MAPSIZE_Y_TEXT,
	GLAND_MAPSIZE_Y_PULLDOWN,

	GLAND_TOWN_TEXT,
	GLAND_TOWN_PULLDOWN,
	GLAND_INDUSTRY_TEXT,
	GLAND_INDUSTRY_PULLDOWN,

	GLAND_RANDOM_EDITBOX,
	GLAND_RANDOM_BUTTON,

	GLAND_GENERATE_BUTTON,

	GLAND_START_DATE_DOWN,
	GLAND_START_DATE_TEXT,
	GLAND_START_DATE_UP,

	GLAND_SNOW_LEVEL_DOWN,
	GLAND_SNOW_LEVEL_TEXT,
	GLAND_SNOW_LEVEL_UP,

	GLAND_TREE_TEXT,
	GLAND_TREE_PULLDOWN,
	GLAND_LANDSCAPE_TEXT,
	GLAND_LANDSCAPE_PULLDOWN,
	GLAND_HEIGHTMAP_ROTATION_TEXT = GLAND_LANDSCAPE_TEXT,
	GLAND_HEIGHTMAP_ROTATION_PULLDOWN = GLAND_LANDSCAPE_PULLDOWN,

	GLAND_TERRAIN_TEXT,
	GLAND_TERRAIN_PULLDOWN,
	GLAND_WATER_TEXT,
	GLAND_WATER_PULLDOWN,
	GLAND_SMOOTHNESS_TEXT,
	GLAND_SMOOTHNESS_PULLDOWN
};

static const Widget _generate_landscape_widgets[] = {
{  WWT_CLOSEBOX,  RESIZE_NONE, 13,   0,  10,   0,  13, STR_00C5,                     STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION, RESIZE_NONE, 13,  11, 337,   0,  13, STR_WORLD_GENERATION_CAPTION, STR_NULL},
{      WWT_PANEL, RESIZE_NONE, 13,   0, 337,  14, 267, 0x0,                          STR_NULL},

{   WWT_IMGBTN_2, RESIZE_NONE, 12,  10,  86,  24,  78, SPR_SELECT_TEMPERATE,         STR_030E_SELECT_TEMPERATE_LANDSCAPE},
{   WWT_IMGBTN_2, RESIZE_NONE, 12,  90, 166,  24,  78, SPR_SELECT_SUB_ARCTIC,        STR_030F_SELECT_SUB_ARCTIC_LANDSCAPE},
{   WWT_IMGBTN_2, RESIZE_NONE, 12, 170, 246,  24,  78, SPR_SELECT_SUB_TROPICAL,      STR_0310_SELECT_SUB_TROPICAL_LANDSCAPE},
{   WWT_IMGBTN_2, RESIZE_NONE, 12, 250, 326,  24,  78, SPR_SELECT_TOYLAND,           STR_0311_SELECT_TOYLAND_LANDSCAPE},

{      WWT_PANEL, RESIZE_NONE, 12, 114, 149,  90, 101, 0x0,                          STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 150, 161,  90, 101, STR_0225,                     STR_NULL}, // Mapsize X
{      WWT_PANEL, RESIZE_NONE, 12, 180, 215,  90, 101, 0x0,                          STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 216, 227,  90, 101, STR_0225,                     STR_NULL}, // Mapsize Y

{      WWT_PANEL, RESIZE_NONE, 12, 114, 163, 112, 123, 0x0,                          STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 164, 175, 112, 123, STR_0225,                     STR_NULL}, // Number of towns
{      WWT_PANEL, RESIZE_NONE, 12, 114, 163, 130, 141, 0x0,                          STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 164, 175, 130, 141, STR_0225,                     STR_NULL}, // Number of industries

{      WWT_PANEL, RESIZE_NONE, 15, 114, 207, 152, 163, 0x0,                          STR_RANDOM_SEED_HELP}, // Edit box for seed
{    WWT_TEXTBTN, RESIZE_NONE, 12, 216, 326, 152, 163, STR_RANDOM,                   STR_RANDOM_HELP},

{    WWT_TEXTBTN, RESIZE_NONE,  6, 243, 326, 228, 257, STR_GENERATE,                 STR_NULL}, // Generate button

{     WWT_IMGBTN, RESIZE_NONE, 12, 216, 227, 112, 123, SPR_ARROW_DOWN,               STR_029E_MOVE_THE_STARTING_DATE},
{      WWT_PANEL, RESIZE_NONE, 12, 228, 314, 112, 123, 0x0,                          STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 315, 326, 112, 123, SPR_ARROW_UP,                 STR_029F_MOVE_THE_STARTING_DATE},

{     WWT_IMGBTN, RESIZE_NONE, 12, 282, 293, 130, 141, SPR_ARROW_DOWN,               STR_SNOW_LINE_DOWN},
{      WWT_PANEL, RESIZE_NONE, 12, 294, 314, 130, 141, 0x0,                          STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 315, 326, 130, 141, SPR_ARROW_UP,                 STR_SNOW_LINE_UP},

{      WWT_PANEL, RESIZE_NONE, 12, 114, 219, 192, 203, 0x0,                          STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 220, 231, 192, 203, STR_0225,                     STR_NULL}, // Tree placer
{      WWT_PANEL, RESIZE_NONE, 12, 114, 219, 174, 185, 0x0,                          STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 220, 231, 174, 185, STR_0225,                     STR_NULL}, // Landscape generator
{      WWT_PANEL, RESIZE_NONE, 12, 114, 219, 210, 221, 0x0,                          STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 220, 231, 210, 221, STR_0225,                     STR_NULL}, // Terrain type
{      WWT_PANEL, RESIZE_NONE, 12, 113, 219, 228, 239, 0x0,                          STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 220, 231, 228, 239, STR_0225,                     STR_NULL}, // Water quantity
{      WWT_PANEL, RESIZE_NONE, 12, 113, 219, 246, 257, 0x0,                          STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 220, 231, 246, 257, STR_0225,                     STR_NULL}, // Map smoothness
{   WIDGETS_END},
};

static const Widget _heightmap_load_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE, 13,   0,  10,   0,  13, STR_00C5,                     STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION, RESIZE_NONE, 13,  11, 337,   0,  13, STR_WORLD_GENERATION_CAPTION, STR_NULL},
{      WWT_PANEL, RESIZE_NONE, 13,   0, 337,  14, 235, 0x0,                          STR_NULL},

{   WWT_IMGBTN_2, RESIZE_NONE, 12,  10,  86,  24,  78, SPR_SELECT_TEMPERATE,        STR_030E_SELECT_TEMPERATE_LANDSCAPE},
{   WWT_IMGBTN_2, RESIZE_NONE, 12,  90, 166,  24,  78, SPR_SELECT_SUB_ARCTIC,       STR_030F_SELECT_SUB_ARCTIC_LANDSCAPE},
{   WWT_IMGBTN_2, RESIZE_NONE, 12, 170, 246,  24,  78, SPR_SELECT_SUB_TROPICAL,     STR_0310_SELECT_SUB_TROPICAL_LANDSCAPE},
{   WWT_IMGBTN_2, RESIZE_NONE, 12, 250, 326,  24,  78, SPR_SELECT_TOYLAND,          STR_0311_SELECT_TOYLAND_LANDSCAPE},

{      WWT_PANEL, RESIZE_NONE, 12, 114, 149, 112, 123, 0x0,                          STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 150, 161, 112, 123, STR_0225,                     STR_NULL}, // Mapsize X
{      WWT_PANEL, RESIZE_NONE, 12, 180, 215, 112, 123, 0x0,                          STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 216, 227, 112, 123, STR_0225,                     STR_NULL}, // Mapsize Y

{      WWT_PANEL, RESIZE_NONE, 12, 114, 163, 134, 145, 0x0,                          STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 164, 175, 134, 145, STR_0225,                     STR_NULL}, // Number of towns
{      WWT_PANEL, RESIZE_NONE, 12, 114, 163, 152, 163, 0x0,                          STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 164, 175, 152, 163, STR_0225,                     STR_NULL}, // Number of industries

{      WWT_PANEL, RESIZE_NONE, 15, 114, 194, 174, 185, 0x0,                          STR_RANDOM_SEED_HELP}, // Edit box for seed
{    WWT_TEXTBTN, RESIZE_NONE, 12, 203, 285, 174, 185, STR_RANDOM,                   STR_RANDOM_HELP},

{    WWT_TEXTBTN, RESIZE_NONE,  6, 243, 326, 196, 225, STR_GENERATE,                 STR_NULL}, // Generate button

{     WWT_IMGBTN, RESIZE_NONE, 12, 216, 227, 134, 145, SPR_ARROW_DOWN,               STR_029E_MOVE_THE_STARTING_DATE},
{      WWT_PANEL, RESIZE_NONE, 12, 228, 314, 134, 145, 0x0,                          STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 315, 326, 134, 145, SPR_ARROW_UP,                 STR_029F_MOVE_THE_STARTING_DATE},

{     WWT_IMGBTN, RESIZE_NONE, 12, 282, 293, 152, 163, SPR_ARROW_DOWN,               STR_SNOW_LINE_DOWN},
{      WWT_PANEL, RESIZE_NONE, 12, 294, 314, 152, 163, 0x0,                          STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 315, 326, 152, 163, SPR_ARROW_UP,                 STR_SNOW_LINE_UP},

{      WWT_PANEL, RESIZE_NONE, 12, 114, 219, 196, 207, 0x0,                          STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 220, 231, 196, 207, STR_0225,                     STR_NULL}, // Tree placer

{      WWT_PANEL, RESIZE_NONE, 12, 114, 219, 214, 225, 0x0,                          STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 220, 231, 214, 225, STR_0225,                     STR_NULL}, // Heightmap rotation
{   WIDGETS_END},
};

void StartGeneratingLandscape(glwp_modes mode)
{
	DeleteAllNonVitalWindows();

	/* Copy all XXX_newgame to XXX when coming from outside the editor */
	UpdatePatches();
	_opt = _opt_newgame;
	_opt_ptr = &_opt;
	ResetGRFConfig(true);

	SndPlayFx(SND_15_BEEP);
	switch (mode) {
		case GLWP_GENERATE:  _switch_mode = (_game_mode == GM_EDITOR) ? SM_GENRANDLAND    : SM_NEWGAME;         break;
		case GLWP_HEIGHTMAP: _switch_mode = (_game_mode == GM_EDITOR) ? SM_LOAD_HEIGHTMAP : SM_START_HEIGHTMAP; break;
		case GLWP_SCENARIO:  _switch_mode = SM_EDITOR; break;
		default: NOT_REACHED();
	}
}

static void LandscapeGenerationCallback(Window *w, bool confirmed)
{
	if (confirmed) StartGeneratingLandscape((glwp_modes)w->window_number);
}

static void GenerateLandscapeWndProc(Window *w, WindowEvent *e)
{
	static const StringID mapsizes[]    = {STR_64, STR_128, STR_256, STR_512, STR_1024, STR_2048, INVALID_STRING_ID};
	static const StringID elevations[]  = {STR_682A_VERY_FLAT, STR_682B_FLAT, STR_682C_HILLY, STR_682D_MOUNTAINOUS, INVALID_STRING_ID};
	static const StringID sea_lakes[]   = {STR_VERY_LOW, STR_6820_LOW, STR_6821_MEDIUM, STR_6822_HIGH, INVALID_STRING_ID};
	static const StringID smoothness[]  = {STR_CONFIG_PATCHES_ROUGHNESS_OF_TERRAIN_VERY_SMOOTH, STR_CONFIG_PATCHES_ROUGHNESS_OF_TERRAIN_SMOOTH, STR_CONFIG_PATCHES_ROUGHNESS_OF_TERRAIN_ROUGH, STR_CONFIG_PATCHES_ROUGHNESS_OF_TERRAIN_VERY_ROUGH, INVALID_STRING_ID};
	static const StringID tree_placer[] = {STR_CONFIG_PATCHES_TREE_PLACER_NONE, STR_CONFIG_PATCHES_TREE_PLACER_ORIGINAL, STR_CONFIG_PATCHES_TREE_PLACER_IMPROVED, INVALID_STRING_ID};
	static const StringID rotation[]    = {STR_CONFIG_PATCHES_HEIGHTMAP_ROTATION_COUNTER_CLOCKWISE, STR_CONFIG_PATCHES_HEIGHTMAP_ROTATION_CLOCKWISE, INVALID_STRING_ID};
	static const StringID landscape[]   = {STR_CONFIG_PATCHES_LAND_GENERATOR_ORIGINAL, STR_CONFIG_PATCHES_LAND_GENERATOR_TERRA_GENESIS, INVALID_STRING_ID};
	static const StringID num_towns[]   = {STR_NUM_VERY_LOW, STR_6816_LOW, STR_6817_NORMAL, STR_6818_HIGH, INVALID_STRING_ID};
	static const StringID num_inds[]    = {STR_26816_NONE, STR_NUM_VERY_LOW, STR_6816_LOW, STR_6817_NORMAL, STR_6818_HIGH, INVALID_STRING_ID};

	/* Data used for the generate seed edit box */
	static querystr_d _genseed_query;
	static char _genseed_buffer[11];

	glwp_modes mode = (glwp_modes)w->window_number;
	uint y;

	switch (e->event) {
	case WE_CREATE:
		w->LowerWidget(_opt_newgame.landscape + GLAND_TEMPERATE);

		snprintf(_genseed_buffer, sizeof(_genseed_buffer), "%u", _patches_newgame.generation_seed);
		InitializeTextBuffer(&_genseed_query.text, _genseed_buffer, lengthof(_genseed_buffer), 120);
		_genseed_query.caption = STR_NULL;
		_genseed_query.afilter = CS_NUMERAL;
		break;

	case WE_PAINT:
		/* You can't select smoothness if not terragenesis */
		if (mode == GLWP_GENERATE) {
			w->SetWidgetDisabledState(GLAND_SMOOTHNESS_TEXT,     _patches_newgame.land_generator == 0);
			w->SetWidgetDisabledState(GLAND_SMOOTHNESS_PULLDOWN, _patches_newgame.land_generator == 0);
		}
		/* Disable snowline if not hilly */
		w->SetWidgetDisabledState(GLAND_SNOW_LEVEL_TEXT, _opt_newgame.landscape != LT_ARCTIC);
		/* Disable town, industry and trees in SE */
		w->SetWidgetDisabledState(GLAND_TOWN_TEXT,         _game_mode == GM_EDITOR);
		w->SetWidgetDisabledState(GLAND_TOWN_PULLDOWN,     _game_mode == GM_EDITOR);
		w->SetWidgetDisabledState(GLAND_INDUSTRY_TEXT,     _game_mode == GM_EDITOR);
		w->SetWidgetDisabledState(GLAND_INDUSTRY_PULLDOWN, _game_mode == GM_EDITOR);
		w->SetWidgetDisabledState(GLAND_TREE_TEXT,         _game_mode == GM_EDITOR);
		w->SetWidgetDisabledState(GLAND_TREE_PULLDOWN,     _game_mode == GM_EDITOR);

		w->SetWidgetDisabledState(GLAND_START_DATE_DOWN, _patches_newgame.starting_year <= MIN_YEAR);
		w->SetWidgetDisabledState(GLAND_START_DATE_UP,   _patches_newgame.starting_year >= MAX_YEAR);
		w->SetWidgetDisabledState(GLAND_SNOW_LEVEL_DOWN, _patches_newgame.snow_line_height <= 2 || _opt_newgame.landscape != LT_ARCTIC);
		w->SetWidgetDisabledState(GLAND_SNOW_LEVEL_UP,   _patches_newgame.snow_line_height >= MAX_SNOWLINE_HEIGHT || _opt_newgame.landscape != LT_ARCTIC);

		w->SetWidgetLoweredState(GLAND_TEMPERATE, _opt_newgame.landscape == LT_TEMPERATE);
		w->SetWidgetLoweredState(GLAND_ARCTIC,    _opt_newgame.landscape == LT_ARCTIC);
		w->SetWidgetLoweredState(GLAND_TROPICAL,  _opt_newgame.landscape == LT_TROPIC);
		w->SetWidgetLoweredState(GLAND_TOYLAND,   _opt_newgame.landscape == LT_TOYLAND);
		DrawWindowWidgets(w);

		y = (mode == GLWP_HEIGHTMAP) ? 22 : 0;

		DrawString( 12,  91 + y, STR_MAPSIZE, TC_FROMSTRING);
		DrawString(119,  91 + y, mapsizes[_patches_newgame.map_x - 6], TC_BLACK);
		DrawString(168,  91 + y, STR_BY, TC_FROMSTRING);
		DrawString(182,  91 + y, mapsizes[_patches_newgame.map_y - 6], TC_BLACK);

		DrawString( 12, 113 + y, STR_NUMBER_OF_TOWNS, TC_FROMSTRING);
		DrawString( 12, 131 + y, STR_NUMBER_OF_INDUSTRIES, TC_FROMSTRING);
		if (_game_mode == GM_EDITOR) {
			DrawString(118, 113 + y, STR_6836_OFF, TC_BLACK);
			DrawString(118, 131 + y, STR_6836_OFF, TC_BLACK);
		} else {
			DrawString(118, 113 + y, num_towns[_opt_newgame.diff.number_towns], TC_BLACK);
			DrawString(118, 131 + y, num_inds[_opt_newgame.diff.number_industries], TC_BLACK);
		}

		DrawString( 12, 153 + y, STR_RANDOM_SEED, TC_FROMSTRING);
		DrawEditBox(w, &_genseed_query, GLAND_RANDOM_EDITBOX);

		DrawString(182, 113 + y, STR_DATE, TC_FROMSTRING);
		SetDParam(0, ConvertYMDToDate(_patches_newgame.starting_year, 0, 1));
		DrawStringCentered(271, 113 + y, STR_GENERATE_DATE, TC_FROMSTRING);

		DrawString(182, 131 + y, STR_SNOW_LINE_HEIGHT, TC_FROMSTRING);
		SetDParam(0, _patches_newgame.snow_line_height);
		DrawStringCentered(303, 131 + y, STR_SNOW_LINE_HEIGHT_NUM, TC_BLACK);

		if (mode == GLWP_GENERATE) {
			DrawString( 12, 175, STR_LAND_GENERATOR, TC_FROMSTRING);
			DrawString(118, 175, landscape[_patches_newgame.land_generator], TC_BLACK);

			DrawString( 12, 193, STR_TREE_PLACER, TC_FROMSTRING);
			DrawString(118, 193, tree_placer[_patches_newgame.tree_placer], TC_BLACK);

			DrawString( 12, 211, STR_TERRAIN_TYPE, TC_FROMSTRING);
			DrawString(118, 211, elevations[_opt_newgame.diff.terrain_type], TC_BLACK);

			DrawString( 12, 229, STR_QUANTITY_OF_SEA_LAKES, TC_FROMSTRING);
			DrawString(118, 229, sea_lakes[_opt_newgame.diff.quantity_sea_lakes], TC_BLACK);

			DrawString( 12, 247, STR_SMOOTHNESS, TC_FROMSTRING);
			DrawString(118, 247, smoothness[_patches_newgame.tgen_smoothness], TC_BLACK);
		} else {
			char buffer[512];

			if (_patches_newgame.heightmap_rotation == HM_CLOCKWISE) {
				SetDParam(0, WP(w, generate_d).y);
				SetDParam(1, WP(w, generate_d).x);
			} else {
				SetDParam(0, WP(w, generate_d).x);
				SetDParam(1, WP(w, generate_d).y);
			}
			GetString(buffer, STR_HEIGHTMAP_SIZE, lastof(buffer));
			DrawStringRightAligned(326, 91, STR_HEIGHTMAP_SIZE, TC_BLACK);

			DrawString( 12,  91, STR_HEIGHTMAP_NAME, TC_BLACK);
			SetDParamStr(0, WP(w, generate_d).name);
			DrawStringTruncated(114,  91, STR_ORANGE, TC_BLACK, 326 - 114 - GetStringBoundingBox(buffer).width - 5);

			DrawString( 12, 197, STR_TREE_PLACER, TC_FROMSTRING);
			DrawString(118, 197, tree_placer[_patches_newgame.tree_placer], TC_BLACK);

			DrawString( 12, 215, STR_HEIGHTMAP_ROTATION, TC_FROMSTRING);
			DrawString(118, 215, rotation[_patches_newgame.heightmap_rotation], TC_BLACK);
		}

		break;
	case WE_CLICK:
		switch (e->we.click.widget) {
		case 0: DeleteWindow(w); break;
		case GLAND_TEMPERATE: case GLAND_ARCTIC: case GLAND_TROPICAL: case GLAND_TOYLAND:
			w->RaiseWidget(_opt_newgame.landscape + GLAND_TEMPERATE);
			SetNewLandscapeType(e->we.click.widget - GLAND_TEMPERATE);
			break;
		case GLAND_MAPSIZE_X_TEXT: case GLAND_MAPSIZE_X_PULLDOWN: // Mapsize X
			ShowDropDownMenu(w, mapsizes, _patches_newgame.map_x - 6, GLAND_MAPSIZE_X_PULLDOWN, 0, 0);
			break;
		case GLAND_MAPSIZE_Y_TEXT: case GLAND_MAPSIZE_Y_PULLDOWN: // Mapsize Y
			ShowDropDownMenu(w, mapsizes, _patches_newgame.map_y - 6, GLAND_MAPSIZE_Y_PULLDOWN, 0, 0);
			break;
		case GLAND_TOWN_TEXT: case GLAND_TOWN_PULLDOWN: // Number of towns
			ShowDropDownMenu(w, num_towns, _opt_newgame.diff.number_towns, GLAND_TOWN_PULLDOWN, 0, 0);
			break;
		case GLAND_INDUSTRY_TEXT: case GLAND_INDUSTRY_PULLDOWN: // Number of industries
			ShowDropDownMenu(w, num_inds, _opt_newgame.diff.number_industries, GLAND_INDUSTRY_PULLDOWN, 0, 0);
			break;
		case GLAND_RANDOM_BUTTON: // Random seed
			_patches_newgame.generation_seed = InteractiveRandom();
			snprintf(_genseed_buffer, lengthof(_genseed_buffer), "%u", _patches_newgame.generation_seed);
			UpdateTextBufferSize(&_genseed_query.text);
			SetWindowDirty(w);
			break;
		case GLAND_GENERATE_BUTTON: // Generate

			UpdatePatches();

			if (_patches.town_layout == TL_NO_ROADS) {
				ShowQuery(
					STR_TOWN_LAYOUT_WARNING_CAPTION,
					STR_TOWN_LAYOUT_WARNING_MESSAGE,
					w,
					LandscapeGenerationCallback);
			} else if (mode == GLWP_HEIGHTMAP &&
					(WP(w, generate_d).x * 2 < (1U << _patches_newgame.map_x) ||
					WP(w, generate_d).x / 2 > (1U << _patches_newgame.map_x) ||
					WP(w, generate_d).y * 2 < (1U << _patches_newgame.map_y) ||
					WP(w, generate_d).y / 2 > (1U << _patches_newgame.map_y))) {
				ShowQuery(
					STR_HEIGHTMAP_SCALE_WARNING_CAPTION,
					STR_HEIGHTMAP_SCALE_WARNING_MESSAGE,
					w,
					LandscapeGenerationCallback);

			} else {
				StartGeneratingLandscape(mode);
			}
			break;
		case GLAND_START_DATE_DOWN: case GLAND_START_DATE_UP: // Year buttons
			/* Don't allow too fast scrolling */
			if ((w->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
				w->HandleButtonClick(e->we.click.widget);
				SetWindowDirty(w);

				_patches_newgame.starting_year = Clamp(_patches_newgame.starting_year + e->we.click.widget - GLAND_START_DATE_TEXT, MIN_YEAR, MAX_YEAR);
			}
			_left_button_clicked = false;
			break;
		case GLAND_START_DATE_TEXT: // Year text
			WP(w, generate_d).widget_id = GLAND_START_DATE_TEXT;
			SetDParam(0, _patches_newgame.starting_year);
			ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_START_DATE_QUERY_CAPT, 8, 100, w, CS_NUMERAL);
			break;
		case GLAND_SNOW_LEVEL_DOWN: case GLAND_SNOW_LEVEL_UP: // Snow line buttons
			/* Don't allow too fast scrolling */
			if ((w->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
				w->HandleButtonClick(e->we.click.widget);
				SetWindowDirty(w);

				_patches_newgame.snow_line_height = Clamp(_patches_newgame.snow_line_height + e->we.click.widget - GLAND_SNOW_LEVEL_TEXT, 2, MAX_SNOWLINE_HEIGHT);
			}
			_left_button_clicked = false;
			break;
		case GLAND_SNOW_LEVEL_TEXT: // Snow line text
			WP(w, generate_d).widget_id = GLAND_SNOW_LEVEL_TEXT;
			SetDParam(0, _patches_newgame.snow_line_height);
			ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_SNOW_LINE_QUERY_CAPT, 3, 100, w, CS_NUMERAL);
			break;
		case GLAND_TREE_TEXT: case GLAND_TREE_PULLDOWN: // Tree placer
			ShowDropDownMenu(w, tree_placer, _patches_newgame.tree_placer, GLAND_TREE_PULLDOWN, 0, 0);
			break;
		case GLAND_LANDSCAPE_TEXT: case GLAND_LANDSCAPE_PULLDOWN: // Landscape generator OR Heightmap rotation
		/* case GLAND_HEIGHTMAP_ROTATION_TEXT: case GLAND_HEIGHTMAP_ROTATION_PULLDOWN:*/
			if (mode == GLWP_HEIGHTMAP) {
				ShowDropDownMenu(w, rotation, _patches_newgame.heightmap_rotation, GLAND_HEIGHTMAP_ROTATION_PULLDOWN, 0, 0);
			} else {
				ShowDropDownMenu(w, landscape, _patches_newgame.land_generator, GLAND_LANDSCAPE_PULLDOWN, 0, 0);
			}
			break;
		case GLAND_TERRAIN_TEXT: case GLAND_TERRAIN_PULLDOWN: // Terrain type
			ShowDropDownMenu(w, elevations, _opt_newgame.diff.terrain_type, GLAND_TERRAIN_PULLDOWN, 0, 0);
			break;
		case GLAND_WATER_TEXT: case GLAND_WATER_PULLDOWN: // Water quantity
			ShowDropDownMenu(w, sea_lakes, _opt_newgame.diff.quantity_sea_lakes, GLAND_WATER_PULLDOWN, 0, 0);
			break;
		case GLAND_SMOOTHNESS_TEXT: case GLAND_SMOOTHNESS_PULLDOWN: // Map smoothness
			ShowDropDownMenu(w, smoothness, _patches_newgame.tgen_smoothness, GLAND_SMOOTHNESS_PULLDOWN, 0, 0);
			break;
		}
		break;

	case WE_MOUSELOOP:
		HandleEditBox(w, &_genseed_query, GLAND_RANDOM_EDITBOX);
		break;

	case WE_KEYPRESS:
		HandleEditBoxKey(w, &_genseed_query, GLAND_RANDOM_EDITBOX, e);
		/* the seed is unsigned, therefore atoi cannot be used.
		 * As 2^32 - 1 (MAX_UVALUE(uint32)) is a 'magic' value
		 * (use random seed) it should not be possible to be
		 * entered into the input field; the generate seed
		 * button can be used instead. */
		_patches_newgame.generation_seed = minu(strtoul(_genseed_buffer, NULL, sizeof(_genseed_buffer) - 1), MAX_UVALUE(uint32) - 1);
		break;

	case WE_DROPDOWN_SELECT:
		switch (e->we.dropdown.button) {
			case GLAND_MAPSIZE_X_PULLDOWN:  _patches_newgame.map_x = e->we.dropdown.index + 6; break;
			case GLAND_MAPSIZE_Y_PULLDOWN:  _patches_newgame.map_y = e->we.dropdown.index + 6; break;
			case GLAND_TREE_PULLDOWN:       _patches_newgame.tree_placer = e->we.dropdown.index; break;
			case GLAND_SMOOTHNESS_PULLDOWN: _patches_newgame.tgen_smoothness = e->we.dropdown.index;  break;

			case GLAND_TOWN_PULLDOWN:
				_opt_newgame.diff.number_towns = e->we.dropdown.index;
				if (_opt_newgame.diff_level != 3) ShowErrorMessage(INVALID_STRING_ID, STR_DIFFICULTY_TO_CUSTOM, 0, 0);
				DoCommandP(0, 2, _opt_newgame.diff.number_towns, NULL, CMD_CHANGE_DIFFICULTY_LEVEL);
				break;
			case GLAND_INDUSTRY_PULLDOWN:
				_opt_newgame.diff.number_industries = e->we.dropdown.index;
				if (_opt_newgame.diff_level != 3) ShowErrorMessage(INVALID_STRING_ID, STR_DIFFICULTY_TO_CUSTOM, 0, 0);
				DoCommandP(0, 3, _opt_newgame.diff.number_industries, NULL, CMD_CHANGE_DIFFICULTY_LEVEL);
				break;
			case GLAND_LANDSCAPE_PULLDOWN:
			/* case GLAND_HEIGHTMAP_PULLDOWN: */
				if (mode == GLWP_HEIGHTMAP) {
					_patches_newgame.heightmap_rotation = e->we.dropdown.index;
				} else {
					_patches_newgame.land_generator = e->we.dropdown.index;
				}
				break;
			case GLAND_TERRAIN_PULLDOWN:
				_opt_newgame.diff.terrain_type = e->we.dropdown.index;
				if (_opt_newgame.diff_level != 3) ShowErrorMessage(INVALID_STRING_ID, STR_DIFFICULTY_TO_CUSTOM, 0, 0);
				DoCommandP(0, 12, _opt_newgame.diff.terrain_type, NULL, CMD_CHANGE_DIFFICULTY_LEVEL);
				break;
			case GLAND_WATER_PULLDOWN:
				_opt_newgame.diff.quantity_sea_lakes = e->we.dropdown.index;
				if (_opt_newgame.diff_level != 3) ShowErrorMessage(INVALID_STRING_ID, STR_DIFFICULTY_TO_CUSTOM, 0, 0);
				DoCommandP(0, 13, _opt_newgame.diff.quantity_sea_lakes, NULL, CMD_CHANGE_DIFFICULTY_LEVEL);
				break;
		}
		SetWindowDirty(w);
		break;

	case WE_ON_EDIT_TEXT: {
		if (e->we.edittext.str != NULL) {
			int32 value = atoi(e->we.edittext.str);

			switch (WP(w, generate_d).widget_id) {
			case GLAND_START_DATE_TEXT:
				w->InvalidateWidget(GLAND_START_DATE_TEXT);
				_patches_newgame.starting_year = Clamp(value, MIN_YEAR, MAX_YEAR);
				break;
			case GLAND_SNOW_LEVEL_TEXT:
				w->InvalidateWidget(GLAND_SNOW_LEVEL_TEXT);
				_patches_newgame.snow_line_height = Clamp(value, 2, MAX_SNOWLINE_HEIGHT);
				break;
			}

			SetWindowDirty(w);
		}
		break;
	}
	}
}

static const WindowDesc _generate_landscape_desc = {
	WDP_CENTER, WDP_CENTER, 338, 268, 338, 268,
	WC_GENERATE_LANDSCAPE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_generate_landscape_widgets,
	GenerateLandscapeWndProc,
};

static const WindowDesc _heightmap_load_desc = {
	WDP_CENTER, WDP_CENTER, 338, 236, 338, 236,
	WC_GENERATE_LANDSCAPE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS,
	_heightmap_load_widgets,
	GenerateLandscapeWndProc,
};

static void _ShowGenerateLandscape(glwp_modes mode)
{
	uint x = 0;
	uint y = 0;

	DeleteWindowByClass(WC_GENERATE_LANDSCAPE);

	/* Always give a new seed if not editor */
	if (_game_mode != GM_EDITOR) _patches_newgame.generation_seed = InteractiveRandom();

	if (mode == GLWP_HEIGHTMAP) {
		/* If the function returns negative, it means there was a problem loading the heightmap */
		if (!GetHeightmapDimensions(_file_to_saveload.name, &x, &y)) return;
	}

	Window *w = AllocateWindowDescFront((mode == GLWP_HEIGHTMAP) ? &_heightmap_load_desc : &_generate_landscape_desc, mode);

	if (w == NULL) return;

	if (mode == GLWP_HEIGHTMAP) {
		WP(w, generate_d).x = x;
		WP(w, generate_d).y = y;
		strecpy(WP(w, generate_d).name, _file_to_saveload.title, lastof(WP(w, generate_d).name));
	}

	InvalidateWindow(WC_GENERATE_LANDSCAPE, mode);
}

void ShowGenerateLandscape()
{
	_ShowGenerateLandscape(GLWP_GENERATE);
}

void ShowHeightmapLoad()
{
	_ShowGenerateLandscape(GLWP_HEIGHTMAP);
}

void StartScenarioEditor()
{
	if (_patches_newgame.town_layout == TL_NO_ROADS) {
		_patches_newgame.town_layout = TL_ORIGINAL;
	}

	StartGeneratingLandscape(GLWP_SCENARIO);
}

void StartNewGameWithoutGUI(uint seed)
{
	/* GenerateWorld takes care of the possible GENERATE_NEW_SEED value in 'seed' */
	_patches_newgame.generation_seed = seed;

	StartGeneratingLandscape(GLWP_GENERATE);
}

enum CreateScenarioWindowWidgets {
	CSCEN_TEMPERATE = 3,
	CSCEN_ARCTIC,
	CSCEN_TROPICAL,
	CSCEN_TOYLAND,
	CSCEN_EMPTY_WORLD,
	CSCEN_RANDOM_WORLD,
	CSCEN_MAPSIZE_X_TEXT,
	CSCEN_MAPSIZE_X_PULLDOWN,
	CSCEN_MAPSIZE_Y_TEXT,
	CSCEN_MAPSIZE_Y_PULLDOWN,
	CSCEN_START_DATE_DOWN,
	CSCEN_START_DATE_TEXT,
	CSCEN_START_DATE_UP,
	CSCEN_FLAT_LAND_HEIGHT_DOWN,
	CSCEN_FLAT_LAND_HEIGHT_TEXT,
	CSCEN_FLAT_LAND_HEIGHT_UP
};


static void CreateScenarioWndProc(Window *w, WindowEvent *e)
{
	static const StringID mapsizes[] = {STR_64, STR_128, STR_256, STR_512, STR_1024, STR_2048, INVALID_STRING_ID};

	switch (e->event) {
	case WE_CREATE: w->LowerWidget(_opt_newgame.landscape + CSCEN_TEMPERATE); break;

	case WE_PAINT:
		w->SetWidgetDisabledState(CSCEN_START_DATE_DOWN,       _patches_newgame.starting_year <= MIN_YEAR);
		w->SetWidgetDisabledState(CSCEN_START_DATE_UP,         _patches_newgame.starting_year >= MAX_YEAR);
		w->SetWidgetDisabledState(CSCEN_FLAT_LAND_HEIGHT_DOWN, _patches_newgame.se_flat_world_height <= 0);
		w->SetWidgetDisabledState(CSCEN_FLAT_LAND_HEIGHT_UP,   _patches_newgame.se_flat_world_height >= MAX_TILE_HEIGHT);

		w->SetWidgetLoweredState(CSCEN_TEMPERATE, _opt_newgame.landscape == LT_TEMPERATE);
		w->SetWidgetLoweredState(CSCEN_ARCTIC,    _opt_newgame.landscape == LT_ARCTIC);
		w->SetWidgetLoweredState(CSCEN_TROPICAL,  _opt_newgame.landscape == LT_TROPIC);
		w->SetWidgetLoweredState(CSCEN_TOYLAND,   _opt_newgame.landscape == LT_TOYLAND);
		DrawWindowWidgets(w);

		DrawStringRightAligned(211, 97, STR_MAPSIZE, TC_FROMSTRING);
		DrawString(            221, 97, mapsizes[_patches_newgame.map_x - 6], TC_BLACK);
		DrawStringCentered(    272, 97, STR_BY, TC_FROMSTRING);
		DrawString(            284, 97, mapsizes[_patches_newgame.map_y - 6], TC_BLACK);

		DrawStringRightAligned(211, 115, STR_DATE, TC_FROMSTRING);
		SetDParam(0, ConvertYMDToDate(_patches_newgame.starting_year, 0, 1));
		DrawStringCentered(271, 115, STR_GENERATE_DATE, TC_FROMSTRING);

		DrawStringRightAligned(278, 133, STR_FLAT_WORLD_HEIGHT, TC_FROMSTRING);
		SetDParam(0, _patches_newgame.se_flat_world_height);
		DrawStringCentered(303, 133, STR_FLAT_WORLD_HEIGHT_NUM, TC_BLACK);

		break;
	case WE_CLICK:
		switch (e->we.click.widget) {
		case CSCEN_TEMPERATE: case CSCEN_ARCTIC: case CSCEN_TROPICAL: case CSCEN_TOYLAND:
			w->RaiseWidget(_opt_newgame.landscape + CSCEN_TEMPERATE);
			SetNewLandscapeType(e->we.click.widget - CSCEN_TEMPERATE);
			break;
		case CSCEN_MAPSIZE_X_TEXT: case CSCEN_MAPSIZE_X_PULLDOWN: // Mapsize X
			ShowDropDownMenu(w, mapsizes, _patches_newgame.map_x - 6, CSCEN_MAPSIZE_X_PULLDOWN, 0, 0);
			break;
		case CSCEN_MAPSIZE_Y_TEXT: case CSCEN_MAPSIZE_Y_PULLDOWN: // Mapsize Y
			ShowDropDownMenu(w, mapsizes, _patches_newgame.map_y - 6, CSCEN_MAPSIZE_Y_PULLDOWN, 0, 0);
			break;
		case CSCEN_EMPTY_WORLD: // Empty world / flat world
			StartGeneratingLandscape(GLWP_SCENARIO);
			break;
		case CSCEN_RANDOM_WORLD: // Generate
			ShowGenerateLandscape();
			break;
		case CSCEN_START_DATE_DOWN: case CSCEN_START_DATE_UP: // Year buttons
			/* Don't allow too fast scrolling */
			if ((w->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
				w->HandleButtonClick(e->we.click.widget);
				SetWindowDirty(w);

				_patches_newgame.starting_year = Clamp(_patches_newgame.starting_year + e->we.click.widget - CSCEN_START_DATE_TEXT, MIN_YEAR, MAX_YEAR);
			}
			_left_button_clicked = false;
			break;
		case CSCEN_START_DATE_TEXT: // Year text
			WP(w, generate_d).widget_id = CSCEN_START_DATE_TEXT;
			SetDParam(0, _patches_newgame.starting_year);
			ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_START_DATE_QUERY_CAPT, 8, 100, w, CS_NUMERAL);
			break;
		case CSCEN_FLAT_LAND_HEIGHT_DOWN: case CSCEN_FLAT_LAND_HEIGHT_UP: // Height level buttons
			/* Don't allow too fast scrolling */
			if ((w->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
				w->HandleButtonClick(e->we.click.widget);
				SetWindowDirty(w);

				_patches_newgame.se_flat_world_height = Clamp(_patches_newgame.se_flat_world_height + e->we.click.widget - CSCEN_FLAT_LAND_HEIGHT_TEXT, 0, MAX_TILE_HEIGHT);
			}
			_left_button_clicked = false;
			break;
		case CSCEN_FLAT_LAND_HEIGHT_TEXT: // Height level text
			WP(w, generate_d).widget_id = CSCEN_FLAT_LAND_HEIGHT_TEXT;
			SetDParam(0, _patches_newgame.se_flat_world_height);
			ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_FLAT_WORLD_HEIGHT_QUERY_CAPT, 3, 100, w, CS_NUMERAL);
			break;
		}
		break;

	case WE_DROPDOWN_SELECT:
		switch (e->we.dropdown.button) {
			case CSCEN_MAPSIZE_X_PULLDOWN: _patches_newgame.map_x = e->we.dropdown.index + 6; break;
			case CSCEN_MAPSIZE_Y_PULLDOWN: _patches_newgame.map_y = e->we.dropdown.index + 6; break;
		}
		SetWindowDirty(w);
		break;

	case WE_ON_EDIT_TEXT: {
		if (e->we.edittext.str != NULL) {
			int32 value = atoi(e->we.edittext.str);

			switch (WP(w, generate_d).widget_id) {
			case CSCEN_START_DATE_TEXT:
				w->InvalidateWidget(CSCEN_START_DATE_TEXT);
				_patches_newgame.starting_year = Clamp(value, MIN_YEAR, MAX_YEAR);
				break;
			case CSCEN_FLAT_LAND_HEIGHT_TEXT:
				w->InvalidateWidget(CSCEN_FLAT_LAND_HEIGHT_TEXT);
				_patches_newgame.se_flat_world_height = Clamp(value, 0, MAX_TILE_HEIGHT);
				break;
			}

			SetWindowDirty(w);
		}
		break;
	}
	}
}

static const Widget _create_scenario_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE, 13,   0,  10,   0,  13, STR_00C5,                STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION, RESIZE_NONE, 13,  11, 337,   0,  13, STR_SE_CAPTION,          STR_NULL},
{      WWT_PANEL, RESIZE_NONE, 13,   0, 337,  14, 169, 0x0,                     STR_NULL},

{   WWT_IMGBTN_2, RESIZE_NONE, 12,  10,  86,  24,  78, SPR_SELECT_TEMPERATE,    STR_030E_SELECT_TEMPERATE_LANDSCAPE},
{   WWT_IMGBTN_2, RESIZE_NONE, 12,  90, 166,  24,  78, SPR_SELECT_SUB_ARCTIC,   STR_030F_SELECT_SUB_ARCTIC_LANDSCAPE},
{   WWT_IMGBTN_2, RESIZE_NONE, 12, 170, 246,  24,  78, SPR_SELECT_SUB_TROPICAL, STR_0310_SELECT_SUB_TROPICAL_LANDSCAPE},
{   WWT_IMGBTN_2, RESIZE_NONE, 12, 250, 326,  24,  78, SPR_SELECT_TOYLAND,      STR_0311_SELECT_TOYLAND_LANDSCAPE},

{    WWT_TEXTBTN, RESIZE_NONE,  6,  12, 115,  95, 124, STR_SE_FLAT_WORLD,       STR_SE_FLAT_WORLD_TIP},         // Empty (sea-level) map
{    WWT_TEXTBTN, RESIZE_NONE,  6,  12, 115, 131, 160, STR_SE_RANDOM_LAND,      STR_022A_GENERATE_RANDOM_LAND}, // Generate

{      WWT_PANEL, RESIZE_NONE, 12, 216, 251,  95, 106, 0x0,                     STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 252, 263,  95, 106, STR_0225,                STR_NULL}, // Mapsize X
{      WWT_PANEL, RESIZE_NONE, 12, 279, 314,  95, 106, 0x0,                     STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 315, 326,  95, 106, STR_0225,                STR_NULL}, // Mapsize Y

{     WWT_IMGBTN, RESIZE_NONE, 12, 216, 227, 113, 124, SPR_ARROW_DOWN,          STR_029E_MOVE_THE_STARTING_DATE},
{      WWT_PANEL, RESIZE_NONE, 12, 228, 314, 113, 124, 0x0,                     STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 315, 326, 113, 124, SPR_ARROW_UP,            STR_029F_MOVE_THE_STARTING_DATE},

{     WWT_IMGBTN, RESIZE_NONE, 12, 282, 293, 131, 142, SPR_ARROW_DOWN,          STR_FLAT_WORLD_HEIGHT_DOWN},
{      WWT_PANEL, RESIZE_NONE, 12, 294, 314, 131, 142, 0x0,                     STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 315, 326, 131, 142, SPR_ARROW_UP,            STR_FLAT_WORLD_HEIGHT_UP},
{   WIDGETS_END},
};

static const WindowDesc _create_scenario_desc = {
	WDP_CENTER, WDP_CENTER, 338, 170, 338, 170,
	WC_GENERATE_LANDSCAPE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS,
	_create_scenario_widgets,
	CreateScenarioWndProc,
};

void ShowCreateScenario()
{
	DeleteWindowByClass(WC_GENERATE_LANDSCAPE);
	AllocateWindowDescFront(&_create_scenario_desc, GLWP_SCENARIO);
}


static const Widget _show_terrain_progress_widgets[] = {
{    WWT_CAPTION,   RESIZE_NONE,    14,     0,   180,     0,    13, STR_GENERATION_WORLD,   STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   180,    14,    96, 0x0,                    STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,    15,    20,   161,    74,    85, STR_GENERATION_ABORT,   STR_NULL}, // Abort button
{   WIDGETS_END},
};

struct tp_info {
	uint percent;
	StringID cls;
	uint current;
	uint total;
	int timer;
};

static tp_info _tp;

static void AbortGeneratingWorldCallback(Window *w, bool confirmed)
{
	if (confirmed) {
		AbortGeneratingWorld();
	} else if (IsGeneratingWorld() && !IsGeneratingWorldAborted()) {
		SetMouseCursor(SPR_CURSOR_ZZZ, PAL_NONE);
	}
}

static void ShowTerrainProgressProc(Window* w, WindowEvent* e)
{
	switch (e->event) {
	case WE_CLICK:
		switch (e->we.click.widget) {
		case 2:
			if (_cursor.sprite == SPR_CURSOR_ZZZ) SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);
			ShowQuery(
				STR_GENERATION_ABORT_CAPTION,
				STR_GENERATION_ABORT_MESSAGE,
				w,
				AbortGeneratingWorldCallback
			);
			break;
		}
		break;

	case WE_PAINT:
		DrawWindowWidgets(w);

		/* Draw the % complete with a bar and a text */
		DrawFrameRect(19, 20, (w->width - 18), 37, 14, FR_BORDERONLY);
		DrawFrameRect(20, 21, (int)((w->width - 40) * _tp.percent / 100) + 20, 36, 10, FR_NONE);
		SetDParam(0, _tp.percent);
		DrawStringCentered(90, 25, STR_PROGRESS, TC_FROMSTRING);

		/* Tell which class we are generating */
		DrawStringCentered(90, 46, _tp.cls, TC_FROMSTRING);

		/* And say where we are in that class */
		SetDParam(0, _tp.current);
		SetDParam(1, _tp.total);
		DrawStringCentered(90, 58, STR_GENERATION_PROGRESS, TC_FROMSTRING);

		SetWindowDirty(w);
		break;
	}
}

static const WindowDesc _show_terrain_progress_desc = {
	WDP_CENTER, WDP_CENTER, 181, 97, 181, 97,
	WC_GENERATE_PROGRESS_WINDOW, WC_NONE,
	WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_show_terrain_progress_widgets,
	ShowTerrainProgressProc
};

/**
 * Initializes the progress counters to the starting point.
 */
void PrepareGenerateWorldProgress()
{
	_tp.cls   = STR_WORLD_GENERATION;
	_tp.current = 0;
	_tp.total   = 0;
	_tp.percent = 0;
	_tp.timer   = 0; // Forces to paint the progress window immediatelly
}

/**
 * Show the window where a user can follow the process of the map generation.
 */
void ShowGenerateWorldProgress()
{
	AllocateWindowDescFront(&_show_terrain_progress_desc, 0);
}

static void _SetGeneratingWorldProgress(gwp_class cls, uint progress, uint total)
{
	static const int percent_table[GWP_CLASS_COUNT + 1] = {0, 5, 15, 20, 40, 60, 65, 80, 85, 99, 100 };
	static const StringID class_table[GWP_CLASS_COUNT]  = {
		STR_WORLD_GENERATION,
		STR_022E_LANDSCAPE_GENERATION,
		STR_CLEARING_TILES,
		STR_022F_TOWN_GENERATION,
		STR_0230_INDUSTRY_GENERATION,
		STR_UNMOVABLE_GENERATION,
		STR_TREE_GENERATION,
		STR_SETTINGUP_GAME,
		STR_PREPARING_TILELOOP,
		STR_PREPARING_GAME
	};

	assert(cls < GWP_CLASS_COUNT);

	/* Do not run this function if we aren't in a thread */
	if (!IsGenerateWorldThreaded() && !_network_dedicated) return;

	if (IsGeneratingWorldAborted()) HandleGeneratingWorldAbortion();

	if (total == 0) {
		assert(_tp.cls == class_table[cls]);
		_tp.current += progress;
	} else {
		_tp.cls   = class_table[cls];
		_tp.current = progress;
		_tp.total   = total;
		_tp.percent = percent_table[cls];
	}

	/* Don't update the screen too often. So update it once in every 200ms */
	if (!_network_dedicated && _tp.timer != 0 && _realtime_tick - _tp.timer < 200) return;

	/* Percentage is about the number of completed tasks, so 'current - 1' */
	_tp.percent = percent_table[cls] + (percent_table[cls + 1] - percent_table[cls]) * (_tp.current == 0 ? 0 : _tp.current - 1) / _tp.total;

	if (_network_dedicated) {
		static uint last_percent = 0;

		/* Never display 0% */
		if (_tp.percent == 0) return;
		/* Reset if percent is lower then the last recorded */
		if (_tp.percent < last_percent) last_percent = 0;
		/* Display every 5%, but 6% is also very valid.. just not smaller steps then 5% */
		if (_tp.percent % 5 != 0 && _tp.percent <= last_percent + 5) return;
		/* Never show steps smaller then 2%, even if it is a mod 5% */
		if (_tp.percent <= last_percent + 2) return;

		DEBUG(net, 1, "Map generation percentage complete: %d", _tp.percent);
		last_percent = _tp.percent;

		/* Don't continue as dedicated never has a thread running */
		return;
	}

	InvalidateWindow(WC_GENERATE_PROGRESS_WINDOW, 0);
	MarkWholeScreenDirty();
	SetGeneratingWorldPaintStatus(true);

	/* We wait here till the paint is done, so we don't read and write
	 *  on the same tile at the same moment. Nasty hack, but that happens
	 *  if you implement threading afterwards */
	while (IsGeneratingWorldReadyForPaint()) { CSleep(10); }

	_tp.timer = _realtime_tick;
}

/**
 * Set the total of a stage of the world generation.
 * @param cls the current class we are in.
 * @param total Set the total expected items for this class.
 *
 * Warning: this function isn't clever. Don't go from class 4 to 3. Go upwards, always.
 *  Also, progress works if total is zero, total works if progress is zero.
 */
void SetGeneratingWorldProgress(gwp_class cls, uint total)
{
	if (total == 0) return;

	_SetGeneratingWorldProgress(cls, 0, total);
}

/**
 * Increases the current stage of the world generation with one.
 * @param cls the current class we are in.
 *
 * Warning: this function isn't clever. Don't go from class 4 to 3. Go upwards, always.
 *  Also, progress works if total is zero, total works if progress is zero.
 */
void IncreaseGeneratingWorldProgress(gwp_class cls)
{
	/* In fact the param 'class' isn't needed.. but for some security reasons, we want it around */
	_SetGeneratingWorldProgress(cls, 1, 0);
}
