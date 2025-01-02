/*
 * This file is part of OpenTTD.
 *
 * OpenTTD is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, version 2.
 *
 * OpenTTD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file company_gui.h
 * @brief GUI Functions and utilities related to managing and displaying
 *        companies and their associated assets in OpenTTD.
 *
 * This header provides a set of functions for drawing company icons,
 * displaying company-specific windows (finances, station lists, etc.),
 * and invalidating or closing GUI elements when needed. It draws upon
 * fundamental data structures like #CompanyID, #GroupID, and game-level
 * constructs for rendering.
 */

#ifndef COMPANY_GUI_H
#define COMPANY_GUI_H

#include "company_type.h"
#include "group.h"
#include "gfx_type.h"

/**
 * Retrieve a text color suitable for rendering strings associated with
 * a given company. This color is typically used to distinguish ownership
 * or branding visually in GUIs or tooltips.
 *
 * @param company The ID of the company to derive text color from.
 * @return An appropriate #TextColour for the specified company.
 */
TextColour GetDrawStringCompanyColour(CompanyID company);

/**
 * Render a company’s icon at a specified coordinate on the screen.
 *
 * @param c The ID of the company whose icon will be drawn.
 * @param x The X-coordinate where the icon should be placed.
 * @param y The Y-coordinate where the icon should be placed.
 */
void DrawCompanyIcon(CompanyID c, int x, int y);

/**
 * Show the window that allows customization or viewing
 * of a company's livery. This may relate to vehicle or station
 * color schemes, logos, etc.
 *
 * @param company The ID of the company in question.
 * @param group   The group for which this livery applies (e.g., default group).
 */
void ShowCompanyLiveryWindow(CompanyID company, GroupID group);

/**
 * Display a station list window for the specified company.
 *
 * @param company The ID of the company whose station list should be displayed.
 */
void ShowCompanyStations(CompanyID company);

/**
 * Bring up the company finances window for inspection of
 * income, expenses, loans, and other monetary details.
 *
 * @param company The ID of the company whose finances are shown.
 */
void ShowCompanyFinances(CompanyID company);

/**
 * Open a general information window for the company, potentially
 * including name, management, performance rating, etc.
 *
 * @param company The ID of the company for which to open the info window.
 */
void ShowCompany(CompanyID company);

/**
 * Invalidate the windows belonging to a given company to
 * trigger a re-draw or refresh event. This is especially useful when
 * crucial data (ownership, color scheme, etc.) has changed.
 *
 * @param c Pointer to the Company object to invalidate windows for.
 */
void InvalidateCompanyWindows(const Company *c);

/**
 * Close all GUI windows associated with a specified company.
 *
 * @param company The ID of the company whose windows should be closed.
 */
void CloseCompanyWindows(CompanyID company);

/**
 * Mark infrastructure-related windows of a company as 'dirty'
 * so they get refreshed or redrawn. This is useful if tracks,
 * stations, or depots have changed in a way that affects the display.
 *
 * @param company The ID of the company to be updated.
 */
void DirtyCompanyInfrastructureWindows(CompanyID company);

#endif /* COMPANY_GUI_H */
