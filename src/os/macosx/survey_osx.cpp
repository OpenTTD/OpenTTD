/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file survey_osx.cpp OSX implementation of OS-specific survey information. */

#include "../../stdafx.h"

#include "../../3rdparty/fmt/format.h"
#include "../../survey.h"
#include "macos.h"

#include <mach-o/arch.h>
#include <thread>

#include "../../safeguards.h"

void SurveyOS(nlohmann::json &json)
{
	int ver_maj, ver_min, ver_bug;
	GetMacOSVersion(&ver_maj, &ver_min, &ver_bug);

	const NXArchInfo *arch = NXGetLocalArchInfo();

	json["os"] = "MacOS";
	json["release"] = fmt::format("{}.{}.{}", ver_maj, ver_min, ver_bug);
	json["machine"] = arch != nullptr ? arch->description : "unknown";
	json["min_ver"] = MAC_OS_X_VERSION_MIN_REQUIRED;
	json["max_ver"] = MAC_OS_X_VERSION_MAX_ALLOWED;

	json["memory"] = SurveyMemoryToText(MacOSGetPhysicalMemory());
	json["hardware_concurrency"] = std::thread::hardware_concurrency();
}
