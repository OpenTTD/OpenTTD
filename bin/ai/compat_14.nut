/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/* This file contains code to downgrade the API from 15 to 14. */

AIBridge.GetBridgeID <- AIBridge.GetBridgeType;

AIBaseStation._GetConstructionDate <- AIBaseStation.GetConstructionDate;
AIBaseStation.GetConstructionDate <- function(station_id)
{
	return AIBaseStation._GetConstructionDate(station_id).GetValue();
}

AIDate.IsValidDate <- function(date)
{
	return AIDate(date, AIDate.DT_ECONOMY).IsValid(AIDate.DT_ECONOMY);
}

AIDate._GetCurrentDate <- AIDate.GetCurrentDate;
AIDate.GetCurrentDate <- function()
{
	return AIDate._GetCurrentDate(false).GetValue();
}

AIDate._GetYear <- AIDate.GetYear;
AIDate.GetYear <- function(date)
{
	return AIDate(date, AIDate.DT_ECONOMY)._GetYear();
}

AIDate._GetMonth <- AIDate.GetMonth;
AIDate.GetMonth <- function(date)
{
	return AIDate(date, AIDate.DT_ECONOMY)._GetMonth();
}

AIDate._GetDayOfMonth <- AIDate.GetDayOfMonth;
AIDate.GetDayOfMonth <- function(date)
{
	return AIDate(date, AIDate.DT_ECONOMY)._GetYear();
}

AIDate._GetDate <- AIDate.GetDate;
AIDate.GetDate <- function(year, month, day)
{
	return AIDate._GetDate(year, month, day, false).GetValue();
}

AIEngine._GetDesignDate <- AIEngine.GetDesignDate;
AIEngine.GetDesignDate <- function(engine_id)
{
	return AIEngine._GetDesignDate(engine_id).GetValue();
}

AISubsidy._GetExpireDate <- AISubsidy.GetExpireDate;
AISubsidy.GetExpireDate <- function(subsidy_id)
{
	return AISubsidy._GetExpireDate(subsidy_id).GetValue();
}
