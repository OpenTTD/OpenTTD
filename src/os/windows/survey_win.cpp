/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file survey_win.cpp Windows implementation of OS-specific survey information. */

#include "../../stdafx.h"

#include "../../3rdparty/fmt/format.h"
#include "../../survey.h"

#include <thread>
#include <windows.h>

#include "../../safeguards.h"

void SurveyOS(nlohmann::json &json)
{
	_OSVERSIONINFOA os;
	os.dwOSVersionInfoSize = sizeof(os);
	GetVersionExA(&os);

	json["os"] = "Windows";
	json["release"] = fmt::format("{}.{}.{} ({})", os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber, os.szCSDVersion);

	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);

	json["memory"] = SurveyMemoryToText(status.ullTotalPhys);
	json["hardware_concurrency"] = std::thread::hardware_concurrency();
}
