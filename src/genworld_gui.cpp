/* $Id$ */

/** @file genworld_gui.cpp GUI to configure and show progress during map generation. */

#include "stdafx.h"
#include "openttd.h"
#include "heightmap.h"
#include "gui.h"
#include "variables.h"
#include "debug.h"
#include "genworld.h"
#include "network/network.h"
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
#include "town.h"
#include "thread.h"
#include "settings_func.h"

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

extern void SwitchToMode(SwitchMode new_mode);
extern void MakeNewgameSettingsLive();

static inline void SetNewLandscapeType(byte landscape)
{
	_settings_newgame.game_creation.landscape = landscape;
	InvalidateWindowClasses(WC_SELECT_GAME);
	InvalidateWindowClasses(WC_GENERATE_LANDSCAPE);
}

enum GenerateLandscapeWindowWidgets {
	GLAND_CLOSEBOX,
	GLAND_TITLEBAR,
	GLAND_BACKGROUND,

	GLAND_TEMPERATE,
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
	GLAND_SMOOTHNESS_PULLDOWN,

	GLAND_BORDER_TYPES,
	GLAND_BORDERS_RANDOM,
	GLAND_WATER_NW_TEXT,
	GLAND_WATER_NE_TEXT,
	GLAND_WATER_SE_TEXT,
	GLAND_WATER_SW_TEXT,
	GLAND_WATER_NW,
	GLAND_WATER_NE,
	GLAND_WATER_SE,
	GLAND_WATER_SW,
};

static const Widget _generate_landscape_widgets[] = {
{  WWT_CLOSEBOX,  RESIZE_NONE, COLOUR_BROWN,    0,  10,   0,  13, STR_BLACK_CROSS,              STR_TOOLTIP_CLOSE_WINDOW},                 // GLAND_CLOSEBOX
{    WWT_CAPTION, RESIZE_NONE, COLOUR_BROWN,   11, 337,   0,  13, STR_WORLD_GENERATION_CAPTION, STR_NULL},                                 // GLAND_TITLEBAR
{      WWT_PANEL, RESIZE_NONE, COLOUR_BROWN,    0, 337,  14, 313, 0x0,                          STR_NULL},                                 // GLAND_BACKGROUND

/* Landscape selection */
{   WWT_IMGBTN_2, RESIZE_NONE, COLOUR_ORANGE,  10,  86,  24,  78, SPR_SELECT_TEMPERATE,         STR_INTRO_TOOLTIP_TEMPERATE},              // GLAND_TEMPERATE
{   WWT_IMGBTN_2, RESIZE_NONE, COLOUR_ORANGE,  90, 166,  24,  78, SPR_SELECT_SUB_ARCTIC,        STR_INTRO_TOOLTIP_SUB_ARCTIC_LANDSCAPE},   // GLAND_ARCTIC
{   WWT_IMGBTN_2, RESIZE_NONE, COLOUR_ORANGE, 170, 246,  24,  78, SPR_SELECT_SUB_TROPICAL,      STR_INTRO_TOOLTIP_SUB_TROPICAL_LANDSCAPE}, // GLAND_TROPICAL
{   WWT_IMGBTN_2, RESIZE_NONE, COLOUR_ORANGE, 250, 326,  24,  78, SPR_SELECT_TOYLAND,           STR_INTRO_TOOLTIP_TOYLAND_LANDSCAPE},      // GLAND_TOYLAND

/* Mapsize X */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12, 110,  91, 101, STR_MAPSIZE,                  STR_NULL},                               // GLAND_MAPSIZE_X_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 114, 161,  90, 101, STR_NUM_1,                    STR_NULL},                               // GLAND_MAPSIZE_X_PULLDOWN
/* Mapsize Y */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE, 168, 176,  91, 101, STR_BY,                       STR_NULL},                               // GLAND_MAPSIZE_Y_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 180, 227,  90, 101, STR_NUM_2,                    STR_NULL},                               // GLAND_MAPSIZE_Y_PULLDOWN

/*  Number of towns */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12, 110, 113, 123, STR_NUMBER_OF_TOWNS,          STR_NULL},                               // GLAND_TOWN_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 114, 175, 112, 123, 0x0,                          STR_NULL},                               // GLAND_TOWN_PULLDOWN

/* Number of industries */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12, 110, 131, 141, STR_NUMBER_OF_INDUSTRIES,     STR_NULL},                               // GLAND_INDUSTRY_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 114, 175, 130, 141, 0x0,                          STR_NULL},                               // GLAND_INDUSTRY_PULLDOWN

/* Edit box for seed */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12, 110, 153, 163, STR_RANDOM_SEED,              STR_NULL},                               // GLAND_RANDOM_TEXT
{    WWT_EDITBOX, RESIZE_NONE, COLOUR_WHITE,  114, 207, 152, 163, STR_RANDOM_SEED_OSKTITLE,     STR_RANDOM_SEED_HELP},                   // GLAND_RANDOM_EDITBOX
{    WWT_TEXTBTN, RESIZE_NONE, COLOUR_ORANGE, 216, 326, 152, 163, STR_RANDOM,                   STR_RANDOM_HELP},                        // GLAND_RANDOM_BUTTON

/* Generate button */
{    WWT_TEXTBTN, RESIZE_NONE, COLOUR_GREEN, 243, 326, 228, 257, STR_GENERATE,                 STR_NULL},                                // GLAND_GENERATE_BUTTON

/* Start date */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE, 182, 212, 113, 123, STR_DATE,                     STR_NULL},                               // GLAND_START_DATE_TEXT1
{     WWT_IMGBTN, RESIZE_NONE, COLOUR_ORANGE, 216, 227, 112, 123, SPR_ARROW_DOWN,               STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_BACKWARD}, // GLAND_START_DATE_DOWN
{    WWT_TEXTBTN, RESIZE_NONE, COLOUR_ORANGE, 228, 314, 112, 123, STR_GENERATE_DATE,            STR_NULL},                               // GLAND_START_DATE_TEXT
{     WWT_IMGBTN, RESIZE_NONE, COLOUR_ORANGE, 315, 326, 112, 123, SPR_ARROW_UP,                 STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_FORWARD},  // GLAND_START_DATE_UP

/* Snow line */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE, 182, 278, 131, 141, STR_SNOW_LINE_HEIGHT,         STR_NULL},                               // GLAND_SNOW_LEVEL_TEXT1
{     WWT_IMGBTN, RESIZE_NONE, COLOUR_ORANGE, 282, 293, 130, 141, SPR_ARROW_DOWN,               STR_SNOW_LINE_DOWN},                     // GLAND_SNOW_LEVEL_DOWN
{    WWT_TEXTBTN, RESIZE_NONE, COLOUR_ORANGE, 294, 314, 130, 141, STR_NUM_3,                    STR_NULL},                               // GLAND_SNOW_LEVEL_TEXT
{     WWT_IMGBTN, RESIZE_NONE, COLOUR_ORANGE, 315, 326, 130, 141, SPR_ARROW_UP,                 STR_SNOW_LINE_UP},                       // GLAND_SNOW_LEVEL_UP

/* Tree placer */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12, 110, 193, 203, STR_TREE_PLACER,              STR_NULL},                               // GLAND_TREE_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 114, 231, 192, 203, 0x0,                          STR_NULL},                               // GLAND_TREE_PULLDOWN

/* Landscape generator */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12, 110, 175, 185, STR_LAND_GENERATOR,           STR_NULL},                               // GLAND_LANDSCAPE_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 114, 231, 174, 185, 0x0,                          STR_NULL},                               // GLAND_LANDSCAPE_PULLDOWN

/* Terrain type */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12, 110, 211, 221, STR_TERRAIN_TYPE,             STR_NULL},                               // GLAND_TERRAIN_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 114, 231, 210, 221, 0x0,                          STR_NULL},                               // GLAND_TERRAIN_PULLDOWN

/* Water quantity */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12, 110, 229, 239, STR_QUANTITY_OF_SEA_LAKES,    STR_NULL},                               // GLAND_WATER_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 114, 231, 228, 239, 0x0,                          STR_NULL},                               // GLAND_WATER_PULLDOWN

/* Map smoothness */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12, 110, 247, 257, STR_SMOOTHNESS,               STR_NULL},                               // GLAND_SMOOTHNESS_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 114, 231, 246, 257, 0x0,                          STR_NULL},                               // GLAND_SMOOTHNESS_PULLDOWN

