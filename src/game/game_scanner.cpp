/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_scanner.cpp allows scanning Game scripts */

#include "../stdafx.h"

#include "../script/squirrel_class.hpp"
#include "game_info.hpp"
#include "game_scanner.hpp"

#include "../safeguards.h"


void GameScannerInfo::Initialize()
{
	ScriptScanner::Initialize("GSScanner");
}

void GameScannerInfo::GetScriptName(ScriptInfo *info, char *name, const char *last)
{
	seprintf(name, last, "%s", info->GetName());
}

void GameScannerInfo::RegisterAPI(class Squirrel *engine)
{
	GameInfo::RegisterAPI(engine);
}

GameInfo *GameScannerInfo::FindInfo(const char *nameParam, int versionParam, bool force_exact_match)
{
	if (this->info_list.size() == 0) return NULL;
	if (nameParam == NULL) return NULL;

	char game_name[1024];
	strecpy(game_name, nameParam, lastof(game_name));
	strtolower(game_name);

	GameInfo *info = NULL;
	int version = -1;

	if (versionParam == -1) {
		/* We want to load the latest version of this Game script; so find it */
		if (this->info_single_list.find(game_name) != this->info_single_list.end()) return static_cast<GameInfo *>(this->info_single_list[game_name]);

		/* If we didn't find a match Game script, maybe the user included a version */
		char *e = strrchr(game_name, '.');
		if (e == NULL) return NULL;
		*e = '\0';
		e++;
		versionParam = atoi(e);
		/* Continue like we were calling this function with a version. */
	}

	if (force_exact_match) {
		/* Try to find a direct 'name.version' match */
		char game_name_tmp[1024];
		seprintf(game_name_tmp, lastof(game_name_tmp), "%s.%d", game_name, versionParam);
		strtolower(game_name_tmp);
		if (this->info_list.find(game_name_tmp) != this->info_list.end()) return static_cast<GameInfo *>(this->info_list[game_name_tmp]);
	}

	/* See if there is a compatible Game script which goes by that name, with the highest
	 *  version which allows loading the requested version */
	ScriptInfoList::iterator it = this->info_list.begin();
	for (; it != this->info_list.end(); it++) {
		GameInfo *i = static_cast<GameInfo *>((*it).second);
		if (strcasecmp(game_name, i->GetName()) == 0 && i->CanLoadFromVersion(versionParam) && (version == -1 || i->GetVersion() > version)) {
			version = (*it).second->GetVersion();
			info = i;
		}
	}

	return info;
}


void GameScannerLibrary::Initialize()
{
	ScriptScanner::Initialize("GSScanner");
}

void GameScannerLibrary::GetScriptName(ScriptInfo *info, char *name, const char *last)
{
	GameLibrary *library = static_cast<GameLibrary *>(info);
	seprintf(name, last, "%s.%s", library->GetCategory(), library->GetInstanceName());
}

void GameScannerLibrary::RegisterAPI(class Squirrel *engine)
{
	GameLibrary::RegisterAPI(engine);
}

GameLibrary *GameScannerLibrary::FindLibrary(const char *library, int version)
{
	/* Internally we store libraries as 'library.version' */
	char library_name[1024];
	seprintf(library_name, lastof(library_name), "%s.%d", library, version);
	strtolower(library_name);

	/* Check if the library + version exists */
	ScriptInfoList::iterator iter = this->info_list.find(library_name);
	if (iter == this->info_list.end()) return NULL;

	return static_cast<GameLibrary *>((*iter).second);
}
