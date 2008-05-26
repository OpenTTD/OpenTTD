/* $Id$ */

/** @file genworld_gui.cpp GUI to configure and show progress during map generation. */

#include "stdafx.h"
#include "openttd.h"
#include "heightmap.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "gfxinit.h"
#include "command_func.h"
#include "variables.h"
#include "settings_func.h"
#include "debug.h"
#include "genworld.h"
#include "network/network.h"
#include "thread.h"
#include "newgrf_config.h"
#include "strings_func.h"
#include "window_func.h"
#include "date_func.h"
#include "sound_func.h"
#include "fios.h"
#include "string_func.h"
#include "gfx_func.h"
#include "settings_type.h"
#include "widgets/dropdown_type.h"
#include "widgets/dropdown_func.h"
#include "core/random_func.hpp"
#include "landscape_type.h"
#include "querystring_gui.h"

#include "table/strings.h"
#include "table/sprites.h"

/**
 * In what 'mode' the GenerateLandscapeWindowProc is.
 */
enum glwp_modes {
	GLWP_GENERATE,
	GLWP_HEIGHTMAP,
	GLWP_SCENARIO,
	GLWP_END
};

extern void SwitchMode(int new_mode);

static inline void SetNewLandscapeType(byte landscape)
{
	_settings_newgame.game_creation.landscape = landscape;
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

	GLAND_RANDOM_TEXT,
	GLAND_RANDOM_EDITBOX,
	GLAND_RANDOM_BUTTON,

	GLAND_GENERATE_BUTTON,

	GLAND_START_DATE_TEXT1,
	GLAND_START_DATE_DOWN,
	GLAND_START_DATE_TEXT,
	GLAND_START_DATE_UP,

	GLAND_SNOW_LEVEL_TEXT1,
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

{       WWT_TEXT, RESIZE_NONE,  0,  12, 110,  91, 101, STR_MAPSIZE,                  STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 114, 161,  90, 101, STR_NUM_1,                    STR_NULL}, // Mapsize X
{       WWT_TEXT, RESIZE_NONE,  0, 168, 176,  91, 101, STR_BY,                       STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 180, 227,  90, 101, STR_NUM_2,                    STR_NULL}, // Mapsize Y

{       WWT_TEXT, RESIZE_NONE,  0,  12, 110, 113, 123, STR_NUMBER_OF_TOWNS,          STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 114, 175, 112, 123, 0x0,                          STR_NULL}, // Number of towns

{       WWT_TEXT, RESIZE_NONE,  0,  12, 110, 131, 141, STR_NUMBER_OF_INDUSTRIES,     STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 114, 175, 130, 141, 0x0,                          STR_NULL}, // Number of industries

{       WWT_TEXT, RESIZE_NONE,  0,  12, 110, 153, 163, STR_RANDOM_SEED,              STR_NULL},
{    WWT_EDITBOX, RESIZE_NONE, 15, 114, 207, 152, 163, STR_RANDOM_SEED_OSKTITLE,     STR_RANDOM_SEED_HELP}, // Edit box for seed
{    WWT_TEXTBTN, RESIZE_NONE, 12, 216, 326, 152, 163, STR_RANDOM,                   STR_RANDOM_HELP},

{    WWT_TEXTBTN, RESIZE_NONE,  6, 243, 326, 228, 257, STR_GENERATE,                 STR_NULL}, // Generate button

{       WWT_TEXT, RESIZE_NONE,  0, 182, 212, 113, 123, STR_DATE,                     STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 216, 227, 112, 123, SPR_ARROW_DOWN,               STR_029E_MOVE_THE_STARTING_DATE},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 228, 314, 112, 123, STR_GENERATE_DATE,            STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 315, 326, 112, 123, SPR_ARROW_UP,                 STR_029F_MOVE_THE_STARTING_DATE},

{       WWT_TEXT, RESIZE_NONE,  0, 182, 278, 131, 141, STR_SNOW_LINE_HEIGHT,         STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 282, 293, 130, 141, SPR_ARROW_DOWN,               STR_SNOW_LINE_DOWN},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 294, 314, 130, 141, STR_NUM_3,                    STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 315, 326, 130, 141, SPR_ARROW_UP,                 STR_SNOW_LINE_UP},

{       WWT_TEXT, RESIZE_NONE,  0,  12, 110, 193, 203, STR_TREE_PLACER,              STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 114, 231, 192, 203, 0x0,                          STR_NULL}, // Tree placer

{       WWT_TEXT, RESIZE_NONE,  0,  12, 110, 175, 185, STR_LAND_GENERATOR,           STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 114, 231, 174, 185, 0x0,                          STR_NULL}, // Landscape generator

{       WWT_TEXT, RESIZE_NONE,  0,  12, 110, 211, 221, STR_TERRAIN_TYPE,             STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 114, 231, 210, 221, 0x0,                          STR_NULL}, // Terrain type

{       WWT_TEXT, RESIZE_NONE,  0,  12, 110, 229, 239, STR_QUANTITY_OF_SEA_LAKES,    STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 114, 231, 228, 239, 0x0,                          STR_NULL}, // Water quantity