/* Water borders */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12, 110, 265, 275, STR_BORDER_TYPE,              STR_NULL},                               // GLAND_BORDER_TYPES
{ WWT_PUSHTXTBTN, RESIZE_NONE, COLOUR_ORANGE, 114, 231, 264, 275, STR_BORDER_RANDOMIZE,         STR_NULL},                               // GLAND_BORDERS_RANDOM
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12,  95, 282, 292, STR_NORTHWEST,                STR_NULL},                               // GLAND_WATER_NW_TEXT
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE, 250, 326, 282, 292, STR_NORTHEAST,                STR_NULL},                               // GLAND_WATER_NE_TEXT
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE, 250, 326, 294, 304, STR_SOUTHEAST,                STR_NULL},                               // GLAND_WATER_SE_TEXT
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12,  95, 294, 304, STR_SOUTHWEST,                STR_NULL},                               // GLAND_WATER_SW_TEXT
{ WWT_PUSHTXTBTN, RESIZE_NONE, COLOUR_ORANGE, 100, 172, 280, 291, 0x0,                          STR_NORTHWEST},                          // GLAND_WATER_NW
{ WWT_PUSHTXTBTN, RESIZE_NONE, COLOUR_ORANGE, 173, 245, 280, 291, 0x0,                          STR_NORTHEAST},                          // GLAND_WATER_NE
{ WWT_PUSHTXTBTN, RESIZE_NONE, COLOUR_ORANGE, 173, 245, 292, 303, 0x0,                          STR_SOUTHEAST},                          // GLAND_WATER_SE
{ WWT_PUSHTXTBTN, RESIZE_NONE, COLOUR_ORANGE, 100, 172, 292, 303, 0x0,                          STR_SOUTHWEST},                          // GLAND_WATER_SW
{   WIDGETS_END},
};

static const NWidgetPart _nested_generate_landscape_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN, GLAND_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_BROWN, GLAND_TITLEBAR), SetDataTip(STR_WORLD_GENERATION_CAPTION, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, GLAND_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 10),
		/* Landscape selection. */
		NWidget(NWID_HORIZONTAL), SetPIP(10, 3, 11),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, GLAND_TEMPERATE), SetMinimalSize(77, 55), SetDataTip(SPR_SELECT_TEMPERATE, STR_INTRO_TOOLTIP_TEMPERATE),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, GLAND_ARCTIC), SetMinimalSize(77, 55), SetDataTip(SPR_SELECT_SUB_ARCTIC, STR_INTRO_TOOLTIP_SUB_ARCTIC_LANDSCAPE),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, GLAND_TROPICAL), SetMinimalSize(77, 55), SetDataTip(SPR_SELECT_SUB_TROPICAL, STR_INTRO_TOOLTIP_SUB_TROPICAL_LANDSCAPE),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, GLAND_TOYLAND), SetMinimalSize(77, 55), SetDataTip(SPR_SELECT_TOYLAND, STR_INTRO_TOOLTIP_TOYLAND_LANDSCAPE),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 11),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(12, 0),
			/* Left column with labels. */
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 1),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_MAPSIZE_X_TEXT), SetMinimalSize(99, 11), SetDataTip(STR_MAPSIZE, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 11),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_TOWN_TEXT), SetMinimalSize(99, 11), SetDataTip(STR_NUMBER_OF_TOWNS, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 7),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_INDUSTRY_TEXT), SetMinimalSize(99, 11), SetDataTip(STR_NUMBER_OF_INDUSTRIES, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 11),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_RANDOM_TEXT), SetMinimalSize(99, 11), SetDataTip(STR_RANDOM_SEED, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 11),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_LANDSCAPE_TEXT), SetMinimalSize(99, 11), SetDataTip(STR_LAND_GENERATOR, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 7),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_TREE_TEXT), SetMinimalSize(99, 11), SetDataTip(STR_TREE_PLACER, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 7),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_TERRAIN_TEXT), SetMinimalSize(99, 11), SetDataTip(STR_TERRAIN_TYPE, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 7),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_WATER_TEXT), SetMinimalSize(99, 11), SetDataTip(STR_QUANTITY_OF_SEA_LAKES, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 7),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_SMOOTHNESS_TEXT), SetMinimalSize(99, 11), SetDataTip(STR_SMOOTHNESS, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 7),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_BORDER_TYPES), SetMinimalSize(99, 11), SetDataTip(STR_BORDER_TYPE, STR_NULL),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(3, 0),
			/* Widgets at the right of the labels. */
			NWidget(NWID_VERTICAL),
				/* Mapsize X * Y. */
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_MAPSIZE_X_PULLDOWN), SetMinimalSize(48, 12), SetDataTip(STR_NUM_1, STR_NULL),
					NWidget(NWID_SPACER), SetMinimalSize(6, 0),
					NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_MAPSIZE_Y_TEXT), SetMinimalSize(9, 11), SetDataTip(STR_BY, STR_NULL), SetPadding(1, 0, 0, 0),
					NWidget(NWID_SPACER), SetMinimalSize(3, 0),
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_MAPSIZE_Y_PULLDOWN), SetMinimalSize(48, 12), SetDataTip(STR_NUM_2, STR_NULL),
					NWidget(NWID_SPACER), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 10), SetFill(1, 0),
				NWidget(NWID_HORIZONTAL),
					/* Number of towns. */
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_TOWN_PULLDOWN), SetMinimalSize(62, 12), SetDataTip(0x0, STR_NULL),
					NWidget(NWID_SPACER), SetMinimalSize(6, 0),
					/* Starting date. */
					NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_START_DATE_TEXT1), SetMinimalSize(31, 11), SetDataTip(STR_DATE, STR_NULL), SetPadding(1, 0, 0, 0),
					NWidget(NWID_SPACER), SetMinimalSize(3, 0),
					NWidget(WWT_IMGBTN, COLOUR_ORANGE, GLAND_START_DATE_DOWN), SetMinimalSize(12, 12), SetDataTip(SPR_ARROW_DOWN, STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_BACKWARD),
					NWidget(WWT_TEXTBTN, COLOUR_ORANGE, GLAND_START_DATE_TEXT), SetMinimalSize(87, 12), SetDataTip(STR_GENERATE_DATE, STR_NULL),
					NWidget(WWT_IMGBTN, COLOUR_ORANGE, GLAND_START_DATE_UP), SetMinimalSize(12, 12), SetDataTip(SPR_ARROW_UP, STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_FORWARD),
					NWidget(NWID_SPACER), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 6), SetFill(1, 0),
				NWidget(NWID_HORIZONTAL),
					/* Number of industries. */
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_INDUSTRY_PULLDOWN), SetMinimalSize(62, 12), SetDataTip(0x0, STR_NULL),
					NWidget(NWID_SPACER), SetMinimalSize(6, 0),
					/* Snow line. */
					NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_SNOW_LEVEL_TEXT1), SetMinimalSize(97, 11),
														SetDataTip(STR_SNOW_LINE_HEIGHT, STR_NULL), SetPadding(1, 0, 0, 0),
					NWidget(NWID_SPACER), SetMinimalSize(3, 0),
					NWidget(WWT_IMGBTN, COLOUR_ORANGE, GLAND_SNOW_LEVEL_DOWN), SetMinimalSize(12, 12), SetDataTip(SPR_ARROW_DOWN, STR_SNOW_LINE_DOWN),
					NWidget(WWT_TEXTBTN, COLOUR_ORANGE, GLAND_SNOW_LEVEL_TEXT), SetMinimalSize(21, 12), SetDataTip(STR_NUM_3, STR_NULL),
					NWidget(WWT_IMGBTN, COLOUR_ORANGE, GLAND_SNOW_LEVEL_UP), SetMinimalSize(12, 12), SetDataTip(SPR_ARROW_UP, STR_SNOW_LINE_UP),
					NWidget(NWID_SPACER), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 10), SetFill(1, 0),
				NWidget(NWID_HORIZONTAL),
					/* Edit box for seed. */
					NWidget(WWT_EDITBOX, COLOUR_WHITE, GLAND_RANDOM_EDITBOX), SetMinimalSize(94, 12), SetDataTip(STR_RANDOM_SEED_OSKTITLE, STR_RANDOM_SEED_HELP),
					NWidget(NWID_SPACER), SetMinimalSize(8, 0),
					NWidget(WWT_TEXTBTN, COLOUR_ORANGE, GLAND_RANDOM_BUTTON), SetMinimalSize(111, 12), SetDataTip(STR_RANDOM, STR_RANDOM_HELP),
					NWidget(NWID_SPACER), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL),
					NWidget(NWID_VERTICAL),
						NWidget(NWID_SPACER), SetMinimalSize(0, 10),
						/* Dropdown buttons. */
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_LANDSCAPE_PULLDOWN), SetMinimalSize(118, 12),
						NWidget(NWID_SPACER), SetMinimalSize(0, 6),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_TREE_PULLDOWN), SetMinimalSize(118, 12),
						NWidget(NWID_SPACER), SetMinimalSize(0, 6),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_TERRAIN_PULLDOWN), SetMinimalSize(118, 12),
						NWidget(NWID_SPACER), SetMinimalSize(0, 6),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_WATER_PULLDOWN), SetMinimalSize(118, 12),
						NWidget(NWID_SPACER), SetMinimalSize(0, 6),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_SMOOTHNESS_PULLDOWN), SetMinimalSize(118, 12),
						NWidget(NWID_SPACER), SetMinimalSize(0, 6),
						NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, GLAND_BORDERS_RANDOM), SetMinimalSize(118, 12), SetDataTip(STR_BORDER_RANDOMIZE, STR_NULL),
					EndContainer(),
					NWidget(NWID_VERTICAL),
						NWidget(WWT_TEXTBTN, COLOUR_GREEN, GLAND_GENERATE_BUTTON), SetMinimalSize(84,30),
														SetDataTip(STR_GENERATE, STR_NULL), SetPadding(64, 0, 0, 11),
						NWidget(NWID_SPACER), SetFill(0, 1),
					EndContainer(),
					NWidget(NWID_SPACER), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_SPACER), SetFill(1, 1),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 4),
		/* Map borders buttons for each edge. */
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_WATER_NW_TEXT), SetMinimalSize(84, 11), SetDataTip(STR_NORTHWEST, STR_NULL), SetPadding(2, 0, 0, 12),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_WATER_SW_TEXT), SetMinimalSize(84, 11), SetDataTip(STR_SOUTHWEST, STR_NULL), SetPadding(1, 0, 0, 12),
			EndContainer(),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, GLAND_WATER_NW), SetMinimalSize(73, 12), SetDataTip(0x0, STR_NORTHWEST), SetPadding(0, 0, 0, 4),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, GLAND_WATER_NE), SetMinimalSize(73, 12), SetDataTip(0x0, STR_NORTHEAST),
				EndContainer(),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, GLAND_WATER_SW), SetMinimalSize(73, 12), SetDataTip(0x0, STR_SOUTHWEST), SetPadding(0, 0, 0, 4),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, GLAND_WATER_SE), SetMinimalSize(73, 12), SetDataTip(0x0, STR_SOUTHEAST),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_WATER_NE_TEXT), SetMinimalSize(77, 11), SetDataTip(STR_NORTHEAST, STR_NULL), SetPadding(2, 0, 0, 4),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_WATER_SE_TEXT), SetMinimalSize(77, 11), SetDataTip(STR_SOUTHEAST, STR_NULL), SetPadding(1, 0, 0, 4),
			EndContainer(),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 9),
	EndContainer(),
};

