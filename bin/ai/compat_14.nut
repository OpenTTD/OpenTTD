/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/* This file contains code to downgrade the API from 15 to 14. */

AIBridge.GetBridgeID <- AIBridge.GetBridgeType;

class AICompat14 {
	function Text(text)
	{
		if (type(text) == "string") return text;
		return null;
	}
}

AIBaseStation._SetName <- AIBaseStation.SetName;
AIBaseStation.SetName <- function(id, name) { return AIBaseStation._SetName(id, AICompat14.Text(name)); }

AICompany._SetName <- AICompany.SetName;
AICompany.SetName <- function(name) { return AICompany._SetName(AICompat14.Text(name)); }
AICompany._SetPresidentName <- AICompany.SetPresidentName;
AICompany.SetPresidentName <- function(name) { return AICompany._SetPresidentName(AICompat14.Text(name)); }

AIGroup._SetName <- AIGroup.SetName;
AIGroup.SetName <- function(id, name) { return AIGroup._SetName(id, AICompat14.Text(name)); }

AISign._BuildSign <- AISign.BuildSign;
AISign.BuildSign <- function(id, name) { return AISign._BuildSign(id, AICompat14.Text(name)); }

AITown._FoundTown <- AITown.FoundTown;
AITown.FoundTown <- function(tile, size, city, layout, name) { return AITown._FoundTown(tile, size, city, layout, AICompat14.Text(name)); }

AIVehicle._SetName <- AIVehicle.SetName;
AIVehicle.SetName <- function(id, name) { return AIVehicle._SetName(id, AICompat14.Text(name)); }

AIDate <- AIEconomyDate;
AIDate.IsValidDate <- function(date) { return AIDate(date).IsValid(); }
AIDate._GetCurrentDate <- AIDate.GetCurrentDate;
AIDate.GetCurrentDate <- function() { return AIDate._GetCurrentDate().date(); }
AIDate._GetYear <- AIDate.GetYear;
AIDate.GetYear <- function(date) { return AIDate(date)._GetYear(); }
AIDate._GetMonth <- AIDate.GetMonth;
AIDate.GetMonth <- function(date) { return AIDate(date)._GetMonth(); }
AIDate._GetDayOfMonth <- AIDate.GetDayOfMonth;
AIDate.GetDayOfMonth <- function(date) { return AIDate(date)._GetDayOfMonth(); }
AIDate._GetDate <- AIDate.GetDate;
AIDate.GetDate <- function(year, month, day) { return AIDate._GetDate(year, month, day, false).date(); }

AIBaseStation._GetConstructionDate <- AIBaseStation.GetConstructionDate;
AIBaseStation.GetConstructionDate <- function(station_id) { return AIBaseStation._GetConstructionDate(station_id).date(); }

AIEngine._GetDesignDate <- AIEngine.GetDesignDate;
AIEngine.GetDesignDate <- function(engine_id) { return AIEngine._GetDesignDate(engine_id).date(); }

AISubsidy._GetExpireDate <- AISubsidy.GetExpireDate;
AISubsidy.GetExpireDate <- function(subsidy_id) { return AISubsidy._GetExpireDate(subsidy_id).date(); }
