/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

 /** @file script_gui.hpp %Window for configuring the scripts  */

#ifndef SCRIPT_GUI_HPP
#define SCRIPT_GUI_HPP

#include "../company_type.h"
#include "../textfile_type.h"

void ShowScriptListWindow(CompanyID slot, bool show_all);
Window *ShowScriptDebugWindow(CompanyID show_company = INVALID_COMPANY, bool new_window = false);
void ShowScriptSettingsWindow(CompanyID slot);
void ShowScriptTextfileWindow(TextfileType file_type, CompanyID slot);
void ShowScriptDebugWindowIfScriptError();
void InitializeScriptGui();

#endif /* SCRIPT_GUI_HPP */
