/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file genworld_gui.cpp GUI to configure and show progress during map generation. */

#include "stdafx.h"
#include "heightmap.h"
#include "debug.h"
#include "genworld.h"
#include "network/network.h"
#include "strings_func.h"
#include "window_func.h"
#include "timer/timer_game_calendar.h"
#include "sound_func.h"
#include "fios.h"
#include "string_func.h"
#include "dropdown_type.h"
#include "dropdown_func.h"
#include "querystring_gui.h"
#include "town.h"
#include "core/geometry_func.hpp"
#include "core/random_func.hpp"
#include "saveload/saveload.h"
#include "progress.h"
#include "error.h"
#include "newgrf_townname.h"
#include "townname_type.h"
#include "video/video_driver.hpp"
#include "ai/ai_gui.hpp"
#include "game/game_gui.hpp"
#include "industry.h"
#include "core/string_consumer.hpp"

#include "widgets/genworld_widget.h"

#include "table/strings.h"

#include "dropdown_common_type.h"

#include "safeguards.h"


extern void MakeNewgameSettingsLive();

/** Enum for the modes we can generate in. */
enum GenerateLandscapeWindowMode : uint8_t {
	GLWM_GENERATE,  ///< Generate new game.
	GLWM_HEIGHTMAP, ///< Load from heightmap.
	GLWM_SCENARIO,  ///< Generate flat land.
};

/**
 * Get the map height limit, or if set to "auto", the absolute limit.
 */
static uint GetMapHeightLimit()
{
	if (_settings_newgame.construction.map_height_limit == 0) return MAX_MAP_HEIGHT_LIMIT;
	return _settings_newgame.construction.map_height_limit;
}

/**
 * Changes landscape type and sets genworld window dirty
 * @param landscape new landscape type
 */
void SetNewLandscapeType(LandscapeType landscape)
{
	_settings_newgame.game_creation.landscape = landscape;
	InvalidateWindowClassesData(WC_SELECT_GAME);
	InvalidateWindowClassesData(WC_GENERATE_LANDSCAPE);
}

/** Widgets of GenerateLandscapeWindow when generating world */
static constexpr std::initializer_list<NWidgetPart> _nested_generate_landscape_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_MAPGEN_WORLD_GENERATION_CAPTION),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.sparse),
			/* Landscape selection. */
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0), SetPIPRatio(1, 1, 1),
				NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, WID_GL_TEMPERATE), SetSpriteTip(SPR_SELECT_TEMPERATE, STR_INTRO_TOOLTIP_TEMPERATE),
				NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, WID_GL_ARCTIC), SetSpriteTip(SPR_SELECT_SUB_ARCTIC, STR_INTRO_TOOLTIP_SUB_ARCTIC_LANDSCAPE),
				NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, WID_GL_TROPICAL), SetSpriteTip(SPR_SELECT_SUB_TROPICAL, STR_INTRO_TOOLTIP_SUB_TROPICAL_LANDSCAPE),
				NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, WID_GL_TOYLAND), SetSpriteTip(SPR_SELECT_TOYLAND, STR_INTRO_TOOLTIP_TOYLAND_LANDSCAPE),
			EndContainer(),

			/* Generation options. */
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				/* Left half (land generation options) */
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
					/* Labels on the left side (global column 1). */
					NWidget(NWID_VERTICAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_MAPSIZE, STR_MAPGEN_MAPSIZE_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_TERRAIN_TYPE, STR_CONFIG_SETTING_TERRAIN_TYPE_HELPTEXT), SetFill(1, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_VARIETY, STR_CONFIG_SETTING_VARIETY_HELPTEXT), SetFill(1, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_SMOOTHNESS, STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN_HELPTEXT), SetFill(1, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_QUANTITY_OF_RIVERS, STR_CONFIG_SETTING_RIVER_AMOUNT_HELPTEXT), SetFill(1, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_BORDER_TYPE, STR_MAPGEN_BORDER_TYPE_TOOLTIP), SetFill(1, 1),
					EndContainer(),

					/* Widgets on the right side (global column 2). */
					NWidget(NWID_VERTICAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
						/* Mapsize X * Y. */
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_MAPSIZE_X_PULLDOWN), SetToolTip(STR_MAPGEN_MAPSIZE_TOOLTIP), SetFill(1, 1),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_BY), SetFill(0, 1), SetAlignment(SA_CENTER),
							NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_MAPSIZE_Y_PULLDOWN), SetToolTip(STR_MAPGEN_MAPSIZE_TOOLTIP), SetFill(1, 1),
						EndContainer(),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_TERRAIN_PULLDOWN), SetToolTip(STR_CONFIG_SETTING_TERRAIN_TYPE_HELPTEXT), SetFill(1, 1),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_VARIETY_PULLDOWN), SetToolTip(STR_CONFIG_SETTING_VARIETY_HELPTEXT), SetFill(1, 1),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_SMOOTHNESS_PULLDOWN), SetToolTip(STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN_HELPTEXT), SetFill(1, 1),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_RIVER_PULLDOWN), SetToolTip(STR_CONFIG_SETTING_RIVER_AMOUNT_HELPTEXT), SetFill(1, 1),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_BORDERS_PULLDOWN), SetToolTip(STR_MAPGEN_BORDER_TYPE_TOOLTIP), SetFill(1, 1),
					EndContainer(),
				EndContainer(),

				/* Right half (all other options) */
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
					/* Labels on the left side (global column 3). */
					NWidget(NWID_VERTICAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_GL_CLIMATE_SEL_LABEL),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_SNOW_COVERAGE, STR_CONFIG_SETTING_SNOW_COVERAGE_HELPTEXT), SetFill(1, 1),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_DESERT_COVERAGE, STR_CONFIG_SETTING_DESERT_COVERAGE_HELPTEXT), SetFill(1, 1),
							NWidget(NWID_SPACER), SetFill(1, 1),
						EndContainer(),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_DATE, STR_MAPGEN_DATE_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_TOWN_NAME_LABEL, STR_MAPGEN_TOWN_NAME_DROPDOWN_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_NUMBER_OF_TOWNS, STR_MAPGEN_NUMBER_OF_TOWNS_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_NUMBER_OF_INDUSTRIES, STR_MAPGEN_NUMBER_OF_INDUSTRIES_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_SEA_LEVEL, STR_MAPGEN_SEA_LEVEL_TOOLTIP), SetFill(1, 1),
					EndContainer(),

					/* Widgets on the right side (global column 4). */
					NWidget(NWID_VERTICAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
						/* Climate selector. */
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_GL_CLIMATE_SEL_SELECTOR),
							/* Snow coverage. */
							NWidget(NWID_HORIZONTAL),
								NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_GL_SNOW_COVERAGE_DOWN), SetSpriteTip(SPR_ARROW_DOWN, STR_MAPGEN_SNOW_COVERAGE_DOWN_TOOLTIP), SetFill(0, 1), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
								NWidget(WWT_TEXTBTN, COLOUR_ORANGE, WID_GL_SNOW_COVERAGE_TEXT), SetToolTip(STR_CONFIG_SETTING_SNOW_COVERAGE_HELPTEXT), SetFill(1, 1),
								NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_GL_SNOW_COVERAGE_UP), SetSpriteTip(SPR_ARROW_UP, STR_MAPGEN_SNOW_COVERAGE_UP_TOOLTIP), SetFill(0, 1), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
							EndContainer(),
							/* Desert coverage. */
							NWidget(NWID_HORIZONTAL),
								NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_GL_DESERT_COVERAGE_DOWN), SetSpriteTip(SPR_ARROW_DOWN, STR_MAPGEN_DESERT_COVERAGE_DOWN_TOOLTIP), SetFill(0, 1), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
								NWidget(WWT_TEXTBTN, COLOUR_ORANGE, WID_GL_DESERT_COVERAGE_TEXT), SetToolTip(STR_CONFIG_SETTING_DESERT_COVERAGE_HELPTEXT), SetFill(1, 1),
								NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_GL_DESERT_COVERAGE_UP), SetSpriteTip(SPR_ARROW_UP, STR_MAPGEN_DESERT_COVERAGE_UP_TOOLTIP), SetFill(0, 1), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
							EndContainer(),
							/* Temperate/Toyland spacer. */
							NWidget(NWID_SPACER), SetFill(1, 1),
						EndContainer(),
						/* Starting date. */
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_GL_START_DATE_DOWN), SetSpriteTip(SPR_ARROW_DOWN, STR_SCENEDIT_TOOLBAR_MOVE_THE_STARTING_DATE_BACKWARD_TOOLTIP), SetFill(0, 1), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
							NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_GL_START_DATE_TEXT), SetToolTip(STR_MAPGEN_DATE_TOOLTIP), SetFill(1, 1),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_GL_START_DATE_UP), SetSpriteTip(SPR_ARROW_UP, STR_SCENEDIT_TOOLBAR_MOVE_THE_STARTING_DATE_FORWARD_TOOLTIP), SetFill(0, 1), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
						EndContainer(),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_TOWNNAME_DROPDOWN), SetToolTip(STR_MAPGEN_TOWN_NAME_DROPDOWN_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_TOWN_PULLDOWN), SetToolTip(STR_MAPGEN_NUMBER_OF_TOWNS_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_INDUSTRY_PULLDOWN), SetToolTip(STR_MAPGEN_NUMBER_OF_INDUSTRIES_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_WATER_PULLDOWN), SetToolTip(STR_MAPGEN_SEA_LEVEL_TOOLTIP), SetFill(1, 1),
					EndContainer(),
				EndContainer(),
			EndContainer(),

			/* Map borders buttons for each edge. */
			NWidget(NWID_VERTICAL),
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
					NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_NORTHWEST), SetPadding(0, WidgetDimensions::unscaled.hsep_normal, 0, 0), SetFill(1, 1), SetAlignment(SA_RIGHT | SA_VERT_CENTER),
					NWidget(WWT_TEXTBTN, COLOUR_ORANGE, WID_GL_WATER_NW), SetToolTip(STR_MAPGEN_NORTHWEST_TOOLTIP), SetFill(1, 1),
					NWidget(WWT_TEXTBTN, COLOUR_ORANGE, WID_GL_WATER_NE), SetToolTip(STR_MAPGEN_NORTHEAST_TOOLTIP), SetFill(1, 1),
					NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_NORTHEAST), SetPadding(0, 0, 0, WidgetDimensions::unscaled.hsep_normal), SetFill(1, 1),
				EndContainer(),
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
					NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_SOUTHWEST), SetPadding(0, WidgetDimensions::unscaled.hsep_normal, 0, 0), SetFill(1, 1), SetAlignment(SA_RIGHT | SA_VERT_CENTER),
					NWidget(WWT_TEXTBTN, COLOUR_ORANGE, WID_GL_WATER_SW), SetToolTip(STR_MAPGEN_SOUTHWEST_TOOLTIP), SetFill(1, 1),
					NWidget(WWT_TEXTBTN, COLOUR_ORANGE, WID_GL_WATER_SE), SetToolTip(STR_MAPGEN_SOUTHEAST_TOOLTIP), SetFill(1, 1),
					NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_SOUTHEAST), SetPadding(0, 0, 0, WidgetDimensions::unscaled.hsep_normal), SetFill(1, 1),
				EndContainer(),
			EndContainer(),

			/* AI, GS, and NewGRF settings */
			NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_GL_AI_BUTTON), SetMinimalTextLines(2, 0), SetStringTip(STR_MAPGEN_AI_SETTINGS, STR_MAPGEN_AI_SETTINGS_TOOLTIP), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_GL_GS_BUTTON), SetMinimalTextLines(2, 0), SetStringTip(STR_MAPGEN_GS_SETTINGS, STR_MAPGEN_GS_SETTINGS_TOOLTIP), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_GL_NEWGRF_BUTTON), SetMinimalTextLines(2, 0), SetStringTip(STR_MAPGEN_NEWGRF_SETTINGS, STR_MAPGEN_NEWGRF_SETTINGS_TOOLTIP), SetFill(1, 0),
			EndContainer(),

			/* Generate */
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_GL_GENERATE_BUTTON), SetMinimalTextLines(3, 0), SetStringTip(STR_MAPGEN_GENERATE, STR_MAPGEN_GENERATE_TOOLTIP), SetFill(1, 1),
		EndContainer(),
	EndContainer(),
};

