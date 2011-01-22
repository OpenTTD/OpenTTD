/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file genworld_gui.cpp GUI to configure and show progress during map generation. */

#include "stdafx.h"
#include "heightmap.h"
#include "debug.h"
#include "genworld.h"
#include "network/network.h"
#include "strings_func.h"
#include "window_func.h"
#include "date_func.h"
#include "sound_func.h"
#include "fios.h"
#include "string_func.h"
#include "widgets/dropdown_type.h"
#include "widgets/dropdown_func.h"
#include "landscape_type.h"
#include "querystring_gui.h"
#include "town.h"
#include "thread/thread.h"
#include "settings_func.h"
#include "core/geometry_func.hpp"
#include "core/random_func.hpp"

#include "table/strings.h"
#include "table/sprites.h"

/**
 * In what 'mode' the GenerateLandscapeWindowProc is.
 */
enum GenenerateLandscapeWindowMode {
	GLWM_GENERATE,  ///< Generate new game
	GLWM_HEIGHTMAP, ///< Load from heightmap
	GLWM_SCENARIO,  ///< Generate flat land
};

extern void SwitchToMode(SwitchMode new_mode);
extern void MakeNewgameSettingsLive();

/**
 * Changes landscape type and sets genworld window dirty
 * @param landscape new landscape type
 */
void SetNewLandscapeType(byte landscape)
{
	_settings_newgame.game_creation.landscape = landscape;
	InvalidateWindowClassesData(WC_SELECT_GAME);
	InvalidateWindowClassesData(WC_GENERATE_LANDSCAPE);
}

/** Widgets of GenerateLandscapeWindow */
enum GenerateLandscapeWindowWidgets {
	GLAND_TEMPERATE,          ///< Button with icon "Temperate"
	GLAND_ARCTIC,             ///< Button with icon "Arctic"
	GLAND_TROPICAL,           ///< Button with icon "Tropical"
	GLAND_TOYLAND,            ///< Button with icon "Toyland"

	GLAND_MAPSIZE_X_PULLDOWN, ///< Dropdown 'map X size'
	GLAND_MAPSIZE_Y_PULLDOWN, ///< Dropdown 'map Y size'

	GLAND_TOWN_PULLDOWN,      ///< Dropdown 'No. of towns'
	GLAND_INDUSTRY_PULLDOWN,  ///< Dropdown 'No. of industries'

	GLAND_RANDOM_EDITBOX,     ///< 'Random seed' editbox
	GLAND_RANDOM_BUTTON,      ///< 'Randomise' button

	GLAND_GENERATE_BUTTON,    ///< 'Generate' button

	GLAND_START_DATE_DOWN,    ///< Decrease start year
	GLAND_START_DATE_TEXT,    ///< Start year
	GLAND_START_DATE_UP,      ///< Increase start year

	GLAND_SNOW_LEVEL_DOWN,    ///< Decrease snow level
	GLAND_SNOW_LEVEL_TEXT,    ///< Snow level
	GLAND_SNOW_LEVEL_UP,      ///< Increase snow level

	GLAND_TREE_PULLDOWN,      ///< Dropdown 'Tree algorithm'
	GLAND_LANDSCAPE_PULLDOWN, ///< Dropdown 'Land generator'

	GLAND_HEIGHTMAP_NAME_TEXT,         ///< Heightmap name
	GLAND_HEIGHTMAP_NAME_SPACER,       ///< Spacer used for aligning items in the second column nicely
	GLAND_HEIGHTMAP_SIZE_TEXT,         ///< Size of heightmap
	GLAND_HEIGHTMAP_ROTATION_PULLDOWN, ///< Dropdown 'Heightmap rotation'

	GLAND_TERRAIN_PULLDOWN,    ///< Dropdown 'Terrain type'
	GLAND_WATER_PULLDOWN,      ///< Dropdown 'Sea level'
	GLAND_SMOOTHNESS_PULLDOWN, ///< Dropdown 'Smoothness'
	GLAND_VARIETY_PULLDOWN,    ///< Dropdown 'Variety distribution'

	GLAND_BORDERS_RANDOM,      ///< 'Random'/'Manual' borders
	GLAND_WATER_NW,            ///< NW 'Water'/'Freeform'
	GLAND_WATER_NE,            ///< NE 'Water'/'Freeform'
	GLAND_WATER_SE,            ///< SE 'Water'/'Freeform'
	GLAND_WATER_SW,            ///< SW 'Water'/'Freeform'
};

/** Widgets of GenerateLandscapeWindow when generating world */
static const NWidgetPart _nested_generate_landscape_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_MAPGEN_WORLD_GENERATION_CAPTION, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(NWID_SPACER), SetMinimalSize(0, 10),
		/* Landscape selection. */
		NWidget(NWID_HORIZONTAL), SetPIP(10, 0, 10),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, GLAND_TEMPERATE), SetDataTip(SPR_SELECT_TEMPERATE, STR_INTRO_TOOLTIP_TEMPERATE),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, GLAND_ARCTIC), SetDataTip(SPR_SELECT_SUB_ARCTIC, STR_INTRO_TOOLTIP_SUB_ARCTIC_LANDSCAPE),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, GLAND_TROPICAL), SetDataTip(SPR_SELECT_SUB_TROPICAL, STR_INTRO_TOOLTIP_SUB_TROPICAL_LANDSCAPE),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, GLAND_TOYLAND), SetDataTip(SPR_SELECT_TOYLAND, STR_INTRO_TOOLTIP_TOYLAND_LANDSCAPE),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 11),
		NWidget(NWID_HORIZONTAL), SetPIP(10, 5, 10),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
				/* Left column with labels. */
				NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPIP(0, 4, 0),
					NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_MAPSIZE, STR_NULL), SetFill(1, 1),
					NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_LAND_GENERATOR, STR_NULL), SetFill(1, 1),
					NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_NUMBER_OF_TOWNS, STR_NULL), SetFill(1, 1),
					NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_TERRAIN_TYPE, STR_NULL), SetFill(1, 1),
					NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_RANDOM_SEED, STR_NULL), SetFill(1, 1),
					NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_QUANTITY_OF_SEA_LAKES, STR_NULL), SetFill(1, 1),
					NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_TREE_PLACER, STR_NULL), SetFill(1, 1),
					NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_VARIETY, STR_NULL), SetFill(1, 1),
					NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_BORDER_TYPE, STR_NULL), SetFill(1, 1),
				EndContainer(),
				/* Widgets at the right of the labels. */
				NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPIP(0, 4, 0),
					/* Mapsize X * Y. */
					NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_MAPSIZE_X_PULLDOWN), SetDataTip(STR_JUST_INT, STR_NULL), SetFill(1, 0),
						NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_BY, STR_NULL), SetPadding(1, 0, 0, 0), SetFill(1, 1),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_MAPSIZE_Y_PULLDOWN), SetDataTip(STR_JUST_INT, STR_NULL), SetFill(1, 0),
					EndContainer(),
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_LANDSCAPE_PULLDOWN), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_TOWN_PULLDOWN), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_TERRAIN_PULLDOWN), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_EDITBOX, COLOUR_WHITE, GLAND_RANDOM_EDITBOX), SetDataTip(STR_MAPGEN_RANDOM_SEED_OSKTITLE, STR_MAPGEN_RANDOM_SEED_HELP), SetFill(1, 1),
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_WATER_PULLDOWN), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_TREE_PULLDOWN), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_VARIETY_PULLDOWN), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, GLAND_BORDERS_RANDOM), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_VERTICAL), SetPIP(0, 4, 0),
				NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
					NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPIP(0, 4, 0),
						NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_DATE, STR_NULL), SetFill(1, 1),
						NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_SNOW_LINE_HEIGHT, STR_NULL), SetFill(1, 1),
						NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_NUMBER_OF_INDUSTRIES, STR_NULL), SetFill(1, 1),
						NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_SMOOTHNESS, STR_NULL), SetFill(1, 1),
					EndContainer(),
					NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPIP(0, 4, 0),
						/* Starting date. */
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, GLAND_START_DATE_DOWN), SetDataTip(SPR_ARROW_DOWN, STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_BACKWARD), SetFill(0, 1),
							NWidget(WWT_TEXTBTN, COLOUR_ORANGE, GLAND_START_DATE_TEXT), SetDataTip(STR_BLACK_DATE_LONG, STR_NULL), SetFill(1, 0),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, GLAND_START_DATE_UP), SetDataTip(SPR_ARROW_UP, STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_FORWARD), SetFill(0, 1),
						EndContainer(),
						/* Snow line. */
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, GLAND_SNOW_LEVEL_DOWN), SetDataTip(SPR_ARROW_DOWN, STR_MAPGEN_SNOW_LINE_DOWN), SetFill(0, 1),
							NWidget(WWT_TEXTBTN, COLOUR_ORANGE, GLAND_SNOW_LEVEL_TEXT), SetDataTip(STR_BLACK_INT, STR_NULL), SetFill(1, 0),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, GLAND_SNOW_LEVEL_UP), SetDataTip(SPR_ARROW_UP, STR_MAPGEN_SNOW_LINE_UP), SetFill(0, 1),
						EndContainer(),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_INDUSTRY_PULLDOWN), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_SMOOTHNESS_PULLDOWN), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
					EndContainer(),
				EndContainer(),
				NWidget(WWT_TEXTBTN, COLOUR_ORANGE, GLAND_RANDOM_BUTTON), SetDataTip(STR_MAPGEN_RANDOM, STR_MAPGEN_RANDOM_HELP), SetFill(1, 0),
				NWidget(NWID_SPACER), SetFill(1, 1),
				NWidget(WWT_TEXTBTN, COLOUR_GREEN, GLAND_GENERATE_BUTTON), SetMinimalSize(84, 30), SetDataTip(STR_MAPGEN_GENERATE, STR_NULL), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 4),
		/* Map borders buttons for each edge. */
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(10, 0, 10),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 0, 3),
				NWidget(NWID_SPACER), SetFill(1, 1),
				NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_NORTHWEST, STR_NULL), SetPadding(1, 0, 0, 0), SetFill(0, 1),
			EndContainer(),
			NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, GLAND_WATER_NW), SetDataTip(STR_JUST_STRING, STR_MAPGEN_NORTHWEST), SetFill(1, 1),
			NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, GLAND_WATER_NE), SetDataTip(STR_JUST_STRING, STR_MAPGEN_NORTHEAST), SetFill(1, 1),
			NWidget(NWID_HORIZONTAL), SetPIP(3, 0, 0),
				NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_NORTHEAST, STR_NULL), SetPadding(1, 0, 0, 0), SetFill(0, 1),
				NWidget(NWID_SPACER), SetFill(1, 1),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(10, 0, 10),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 0, 3),
				NWidget(NWID_SPACER), SetFill(1, 1),
				NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_SOUTHWEST, STR_NULL), SetPadding(1, 0, 0, 0), SetFill(0, 1),
			EndContainer(),
			NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, GLAND_WATER_SW), SetDataTip(STR_JUST_STRING, STR_MAPGEN_SOUTHWEST), SetFill(1, 1),
			NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, GLAND_WATER_SE), SetDataTip(STR_JUST_STRING, STR_MAPGEN_SOUTHEAST), SetFill(1, 1),
			NWidget(NWID_HORIZONTAL), SetPIP(3, 0, 0),
				NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_SOUTHEAST, STR_NULL), SetPadding(1, 0, 0, 0), SetFill(0, 1),
				NWidget(NWID_SPACER), SetFill(1, 1),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 9), SetFill(1, 1),
	EndContainer(),
};

