/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

GSLog.Info("1.3 API compatibility in effect.");

GSTown._SetGrowthRate <- GSTown.SetGrowthRate;
GSTown.SetGrowthRate <- function(town_id, days_between_town_growth)
{
	/* Growth rate 0 caused resetting the custom growth rate. While this was undocumented, it was used nevertheless (ofc). */
	if (days_between_town_growth == 0) days_between_town_growth = GSTown.TOWN_GROWTH_NORMAL;
	return GSTown._SetGrowthRate(town_id, days_between_town_growth);
}

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
