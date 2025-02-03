/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

GSTown._SetGrowthRate <- GSTown.SetGrowthRate;
GSTown.SetGrowthRate <- function(town_id, days_between_town_growth)
{
	/* Growth rate 0 caused resetting the custom growth rate. While this was undocumented, it was used nevertheless (ofc). */
	if (days_between_town_growth == 0) days_between_town_growth = GSTown.TOWN_GROWTH_NORMAL;
	return GSTown._SetGrowthRate(town_id, days_between_town_growth);
}

require("compat_1.4.nut")