static const Widget _heightmap_load_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE, COLOUR_BROWN,    0,  10,   0,  13, STR_BLACK_CROSS,              STR_TOOLTIP_CLOSE_WINDOW},                 // GLAND_CLOSEBOX
{    WWT_CAPTION, RESIZE_NONE, COLOUR_BROWN,   11, 337,   0,  13, STR_WORLD_GENERATION_CAPTION, STR_NULL},                                 // GLAND_TITLEBAR
{      WWT_PANEL, RESIZE_NONE, COLOUR_BROWN,    0, 337,  14, 235, 0x0,                          STR_NULL},                                 // GLAND_BACKGROUND

/* Landscape selection */
{   WWT_IMGBTN_2, RESIZE_NONE, COLOUR_ORANGE,  10,  86,  24,  78, SPR_SELECT_TEMPERATE,         STR_INTRO_TOOLTIP_TEMPERATE},              // GLAND_TEMPERATE
{   WWT_IMGBTN_2, RESIZE_NONE, COLOUR_ORANGE,  90, 166,  24,  78, SPR_SELECT_SUB_ARCTIC,        STR_INTRO_TOOLTIP_SUB_ARCTIC_LANDSCAPE},   // GLAND_ARCTIC
{   WWT_IMGBTN_2, RESIZE_NONE, COLOUR_ORANGE, 170, 246,  24,  78, SPR_SELECT_SUB_TROPICAL,      STR_INTRO_TOOLTIP_SUB_TROPICAL_LANDSCAPE}, // GLAND_TROPICAL
{   WWT_IMGBTN_2, RESIZE_NONE, COLOUR_ORANGE, 250, 326,  24,  78, SPR_SELECT_TOYLAND,           STR_INTRO_TOOLTIP_TOYLAND_LANDSCAPE},      // GLAND_TOYLAND

/* Mapsize X */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12, 110, 113, 123, STR_MAPSIZE,                  STR_NULL},                              // GLAND_MAPSIZE_X_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 114, 161, 112, 123, STR_NUM_1,                    STR_NULL},                              // GLAND_MAPSIZE_X_PULLDOWN
/* Mapsize Y */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE, 168, 176, 113, 123, STR_BY,                       STR_NULL},                              // GLAND_MAPSIZE_Y_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 180, 227, 112, 123, STR_NUM_2,                    STR_NULL},                              // GLAND_MAPSIZE_Y_PULLDOWN

/* Number of towns */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12, 110, 135, 145, STR_NUMBER_OF_TOWNS,          STR_NULL},                              // GLAND_TOWN_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 114, 175, 134, 145, 0x0,                          STR_NULL},                              // GLAND_TOWN_PULLDOWN

/* Number of industries */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12, 110, 153, 163, STR_NUMBER_OF_INDUSTRIES,     STR_NULL},                              // GLAND_INDUSTRY_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 114, 175, 152, 163, 0x0,                          STR_NULL},                              // GLAND_INDUSTRY_PULLDOWN

/* Edit box for seed */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12, 110, 175, 185, STR_RANDOM_SEED,              STR_NULL},                              // GLAND_RANDOM_TEXT
{    WWT_EDITBOX, RESIZE_NONE, COLOUR_WHITE,  114, 207, 174, 185, STR_RANDOM_SEED_OSKTITLE,     STR_RANDOM_SEED_HELP},                  // GLAND_RANDOM_EDITBOX
{    WWT_TEXTBTN, RESIZE_NONE, COLOUR_ORANGE, 216, 326, 174, 185, STR_RANDOM,                   STR_RANDOM_HELP},                       // GLAND_RANDOM_BUTTON

/* Generate button */
{    WWT_TEXTBTN, RESIZE_NONE, COLOUR_GREEN,  243, 326, 196, 225, STR_GENERATE,                 STR_NULL},                              // GLAND_GENERATE_BUTTON

/* Starting date */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE, 182, 212, 135, 145, STR_DATE,                     STR_NULL},                              // GLAND_START_DATE_TEXT1
{     WWT_IMGBTN, RESIZE_NONE, COLOUR_ORANGE, 216, 227, 134, 145, SPR_ARROW_DOWN,               STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_BACKWARD}, // GLAND_START_DATE_DOWN
{    WWT_TEXTBTN, RESIZE_NONE, COLOUR_ORANGE, 228, 314, 134, 145, STR_GENERATE_DATE,            STR_NULL},                              // GLAND_START_DATE_TEXT
{     WWT_IMGBTN, RESIZE_NONE, COLOUR_ORANGE, 315, 326, 134, 145, SPR_ARROW_UP,                 STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_FORWARD},  // GLAND_START_DATE_UP