{       WWT_TEXT, RESIZE_NONE,  0,  12, 110, 245, 257, STR_SMOOTHNESS,               STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 114, 231, 246, 257, 0x0,                          STR_NULL}, // Map smoothness
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

{       WWT_TEXT, RESIZE_NONE,  0,  12, 110, 113, 123, STR_MAPSIZE,                  STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 114, 161, 112, 123, STR_NUM_1,                    STR_NULL}, // Mapsize X
{       WWT_TEXT, RESIZE_NONE,  0, 168, 176, 113, 123, STR_BY,                       STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 180, 227, 112, 123, STR_NUM_2,                    STR_NULL}, // Mapsize Y

{       WWT_TEXT, RESIZE_NONE,  0,  12, 110, 135, 145, STR_NUMBER_OF_TOWNS,          STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 114, 175, 134, 145, 0x0,                          STR_NULL}, // Number of towns

{       WWT_TEXT, RESIZE_NONE,  0,  12, 110, 153, 163, STR_NUMBER_OF_INDUSTRIES,     STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 114, 175, 152, 163, 0x0,                          STR_NULL}, // Number of industries

{       WWT_TEXT, RESIZE_NONE,  0,  12, 110, 175, 185, STR_RANDOM_SEED,              STR_NULL},
{    WWT_EDITBOX, RESIZE_NONE, 15, 114, 207, 174, 185, STR_RANDOM_SEED_OSKTITLE,     STR_RANDOM_SEED_HELP}, // Edit box for seed
{    WWT_TEXTBTN, RESIZE_NONE, 12, 216, 326, 174, 185, STR_RANDOM,                   STR_RANDOM_HELP},

{    WWT_TEXTBTN, RESIZE_NONE,  6, 243, 326, 196, 225, STR_GENERATE,                 STR_NULL}, // Generate button

{       WWT_TEXT, RESIZE_NONE,  0, 182, 212, 135, 145, STR_DATE,                     STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 216, 227, 134, 145, SPR_ARROW_DOWN,               STR_029E_MOVE_THE_STARTING_DATE},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 228, 314, 134, 145, STR_GENERATE_DATE,            STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 315, 326, 134, 145, SPR_ARROW_UP,                 STR_029F_MOVE_THE_STARTING_DATE},

{       WWT_TEXT, RESIZE_NONE,  0, 182, 278, 153, 163, STR_SNOW_LINE_HEIGHT,         STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 282, 293, 152, 163, SPR_ARROW_DOWN,               STR_SNOW_LINE_DOWN},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 294, 314, 152, 163, STR_NUM_3,                    STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 315, 326, 152, 163, SPR_ARROW_UP,                 STR_SNOW_LINE_UP},

{       WWT_TEXT, RESIZE_NONE,  0,  12, 110, 197, 207, STR_TREE_PLACER,              STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 114, 231, 196, 207, STR_0225,                     STR_NULL}, // Tree placer

{       WWT_TEXT, RESIZE_NONE,  0,  12, 110, 215, 225, STR_HEIGHTMAP_ROTATION,       STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 114, 231, 214, 225, STR_0225,                     STR_NULL}, // Heightmap rotation

{   WIDGETS_END},
};