/** Widgets of GenerateLandscapeWindow when loading heightmap */
static const NWidgetPart _nested_heightmap_load_widgets[] = {
	/* Window header. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_MAPGEN_WORLD_GENERATION_CAPTION, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(NWID_SPACER), SetMinimalSize(0, 10),
		/* Landscape selection. */
		NWidget(NWID_HORIZONTAL), SetPIP(10, 0, 10),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, GLAND_TEMPERATE), SetDataTip(SPR_SELECT_TEMPERATE, STR_INTRO_TOOLTIP_TEMPERATE),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, GLAND_ARCTIC), SetDataTip(SPR_SELECT_SUB_ARCTIC, STR_INTRO_TOOLTIP_SUB_ARCTIC_LANDSCAPE),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, GLAND_TROPICAL), SetDataTip(SPR_SELECT_SUB_TROPICAL, STR_INTRO_TOOLTIP_SUB_TROPICAL_LANDSCAPE),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, GLAND_TOYLAND), SetDataTip(SPR_SELECT_TOYLAND, STR_INTRO_TOOLTIP_TOYLAND_LANDSCAPE),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 11), SetFill(0, 1),
		NWidget(NWID_HORIZONTAL), SetPIP(10, 3, 10),
			/* Labels at the left side. */
			NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_HEIGHTMAP_NAME, STR_NULL), SetFill(1, 1),
				NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_MAPSIZE, STR_NULL), SetFill(1, 1),
				NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_NUMBER_OF_TOWNS, STR_NULL), SetFill(1, 1),
				NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_NUMBER_OF_INDUSTRIES, STR_NULL), SetFill(1, 1),
				NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_RANDOM_SEED, STR_NULL), SetFill(1, 1),
				NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_TREE_PLACER, STR_NULL), SetFill(1, 1),
				NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_HEIGHTMAP_ROTATION, STR_NULL), SetFill(1, 1),
			EndContainer(),
			/* Widgets at the right of the labels. */
			NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPIP(0, 4, 0),
				NWidget(WWT_EMPTY, COLOUR_ORANGE, GLAND_HEIGHTMAP_NAME_TEXT), SetFill(1, 0),
				/* Mapsize X * Y. */
				NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_MAPSIZE_X_PULLDOWN), SetDataTip(STR_JUST_INT, STR_NULL), SetFill(1, 0),
					NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_BY, STR_NULL), SetPadding(1, 0, 0, 0), SetFill(1, 1),
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_MAPSIZE_Y_PULLDOWN), SetDataTip(STR_JUST_INT, STR_NULL), SetFill(1, 0),
				EndContainer(),
				NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_TOWN_PULLDOWN), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
				NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_INDUSTRY_PULLDOWN), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
				NWidget(WWT_EDITBOX, COLOUR_WHITE, GLAND_RANDOM_EDITBOX), SetDataTip(STR_MAPGEN_RANDOM_SEED_OSKTITLE, STR_MAPGEN_RANDOM_SEED_HELP), SetFill(1, 1),
				NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_TREE_PULLDOWN), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
				NWidget(WWT_DROPDOWN, COLOUR_ORANGE, GLAND_HEIGHTMAP_ROTATION_PULLDOWN), SetDataTip(STR_JUST_STRING, STR_NULL), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_VERTICAL), SetPIP(0, 4, 0),
				NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
					NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPIP(0, 4, 0),
						NWidget(WWT_EMPTY, INVALID_COLOUR, GLAND_HEIGHTMAP_NAME_SPACER), SetFill(1, 0),
						NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_HEIGHTMAP_SIZE_LABEL, STR_NULL), SetFill(1, 1),
						NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_DATE, STR_NULL), SetFill(1, 1),
						NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_SNOW_LINE_HEIGHT, STR_NULL), SetFill(1, 1),
					EndContainer(),
					NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPIP(0, 4, 0),
						NWidget(WWT_EMPTY, INVALID_COLOUR, GLAND_HEIGHTMAP_NAME_SPACER), SetFill(1, 0),
						NWidget(WWT_TEXT, COLOUR_ORANGE, GLAND_HEIGHTMAP_SIZE_TEXT), SetDataTip(STR_MAPGEN_HEIGHTMAP_SIZE, STR_NULL), SetFill(1, 0),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, GLAND_START_DATE_DOWN), SetDataTip(SPR_ARROW_DOWN, STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_BACKWARD), SetFill(0, 1),
							NWidget(WWT_TEXTBTN, COLOUR_ORANGE, GLAND_START_DATE_TEXT), SetDataTip(STR_BLACK_DATE_LONG, STR_NULL), SetFill(1, 0),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, GLAND_START_DATE_UP), SetDataTip(SPR_ARROW_UP, STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_FORWARD), SetFill(0, 1),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, GLAND_SNOW_LEVEL_DOWN), SetDataTip(SPR_ARROW_DOWN, STR_MAPGEN_SNOW_LINE_DOWN), SetFill(0, 1),
							NWidget(WWT_TEXTBTN, COLOUR_ORANGE, GLAND_SNOW_LEVEL_TEXT), SetDataTip(STR_BLACK_INT, STR_NULL), SetFill(1, 0),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, GLAND_SNOW_LEVEL_UP), SetDataTip(SPR_ARROW_UP, STR_MAPGEN_SNOW_LINE_UP), SetFill(0, 1),
						EndContainer(),
					EndContainer(),
				EndContainer(),
				NWidget(WWT_TEXTBTN, COLOUR_ORANGE, GLAND_RANDOM_BUTTON), SetDataTip(STR_MAPGEN_RANDOM, STR_MAPGEN_RANDOM_HELP), SetFill(1, 0),
				NWidget(WWT_TEXTBTN, COLOUR_GREEN, GLAND_GENERATE_BUTTON), SetDataTip(STR_MAPGEN_GENERATE, STR_NULL), SetFill(1, 1),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 9), SetFill(1, 1),
	EndContainer(),
};

