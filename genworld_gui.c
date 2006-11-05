/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "heightmap.h"
#include "functions.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
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
#include "network.h"
#include "thread.h"
#include "date.h"

enum {
	START_DATE_QUERY,
	SNOW_LINE_QUERY,
	FLAT_WORLD_HEIGHT_QUERY,

	LEN_RND_SEED = 11,
	SEED_EDIT    = 15,
};

/**
 * In what 'mode' the GenerateLandscapeWindowProc is.
 */
typedef enum glwp_modes {
	GLWP_GENERATE,
	GLWP_HEIGHTMAP,
	GLWP_SCENARIO,
	GLWP_END
} glwp_modes;

static char _edit_str_buf[LEN_RND_SEED];
static uint _heightmap_x = 0;
static uint _heightmap_y = 0;
static StringID _heightmap_str = STR_NULL;
static bool _goto_editor = false;

extern void SwitchMode(int new_mode);

static inline void SetNewLandscapeType(byte landscape)
{
	_opt_newgame.landscape = landscape;
	InvalidateWindowClasses(WC_SELECT_GAME);
	InvalidateWindowClasses(WC_GENERATE_LANDSCAPE);
}

// no longer static to allow calling from outside module
const Widget _generate_landscape_widgets[] = {
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

const Widget _heightmap_load_widgets[] = {
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

static void StartGeneratingLandscape(glwp_modes mode)
{
	/* If we want to go to the editor, and aren't yet, we need to delay
	 *  it as long as possible, else it gives nasty side-effects (aborting
	 *  results in ending up in the SE, which you don't want. Therefor we
	 *  use this switch to do it at the very end.
	 */
	if (_goto_editor) _game_mode = GM_EDITOR;

	DeleteWindowByClass(WC_GENERATE_LANDSCAPE);
	DeleteWindowByClass(WC_INDUSTRY_VIEW);
	DeleteWindowByClass(WC_TOWN_VIEW);
	DeleteWindowByClass(WC_LAND_INFO);

	/* Copy all XXX_newgame to XXX */
	UpdatePatches();
	_opt_ptr = &_opt;
	*_opt_ptr = _opt_newgame;
	/* Load the right landscape stuff */
	GfxLoadSprites();

	SndPlayFx(SND_15_BEEP);
	switch (mode) {
	case GLWP_GENERATE:  _switch_mode = (_game_mode == GM_EDITOR) ? SM_GENRANDLAND    : SM_NEWGAME;         break;
	case GLWP_HEIGHTMAP: _switch_mode = (_game_mode == GM_EDITOR) ? SM_LOAD_HEIGHTMAP : SM_START_HEIGHTMAP; break;
	case GLWP_SCENARIO:  _switch_mode = SM_EDITOR; break;
	default: NOT_REACHED(); return;
	}
}

static void HeightmapScaledTooMuchCallback(bool ok_clicked)
{
	if (ok_clicked) {
		Window *w;
		glwp_modes mode = 0;
		for (mode = 0; mode < GLWP_END; mode++) {
			w = FindWindowById(WC_GENERATE_LANDSCAPE, mode);
			if (w != NULL) StartGeneratingLandscape(mode);
		}
	}
}

void GenerateLandscapeWndProc(Window *w, WindowEvent *e)
{
	static const StringID mapsizes[]    = {STR_64, STR_128, STR_256, STR_512, STR_1024, STR_2048, INVALID_STRING_ID};
	static const StringID elevations[]  = {STR_682A_VERY_FLAT, STR_682B_FLAT, STR_682C_HILLY, STR_682D_MOUNTAINOUS, INVALID_STRING_ID};
	static const StringID sea_lakes[]   = {STR_VERY_LOW, STR_6820_LOW, STR_6821_MEDIUM, STR_6822_HIGH, INVALID_STRING_ID};
	static const StringID smoothness[]  = {STR_CONFIG_PATCHES_ROUGHNESS_OF_TERRAIN_VERY_SMOOTH, STR_CONFIG_PATCHES_ROUGHNESS_OF_TERRAIN_SMOOTH, STR_CONFIG_PATCHES_ROUGHNESS_OF_TERRAIN_ROUGH, STR_CONFIG_PATCHES_ROUGHNESS_OF_TERRAIN_VERY_ROUGH, INVALID_STRING_ID};
	static const StringID tree_placer[] = {STR_CONFIG_PATCHES_TREE_PLACER_NONE, STR_CONFIG_PATCHES_TREE_PLACER_ORIGINAL, STR_CONFIG_PATCHES_TREE_PLACER_IMPROVED, INVALID_STRING_ID};
	static const StringID rotation[]    = {STR_CONFIG_PATCHES_HEIGHTMAP_ROTATION_COUNTER_CLOCKWISE, STR_CONFIG_PATCHES_HEIGHTMAP_ROTATION_CLOCKWISE, INVALID_STRING_ID};
	static const StringID landscape[]   = {STR_CONFIG_PATCHES_LAND_GENERATOR_ORIGINAL, STR_CONFIG_PATCHES_LAND_GENERATOR_TERRA_GENESIS, INVALID_STRING_ID};
	static const StringID num_towns[]   = {STR_6816_LOW, STR_6817_NORMAL, STR_6818_HIGH, INVALID_STRING_ID};
	static const StringID num_inds[]    = {STR_26816_NONE, STR_6816_LOW, STR_6817_NORMAL, STR_6818_HIGH, INVALID_STRING_ID};

	uint mode = w->window_number;
	uint y;

	switch (e->event) {
	case WE_CREATE: LowerWindowWidget(w, _opt_newgame.landscape + 3); break;

	case WE_PAINT:
		/* You can't select smoothness if not terragenesis */
		SetWindowWidgetDisabledState(w, 32, _patches_newgame.land_generator == 0);
		SetWindowWidgetDisabledState(w, 33, _patches_newgame.land_generator == 0);
		/* Disable snowline if not hilly */
		SetWindowWidgetDisabledState(w, 22, _opt_newgame.landscape != LT_HILLY);
		/* Disable town and industry in SE */
		SetWindowWidgetDisabledState(w, 11, _game_mode == GM_EDITOR);
		SetWindowWidgetDisabledState(w, 12, _game_mode == GM_EDITOR);
		SetWindowWidgetDisabledState(w, 13, _game_mode == GM_EDITOR);
		SetWindowWidgetDisabledState(w, 14, _game_mode == GM_EDITOR);
		SetWindowWidgetDisabledState(w, 24, _game_mode == GM_EDITOR);
		SetWindowWidgetDisabledState(w, 25, _game_mode == GM_EDITOR);

		SetWindowWidgetDisabledState(w, 18, _patches_newgame.starting_year <= MIN_YEAR);
		SetWindowWidgetDisabledState(w, 20, _patches_newgame.starting_year >= MAX_YEAR);
		SetWindowWidgetDisabledState(w, 21, _patches_newgame.snow_line_height <= 2 || _opt_newgame.landscape != LT_HILLY);
		SetWindowWidgetDisabledState(w, 23, _patches_newgame.snow_line_height >= 13 || _opt_newgame.landscape != LT_HILLY);

		SetWindowWidgetLoweredState(w, 3, _opt_newgame.landscape == LT_NORMAL);
		SetWindowWidgetLoweredState(w, 4, _opt_newgame.landscape == LT_HILLY);
		SetWindowWidgetLoweredState(w, 5, _opt_newgame.landscape == LT_DESERT);
		SetWindowWidgetLoweredState(w, 6, _opt_newgame.landscape == LT_CANDY);
		DrawWindowWidgets(w);

		y = (mode == GLWP_HEIGHTMAP) ? 22 : 0;

		DrawString( 12,  91 + y, STR_MAPSIZE, 0);
		DrawString(119,  91 + y, mapsizes[_patches_newgame.map_x - 6], 0x10);
		DrawString(168,  91 + y, STR_BY, 0);
		DrawString(182,  91 + y, mapsizes[_patches_newgame.map_y - 6], 0x10);

		DrawString( 12, 113 + y, STR_NUMBER_OF_TOWNS, 0);
		DrawString( 12, 131 + y, STR_NUMBER_OF_INDUSTRIES, 0);
		if (_game_mode == GM_EDITOR) {
			DrawString(118, 113 + y, STR_6836_OFF, 0x10);
			DrawString(118, 131 + y, STR_6836_OFF, 0x10);
		} else {
			DrawString(118, 113 + y, num_towns[_opt_newgame.diff.number_towns], 0x10);
			DrawString(118, 131 + y, num_inds[_opt_newgame.diff.number_industries], 0x10);
		}

		DrawString( 12, 153 + y, STR_RANDOM_SEED, 0);
		DrawEditBox(w, &WP(w, querystr_d), SEED_EDIT);

		DrawString(182, 113 + y, STR_DATE, 0);
		SetDParam(0, ConvertYMDToDate(_patches_newgame.starting_year, 0, 1));
		DrawStringCentered(271, 113 + y, STR_GENERATE_DATE, 0);

		DrawString(182, 131 + y, STR_SNOW_LINE_HEIGHT, 0);
		SetDParam(0, _patches_newgame.snow_line_height);
		DrawStringCentered(303, 131 + y, STR_SNOW_LINE_HEIGHT_NUM, 0x10);

		if (mode == GLWP_GENERATE) {
			DrawString( 12, 175, STR_LAND_GENERATOR, 0);
			DrawString(118, 175, landscape[_patches_newgame.land_generator], 0x10);

			DrawString( 12, 193, STR_TREE_PLACER, 0);
			DrawString(118, 193, tree_placer[_patches_newgame.tree_placer], 0x10);

			DrawString( 12, 211, STR_TERRAIN_TYPE, 0);
			DrawString(118, 211, elevations[_opt_newgame.diff.terrain_type], 0x10);

			DrawString( 12, 229, STR_QUANTITY_OF_SEA_LAKES, 0);
			DrawString(118, 229, sea_lakes[_opt_newgame.diff.quantity_sea_lakes], 0x10);

			DrawString( 12, 247, STR_SMOOTHNESS, 0);
			DrawString(118, 247, smoothness[_patches_newgame.tgen_smoothness], 0x10);
		} else {
			char buffer[512];

			if (_patches_newgame.heightmap_rotation == HM_CLOCKWISE) {
				SetDParam(0, _heightmap_y);
				SetDParam(1, _heightmap_x);
			} else {
				SetDParam(0, _heightmap_x);
				SetDParam(1, _heightmap_y);
			}
			GetString(buffer, STR_HEIGHTMAP_SIZE, lastof(buffer));
			DrawStringRightAligned(326, 91, STR_HEIGHTMAP_SIZE, 0x10);

			DrawString( 12,  91, STR_HEIGHTMAP_NAME, 0x10);
			SetDParam(0, _heightmap_str);
			DrawStringTruncated(114,  91, STR_ORANGE, 0x10, 326 - 114 - GetStringBoundingBox(buffer).width - 5);

			DrawString( 12, 197, STR_TREE_PLACER, 0);
			DrawString(118, 197, tree_placer[_patches_newgame.tree_placer], 0x10);

			DrawString( 12, 215, STR_HEIGHTMAP_ROTATION, 0);
			DrawString(118, 215, rotation[_patches_newgame.heightmap_rotation], 0x10);
		}

		break;
	case WE_CLICK:
		switch (e->we.click.widget) {
		case 0: DeleteWindow(w); break;
		case 3: case 4: case 5: case 6:
			RaiseWindowWidget(w, _opt_newgame.landscape + 3);
			SetNewLandscapeType(e->we.click.widget - 3);
			break;
		case 7: case 8: // Mapsize X
			ShowDropDownMenu(w, mapsizes, _patches_newgame.map_x - 6, 8, 0, 0);
			break;
		case 9: case 10: // Mapsize Y
			ShowDropDownMenu(w, mapsizes, _patches_newgame.map_y - 6, 10, 0, 0);
			break;
		case 11: case 12: // Number of towns
			ShowDropDownMenu(w, num_towns, _opt_newgame.diff.number_towns, 12, 0, 0);
			break;
		case 13: case 14: // Number of industries
			ShowDropDownMenu(w, num_inds, _opt_newgame.diff.number_industries, 14, 0, 0);
			break;
		case 16: // Random seed
			_patches_newgame.generation_seed = InteractiveRandom();
			ttd_strlcpy(_edit_str_buf, str_fmt("%u", _patches_newgame.generation_seed), lengthof(_edit_str_buf));
			UpdateTextBufferSize(&WP(w, querystr_d).text);
			SetWindowDirty(w);
			break;
		case 17: // Generate
			if (mode == GLWP_HEIGHTMAP && (
					_heightmap_x * 2 < (1U << _patches_newgame.map_x) || _heightmap_x / 2 > (1U << _patches_newgame.map_x) ||
					_heightmap_y * 2 < (1U << _patches_newgame.map_y) || _heightmap_y / 2 > (1U << _patches_newgame.map_y))) {
				ShowQuery(STR_HEIGHTMAP_SCALE_WARNING_CAPTION, STR_HEIGHTMAP_SCALE_WARNING_MESSAGE, HeightmapScaledTooMuchCallback, WC_GENERATE_LANDSCAPE, mode);
			} else {
				StartGeneratingLandscape(mode);
			}
			break;
		case 18: case 20: // Year buttons
			/* Don't allow too fast scrolling */
			if ((w->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
				HandleButtonClick(w, e->we.click.widget);
				SetWindowDirty(w);

				_patches_newgame.starting_year = clamp(_patches_newgame.starting_year + e->we.click.widget - 19, MIN_YEAR, MAX_YEAR);
			}
			_left_button_clicked = false;
			break;
		case 19: // Year text
			WP(w, def_d).data_3 = START_DATE_QUERY;
			SetDParam(0, _patches_newgame.starting_year);
			ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_START_DATE_QUERY_CAPT, 8, 100, WC_GENERATE_LANDSCAPE, mode, CS_NUMERAL);
			break;
		case 21: case 23: // Snow line buttons
			/* Don't allow too fast scrolling */
			if ((w->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
				HandleButtonClick(w, e->we.click.widget);
				SetWindowDirty(w);

				_patches_newgame.snow_line_height = clamp(_patches_newgame.snow_line_height + e->we.click.widget - 22, 2, 13);
			}
			_left_button_clicked = false;
			break;
		case 22: // Snow line text
			WP(w, def_d).data_3 = SNOW_LINE_QUERY;
			SetDParam(0, _patches_newgame.snow_line_height);
			ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_SNOW_LINE_QUERY_CAPT, 3, 100, WC_GENERATE_LANDSCAPE, mode, CS_NUMERAL);
			break;
		case 24: case 25: // Tree placer
			ShowDropDownMenu(w, tree_placer, _patches_newgame.tree_placer, 25, 0, 0);
			break;
		case 26: case 27: // Landscape generator OR Heightmap rotation
			if (mode == GLWP_HEIGHTMAP) {
				ShowDropDownMenu(w, rotation, _patches_newgame.heightmap_rotation, 27, 0, 0);
			} else {
				ShowDropDownMenu(w, landscape, _patches_newgame.land_generator, 27, 0, 0);
			}
			break;
		case 28: case 29: // Terrain type
			ShowDropDownMenu(w, elevations, _opt_newgame.diff.terrain_type, 29, 0, 0);
			break;
		case 30: case 31: // Water quantity
			ShowDropDownMenu(w, sea_lakes, _opt_newgame.diff.quantity_sea_lakes, 31, 0, 0);
			break;
		case 32: case 33: // Map smoothness
			ShowDropDownMenu(w, smoothness, _patches_newgame.tgen_smoothness, 33, 0, 0);
			break;
		}
		break;

	case WE_MESSAGE:
		ttd_strlcpy(_edit_str_buf, str_fmt("%u", _patches_newgame.generation_seed), lengthof(_edit_str_buf));
		DrawEditBox(w, &WP(w, querystr_d), SEED_EDIT);
		break;

	case WE_MOUSELOOP:
		HandleEditBox(w, &WP(w, querystr_d), SEED_EDIT);
		break;

	case WE_KEYPRESS:
		HandleEditBoxKey(w, &WP(w, querystr_d), SEED_EDIT, e);
		/* the seed is unsigned, therefore atoi cannot be used.
		 * As 2^32 - 1 (MAX_UVALUE(uint32)) is a 'magic' value
		 * (use random seed) it should not be possible to be
		 * entered into the input field; the generate seed
		 * button can be used instead. */
		_patches_newgame.generation_seed = minu(strtoul(_edit_str_buf, NULL, 10), MAX_UVALUE(uint32) - 1);
		break;

	case WE_DROPDOWN_SELECT:
		switch (e->we.dropdown.button) {
			case 8:  _patches_newgame.map_x = e->we.dropdown.index + 6; break;
			case 10: _patches_newgame.map_y = e->we.dropdown.index + 6; break;
			case 12:
				_opt_newgame.diff.number_towns = e->we.dropdown.index;
				if (_opt_newgame.diff_level != 3) ShowErrorMessage(INVALID_STRING_ID, STR_DIFFICULTY_TO_CUSTOM, 0, 0);
				DoCommandP(0, 2, _opt_newgame.diff.number_towns, NULL, CMD_CHANGE_DIFFICULTY_LEVEL);
				break;
			case 14:
				_opt_newgame.diff.number_industries = e->we.dropdown.index;
				if (_opt_newgame.diff_level != 3) ShowErrorMessage(INVALID_STRING_ID, STR_DIFFICULTY_TO_CUSTOM, 0, 0);
				DoCommandP(0, 3, _opt_newgame.diff.number_industries, NULL, CMD_CHANGE_DIFFICULTY_LEVEL);
				break;
			case 25:
				_patches_newgame.tree_placer = e->we.dropdown.index;
				break;
			case 27:
				if (mode == GLWP_HEIGHTMAP) {
					_patches_newgame.heightmap_rotation = e->we.dropdown.index;
				} else {
					_patches_newgame.land_generator = e->we.dropdown.index;
				}
				break;
			case 29:
				_opt_newgame.diff.terrain_type = e->we.dropdown.index;
				if (_opt_newgame.diff_level != 3) ShowErrorMessage(INVALID_STRING_ID, STR_DIFFICULTY_TO_CUSTOM, 0, 0);
				DoCommandP(0, 12, _opt_newgame.diff.terrain_type, NULL, CMD_CHANGE_DIFFICULTY_LEVEL);
				break;
			case 31:
				_opt_newgame.diff.quantity_sea_lakes = e->we.dropdown.index;
				if (_opt_newgame.diff_level != 3) ShowErrorMessage(INVALID_STRING_ID, STR_DIFFICULTY_TO_CUSTOM, 0, 0);
				DoCommandP(0, 13, _opt_newgame.diff.quantity_sea_lakes, NULL, CMD_CHANGE_DIFFICULTY_LEVEL);
				break;
			case 33:
				_patches_newgame.tgen_smoothness = e->we.dropdown.index;
				break;
		}
		SetWindowDirty(w);
		break;

	case WE_ON_EDIT_TEXT: {
		if (e->we.edittext.str != NULL) {
			int32 value = atoi(e->we.edittext.str);

			switch (WP(w, def_d).data_3) {
			case START_DATE_QUERY:
				InvalidateWidget(w, 19);
				_patches_newgame.starting_year = clamp(value, MIN_YEAR, MAX_YEAR);
				break;
			case SNOW_LINE_QUERY:
				InvalidateWidget(w, 22);
				_patches_newgame.snow_line_height = clamp(value, 2, 13);
				break;
			}

			SetWindowDirty(w);
		}
		break;
	}
	}
}

const WindowDesc _generate_landscape_desc = {
	WDP_CENTER, WDP_CENTER, 338, 268,
	WC_GENERATE_LANDSCAPE, 0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_generate_landscape_widgets,
	GenerateLandscapeWndProc,
};

const WindowDesc _heightmap_load_desc = {
	WDP_CENTER, WDP_CENTER, 338, 236,
	WC_GENERATE_LANDSCAPE, 0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_heightmap_load_widgets,
	GenerateLandscapeWndProc,
};

static void _ShowGenerateLandscape(glwp_modes mode)
{
	Window *w;

	/* Don't kill WC_GENERATE_LANDSCAPE:GLWP_SCENARIO, because it resets
	 *  _goto_editor, which we maybe need later on. */
	DeleteWindowById(WC_GENERATE_LANDSCAPE, GLWP_GENERATE);
	DeleteWindowById(WC_GENERATE_LANDSCAPE, GLWP_HEIGHTMAP);

	/* Always give a new seed if not editor */
	if (_game_mode != GM_EDITOR) _patches_newgame.generation_seed = InteractiveRandom();

	if (mode == GLWP_HEIGHTMAP) {
		if (_heightmap_str != STR_NULL) DeleteName(_heightmap_str);

		_heightmap_x = 0;
		_heightmap_y = 0;
		_heightmap_str = AllocateName(_file_to_saveload.title, 0);
		/* If the function returns negative, it means there was a problem loading the heightmap */
		if (!GetHeightmapDimensions(_file_to_saveload.name, &_heightmap_x, &_heightmap_y))
			return;
	}

	w = AllocateWindowDescFront((mode == GLWP_HEIGHTMAP) ? &_heightmap_load_desc : &_generate_landscape_desc, mode);

	if (w != NULL) {
		querystr_d *querystr = &WP(w, querystr_d);

		ttd_strlcpy(_edit_str_buf, str_fmt("%u", _patches_newgame.generation_seed), lengthof(_edit_str_buf));

		InitializeTextBuffer(&querystr->text, _edit_str_buf, lengthof(_edit_str_buf), 120);
		querystr->caption = STR_NULL;
		querystr->afilter = CS_NUMERAL;

		InvalidateWindow(WC_GENERATE_LANDSCAPE, mode);
	}
}

void ShowGenerateLandscape(void)
{
	_ShowGenerateLandscape(GLWP_GENERATE);
}

void ShowHeightmapLoad(void)
{
	_ShowGenerateLandscape(GLWP_HEIGHTMAP);
}

void StartNewGameWithoutGUI(uint seed)
{
	/* GenerateWorld takes care of the possible GENERATE_NEW_SEED value in 'seed' */
	_patches_newgame.generation_seed = seed;

	StartGeneratingLandscape(GLWP_GENERATE);
}


void CreateScenarioWndProc(Window *w, WindowEvent *e)
{
	static const StringID mapsizes[] = {STR_64, STR_128, STR_256, STR_512, STR_1024, STR_2048, INVALID_STRING_ID};

	switch (e->event) {
	case WE_CREATE: LowerWindowWidget(w, _opt_newgame.landscape + 3); break;

	case WE_PAINT:
		SetWindowWidgetDisabledState(w, 14, _patches_newgame.starting_year <= MIN_YEAR);
		SetWindowWidgetDisabledState(w, 16, _patches_newgame.starting_year >= MAX_YEAR);
		SetWindowWidgetDisabledState(w, 17, _patches_newgame.se_flat_world_height <= 0);
		SetWindowWidgetDisabledState(w, 19, _patches_newgame.se_flat_world_height >= 15);

		SetWindowWidgetLoweredState(w, 3, _opt_newgame.landscape == LT_NORMAL);
		SetWindowWidgetLoweredState(w, 4, _opt_newgame.landscape == LT_HILLY);
		SetWindowWidgetLoweredState(w, 5, _opt_newgame.landscape == LT_DESERT);
		SetWindowWidgetLoweredState(w, 6, _opt_newgame.landscape == LT_CANDY);
		DrawWindowWidgets(w);

		DrawString( 12,  96, STR_MAPSIZE, 0);
		DrawString(167,  96, mapsizes[_patches_newgame.map_x - 6], 0x10);
		DrawString(216,  96, STR_BY, 0);
		DrawString(230,  96, mapsizes[_patches_newgame.map_y - 6], 0x10);

		DrawString(162, 118, STR_DATE, 0);
		SetDParam(0, ConvertYMDToDate(_patches_newgame.starting_year, 0, 1));
		DrawStringCentered(271, 118, STR_GENERATE_DATE, 0);

		DrawString(162, 136, STR_FLAT_WORLD_HEIGHT, 0);
		SetDParam(0, _patches_newgame.se_flat_world_height);
		DrawStringCentered(303, 136, STR_FLAT_WORLD_HEIGHT_NUM, 0x10);

		break;
	case WE_CLICK:
		switch (e->we.click.widget) {
		case 0: DeleteWindow(w); break;
		case 3: case 4: case 5: case 6:
			RaiseWindowWidget(w, _opt_newgame.landscape + 3);
			SetNewLandscapeType(e->we.click.widget - 3);
			break;
		case 7: case 8: // Mapsize X
			ShowDropDownMenu(w, mapsizes, _patches_newgame.map_x - 6, 8, 0, 0);
			break;
		case 9: case 10: // Mapsize Y
			ShowDropDownMenu(w, mapsizes, _patches_newgame.map_y - 6, 10, 0, 0);
			break;
		case 11: // Empty world / flat world
			StartGeneratingLandscape(GLWP_SCENARIO);
			break;
		case 12: // Generate
			_goto_editor = true;
			ShowGenerateLandscape();
			break;
		case 13: // Heightmap
			_goto_editor = true;
			ShowSaveLoadDialog(SLD_LOAD_HEIGHTMAP);
			break;
		case 14: case 16: // Year buttons
			/* Don't allow too fast scrolling */
			if ((w->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
				HandleButtonClick(w, e->we.click.widget);
				SetWindowDirty(w);

				_patches_newgame.starting_year = clamp(_patches_newgame.starting_year + e->we.click.widget - 15, MIN_YEAR, MAX_YEAR);
			}
			_left_button_clicked = false;
			break;
		case 15: // Year text
			WP(w, def_d).data_3 = START_DATE_QUERY;
			SetDParam(0, _patches_newgame.starting_year);
			ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_START_DATE_QUERY_CAPT, 8, 100, WC_GENERATE_LANDSCAPE, GLWP_SCENARIO, CS_NUMERAL);
			break;
		case 17: case 19: // Height level buttons
			/* Don't allow too fast scrolling */
			if ((w->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
				HandleButtonClick(w, e->we.click.widget);
				SetWindowDirty(w);

				_patches_newgame.se_flat_world_height = clamp(_patches_newgame.se_flat_world_height + e->we.click.widget - 18, 0, 15);
			}
			_left_button_clicked = false;
			break;
		case 18: // Height level text
			WP(w, def_d).data_3 = FLAT_WORLD_HEIGHT_QUERY;
			SetDParam(0, _patches_newgame.se_flat_world_height);
			ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_FLAT_WORLD_HEIGHT_QUERY_CAPT, 3, 100, WC_GENERATE_LANDSCAPE, GLWP_SCENARIO, CS_NUMERAL);
			break;
		}
		break;

	case WE_DROPDOWN_SELECT:
		switch (e->we.dropdown.button) {
			case 8:  _patches_newgame.map_x = e->we.dropdown.index + 6; break;
			case 10: _patches_newgame.map_y = e->we.dropdown.index + 6; break;
		}
		SetWindowDirty(w);
		break;

	case WE_DESTROY:
		_goto_editor = false;
		break;

	case WE_ON_EDIT_TEXT: {
		if (e->we.edittext.str != NULL) {
			int32 value = atoi(e->we.edittext.str);

			switch (WP(w, def_d).data_3) {
			case START_DATE_QUERY:
				InvalidateWidget(w, 15);
				_patches_newgame.starting_year = clamp(value, MIN_YEAR, MAX_YEAR);
				break;
			case FLAT_WORLD_HEIGHT_QUERY:
				InvalidateWidget(w, 18);
				_patches_newgame.se_flat_world_height = clamp(value, 0, 15);
				break;
			}

			SetWindowDirty(w);
		}
		break;
	}
	}
}

const Widget _create_scenario_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE, 13,   0,  10,   0,  13, STR_00C5,                STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION, RESIZE_NONE, 13,  11, 337,   0,  13, STR_SE_CAPTION,          STR_NULL},
{      WWT_PANEL, RESIZE_NONE, 13,   0, 337,  14, 179, 0x0,                     STR_NULL},

{   WWT_IMGBTN_2, RESIZE_NONE, 12,  10,  86,  24,  78, SPR_SELECT_TEMPERATE,    STR_030E_SELECT_TEMPERATE_LANDSCAPE},
{   WWT_IMGBTN_2, RESIZE_NONE, 12,  90, 166,  24,  78, SPR_SELECT_SUB_ARCTIC,   STR_030F_SELECT_SUB_ARCTIC_LANDSCAPE},
{   WWT_IMGBTN_2, RESIZE_NONE, 12, 170, 246,  24,  78, SPR_SELECT_SUB_TROPICAL, STR_0310_SELECT_SUB_TROPICAL_LANDSCAPE},
{   WWT_IMGBTN_2, RESIZE_NONE, 12, 250, 326,  24,  78, SPR_SELECT_TOYLAND,      STR_0311_SELECT_TOYLAND_LANDSCAPE},

{      WWT_PANEL, RESIZE_NONE, 12, 162, 197,  95, 106, 0x0,                     STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 198, 209,  95, 106, STR_0225,                STR_NULL}, // Mapsize X
{      WWT_PANEL, RESIZE_NONE, 12, 228, 263,  95, 106, 0x0,                     STR_NULL},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 264, 275,  95, 106, STR_0225,                STR_NULL}, // Mapsize Y