void StartGeneratingLandscape(glwp_modes mode)
{
	DeleteAllNonVitalWindows();

	/* Copy all XXX_newgame to XXX when coming from outside the editor */
	UpdatePatches();
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

static DropDownList *BuildMapsizeDropDown()
{
	DropDownList *list = new DropDownList();

	for (uint i = 6; i <= 11; i++) {
		DropDownListParamStringItem *item = new DropDownListParamStringItem(STR_JUST_INT, i, false);
		item->SetParam(0, 1 << i);
		list->push_back(item);
	}

	return list;
}

static const StringID _elevations[]  = {STR_682A_VERY_FLAT, STR_682B_FLAT, STR_682C_HILLY, STR_682D_MOUNTAINOUS, INVALID_STRING_ID};
static const StringID _sea_lakes[]   = {STR_VERY_LOW, STR_6820_LOW, STR_6821_MEDIUM, STR_6822_HIGH, INVALID_STRING_ID};
static const StringID _smoothness[]  = {STR_CONFIG_PATCHES_ROUGHNESS_OF_TERRAIN_VERY_SMOOTH, STR_CONFIG_PATCHES_ROUGHNESS_OF_TERRAIN_SMOOTH, STR_CONFIG_PATCHES_ROUGHNESS_OF_TERRAIN_ROUGH, STR_CONFIG_PATCHES_ROUGHNESS_OF_TERRAIN_VERY_ROUGH, INVALID_STRING_ID};
static const StringID _tree_placer[] = {STR_CONFIG_PATCHES_TREE_PLACER_NONE, STR_CONFIG_PATCHES_TREE_PLACER_ORIGINAL, STR_CONFIG_PATCHES_TREE_PLACER_IMPROVED, INVALID_STRING_ID};
static const StringID _rotation[]    = {STR_CONFIG_PATCHES_HEIGHTMAP_ROTATION_COUNTER_CLOCKWISE, STR_CONFIG_PATCHES_HEIGHTMAP_ROTATION_CLOCKWISE, INVALID_STRING_ID};
static const StringID _landscape[]   = {STR_CONFIG_PATCHES_LAND_GENERATOR_ORIGINAL, STR_CONFIG_PATCHES_LAND_GENERATOR_TERRA_GENESIS, INVALID_STRING_ID};
static const StringID _num_towns[]   = {STR_NUM_VERY_LOW, STR_6816_LOW, STR_6817_NORMAL, STR_6818_HIGH, INVALID_STRING_ID};
static const StringID _num_inds[]    = {STR_NONE, STR_NUM_VERY_LOW, STR_6816_LOW, STR_6817_NORMAL, STR_6818_HIGH, INVALID_STRING_ID};

struct GenerateLandscapeWindow : public QueryStringBaseWindow {
	uint widget_id;
	uint x;
	uint y;
	char name[64];
	glwp_modes mode;

	GenerateLandscapeWindow(const WindowDesc *desc, WindowNumber number = 0) : QueryStringBaseWindow(desc, number)
	{
		this->LowerWidget(_settings_newgame.game_creation.landscape + GLAND_TEMPERATE);

		snprintf(this->edit_str_buf, sizeof(this->edit_str_buf), "%u", _settings_newgame.game_creation.generation_seed);
		InitializeTextBuffer(&this->text, this->edit_str_buf, lengthof(this->edit_str_buf), 120);
		this->caption = STR_NULL;
		this->afilter = CS_NUMERAL;

		this->mode = (glwp_modes)this->window_number;

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		/* You can't select smoothness if not terragenesis */
		if (mode == GLWP_GENERATE) {
			this->SetWidgetDisabledState(GLAND_SMOOTHNESS_PULLDOWN, _settings_newgame.game_creation.land_generator == 0);
		}
		/* Disable snowline if not hilly */
		this->SetWidgetDisabledState(GLAND_SNOW_LEVEL_TEXT, _settings_newgame.game_creation.landscape != LT_ARCTIC);
		/* Disable town, industry and trees in SE */
		this->SetWidgetDisabledState(GLAND_TOWN_PULLDOWN,     _game_mode == GM_EDITOR);
		this->SetWidgetDisabledState(GLAND_INDUSTRY_PULLDOWN, _game_mode == GM_EDITOR);
		this->SetWidgetDisabledState(GLAND_TREE_PULLDOWN,     _game_mode == GM_EDITOR);

		this->SetWidgetDisabledState(GLAND_START_DATE_DOWN, _settings_newgame.game_creation.starting_year <= MIN_YEAR);
		this->SetWidgetDisabledState(GLAND_START_DATE_UP,   _settings_newgame.game_creation.starting_year >= MAX_YEAR);
		this->SetWidgetDisabledState(GLAND_SNOW_LEVEL_DOWN, _settings_newgame.game_creation.snow_line_height <= 2 || _settings_newgame.game_creation.landscape != LT_ARCTIC);
		this->SetWidgetDisabledState(GLAND_SNOW_LEVEL_UP,   _settings_newgame.game_creation.snow_line_height >= MAX_SNOWLINE_HEIGHT || _settings_newgame.game_creation.landscape != LT_ARCTIC);

		this->SetWidgetLoweredState(GLAND_TEMPERATE, _settings_newgame.game_creation.landscape == LT_TEMPERATE);
		this->SetWidgetLoweredState(GLAND_ARCTIC,    _settings_newgame.game_creation.landscape == LT_ARCTIC);
		this->SetWidgetLoweredState(GLAND_TROPICAL,  _settings_newgame.game_creation.landscape == LT_TROPIC);
		this->SetWidgetLoweredState(GLAND_TOYLAND,   _settings_newgame.game_creation.landscape == LT_TOYLAND);

		if (_game_mode == GM_EDITOR) {
			this->widget[GLAND_TOWN_PULLDOWN].data     = STR_6836_OFF;
			this->widget[GLAND_INDUSTRY_PULLDOWN].data = STR_6836_OFF;
		} else {
			this->widget[GLAND_TOWN_PULLDOWN].data     = _num_towns[_settings_newgame.difficulty.number_towns];
			this->widget[GLAND_INDUSTRY_PULLDOWN].data = _num_inds[_settings_newgame.difficulty.number_industries];
		}

		if (mode == GLWP_GENERATE) {
			this->widget[GLAND_LANDSCAPE_PULLDOWN].data  = _landscape[_settings_newgame.game_creation.land_generator];
			this->widget[GLAND_TREE_PULLDOWN].data       = _tree_placer[_settings_newgame.game_creation.tree_placer];
			this->widget[GLAND_TERRAIN_PULLDOWN].data    = _elevations[_settings_newgame.difficulty.terrain_type];
			this->widget[GLAND_WATER_PULLDOWN].data      = _sea_lakes[_settings_newgame.difficulty.quantity_sea_lakes];
			this->widget[GLAND_SMOOTHNESS_PULLDOWN].data = _smoothness[_settings_newgame.game_creation.tgen_smoothness];
		} else {
			this->widget[GLAND_TREE_PULLDOWN].data               = _tree_placer[_settings_newgame.game_creation.tree_placer];
			this->widget[GLAND_HEIGHTMAP_ROTATION_PULLDOWN].data = _rotation[_settings_newgame.game_creation.heightmap_rotation];
		}

		/* Set parameters for widget text that requires them. */
		SetDParam(0, ConvertYMDToDate(_settings_newgame.game_creation.starting_year, 0, 1)); // GLAND_START_DATE_TEXT
		SetDParam(1, 1 << _settings_newgame.game_creation.map_x); // GLAND_MAPSIZE_X_PULLDOWN
		SetDParam(2, 1 << _settings_newgame.game_creation.map_y); // GLAND_MAPSIZE_Y_PULLDOWN
		SetDParam(3, _settings_newgame.game_creation.snow_line_height); // GLAND_SNOW_LEVEL_TEXT

		this->DrawWidgets();

		this->DrawEditBox(GLAND_RANDOM_EDITBOX);

		if (mode != GLWP_GENERATE) {
			char buffer[512];

			if (_settings_newgame.game_creation.heightmap_rotation == HM_CLOCKWISE) {
				SetDParam(0, this->y);
				SetDParam(1, this->x);
			} else {
				SetDParam(0, this->x);
				SetDParam(1, this->y);
			}
			GetString(buffer, STR_HEIGHTMAP_SIZE, lastof(buffer));
			DrawStringRightAligned(326, 91, STR_HEIGHTMAP_SIZE, TC_BLACK);

			DrawString( 12,  91, STR_HEIGHTMAP_NAME, TC_BLACK);
			SetDParamStr(0, this->name);
			DrawStringTruncated(114,  91, STR_ORANGE, TC_BLACK, 326 - 114 - GetStringBoundingBox(buffer).width - 5);
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case 0: delete this; break;

			case GLAND_TEMPERATE:
			case GLAND_ARCTIC:
			case GLAND_TROPICAL:
			case GLAND_TOYLAND:
				this->RaiseWidget(_settings_newgame.game_creation.landscape + GLAND_TEMPERATE);
				SetNewLandscapeType(widget - GLAND_TEMPERATE);
				break;

			case GLAND_MAPSIZE_X_PULLDOWN: // Mapsize X
				ShowDropDownList(this, BuildMapsizeDropDown(), _settings_newgame.game_creation.map_x, GLAND_MAPSIZE_X_PULLDOWN);
				break;

			case GLAND_MAPSIZE_Y_PULLDOWN: // Mapsize Y
				ShowDropDownList(this, BuildMapsizeDropDown(), _settings_newgame.game_creation.map_y, GLAND_MAPSIZE_Y_PULLDOWN);
				break;

			case GLAND_TOWN_PULLDOWN: // Number of towns
				ShowDropDownMenu(this, _num_towns, _settings_newgame.difficulty.number_towns, GLAND_TOWN_PULLDOWN, 0, 0);
				break;

			case GLAND_INDUSTRY_PULLDOWN: // Number of industries
				ShowDropDownMenu(this, _num_inds, _settings_newgame.difficulty.number_industries, GLAND_INDUSTRY_PULLDOWN, 0, 0);
				break;

			case GLAND_RANDOM_BUTTON: // Random seed
				_settings_newgame.game_creation.generation_seed = InteractiveRandom();
				snprintf(this->edit_str_buf, lengthof(this->edit_str_buf), "%u", _settings_newgame.game_creation.generation_seed);
				UpdateTextBufferSize(&this->text);
				this->SetDirty();
				break;

			case GLAND_RANDOM_EDITBOX: // edit box for random seed
				ShowOnScreenKeyboard(this, GLAND_RANDOM_EDITBOX, 0, 0);
				break;

			case GLAND_GENERATE_BUTTON: // Generate
				UpdatePatches();

				if (_settings.economy.town_layout == TL_NO_ROADS) {
					ShowQuery(
						STR_TOWN_LAYOUT_WARNING_CAPTION,
						STR_TOWN_LAYOUT_WARNING_MESSAGE,
						this,
						LandscapeGenerationCallback);
				} else if (mode == GLWP_HEIGHTMAP &&
						(this->x * 2 < (1U << _settings_newgame.game_creation.map_x) ||
						this->x / 2 > (1U << _settings_newgame.game_creation.map_x) ||
						this->y * 2 < (1U << _settings_newgame.game_creation.map_y) ||
						this->y / 2 > (1U << _settings_newgame.game_creation.map_y))) {
					ShowQuery(
						STR_HEIGHTMAP_SCALE_WARNING_CAPTION,
						STR_HEIGHTMAP_SCALE_WARNING_MESSAGE,
						this,
						LandscapeGenerationCallback);
				} else {
					StartGeneratingLandscape(mode);
				}
				break;

			case GLAND_START_DATE_DOWN:
			case GLAND_START_DATE_UP: // Year buttons
				/* Don't allow too fast scrolling */
				if ((this->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
					this->HandleButtonClick(widget);
					this->SetDirty();

					_settings_newgame.game_creation.starting_year = Clamp(_settings_newgame.game_creation.starting_year + widget - GLAND_START_DATE_TEXT, MIN_YEAR, MAX_YEAR);
				}
				_left_button_clicked = false;
				break;

			case GLAND_START_DATE_TEXT: // Year text
				this->widget_id = GLAND_START_DATE_TEXT;
				SetDParam(0, _settings_newgame.game_creation.starting_year);
				ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_START_DATE_QUERY_CAPT, 8, 100, this, CS_NUMERAL);
				break;

			case GLAND_SNOW_LEVEL_DOWN:
			case GLAND_SNOW_LEVEL_UP: // Snow line buttons
				/* Don't allow too fast scrolling */
				if ((this->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
					this->HandleButtonClick(widget);
					this->SetDirty();

					_settings_newgame.game_creation.snow_line_height = Clamp(_settings_newgame.game_creation.snow_line_height + widget - GLAND_SNOW_LEVEL_TEXT, 2, MAX_SNOWLINE_HEIGHT);
				}
				_left_button_clicked = false;
				break;

			case GLAND_SNOW_LEVEL_TEXT: // Snow line text
				this->widget_id = GLAND_SNOW_LEVEL_TEXT;
				SetDParam(0, _settings_newgame.game_creation.snow_line_height);
				ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_SNOW_LINE_QUERY_CAPT, 3, 100, this, CS_NUMERAL);
				break;

			case GLAND_TREE_PULLDOWN: // Tree placer
				ShowDropDownMenu(this, _tree_placer, _settings_newgame.game_creation.tree_placer, GLAND_TREE_PULLDOWN, 0, 0);
				break;

			case GLAND_LANDSCAPE_PULLDOWN: // Landscape generator OR Heightmap rotation
			/* case GLAND_HEIGHTMAP_ROTATION_TEXT: case GLAND_HEIGHTMAP_ROTATION_PULLDOWN:*/
				if (mode == GLWP_HEIGHTMAP) {
					ShowDropDownMenu(this, _rotation, _settings_newgame.game_creation.heightmap_rotation, GLAND_HEIGHTMAP_ROTATION_PULLDOWN, 0, 0);
				} else {
					ShowDropDownMenu(this, _landscape, _settings_newgame.game_creation.land_generator, GLAND_LANDSCAPE_PULLDOWN, 0, 0);
				}
				break;

			case GLAND_TERRAIN_PULLDOWN: // Terrain type
				ShowDropDownMenu(this, _elevations, _settings_newgame.difficulty.terrain_type, GLAND_TERRAIN_PULLDOWN, 0, 0);
				break;

			case GLAND_WATER_PULLDOWN: // Water quantity
				ShowDropDownMenu(this, _sea_lakes, _settings_newgame.difficulty.quantity_sea_lakes, GLAND_WATER_PULLDOWN, 0, 0);
				break;

			case GLAND_SMOOTHNESS_PULLDOWN: // Map smoothness
				ShowDropDownMenu(this, _smoothness, _settings_newgame.game_creation.tgen_smoothness, GLAND_SMOOTHNESS_PULLDOWN, 0, 0);
				break;
		}
	}

	virtual void OnMouseLoop()
	{
		this->HandleEditBox(GLAND_RANDOM_EDITBOX);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		EventState state;
		this->HandleEditBoxKey(GLAND_RANDOM_EDITBOX, key, keycode, state);
		/* the seed is unsigned, therefore atoi cannot be used.
			* As 2^32 - 1 (MAX_UVALUE(uint32)) is a 'magic' value
			* (use random seed) it should not be possible to be
			* entered into the input field; the generate seed
			* button can be used instead. */
		_settings_newgame.game_creation.generation_seed = minu(strtoul(this->edit_str_buf, NULL, sizeof(this->edit_str_buf) - 1), MAX_UVALUE(uint32) - 1);
		return state;
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case GLAND_MAPSIZE_X_PULLDOWN:  _settings_newgame.game_creation.map_x = index; break;
			case GLAND_MAPSIZE_Y_PULLDOWN:  _settings_newgame.game_creation.map_y = index; break;
			case GLAND_TREE_PULLDOWN:       _settings_newgame.game_creation.tree_placer = index; break;
			case GLAND_SMOOTHNESS_PULLDOWN: _settings_newgame.game_creation.tgen_smoothness = index;  break;

			case GLAND_TOWN_PULLDOWN:
				_settings_newgame.difficulty.number_towns = index;
				if (_settings_newgame.difficulty.diff_level != 3) ShowErrorMessage(INVALID_STRING_ID, STR_DIFFICULTY_TO_CUSTOM, 0, 0);
				IConsoleSetPatchSetting("difficulty.number_towns", _settings_newgame.difficulty.number_towns);
				break;

			case GLAND_INDUSTRY_PULLDOWN:
				_settings_newgame.difficulty.number_industries = index;
				if (_settings_newgame.difficulty.diff_level != 3) ShowErrorMessage(INVALID_STRING_ID, STR_DIFFICULTY_TO_CUSTOM, 0, 0);
				IConsoleSetPatchSetting("difficulty.number_industries", _settings_newgame.difficulty.number_industries);
				break;

			case GLAND_LANDSCAPE_PULLDOWN:
			/* case GLAND_HEIGHTMAP_PULLDOWN: */
				if (mode == GLWP_HEIGHTMAP) {
					_settings_newgame.game_creation.heightmap_rotation = index;
				} else {
					_settings_newgame.game_creation.land_generator = index;
				}
				break;

			case GLAND_TERRAIN_PULLDOWN:
				_settings_newgame.difficulty.terrain_type = index;
				if (_settings_newgame.difficulty.diff_level != 3) ShowErrorMessage(INVALID_STRING_ID, STR_DIFFICULTY_TO_CUSTOM, 0, 0);
				IConsoleSetPatchSetting("difficulty.terrain_type", _settings_newgame.difficulty.terrain_type);
				break;

			case GLAND_WATER_PULLDOWN:
				_settings_newgame.difficulty.quantity_sea_lakes = index;
				if (_settings_newgame.difficulty.diff_level != 3) ShowErrorMessage(INVALID_STRING_ID, STR_DIFFICULTY_TO_CUSTOM, 0, 0);
				IConsoleSetPatchSetting("difficulty.quantity_sea_lakes", _settings_newgame.difficulty.quantity_sea_lakes);
				break;
		}
		this->SetDirty();
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (!StrEmpty(str)) {
			int32 value = atoi(str);

			switch (this->widget_id) {
				case GLAND_START_DATE_TEXT:
					this->InvalidateWidget(GLAND_START_DATE_TEXT);
					_settings_newgame.game_creation.starting_year = Clamp(value, MIN_YEAR, MAX_YEAR);
					break;

				case GLAND_SNOW_LEVEL_TEXT:
					this->InvalidateWidget(GLAND_SNOW_LEVEL_TEXT);
					_settings_newgame.game_creation.snow_line_height = Clamp(value, 2, MAX_SNOWLINE_HEIGHT);
					break;
			}

			this->SetDirty();
		}
	}
};