/** Widgets of GenerateLandscapeWindow when loading heightmap */
static constexpr std::initializer_list<NWidgetPart> _nested_heightmap_load_widgets = {
	/* Window header. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_MAPGEN_WORLD_GENERATION_CAPTION),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.sparse),
			/* Landscape selection. */
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0), SetPIPRatio(1, 1, 1),
				NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, WID_GL_TEMPERATE), SetSpriteTip(SPR_SELECT_TEMPERATE, STR_INTRO_TOOLTIP_TEMPERATE),
				NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, WID_GL_ARCTIC), SetSpriteTip(SPR_SELECT_SUB_ARCTIC, STR_INTRO_TOOLTIP_SUB_ARCTIC_LANDSCAPE),
				NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, WID_GL_TROPICAL), SetSpriteTip(SPR_SELECT_SUB_TROPICAL, STR_INTRO_TOOLTIP_SUB_TROPICAL_LANDSCAPE),
				NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, WID_GL_TOYLAND), SetSpriteTip(SPR_SELECT_TOYLAND, STR_INTRO_TOOLTIP_TOYLAND_LANDSCAPE),
			EndContainer(),

			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				/* Heightmap name label. */
				NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_HEIGHTMAP_NAME, STR_MAPGEN_HEIGHTMAP_NAME_TOOLTIP),
				NWidget(WWT_TEXT, INVALID_COLOUR, WID_GL_HEIGHTMAP_NAME_TEXT), SetTextStyle(TC_ORANGE), SetToolTip(STR_MAPGEN_HEIGHTMAP_NAME_TOOLTIP), SetFill(1, 0),
			EndContainer(),

			/* Generation options. */
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				/* Left half (land generation options) */
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
					/* Labels on the left side (global column 1). */
					NWidget(NWID_VERTICAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
						/* Land generation option labels. */
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_HEIGHTMAP_SIZE_LABEL, STR_MAPGEN_HEIGHTMAP_SIZE_LABEL_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_MAPSIZE, STR_MAPGEN_MAPSIZE_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_HEIGHTMAP_ROTATION, STR_CONFIG_SETTING_HEIGHTMAP_ROTATION_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_HEIGHTMAP_HEIGHT, STR_MAPGEN_HEIGHTMAP_HEIGHT_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_QUANTITY_OF_RIVERS, STR_CONFIG_SETTING_RIVER_AMOUNT_HELPTEXT), SetFill(1, 1),
					EndContainer(),

					/* Left half widgets (global column 2) */
					NWidget(NWID_VERTICAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
						NWidget(WWT_TEXT, INVALID_COLOUR, WID_GL_HEIGHTMAP_SIZE_TEXT), SetStringTip(STR_MAPGEN_HEIGHTMAP_SIZE, STR_MAPGEN_HEIGHTMAP_SIZE_LABEL_TOOLTIP), SetFill(1, 1),
						/* Mapsize X * Y. */
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_MAPSIZE_X_PULLDOWN), SetToolTip(STR_MAPGEN_MAPSIZE_TOOLTIP), SetFill(1, 1),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_BY), SetFill(0, 1), SetAlignment(SA_CENTER),
							NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_MAPSIZE_Y_PULLDOWN), SetToolTip(STR_MAPGEN_MAPSIZE_TOOLTIP), SetFill(1, 1),
						EndContainer(),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_HEIGHTMAP_ROTATION_PULLDOWN), SetToolTip(STR_CONFIG_SETTING_HEIGHTMAP_ROTATION_TOOLTIP), SetFill(1, 1),
						/* Heightmap highest peak. */
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_GL_HEIGHTMAP_HEIGHT_DOWN), SetSpriteTip(SPR_ARROW_DOWN, STR_MAPGEN_HEIGHTMAP_HEIGHT_DOWN_TOOLTIP), SetFill(0, 1), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
							NWidget(WWT_TEXTBTN, COLOUR_ORANGE, WID_GL_HEIGHTMAP_HEIGHT_TEXT), SetToolTip(STR_MAPGEN_HEIGHTMAP_HEIGHT_TOOLTIP), SetFill(1, 1),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_GL_HEIGHTMAP_HEIGHT_UP), SetSpriteTip(SPR_ARROW_UP, STR_MAPGEN_HEIGHTMAP_HEIGHT_UP_TOOLTIP), SetFill(0, 1), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
						EndContainer(),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_RIVER_PULLDOWN), SetToolTip(STR_CONFIG_SETTING_RIVER_AMOUNT_HELPTEXT), SetFill(1, 1),
					EndContainer(),
				EndContainer(),

				/* Right half (all other options) */
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
					/* Right half labels (global column 3) */
					NWidget(NWID_VERTICAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_GL_CLIMATE_SEL_LABEL),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_SNOW_COVERAGE, STR_CONFIG_SETTING_SNOW_COVERAGE_HELPTEXT), SetFill(1, 1),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_DESERT_COVERAGE, STR_CONFIG_SETTING_DESERT_COVERAGE_HELPTEXT), SetFill(1, 1),
							NWidget(NWID_SPACER), SetFill(1, 1),
						EndContainer(),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_DATE, STR_MAPGEN_DATE_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_TOWN_NAME_LABEL, STR_MAPGEN_TOWN_NAME_DROPDOWN_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_NUMBER_OF_TOWNS, STR_MAPGEN_NUMBER_OF_TOWNS_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_NUMBER_OF_INDUSTRIES, STR_MAPGEN_NUMBER_OF_INDUSTRIES_TOOLTIP), SetFill(1, 1),
					EndContainer(),

					/* Right half widgets (global column 4) */
					NWidget(NWID_VERTICAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
						/* Climate selector. */
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_GL_CLIMATE_SEL_SELECTOR),
							/* Snow coverage. */
							NWidget(NWID_HORIZONTAL),
								NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_GL_SNOW_COVERAGE_DOWN), SetSpriteTip(SPR_ARROW_DOWN, STR_MAPGEN_SNOW_COVERAGE_DOWN_TOOLTIP), SetFill(0, 1), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
								NWidget(WWT_TEXTBTN, COLOUR_ORANGE, WID_GL_SNOW_COVERAGE_TEXT), SetToolTip(STR_CONFIG_SETTING_SNOW_COVERAGE_HELPTEXT), SetFill(1, 1),
								NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_GL_SNOW_COVERAGE_UP), SetSpriteTip(SPR_ARROW_UP, STR_MAPGEN_SNOW_COVERAGE_UP_TOOLTIP), SetFill(0, 1), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
							EndContainer(),
							/* Desert coverage. */
							NWidget(NWID_HORIZONTAL),
								NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_GL_DESERT_COVERAGE_DOWN), SetSpriteTip(SPR_ARROW_DOWN, STR_MAPGEN_DESERT_COVERAGE_DOWN_TOOLTIP), SetFill(0, 1), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
								NWidget(WWT_TEXTBTN, COLOUR_ORANGE, WID_GL_DESERT_COVERAGE_TEXT), SetToolTip(STR_CONFIG_SETTING_DESERT_COVERAGE_HELPTEXT), SetFill(1, 1),
								NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_GL_DESERT_COVERAGE_UP), SetSpriteTip(SPR_ARROW_UP, STR_MAPGEN_DESERT_COVERAGE_UP_TOOLTIP), SetFill(0, 1), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
							EndContainer(),
							/* Temperate/Toyland spacer. */
							NWidget(NWID_SPACER), SetFill(1, 1),
						EndContainer(),
						/* Starting date. */
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_GL_START_DATE_DOWN), SetSpriteTip(SPR_ARROW_DOWN, STR_SCENEDIT_TOOLBAR_MOVE_THE_STARTING_DATE_BACKWARD_TOOLTIP), SetFill(0, 1), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
							NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_GL_START_DATE_TEXT), SetToolTip(STR_MAPGEN_DATE_TOOLTIP), SetFill(1, 1),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_GL_START_DATE_UP), SetSpriteTip(SPR_ARROW_UP, STR_SCENEDIT_TOOLBAR_MOVE_THE_STARTING_DATE_FORWARD_TOOLTIP), SetFill(0, 1), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
						EndContainer(),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_TOWNNAME_DROPDOWN), SetToolTip(STR_MAPGEN_TOWN_NAME_DROPDOWN_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_TOWN_PULLDOWN), SetToolTip(STR_MAPGEN_NUMBER_OF_TOWNS_TOOLTIP), SetFill(1, 1),
						NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_GL_INDUSTRY_PULLDOWN), SetToolTip(STR_MAPGEN_NUMBER_OF_INDUSTRIES_TOOLTIP), SetFill(1, 1),
					EndContainer(),
				EndContainer(),
			EndContainer(),

			/* AI, GS, and NewGRF settings */
			NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_GL_AI_BUTTON), SetMinimalTextLines(2, 0), SetStringTip(STR_MAPGEN_AI_SETTINGS, STR_MAPGEN_AI_SETTINGS_TOOLTIP), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_GL_GS_BUTTON), SetMinimalTextLines(2, 0), SetStringTip(STR_MAPGEN_GS_SETTINGS, STR_MAPGEN_GS_SETTINGS_TOOLTIP),  SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_GL_NEWGRF_BUTTON), SetMinimalTextLines(2, 0), SetStringTip(STR_MAPGEN_NEWGRF_SETTINGS, STR_MAPGEN_NEWGRF_SETTINGS_TOOLTIP), SetFill(1, 0),
			EndContainer(),

			/* Generate */
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_GL_GENERATE_BUTTON), SetMinimalTextLines(3, 0), SetStringTip(STR_MAPGEN_GENERATE, STR_MAPGEN_GENERATE_TOOLTIP), SetFill(1, 1),
		EndContainer(),
	EndContainer(),
};