{    WWT_TEXTBTN, RESIZE_NONE,  6,  12, 145, 117, 128, STR_SE_FLAT_WORLD,       STR_SE_FLAT_WORLD_TIP},                      // Empty (sea-level) map
{    WWT_TEXTBTN, RESIZE_NONE,  6,  12, 145, 135, 146, STR_SE_RANDOM_LAND,      STR_022A_GENERATE_RANDOM_LAND}, // Generate
{    WWT_TEXTBTN, RESIZE_NONE,  6,  12, 145, 153, 164, STR_LOAD_GAME_HEIGHTMAP, STR_LOAD_SCEN_HEIGHTMAP},       // Heightmap

{     WWT_IMGBTN, RESIZE_NONE, 12, 216, 227, 117, 128, SPR_ARROW_DOWN,          STR_029E_MOVE_THE_STARTING_DATE},
{      WWT_PANEL, RESIZE_NONE, 12, 228, 314, 117, 128, 0x0,                     STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 315, 326, 117, 128, SPR_ARROW_UP,            STR_029F_MOVE_THE_STARTING_DATE},

{     WWT_IMGBTN, RESIZE_NONE, 12, 282, 293, 135, 146, SPR_ARROW_DOWN,          STR_FLAT_WORLD_HEIGHT_DOWN},
{      WWT_PANEL, RESIZE_NONE, 12, 294, 314, 135, 146, 0x0,                     STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 315, 326, 135, 146, SPR_ARROW_UP,            STR_FLAT_WORLD_HEIGHT_UP},
{   WIDGETS_END},
};