static void StartGeneratingLandscape(GenenerateLandscapeWindowMode mode)
{
	DeleteAllNonVitalWindows();

	/* Copy all XXX_newgame to XXX when coming from outside the editor */
	MakeNewgameSettingsLive();
	ResetGRFConfig(true);

	SndPlayFx(SND_15_BEEP);
	switch (mode) {
		case GLWM_GENERATE:  _switch_mode = (_game_mode == GM_EDITOR) ? SM_GENRANDLAND    : SM_NEWGAME;         break;
		case GLWM_HEIGHTMAP: _switch_mode = (_game_mode == GM_EDITOR) ? SM_LOAD_HEIGHTMAP : SM_START_HEIGHTMAP; break;
		case GLWM_SCENARIO:  _switch_mode = SM_EDITOR; break;
		default: NOT_REACHED();
	}
}

static void LandscapeGenerationCallback(Window *w, bool confirmed)
{
	if (confirmed) StartGeneratingLandscape((GenenerateLandscapeWindowMode)w->window_number);
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
static const StringID _sea_lakes[]   = {STR_SEA_LEVEL_VERY_LOW, STR_SEA_LEVEL_LOW, STR_SEA_LEVEL_MEDIUM, STR_SEA_LEVEL_HIGH, STR_SEA_LEVEL_CUSTOM, INVALID_STRING_ID};
static const StringID _smoothness[]  = {STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN_VERY_SMOOTH, STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN_SMOOTH, STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN_ROUGH, STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN_VERY_ROUGH, INVALID_STRING_ID};
static const StringID _tree_placer[] = {STR_CONFIG_SETTING_TREE_PLACER_NONE, STR_CONFIG_SETTING_TREE_PLACER_ORIGINAL, STR_CONFIG_SETTING_TREE_PLACER_IMPROVED, INVALID_STRING_ID};
static const StringID _rotation[]    = {STR_CONFIG_SETTING_HEIGHTMAP_ROTATION_COUNTER_CLOCKWISE, STR_CONFIG_SETTING_HEIGHTMAP_ROTATION_CLOCKWISE, INVALID_STRING_ID};
static const StringID _landscape[]   = {STR_CONFIG_SETTING_LAND_GENERATOR_ORIGINAL, STR_CONFIG_SETTING_LAND_GENERATOR_TERRA_GENESIS, INVALID_STRING_ID};
static const StringID _num_towns[]   = {STR_NUM_VERY_LOW, STR_NUM_LOW, STR_NUM_NORMAL, STR_NUM_HIGH, STR_NUM_CUSTOM, INVALID_STRING_ID};
static const StringID _num_inds[]    = {STR_NONE, STR_NUM_VERY_LOW, STR_NUM_LOW, STR_NUM_NORMAL, STR_NUM_HIGH, INVALID_STRING_ID};
static const StringID _variety[]     = {STR_VARIETY_NONE, STR_VARIETY_VERY_LOW, STR_VARIETY_LOW, STR_VARIETY_MEDIUM, STR_VARIETY_HIGH, STR_VARIETY_VERY_HIGH, INVALID_STRING_ID};

struct GenerateLandscapeWindow : public QueryStringBaseWindow {
	uint widget_id;
	uint x;
	uint y;
	char name[64];
	GenenerateLandscapeWindowMode mode;

	GenerateLandscapeWindow(const WindowDesc *desc, WindowNumber number = 0) : QueryStringBaseWindow(11)
	{
		this->InitNested(desc, number);

		this->LowerWidget(_settings_newgame.game_creation.landscape + GLAND_TEMPERATE);

		/* snprintf() always outputs trailing '\0', so whole buffer can be used */
		snprintf(this->edit_str_buf, this->edit_str_size, "%u", _settings_newgame.game_creation.generation_seed);
		InitializeTextBuffer(&this->text, this->edit_str_buf, this->edit_str_size, 120);
		this->caption = STR_NULL;
		this->afilter = CS_NUMERAL;

		this->mode = (GenenerateLandscapeWindowMode)this->window_number;

		/* Disable town, industry and trees in SE */
		this->SetWidgetDisabledState(GLAND_TOWN_PULLDOWN,     _game_mode == GM_EDITOR);
		this->SetWidgetDisabledState(GLAND_INDUSTRY_PULLDOWN, _game_mode == GM_EDITOR);
		this->SetWidgetDisabledState(GLAND_TREE_PULLDOWN,     _game_mode == GM_EDITOR);

		this->OnInvalidateData();
	}


	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case GLAND_START_DATE_TEXT:     SetDParam(0, ConvertYMDToDate(_settings_newgame.game_creation.starting_year, 0, 1)); break;
			case GLAND_MAPSIZE_X_PULLDOWN:  SetDParam(0, 1 << _settings_newgame.game_creation.map_x); break;
			case GLAND_MAPSIZE_Y_PULLDOWN:  SetDParam(0, 1 << _settings_newgame.game_creation.map_y); break;
			case GLAND_SNOW_LEVEL_TEXT:     SetDParam(0, _settings_newgame.game_creation.snow_line_height); break;

			case GLAND_TOWN_PULLDOWN:
				if (_game_mode == GM_EDITOR) {
					SetDParam(0, STR_DISASTERS_OFF);
				} else if (_settings_newgame.difficulty.number_towns == CUSTOM_TOWN_NUMBER_DIFFICULTY) {
					SetDParam(0, STR_NUM_CUSTOM_NUMBER);
					SetDParam(1, _settings_newgame.game_creation.custom_town_number);
				} else {
					SetDParam(0, _num_towns[_settings_newgame.difficulty.number_towns]);
				}
				break;

			case GLAND_INDUSTRY_PULLDOWN:   SetDParam(0, _game_mode == GM_EDITOR ? STR_DISASTERS_OFF : _num_inds[_settings_newgame.difficulty.number_industries]); break;
			case GLAND_LANDSCAPE_PULLDOWN:  SetDParam(0, _landscape[_settings_newgame.game_creation.land_generator]); break;
			case GLAND_TREE_PULLDOWN:       SetDParam(0, _tree_placer[_settings_newgame.game_creation.tree_placer]); break;
			case GLAND_TERRAIN_PULLDOWN:    SetDParam(0, _elevations[_settings_newgame.difficulty.terrain_type]); break;

			case GLAND_WATER_PULLDOWN:
				if (_settings_newgame.difficulty.quantity_sea_lakes == CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY) {
					SetDParam(0, STR_SEA_LEVEL_CUSTOM_PERCENTAGE);
					SetDParam(1, _settings_newgame.game_creation.custom_sea_level);
				} else {
					SetDParam(0, _sea_lakes[_settings_newgame.difficulty.quantity_sea_lakes]);
				}
				break;

			case GLAND_SMOOTHNESS_PULLDOWN: SetDParam(0, _smoothness[_settings_newgame.game_creation.tgen_smoothness]); break;
			case GLAND_VARIETY_PULLDOWN:    SetDParam(0, _variety[_settings_newgame.game_creation.variety]); break;
			case GLAND_BORDERS_RANDOM:      SetDParam(0, (_settings_newgame.game_creation.water_borders == BORDERS_RANDOM) ? STR_MAPGEN_BORDER_RANDOMIZE : STR_MAPGEN_BORDER_MANUAL); break;
			case GLAND_WATER_NE: SetDParam(0, (_settings_newgame.game_creation.water_borders == BORDERS_RANDOM) ? STR_MAPGEN_BORDER_RANDOM : HasBit(_settings_newgame.game_creation.water_borders, BORDER_NE) ? STR_MAPGEN_BORDER_WATER : STR_MAPGEN_BORDER_FREEFORM); break;
			case GLAND_WATER_NW: SetDParam(0, (_settings_newgame.game_creation.water_borders == BORDERS_RANDOM) ? STR_MAPGEN_BORDER_RANDOM : HasBit(_settings_newgame.game_creation.water_borders, BORDER_NW) ? STR_MAPGEN_BORDER_WATER : STR_MAPGEN_BORDER_FREEFORM); break;
			case GLAND_WATER_SE: SetDParam(0, (_settings_newgame.game_creation.water_borders == BORDERS_RANDOM) ? STR_MAPGEN_BORDER_RANDOM : HasBit(_settings_newgame.game_creation.water_borders, BORDER_SE) ? STR_MAPGEN_BORDER_WATER : STR_MAPGEN_BORDER_FREEFORM); break;
			case GLAND_WATER_SW: SetDParam(0, (_settings_newgame.game_creation.water_borders == BORDERS_RANDOM) ? STR_MAPGEN_BORDER_RANDOM : HasBit(_settings_newgame.game_creation.water_borders, BORDER_SW) ? STR_MAPGEN_BORDER_WATER : STR_MAPGEN_BORDER_FREEFORM); break;
			case GLAND_HEIGHTMAP_ROTATION_PULLDOWN: SetDParam(0, _rotation[_settings_newgame.game_creation.heightmap_rotation]); break;

			case GLAND_HEIGHTMAP_SIZE_TEXT:
				if (_settings_newgame.game_creation.heightmap_rotation == HM_CLOCKWISE) {
					SetDParam(0, this->y);
					SetDParam(1, this->x);
				} else {
					SetDParam(0, this->x);
					SetDParam(1, this->y);
				}
				break;
		}
	}

	virtual void OnInvalidateData(int data = 0)
	{
		/* Update the climate buttons */
		this->SetWidgetLoweredState(GLAND_TEMPERATE, _settings_newgame.game_creation.landscape == LT_TEMPERATE);
		this->SetWidgetLoweredState(GLAND_ARCTIC,    _settings_newgame.game_creation.landscape == LT_ARCTIC);
		this->SetWidgetLoweredState(GLAND_TROPICAL,  _settings_newgame.game_creation.landscape == LT_TROPIC);
		this->SetWidgetLoweredState(GLAND_TOYLAND,   _settings_newgame.game_creation.landscape == LT_TOYLAND);

		/* You can't select smoothness / non-water borders if not terragenesis */
		if (mode == GLWM_GENERATE) {
			this->SetWidgetDisabledState(GLAND_SMOOTHNESS_PULLDOWN, _settings_newgame.game_creation.land_generator == 0);
			this->SetWidgetDisabledState(GLAND_VARIETY_PULLDOWN, _settings_newgame.game_creation.land_generator == 0);
			this->SetWidgetDisabledState(GLAND_BORDERS_RANDOM, _settings_newgame.game_creation.land_generator == 0 || !_settings_newgame.construction.freeform_edges);
			this->SetWidgetsDisabledState(_settings_newgame.game_creation.land_generator == 0 || !_settings_newgame.construction.freeform_edges || _settings_newgame.game_creation.water_borders == BORDERS_RANDOM,
					GLAND_WATER_NW, GLAND_WATER_NE, GLAND_WATER_SE, GLAND_WATER_SW, WIDGET_LIST_END);

			this->SetWidgetLoweredState(GLAND_BORDERS_RANDOM, _settings_newgame.game_creation.water_borders == BORDERS_RANDOM);

			this->SetWidgetLoweredState(GLAND_WATER_NW, HasBit(_settings_newgame.game_creation.water_borders, BORDER_NW));
			this->SetWidgetLoweredState(GLAND_WATER_NE, HasBit(_settings_newgame.game_creation.water_borders, BORDER_NE));
			this->SetWidgetLoweredState(GLAND_WATER_SE, HasBit(_settings_newgame.game_creation.water_borders, BORDER_SE));
			this->SetWidgetLoweredState(GLAND_WATER_SW, HasBit(_settings_newgame.game_creation.water_borders, BORDER_SW));

			this->SetWidgetsDisabledState(_settings_newgame.game_creation.land_generator == 0 && (_settings_newgame.game_creation.landscape == LT_ARCTIC || _settings_newgame.game_creation.landscape == LT_TROPIC),
					GLAND_TERRAIN_PULLDOWN, GLAND_WATER_PULLDOWN, WIDGET_LIST_END);
		}

		/* Disable snowline if not arctic */
		this->SetWidgetDisabledState(GLAND_SNOW_LEVEL_TEXT, _settings_newgame.game_creation.landscape != LT_ARCTIC);

		/* Update availability of decreasing / increasing start date and snow level */
		this->SetWidgetDisabledState(GLAND_START_DATE_DOWN, _settings_newgame.game_creation.starting_year <= MIN_YEAR);
		this->SetWidgetDisabledState(GLAND_START_DATE_UP,   _settings_newgame.game_creation.starting_year >= MAX_YEAR);
		this->SetWidgetDisabledState(GLAND_SNOW_LEVEL_DOWN, _settings_newgame.game_creation.snow_line_height <= MIN_SNOWLINE_HEIGHT || _settings_newgame.game_creation.landscape != LT_ARCTIC);
		this->SetWidgetDisabledState(GLAND_SNOW_LEVEL_UP,   _settings_newgame.game_creation.snow_line_height >= MAX_SNOWLINE_HEIGHT || _settings_newgame.game_creation.landscape != LT_ARCTIC);

		/* Do not allow a custom sea level with the original land generator. */
		if (_settings_newgame.game_creation.land_generator == 0 &&
				_settings_newgame.difficulty.quantity_sea_lakes == CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY) {
			_settings_newgame.difficulty.quantity_sea_lakes = CUSTOM_SEA_LEVEL_MIN_PERCENTAGE;
		}

	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		const StringID *strs = NULL;
		switch (widget) {
			case GLAND_START_DATE_TEXT:
				SetDParam(0, ConvertYMDToDate(MAX_YEAR, 0, 1));
				*size = GetStringBoundingBox(STR_BLACK_DATE_LONG);
				break;

			case GLAND_MAPSIZE_X_PULLDOWN:
			case GLAND_MAPSIZE_Y_PULLDOWN:
				SetDParam(0, MAX_MAP_SIZE);
				*size = GetStringBoundingBox(STR_JUST_INT);
				break;

			case GLAND_SNOW_LEVEL_TEXT:
				SetDParam(0, MAX_TILE_HEIGHT);
				*size = GetStringBoundingBox(STR_JUST_INT);
				break;

			case GLAND_HEIGHTMAP_SIZE_TEXT:
				SetDParam(0, this->x);
				SetDParam(1, this->y);
				*size = GetStringBoundingBox(STR_MAPGEN_HEIGHTMAP_SIZE);
				break;

			case GLAND_TOWN_PULLDOWN:
				strs = _num_towns;
				SetDParam(0, CUSTOM_TOWN_MAX_NUMBER);
				*size = GetStringBoundingBox(STR_NUM_CUSTOM_NUMBER);
				break;

			case GLAND_INDUSTRY_PULLDOWN:   strs = _num_inds; break;
			case GLAND_LANDSCAPE_PULLDOWN:  strs = _landscape; break;
			case GLAND_TREE_PULLDOWN:       strs = _tree_placer; break;
			case GLAND_TERRAIN_PULLDOWN:    strs = _elevations; break;
			case GLAND_WATER_PULLDOWN:
				strs = _sea_lakes;
				SetDParam(0, CUSTOM_SEA_LEVEL_MAX_PERCENTAGE);
				*size = GetStringBoundingBox(STR_SEA_LEVEL_CUSTOM_PERCENTAGE);
				break;

			case GLAND_SMOOTHNESS_PULLDOWN: strs = _smoothness; break;
			case GLAND_VARIETY_PULLDOWN:    strs = _variety; break;
			case GLAND_HEIGHTMAP_ROTATION_PULLDOWN: strs = _rotation; break;
			case GLAND_BORDERS_RANDOM:
				*size = maxdim(GetStringBoundingBox(STR_MAPGEN_BORDER_RANDOMIZE), GetStringBoundingBox(STR_MAPGEN_BORDER_MANUAL));
				break;

			case GLAND_WATER_NE:
			case GLAND_WATER_NW:
			case GLAND_WATER_SE:
			case GLAND_WATER_SW:
				*size = maxdim(GetStringBoundingBox(STR_MAPGEN_BORDER_RANDOM), maxdim(GetStringBoundingBox(STR_MAPGEN_BORDER_WATER), GetStringBoundingBox(STR_MAPGEN_BORDER_FREEFORM)));
				break;

			case GLAND_HEIGHTMAP_NAME_SPACER:
			case GLAND_HEIGHTMAP_NAME_TEXT:
				size->width = 0;
				break;

			default:
				return;
		}
		if (strs != NULL) {
			while (*strs != INVALID_STRING_ID) {
				*size = maxdim(*size, GetStringBoundingBox(*strs++));
			}
		}
		size->width += padding.width;
		size->height = FONT_HEIGHT_NORMAL + WD_DROPDOWNTEXT_TOP + WD_DROPDOWNTEXT_BOTTOM;
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case GLAND_HEIGHTMAP_NAME_TEXT: {
				/* Little bit of a hack going on here; just to get the widgets
				 * spaced without doing much magic. The space we can draw on is
				 * covered by both the spacer and text widgets, so take their
				 * outer most boundaries (left and right) as draw locations. */
				const NWidgetBase *nwi_spacer = this->GetWidget<NWidgetBase>(GLAND_HEIGHTMAP_NAME_SPACER);
				DrawString(min(r.left, nwi_spacer->pos_x), max<int>(r.right, nwi_spacer->pos_x + nwi_spacer->current_x), r.top, this->name, TC_ORANGE);
				break;
			}
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
		this->DrawEditBox(GLAND_RANDOM_EDITBOX);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case GLAND_TEMPERATE:
			case GLAND_ARCTIC:
			case GLAND_TROPICAL:
			case GLAND_TOYLAND:
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

				if (mode == GLWM_HEIGHTMAP &&
						(this->x * 2 < (1U << _settings_newgame.game_creation.map_x) ||
						this->x / 2 > (1U << _settings_newgame.game_creation.map_x) ||
						this->y * 2 < (1U << _settings_newgame.game_creation.map_y) ||
						this->y / 2 > (1U << _settings_newgame.game_creation.map_y))) {
					ShowQuery(
						STR_WARNING_HEIGHTMAP_SCALE_CAPTION,
						STR_WARNING_HEIGHTMAP_SCALE_MESSAGE,
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

					_settings_newgame.game_creation.starting_year = Clamp(_settings_newgame.game_creation.starting_year + widget - GLAND_START_DATE_TEXT, MIN_YEAR, MAX_YEAR);
					this->InvalidateData();
				}
				_left_button_clicked = false;
				break;

			case GLAND_START_DATE_TEXT: // Year text
				this->widget_id = GLAND_START_DATE_TEXT;
				SetDParam(0, _settings_newgame.game_creation.starting_year);
				ShowQueryString(STR_JUST_INT, STR_MAPGEN_START_DATE_QUERY_CAPT, 8, 100, this, CS_NUMERAL, QSF_ENABLE_DEFAULT);
				break;

			case GLAND_SNOW_LEVEL_DOWN:
			case GLAND_SNOW_LEVEL_UP: // Snow line buttons
				/* Don't allow too fast scrolling */
				if ((this->flags4 & WF_TIMEOUT_MASK) <= WF_TIMEOUT_TRIGGER) {
					this->HandleButtonClick(widget);

					_settings_newgame.game_creation.snow_line_height = Clamp(_settings_newgame.game_creation.snow_line_height + widget - GLAND_SNOW_LEVEL_TEXT, MIN_SNOWLINE_HEIGHT, MAX_SNOWLINE_HEIGHT);
					this->InvalidateData();
				}
				_left_button_clicked = false;
				break;

			case GLAND_SNOW_LEVEL_TEXT: // Snow line text
				this->widget_id = GLAND_SNOW_LEVEL_TEXT;
				SetDParam(0, _settings_newgame.game_creation.snow_line_height);
				ShowQueryString(STR_JUST_INT, STR_MAPGEN_SNOW_LINE_QUERY_CAPT, 3, 100, this, CS_NUMERAL, QSF_ENABLE_DEFAULT);
				break;

			case GLAND_TREE_PULLDOWN: // Tree placer
				ShowDropDownMenu(this, _tree_placer, _settings_newgame.game_creation.tree_placer, GLAND_TREE_PULLDOWN, 0, 0);
				break;

			case GLAND_LANDSCAPE_PULLDOWN: // Landscape generator
				ShowDropDownMenu(this, _landscape, _settings_newgame.game_creation.land_generator, GLAND_LANDSCAPE_PULLDOWN, 0, 0);
				break;

			case GLAND_HEIGHTMAP_ROTATION_PULLDOWN: // Heightmap rotation
				ShowDropDownMenu(this, _rotation, _settings_newgame.game_creation.heightmap_rotation, GLAND_HEIGHTMAP_ROTATION_PULLDOWN, 0, 0);
				break;

			case GLAND_TERRAIN_PULLDOWN: // Terrain type
				ShowDropDownMenu(this, _elevations, _settings_newgame.difficulty.terrain_type, GLAND_TERRAIN_PULLDOWN, 0, 0);
				break;

			case GLAND_WATER_PULLDOWN: { // Water quantity
				uint32 hidden_mask = 0;
				/* Disable custom water level when the original map generator is active. */
				if (_settings_newgame.game_creation.land_generator == 0) {
					SetBit(hidden_mask, CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY);
				}
				ShowDropDownMenu(this, _sea_lakes, _settings_newgame.difficulty.quantity_sea_lakes, GLAND_WATER_PULLDOWN, 0, hidden_mask);
				break;
			}

			case GLAND_SMOOTHNESS_PULLDOWN: // Map smoothness
				ShowDropDownMenu(this, _smoothness, _settings_newgame.game_creation.tgen_smoothness, GLAND_SMOOTHNESS_PULLDOWN, 0, 0);
				break;

			case GLAND_VARIETY_PULLDOWN: // Map variety
				ShowDropDownMenu(this, _variety, _settings_newgame.game_creation.variety, GLAND_VARIETY_PULLDOWN, 0, 0);
				break;

			/* Freetype map borders */
			case GLAND_WATER_NW:
				_settings_newgame.game_creation.water_borders = ToggleBit(_settings_newgame.game_creation.water_borders, BORDER_NW);
				this->InvalidateData();
				break;

			case GLAND_WATER_NE:
				_settings_newgame.game_creation.water_borders = ToggleBit(_settings_newgame.game_creation.water_borders, BORDER_NE);
				this->InvalidateData();
				break;

			case GLAND_WATER_SE:
				_settings_newgame.game_creation.water_borders = ToggleBit(_settings_newgame.game_creation.water_borders, BORDER_SE);
				this->InvalidateData();
				break;

			case GLAND_WATER_SW:
				_settings_newgame.game_creation.water_borders = ToggleBit(_settings_newgame.game_creation.water_borders, BORDER_SW);
				this->InvalidateData();
				break;

			case GLAND_BORDERS_RANDOM:
				_settings_newgame.game_creation.water_borders = (_settings_newgame.game_creation.water_borders == BORDERS_RANDOM) ? 0 : BORDERS_RANDOM;
				this->InvalidateData();
				break;
		}
	}

	virtual void OnTimeout()
	{
		static const int raise_widgets[] = {GLAND_START_DATE_DOWN, GLAND_START_DATE_UP, GLAND_SNOW_LEVEL_UP, GLAND_SNOW_LEVEL_DOWN, WIDGET_LIST_END};
		for (const int *widget = raise_widgets; *widget != WIDGET_LIST_END; widget++) {
			if (this->IsWidgetLowered(*widget)) {
				this->RaiseWidget(*widget);
				this->SetWidgetDirty(*widget);
			}
		}
	}

	virtual void OnMouseLoop()
	{
		this->HandleEditBox(GLAND_RANDOM_EDITBOX);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		EventState state = ES_NOT_HANDLED;
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
			case GLAND_VARIETY_PULLDOWN:       _settings_newgame.game_creation.variety = index; break;
			case GLAND_LANDSCAPE_PULLDOWN:     _settings_newgame.game_creation.land_generator = index; break;
			case GLAND_HEIGHTMAP_ROTATION_PULLDOWN: _settings_newgame.game_creation.heightmap_rotation = index; break;

			case GLAND_TOWN_PULLDOWN:
				if ((uint)index == CUSTOM_TOWN_NUMBER_DIFFICULTY) {
					this->widget_id = widget;
					SetDParam(0, _settings_newgame.game_creation.custom_town_number);
					ShowQueryString(STR_JUST_INT, STR_MAPGEN_NUMBER_OF_TOWNS, 5, 50, this, CS_NUMERAL, QSF_NONE);
				}
				IConsoleSetSetting("difficulty.number_towns", index);
				break;

			case GLAND_INDUSTRY_PULLDOWN:
				IConsoleSetSetting("difficulty.number_industries", index);
				break;

			case GLAND_TERRAIN_PULLDOWN: {
				GameMode old_gm = _game_mode;
				_game_mode = GM_MENU;
				IConsoleSetSetting("difficulty.terrain_type", index);
				_game_mode = old_gm;
				break;
			}

			case GLAND_WATER_PULLDOWN: {
				if ((uint)index == CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY) {
					this->widget_id = widget;
					SetDParam(0, _settings_newgame.game_creation.custom_sea_level);
					ShowQueryString(STR_JUST_INT, STR_MAPGEN_QUANTITY_OF_SEA_LAKES, 3, 50, this, CS_NUMERAL, QSF_NONE);
				}
				GameMode old_gm = _game_mode;
				_game_mode = GM_MENU;
				IConsoleSetSetting("difficulty.quantity_sea_lakes", index);
				_game_mode = old_gm;
				break;
			}
		}
		this->InvalidateData();
	}

	virtual void OnQueryTextFinished(char *str)
	{
		/* Was 'cancel' pressed? */
		if (str == NULL) return;

		int32 value;
		if (!StrEmpty(str)) {
			value = atoi(str);
		} else {
			/* An empty string means revert to the default */
			switch (this->widget_id) {
				case GLAND_START_DATE_TEXT: value = DEF_START_YEAR; break;
				case GLAND_SNOW_LEVEL_TEXT: value = DEF_SNOWLINE_HEIGHT; break;
				case GLAND_TOWN_PULLDOWN:   value = 1; break;
				case GLAND_WATER_PULLDOWN:  value = CUSTOM_SEA_LEVEL_MIN_PERCENTAGE; break;
				default: NOT_REACHED();
			}
		}

		switch (this->widget_id) {
			case GLAND_START_DATE_TEXT:
				this->SetWidgetDirty(GLAND_START_DATE_TEXT);
				_settings_newgame.game_creation.starting_year = Clamp(value, MIN_YEAR, MAX_YEAR);
				break;

			case GLAND_SNOW_LEVEL_TEXT:
				this->SetWidgetDirty(GLAND_SNOW_LEVEL_TEXT);
				_settings_newgame.game_creation.snow_line_height = Clamp(value, MIN_SNOWLINE_HEIGHT, MAX_SNOWLINE_HEIGHT);
				break;

			case GLAND_TOWN_PULLDOWN:
				_settings_newgame.game_creation.custom_town_number = Clamp(value, 1, CUSTOM_TOWN_MAX_NUMBER);
				break;

			case GLAND_WATER_PULLDOWN:
				_settings_newgame.game_creation.custom_sea_level = Clamp(value, CUSTOM_SEA_LEVEL_MIN_PERCENTAGE, CUSTOM_SEA_LEVEL_MAX_PERCENTAGE);
				break;
		}

		this->InvalidateData();
	}
};