static void StartGeneratingLandscape(GenerateLandscapeWindowMode mode)
{
	CloseAllNonVitalWindows();
	ClearErrorMessages();

	/* Copy all XXX_newgame to XXX when coming from outside the editor */
	MakeNewgameSettingsLive();
	ResetGRFConfig(true);

	SndConfirmBeep();
	switch (mode) {
		case GLWM_GENERATE:  _switch_mode = (_game_mode == GM_EDITOR) ? SM_GENRANDLAND    : SM_NEWGAME;         break;
		case GLWM_HEIGHTMAP: _switch_mode = (_game_mode == GM_EDITOR) ? SM_LOAD_HEIGHTMAP : SM_START_HEIGHTMAP; break;
		case GLWM_SCENARIO:  _switch_mode = SM_EDITOR; break;
		default: NOT_REACHED();
	}
}

static void LandscapeGenerationCallback(Window *w, bool confirmed)
{
	if (confirmed) StartGeneratingLandscape(w->window_number);
}

static DropDownList BuildMapsizeDropDown()
{
	DropDownList list;

	for (uint i = MIN_MAP_SIZE_BITS; i <= MAX_MAP_SIZE_BITS; i++) {
		list.push_back(MakeDropDownListStringItem(GetString(STR_JUST_INT, 1ULL << i), i));
	}

	return list;
}

static DropDownList BuildTownNameDropDown()
{
	DropDownList list;

	/* Add and sort newgrf townnames generators */
	const auto &grf_names = GetGRFTownNameList();
	for (uint i = 0; i < grf_names.size(); i++) {
		list.push_back(MakeDropDownListStringItem(grf_names[i], BUILTIN_TOWNNAME_GENERATOR_COUNT + i));
	}
	std::sort(list.begin(), list.end(), DropDownListStringItem::NatSortFunc);

	size_t newgrf_size = list.size();
	/* Insert newgrf_names at the top of the list */
	if (newgrf_size > 0) {
		list.push_back(MakeDropDownListDividerItem()); // separator line
		newgrf_size++;
	}

	/* Add and sort original townnames generators */
	for (uint i = 0; i < BUILTIN_TOWNNAME_GENERATOR_COUNT; i++) {
		list.push_back(MakeDropDownListStringItem(STR_MAPGEN_TOWN_NAME_ORIGINAL_ENGLISH + i, i));
	}
	std::sort(list.begin() + newgrf_size, list.end(), DropDownListStringItem::NatSortFunc);

	return list;
}


static const StringID _elevations[]  = {STR_TERRAIN_TYPE_VERY_FLAT, STR_TERRAIN_TYPE_FLAT, STR_TERRAIN_TYPE_HILLY, STR_TERRAIN_TYPE_MOUNTAINOUS, STR_TERRAIN_TYPE_ALPINIST, STR_TERRAIN_TYPE_CUSTOM};
static const StringID _sea_lakes[]   = {STR_SEA_LEVEL_VERY_LOW, STR_SEA_LEVEL_LOW, STR_SEA_LEVEL_MEDIUM, STR_SEA_LEVEL_HIGH, STR_SEA_LEVEL_CUSTOM};
static const StringID _rivers[]      = {STR_RIVERS_NONE, STR_RIVERS_FEW, STR_RIVERS_MODERATE, STR_RIVERS_LOT};
static const StringID _borders[]     = {STR_MAPGEN_BORDER_RANDOMIZE, STR_MAPGEN_BORDER_MANUAL, STR_MAPGEN_BORDER_INFINITE_WATER};
static const StringID _smoothness[]  = {STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN_VERY_SMOOTH, STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN_SMOOTH, STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN_ROUGH, STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN_VERY_ROUGH};
static const StringID _rotation[]    = {STR_CONFIG_SETTING_HEIGHTMAP_ROTATION_COUNTER_CLOCKWISE, STR_CONFIG_SETTING_HEIGHTMAP_ROTATION_CLOCKWISE};
static const StringID _num_towns[]   = {STR_NUM_VERY_LOW, STR_NUM_LOW, STR_NUM_NORMAL, STR_NUM_HIGH, STR_NUM_CUSTOM};
static const StringID _num_inds[]    = {STR_FUNDING_ONLY, STR_MINIMAL, STR_NUM_VERY_LOW, STR_NUM_LOW, STR_NUM_NORMAL, STR_NUM_HIGH, STR_NUM_CUSTOM};
static const StringID _variety[]     = {STR_VARIETY_NONE, STR_VARIETY_VERY_LOW, STR_VARIETY_LOW, STR_VARIETY_MEDIUM, STR_VARIETY_HIGH, STR_VARIETY_VERY_HIGH};

static_assert(std::size(_num_inds) == ID_END);

struct GenerateLandscapeWindow : public Window {
	WidgetID widget_id{};
	uint x = 0;
	uint y = 0;
	EncodedString name{};
	GenerateLandscapeWindowMode mode{};

	GenerateLandscapeWindow(WindowDesc &desc, WindowNumber number = 0) : Window(desc)
	{
		this->InitNested(number);

		this->LowerWidget(to_underlying(_settings_newgame.game_creation.landscape) + WID_GL_TEMPERATE);

		this->mode = this->window_number;

		/* Disable town and industry in SE */
		this->SetWidgetDisabledState(WID_GL_TOWN_PULLDOWN,     _game_mode == GM_EDITOR);
		this->SetWidgetDisabledState(WID_GL_INDUSTRY_PULLDOWN, _game_mode == GM_EDITOR);

		/* In case the map_height_limit is changed, clamp heightmap_height and custom_terrain_type. */
		_settings_newgame.game_creation.heightmap_height = Clamp(_settings_newgame.game_creation.heightmap_height, MIN_HEIGHTMAP_HEIGHT, GetMapHeightLimit());
		_settings_newgame.game_creation.custom_terrain_type = Clamp(_settings_newgame.game_creation.custom_terrain_type, MIN_CUSTOM_TERRAIN_TYPE, GetMapHeightLimit());

		/* If original landgenerator is selected and alpinist terrain_type was selected, revert to mountainous. */
		if (_settings_newgame.game_creation.land_generator == LG_ORIGINAL) {
			_settings_newgame.difficulty.terrain_type = Clamp(_settings_newgame.difficulty.terrain_type, 0, 3);
		}

		this->OnInvalidateData();
	}


	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_GL_START_DATE_TEXT:      return GetString(STR_JUST_DATE_LONG, TimerGameCalendar::ConvertYMDToDate(_settings_newgame.game_creation.starting_year, 0, 1));
			case WID_GL_MAPSIZE_X_PULLDOWN:   return GetString(STR_JUST_INT, 1LL << _settings_newgame.game_creation.map_x);
			case WID_GL_MAPSIZE_Y_PULLDOWN:   return GetString(STR_JUST_INT, 1LL << _settings_newgame.game_creation.map_y);
			case WID_GL_HEIGHTMAP_HEIGHT_TEXT: return GetString(STR_JUST_INT, _settings_newgame.game_creation.heightmap_height);
			case WID_GL_SNOW_COVERAGE_TEXT:   return GetString(STR_MAPGEN_SNOW_COVERAGE_TEXT, _settings_newgame.game_creation.snow_coverage);
			case WID_GL_DESERT_COVERAGE_TEXT: return GetString(STR_MAPGEN_DESERT_COVERAGE_TEXT, _settings_newgame.game_creation.desert_coverage);

			case WID_GL_TOWN_PULLDOWN:
				if (_game_mode == GM_EDITOR) {
					return GetString(STR_CONFIG_SETTING_OFF);
				}
				if (_settings_newgame.difficulty.number_towns == CUSTOM_TOWN_NUMBER_DIFFICULTY) {
					return GetString(STR_NUM_CUSTOM_NUMBER, _settings_newgame.game_creation.custom_town_number);
				}
				return GetString(_num_towns[_settings_newgame.difficulty.number_towns]);

			case WID_GL_TOWNNAME_DROPDOWN: {
				uint gen = _settings_newgame.game_creation.town_name;
				StringID name = gen < BUILTIN_TOWNNAME_GENERATOR_COUNT ?
						STR_MAPGEN_TOWN_NAME_ORIGINAL_ENGLISH + gen :
						GetGRFTownNameName(gen - BUILTIN_TOWNNAME_GENERATOR_COUNT);
				return GetString(name);
			}

			case WID_GL_INDUSTRY_PULLDOWN:
				if (_game_mode == GM_EDITOR) {
					return GetString(STR_CONFIG_SETTING_OFF);
				}
				if (_settings_newgame.difficulty.industry_density == ID_CUSTOM) {
					return GetString(STR_NUM_CUSTOM_NUMBER, _settings_newgame.game_creation.custom_industry_number);
				}
				return GetString(_num_inds[_settings_newgame.difficulty.industry_density]);

			case WID_GL_TERRAIN_PULLDOWN:
				if (_settings_newgame.difficulty.terrain_type == CUSTOM_TERRAIN_TYPE_NUMBER_DIFFICULTY) {
					return GetString(STR_TERRAIN_TYPE_CUSTOM_VALUE, _settings_newgame.game_creation.custom_terrain_type);
				}
				return GetString(_elevations[_settings_newgame.difficulty.terrain_type]);

			case WID_GL_WATER_PULLDOWN:
				if (_settings_newgame.difficulty.quantity_sea_lakes == CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY) {
					return GetString(STR_SEA_LEVEL_CUSTOM_PERCENTAGE, _settings_newgame.game_creation.custom_sea_level);
				}
				return GetString(_sea_lakes[_settings_newgame.difficulty.quantity_sea_lakes]);

