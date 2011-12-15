/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tooltype_widget.h Types related to the toolbar widgets. */

#ifndef WIDGETS_TOOLBAR_WIDGET_H
#define WIDGETS_TOOLBAR_WIDGET_H

/** Widgets of the WC_MAIN_TOOLBAR, when in normal game mode. */
enum ToolbarNormalWidgets {
	TBN_PAUSE         = 0,
	TBN_FASTFORWARD,
	TBN_SETTINGS,
	TBN_SAVEGAME,
	TBN_SMALLMAP,
	TBN_TOWNDIRECTORY,
	TBN_SUBSIDIES,
	TBN_STATIONS,
	TBN_FINANCES,
	TBN_COMPANIES,
	TBN_GRAPHICS,
	TBN_LEAGUE,
	TBN_INDUSTRIES,
	TBN_VEHICLESTART,      ///< trains, actually.  So following are trucks, boats and planes
	TBN_TRAINS        = TBN_VEHICLESTART,
	TBN_ROADVEHS,
	TBN_SHIPS,
	TBN_AIRCRAFTS,
	TBN_ZOOMIN,
	TBN_ZOOMOUT,
	TBN_RAILS,
	TBN_ROADS,
	TBN_WATER,
	TBN_AIR,
	TBN_LANDSCAPE,
	TBN_MUSICSOUND,
	TBN_NEWSREPORT,
	TBN_HELP,
	TBN_SWITCHBAR,         ///< only available when toolbar has been split
	TBN_END                ///< The end marker
};

/** Widgets of the WC_MAIN_TOOLBAR, when in scenario editor. */
enum ToolbarScenEditorWidgets {
	TBSE_PAUSE        = 0,
	TBSE_FASTFORWARD,
	TBSE_SETTINGS,
	TBSE_SAVESCENARIO,
	TBSE_SPACERPANEL,
	TBSE_DATEPANEL,
	TBSE_DATEBACKWARD,
	TBSE_DATEFORWARD,
	TBSE_SMALLMAP,
	TBSE_ZOOMIN,
	TBSE_ZOOMOUT,
	TBSE_LANDGENERATE,
	TBSE_TOWNGENERATE,
	TBSE_INDUSTRYGENERATE,
	TBSE_BUILDROAD,
	TBSE_BUILDDOCKS,
	TBSE_PLANTTREES,
	TBSE_PLACESIGNS,
	TBSE_DATEPANEL_CONTAINER,
};

#endif /* WIDGETS_TOOLBAR_WIDGET_H */