static const WindowDesc _generate_landscape_desc(
	WDP_CENTER, 0, 0,
	WC_GENERATE_LANDSCAPE, WC_NONE,
	0,
	_nested_generate_landscape_widgets, lengthof(_nested_generate_landscape_widgets)
);

static const WindowDesc _heightmap_load_desc(
	WDP_CENTER, 0, 0,
	WC_GENERATE_LANDSCAPE, WC_NONE,
	0,
	_nested_heightmap_load_widgets, lengthof(_nested_heightmap_load_widgets)
);

static void _ShowGenerateLandscape(GenenerateLandscapeWindowMode mode)
{
	uint x = 0;
	uint y = 0;

	DeleteWindowByClass(WC_GENERATE_LANDSCAPE);

	/* Always give a new seed if not editor */
	if (_game_mode != GM_EDITOR) _settings_newgame.game_creation.generation_seed = InteractiveRandom();

	if (mode == GLWM_HEIGHTMAP) {
		/* If the function returns negative, it means there was a problem loading the heightmap */
		if (!GetHeightmapDimensions(_file_to_saveload.name, &x, &y)) return;
	}

	GenerateLandscapeWindow *w = AllocateWindowDescFront<GenerateLandscapeWindow>((mode == GLWM_HEIGHTMAP) ? &_heightmap_load_desc : &_generate_landscape_desc, mode);

	if (mode == GLWM_HEIGHTMAP) {
		w->x = x;
		w->y = y;
		strecpy(w->name, _file_to_saveload.title, lastof(w->name));
	}

	SetWindowDirty(WC_GENERATE_LANDSCAPE, mode);
}

