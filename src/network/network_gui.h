/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_gui.h GUIs related to networking. */

#ifndef NETWORK_GUI_H
#define NETWORK_GUI_H

#include "../company_type.h"
#include "../timer/timer_game_calendar.h"
#include "../economy_type.h"
#include "../window_type.h"
#include "network_type.h"
#include "network_gamelist.h"

void ShowNetworkNeedPassword(NetworkPasswordType npt);
void ShowNetworkChatQueryWindow(DestType type, int dest);
void ShowJoinStatusWindow();
void ShowNetworkGameWindow();
void ShowClientList();
void ShowNetworkCompanyPasswordWindow(Window *parent);
void ShowNetworkAskRelay(const std::string &server_connection_string, const std::string &relay_connection_string, const std::string &token);
void ShowNetworkAskSurvey();
void ShowSurveyResultTextfileWindow();

/** Company information stored at the client side */
struct NetworkCompanyInfo : NetworkCompanyStats {
	std::string company_name; ///< Company name
	TimerGameCalendar::Year inaugurated_year; ///< What year the company started in
	Money company_value;      ///< The company value
	Money money;              ///< The amount of money the company has
	Money income;             ///< How much did the company earn last year
	uint16_t performance;       ///< What was his performance last month?
	bool use_password;        ///< Is there a password
	std::string clients;      ///< The clients that control this company (Name1, name2, ..)
};

enum NetworkRelayWindowCloseData {
	NRWCD_UNHANDLED = 0, ///< Relay request is unhandled.
	NRWCD_HANDLED = 1, ///< Relay request is handled, either by user or by timeout.
};

#endif /* NETWORK_GUI_H */
