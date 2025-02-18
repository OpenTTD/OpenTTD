/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/* This file contains code to downgrade the API from 15 to 14. */

GSBridge.GetBridgeID <- GSBridge.GetBridgeType;

GSBaseStation._GetConstructionDate <- GSBaseStation.GetConstructionDate;
GSBaseStation.GetConstructionDate <- function(station_id)
{
	return GSBaseStation._GetConstructionDate(station_id).GetValue();
}

GSDate.IsValidDate <- function(date)
{
	return GSDate(date, GSDate.DT_ECONOMY).IsValid(GSDate.DT_ECONOMY);
}

GSDate._GetCurrentDate <- GSDate.GetCurrentDate;
GSDate.GetCurrentDate <- function()
{
	return GSDate._GetCurrentDate(false).GetValue();
}

GSDate._GetYear <- GSDate.GetYear;
GSDate.GetYear <- function(date)
{
	return GSDate(date, GSDate.DT_ECONOMY)._GetYear();
}

GSDate._GetMonth <- GSDate.GetMonth;
GSDate.GetMonth <- function(date)
{
	return GSDate(date, GSDate.DT_ECONOMY)._GetMonth();
}

GSDate._GetDayOfMonth <- GSDate.GetDayOfMonth;
GSDate.GetDayOfMonth <- function(date)
{
	return GSDate(date, GSDate.DT_ECONOMY)._GetYear();
}

GSDate._GetDate <- GSDate.GetDate;
GSDate.GetDate <- function(year, month, day)
{
	return GSDate._GetDate(year, month, day, false).GetValue();
}

GSDate._GetSystemTime <- GSDate.GetSystemTime;
GSDate.GetSystemTime <- function()
{
	return GSDate._GetSystemTime().GetValue();
}

GSEngine._GetDesignDate <- GSEngine.GetDesignDate;
GSEngine.GetDesignDate <- function(engine_id)
{
	return GSEngine._GetDesignDate(engine_id).GetValue();
}

GSIndustry._GetConstructionDate <- GSIndustry.GetConstructionDate;
GSIndustry.GetConstructionDate <- function(industry_id)
{
	return GSIndustry._GetConstructionDate(industry_id).GetValue();
}

GSIndustry._GetCargoLastAcceptedDate <- GSIndustry.GetCargoLastAcceptedDate;
GSIndustry.GetCargoLastAcceptedDate <- function(industry_id, cargo_type)
{
	return GSIndustry._GetCargoLastAcceptedDate(industry_id, cargo_type).GetValue();
}

GSStoryPage._GetDate <- GSStoryPage.GetDate;
GSStoryPage.GetDate <- function(story_page_id)
{
	return GSStoryPage._GetDate(story_page_id).GetValue();
}

GSStoryPage._SetDate <- GSStoryPage.SetDate;
GSStoryPage.SetDate <- function(story_page_id, date)
{
	return GSStoryPage._SetDate(story_page_id, GSDate(date, GSDate.DT_CALENDAR));
}

GSSubsidy._GetExpireDate <- GSSubsidy.GetExpireDate;
GSSubsidy.GetExpireDate <- function(subsidy_id)
{
	return GSSubsidy._GetExpireDate(subsidy_id).GetValue();
}
