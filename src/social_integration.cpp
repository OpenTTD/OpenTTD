/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file social_integration.cpp Base implementation of social integration support. */

#include "stdafx.h"

#include "social_integration.h"
#include "3rdparty/openttd_social_integration_api/openttd_social_integration_api.h"

#include "debug.h"
#include "fileio_func.h"
#include "library_loader.h"
#include "rev.h"
#include "string_func.h"

#include "safeguards.h"

/** Helper for scanning for files with SocialIntegration as extension */
class SocialIntegrationFileScanner : FileScanner {
public:
	void Scan()
	{
#ifdef _WIN32
		std::string extension = "-social.dll";
#elif defined(__APPLE__)
		std::string extension = "-social.dylib";
#else
		std::string extension = "-social.so";
#endif

		this->FileScanner::Scan(extension.c_str(), SOCIAL_INTEGRATION_DIR, false);
	}

	bool AddFile(const std::string &filename, size_t basepath_length, const std::string &) override
	{
		std::string basepath = filename.substr(basepath_length);
		Debug(misc, 1, "[Social Integration: {}] Loading ...", basepath);
	}
};

void SocialIntegration::Initialize()
{
	SocialIntegrationFileScanner fs;
	fs.Scan();
}