void ShowGenerateLandscape()
{
	_ShowGenerateLandscape(GLWM_GENERATE);
}

void ShowHeightmapLoad()
{
	_ShowGenerateLandscape(GLWM_HEIGHTMAP);
}

void StartScenarioEditor()
{
	StartGeneratingLandscape(GLWM_SCENARIO);
}

void StartNewGameWithoutGUI(uint seed)
{
	/* GenerateWorld takes care of the possible GENERATE_NEW_SEED value in 'seed' */
	_settings_newgame.game_creation.generation_seed = seed;

	StartGeneratingLandscape(GLWM_GENERATE);
}

/** Widget numbers of the create scenario window. */
enum CreateScenarioWindowWidgets {
	CSCEN_TEMPERATE,              ///< Select temperate landscape style.
	CSCEN_ARCTIC,                 ///< Select arctic landscape style.
	CSCEN_TROPICAL,               ///< Select tropical landscape style.
	CSCEN_TOYLAND,                ///< Select toy-land landscape style.
	CSCEN_EMPTY_WORLD,            ///< Generate an empty flat world.
	CSCEN_RANDOM_WORLD,           ///< Generate random land button
	CSCEN_MAPSIZE_X_PULLDOWN,     ///< Pull-down arrow for x map size.
	CSCEN_MAPSIZE_Y_PULLDOWN,     ///< Pull-down arrow for y map size.
	CSCEN_START_DATE_DOWN,        ///< Decrease start year (start earlier).
	CSCEN_START_DATE_TEXT,        ///< Clickable start date value.
	CSCEN_START_DATE_UP,          ///< Increase start year (start later).
	CSCEN_FLAT_LAND_HEIGHT_DOWN,  ///< Decrease flat land height.
	CSCEN_FLAT_LAND_HEIGHT_TEXT,  ///< Clickable flat land height value.
	CSCEN_FLAT_LAND_HEIGHT_UP     ///< Increase flat land height.
};