/* Snow line */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE, 182, 278, 153, 163, STR_SNOW_LINE_HEIGHT,         STR_NULL},                              // GLAND_SNOW_LEVEL_TEXT1
{     WWT_IMGBTN, RESIZE_NONE, COLOUR_ORANGE, 282, 293, 152, 163, SPR_ARROW_DOWN,               STR_SNOW_LINE_DOWN},                    // GLAND_SNOW_LEVEL_DOWN
{    WWT_TEXTBTN, RESIZE_NONE, COLOUR_ORANGE, 294, 314, 152, 163, STR_NUM_3,                    STR_NULL},                              // GLAND_SNOW_LEVEL_TEXT
{     WWT_IMGBTN, RESIZE_NONE, COLOUR_ORANGE, 315, 326, 152, 163, SPR_ARROW_UP,                 STR_SNOW_LINE_UP},                      // GLAND_SNOW_LEVEL_UP

/* Tree placer */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12, 110, 197, 207, STR_TREE_PLACER,              STR_NULL},                              // GLAND_TREE_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 114, 231, 196, 207, STR_BLACK_ARROW_DOWN,         STR_NULL},                              // GLAND_TREE_PULLDOWN

/* Heightmap rotation */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE,  12, 110, 215, 225, STR_HEIGHTMAP_ROTATION,       STR_NULL},                              // GLAND_HEIGHTMAP_ROTATION_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 114, 231, 214, 225, STR_BLACK_ARROW_DOWN,         STR_NULL},                              // GLAND_HEIGHTMAP_ROTATION_PULLDOWN

{   WIDGETS_END},
};

static const NWidgetPart _nested_heightmap_load_widgets[] = {
	/* Window header. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN, GLAND_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_BROWN, GLAND_TITLEBAR), SetDataTip(STR_WORLD_GENERATION_CAPTION, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, GLAND_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 10),
		/* Landscape selection. */
		NWidget(NWID_HORIZONTAL), SetPIP(10, 3, 11),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, GLAND_TEMPERATE), SetMinimalSize(77, 55), SetDataTip(SPR_SELECT_TEMPERATE, STR_INTRO_TOOLTIP_TEMPERATE),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, GLAND_ARCTIC), SetMinimalSize(77, 55), SetDataTip(SPR_SELECT_SUB_ARCTIC, STR_INTRO_TOOLTIP_SUB_ARCTIC_LANDSCAPE),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, GLAND_TROPICAL), SetMinimalSize(77, 55), SetDataTip(SPR_SELECT_SUB_TROPICAL, STR_INTRO_TOOLTIP_SUB_TROPICAL_LANDSCAPE),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, GLAND_TOYLAND), SetMinimalSize(77, 55), SetDataTip(SPR_SELECT_TOYLAND, STR_INTRO_TOOLTIP_TOYLAND_LANDSCAPE),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 33), SetFill(0,1),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(12, 0),
			/* Labels at the left side. */
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 1),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_MAPSIZE_X_TEXT), SetMinimalSize(99, 11), SetDataTip(STR_MAPSIZE, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 11),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_TOWN_TEXT), SetMinimalSize(99, 11), SetDataTip(STR_NUMBER_OF_TOWNS, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 7),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_INDUSTRY_TEXT), SetMinimalSize(99, 11), SetDataTip(STR_NUMBER_OF_INDUSTRIES, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 11),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_RANDOM_TEXT), SetMinimalSize(99, 11), SetDataTip(STR_RANDOM_SEED, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 11),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_TREE_TEXT), SetMinimalSize(99, 11), SetDataTip(STR_TREE_PLACER, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 7),
				NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_HEIGHTMAP_ROTATION_TEXT), SetMinimalSize(99, 11), SetDataTip(STR_HEIGHTMAP_ROTATION, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 10),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(3, 0),
			/* Widgets at the right of the labels. */
			NWidget(NWID_VERTICAL),
				/* Mapsize X * Y. */
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_MAPSIZE_X_PULLDOWN), SetMinimalSize(48, 12), SetDataTip(STR_NUM_1, STR_NULL),
					NWidget(NWID_SPACER), SetMinimalSize(6, 0),
					NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_MAPSIZE_Y_TEXT), SetMinimalSize(9, 11), SetDataTip(STR_BY, STR_NULL), SetPadding(1, 0, 0, 0),
					NWidget(NWID_SPACER), SetMinimalSize(3, 0),
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_MAPSIZE_Y_PULLDOWN), SetMinimalSize(48, 12), SetDataTip(STR_NUM_2, STR_NULL),
					NWidget(NWID_SPACER), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 10),
				NWidget(NWID_HORIZONTAL),
					/* Number of towns. */
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_TOWN_PULLDOWN), SetMinimalSize(62, 12), SetDataTip(0x0, STR_NULL),
					NWidget(NWID_SPACER), SetMinimalSize(6, 0),
					/* Starting date. */
					NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_START_DATE_TEXT1), SetMinimalSize(31, 11), SetDataTip(STR_DATE, STR_NULL), SetPadding(1, 0, 0, 0),
					NWidget(NWID_SPACER), SetMinimalSize(3, 0),
					NWidget(WWT_IMGBTN, COLOUR_ORANGE, GLAND_START_DATE_DOWN), SetMinimalSize(12, 12), SetDataTip(SPR_ARROW_DOWN, STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_BACKWARD),
					NWidget(WWT_TEXTBTN, COLOUR_ORANGE, GLAND_START_DATE_TEXT), SetMinimalSize(87, 12), SetDataTip(STR_GENERATE_DATE, STR_NULL),
					NWidget(WWT_IMGBTN, COLOUR_ORANGE, GLAND_START_DATE_UP), SetMinimalSize(12, 12), SetDataTip(SPR_ARROW_UP, STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_FORWARD),
					NWidget(NWID_SPACER), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 6),
				NWidget(NWID_HORIZONTAL),
					/* Number of industries. */
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_INDUSTRY_PULLDOWN), SetMinimalSize(62, 12), SetDataTip(0x0, STR_NULL),
					NWidget(NWID_SPACER), SetMinimalSize(6, 0),
					/* Snow line. */
					NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_SNOW_LEVEL_TEXT1), SetMinimalSize(97, 11),
														SetDataTip(STR_SNOW_LINE_HEIGHT, STR_NULL), SetPadding(1, 0, 0, 0),
					NWidget(NWID_SPACER), SetMinimalSize(3, 0),
					NWidget(WWT_IMGBTN, COLOUR_ORANGE, GLAND_SNOW_LEVEL_DOWN), SetMinimalSize(12, 12), SetDataTip(SPR_ARROW_DOWN, STR_SNOW_LINE_DOWN),
					NWidget(WWT_TEXTBTN, COLOUR_ORANGE, GLAND_SNOW_LEVEL_TEXT), SetMinimalSize(21, 12), SetDataTip(STR_NUM_3, STR_NULL),
					NWidget(WWT_IMGBTN, COLOUR_ORANGE, GLAND_SNOW_LEVEL_UP), SetMinimalSize(12, 12), SetDataTip(SPR_ARROW_UP, STR_SNOW_LINE_UP),
					NWidget(NWID_SPACER), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 10),
				NWidget(NWID_HORIZONTAL),
					/* Edit box for seed. */
					NWidget(WWT_EDITBOX, COLOUR_WHITE, GLAND_RANDOM_EDITBOX), SetMinimalSize(94, 12), SetDataTip(STR_RANDOM_SEED_OSKTITLE, STR_RANDOM_SEED_HELP),
					NWidget(NWID_SPACER), SetMinimalSize(8, 0),
					NWidget(WWT_TEXTBTN, COLOUR_ORANGE, GLAND_RANDOM_BUTTON), SetMinimalSize(111, 12), SetDataTip(STR_RANDOM, STR_RANDOM_HELP),
					NWidget(NWID_SPACER), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 10),
				NWidget(NWID_HORIZONTAL),
					NWidget(NWID_VERTICAL),
						/* Tree placer. */
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_TREE_PULLDOWN), SetMinimalSize(118, 12), SetDataTip(STR_BLACK_ARROW_DOWN, STR_NULL),
						NWidget(NWID_SPACER), SetMinimalSize(0, 6),
						/* Heightmap rotation. */
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_HEIGHTMAP_ROTATION_PULLDOWN), SetMinimalSize(118, 12), SetDataTip(STR_BLACK_ARROW_DOWN, STR_NULL),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(11, 0),
					/* Generate button. */
					NWidget(WWT_TEXTBTN, COLOUR_GREEN, GLAND_GENERATE_BUTTON), SetMinimalSize(84, 30), SetDataTip(STR_GENERATE, STR_NULL),
					NWidget(NWID_SPACER), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
	EndContainer(),
};