			case WID_GL_HEIGHTMAP_NAME_TEXT: return this->name.GetDecodedString();
			case WID_GL_RIVER_PULLDOWN:      return GetString(_rivers[_settings_newgame.game_creation.amount_of_rivers]);
			case WID_GL_SMOOTHNESS_PULLDOWN: return GetString(_smoothness[_settings_newgame.game_creation.tgen_smoothness]);
			case WID_GL_VARIETY_PULLDOWN:    return GetString(_variety[_settings_newgame.game_creation.variety]);
			case WID_GL_BORDERS_PULLDOWN: return GetString(_borders[_settings_newgame.game_creation.water_border_presets]);
			case WID_GL_WATER_NE: return GetString((_settings_newgame.game_creation.water_borders == BorderFlag::Random) ? STR_MAPGEN_BORDER_RANDOM : _settings_newgame.game_creation.water_borders.Test(BorderFlag::NorthEast) ? STR_MAPGEN_BORDER_WATER : STR_MAPGEN_BORDER_FREEFORM);
			case WID_GL_WATER_NW: return GetString((_settings_newgame.game_creation.water_borders == BorderFlag::Random) ? STR_MAPGEN_BORDER_RANDOM : _settings_newgame.game_creation.water_borders.Test(BorderFlag::NorthWest) ? STR_MAPGEN_BORDER_WATER : STR_MAPGEN_BORDER_FREEFORM);
			case WID_GL_WATER_SE: return GetString((_settings_newgame.game_creation.water_borders == BorderFlag::Random) ? STR_MAPGEN_BORDER_RANDOM : _settings_newgame.game_creation.water_borders.Test(BorderFlag::SouthEast) ? STR_MAPGEN_BORDER_WATER : STR_MAPGEN_BORDER_FREEFORM);
			case WID_GL_WATER_SW: return GetString((_settings_newgame.game_creation.water_borders == BorderFlag::Random) ? STR_MAPGEN_BORDER_RANDOM : _settings_newgame.game_creation.water_borders.Test(BorderFlag::SouthWest) ? STR_MAPGEN_BORDER_WATER : STR_MAPGEN_BORDER_FREEFORM);
			case WID_GL_HEIGHTMAP_ROTATION_PULLDOWN: return GetString(_rotation[_settings_newgame.game_creation.heightmap_rotation]);

			case WID_GL_HEIGHTMAP_SIZE_TEXT:
				if (_settings_newgame.game_creation.heightmap_rotation == HM_CLOCKWISE) {
					return GetString(STR_MAPGEN_HEIGHTMAP_SIZE, this->y, this->x);
				}
				return GetString(STR_MAPGEN_HEIGHTMAP_SIZE, this->x, this->y);

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		/* Update the climate buttons */
		this->SetWidgetLoweredState(WID_GL_TEMPERATE, _settings_newgame.game_creation.landscape == LandscapeType::Temperate);
		this->SetWidgetLoweredState(WID_GL_ARCTIC,    _settings_newgame.game_creation.landscape == LandscapeType::Arctic);
		this->SetWidgetLoweredState(WID_GL_TROPICAL,  _settings_newgame.game_creation.landscape == LandscapeType::Tropic);
		this->SetWidgetLoweredState(WID_GL_TOYLAND,   _settings_newgame.game_creation.landscape == LandscapeType::Toyland);

		/* You can't select smoothness / non-water borders if not terragenesis */
		if (mode == GLWM_GENERATE) {
			this->SetWidgetDisabledState(WID_GL_SMOOTHNESS_PULLDOWN, _settings_newgame.game_creation.land_generator == LG_ORIGINAL);
			this->SetWidgetDisabledState(WID_GL_VARIETY_PULLDOWN, _settings_newgame.game_creation.land_generator == LG_ORIGINAL);
			this->SetWidgetDisabledState(WID_GL_BORDERS_PULLDOWN, _settings_newgame.game_creation.land_generator == LG_ORIGINAL);
			this->SetWidgetsDisabledState(_settings_newgame.game_creation.land_generator == LG_ORIGINAL || !_settings_newgame.construction.freeform_edges || _settings_newgame.game_creation.water_borders == BorderFlag::Random,
					WID_GL_WATER_NW, WID_GL_WATER_NE, WID_GL_WATER_SE, WID_GL_WATER_SW);

			this->SetWidgetLoweredState(WID_GL_WATER_NW, _settings_newgame.game_creation.water_borders.Test(BorderFlag::NorthWest));
			this->SetWidgetLoweredState(WID_GL_WATER_NE, _settings_newgame.game_creation.water_borders.Test(BorderFlag::NorthEast));
			this->SetWidgetLoweredState(WID_GL_WATER_SE, _settings_newgame.game_creation.water_borders.Test(BorderFlag::SouthEast));
			this->SetWidgetLoweredState(WID_GL_WATER_SW, _settings_newgame.game_creation.water_borders.Test(BorderFlag::SouthWest));

			this->SetWidgetsDisabledState(_settings_newgame.game_creation.land_generator == LG_ORIGINAL && (_settings_newgame.game_creation.landscape == LandscapeType::Arctic || _settings_newgame.game_creation.landscape == LandscapeType::Tropic),
					WID_GL_TERRAIN_PULLDOWN, WID_GL_WATER_PULLDOWN);
		}

		/* Disable snowline if not arctic */
		this->SetWidgetDisabledState(WID_GL_SNOW_COVERAGE_TEXT, _settings_newgame.game_creation.landscape != LandscapeType::Arctic);
		/* Disable desert if not tropic */
		this->SetWidgetDisabledState(WID_GL_DESERT_COVERAGE_TEXT, _settings_newgame.game_creation.landscape != LandscapeType::Tropic);

		/* Set snow/rainforest selections */
		int climate_plane = 0;
		switch (_settings_newgame.game_creation.landscape) {
			case LandscapeType::Temperate: climate_plane = 2; break;
			case LandscapeType::Arctic:    climate_plane = 0; break;
			case LandscapeType::Tropic:    climate_plane = 1; break;
			case LandscapeType::Toyland:   climate_plane = 2; break;
		}
		this->GetWidget<NWidgetStacked>(WID_GL_CLIMATE_SEL_LABEL)->SetDisplayedPlane(climate_plane);
		this->GetWidget<NWidgetStacked>(WID_GL_CLIMATE_SEL_SELECTOR)->SetDisplayedPlane(climate_plane);

		/* Update availability of decreasing / increasing start date and snow level */
		if (mode == GLWM_HEIGHTMAP) {
			this->SetWidgetDisabledState(WID_GL_HEIGHTMAP_HEIGHT_DOWN, _settings_newgame.game_creation.heightmap_height <= MIN_HEIGHTMAP_HEIGHT);
			this->SetWidgetDisabledState(WID_GL_HEIGHTMAP_HEIGHT_UP, _settings_newgame.game_creation.heightmap_height >= GetMapHeightLimit());
		}
		this->SetWidgetDisabledState(WID_GL_START_DATE_DOWN, _settings_newgame.game_creation.starting_year <= CalendarTime::MIN_YEAR);
		this->SetWidgetDisabledState(WID_GL_START_DATE_UP,   _settings_newgame.game_creation.starting_year >= CalendarTime::MAX_YEAR);
		this->SetWidgetDisabledState(WID_GL_SNOW_COVERAGE_DOWN, _settings_newgame.game_creation.snow_coverage <= 0 || _settings_newgame.game_creation.landscape != LandscapeType::Arctic);
		this->SetWidgetDisabledState(WID_GL_SNOW_COVERAGE_UP,   _settings_newgame.game_creation.snow_coverage >= 100 || _settings_newgame.game_creation.landscape != LandscapeType::Arctic);
		this->SetWidgetDisabledState(WID_GL_DESERT_COVERAGE_DOWN, _settings_newgame.game_creation.desert_coverage <= 0 || _settings_newgame.game_creation.landscape != LandscapeType::Tropic);
		this->SetWidgetDisabledState(WID_GL_DESERT_COVERAGE_UP,   _settings_newgame.game_creation.desert_coverage >= 100 || _settings_newgame.game_creation.landscape != LandscapeType::Tropic);

		/* Do not allow a custom sea level or terrain type with the original land generator. */
		if (_settings_newgame.game_creation.land_generator == LG_ORIGINAL) {
			if (_settings_newgame.difficulty.quantity_sea_lakes == CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY) {
				_settings_newgame.difficulty.quantity_sea_lakes = 1;
			}
			if (_settings_newgame.difficulty.terrain_type == CUSTOM_TERRAIN_TYPE_NUMBER_DIFFICULTY) {
				_settings_newgame.difficulty.terrain_type = 1;
			}
		}

	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		Dimension d{0, (uint)GetCharacterHeight(FS_NORMAL)};
		std::span<const StringID> strs;
		switch (widget) {
			case WID_GL_TEMPERATE: case WID_GL_ARCTIC:
			case WID_GL_TROPICAL: case WID_GL_TOYLAND:
				size.width += WidgetDimensions::scaled.fullbevel.Horizontal();
				size.height += WidgetDimensions::scaled.fullbevel.Vertical();
				break;

			case WID_GL_HEIGHTMAP_HEIGHT_TEXT:
				d = GetStringBoundingBox(GetString(STR_JUST_INT, MAX_TILE_HEIGHT));
				break;

			case WID_GL_START_DATE_TEXT:
				d = GetStringBoundingBox(GetString(STR_JUST_DATE_LONG, TimerGameCalendar::ConvertYMDToDate(CalendarTime::MAX_YEAR, 0, 1)));
				break;

			case WID_GL_MAPSIZE_X_PULLDOWN:
			case WID_GL_MAPSIZE_Y_PULLDOWN:
				d = GetStringBoundingBox(GetString(STR_JUST_INT, GetParamMaxValue(MAX_MAP_SIZE)));
				break;

			case WID_GL_SNOW_COVERAGE_TEXT:
				d = GetStringBoundingBox(GetString(STR_MAPGEN_SNOW_COVERAGE_TEXT, GetParamMaxValue(MAX_TILE_HEIGHT)));
				break;

			case WID_GL_DESERT_COVERAGE_TEXT:
				d = GetStringBoundingBox(GetString(STR_MAPGEN_DESERT_COVERAGE_TEXT, GetParamMaxValue(MAX_TILE_HEIGHT)));
				break;

			case WID_GL_HEIGHTMAP_SIZE_TEXT:
				d = GetStringBoundingBox(GetString(STR_MAPGEN_HEIGHTMAP_SIZE, this->x, this->y));
				break;

			case WID_GL_TOWN_PULLDOWN:
				strs = _num_towns;
				d = GetStringBoundingBox(GetString(STR_NUM_CUSTOM_NUMBER, GetParamMaxValue(CUSTOM_TOWN_MAX_NUMBER)));
				break;

			case WID_GL_INDUSTRY_PULLDOWN:
				strs = _num_inds;
				d = GetStringBoundingBox(GetString(STR_NUM_CUSTOM_NUMBER, GetParamMaxValue(IndustryPool::MAX_SIZE)));
				break;

			case WID_GL_TERRAIN_PULLDOWN:
				strs = _elevations;
				d = GetStringBoundingBox(GetString(STR_TERRAIN_TYPE_CUSTOM_VALUE, GetParamMaxValue(MAX_MAP_HEIGHT_LIMIT)));
				break;

			case WID_GL_WATER_PULLDOWN:
				strs = _sea_lakes;
				d = GetStringBoundingBox(GetString(STR_SEA_LEVEL_CUSTOM_PERCENTAGE, GetParamMaxValue(CUSTOM_SEA_LEVEL_MAX_PERCENTAGE)));
				break;

			case WID_GL_RIVER_PULLDOWN:      strs = _rivers; break;
			case WID_GL_SMOOTHNESS_PULLDOWN: strs = _smoothness; break;
			case WID_GL_VARIETY_PULLDOWN:    strs = _variety; break;
			case WID_GL_HEIGHTMAP_ROTATION_PULLDOWN: strs = _rotation; break;
			case WID_GL_BORDERS_PULLDOWN:    strs = _borders; break;
			case WID_GL_WATER_NE:
			case WID_GL_WATER_NW:
			case WID_GL_WATER_SE:
			case WID_GL_WATER_SW:
				d = maxdim(GetStringBoundingBox(STR_MAPGEN_BORDER_RANDOM), maxdim(GetStringBoundingBox(STR_MAPGEN_BORDER_WATER), GetStringBoundingBox(STR_MAPGEN_BORDER_FREEFORM)));
				break;

			case WID_GL_HEIGHTMAP_NAME_TEXT:
				size.width = 0;
				break;

			default:
				return;
		}
		d = maxdim(d, GetStringListBoundingBox(strs));
		d.width += padding.width;
		d.height += padding.height;
		size = maxdim(size, d);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_GL_TEMPERATE:
			case WID_GL_ARCTIC:
			case WID_GL_TROPICAL:
			case WID_GL_TOYLAND:
				SetNewLandscapeType(LandscapeType(widget - WID_GL_TEMPERATE));
				SndClickBeep();
				break;

