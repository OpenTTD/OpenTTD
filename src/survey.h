/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file survey.h Functions to survey the current game / system, for crashlog and network-survey. */

#ifndef SURVEY_H
#define SURVEY_H

#include "3rdparty/nlohmann/json.hpp"

std::string SurveyMemoryToText(uint64_t memory);

void SurveyCompanies(nlohmann::json &survey);
void SurveyCompiler(nlohmann::json &survey);
void SurveyConfiguration(nlohmann::json &survey);
void SurveyFont(nlohmann::json &survey);
void SurveyGameScript(nlohmann::json &survey);
void SurveyGrfs(nlohmann::json &survey);
void SurveyLibraries(nlohmann::json &survey);
void SurveyOpenTTD(nlohmann::json &survey);
void SurveySettings(nlohmann::json &survey, bool skip_if_default);
void SurveyTimers(nlohmann::json &survey);

/* Defined in os/<os>/survey_<os>.cpp. */
void SurveyOS(nlohmann::json &json);

#endif /* SURVEY_H */