const WindowDesc _create_scenario_desc = {
	WDP_CENTER, WDP_CENTER, 338, 180,
	WC_GENERATE_LANDSCAPE, 0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_create_scenario_widgets,
	CreateScenarioWndProc,
};

void ShowCreateScenario(void)
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

typedef struct tp_info {
	uint percent;
	StringID class;
	uint current;
	uint total;
	int timer;
} tp_info;

static tp_info _tp;

static void AbortGeneratingWorldCallback(bool ok_clicked)
{
	if (ok_clicked) AbortGeneratingWorld();
	else if (IsGeneratingWorld() && !IsGeneratingWorldAborted()) SetMouseCursor(SPR_CURSOR_ZZZ);
}

static void ShowTerrainProgressProc(Window* w, WindowEvent* e)
{
	switch (e->event) {
	case WE_CLICK:
		switch (e->we.click.widget) {
		case 2:
			if (_cursor.sprite == SPR_CURSOR_ZZZ) SetMouseCursor(SPR_CURSOR_MOUSE);
			ShowQuery(STR_GENERATION_ABORT_CAPTION, STR_GENERATION_ABORT_MESSAGE, AbortGeneratingWorldCallback, WC_GENERATE_PROGRESS_WINDOW, 0);
			break;
		}
		break;

	case WE_PAINT:
		DrawWindowWidgets(w);

		/* Draw the % complete with a bar and a text */
		DrawFrameRect(19, 20, (w->width - 18), 37, 14, FR_BORDERONLY);
		DrawFrameRect(20, 21, (int)((w->width - 40) * _tp.percent / 100) + 20, 36, 10, 0);
		SetDParam(0, _tp.percent);
		DrawStringCentered(90, 25, STR_PROGRESS, 0);

		/* Tell which class we are generating */
		DrawStringCentered(90, 46, _tp.class, 0);

		/* And say where we are in that class */
		SetDParam(0, _tp.current);
		SetDParam(1, _tp.total);
		DrawStringCentered(90, 58, STR_GENERATION_PROGRESS, 0);

		SetWindowDirty(w);
		break;
	}
}

