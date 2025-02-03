/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

AILog.Info("0.7 API compatibility in effect:");
AILog.Info(" - AITown::GetLastMonthProduction's behaviour has slightly changed.");
AILog.Info(" - AISubsidy::GetDestination returns STATION_INVALID for awarded subsidies.");
AILog.Info(" - AISubsidy::GetSource returns STATION_INVALID for awarded subsidies.");

require("compat.nut")
AICompatibility.Add(0, 7)