struct CreateScenarioWindow : public Window
{
	uint widget_id;

	CreateScenarioWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->InitNested(desc, window_number);
		this->LowerWidget(_settings_newgame.game_creation.landscape + CSCEN_TEMPERATE);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case CSCEN_START_DATE_TEXT:
				SetDParam(0, ConvertYMDToDate(_settings_newgame.game_creation.starting_year, 0, 1));
				break;

			case CSCEN_MAPSIZE_X_PULLDOWN:
				SetDParam(0, 1 << _settings_newgame.game_creation.map_x);
				break;

			case CSCEN_MAPSIZE_Y_PULLDOWN:
				SetDParam(0, 1 << _settings_newgame.game_creation.map_y);
				break;

			case CSCEN_FLAT_LAND_HEIGHT_TEXT:
				SetDParam(0, _settings_newgame.game_creation.se_flat_world_height);
				break;
		}
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

		this->DrawWidgets();
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		StringID str = STR_JUST_INT;
		switch (widget) {
			case CSCEN_START_DATE_TEXT:
				SetDParam(0, ConvertYMDToDate(MAX_YEAR, 0, 1));
				str = STR_BLACK_DATE_LONG;
				break;

			case CSCEN_MAPSIZE_X_PULLDOWN:
			case CSCEN_MAPSIZE_Y_PULLDOWN:
				SetDParam(0, MAX_MAP_SIZE);
				break;

			case CSCEN_FLAT_LAND_HEIGHT_TEXT:
				SetDParam(0, MAX_TILE_HEIGHT);
				break;

			default:
				return;
		}
		*size = GetStringBoundingBox(str);
		size->width += padding.width;
		size->height += padding.height;
	}

	virtual void OnClick(Point pt, int widget, int click_count)
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
				StartGeneratingLandscape(GLWM_SCENARIO);
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
				ShowQueryString(STR_JUST_INT, STR_MAPGEN_START_DATE_QUERY_CAPT, 8, 100, this, CS_NUMERAL, QSF_NONE);
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
				ShowQueryString(STR_JUST_INT, STR_SE_MAPGEN_FLAT_WORLD_HEIGHT_QUERY_CAPT, 3, 100, this, CS_NUMERAL, QSF_NONE);
				break;
		}
	}

	virtual void OnTimeout()
	{
		static const int raise_widgets[] = {CSCEN_START_DATE_DOWN, CSCEN_START_DATE_UP, CSCEN_FLAT_LAND_HEIGHT_DOWN, CSCEN_FLAT_LAND_HEIGHT_UP, WIDGET_LIST_END};
		for (const int *widget = raise_widgets; *widget != WIDGET_LIST_END; widget++) {
			if (this->IsWidgetLowered(*widget)) {
				this->RaiseWidget(*widget);
				this->SetWidgetDirty(*widget);
			}
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
					this->SetWidgetDirty(CSCEN_START_DATE_TEXT);
					_settings_newgame.game_creation.starting_year = Clamp(value, MIN_YEAR, MAX_YEAR);
					break;

				case CSCEN_FLAT_LAND_HEIGHT_TEXT:
					this->SetWidgetDirty(CSCEN_FLAT_LAND_HEIGHT_TEXT);
					_settings_newgame.game_creation.se_flat_world_height = Clamp(value, 0, MAX_TILE_HEIGHT);
					break;
			}

			this->SetDirty();
		}
	}
};

