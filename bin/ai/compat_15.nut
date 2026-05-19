/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/* This file contains code to downgrade the API from 16 to 15. */

AIDate <- AIEconomyDate;
AIDate.IsValidDate <- function(date) { return AIDate(date).IsValid(); }
AIDate.GetCurrentDateCompat15 <- AIDate.GetCurrentDate;
AIDate.GetCurrentDate <- function() { return AIDate.GetCurrentDateCompat15().date(); }
AIDate.GetYearCompat15 <- AIDate.GetYear;
AIDate.GetYear <- function(date) { return AIDate(date).GetYearCompat15(); }
AIDate.GetMonthCompat15 <- AIDate.GetMonth;
AIDate.GetMonth <- function(date) { return AIDate(date).GetMonthCompat15(); }
AIDate.GetDayOfMonthCompat15 <- AIDate.GetDayOfMonth;
AIDate.GetDayOfMonth <- function(date) { return AIDate(date).GetDayOfMonthCompat15(); }
AIDate.GetDateCompat15 <- AIDate.GetDate;
AIDate.GetDate <- function(year, month, day) { return AIDate.GetDateCompat15(year, month, day).date(); }

AIBaseStation.GetConstructionDateCompat15 <- AIBaseStation.GetConstructionDate;
AIBaseStation.GetConstructionDate <- function(station_id) { return AIBaseStation.GetConstructionDateCompat15(station_id).date(); }

AIEngine.GetDesignDateCompat15 <- AIEngine.GetDesignDate;
AIEngine.GetDesignDate <- function(engine_id) { return AIEngine.GetDesignDateCompat15(engine_id).date(); }

AISubsidy.GetExpireDateCompat15 <- AISubsidy.GetExpireDate;
AISubsidy.GetExpireDate <- function(subsidy_id) { return AISubsidy.GetExpireDateCompat15(subsidy_id).date(); }
