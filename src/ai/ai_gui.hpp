/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_gui.hpp Window for configuring the AIs  */

#ifndef AI_GUI_HPP
#define AI_GUI_HPP

#include "../company_type.h"

#ifdef ENABLE_AI
void ShowAIDebugWindow(CompanyID show_company = INVALID_COMPANY);
void ShowAIConfigWindow();
#else
#include "table/strings.h"

static inline void ShowAIConfigWindow()
{
	ShowErrorMessage(STR_ERROR_NO_AI, STR_ERROR_NO_AI_SUB, WL_INFO);
}
static inline void ShowAIDebugWindow(CompanyID show_company = INVALID_COMPANY) {ShowAIConfigWindow();}
#endif /* ENABLE_AI */

#endif /* AI_GUI_HPP */
