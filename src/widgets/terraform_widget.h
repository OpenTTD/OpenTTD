/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file terraform_widget.h Types related to the terraform widgets. */

#ifndef WIDGETS_TERRAFORM_WIDGET_H
#define WIDGETS_TERRAFORM_WIDGET_H

/** Widgets of the #TerraformToolbarWindow class. */
enum TerraformToolbarWidgets : WidgetID {
	WID_TT_SHOW_PLACE_OBJECT,                 ///< Should the place object button be shown?
	WID_TT_BUTTONS_START,                     ///< Start of pushable buttons.
	WID_TT_LOWER_LAND = WID_TT_BUTTONS_START, ///< Lower land button.
	WID_TT_RAISE_LAND,                        ///< Raise land button.
	WID_TT_LEVEL_LAND,                        ///< Level land button.
	WID_TT_DEMOLISH,                          ///< Demolish aka dynamite button.
	WID_TT_BUY_LAND,                          ///< Buy land button.
	WID_TT_PLANT_TREES,                       ///< Plant trees button (note: opens separate window, no place-push-button).
	WID_TT_PLACE_SIGN,                        ///< Place sign button.
	WID_TT_PLACE_OBJECT,                      ///< Place object button.

	INVALID_WID_TT = -1,
};

/** Widgets of the #ScenarioEditorLandscapeGenerationWindow class. */
enum EditorTerraformToolbarWidgets : WidgetID {
	WID_ETT_SHOW_PLACE_DESERT,                   ///< Should the place desert button be shown?
	WID_ETT_START,                               ///< Used for iterations.
	WID_ETT_DOTS = WID_ETT_START,                ///< Invisible widget for rendering the terraform size on.
	WID_ETT_BUTTONS_START,                       ///< Start of pushable buttons.
	WID_ETT_DEMOLISH = WID_ETT_BUTTONS_START,    ///< Demolish aka dynamite button.
	WID_ETT_LOWER_LAND,                          ///< Lower land button.
	WID_ETT_RAISE_LAND,                          ///< Raise land button.
	WID_ETT_LEVEL_LAND,                          ///< Level land button.
	WID_ETT_PLACE_ROCKS,                         ///< Place rocks button.
	WID_ETT_PLACE_DESERT,                        ///< Place desert button (in tropical climate).
	WID_ETT_PLACE_OBJECT,                        ///< Place transmitter button.
	WID_ETT_BUTTONS_END,                         ///< End of pushable buttons.
	WID_ETT_INCREASE_SIZE = WID_ETT_BUTTONS_END, ///< Upwards arrow button to increase terraforming size.
	WID_ETT_DECREASE_SIZE,                       ///< Downwards arrow button to decrease terraforming size.
	WID_ETT_NEW_SCENARIO,                        ///< Button for generating a new scenario.
	WID_ETT_RESET_LANDSCAPE,                     ///< Button for removing all company-owned property.

	INVALID_WID_ETT = -1,
};

#endif /* WIDGETS_TERRAFORM_WIDGET_H */