			case WID_GL_MAPSIZE_X_PULLDOWN: // Mapsize X
				ShowDropDownList(this, BuildMapsizeDropDown(), _settings_newgame.game_creation.map_x, WID_GL_MAPSIZE_X_PULLDOWN);
				break;

			case WID_GL_MAPSIZE_Y_PULLDOWN: // Mapsize Y
				ShowDropDownList(this, BuildMapsizeDropDown(), _settings_newgame.game_creation.map_y, WID_GL_MAPSIZE_Y_PULLDOWN);
				break;

			case WID_GL_TOWN_PULLDOWN: // Number of towns
				ShowDropDownMenu(this, _num_towns, _settings_newgame.difficulty.number_towns, WID_GL_TOWN_PULLDOWN, 0, 0);
				break;

			case WID_GL_TOWNNAME_DROPDOWN: // Townname generator
				ShowDropDownList(this, BuildTownNameDropDown(), _settings_newgame.game_creation.town_name, WID_GL_TOWNNAME_DROPDOWN);
				break;

			case WID_GL_INDUSTRY_PULLDOWN: // Number of industries
				ShowDropDownMenu(this, _num_inds, _settings_newgame.difficulty.industry_density, WID_GL_INDUSTRY_PULLDOWN, 0, 0);
				break;

			case WID_GL_GENERATE_BUTTON: { // Generate
				/* Get rotated map size. */
				uint map_x;
				uint map_y;
				if (_settings_newgame.game_creation.heightmap_rotation == HM_CLOCKWISE) {
					map_x = this->y;
					map_y = this->x;
				} else {
					map_x = this->x;
					map_y = this->y;
				}
				if (mode == GLWM_HEIGHTMAP &&
						(map_x * 2 < (1U << _settings_newgame.game_creation.map_x) ||
						map_x / 2 > (1U << _settings_newgame.game_creation.map_x) ||
						map_y * 2 < (1U << _settings_newgame.game_creation.map_y) ||
						map_y / 2 > (1U << _settings_newgame.game_creation.map_y))) {
					ShowQuery(
						GetEncodedString(STR_WARNING_HEIGHTMAP_SCALE_CAPTION),
						GetEncodedString(STR_WARNING_HEIGHTMAP_SCALE_MESSAGE),
						this,
						LandscapeGenerationCallback);
				} else {
					StartGeneratingLandscape(mode);
				}
				break;
			}

			case WID_GL_HEIGHTMAP_HEIGHT_DOWN:
			case WID_GL_HEIGHTMAP_HEIGHT_UP: // Height level buttons
				/* Don't allow too fast scrolling */
				if (!this->flags.Test(WindowFlag::Timeout) || this->timeout_timer <= 1) {
					this->HandleButtonClick(widget);

					_settings_newgame.game_creation.heightmap_height = Clamp(_settings_newgame.game_creation.heightmap_height + widget - WID_GL_HEIGHTMAP_HEIGHT_TEXT, MIN_HEIGHTMAP_HEIGHT, GetMapHeightLimit());
					this->InvalidateData();
				}
				_left_button_clicked = false;
				break;

			case WID_GL_HEIGHTMAP_HEIGHT_TEXT: // Height level text
				this->widget_id = WID_GL_HEIGHTMAP_HEIGHT_TEXT;
				ShowQueryString(GetString(STR_JUST_INT, _settings_newgame.game_creation.heightmap_height), STR_MAPGEN_HEIGHTMAP_HEIGHT_QUERY_CAPT, 4, this, CS_NUMERAL, QueryStringFlag::EnableDefault);
				SndClickBeep();
				break;


			case WID_GL_START_DATE_DOWN:
			case WID_GL_START_DATE_UP: // Year buttons
				/* Don't allow too fast scrolling */
				if (!this->flags.Test(WindowFlag::Timeout) || this->timeout_timer <= 1) {
					this->HandleButtonClick(widget);

					_settings_newgame.game_creation.starting_year = Clamp(_settings_newgame.game_creation.starting_year + widget - WID_GL_START_DATE_TEXT, CalendarTime::MIN_YEAR, CalendarTime::MAX_YEAR);
					this->InvalidateData();
				}
				_left_button_clicked = false;
				break;

			case WID_GL_START_DATE_TEXT: // Year text
				this->widget_id = WID_GL_START_DATE_TEXT;
				ShowQueryString(GetString(STR_JUST_INT, _settings_newgame.game_creation.starting_year), STR_MAPGEN_START_DATE_QUERY_CAPT, 8, this, CS_NUMERAL, QueryStringFlag::EnableDefault);
				break;

			case WID_GL_SNOW_COVERAGE_DOWN:
			case WID_GL_SNOW_COVERAGE_UP: // Snow coverage buttons
				/* Don't allow too fast scrolling */
				if (!this->flags.Test(WindowFlag::Timeout) || this->timeout_timer <= 1) {
					this->HandleButtonClick(widget);

					_settings_newgame.game_creation.snow_coverage = Clamp(_settings_newgame.game_creation.snow_coverage + (widget - WID_GL_SNOW_COVERAGE_TEXT) * 10, 0, 100);
					this->InvalidateData();
				}
				_left_button_clicked = false;
				break;

			case WID_GL_SNOW_COVERAGE_TEXT: // Snow coverage text
				this->widget_id = WID_GL_SNOW_COVERAGE_TEXT;
				ShowQueryString(GetString(STR_JUST_INT, _settings_newgame.game_creation.snow_coverage), STR_MAPGEN_SNOW_COVERAGE_QUERY_CAPT, 4, this, CS_NUMERAL, QueryStringFlag::EnableDefault);
				SndClickBeep();
				break;

			case WID_GL_DESERT_COVERAGE_DOWN:
			case WID_GL_DESERT_COVERAGE_UP: // Desert coverage buttons
				/* Don't allow too fast scrolling */
				if (!this->flags.Test(WindowFlag::Timeout) || this->timeout_timer <= 1) {
					this->HandleButtonClick(widget);

					_settings_newgame.game_creation.desert_coverage = Clamp(_settings_newgame.game_creation.desert_coverage + (widget - WID_GL_DESERT_COVERAGE_TEXT) * 10, 0, 100);
					this->InvalidateData();
				}
				_left_button_clicked = false;
				break;

			case WID_GL_DESERT_COVERAGE_TEXT: // Desert line text
				this->widget_id = WID_GL_DESERT_COVERAGE_TEXT;
				ShowQueryString(GetString(STR_JUST_INT, _settings_newgame.game_creation.desert_coverage), STR_MAPGEN_DESERT_COVERAGE_QUERY_CAPT, 4, this, CS_NUMERAL, QueryStringFlag::EnableDefault);
				SndClickBeep();
				break;

			case WID_GL_HEIGHTMAP_ROTATION_PULLDOWN: // Heightmap rotation
				ShowDropDownMenu(this, _rotation, _settings_newgame.game_creation.heightmap_rotation, WID_GL_HEIGHTMAP_ROTATION_PULLDOWN, 0, 0);
				break;

			case WID_GL_TERRAIN_PULLDOWN: // Terrain type
				/* For the original map generation only the first four are valid. */
				ShowDropDownMenu(this, _elevations, _settings_newgame.difficulty.terrain_type, WID_GL_TERRAIN_PULLDOWN, 0, _settings_newgame.game_creation.land_generator == LG_ORIGINAL ? ~0xF : 0);
				break;

			case WID_GL_WATER_PULLDOWN: { // Water quantity
				uint32_t hidden_mask = 0;
				/* Disable custom water level when the original map generator is active. */
				if (_settings_newgame.game_creation.land_generator == LG_ORIGINAL) {
					SetBit(hidden_mask, CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY);
				}
				ShowDropDownMenu(this, _sea_lakes, _settings_newgame.difficulty.quantity_sea_lakes, WID_GL_WATER_PULLDOWN, 0, hidden_mask);
				break;
			}

			case WID_GL_RIVER_PULLDOWN: // Amount of rivers
				ShowDropDownMenu(this, _rivers, _settings_newgame.game_creation.amount_of_rivers, WID_GL_RIVER_PULLDOWN, 0, 0);
				break;

			case WID_GL_SMOOTHNESS_PULLDOWN: // Map smoothness
				ShowDropDownMenu(this, _smoothness, _settings_newgame.game_creation.tgen_smoothness, WID_GL_SMOOTHNESS_PULLDOWN, 0, 0);
				break;

			case WID_GL_VARIETY_PULLDOWN: // Map variety
				ShowDropDownMenu(this, _variety, _settings_newgame.game_creation.variety, WID_GL_VARIETY_PULLDOWN, 0, 0);
				break;