static const NWidgetPart _nested_create_scenario_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_SE_MAPGEN_CAPTION, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(NWID_SPACER), SetMinimalSize(0, 10),
		/* Landscape style selection. */
		NWidget(NWID_HORIZONTAL), SetPIP(10, 3, 10),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, CSCEN_TEMPERATE), SetDataTip(SPR_SELECT_TEMPERATE, STR_INTRO_TOOLTIP_TEMPERATE),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, CSCEN_ARCTIC), SetDataTip(SPR_SELECT_SUB_ARCTIC, STR_INTRO_TOOLTIP_SUB_ARCTIC_LANDSCAPE),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, CSCEN_TROPICAL), SetDataTip(SPR_SELECT_SUB_TROPICAL, STR_INTRO_TOOLTIP_SUB_TROPICAL_LANDSCAPE),
			NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, CSCEN_TOYLAND), SetDataTip(SPR_SELECT_TOYLAND, STR_INTRO_TOOLTIP_TOYLAND_LANDSCAPE),
		EndContainer(),
		NWidget(NWID_HORIZONTAL), SetPIP(10, 8, 10),
			/* Green generation type buttons: 'Flat land' and 'Random land'. */
			NWidget(NWID_VERTICAL), SetPIP(10, 6, 10),
				NWidget(WWT_TEXTBTN, COLOUR_GREEN, CSCEN_EMPTY_WORLD), SetDataTip(STR_SE_MAPGEN_FLAT_WORLD, STR_SE_MAPGEN_FLAT_WORLD_TOOLTIP), SetFill(1, 1),
				NWidget(WWT_TEXTBTN, COLOUR_GREEN, CSCEN_RANDOM_WORLD), SetDataTip(STR_SE_MAPGEN_RANDOM_LAND, STR_TERRAFORM_TOOLTIP_GENERATE_RANDOM_LAND), SetFill(1, 1),
			EndContainer(),
			/* Labels + setting drop-downs */
			NWidget(NWID_VERTICAL), SetPIP(10, 6, 10),
				/* Map size. */
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_MAPSIZE, STR_NULL), SetPadding(1, 0, 0, 0),
					NWidget(NWID_SPACER), SetMinimalSize(6, 0), SetFill(1, 0),
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, CSCEN_MAPSIZE_X_PULLDOWN), SetDataTip(STR_JUST_INT, STR_NULL), SetPadding(0, 4, 0, 0),
					NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_BY, STR_NULL), SetPadding(1, 2, 0, 0),
					NWidget(WWT_DROPDOWN, COLOUR_ORANGE, CSCEN_MAPSIZE_Y_PULLDOWN), SetDataTip(STR_JUST_INT, STR_NULL),
				EndContainer(),
				/* Date. */
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_TEXT, COLOUR_ORANGE), SetDataTip(STR_MAPGEN_DATE, STR_NULL), SetPadding(1, 0, 0, 0),
					NWidget(NWID_SPACER), SetMinimalSize(6, 0), SetFill(1, 0),
					NWidget(WWT_IMGBTN, COLOUR_ORANGE, CSCEN_START_DATE_DOWN), SetFill(0, 1), SetDataTip(SPR_ARROW_DOWN, STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_BACKWARD),
					NWidget(WWT_TEXTBTN, COLOUR_ORANGE, CSCEN_START_DATE_TEXT), SetDataTip(STR_BLACK_DATE_LONG, STR_NULL),
					NWidget(WWT_IMGBTN, COLOUR_ORANGE, CSCEN_START_DATE_UP), SetFill(0, 1), SetDataTip(SPR_ARROW_UP, STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_FORWARD),
				EndContainer(),
				/* Flat map height. */
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_TEXT, COLOUR_ORANGE),
												SetDataTip(STR_SE_MAPGEN_FLAT_WORLD_HEIGHT, STR_NULL), SetPadding(1, 0, 0, 0),
					NWidget(NWID_SPACER), SetMinimalSize(6, 0), SetFill(1, 0),
					NWidget(WWT_IMGBTN, COLOUR_ORANGE, CSCEN_FLAT_LAND_HEIGHT_DOWN), SetFill(0, 1), SetDataTip(SPR_ARROW_DOWN, STR_SE_MAPGEN_FLAT_WORLD_HEIGHT_DOWN),
					NWidget(WWT_TEXTBTN, COLOUR_ORANGE, CSCEN_FLAT_LAND_HEIGHT_TEXT), SetDataTip(STR_BLACK_INT, STR_NULL),
					NWidget(WWT_IMGBTN, COLOUR_ORANGE, CSCEN_FLAT_LAND_HEIGHT_UP), SetFill(0, 1), SetDataTip(SPR_ARROW_UP, STR_SE_MAPGEN_FLAT_WORLD_HEIGHT_UP),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _create_scenario_desc(
	WDP_CENTER, 0, 0,
	WC_GENERATE_LANDSCAPE, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_create_scenario_widgets, lengthof(_nested_create_scenario_widgets)
);

void ShowCreateScenario()
{
	DeleteWindowByClass(WC_GENERATE_LANDSCAPE);
	new CreateScenarioWindow(&_create_scenario_desc, GLWM_SCENARIO);
}

