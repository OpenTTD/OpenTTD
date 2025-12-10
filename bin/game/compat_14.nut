/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/* This file contains code to downgrade the API from 15 to 14. */

GSBridge.GetBridgeID <- GSBridge.GetBridgeType;

/* Emulate old GSText parameter padding behaviour */
GSText.SCRIPT_TEXT_MAX_PARAMETERS <- 20;

class GSCompat14 {
	function Text(text)
	{
		if (typeof text == "string") return text;
		if (typeof text == "instance" && text instanceof GSText) return text;
		return null;
	}
}

GSBaseStation.SetNameCompat14 <- GSBaseStation.SetName;
GSBaseStation.SetName <- function(id, name) { return GSBaseStation.SetNameCompat14(id, GSCompat14.Text(name)); }

GSCompany.SetNameCompat14 <- GSCompany.SetName;
GSCompany.SetName <- function(name) { return GSCompany.SetNameCompat14(GSCompat14.Text(name)); }
GSCompany.SetPresidentNameCompat14 <- GSCompany.SetPresidentName;
GSCompany.SetPresidentName <- function(name) { return GSCompany.SetPresidentNameCompat14(GSCompat14.Text(name)); }

GSGoal.NewCompat14 <- GSGoal.New;
GSGoal.New <- function(company, goal, type, dest) { return GSGoal.NewCompat14(company, GSCompat14.Text(goal), type, dest); }
GSGoal.SetTextCompat14 <- GSGoal.SetText;
GSGoal.SetText <- function(id, goal) { return GSGoal.SetTextCompat14(id, GSCompat14.Text(goal)); }
GSGoal.SetProgressCompat14 <- GSGoal.SetProgress;
GSGoal.SetProgress <- function(id, progress) { return GSGoal.SetProgressCompat14(id, GSCompat14.Text(progress)); }
GSGoal.QuestionCompat14 <- GSGoal.Question;
GSGoal.Question <- function(id, company, question, type, buttons) { return GSGoal.QuestionCompat14(id, company, GSCompat14.Text(question), type, buttons); }
GSGoal.QuestionClientCompat14 <- GSGoal.QuestionClient;
GSGoal.QuestionClient <- function(id, target, is_client, question, type, buttons) { return GSGoal.QuestionClientCompat14(id, target, is_client, GSCompat14.Text(question), type, buttons); }

GSGroup.SetNameCompat14 <- GSGroup.SetName;
GSGroup.SetName <- function(id, name) { return GSGroup.SetNameCompat14(id, GSCompat14.Text(name)); }

GSIndustry.SetTextCompat14 <- GSIndustry.SetText;
GSIndustry.SetText <- function(id, text) { return GSIndustry.SetTextCompat14(id, GSCompat14.Text(text)); }
GSIndustry.SetProductionLevelCompat14 <- GSIndustry.SetProductionLevel;
GSIndustry.SetProductionLevel <- function(id, level, news, text) { return GSIndustry.SetProductionLevelCompat14(id, level, news, GSCompat14.Text(text)); }

GSLeagueTable.NewCompat14 <- GSLeagueTable.New;
GSLeagueTable.New <- function(title, header, footer) { return GSLeagueTable.NewCompat14(GSCompat14.Text(title), GSCompat14.Text(header), GSCompat14.Text(footer)); }
GSLeagueTable.NewElementCompat14 <- GSLeagueTable.NewElement;
GSLeagueTable.NewElement <- function(table, rating, company, text, score, type, target) { return GSLeagueTable.NewElementCompat14(table, rating, company, GSCompat14.Text(text), GSCompat14.Text(score), type, target); }
GSLeagueTable.UpdateElementDataCompat14 <- GSLeagueTable.UpdateElementData;
GSLeagueTable.UpdateElementData <- function(element, company, text, type, target) { return GSLeagueTable.UpdateElementDataCompat14(element, company, GSCompat14.Text(text), type, target); }
GSLeagueTable.UpdateElementScoreCompat14 <- GSLeagueTable.UpdateElementScore;
GSLeagueTable.UpdateElementScore <- function(element, rating, score) { return GSLeagueTable.UpdateElementScoreCompat14(element, rating, GSCompat14.Text(score)); }

GSNews.CreateCompat14 <- GSNews.Create;
GSNews.Create <- function(type, text, company, ref_type, ref) { return GSNews.CreateCompat14(type, GSCompat14.Text(text), company, ref_type, ref); }

GSSign.BuildSignCompat14 <- GSSign.BuildSign;
GSSign.BuildSign <- function(id, name) { return GSSign.BuildSignCompat14(id, GSCompat14.Text(name)); }

GSStoryPage.NewCompat14 <- GSStoryPage.New;
GSStoryPage.New <- function(company, title) { return GSStoryPage.NewCompat14(company, GSCompat14.Text(title)); }
GSStoryPage.NewElementCompat14 <- GSStoryPage.NewElement;
GSStoryPage.NewElement <- function(page, type, ref, text) { return GSStoryPage.NewElementCompat14(page, type, ref, GSCompat14.Text(text)); }
GSStoryPage.UpdateElementCompat14 <- GSStoryPage.UpdateElement;
GSStoryPage.UpdateElement <- function(id, ref, text) { return GSStoryPage.UpdateElementCompat14(id, ref, GSCompat14.Text(text)); }
GSStoryPage.SetTitleCompat14 <- GSStoryPage.SetTitle;
GSStoryPage.SetTitle <- function(page, tile) { return GSStoryPage.SetTitleCompat14(page, GSCompat14.Text(title)); }

GSTown.SetNameCompat14 <- GSTown.SetName;
GSTown.SetName <- function(id, name) { return GSTown.SetNameCompat14(id, GSCompat14.Text(name)); }
GSTown.SetTextCompat14 <- GSTown.SetText;
GSTown.SetText <- function(id, text) { return GSTown.SetTextCompat14(id, GSCompat14.Text(text)); }
GSTown.FoundTownCompat14 <- GSTown.FoundTown;
GSTown.FoundTown <- function(tile, size, city, layout, name) { return GSTown.FoundTownCompat14(tile, size, city, layout, GSCompat14.Text(name)); }

GSVehicle.SetNameCompat14 <- GSVehicle.SetName;
GSVehicle.SetName <- function(id, name) { return GSVehicle.SetNameCompat14(id, GSCompat14.Text(name)); }

GSObject.constructorCompat14 <- GSObject.constructor;
foreach(name, object in CompatScriptRootTable) {
	if (type(object) != "class") continue;
	if (!object.rawin("constructor")) continue;
	if (object.constructor != GSObject.constructorCompat14) continue;
	object.constructor <- function() : (name) { GSLog.Error("'" + name + "' is not instantiable"); }
}
