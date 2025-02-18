/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/* This file contains code to downgrade the API from 15 to 14. */

AIDate <- AIEconomyDate;
AIDate.IsValidDate <- function(date) { return AIDate(date).IsValid(); }
AIDate.GetCurrentDateCompat14 <- AIDate.GetCurrentDate;
AIDate.GetCurrentDate <- function() { return AIDate.GetCurrentDateCompat14().date(); }
AIDate.GetYearCompat14 <- AIDate.GetYear;
AIDate.GetYear <- function(date) { return AIDate(date).GetYearCompat14(); }
AIDate.GetMonthCompat14 <- AIDate.GetMonth;
AIDate.GetMonth <- function(date) { return AIDate(date).GetMonthCompat14(); }
AIDate.GetDayOfMonthCompat14 <- AIDate.GetDayOfMonth;
AIDate.GetDayOfMonth <- function(date) { return AIDate(date).GetDayOfMonthCompat14(); }
AIDate.GetDateCompat14 <- AIDate.GetDate;
AIDate.GetDate <- function(year, month, day) { return AIDate.GetDateCompat14(year, month, day).date(); }

AIBridge.GetBridgeID <- AIBridge.GetBridgeType;

class AICompat14 {
	function Text(text)
	{
		if (typeof text == "string") return text;
		return null;
	}
}

AIBaseStation.GetConstructionDateCompat14 <- AIBaseStation.GetConstructionDate;
AIBaseStation.GetConstructionDate <- function(station_id) { return AIBaseStation.GetConstructionDateCompat14(station_id).date(); }
AIBaseStation.SetNameCompat14 <- AIBaseStation.SetName;
AIBaseStation.SetName <- function(id, name) { return AIBaseStation.SetNameCompat14(id, AICompat14.Text(name)); }

AICompany.SetNameCompat14 <- AICompany.SetName;
AICompany.SetName <- function(name) { return AICompany.SetNameCompat14(AICompat14.Text(name)); }
AICompany.SetPresidentNameCompat14 <- AICompany.SetPresidentName;
AICompany.SetPresidentName <- function(name) { return AICompany.SetPresidentNameCompat14(AICompat14.Text(name)); }

AIEngine.GetDesignDateCompat14 <- AIEngine.GetDesignDate;
AIEngine.GetDesignDate <- function(engine_id) { return AIEngine.GetDesignDateCompat14(engine_id).date(); }

AIGroup.SetNameCompat14 <- AIGroup.SetName;
AIGroup.SetName <- function(id, name) { return AIGroup.SetNameCompat14(id, AICompat14.Text(name)); }

AISign.BuildSignCompat14 <- AISign.BuildSign;
AISign.BuildSign <- function(id, name) { return AISign.BuildSignCompat14(id, AICompat14.Text(name)); }

AISubsidy.GetExpireDateCompat14 <- AISubsidy.GetExpireDate;
AISubsidy.GetExpireDate <- function(subsidy_id) { return AISubsidy.GetExpireDateCompat14(subsidy_id).date(); }

AITown.FoundTownCompat14 <- AITown.FoundTown;
AITown.FoundTown <- function(tile, size, city, layout, name) { return AITown.FoundTownCompat14(tile, size, city, layout, AICompat14.Text(name)); }

AIVehicle.SetNameCompat14 <- AIVehicle.SetName;
AIVehicle.SetName <- function(id, name) { return AIVehicle.SetNameCompat14(id, AICompat14.Text(name)); }
