/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file genworld_widget.h Types related to the genworld widgets. */

#ifndef WIDGETS_GENWORLD_WIDGET_H
#define WIDGETS_GENWORLD_WIDGET_H

/** Widgets of the WC_GENERATE_LANDSCAPE (WC_GENERATE_LANDSCAPE is also used in GenerateLandscapeWindowWidgets and CreateScenarioWindowWidgets). */
enum GenenerateLandscapeWindowMode {
	GLWM_GENERATE,  ///< Generate new game
	GLWM_HEIGHTMAP, ///< Load from heightmap
	GLWM_SCENARIO,  ///< Generate flat land
};

/** Widgets of the WC_GENERATE_LANDSCAPE (WC_GENERATE_LANDSCAPE is also used in GenenerateLandscapeWindowMode and CreateScenarioWindowWidgets). */
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
	GLAND_HEIGHTMAP_SIZE_TEXT,         ///< Size of heightmap
	GLAND_HEIGHTMAP_ROTATION_PULLDOWN, ///< Dropdown 'Heightmap rotation'

	GLAND_TERRAIN_PULLDOWN,    ///< Dropdown 'Terrain type'
	GLAND_WATER_PULLDOWN,      ///< Dropdown 'Sea level'
	GLAND_RIVER_PULLDOWN,      ///< Dropdown 'Rivers'
	GLAND_SMOOTHNESS_PULLDOWN, ///< Dropdown 'Smoothness'
	GLAND_VARIETY_PULLDOWN,    ///< Dropdown 'Variety distribution'

	GLAND_BORDERS_RANDOM,      ///< 'Random'/'Manual' borders
	GLAND_WATER_NW,            ///< NW 'Water'/'Freeform'
	GLAND_WATER_NE,            ///< NE 'Water'/'Freeform'
	GLAND_WATER_SE,            ///< SE 'Water'/'Freeform'
	GLAND_WATER_SW,            ///< SW 'Water'/'Freeform'
};

/** Widgets of the WC_GENERATE_LANDSCAPE (WC_GENERATE_LANDSCAPE is also used in GenerateLandscapeWindowWidgets and GenenerateLandscapeWindowMode). */
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

/** Widgets of the WC_MODAL_PROGRESS (WC_MODAL_PROGRESS is also used in ScanProgressWindowWidgets). */
enum GenerationProgressWindowWidgets {
	GPWW_PROGRESS_BAR,
	GPWW_PROGRESS_TEXT,
	GPWW_ABORT,
};

#endif /* WIDGETS_GENWORLD_WIDGET_H */
