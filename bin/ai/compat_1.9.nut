/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

AILog.Info("1.9 API compatibility in effect.");

/* 13 really checks RoadType against RoadType */
AIRoad._HasRoadType <- AIRoad.HasRoadType;
AIRoad.HasRoadType <- function(tile, road_type)
{
	local list = AIRoadTypeList(AIRoad.GetRoadTramType(road_type));
	foreach (rt, _ in list) {
		if (AIRoad._HasRoadType(tile, rt)) {
			return true;
		}
	}
	return false;
}

/* 14 splits date into calendar and economy time. Economy time ticks at the usual rate, but calendar time can be sped up, slowed down, or frozen entirely.
 * For maximum compatibility, all ScriptDate calls go to TimerGameEcomomy. */
AIDate <- AIDateEconomy;

AIBaseStation._GetConstructionDate <- AIBaseStation.GetConstructionDate;
AIBaseStation.GetConstructionDate <- function(station_id)
{
	if (!IsValidBaseStation(station_id)) return ScriptDateEconomy::DATE_INVALID;

	return (ScriptDateEconomy::Date)::BaseStation::Get(station_id)->build_date.base();
}

AIEngine._GetDesignDate <- AIScriptEngine.GetDesignDate;
AIEngine.GetDesignDate <- function(engine_id)
{
	if (!IsValidEngine(engine_id)) return ScriptDateEconomy::DATE_INVALID;

	return (ScriptDateEconomy::Date)::Engine::Get(engine_id)->intro_date.base();
}
