/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file terraform_widget.h Types related to the terraform widgets. */

#ifndef WIDGETS_TERRAFORM_WIDGET_H
#define WIDGETS_TERRAFORM_WIDGET_H

/** Widgets of the WC_SCEN_LAND_GEN (WC_SCEN_LAND_GEN is also used in EditorTerraformToolbarWidgets). */
enum TerraformToolbarWidgets {
	TTW_SHOW_PLACE_OBJECT,                ///< Should the place object button be shown?
	TTW_BUTTONS_START,                    ///< Start of pushable buttons
	TTW_LOWER_LAND = TTW_BUTTONS_START,   ///< Lower land button
	TTW_RAISE_LAND,                       ///< Raise land button
	TTW_LEVEL_LAND,                       ///< Level land button
	TTW_DEMOLISH,                         ///< Demolish aka dynamite button
	TTW_BUY_LAND,                         ///< Buy land button
	TTW_PLANT_TREES,                      ///< Plant trees button (note: opens seperate window, no place-push-button)
	TTW_PLACE_SIGN,                       ///< Place sign button
	TTW_PLACE_OBJECT,                     ///< Place object button
};

/** Widgets of the WC_SCEN_LAND_GEN (WC_SCEN_LAND_GEN is also used in TerraformToolbarWidgets). */
enum EditorTerraformToolbarWidgets {
	ETTW_SHOW_PLACE_DESERT,                ///< Should the place desert button be shown?
	ETTW_START,                            ///< Used for iterations
	ETTW_DOTS = ETTW_START,                ///< Invisible widget for rendering the terraform size on.
	ETTW_BUTTONS_START,                    ///< Start of pushable buttons
	ETTW_DEMOLISH = ETTW_BUTTONS_START,    ///< Demolish aka dynamite button
	ETTW_LOWER_LAND,                       ///< Lower land button
	ETTW_RAISE_LAND,                       ///< Raise land button
	ETTW_LEVEL_LAND,                       ///< Level land button
	ETTW_PLACE_ROCKS,                      ///< Place rocks button
	ETTW_PLACE_DESERT,                     ///< Place desert button (in tropical climate)
	ETTW_PLACE_OBJECT,                     ///< Place transmitter button
	ETTW_BUTTONS_END,                      ///< End of pushable buttons
	ETTW_INCREASE_SIZE = ETTW_BUTTONS_END, ///< Upwards arrow button to increase terraforming size
	ETTW_DECREASE_SIZE,                    ///< Downwards arrow button to decrease terraforming size
	ETTW_NEW_SCENARIO,                     ///< Button for generating a new scenario
	ETTW_RESET_LANDSCAPE,                  ///< Button for removing all company-owned property
};

#endif /* WIDGETS_TERRAFORM_WIDGET_H */
