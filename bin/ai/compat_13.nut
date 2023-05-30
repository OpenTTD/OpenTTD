/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

AILog.Info("13 API compatibility in effect.");

/* 14.0 removes small and medium advertise options */
AITown.TOWN_ACTION_ADVERTISE_SMALL <- AITown.TOWN_ACTION_ADVERTISE
AITown.TOWN_ACTION_ADVERTISE_MEDIUM <- AITown.TOWN_ACTION_ADVERTISE
AITown.TOWN_ACTION_ADVERTISE_LARGE <- AITown.TOWN_ACTION_ADVERTISE