void StartGeneratingLandscape(glwp_modes mode)
{
	DeleteAllNonVitalWindows();

	/* Copy all XXX_newgame to XXX when coming from outside the editor */
	MakeNewgameSettingsLive();
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

	for (uint i = MIN_MAP_SIZE_BITS; i <= MAX_MAP_SIZE_BITS; i++) {
		DropDownListParamStringItem *item = new DropDownListParamStringItem(STR_JUST_INT, i, false);
		item->SetParam(0, 1 << i);
		list->push_back(item);
	}

	return list;
}

static const StringID _elevations[]  = {STR_TERRAIN_TYPE_VERY_FLAT, STR_TERRAIN_TYPE_FLAT, STR_TERRAIN_TYPE_HILLY, STR_TERRAIN_TYPE_MOUNTAINOUS, INVALID_STRING_ID};
static const StringID _sea_lakes[]   = {STR_SEA_LEVEL_VERY_LOW, STR_SEA_LEVEL_LOW, STR_SEA_LEVEL_MEDIUM, STR_SEA_LEVEL_HIGH, INVALID_STRING_ID};
static const StringID _smoothness[]  = {STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN_VERY_SMOOTH, STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN_SMOOTH, STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN_ROUGH, STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN_VERY_ROUGH, INVALID_STRING_ID};
static const StringID _tree_placer[] = {STR_CONFIG_SETTING_TREE_PLACER_NONE, STR_CONFIG_SETTING_TREE_PLACER_ORIGINAL, STR_CONFIG_SETTING_TREE_PLACER_IMPROVED, INVALID_STRING_ID};
static const StringID _rotation[]    = {STR_CONFIG_SETTING_HEIGHTMAP_ROTATION_COUNTER_CLOCKWISE, STR_CONFIG_SETTING_HEIGHTMAP_ROTATION_CLOCKWISE, INVALID_STRING_ID};
static const StringID _landscape[]   = {STR_CONFIG_SETTING_LAND_GENERATOR_ORIGINAL, STR_CONFIG_SETTING_LAND_GENERATOR_TERRA_GENESIS, INVALID_STRING_ID};
static const StringID _num_towns[]   = {STR_NUM_VERY_LOW, STR_NUM_LOW, STR_NUM_NORMAL, STR_NUM_HIGH, STR_NUM_CUSTOM, INVALID_STRING_ID};
static const StringID _num_inds[]    = {STR_NONE, STR_NUM_VERY_LOW, STR_NUM_LOW, STR_NUM_NORMAL, STR_NUM_HIGH, INVALID_STRING_ID};

struct GenerateLandscapeWindow : public QueryStringBaseWindow {
	uint widget_id;
	uint x;
	uint y;
	char name[64];
	glwp_modes mode;

	GenerateLandscapeWindow(const WindowDesc *desc, WindowNumber number = 0) : QueryStringBaseWindow(11, desc, number)
	{
		this->LowerWidget(_settings_newgame.game_creation.landscape + GLAND_TEMPERATE);

		/* snprintf() always outputs trailing '\0', so whole buffer can be used */
		snprintf(this->edit_str_buf, this->edit_str_size, "%u", _settings_newgame.game_creation.generation_seed);
		InitializeTextBuffer(&this->text, this->edit_str_buf, this->edit_str_size, 120);
		this->SetFocusedWidget(GLAND_RANDOM_EDITBOX);
		this->caption = STR_NULL;
		this->afilter = CS_NUMERAL;

		this->mode = (glwp_modes)this->window_number;

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		/* You can't select smoothness / non-water borders if not terragenesis */
		if (mode == GLWP_GENERATE) {
			this->SetWidgetDisabledState(GLAND_SMOOTHNESS_PULLDOWN, _settings_newgame.game_creation.land_generator == 0);
			this->SetWidgetDisabledState(GLAND_BORDERS_RANDOM, _settings_newgame.game_creation.land_generator == 0 || !_settings_newgame.construction.freeform_edges);
			this->SetWidgetsDisabledState(_settings_newgame.game_creation.land_generator == 0 || !_settings_newgame.construction.freeform_edges || _settings_newgame.game_creation.water_borders == BORDERS_RANDOM,
					GLAND_WATER_NW, GLAND_WATER_NE, GLAND_WATER_SE, GLAND_WATER_SW, WIDGET_LIST_END);

			this->SetWidgetLoweredState(GLAND_BORDERS_RANDOM, _settings_newgame.game_creation.water_borders == BORDERS_RANDOM);

			this->SetWidgetLoweredState(GLAND_WATER_NW, HasBit(_settings_newgame.game_creation.water_borders, BORDER_NW));
			this->SetWidgetLoweredState(GLAND_WATER_NE, HasBit(_settings_newgame.game_creation.water_borders, BORDER_NE));
			this->SetWidgetLoweredState(GLAND_WATER_SE, HasBit(_settings_newgame.game_creation.water_borders, BORDER_SE));
			this->SetWidgetLoweredState(GLAND_WATER_SW, HasBit(_settings_newgame.game_creation.water_borders, BORDER_SW));
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
			this->widget[GLAND_TOWN_PULLDOWN].data     = STR_DISASTERS_OFF;
			this->widget[GLAND_INDUSTRY_PULLDOWN].data = STR_DISASTERS_OFF;
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
			this->widget[GLAND_BORDERS_RANDOM].data      = (_settings_newgame.game_creation.water_borders == BORDERS_RANDOM) ? STR_BORDER_RANDOMIZE : STR_BORDER_MANUAL;

			if (_settings_newgame.game_creation.water_borders == BORDERS_RANDOM) {
				this->widget[GLAND_WATER_NE].data = STR_BORDER_RANDOM;
				this->widget[GLAND_WATER_NW].data = STR_BORDER_RANDOM;
				this->widget[GLAND_WATER_SE].data = STR_BORDER_RANDOM;
				this->widget[GLAND_WATER_SW].data = STR_BORDER_RANDOM;
			} else {
				this->widget[GLAND_WATER_NE].data = HasBit(_settings_newgame.game_creation.water_borders, BORDER_NE) ? STR_BORDER_WATER : STR_BORDER_FREEFORM;
				this->widget[GLAND_WATER_NW].data = HasBit(_settings_newgame.game_creation.water_borders, BORDER_NW) ? STR_BORDER_WATER : STR_BORDER_FREEFORM;
				this->widget[GLAND_WATER_SE].data = HasBit(_settings_newgame.game_creation.water_borders, BORDER_SE) ? STR_BORDER_WATER : STR_BORDER_FREEFORM;
				this->widget[GLAND_WATER_SW].data = HasBit(_settings_newgame.game_creation.water_borders, BORDER_SW) ? STR_BORDER_WATER : STR_BORDER_FREEFORM;
			}
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
			if (_settings_newgame.game_creation.heightmap_rotation == HM_CLOCKWISE) {
				SetDParam(0, this->y);
				SetDParam(1, this->x);
			} else {
				SetDParam(0, this->x);
				SetDParam(1, this->y);
			}
			int right = DrawString(0, 326, 91, STR_HEIGHTMAP_SIZE, TC_FROMSTRING, SA_RIGHT);

			DrawString( 12, 114, 91, STR_HEIGHTMAP_NAME);
			DrawString(114, right - 5, 91, this->name, TC_ORANGE);
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
				snprintf(this->edit_str_buf, this->edit_str_size, "%u", _settings_newgame.game_creation.generation_seed);
				UpdateTextBufferSize(&this->text);
				this->SetDirty();
				break;

			case GLAND_GENERATE_BUTTON: // Generate
				MakeNewgameSettingsLive();

				if (mode == GLWP_HEIGHTMAP &&
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
				if ((this->flags4 & WF_TIMEOUT_MASK) <= WF_TIMEOUT_TRIGGER) {
					this->HandleButtonClick(widget);
					this->SetDirty();

					_settings_newgame.game_creation.starting_year = Clamp(_settings_newgame.game_creation.starting_year + widget - GLAND_START_DATE_TEXT, MIN_YEAR, MAX_YEAR);
				}
				_left_button_clicked = false;
				break;

			case GLAND_START_DATE_TEXT: // Year text
				this->widget_id = GLAND_START_DATE_TEXT;
				SetDParam(0, _settings_newgame.game_creation.starting_year);
				ShowQueryString(STR_JUST_INT, STR_START_DATE_QUERY_CAPT, 8, 100, this, CS_NUMERAL, QSF_NONE);
				break;

			case GLAND_SNOW_LEVEL_DOWN:
			case GLAND_SNOW_LEVEL_UP: // Snow line buttons
				/* Don't allow too fast scrolling */
				if ((this->flags4 & WF_TIMEOUT_MASK) <= WF_TIMEOUT_TRIGGER) {
					this->HandleButtonClick(widget);
					this->SetDirty();

					_settings_newgame.game_creation.snow_line_height = Clamp(_settings_newgame.game_creation.snow_line_height + widget - GLAND_SNOW_LEVEL_TEXT, 2, MAX_SNOWLINE_HEIGHT);
				}
				_left_button_clicked = false;
				break;

			case GLAND_SNOW_LEVEL_TEXT: // Snow line text
				this->widget_id = GLAND_SNOW_LEVEL_TEXT;
				SetDParam(0, _settings_newgame.game_creation.snow_line_height);
				ShowQueryString(STR_JUST_INT, STR_SNOW_LINE_QUERY_CAPT, 3, 100, this, CS_NUMERAL, QSF_NONE);
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

			/* Freetype map borders */
			case GLAND_WATER_NW:
				_settings_newgame.game_creation.water_borders = ToggleBit(_settings_newgame.game_creation.water_borders, BORDER_NW);
				break;

			case GLAND_WATER_NE:
				_settings_newgame.game_creation.water_borders = ToggleBit(_settings_newgame.game_creation.water_borders, BORDER_NE);
				break;

			case GLAND_WATER_SE:
				_settings_newgame.game_creation.water_borders = ToggleBit(_settings_newgame.game_creation.water_borders, BORDER_SE);
				break;

			case GLAND_WATER_SW:
				_settings_newgame.game_creation.water_borders = ToggleBit(_settings_newgame.game_creation.water_borders, BORDER_SW);
				break;

			case GLAND_BORDERS_RANDOM:
				_settings_newgame.game_creation.water_borders = (_settings_newgame.game_creation.water_borders == BORDERS_RANDOM) ? 0 : BORDERS_RANDOM;
				this->SetDirty();
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
		 * As UINT32_MAX is a 'magic' value (use random seed) it
		 * should not be possible to be entered into the input
		 * field; the generate seed button can be used instead. */
		_settings_newgame.game_creation.generation_seed = minu(strtoul(this->edit_str_buf, NULL, 10), UINT32_MAX - 1);
		return state;
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case GLAND_MAPSIZE_X_PULLDOWN:     _settings_newgame.game_creation.map_x = index; break;
			case GLAND_MAPSIZE_Y_PULLDOWN:     _settings_newgame.game_creation.map_y = index; break;
			case GLAND_TREE_PULLDOWN:          _settings_newgame.game_creation.tree_placer = index; break;
			case GLAND_SMOOTHNESS_PULLDOWN:    _settings_newgame.game_creation.tgen_smoothness = index;  break;

			case GLAND_TOWN_PULLDOWN:
				if ((uint)index == CUSTOM_TOWN_NUMBER_DIFFICULTY) {
					this->widget_id = widget;
					SetDParam(0, _settings_newgame.game_creation.custom_town_number);
					ShowQueryString(STR_JUST_INT, STR_NUMBER_OF_TOWNS, 5, 50, this, CS_NUMERAL, QSF_NONE);
				};
				IConsoleSetSetting("difficulty.number_towns", index);
				break;

			case GLAND_INDUSTRY_PULLDOWN:
				IConsoleSetSetting("difficulty.number_industries", index);
				break;

			case GLAND_LANDSCAPE_PULLDOWN:
			/* case GLAND_HEIGHTMAP_PULLDOWN: */
				if (mode == GLWP_HEIGHTMAP) {
					_settings_newgame.game_creation.heightmap_rotation = index;
				} else {
					_settings_newgame.game_creation.land_generator = index;
				}
				break;

			case GLAND_TERRAIN_PULLDOWN: {
				GameMode old_gm = _game_mode;
				_game_mode = GM_MENU;
				IConsoleSetSetting("difficulty.terrain_type", index);
				_game_mode = old_gm;
				break;
			}

			case GLAND_WATER_PULLDOWN: {
				GameMode old_gm = _game_mode;
				_game_mode = GM_MENU;
				IConsoleSetSetting("difficulty.quantity_sea_lakes", index);
				_game_mode = old_gm;
				break;
			}
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

				case GLAND_TOWN_PULLDOWN:
					_settings_newgame.game_creation.custom_town_number = Clamp(value, 1, CUSTOM_TOWN_MAX_NUMBER);
					break;
			}

			this->SetDirty();
		}
	}
};

