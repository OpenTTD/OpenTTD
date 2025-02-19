/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/* This file contains code to downgrade the API from 15 to 14. */

GSBridge.GetBridgeID <- GSBridge.GetBridgeType;

class GSCompat14 {
	function Text(text)
	{
		if (type(text) == "string") return text;
		if (type(text) == "instance" && text instanceof GSText) return text;
		return null;
	}
}

GSBaseStation._SetName <- GSBaseStation.SetName;
GSBaseStation.SetName <- function(id, name) { return GSBaseStation._SetName(id, GSCompat14.Text(name)); }

GSCompany._SetName <- GSCompany.SetName;
GSCompany.SetName <- function(name) { return GSCompany._SetName(GSCompat14.Text(name)); }
GSCompany._SetPresidentName <- GSCompany.SetPresidentName;
GSCompany.SetPresidentName <- function(name) { return GSCompany._SetPresidentName(GSCompat14.Text(name)); }

GSGoal._New <- GSGoal.New;
GSGoal.New <- function(company, goal, type, dest) { return GSGoal._New(company, GSCompat14.Text(goal), type, dest); }
GSGoal._SetText <- GSGoal.SetText;
GSGoal.SetText <- function(id, goal) { return GSGoal._SetText(id, GSCompat14.Text(goal)); }
GSGoal._SetProgress <- GSGoal.SetProgress;
GSGoal.SetProgress <- function(id, progress) { return GSGoal._SetProgress(id, GSCompat14.Text(progress)); }
GSGoal._Question <- GSGoal.Question;
GSGoal.Question <- function(id, company, question, type, buttons) { return GSGoal._Question(id, company, GSCompat14.Text(question), type, buttons); }
GSGoal._QuestionClient <- GSGoal.QuestionClient;
GSGoal.QuestionClient <- function(id, target, is_client, question, type, buttons) { return GSGoal._QuestionClient(id, target, is_client, GSCompat14.Text(question), type, buttons); }

GSGroup._SetName <- GSGroup.SetName;
GSGroup.SetName <- function(name) { return GSGroup._SetName(GSCompat14.Text(name)); }

GSIndustry._SetText <- GSIndustry.SetText;
GSIndustry.SetText <- function(id, text) { return GSIndustry._SetText(id, GSCompat14.Text(text)); }
GSIndustry._SetProductionLevel <- GSIndustry.SetProductionLevel;
GSIndustry.SetProductionLevel <- function(id, level, news, text) { return GSIndustry._SetProductionLevel(id, level, news, GSCompat14.Text(text)); }

GSLeagueTable._New <- GSLeagueTable.New;
GSLeagueTable.New <- function(title, header, footer) { return GSLeagueTable._New(GSCompat14.Text(title), GSCompat14.Text(header), GSCompat14.Text(footer)); }
GSLeagueTable._NewElement <- GSLeagueTable.NewElement;
GSLeagueTable.NewElement <- function(table, rating, company, text, score, type, target) { return GSLeagueTable._NewElement(table, rating, company, GSCompat14.Text(text), GSCompat14.Text(score), type, target); }
GSLeagueTable._UpdateElementData <- GSLeagueTable.UpdateElementData;
GSLeagueTable.UpdateElementData <- function(element, company, text, type, target) { return GSLeagueTable._UpdateElementData(element, company, GSCompat14.Text(text), type, target); }
GSLeagueTable._UpdateElementScore <- GSLeagueTable.UpdateElementScore;
GSLeagueTable.UpdateElementScore <- function(element, rating, score) { return GSLeagueTable._UpdateElementScore(element, rating, GSCompat14.Text(score)); }

GSNews._Create <- GSNews.Create;
GSNews.Create <- function(type, text, company, ref_type, ref) { return GSNews._Create(type, GSCompat14.Text(text), company, ref_type, ref); }

GSSign._BuildSign <- GSSign.BuildSign;
GSSign.BuildSign <- function(id, name) { return GSSign._BuildSign(id, GSCompat14.Text(name)); }

GSStoryPage._New <- GSStoryPage.New;
GSStoryPage.New <- function(company, title) { return GSStoryPage._New(company, GSCompat14.Text(title)); }
GSStoryPage._NewElement <- GSStoryPage.NewElement;
GSStoryPage.NewElement <- function(page, type, ref, text) { return GSStoryPage._NewElement(page, type, ref, GSCompat14.Text(text)); }
GSStoryPage._UpdateElement <- GSStoryPage.UpdateElement;
GSStoryPage.UpdateElement <- function(id, ref, text) { return GSStoryPage._UpdateElement(id, ref, GSCompat14.Text(text)); }
GSStoryPage._SetTitle <- GSStoryPage.SetTitle;
GSStoryPage.SetTitle <- function(page, tile) { return GSStoryPage._SetTitle(page, GSCompat14.Text(title)); }

GSTown._SetName <- GSTown.SetName;
GSTown.SetName <- function(id, name) { return GSTown._SetName(id, GSCompat14.Text(name)); }
GSTown._SetText <- GSTown.SetText;
GSTown.SetText <- function(id, text) { return GSTown._SetText(id, GSCompat14.Text(text)); }
GSTown._FoundTown <- GSTown.FoundTown;
GSTown.FoundTown <- function(tile, size, city, layout, name) { return GSTown._FoundTown(tile, size, city, layout, GSCompat14.Text(name)); }

GSVehicle._SetName <- GSVehicle.SetName;
GSVehicle.SetName <- function(id, name) { return GSVehicle._SetName(id, GSCompat14.Text(name)); }