static const WindowDesc _generate_landscape_desc = {
	WDP_CENTER, WDP_CENTER, 338, 268, 338, 268,
	WC_GENERATE_LANDSCAPE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_generate_landscape_widgets,
};

static const WindowDesc _heightmap_load_desc = {
	WDP_CENTER, WDP_CENTER, 338, 236, 338, 236,
	WC_GENERATE_LANDSCAPE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS,
	_heightmap_load_widgets,
};

static void _ShowGenerateLandscape(glwp_modes mode)
{
	uint x = 0;
	uint y = 0;

	DeleteWindowByClass(WC_GENERATE_LANDSCAPE);

	/* Always give a new seed if not editor */
	if (_game_mode != GM_EDITOR) _settings_newgame.game_creation.generation_seed = InteractiveRandom();

	if (mode == GLWP_HEIGHTMAP) {
		/* If the function returns negative, it means there was a problem loading the heightmap */
		if (!GetHeightmapDimensions(_file_to_saveload.name, &x, &y)) return;
	}

	GenerateLandscapeWindow *w = AllocateWindowDescFront<GenerateLandscapeWindow>((mode == GLWP_HEIGHTMAP) ? &_heightmap_load_desc : &_generate_landscape_desc, mode);

	if (mode == GLWP_HEIGHTMAP) {
		w->x = x;
		w->y = y;
		strecpy(w->name, _file_to_saveload.title, lastof(w->name));
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
	if (_settings_newgame.economy.town_layout == TL_NO_ROADS) {
		_settings_newgame.economy.town_layout = TL_ORIGINAL;
	}

	StartGeneratingLandscape(GLWP_SCENARIO);
}

void StartNewGameWithoutGUI(uint seed)
{
	/* GenerateWorld takes care of the possible GENERATE_NEW_SEED value in 'seed' */
	_settings_newgame.game_creation.generation_seed = seed;

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
	CSCEN_START_DATE_LABEL,
	CSCEN_START_DATE_DOWN,
	CSCEN_START_DATE_TEXT,
	CSCEN_START_DATE_UP,
	CSCEN_FLAT_LAND_HEIGHT_LABEL,
	CSCEN_FLAT_LAND_HEIGHT_DOWN,
	CSCEN_FLAT_LAND_HEIGHT_TEXT,
	CSCEN_FLAT_LAND_HEIGHT_UP
};


struct CreateScenarioWindow : public Window
{
	uint widget_id;

	CreateScenarioWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->LowerWidget(_settings_newgame.game_creation.landscape + CSCEN_TEMPERATE);
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		this->SetWidgetDisabledState(CSCEN_START_DATE_DOWN,       _settings_newgame.game_creation.starting_year <= MIN_YEAR);
		this->SetWidgetDisabledState(CSCEN_START_DATE_UP,         _settings_newgame.game_creation.starting_year >= MAX_YEAR);
		this->SetWidgetDisabledState(CSCEN_FLAT_LAND_HEIGHT_DOWN, _settings_newgame.game_creation.se_flat_world_height <= 0);
		this->SetWidgetDisabledState(CSCEN_FLAT_LAND_HEIGHT_UP,   _settings_newgame.game_creation.se_flat_world_height >= MAX_TILE_HEIGHT);

		this->SetWidgetLoweredState(CSCEN_TEMPERATE, _settings_newgame.game_creation.landscape == LT_TEMPERATE);
		this->SetWidgetLoweredState(CSCEN_ARCTIC,    _settings_newgame.game_creation.landscape == LT_ARCTIC);
		this->SetWidgetLoweredState(CSCEN_TROPICAL,  _settings_newgame.game_creation.landscape == LT_TROPIC);
		this->SetWidgetLoweredState(CSCEN_TOYLAND,   _settings_newgame.game_creation.landscape == LT_TOYLAND);

		/* Set parameters for widget text that requires them */
		SetDParam(0, ConvertYMDToDate(_settings_newgame.game_creation.starting_year, 0, 1)); // CSCEN_START_DATE_TEXT
		SetDParam(1, 1 << _settings_newgame.game_creation.map_x); // CSCEN_MAPSIZE_X_PULLDOWN
		SetDParam(2, 1 << _settings_newgame.game_creation.map_y); // CSCEN_MAPSIZE_Y_PULLDOWN
		SetDParam(3, _settings_newgame.game_creation.se_flat_world_height); // CSCEN_FLAT_LAND_HEIGHT_TEXT

		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case CSCEN_TEMPERATE:
			case CSCEN_ARCTIC:
			case CSCEN_TROPICAL:
			case CSCEN_TOYLAND:
				this->RaiseWidget(_settings_newgame.game_creation.landscape + CSCEN_TEMPERATE);
				SetNewLandscapeType(widget - CSCEN_TEMPERATE);
				break;

			case CSCEN_MAPSIZE_X_PULLDOWN: // Mapsize X
				ShowDropDownList(this, BuildMapsizeDropDown(), _settings_newgame.game_creation.map_x, CSCEN_MAPSIZE_X_PULLDOWN);
				break;

			case CSCEN_MAPSIZE_Y_PULLDOWN: // Mapsize Y
				ShowDropDownList(this, BuildMapsizeDropDown(), _settings_newgame.game_creation.map_y, CSCEN_MAPSIZE_Y_PULLDOWN);
				break;

			case CSCEN_EMPTY_WORLD: // Empty world / flat world
				StartGeneratingLandscape(GLWP_SCENARIO);
				break;

			case CSCEN_RANDOM_WORLD: // Generate
				ShowGenerateLandscape();
				break;

			case CSCEN_START_DATE_DOWN:
			case CSCEN_START_DATE_UP: // Year buttons
				/* Don't allow too fast scrolling */
				if ((this->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
					this->HandleButtonClick(widget);
					this->SetDirty();

					_settings_newgame.game_creation.starting_year = Clamp(_settings_newgame.game_creation.starting_year + widget - CSCEN_START_DATE_TEXT, MIN_YEAR, MAX_YEAR);
				}
				_left_button_clicked = false;
				break;

			case CSCEN_START_DATE_TEXT: // Year text
				this->widget_id = CSCEN_START_DATE_TEXT;
				SetDParam(0, _settings_newgame.game_creation.starting_year);
				ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_START_DATE_QUERY_CAPT, 8, 100, this, CS_NUMERAL);
				break;

			case CSCEN_FLAT_LAND_HEIGHT_DOWN:
			case CSCEN_FLAT_LAND_HEIGHT_UP: // Height level buttons
				/* Don't allow too fast scrolling */
				if ((this->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
					this->HandleButtonClick(widget);
					this->SetDirty();

					_settings_newgame.game_creation.se_flat_world_height = Clamp(_settings_newgame.game_creation.se_flat_world_height + widget - CSCEN_FLAT_LAND_HEIGHT_TEXT, 0, MAX_TILE_HEIGHT);
				}
				_left_button_clicked = false;
				break;

			case CSCEN_FLAT_LAND_HEIGHT_TEXT: // Height level text
				this->widget_id = CSCEN_FLAT_LAND_HEIGHT_TEXT;
				SetDParam(0, _settings_newgame.game_creation.se_flat_world_height);
				ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_FLAT_WORLD_HEIGHT_QUERY_CAPT, 3, 100, this, CS_NUMERAL);
				break;
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case CSCEN_MAPSIZE_X_PULLDOWN: _settings_newgame.game_creation.map_x = index; break;
			case CSCEN_MAPSIZE_Y_PULLDOWN: _settings_newgame.game_creation.map_y = index; break;
		}
		this->SetDirty();
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (!StrEmpty(str)) {
			int32 value = atoi(str);

			switch (this->widget_id) {
				case CSCEN_START_DATE_TEXT:
					this->InvalidateWidget(CSCEN_START_DATE_TEXT);
					_settings_newgame.game_creation.starting_year = Clamp(value, MIN_YEAR, MAX_YEAR);
					break;

				case CSCEN_FLAT_LAND_HEIGHT_TEXT:
					this->InvalidateWidget(CSCEN_FLAT_LAND_HEIGHT_TEXT);
					_settings_newgame.game_creation.se_flat_world_height = Clamp(value, 0, MAX_TILE_HEIGHT);
					break;
			}

			this->SetDirty();
		}
	}
};

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