enum GenerationProgressWindowWidgets {
	GPWW_PROGRESS_BAR,
	GPWW_PROGRESS_TEXT,
	GPWW_ABORT,
};

static const NWidgetPart _nested_generate_progress_widgets[] = {
	NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_GENERATION_WORLD, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_HORIZONTAL), SetPIP(20, 0, 20),
			NWidget(NWID_VERTICAL), SetPIP(11, 8, 11),
				NWidget(WWT_EMPTY, INVALID_COLOUR, GPWW_PROGRESS_BAR), SetFill(1, 0),
				NWidget(WWT_EMPTY, INVALID_COLOUR, GPWW_PROGRESS_TEXT), SetFill(1, 0),
				NWidget(WWT_TEXTBTN, COLOUR_WHITE, GPWW_ABORT), SetDataTip(STR_GENERATION_ABORT, STR_NULL), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};


static const WindowDesc _generate_progress_desc(
	WDP_CENTER, 0, 0,
	WC_GENERATE_PROGRESS_WINDOW, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_generate_progress_widgets, lengthof(_nested_generate_progress_widgets)
);

struct GenWorldStatus {
	uint percent;
	StringID cls;
	uint current;
	uint total;
	int timer;
};

static GenWorldStatus _gws;

static const StringID _generation_class_table[]  = {
	STR_GENERATION_WORLD_GENERATION,
	STR_SCENEDIT_TOOLBAR_LANDSCAPE_GENERATION,
	STR_GENERATION_CLEARING_TILES,
	STR_SCENEDIT_TOOLBAR_TOWN_GENERATION,
	STR_SCENEDIT_TOOLBAR_INDUSTRY_GENERATION,
	STR_GENERATION_OBJECT_GENERATION,
	STR_GENERATION_TREE_GENERATION,
	STR_GENERATION_SETTINGUP_GAME,
	STR_GENERATION_PREPARING_TILELOOP,
	STR_GENERATION_PREPARING_GAME
};
assert_compile(lengthof(_generation_class_table) == GWP_CLASS_COUNT);


static void AbortGeneratingWorldCallback(Window *w, bool confirmed)
{
	if (confirmed) {
		AbortGeneratingWorld();
	} else if (IsGeneratingWorld() && !IsGeneratingWorldAborted()) {
		SetMouseCursor(SPR_CURSOR_ZZZ, PAL_NONE);
	}
}

struct GenerateProgressWindow : public Window {

	GenerateProgressWindow() : Window()
	{
		this->InitNested(&_generate_progress_desc);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
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

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case GPWW_PROGRESS_BAR: {
				SetDParam(0, 100);
				*size = GetStringBoundingBox(STR_GENERATION_PROGRESS);
				/* We need some spacing for the 'border' */
				size->height += 8;
				size->width += 8;
				break;
			}

			case GPWW_PROGRESS_TEXT:
				for (uint i = 0; i < GWP_CLASS_COUNT; i++) {
					size->width = max(size->width, GetStringBoundingBox(_generation_class_table[i]).width);
				}
				size->height = FONT_HEIGHT_NORMAL * 2 + WD_PAR_VSEP_NORMAL;
				break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case GPWW_PROGRESS_BAR:
				/* Draw the % complete with a bar and a text */
				DrawFrameRect(r.left, r.top, r.right, r.bottom, COLOUR_GREY, FR_BORDERONLY);
				DrawFrameRect(r.left + 1, r.top + 1, (int)((r.right - r.left - 2) * _gws.percent / 100) + r.left + 1, r.bottom - 1, COLOUR_MAUVE, FR_NONE);
				SetDParam(0, _gws.percent);
				DrawString(r.left, r.right, r.top + 5, STR_GENERATION_PROGRESS, TC_FROMSTRING, SA_HOR_CENTER);
				break;

			case GPWW_PROGRESS_TEXT:
				/* Tell which class we are generating */
				DrawString(r.left, r.right, r.top, _gws.cls, TC_FROMSTRING, SA_HOR_CENTER);

				/* And say where we are in that class */
				SetDParam(0, _gws.current);
				SetDParam(1, _gws.total);
				DrawString(r.left, r.right, r.top + FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL, STR_GENERATION_PROGRESS_NUM, TC_FROMSTRING, SA_HOR_CENTER);
		}
	}
};

/**
 * Initializes the progress counters to the starting point.
 */
void PrepareGenerateWorldProgress()
{
	_gws.cls     = STR_GENERATION_WORLD_GENERATION;
	_gws.current = 0;
	_gws.total   = 0;
	_gws.percent = 0;
	_gws.timer   = 0; // Forces to paint the progress window immediately
}

/**
 * Show the window where a user can follow the process of the map generation.
 */
void ShowGenerateWorldProgress()
{
	if (BringWindowToFrontById(WC_GENERATE_PROGRESS_WINDOW, 0)) return;
	new GenerateProgressWindow();
}

static void _SetGeneratingWorldProgress(GenWorldProgress cls, uint progress, uint total)
{
	static const int percent_table[GWP_CLASS_COUNT + 1] = {0, 5, 15, 20, 40, 60, 65, 80, 85, 99, 100 };

	assert(cls < GWP_CLASS_COUNT);

	/* Do not run this function if we aren't in a thread */
	if (!IsGenerateWorldThreaded() && !_network_dedicated) return;

	if (IsGeneratingWorldAborted()) HandleGeneratingWorldAbortion();

	if (total == 0) {
		assert(_gws.cls == _generation_class_table[cls]);
		_gws.current += progress;
		assert(_gws.current <= _gws.total);
	} else {
		_gws.cls     = _generation_class_table[cls];
		_gws.current = progress;
		_gws.total   = total;
		_gws.percent = percent_table[cls];
	}

	/* Don't update the screen too often. So update it once in every once in a while... */
	if (!_network_dedicated && _gws.timer != 0 && _realtime_tick - _gws.timer < GENWORLD_REDRAW_TIMEOUT) return;

	/* Percentage is about the number of completed tasks, so 'current - 1' */
	_gws.percent = percent_table[cls] + (percent_table[cls + 1] - percent_table[cls]) * (_gws.current == 0 ? 0 : _gws.current - 1) / _gws.total;

	if (_network_dedicated) {
		static uint last_percent = 0;

		/* Never display 0% */
		if (_gws.percent == 0) return;
		/* Reset if percent is lower than the last recorded */
		if (_gws.percent < last_percent) last_percent = 0;
		/* Display every 5%, but 6% is also very valid.. just not smaller steps than 5% */
		if (_gws.percent % 5 != 0 && _gws.percent <= last_percent + 5) return;
		/* Never show steps smaller than 2%, even if it is a mod 5% */
		if (_gws.percent <= last_percent + 2) return;

		DEBUG(net, 1, "Map generation percentage complete: %d", _gws.percent);
		last_percent = _gws.percent;

		/* Don't continue as dedicated never has a thread running */
		return;
	}

	SetWindowDirty(WC_GENERATE_PROGRESS_WINDOW, 0);
	MarkWholeScreenDirty();

	/* Release the rights to the map generator, and acquire the rights to the
	 * paint thread. The 'other' thread already has the paint thread rights so
	 * this ensures us that we are waiting until the paint thread is done
	 * before we reacquire the mapgen rights */
	_genworld_mapgen_mutex->EndCritical();
	_genworld_paint_mutex->BeginCritical();
	_genworld_mapgen_mutex->BeginCritical();
	_genworld_paint_mutex->EndCritical();

	_gws.timer = _realtime_tick;
}

/**
 * Set the total of a stage of the world generation.
 * @param cls the current class we are in.
 * @param total Set the total expected items for this class.
 *
 * Warning: this function isn't clever. Don't go from class 4 to 3. Go upwards, always.
 *  Also, progress works if total is zero, total works if progress is zero.
 */
void SetGeneratingWorldProgress(GenWorldProgress cls, uint total)
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
void IncreaseGeneratingWorldProgress(GenWorldProgress cls)
{
	/* In fact the param 'class' isn't needed.. but for some security reasons, we want it around */
	_SetGeneratingWorldProgress(cls, 1, 0);
}