static const WindowDesc _generate_landscape_desc(
	WDP_CENTER, WDP_CENTER, 338, 313, 338, 313,
	WC_GENERATE_LANDSCAPE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_generate_landscape_widgets, _nested_generate_landscape_widgets, lengthof(_nested_generate_landscape_widgets)
);

static const WindowDesc _heightmap_load_desc(
	WDP_CENTER, WDP_CENTER, 338, 236, 338, 236,
	WC_GENERATE_LANDSCAPE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS,
	_heightmap_load_widgets, _nested_heightmap_load_widgets, lengthof(_nested_heightmap_load_widgets)
);

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
	StartGeneratingLandscape(GLWP_SCENARIO);
}

void StartNewGameWithoutGUI(uint seed)
{
	/* GenerateWorld takes care of the possible GENERATE_NEW_SEED value in 'seed' */
	_settings_newgame.game_creation.generation_seed = seed;

	StartGeneratingLandscape(GLWP_GENERATE);
}

/** Widget numbers of the create scenario window. */
enum CreateScenarioWindowWidgets {
	CSCEN_CLOSEBOX,               ///< Close button.
	CSCEN_CAPTION,                ///< Title bar.
	CSCEN_BACKGROUND,             ///< Background panel.
	CSCEN_TEMPERATE,              ///< Select temperate landscape style.
	CSCEN_ARCTIC,                 ///< Select arctic landscape style.
	CSCEN_TROPICAL,               ///< Select tropical landscape style.
	CSCEN_TOYLAND,                ///< Select toy-land landscape style.
	CSCEN_EMPTY_WORLD,            ///< Generate an empty flat world.
	CSCEN_RANDOM_WORLD,           ///< Generate random land button
	CSCEN_MAPSIZE_X_TEXT,         ///< Clickable currently selected x map size.
	CSCEN_MAPSIZE_X_PULLDOWN,     ///< Pull-down arrow for x map size.
	CSCEN_MAPSIZE_Y_TEXT,         ///< Clickable currently selected y map size.
	CSCEN_MAPSIZE_Y_PULLDOWN,     ///< Pull-down arrow for y map size.
	CSCEN_START_DATE_LABEL,       ///< 'Date' label
	CSCEN_START_DATE_DOWN,        ///< Decrease start year (start earlier).
	CSCEN_START_DATE_TEXT,        ///< Clickable start date value.
	CSCEN_START_DATE_UP,          ///< Increase start year (start later).
	CSCEN_FLAT_LAND_HEIGHT_LABEL, ///< 'Height of flat land' label.
	CSCEN_FLAT_LAND_HEIGHT_DOWN,  ///< Decrease flat land height.
	CSCEN_FLAT_LAND_HEIGHT_TEXT,  ///< Clickable flat land height value.
	CSCEN_FLAT_LAND_HEIGHT_UP     ///< Increase flat land height.
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
				if ((this->flags4 & WF_TIMEOUT_MASK) <= WF_TIMEOUT_TRIGGER) {
					this->HandleButtonClick(widget);
					this->SetDirty();

					_settings_newgame.game_creation.starting_year = Clamp(_settings_newgame.game_creation.starting_year + widget - CSCEN_START_DATE_TEXT, MIN_YEAR, MAX_YEAR);
				}
				_left_button_clicked = false;
				break;

			case CSCEN_START_DATE_TEXT: // Year text
				this->widget_id = CSCEN_START_DATE_TEXT;
				SetDParam(0, _settings_newgame.game_creation.starting_year);
				ShowQueryString(STR_JUST_INT, STR_START_DATE_QUERY_CAPT, 8, 100, this, CS_NUMERAL, QSF_NONE);
				break;