			/* Map borders */
			case WID_GL_BORDERS_PULLDOWN:
				ShowDropDownMenu(this, _borders, _settings_newgame.game_creation.water_border_presets, WID_GL_BORDERS_PULLDOWN, 0, 0);
				break;

			case WID_GL_WATER_NW:
				_settings_newgame.game_creation.water_borders.Flip(BorderFlag::NorthWest);
				SndClickBeep();
				this->InvalidateData();
				break;

			case WID_GL_WATER_NE:
				_settings_newgame.game_creation.water_borders.Flip(BorderFlag::NorthEast);
				SndClickBeep();
				this->InvalidateData();
				break;

			case WID_GL_WATER_SE:
				_settings_newgame.game_creation.water_borders.Flip(BorderFlag::SouthEast);
				SndClickBeep();
				this->InvalidateData();
				break;

			case WID_GL_WATER_SW:
				_settings_newgame.game_creation.water_borders.Flip(BorderFlag::SouthWest);
				SndClickBeep();
				this->InvalidateData();
				break;

			case WID_GL_AI_BUTTON: ///< AI Settings
				ShowAIConfigWindow();
				break;

			case WID_GL_GS_BUTTON: ///< Game Script Settings
				ShowGSConfigWindow();
				break;

			case WID_GL_NEWGRF_BUTTON: ///< NewGRF Settings
				ShowNewGRFSettings(true, true, false, _grfconfig_newgame);
				break;
		}
	}

	void OnTimeout() override
	{
		if (mode == GLWM_HEIGHTMAP) {
			this->RaiseWidgetsWhenLowered(WID_GL_HEIGHTMAP_HEIGHT_DOWN, WID_GL_HEIGHTMAP_HEIGHT_UP, WID_GL_START_DATE_DOWN, WID_GL_START_DATE_UP, WID_GL_SNOW_COVERAGE_UP, WID_GL_SNOW_COVERAGE_DOWN, WID_GL_DESERT_COVERAGE_UP, WID_GL_DESERT_COVERAGE_DOWN);
		} else {
			this->RaiseWidgetsWhenLowered(WID_GL_START_DATE_DOWN, WID_GL_START_DATE_UP, WID_GL_SNOW_COVERAGE_UP, WID_GL_SNOW_COVERAGE_DOWN, WID_GL_DESERT_COVERAGE_UP, WID_GL_DESERT_COVERAGE_DOWN);
		}
	}

	void OnDropdownSelect(WidgetID widget, int index, int) override
	{
		switch (widget) {
			case WID_GL_MAPSIZE_X_PULLDOWN:     _settings_newgame.game_creation.map_x = index; break;
			case WID_GL_MAPSIZE_Y_PULLDOWN:     _settings_newgame.game_creation.map_y = index; break;
			case WID_GL_RIVER_PULLDOWN:         _settings_newgame.game_creation.amount_of_rivers = index; break;
			case WID_GL_SMOOTHNESS_PULLDOWN:    _settings_newgame.game_creation.tgen_smoothness = index;  break;
			case WID_GL_VARIETY_PULLDOWN:       _settings_newgame.game_creation.variety = index; break;

			case WID_GL_HEIGHTMAP_ROTATION_PULLDOWN: _settings_newgame.game_creation.heightmap_rotation = index; break;

			case WID_GL_TOWN_PULLDOWN:
				if ((uint)index == CUSTOM_TOWN_NUMBER_DIFFICULTY) {
					this->widget_id = widget;
					ShowQueryString(GetString(STR_JUST_INT, _settings_newgame.game_creation.custom_town_number), STR_MAPGEN_NUMBER_OF_TOWNS, 5, this, CS_NUMERAL, {});
				}
				_settings_newgame.difficulty.number_towns = index;
				break;

			case WID_GL_TOWNNAME_DROPDOWN: // Town names
				if (_game_mode == GM_MENU || Town::GetNumItems() == 0) {
					_settings_newgame.game_creation.town_name = index;
					SetWindowDirty(WC_GAME_OPTIONS, WN_GAME_OPTIONS_GAME_OPTIONS);
				}
				break;

			case WID_GL_INDUSTRY_PULLDOWN:
				if ((uint)index == ID_CUSTOM) {
					this->widget_id = widget;
					ShowQueryString(GetString(STR_JUST_INT, _settings_newgame.game_creation.custom_industry_number), STR_MAPGEN_NUMBER_OF_INDUSTRIES, 5, this, CS_NUMERAL, {});
				}
				_settings_newgame.difficulty.industry_density = index;
				break;

			case WID_GL_TERRAIN_PULLDOWN: {
				if ((uint)index == CUSTOM_TERRAIN_TYPE_NUMBER_DIFFICULTY) {
					this->widget_id = widget;
					ShowQueryString(GetString(STR_JUST_INT, _settings_newgame.game_creation.custom_terrain_type), STR_MAPGEN_TERRAIN_TYPE_QUERY_CAPT, 4, this, CS_NUMERAL, {});
				}
				_settings_newgame.difficulty.terrain_type = index;
				break;
			}

			case WID_GL_BORDERS_PULLDOWN: {
				switch (index) {
					case BFP_RANDOM:
						_settings_newgame.game_creation.water_borders = BorderFlag::Random;
						_settings_newgame.construction.freeform_edges = true;
						break;
					case BFP_MANUAL:
						_settings_newgame.game_creation.water_borders = {};
						_settings_newgame.construction.freeform_edges = true;
						break;
					case BFP_INFINITE_WATER:
						_settings_newgame.game_creation.water_borders = BORDERFLAGS_ALL;
						_settings_newgame.construction.freeform_edges = false;
						break;
				}
				_settings_newgame.game_creation.water_border_presets = static_cast<BorderFlagPresets>(index);
				break;
			}

			case WID_GL_WATER_PULLDOWN: {
				if ((uint)index == CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY) {
					this->widget_id = widget;
					ShowQueryString(GetString(STR_JUST_INT, _settings_newgame.game_creation.custom_sea_level), STR_MAPGEN_SEA_LEVEL, 3, this, CS_NUMERAL, {});
				}
				_settings_newgame.difficulty.quantity_sea_lakes = index;
				break;
			}
		}
		this->InvalidateData();
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		/* Was 'cancel' pressed? */
		if (!str.has_value()) return;

		int32_t value;
		if (!str->empty()) {
			auto val = ParseInteger<int32_t>(*str, 10, true);
			if (!val.has_value()) return;
			value = *val;
		} else {
			/* An empty string means revert to the default */
			switch (this->widget_id) {
				case WID_GL_HEIGHTMAP_HEIGHT_TEXT: value = MAP_HEIGHT_LIMIT_AUTO_MINIMUM; break;
				case WID_GL_START_DATE_TEXT: value = CalendarTime::DEF_START_YEAR.base(); break;
				case WID_GL_SNOW_COVERAGE_TEXT: value = DEF_SNOW_COVERAGE; break;
				case WID_GL_DESERT_COVERAGE_TEXT: value = DEF_DESERT_COVERAGE; break;
				case WID_GL_TOWN_PULLDOWN: value = 1; break;
				case WID_GL_INDUSTRY_PULLDOWN: value = 1; break;
				case WID_GL_TERRAIN_PULLDOWN: value = MIN_MAP_HEIGHT_LIMIT; break;
				case WID_GL_WATER_PULLDOWN: value = CUSTOM_SEA_LEVEL_MIN_PERCENTAGE; break;
				default: NOT_REACHED();
			}
		}

		switch (this->widget_id) {
			case WID_GL_HEIGHTMAP_HEIGHT_TEXT:
				this->SetWidgetDirty(WID_GL_HEIGHTMAP_HEIGHT_TEXT);
				_settings_newgame.game_creation.heightmap_height = Clamp(value, MIN_HEIGHTMAP_HEIGHT, GetMapHeightLimit());
				break;

			case WID_GL_START_DATE_TEXT:
				this->SetWidgetDirty(WID_GL_START_DATE_TEXT);
				_settings_newgame.game_creation.starting_year = Clamp(TimerGameCalendar::Year(value), CalendarTime::MIN_YEAR, CalendarTime::MAX_YEAR);
				break;

			case WID_GL_SNOW_COVERAGE_TEXT:
				this->SetWidgetDirty(WID_GL_SNOW_COVERAGE_TEXT);
				_settings_newgame.game_creation.snow_coverage = Clamp(value, 0, 100);
				break;

			case WID_GL_DESERT_COVERAGE_TEXT:
				this->SetWidgetDirty(WID_GL_DESERT_COVERAGE_TEXT);
				_settings_newgame.game_creation.desert_coverage = Clamp(value, 0, 100);
				break;

			case WID_GL_TOWN_PULLDOWN:
				_settings_newgame.game_creation.custom_town_number = Clamp(value, 1, CUSTOM_TOWN_MAX_NUMBER);
				break;

			case WID_GL_INDUSTRY_PULLDOWN:
				_settings_newgame.game_creation.custom_industry_number = Clamp(value, 1, IndustryPool::MAX_SIZE);
				break;

			case WID_GL_TERRAIN_PULLDOWN:
				_settings_newgame.game_creation.custom_terrain_type = Clamp(value, MIN_CUSTOM_TERRAIN_TYPE, GetMapHeightLimit());
				break;

			case WID_GL_WATER_PULLDOWN:
				_settings_newgame.game_creation.custom_sea_level = Clamp(value, CUSTOM_SEA_LEVEL_MIN_PERCENTAGE, CUSTOM_SEA_LEVEL_MAX_PERCENTAGE);
				break;
		}

		this->InvalidateData();
	}
};

static WindowDesc _generate_landscape_desc(
	WDP_CENTER, {}, 0, 0,
	WC_GENERATE_LANDSCAPE, WC_NONE,
	{},
	_nested_generate_landscape_widgets
);

static WindowDesc _heightmap_load_desc(
	WDP_CENTER, {}, 0, 0,
	WC_GENERATE_LANDSCAPE, WC_NONE,
	{},
	_nested_heightmap_load_widgets
);

static void _ShowGenerateLandscape(GenerateLandscapeWindowMode mode)
{
	uint x = 0;
	uint y = 0;

	CloseWindowByClass(WC_GENERATE_LANDSCAPE);

	/* Generate a new seed when opening the window */
	_settings_newgame.game_creation.generation_seed = InteractiveRandom();

	if (mode == GLWM_HEIGHTMAP) {
		/* If the function returns negative, it means there was a problem loading the heightmap */
		if (!GetHeightmapDimensions(_file_to_saveload.ftype.detailed, _file_to_saveload.name, &x, &y)) return;
	}

	WindowDesc &desc = (mode == GLWM_HEIGHTMAP) ? _heightmap_load_desc : _generate_landscape_desc;
	GenerateLandscapeWindow *w = AllocateWindowDescFront<GenerateLandscapeWindow, true>(desc, mode);

	if (mode == GLWM_HEIGHTMAP) {
		w->x = x;
		w->y = y;
		w->name = _file_to_saveload.title;
	}

	SetWindowDirty(WC_GENERATE_LANDSCAPE, mode);
}

