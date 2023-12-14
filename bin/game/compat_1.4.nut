/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

GSLog.Info("1.4 API compatibility in effect.");

/* 1.5 adds a game element reference to the news. */
GSNews._Create <- GSNews.Create;
GSNews.Create <- function(type, text, company)
{
    return GSNews._Create(type, text, company, GSNews.NR_NONE, 0);
}

/* 1.9 adds a vehicle type parameter. */
GSBridge._GetName <- GSBridge.GetName;
GSBridge.GetName <- function(bridge_id)
{
	return GSBridge._GetName(bridge_id, GSVehicle.VT_RAIL);
}

/* 1.11 adds a tile parameter. */
GSCompany._ChangeBankBalance <- GSCompany.ChangeBankBalance;
GSCompany.ChangeBankBalance <- function(company, delta, expenses_type)
{
	return GSCompany._ChangeBankBalance(company, delta, expenses_type, GSMap.TILE_INVALID);
}

/* 13 really checks RoadType against RoadType */
GSRoad._HasRoadType <- GSRoad.HasRoadType;
GSRoad.HasRoadType <- function(tile, road_type)
{
	local list = GSRoadTypeList(GSRoad.GetRoadTramType(road_type));
	foreach (rt, _ in list) {
		if (GSRoad._HasRoadType(tile, rt)) {
			return true;
		}
	}
	return false;
}

/* 14 splits date into calendar and economy time. Economy time ticks at the usual rate, but calendar time can be sped up, slowed down, or frozen entirely.
 * This may break some scripts, but for maximum compatibility, all ScriptDate calls go to TimerGameEcomomy. */
GSDate <- GSDateEconomy;

GSBaseStation._GetConstructionDate <- GSBaseStation.GetConstructionDate;
GSBaseStation.GetConstructionDate <- function(station_id)
{
	if (!IsValidBaseStation(station_id)) return ScriptDateEconomy::DATE_INVALID;

	return (ScriptDateEconomy::Date)::BaseStation::Get(station_id)->build_date.base();
}

GSClient._GetJoinDate <- GSClient.GetJoinDate;
GSClient.GetJoinDate <- function(client)
{
	NetworkClientInfo *ci = FindClientInfo(client);
	if (ci == nullptr) return ScriptDateEconomy::DATE_INVALID;
	return (ScriptDateEconomy::Date)ci->join_date.base();
}

GSEngine._GetDesignDate <- GSEngine.GetDesignDate;
GSEngine.GetDesignDate <- function(engine_id)
{
	if (!IsValidEngine(engine_id)) return ScriptDateEconomy::DATE_INVALID;

	return (ScriptDateEconomy::Date)::Engine::Get(engine_id)->intro_date.base();
}

GSIndustry._GetConstructionDate <- GSIndustry.GetConstructionDate;
GSIndustry.GetConstructionDate <- function(industry_id)
{
	Industry *i = Industry::GetIfValid(industry_id);
	if (i == nullptr) return ScriptDateEconomy::DATE_INVALID;
	return (ScriptDateEconomy::Date)i->construction_date.base();
}

GSStoryPage._GetDate <- GSStoryPage.GetDate;
GSStoryPage.GetDate <- function(story_page_id)
{
	EnforcePrecondition(ScriptDateEconomy::DATE_INVALID, IsValidStoryPage(story_page_id));
	EnforceDeityMode(ScriptDateEconomy::DATE_INVALID);
	return (ScriptDateEconomy::Date)StoryPage::Get(story_page_id)->date.base();
}

GSStoryPage._SetDate <- GSStoryPage.SetDate;
GSStoryPage.SetDate <- function(story_page_id, date)
{
	EnforcePrecondition(false, IsValidStoryPage(story_page_id));
	EnforceDeityMode(false);
	return ScriptObject::Command<CMD_SET_STORY_PAGE_DATE>::Do(story_page_id, date);
}