			case CSCEN_FLAT_LAND_HEIGHT_DOWN:
			case CSCEN_FLAT_LAND_HEIGHT_UP: // Height level buttons
				/* Don't allow too fast scrolling */
				if ((this->flags4 & WF_TIMEOUT_MASK) <= WF_TIMEOUT_TRIGGER) {
					this->HandleButtonClick(widget);
					this->SetDirty();

					_settings_newgame.game_creation.se_flat_world_height = Clamp(_settings_newgame.game_creation.se_flat_world_height + widget - CSCEN_FLAT_LAND_HEIGHT_TEXT, 0, MAX_TILE_HEIGHT);
				}
				_left_button_clicked = false;
				break;

			case CSCEN_FLAT_LAND_HEIGHT_TEXT: // Height level text
				this->widget_id = CSCEN_FLAT_LAND_HEIGHT_TEXT;
				SetDParam(0, _settings_newgame.game_creation.se_flat_world_height);
				ShowQueryString(STR_JUST_INT, STR_FLAT_WORLD_HEIGHT_QUERY_CAPT, 3, 100, this, CS_NUMERAL, QSF_NONE);
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
{   WWT_CLOSEBOX, RESIZE_NONE, COLOUR_BROWN,    0,  10,   0,  13, STR_BLACK_CROSS,         STR_TOOLTIP_CLOSE_WINDOW},                 // CSCEN_CLOSEBOX
{    WWT_CAPTION, RESIZE_NONE, COLOUR_BROWN,   11, 337,   0,  13, STR_SE_CAPTION,          STR_NULL},                                 // CSCEN_CAPTION
{      WWT_PANEL, RESIZE_NONE, COLOUR_BROWN,    0, 337,  14, 169, 0x0,                     STR_NULL},                                 // CSCEN_BACKGROUND

/* Landscape selection */
{   WWT_IMGBTN_2, RESIZE_NONE, COLOUR_ORANGE,  10,  86,  24,  78, SPR_SELECT_TEMPERATE,    STR_INTRO_TOOLTIP_TEMPERATE},              // CSCEN_TEMPERATE
{   WWT_IMGBTN_2, RESIZE_NONE, COLOUR_ORANGE,  90, 166,  24,  78, SPR_SELECT_SUB_ARCTIC,   STR_INTRO_TOOLTIP_SUB_ARCTIC_LANDSCAPE},   // CSCEN_ARCTIC
{   WWT_IMGBTN_2, RESIZE_NONE, COLOUR_ORANGE, 170, 246,  24,  78, SPR_SELECT_SUB_TROPICAL, STR_INTRO_TOOLTIP_SUB_TROPICAL_LANDSCAPE}, // CSCEN_TROPICAL
{   WWT_IMGBTN_2, RESIZE_NONE, COLOUR_ORANGE, 250, 326,  24,  78, SPR_SELECT_TOYLAND,      STR_INTRO_TOOLTIP_TOYLAND_LANDSCAPE},      // CSCEN_TOYLAND

/* Generation type */
{    WWT_TEXTBTN, RESIZE_NONE, COLOUR_GREEN,   12, 115,  95, 124, STR_SE_FLAT_WORLD,       STR_SE_FLAT_WORLD_TIP},                    // CSCEN_EMPTY_WORLD
{    WWT_TEXTBTN, RESIZE_NONE, COLOUR_GREEN,   12, 115, 131, 160, STR_SE_RANDOM_LAND,      STR_TERRAFORM_TOOLTIP_GENERATE_RANDOM_LAND},// CSCEN_RANDOM_WORLD

/* Mapsize X */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE, 135, 212,  96, 106, STR_MAPSIZE,             STR_NULL},                               // CSCEN_MAPSIZE_X_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 216, 263,  95, 106, STR_NUM_1,               STR_NULL},                               // CSCEN_MAPSIZE_X_PULLDOWN
/* Mapsize Y */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE, 268, 276,  96, 106, STR_BY,                  STR_NULL},                               // CSCEN_MAPSIZE_Y_TEXT
{   WWT_DROPDOWN, RESIZE_NONE, COLOUR_ORANGE, 279, 326,  95, 106, STR_NUM_2,               STR_NULL},                               // CSCEN_MAPSIZE_Y_PULLDOWN

/* Start date */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE, 135, 212, 114, 124, STR_DATE,                STR_NULL},                               // CSCEN_START_DATE_LABEL
{     WWT_IMGBTN, RESIZE_NONE, COLOUR_ORANGE, 216, 227, 113, 124, SPR_ARROW_DOWN,          STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_BACKWARD}, // CSCEN_START_DATE_DOWN
{    WWT_TEXTBTN, RESIZE_NONE, COLOUR_ORANGE, 228, 314, 113, 124, STR_GENERATE_DATE,       STR_NULL},                               // CSCEN_START_DATE_TEXT
{     WWT_IMGBTN, RESIZE_NONE, COLOUR_ORANGE, 315, 326, 113, 124, SPR_ARROW_UP,            STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_FORWARD},  // CSCEN_START_DATE_UP

/* Flat map height */
{       WWT_TEXT, RESIZE_NONE, COLOUR_ORANGE, 135, 278, 132, 142, STR_FLAT_WORLD_HEIGHT,   STR_NULL},                               // CSCEN_FLAT_LAND_HEIGHT_LABEL
{     WWT_IMGBTN, RESIZE_NONE, COLOUR_ORANGE, 282, 293, 131, 142, SPR_ARROW_DOWN,          STR_FLAT_WORLD_HEIGHT_DOWN},             // CSCEN_FLAT_LAND_HEIGHT_DOWN
{    WWT_TEXTBTN, RESIZE_NONE, COLOUR_ORANGE, 294, 314, 131, 142, STR_NUM_3,               STR_NULL},                               // CSCEN_FLAT_LAND_HEIGHT_TEXT
{     WWT_IMGBTN, RESIZE_NONE, COLOUR_ORANGE, 315, 326, 131, 142, SPR_ARROW_UP,            STR_FLAT_WORLD_HEIGHT_UP},               // CSCEN_FLAT_LAND_HEIGHT_UP
{   WIDGETS_END},
};