{       WWT_TEXT, RESIZE_NONE, 12, 182, 212,  96, 106, STR_MAPSIZE,             STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 216, 263,  95, 106, STR_NUM_1,               STR_NULL}, // Mapsize X
{       WWT_TEXT, RESIZE_NONE, 12, 268, 276,  96, 106, STR_BY,                  STR_NULL},
{   WWT_DROPDOWN, RESIZE_NONE, 12, 279, 326,  95, 106, STR_NUM_2,               STR_NULL}, // Mapsize Y

{       WWT_TEXT, RESIZE_NONE,  0, 182, 212, 114, 124, STR_DATE,                STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 216, 227, 113, 124, SPR_ARROW_DOWN,          STR_029E_MOVE_THE_STARTING_DATE},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 228, 314, 113, 124, STR_GENERATE_DATE,       STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 315, 326, 113, 124, SPR_ARROW_UP,            STR_029F_MOVE_THE_STARTING_DATE},

{       WWT_TEXT, RESIZE_NONE,  0, 182, 278, 132, 142, STR_FLAT_WORLD_HEIGHT,   STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 282, 293, 131, 142, SPR_ARROW_DOWN,          STR_FLAT_WORLD_HEIGHT_DOWN},
{    WWT_TEXTBTN, RESIZE_NONE, 12, 294, 314, 131, 142, STR_NUM_3,               STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 12, 315, 326, 131, 142, SPR_ARROW_UP,            STR_FLAT_WORLD_HEIGHT_UP},
{   WIDGETS_END},
};

