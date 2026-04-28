/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/* This file contains code to downgrade the API from 16 to 15. */

GSDate <- GSEconomyDate;
GSDate.IsValidDate <- function(date) { return GSDate(date).IsValid(); }
GSDate.GetCurrentDateCompat15 <- GSDate.GetCurrentDate;
GSDate.GetCurrentDate <- function() { return GSDate.GetCurrentDateCompat15().date(); }
GSDate.GetYearCompat15 <- GSDate.GetYear;
GSDate.GetYear <- function(date) { return GSDate(date).GetYearCompat15(); }
GSDate.GetMonthCompat15 <- GSDate.GetMonth;
GSDate.GetMonth <- function(date) { return GSDate(date).GetMonthCompat15(); }
GSDate.GetDayOfMonthCompat15 <- GSDate.GetDayOfMonth;
GSDate.GetDayOfMonth <- function(date) { return GSDate(date).GetDayOfMonthCompat15(); }
GSDate.GetDateCompat15 <- GSDate.GetDate;
GSDate.GetDate <- function(year, month, day) { return GSDate.GetDateCompat15(year, month, day).date(); }

GSBaseStation.GetConstructionDateCompat15 <- GSBaseStation.GetConstructionDate;
GSBaseStation.GetConstructionDate <- function(station_id) { return GSBaseStation.GetConstructionDateCompat15(station_id).date(); }

GSClient.GetJoinDateCompat15 <- GSClient.GetJoinDate;
GSClient.GetJoinDate <- function(client_id) { return GSClient.GetJoinDateCompat15(client_id).date(); }

GSEngine.GetDesignDateCompat15 <- GSEngine.GetDesignDate;
GSEngine.GetDesignDate <- function(engine_id) { return GSEngine.GetDesignDateCompat15(engine_id).date(); }

GSIndustry.GetConstructionDateCompat15 <- GSIndustry.GetConstructionDate;
GSIndustry.GetConstructionDate <- function(industry_id) { return GSIndustry.GetConstructionDateCompat15(industry_id).date(); }
GSIndustry.GetCargoLastAcceptedDateCompat15 <- GSIndustry.GetCargoLastAcceptedDate;
GSIndustry.GetCargoLastAcceptedDate <- function(industry_id, cargo_type) { return GSIndustry.GetCargoLastAcceptedDateCompat15(industry_id, cargo_type).date(); }

GSStoryPage.GetDateCompat15 <- GSStoryPage.GetDate;
GSStoryPage.GetDate <- function(story_page_id) { return GSStoryPage.GetDateCompat15(story_page_id).date(); }
GSStoryPage.SetDateCompat15 <- GSStoryPage.SetDate;
GSStoryPage.SetDate <- function(story_page_id, date) { return GSStoryPage.SetDateCompat15(story_page_id, GSCalendarDate(date)); }

GSSubsidy.GetExpireDateCompat15 <- GSSubsidy.GetExpireDate;
GSSubsidy.GetExpireDate <- function(subsidy_id) { return GSSubsidy.GetExpireDateCompat15(subsidy_id).date(); }
