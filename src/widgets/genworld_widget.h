/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file genworld_widget.h Types related to the genworld widgets. */

#ifndef WIDGETS_GENWORLD_WIDGET_H
#define WIDGETS_GENWORLD_WIDGET_H

/** Widgets of the #GenerateLandscapeWindow class. */
enum GenerateLandscapeWidgets {
	WID_GL_TEMPERATE,                   ///< Button with icon "Temperate".
	WID_GL_ARCTIC,                      ///< Button with icon "Arctic".
	WID_GL_TROPICAL,                    ///< Button with icon "Tropical".
	WID_GL_TOYLAND,                     ///< Button with icon "Toyland".

	WID_GL_MAPSIZE_X_PULLDOWN,          ///< Dropdown 'map X size'.
	WID_GL_MAPSIZE_Y_PULLDOWN,          ///< Dropdown 'map Y size'.

	WID_GL_TOWN_PULLDOWN,               ///< Dropdown 'No. of towns'.
	WID_GL_INDUSTRY_PULLDOWN,           ///< Dropdown 'No. of industries'.

	WID_GL_GENERATE_BUTTON,             ///< 'Generate' button.

	WID_GL_MAX_HEIGHTLEVEL_DOWN,        ///< Decrease max. heightlevel
	WID_GL_MAX_HEIGHTLEVEL_TEXT,        ///< Max. heightlevel
	WID_GL_MAX_HEIGHTLEVEL_UP,          ///< Increase max. heightlevel

	WID_GL_START_DATE_DOWN,             ///< Decrease start year.
	WID_GL_START_DATE_TEXT,             ///< Start year.
	WID_GL_START_DATE_UP,               ///< Increase start year.

	WID_GL_SNOW_LEVEL_DOWN,             ///< Decrease snow level.
	WID_GL_SNOW_LEVEL_TEXT,             ///< Snow level.
	WID_GL_SNOW_LEVEL_UP,               ///< Increase snow level.

	WID_GL_TREE_PULLDOWN,               ///< Dropdown 'Tree algorithm'.
	WID_GL_LANDSCAPE_PULLDOWN,          ///< Dropdown 'Land generator'.

	WID_GL_HEIGHTMAP_NAME_TEXT,         ///< Heightmap name.
	WID_GL_HEIGHTMAP_SIZE_TEXT,         ///< Size of heightmap.
	WID_GL_HEIGHTMAP_ROTATION_PULLDOWN, ///< Dropdown 'Heightmap rotation'.

	WID_GL_TERRAIN_PULLDOWN,            ///< Dropdown 'Terrain type'.
	WID_GL_WATER_PULLDOWN,              ///< Dropdown 'Sea level'.
	WID_GL_RIVER_PULLDOWN,              ///< Dropdown 'Rivers'.
	WID_GL_SMOOTHNESS_PULLDOWN,         ///< Dropdown 'Smoothness'.
	WID_GL_VARIETY_PULLDOWN,            ///< Dropdown 'Variety distribution'.

	WID_GL_BORDERS_RANDOM,              ///< 'Random'/'Manual' borders.
	WID_GL_WATER_NW,                    ///< NW 'Water'/'Freeform'.
	WID_GL_WATER_NE,                    ///< NE 'Water'/'Freeform'.
	WID_GL_WATER_SE,                    ///< SE 'Water'/'Freeform'.
	WID_GL_WATER_SW,                    ///< SW 'Water'/'Freeform'.
};

/** Widgets of the #CreateScenarioWindow class. */
enum CreateScenarioWidgets {
	WID_CS_TEMPERATE,              ///< Select temperate landscape style.
	WID_CS_ARCTIC,                 ///< Select arctic landscape style.
	WID_CS_TROPICAL,               ///< Select tropical landscape style.
	WID_CS_TOYLAND,                ///< Select toy-land landscape style.
	WID_CS_EMPTY_WORLD,            ///< Generate an empty flat world.
	WID_CS_RANDOM_WORLD,           ///< Generate random land button
	WID_CS_MAPSIZE_X_PULLDOWN,     ///< Pull-down arrow for x map size.
	WID_CS_MAPSIZE_Y_PULLDOWN,     ///< Pull-down arrow for y map size.
	WID_CS_START_DATE_DOWN,        ///< Decrease start year (start earlier).
	WID_CS_START_DATE_TEXT,        ///< Clickable start date value.
	WID_CS_START_DATE_UP,          ///< Increase start year (start later).
	WID_CS_FLAT_LAND_HEIGHT_DOWN,  ///< Decrease flat land height.
	WID_CS_FLAT_LAND_HEIGHT_TEXT,  ///< Clickable flat land height value.
	WID_CS_FLAT_LAND_HEIGHT_UP,    ///< Increase flat land height.
};

/** Widgets of the #GenerateProgressWindow class. */
enum GenerationProgressWidgets {
	WID_GP_PROGRESS_BAR,  ///< Progress bar.
	WID_GP_PROGRESS_TEXT, ///< Text with the progress bar.
	WID_GP_ABORT,         ///< Abort button.
};

#endif /* WIDGETS_GENWORLD_WIDGET_H */
