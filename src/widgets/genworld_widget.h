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
enum GenerateLandscapeWidgets : WidgetID {
	WID_GL_TEMPERATE,                   ///< Button with icon "Temperate".
	WID_GL_ARCTIC,                      ///< Button with icon "Arctic".
	WID_GL_TROPICAL,                    ///< Button with icon "Tropical".
	WID_GL_TOYLAND,                     ///< Button with icon "Toyland".

	WID_GL_MAPSIZE_X_PULLDOWN,          ///< Dropdown 'map X size'.
	WID_GL_MAPSIZE_Y_PULLDOWN,          ///< Dropdown 'map Y size'.

	WID_GL_TOWN_PULLDOWN,               ///< Dropdown 'No. of towns'.
	WID_GL_TOWNNAME_DROPDOWN,           ///< Dropdown 'Townnames'.
	WID_GL_INDUSTRY_PULLDOWN,           ///< Dropdown 'No. of industries'.

	WID_GL_GENERATE_BUTTON,             ///< 'Generate' button.

	WID_GL_HEIGHTMAP_HEIGHT_DOWN,       ///< Decrease heightmap highest mountain
	WID_GL_HEIGHTMAP_HEIGHT_TEXT,       ///< Max. heightmap highest mountain
	WID_GL_HEIGHTMAP_HEIGHT_UP,         ///< Increase max. heightmap highest mountain

	WID_GL_START_DATE_DOWN,             ///< Decrease start year.
	WID_GL_START_DATE_TEXT,             ///< Start year.
	WID_GL_START_DATE_UP,               ///< Increase start year.

	WID_GL_SNOW_COVERAGE_DOWN,          ///< Decrease snow coverage.
	WID_GL_SNOW_COVERAGE_TEXT,          ///< Snow coverage.
	WID_GL_SNOW_COVERAGE_UP,            ///< Increase snow coverage.

	WID_GL_DESERT_COVERAGE_DOWN,        ///< Decrease desert coverage.
	WID_GL_DESERT_COVERAGE_TEXT,        ///< Desert coverage.
	WID_GL_DESERT_COVERAGE_UP,          ///< Increase desert coverage.

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

	WID_GL_AI_BUTTON,                   ///< 'AI Settings' button.
	WID_GL_GS_BUTTON,                   ///< 'Game Script Settings' button.
	WID_GL_NEWGRF_BUTTON,               ///< 'NewGRF Settings' button.

	WID_GL_CLIMATE_SEL_LABEL,           ///< NWID_SELECTION for snow or desert coverage label
	WID_GL_CLIMATE_SEL_SELECTOR,        ///< NWID_SELECTION for snow or desert coverage selector
};

/** Widgets of the #CreateScenarioWindow class. */
enum CreateScenarioWidgets : WidgetID {
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
enum GenerationProgressWidgets : WidgetID {
	WID_GP_PROGRESS_BAR,  ///< Progress bar.
	WID_GP_PROGRESS_TEXT, ///< Text with the progress bar.
	WID_GP_ABORT,         ///< Abort button.
};

#endif /* WIDGETS_GENWORLD_WIDGET_H */
