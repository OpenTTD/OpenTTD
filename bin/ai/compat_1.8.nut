/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/* This file contains code to downgrade the API from 1.9 to 1.8. */

AIBridge.GetNameCompat1_8 <- AIBridge.GetName;
AIBridge.GetName <- function(bridge_id)
{
	return AIBridge.GetNameCompat1_8(bridge_id, AIVehicle.VT_RAIL);
}

AIGroup.CreateGroupCompat1_8 <- AIGroup.CreateGroup;
AIGroup.CreateGroup <- function(vehicle_type)
{
	return AIGroup.CreateGroupCompat1_8(vehicle_type, AIGroup.GROUP_INVALID);
}