static const NWidgetPart _nested_create_scenario_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN, CSCEN_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_BROWN, CSCEN_CAPTION), SetDataTip(STR_SE_CAPTION, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, CSCEN_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 10),
		/* Landscape style selection. */
		NWidget(NWID_HORIZONTAL), SetPIP(10, 3, 11),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, CSCEN_TEMPERATE), SetMinimalSize(77, 55), SetDataTip(SPR_SELECT_TEMPERATE, STR_INTRO_TOOLTIP_TEMPERATE),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, CSCEN_ARCTIC), SetMinimalSize(77, 55), SetDataTip(SPR_SELECT_SUB_ARCTIC, STR_INTRO_TOOLTIP_SUB_ARCTIC_LANDSCAPE),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, CSCEN_TROPICAL), SetMinimalSize(77, 55), SetDataTip(SPR_SELECT_SUB_TROPICAL, STR_INTRO_TOOLTIP_SUB_TROPICAL_LANDSCAPE),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, CSCEN_TOYLAND), SetMinimalSize(77, 55), SetDataTip(SPR_SELECT_TOYLAND, STR_INTRO_TOOLTIP_TOYLAND_LANDSCAPE),
		EndContainer(),
		NWidget(NWID_HORIZONTAL), SetPIP(12, 19, 11),
			/* Green generation type buttons: 'Flat land' and 'Random land'. */
			NWidget(NWID_VERTICAL), SetPIP(16, 6, 9),
				NWidget(WWT_TEXTBTN, COLOUR_GREEN, CSCEN_EMPTY_WORLD), SetMinimalSize(104, 30), SetDataTip(STR_SE_FLAT_WORLD, STR_SE_FLAT_WORLD_TIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREEN, CSCEN_RANDOM_WORLD), SetMinimalSize(104, 30), SetDataTip(STR_SE_RANDOM_LAND, STR_TERRAFORM_TOOLTIP_GENERATE_RANDOM_LAND),
			EndContainer(),
			/* Labels + setting drop-downs */
			NWidget(NWID_VERTICAL), SetPIP(16, 6, 0),
				/* Map size. */
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_TEXT, COLOUR_ORANGE, CSCEN_MAPSIZE_X_TEXT), SetMinimalSize(78, 11),
												SetDataTip(STR_MAPSIZE, STR_NULL), SetPadding(1, 0, 0, 0),
					NWidget(NWID_SPACER), SetFill(1, 0),
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, CSCEN_MAPSIZE_X_PULLDOWN), SetMinimalSize(48, 12),
												SetDataTip(STR_NUM_1, STR_NULL), SetPadding(0, 4, 0, 0),
					NWidget(WWT_TEXT, COLOUR_ORANGE, CSCEN_MAPSIZE_Y_TEXT), SetMinimalSize(9, 11),
												SetDataTip(STR_BY, STR_NULL), SetPadding(1, 2, 0, 0),
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, CSCEN_MAPSIZE_Y_PULLDOWN), SetMinimalSize(48, 12),
												SetDataTip(STR_NUM_2, STR_NULL),
				EndContainer(),
				/* Date. */
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_TEXT, COLOUR_ORANGE, CSCEN_START_DATE_LABEL), SetMinimalSize(78, 11),
												SetDataTip(STR_DATE, STR_NULL), SetPadding(1, 0, 0, 0),
					NWidget(NWID_SPACER), SetFill(1, 0),
					NWidget(WWT_IMGBTN, COLOUR_ORANGE, CSCEN_START_DATE_DOWN), SetMinimalSize(12, 12), SetDataTip(SPR_ARROW_DOWN, STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_BACKWARD),
					NWidget(WWT_TEXTBTN, COLOUR_ORANGE, CSCEN_START_DATE_TEXT), SetMinimalSize(87, 12), SetDataTip(STR_GENERATE_DATE, STR_NULL),
					NWidget(WWT_IMGBTN, COLOUR_ORANGE, CSCEN_START_DATE_UP), SetMinimalSize(12, 12), SetDataTip(SPR_ARROW_UP, STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_FORWARD),
				EndContainer(),
				/* Flat map height. */
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_TEXT, COLOUR_ORANGE, CSCEN_FLAT_LAND_HEIGHT_LABEL), SetMinimalSize(144, 11),
												SetDataTip(STR_FLAT_WORLD_HEIGHT, STR_NULL), SetPadding(1, 0, 0, 0),
					NWidget(NWID_SPACER), SetFill(1, 0),
					NWidget(WWT_IMGBTN, COLOUR_ORANGE, CSCEN_FLAT_LAND_HEIGHT_DOWN), SetMinimalSize(12, 12), SetDataTip(SPR_ARROW_DOWN, STR_FLAT_WORLD_HEIGHT_DOWN),
					NWidget(WWT_TEXTBTN, COLOUR_ORANGE, CSCEN_FLAT_LAND_HEIGHT_TEXT), SetMinimalSize(21, 12), SetDataTip(STR_NUM_3, STR_NULL),
					NWidget(WWT_IMGBTN, COLOUR_ORANGE, CSCEN_FLAT_LAND_HEIGHT_UP), SetMinimalSize(12, 12), SetDataTip(SPR_ARROW_UP, STR_FLAT_WORLD_HEIGHT_UP),
				EndContainer(),
				NWidget(NWID_SPACER), SetFill(1, 1),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _create_scenario_desc(
	WDP_CENTER, WDP_CENTER, 338, 170, 338, 170,
	WC_GENERATE_LANDSCAPE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS,
	_create_scenario_widgets, _nested_create_scenario_widgets, lengthof(_nested_create_scenario_widgets)
);

void ShowCreateScenario()
{
	DeleteWindowByClass(WC_GENERATE_LANDSCAPE);
	new CreateScenarioWindow(&_create_scenario_desc, GLWP_SCENARIO);
}

enum GenerationProgressWindowWidgets {
	GPWW_CAPTION,
	GPWW_BACKGROUND,
	GPWW_ABORT,
};

static const Widget _generate_progress_widgets[] = {
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    0,   180,     0,    13, STR_GENERATION_WORLD,   STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS}, // GPWW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    0,   180,    14,    96, 0x0,                    STR_NULL},                           // GPWW_BACKGROUND
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_WHITE,  20,   161,    74,    85, STR_GENERATION_ABORT,   STR_NULL},                           // GPWW_ABORT
{   WIDGETS_END},
};

static const NWidgetPart _nested_generate_progress_widgets[] = {
	NWidget(WWT_CAPTION, COLOUR_GREY, GPWW_CAPTION), SetDataTip(STR_GENERATION_WORLD, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_PANEL, COLOUR_GREY, GPWW_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 60),
		NWidget(NWID_HORIZONTAL), SetPIP(20, 0, 19),
			NWidget(WWT_TEXTBTN, COLOUR_WHITE, GPWW_ABORT), SetMinimalSize(142, 12), SetDataTip(STR_GENERATION_ABORT, STR_NULL),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 11),
	EndContainer(),
};


static const WindowDesc _generate_progress_desc(
	WDP_CENTER, WDP_CENTER, 181, 97, 181, 97,
	WC_GENERATE_PROGRESS_WINDOW, WC_NONE,
	WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_generate_progress_widgets, _nested_generate_progress_widgets, lengthof(_nested_generate_progress_widgets)
);

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
		DrawFrameRect(19, 20, (this->width - 18), 37, COLOUR_GREY, FR_BORDERONLY);
		DrawFrameRect(20, 21, (int)((this->width - 40) * _tp.percent / 100) + 20, 36, COLOUR_MAUVE, FR_NONE);
		SetDParam(0, _tp.percent);
		DrawString(this->widget[GPWW_BACKGROUND].left, this->widget[GPWW_BACKGROUND].right, 25, STR_PROGRESS, TC_FROMSTRING, SA_CENTER);

		/* Tell which class we are generating */
		DrawString(this->widget[GPWW_BACKGROUND].left, this->widget[GPWW_BACKGROUND].right, 46, _tp.cls, TC_FROMSTRING, SA_CENTER);

		/* And say where we are in that class */
		SetDParam(0, _tp.current);
		SetDParam(1, _tp.total);
		DrawString(this->widget[GPWW_BACKGROUND].left, this->widget[GPWW_BACKGROUND].right, 58, STR_GENERATION_PROGRESS, TC_FROMSTRING, SA_CENTER);

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
		STR_SCENEDIT_TOOLBAR_LANDSCAPE_GENERATION,
		STR_CLEARING_TILES,
		STR_SCENEDIT_TOOLBAR_TOWN_GENERATION,
		STR_SCENEDIT_TOOLBAR_INDUSTRY_GENERATION,
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

	/* Don't update the screen too often. So update it once in every once in a while... */
	if (!_network_dedicated && _tp.timer != 0 && _realtime_tick - _tp.timer < GENWORLD_REDRAW_TIMEOUT) return;

	/* Percentage is about the number of completed tasks, so 'current - 1' */
	_tp.percent = percent_table[cls] + (percent_table[cls + 1] - percent_table[cls]) * (_tp.current == 0 ? 0 : _tp.current - 1) / _tp.total;

	if (_network_dedicated) {
		static uint last_percent = 0;

		/* Never display 0% */
		if (_tp.percent == 0) return;
		/* Reset if percent is lower than the last recorded */
		if (_tp.percent < last_percent) last_percent = 0;
		/* Display every 5%, but 6% is also very valid.. just not smaller steps than 5% */
		if (_tp.percent % 5 != 0 && _tp.percent <= last_percent + 5) return;
		/* Never show steps smaller than 2%, even if it is a mod 5% */
		if (_tp.percent <= last_percent + 2) return;

		DEBUG(net, 1, "Map generation percentage complete: %d", _tp.percent);
		last_percent = _tp.percent;

		/* Don't continue as dedicated never has a thread running */
		return;
	}

	InvalidateWindow(WC_GENERATE_PROGRESS_WINDOW, 0);
	MarkWholeScreenDirty();

	/* Release the rights to the map generator, and acquire the rights to the
	 * paint thread. The 'other' thread already has the paint thread rights so
	 * this ensures us that we are waiting until the paint thread is done
	 * before we reacquire the mapgen rights */
	_genworld_mapgen_mutex->EndCritical();
	_genworld_paint_mutex->BeginCritical();
	_genworld_mapgen_mutex->BeginCritical();
	_genworld_paint_mutex->EndCritical();

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
