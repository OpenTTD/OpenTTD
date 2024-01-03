/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file survey_unix.cpp Unix implementation of OS-specific survey information. */

#include "../../stdafx.h"
#include "../../survey.h"

#include <sys/utsname.h>
#include <thread>
#include <unistd.h>

#include "../../safeguards.h"

void SurveyOS(nlohmann::json &json)
{
	struct utsname name;
	if (uname(&name) < 0) {
		json["os"] = "Unix";
		return;
	}

	json["os"] = name.sysname;
	json["release"] = name.release;
	json["machine"] = name.machine;
	json["version"] = name.version;

	long pages = sysconf(_SC_PHYS_PAGES);
	long page_size = sysconf(_SC_PAGE_SIZE);
	json["memory"] = SurveyMemoryToText(pages * page_size);
	json["hardware_concurrency"] = std::thread::hardware_concurrency();
}