/** Start with a normal game. */
void ShowGenerateLandscape()
{
	_ShowGenerateLandscape(GLWM_GENERATE);
}

/** Start with loading a heightmap. */
void ShowHeightmapLoad()
{
	_ShowGenerateLandscape(GLWM_HEIGHTMAP);
}

/** Start with a scenario editor. */
void StartScenarioEditor()
{
	StartGeneratingLandscape(GLWM_SCENARIO);
}

/**
 * Start a normal game without the GUI.
 * @param seed The seed of the new game.
 */
void StartNewGameWithoutGUI(uint32_t seed)
{
	/* GenerateWorld takes care of the possible GENERATE_NEW_SEED value in 'seed' */
	_settings_newgame.game_creation.generation_seed = seed;

	StartGeneratingLandscape(GLWM_GENERATE);
}

struct CreateScenarioWindow : public Window
{
	WidgetID widget_id{};

	CreateScenarioWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);
		this->LowerWidget(to_underlying(_settings_newgame.game_creation.landscape) + WID_CS_TEMPERATE);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_CS_START_DATE_TEXT:
				return GetString(STR_JUST_DATE_LONG, TimerGameCalendar::ConvertYMDToDate(_settings_newgame.game_creation.starting_year, 0, 1));

			case WID_CS_MAPSIZE_X_PULLDOWN:
				return GetString(STR_JUST_INT, 1LL << _settings_newgame.game_creation.map_x);

			case WID_CS_MAPSIZE_Y_PULLDOWN:
				return GetString(STR_JUST_INT, 1LL << _settings_newgame.game_creation.map_y);

			case WID_CS_FLAT_LAND_HEIGHT_TEXT:
				return GetString(STR_JUST_INT, _settings_newgame.game_creation.se_flat_world_height);

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void OnPaint() override
	{
		this->SetWidgetDisabledState(WID_CS_START_DATE_DOWN,       _settings_newgame.game_creation.starting_year <= CalendarTime::MIN_YEAR);
		this->SetWidgetDisabledState(WID_CS_START_DATE_UP,         _settings_newgame.game_creation.starting_year >= CalendarTime::MAX_YEAR);
		this->SetWidgetDisabledState(WID_CS_FLAT_LAND_HEIGHT_DOWN, _settings_newgame.game_creation.se_flat_world_height <= 0);
		this->SetWidgetDisabledState(WID_CS_FLAT_LAND_HEIGHT_UP,   _settings_newgame.game_creation.se_flat_world_height >= GetMapHeightLimit());

		this->SetWidgetLoweredState(WID_CS_TEMPERATE, _settings_newgame.game_creation.landscape == LandscapeType::Temperate);
		this->SetWidgetLoweredState(WID_CS_ARCTIC,    _settings_newgame.game_creation.landscape == LandscapeType::Arctic);
		this->SetWidgetLoweredState(WID_CS_TROPICAL,  _settings_newgame.game_creation.landscape == LandscapeType::Tropic);
		this->SetWidgetLoweredState(WID_CS_TOYLAND,   _settings_newgame.game_creation.landscape == LandscapeType::Toyland);

		this->DrawWidgets();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		std::string str;
		switch (widget) {
			case WID_CS_TEMPERATE: case WID_CS_ARCTIC:
			case WID_CS_TROPICAL: case WID_CS_TOYLAND:
				size.width += WidgetDimensions::scaled.fullbevel.Horizontal();
				size.height += WidgetDimensions::scaled.fullbevel.Vertical();
				return;

			case WID_CS_START_DATE_TEXT:
				str = GetString(STR_JUST_DATE_LONG, TimerGameCalendar::ConvertYMDToDate(CalendarTime::MAX_YEAR, 0, 1));
				break;

			case WID_CS_MAPSIZE_X_PULLDOWN:
			case WID_CS_MAPSIZE_Y_PULLDOWN:
				str = GetString(STR_JUST_INT, GetParamMaxValue(MAX_MAP_SIZE));
				break;

			case WID_CS_FLAT_LAND_HEIGHT_TEXT:
				str = GetString(STR_JUST_INT, GetParamMaxValue(MAX_TILE_HEIGHT));
				break;

			default:
				return;
		}
		Dimension d = GetStringBoundingBox(str);
		d.width += padding.width;
		d.height += padding.height;
		size = maxdim(size, d);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_CS_TEMPERATE:
			case WID_CS_ARCTIC:
			case WID_CS_TROPICAL:
			case WID_CS_TOYLAND:
				this->RaiseWidget(to_underlying(_settings_newgame.game_creation.landscape) + WID_CS_TEMPERATE);
				SetNewLandscapeType(LandscapeType(widget - WID_CS_TEMPERATE));
				break;

			case WID_CS_MAPSIZE_X_PULLDOWN: // Mapsize X
				ShowDropDownList(this, BuildMapsizeDropDown(), _settings_newgame.game_creation.map_x, WID_CS_MAPSIZE_X_PULLDOWN);
				break;

			case WID_CS_MAPSIZE_Y_PULLDOWN: // Mapsize Y
				ShowDropDownList(this, BuildMapsizeDropDown(), _settings_newgame.game_creation.map_y, WID_CS_MAPSIZE_Y_PULLDOWN);
				break;

			case WID_CS_EMPTY_WORLD: // Empty world / flat world
				StartGeneratingLandscape(GLWM_SCENARIO);
				break;

			case WID_CS_RANDOM_WORLD: // Generate
				ShowGenerateLandscape();
				break;

			case WID_CS_START_DATE_DOWN:
			case WID_CS_START_DATE_UP: // Year buttons
				/* Don't allow too fast scrolling */
				if (!this->flags.Test(WindowFlag::Timeout) || this->timeout_timer <= 1) {
					this->HandleButtonClick(widget);
					this->SetDirty();

					_settings_newgame.game_creation.starting_year = Clamp(_settings_newgame.game_creation.starting_year + widget - WID_CS_START_DATE_TEXT, CalendarTime::MIN_YEAR, CalendarTime::MAX_YEAR);
				}
				_left_button_clicked = false;
				break;

			case WID_CS_START_DATE_TEXT: // Year text
				this->widget_id = WID_CS_START_DATE_TEXT;
				ShowQueryString(GetString(STR_JUST_INT, _settings_newgame.game_creation.starting_year), STR_MAPGEN_START_DATE_QUERY_CAPT, 8, this, CS_NUMERAL, {});
				break;

			case WID_CS_FLAT_LAND_HEIGHT_DOWN:
			case WID_CS_FLAT_LAND_HEIGHT_UP: // Height level buttons
				/* Don't allow too fast scrolling */
				if (!this->flags.Test(WindowFlag::Timeout) || this->timeout_timer <= 1) {
					this->HandleButtonClick(widget);
					this->SetDirty();

					_settings_newgame.game_creation.se_flat_world_height = Clamp(_settings_newgame.game_creation.se_flat_world_height + widget - WID_CS_FLAT_LAND_HEIGHT_TEXT, 0, GetMapHeightLimit());
				}
				_left_button_clicked = false;
				break;

			case WID_CS_FLAT_LAND_HEIGHT_TEXT: // Height level text
				this->widget_id = WID_CS_FLAT_LAND_HEIGHT_TEXT;
				ShowQueryString(GetString(STR_JUST_INT, _settings_newgame.game_creation.se_flat_world_height), STR_SE_MAPGEN_FLAT_WORLD_HEIGHT_QUERY_CAPT, 4, this, CS_NUMERAL, {});
				break;
		}
	}

	void OnTimeout() override
	{
		this->RaiseWidgetsWhenLowered(WID_CS_START_DATE_DOWN, WID_CS_START_DATE_UP, WID_CS_FLAT_LAND_HEIGHT_DOWN, WID_CS_FLAT_LAND_HEIGHT_UP);
	}

	void OnDropdownSelect(WidgetID widget, int index, int) override
	{
		switch (widget) {
			case WID_CS_MAPSIZE_X_PULLDOWN: _settings_newgame.game_creation.map_x = index; break;
			case WID_CS_MAPSIZE_Y_PULLDOWN: _settings_newgame.game_creation.map_y = index; break;
		}
		this->SetDirty();
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value()) return;

		auto value = ParseInteger<int32_t>(*str, 10, true);
		if (!value.has_value()) return;

		switch (this->widget_id) {
			case WID_CS_START_DATE_TEXT:
				this->SetWidgetDirty(WID_CS_START_DATE_TEXT);
				_settings_newgame.game_creation.starting_year = Clamp(TimerGameCalendar::Year(*value), CalendarTime::MIN_YEAR, CalendarTime::MAX_YEAR);
				break;

			case WID_CS_FLAT_LAND_HEIGHT_TEXT:
				this->SetWidgetDirty(WID_CS_FLAT_LAND_HEIGHT_TEXT);
				_settings_newgame.game_creation.se_flat_world_height = Clamp(*value, 0, GetMapHeightLimit());
				break;
		}

		this->SetDirty();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_create_scenario_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_SE_MAPGEN_CAPTION),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.sparse),
			/* Landscape style selection. */
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0), SetPIPRatio(1, 1, 1),
				NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, WID_CS_TEMPERATE), SetSpriteTip(SPR_SELECT_TEMPERATE, STR_INTRO_TOOLTIP_TEMPERATE),
				NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, WID_CS_ARCTIC), SetSpriteTip(SPR_SELECT_SUB_ARCTIC, STR_INTRO_TOOLTIP_SUB_ARCTIC_LANDSCAPE),
				NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, WID_CS_TROPICAL), SetSpriteTip(SPR_SELECT_SUB_TROPICAL, STR_INTRO_TOOLTIP_SUB_TROPICAL_LANDSCAPE),
				NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, WID_CS_TOYLAND), SetSpriteTip(SPR_SELECT_TOYLAND, STR_INTRO_TOOLTIP_TOYLAND_LANDSCAPE),
			EndContainer(),

			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				/* Green generation type buttons: 'Flat land' and 'Random land'. */
				NWidget(NWID_VERTICAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_CS_EMPTY_WORLD), SetStringTip(STR_SE_MAPGEN_FLAT_WORLD, STR_SE_MAPGEN_FLAT_WORLD_TOOLTIP), SetFill(1, 1),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_CS_RANDOM_WORLD), SetStringTip(STR_SE_MAPGEN_RANDOM_LAND, STR_TERRAFORM_TOOLTIP_GENERATE_RANDOM_LAND), SetFill(1, 1),
				EndContainer(),

				/* Labels + setting drop-downs */
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
					/* Labels. */
					NWidget(NWID_VERTICAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_MAPSIZE, STR_MAPGEN_MAPSIZE_TOOLTIP), SetFill(0, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_DATE, STR_MAPGEN_DATE_TOOLTIP), SetFill(0, 1),
						NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_SE_MAPGEN_FLAT_WORLD_HEIGHT, STR_SE_MAPGEN_FLAT_WORLD_HEIGHT_TOOLTIP), SetFill(0, 1),
					EndContainer(),

					NWidget(NWID_VERTICAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
						/* Map size. */
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_CS_MAPSIZE_X_PULLDOWN), SetToolTip(STR_MAPGEN_MAPSIZE_TOOLTIP), SetFill(1, 1),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_MAPGEN_BY), SetFill(0, 1), SetAlignment(SA_CENTER),
							NWidget(WWT_DROPDOWN, COLOUR_ORANGE, WID_CS_MAPSIZE_Y_PULLDOWN), SetToolTip(STR_MAPGEN_MAPSIZE_TOOLTIP), SetFill(1, 1),
						EndContainer(),

						/* Date. */
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_CS_START_DATE_DOWN), SetFill(0, 1), SetSpriteTip(SPR_ARROW_DOWN, STR_SCENEDIT_TOOLBAR_MOVE_THE_STARTING_DATE_BACKWARD_TOOLTIP), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
							NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_CS_START_DATE_TEXT),  SetFill(1, 1), SetToolTip(STR_MAPGEN_DATE_TOOLTIP),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_CS_START_DATE_UP), SetFill(0, 1), SetSpriteTip(SPR_ARROW_UP, STR_SCENEDIT_TOOLBAR_MOVE_THE_STARTING_DATE_FORWARD_TOOLTIP), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
						EndContainer(),

						/* Flat map height. */
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_CS_FLAT_LAND_HEIGHT_DOWN), SetFill(0, 1), SetSpriteTip(SPR_ARROW_DOWN, STR_SE_MAPGEN_FLAT_WORLD_HEIGHT_DOWN_TOOLTIP), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
							NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_CS_FLAT_LAND_HEIGHT_TEXT),  SetFill(1, 1), SetToolTip(STR_SE_MAPGEN_FLAT_WORLD_HEIGHT_TOOLTIP),
							NWidget(WWT_IMGBTN, COLOUR_ORANGE, WID_CS_FLAT_LAND_HEIGHT_UP), SetFill(0, 1), SetSpriteTip(SPR_ARROW_UP, STR_SE_MAPGEN_FLAT_WORLD_HEIGHT_UP_TOOLTIP), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON),
						EndContainer(),
					EndContainer(),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _create_scenario_desc(
	WDP_CENTER, {}, 0, 0,
	WC_GENERATE_LANDSCAPE, WC_NONE,
	{},
	_nested_create_scenario_widgets
);