static const WindowDesc _show_terrain_progress_desc = {
	WDP_CENTER, WDP_CENTER, 181, 97,
	WC_GENERATE_PROGRESS_WINDOW, 0,
	WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_show_terrain_progress_widgets,
	ShowTerrainProgressProc
};

/**
 * Initializes the progress counters to the starting point.
 */
void PrepareGenerateWorldProgress(void)
{
	_tp.class   = STR_WORLD_GENERATION;
	_tp.current = 0;
	_tp.total   = 0;
	_tp.percent = 0;
	_tp.timer   = 0; // Forces to paint the progress window immediatelly
}

/**
 * Show the window where a user can follow the process of the map generation.
 */
void ShowGenerateWorldProgress(void)
{
	AllocateWindowDescFront(&_show_terrain_progress_desc, 0);
}

static void _SetGeneratingWorldProgress(gwp_class class, uint progress, uint total)
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

	assert(class < GWP_CLASS_COUNT);

	/* Do not run this function if we aren't in a thread */
	if (!IsGenerateWorldThreaded() && !_network_dedicated) return;

	if (IsGeneratingWorldAborted()) HandleGeneratingWorldAbortion();

	if (total == 0) {
		assert(_tp.class == class_table[class]);
		_tp.current += progress;
	} else {
		_tp.class   = class_table[class];
		_tp.current = progress;
		_tp.total   = total;
		_tp.percent = percent_table[class];
	}

	/* Don't update the screen too often. So update it once in the
	 * _patches.progress_update_interval. However, the _tick_counter
	 * increases with 8 every 30ms, so compensate for that. */
	if (!_network_dedicated && _tp.timer != 0 && _timer_counter - _tp.timer < (_patches.progress_update_interval * 8 / 30)) return;

	/* Percentage is about the number of completed tasks, so 'current - 1' */
	_tp.percent = percent_table[class] + (percent_table[class + 1] - percent_table[class]) * (_tp.current == 0 ? 0 : _tp.current - 1) / _tp.total;
	_tp.timer = _timer_counter;

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

		DEBUG(net, 1)("Percent complete: %d", _tp.percent);
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
}

/**
 * Set the total of a stage of the world generation.
 * @param class the current class we are in.
 * @param total Set the total expected items for this class.
 *
 * Warning: this function isn't clever. Don't go from class 4 to 3. Go upwards, always.
 *  Also, progress works if total is zero, total works if progress is zero.
 */
void SetGeneratingWorldProgress(gwp_class class, uint total)
{
	if (total == 0) return;

	_SetGeneratingWorldProgress(class, 0, total);
}

/**
 * Increases the current stage of the world generation with one.
 * @param class the current class we are in.
 *
 * Warning: this function isn't clever. Don't go from class 4 to 3. Go upwards, always.
 *  Also, progress works if total is zero, total works if progress is zero.
 */
void IncreaseGeneratingWorldProgress(gwp_class class)
{
	/* In fact the param 'class' isn't needed.. but for some security reasons, we want it around */
	_SetGeneratingWorldProgress(class, 1, 0);
}
