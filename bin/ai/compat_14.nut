/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/* This file contains code to downgrade the API from 15 to 14. */

AIBridge.GetBridgeID <- AIBridge.GetBridgeType;

class AICompat14 {
	function Text(text)
	{
		if (typeof text == "string") return text;
		return null;
	}
}

AIBaseStation.SetNameCompat14 <- AIBaseStation.SetName;
AIBaseStation.SetName <- function(id, name) { return AIBaseStation.SetNameCompat14(id, AICompat14.Text(name)); }

AICompany.SetNameCompat14 <- AICompany.SetName;
AICompany.SetName <- function(name) { return AICompany.SetNameCompat14(AICompat14.Text(name)); }
AICompany.SetPresidentNameCompat14 <- AICompany.SetPresidentName;
AICompany.SetPresidentName <- function(name) { return AICompany.SetPresidentNameCompat14(AICompat14.Text(name)); }

AIGroup.SetNameCompat14 <- AIGroup.SetName;
AIGroup.SetName <- function(id, name) { return AIGroup.SetNameCompat14(id, AICompat14.Text(name)); }

AISign.BuildSignCompat14 <- AISign.BuildSign;
AISign.BuildSign <- function(id, name) { return AISign.BuildSignCompat14(id, AICompat14.Text(name)); }

AITown.FoundTownCompat14 <- AITown.FoundTown;
AITown.FoundTown <- function(tile, size, city, layout, name) { return AITown.FoundTownCompat14(tile, size, city, layout, AICompat14.Text(name)); }

AIVehicle.SetNameCompat14 <- AIVehicle.SetName;
AIVehicle.SetName <- function(id, name) { return AIVehicle.SetNameCompat14(id, AICompat14.Text(name)); }

AIObject.constructorCompat14 <- AIObject.constructor;
foreach(name, object in CompatScriptRootTable) {
	if (type(object) != "class") continue;
	if (!object.rawin("constructor")) continue;
	if (object.constructor != AIObject.constructorCompat14) continue;
	object.constructor <- function() : (name) { AILog.Error("'" + name + "' is not instantiable"); }
}