static const WindowDesc _create_scenario_desc = {
	WDP_CENTER, WDP_CENTER, 338, 170, 338, 170,
	WC_GENERATE_LANDSCAPE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS,
	_create_scenario_widgets,
};

void ShowCreateScenario()
{
	DeleteWindowByClass(WC_GENERATE_LANDSCAPE);
	new CreateScenarioWindow(&_create_scenario_desc, GLWP_SCENARIO);
}


static const Widget _generate_progress_widgets[] = {
{    WWT_CAPTION,   RESIZE_NONE,    14,     0,   180,     0,    13, STR_GENERATION_WORLD,   STR_018C_WINDOW_TITLE_DRAG_THIS}, // GPWW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   180,    14,    96, 0x0,                    STR_NULL},                        // GPWW_BACKGROUND
{    WWT_TEXTBTN,   RESIZE_NONE,    15,    20,   161,    74,    85, STR_GENERATION_ABORT,   STR_NULL},                        // GPWW_ABORT
{   WIDGETS_END},
};

static const WindowDesc _generate_progress_desc = {
	WDP_CENTER, WDP_CENTER, 181, 97, 181, 97,
	WC_GENERATE_PROGRESS_WINDOW, WC_NONE,
	WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_generate_progress_widgets,
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

struct GenerateProgressWindow : public Window {
private:
	enum GenerationProgressWindowWidgets {
		GPWW_CAPTION,
		GPWW_BACKGROUND,
		GPWW_ABORT,
	};

public:
	GenerateProgressWindow() : Window(&_generate_progress_desc)
	{
		this->FindWindowPlacementAndResize(&_generate_progress_desc);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case GPWW_ABORT:
				if (_cursor.sprite == SPR_CURSOR_ZZZ) SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);
				ShowQuery(
					STR_GENERATION_ABORT_CAPTION,
					STR_GENERATION_ABORT_MESSAGE,
					this,
					AbortGeneratingWorldCallback
				);
				break;
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

		/* Draw the % complete with a bar and a text */
		DrawFrameRect(19, 20, (this->width - 18), 37, 14, FR_BORDERONLY);
		DrawFrameRect(20, 21, (int)((this->width - 40) * _tp.percent / 100) + 20, 36, 10, FR_NONE);
		SetDParam(0, _tp.percent);
		DrawStringCentered(90, 25, STR_PROGRESS, TC_FROMSTRING);

		/* Tell which class we are generating */
		DrawStringCentered(90, 46, _tp.cls, TC_FROMSTRING);

		/* And say where we are in that class */
		SetDParam(0, _tp.current);
		SetDParam(1, _tp.total);
		DrawStringCentered(90, 58, STR_GENERATION_PROGRESS, TC_FROMSTRING);

		this->SetDirty();
	}
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
	if (BringWindowToFrontById(WC_GENERATE_PROGRESS_WINDOW, 0)) return;
	new GenerateProgressWindow();
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