/** Show the window to create a scenario. */
void ShowCreateScenario()
{
	CloseWindowByClass(WC_GENERATE_LANDSCAPE);
	new CreateScenarioWindow(_create_scenario_desc, GLWM_SCENARIO);
}

static constexpr std::initializer_list<NWidgetPart> _nested_generate_progress_widgets = {
	NWidget(WWT_CAPTION, COLOUR_GREY), SetStringTip(STR_GENERATION_WORLD, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GP_PROGRESS_BAR), SetFill(1, 0),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GP_PROGRESS_TEXT), SetFill(1, 0),
			NWidget(WWT_TEXTBTN, COLOUR_WHITE, WID_GP_ABORT), SetStringTip(STR_GENERATION_ABORT), SetFill(1, 0),
		EndContainer(),
	EndContainer(),
};


static WindowDesc _generate_progress_desc(
	WDP_CENTER, {}, 0, 0,
	WC_MODAL_PROGRESS, WC_NONE,
	WindowDefaultFlag::NoClose,
	_nested_generate_progress_widgets
);

struct GenWorldStatus {
	static inline uint percent;
	static inline StringID cls;
	static inline uint current;
	static inline uint total;
};

static const StringID _generation_class_table[]  = {
	STR_GENERATION_WORLD_GENERATION,
	STR_GENERATION_LANDSCAPE_GENERATION,
	STR_GENERATION_RIVER_GENERATION,
	STR_GENERATION_CLEARING_TILES,
	STR_GENERATION_TOWN_GENERATION,
	STR_GENERATION_LAND_INDUSTRY_GENERATION,
	STR_GENERATION_WATER_INDUSTRY_GENERATION,
	STR_GENERATION_OBJECT_GENERATION,
	STR_GENERATION_TREE_GENERATION,
	STR_GENERATION_SETTINGUP_GAME,
	STR_GENERATION_PREPARING_TILELOOP,
	STR_GENERATION_PREPARING_SCRIPT,
	STR_GENERATION_PREPARING_GAME
};
static_assert(lengthof(_generation_class_table) == GWP_CLASS_COUNT);


static void AbortGeneratingWorldCallback(Window *, bool confirmed)
{
	if (confirmed) {
		AbortGeneratingWorld();
	} else if (HasModalProgress() && !IsGeneratingWorldAborted()) {
		SetMouseCursor(SPR_CURSOR_ZZZ, PAL_NONE);
	}
}

struct GenerateProgressWindow : public Window {

	GenerateProgressWindow() : Window(_generate_progress_desc)
	{
		this->InitNested();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_GP_ABORT:
				SetMouseCursorBusy(false);
				ShowQuery(
					GetEncodedString(STR_GENERATION_ABORT_CAPTION),
					GetEncodedString(STR_GENERATION_ABORT_MESSAGE),
					this,
					AbortGeneratingWorldCallback
				);
				break;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_GP_PROGRESS_BAR: {
				size = GetStringBoundingBox(GetString(STR_GENERATION_PROGRESS, GetParamMaxValue(100)));
				/* We need some spacing for the 'border' */
				size.height += WidgetDimensions::scaled.frametext.Horizontal();
				size.width  += WidgetDimensions::scaled.frametext.Vertical();
				break;
			}

			case WID_GP_PROGRESS_TEXT:
				for (uint i = 0; i < GWP_CLASS_COUNT; i++) {
					size.width = std::max(size.width, GetStringBoundingBox(_generation_class_table[i]).width + padding.width);
				}
				size.height = GetCharacterHeight(FS_NORMAL) * 2 + WidgetDimensions::scaled.vsep_normal;
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_GP_PROGRESS_BAR: {
				/* Draw the % complete with a bar and a text */
				DrawFrameRect(r, COLOUR_GREY, {FrameFlag::BorderOnly, FrameFlag::Lowered});
				Rect br = r.Shrink(WidgetDimensions::scaled.bevel);
				DrawFrameRect(br.WithWidth(br.Width() * GenWorldStatus::percent / 100, _current_text_dir == TD_RTL), COLOUR_MAUVE, {});
				DrawString(br.CentreToHeight(GetCharacterHeight(FS_NORMAL)), GetString(STR_GENERATION_PROGRESS, GenWorldStatus::percent), TC_FROMSTRING, SA_HOR_CENTER);
				break;
			}

			case WID_GP_PROGRESS_TEXT:
				/* Tell which class we are generating */
				DrawString(r.left, r.right, r.top, GenWorldStatus::cls, TC_FROMSTRING, SA_HOR_CENTER);

				/* And say where we are in that class */
				DrawString(r.left, r.right, r.top + GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal,
					GetString(STR_GENERATION_PROGRESS_NUM, GenWorldStatus::current, GenWorldStatus::total), TC_FROMSTRING, SA_HOR_CENTER);
		}
	}
};

/**
 * Initializes the progress counters to the starting point.
 */
void PrepareGenerateWorldProgress()
{
	GenWorldStatus::cls = STR_GENERATION_WORLD_GENERATION;
	GenWorldStatus::current = 0;
	GenWorldStatus::total = 0;
	GenWorldStatus::percent = 0;
}

/**
 * Show the window where a user can follow the process of the map generation.
 */
void ShowGenerateWorldProgress()
{
	if (BringWindowToFrontById(WC_MODAL_PROGRESS, 0)) return;
	new GenerateProgressWindow();
}

static void _SetGeneratingWorldProgress(GenWorldProgress cls, uint progress, uint total)
{
	static const int percent_table[] = {0, 5, 14, 17, 20, 40, 55, 60, 65, 80, 85, 95, 99, 100 };
	static_assert(lengthof(percent_table) == GWP_CLASS_COUNT + 1);
	assert(cls < GWP_CLASS_COUNT);

	/* Check if we really are generating the world.
	 * For example, placing trees via the SE also calls this function, but
	 * shouldn't try to update the progress.
	 */
	if (!HasModalProgress()) return;

	if (IsGeneratingWorldAborted()) {
		HandleGeneratingWorldAbortion();
		return;
	}

	if (total == 0) {
		assert(GenWorldStatus::cls == _generation_class_table[cls]);
		GenWorldStatus::current += progress;
		assert(GenWorldStatus::current <= GenWorldStatus::total);
	} else {
		GenWorldStatus::cls     = _generation_class_table[cls];
		GenWorldStatus::current = progress;
		GenWorldStatus::total   = total;
		GenWorldStatus::percent = percent_table[cls];
	}

	/* Percentage is about the number of completed tasks, so 'current - 1' */
	GenWorldStatus::percent = percent_table[cls] + (percent_table[cls + 1] - percent_table[cls]) * (GenWorldStatus::current == 0 ? 0 : GenWorldStatus::current - 1) / GenWorldStatus::total;

	if (_network_dedicated) {
		static uint last_percent = 0;

		/* Never display 0% */
		if (GenWorldStatus::percent == 0) return;
		/* Reset if percent is lower than the last recorded */
		if (GenWorldStatus::percent < last_percent) last_percent = 0;
		/* Display every 5%, but 6% is also very valid.. just not smaller steps than 5% */
		if (GenWorldStatus::percent % 5 != 0 && GenWorldStatus::percent <= last_percent + 5) return;
		/* Never show steps smaller than 2%, even if it is a mod 5% */
		if (GenWorldStatus::percent <= last_percent + 2) return;

		Debug(net, 3, "Map generation percentage complete: {}", GenWorldStatus::percent);
		last_percent = GenWorldStatus::percent;

		return;
	}

	SetWindowDirty(WC_MODAL_PROGRESS, 0);

	VideoDriver::GetInstance()->GameLoopPause();
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
