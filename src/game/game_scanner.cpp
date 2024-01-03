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

#include "../3rdparty/fmt/format.h"

#include "../safeguards.h"


void GameScannerInfo::Initialize()
{
	ScriptScanner::Initialize("GSScanner");
}

std::string GameScannerInfo::GetScriptName(ScriptInfo *info)
{
	return info->GetName();
}

void GameScannerInfo::RegisterAPI(class Squirrel *engine)
{
	GameInfo::RegisterAPI(engine);
}

GameInfo *GameScannerInfo::FindInfo(const std::string &name, int version, bool force_exact_match)
{
	if (this->info_list.empty()) return nullptr;
	if (name.empty()) return nullptr;

	if (version == -1) {
		/* We want to load the latest version of this Game script; so find it */
		auto it = this->info_single_list.find(name);
		if (it != this->info_single_list.end()) return static_cast<GameInfo *>(it->second);
		return nullptr;
	}

	if (force_exact_match) {
		/* Try to find a direct 'name.version' match */
		std::string name_with_version = fmt::format("{}.{}", name, version);
		auto it = this->info_list.find(name_with_version);
		if (it != this->info_list.end()) return static_cast<GameInfo *>(it->second);
		return nullptr;
	}

	GameInfo *info = nullptr;
	int highest_version = -1;

	/* See if there is a compatible Game script which goes by that name, with the highest
	 *  version which allows loading the requested version */
	for (const auto &item : this->info_list) {
		GameInfo *i = static_cast<GameInfo *>(item.second);
		if (StrEqualsIgnoreCase(name, i->GetName()) && i->CanLoadFromVersion(version) && (highest_version == -1 || i->GetVersion() > highest_version)) {
			highest_version = item.second->GetVersion();
			info = i;
		}
	}

	return info;
}


void GameScannerLibrary::Initialize()
{
	ScriptScanner::Initialize("GSScanner");
}

std::string GameScannerLibrary::GetScriptName(ScriptInfo *info)
{
	GameLibrary *library = static_cast<GameLibrary *>(info);
	return fmt::format("{}.{}", library->GetCategory(), library->GetInstanceName());
}

void GameScannerLibrary::RegisterAPI(class Squirrel *engine)
{
	GameLibrary::RegisterAPI(engine);
}

GameLibrary *GameScannerLibrary::FindLibrary(const std::string &library, int version)
{
	/* Internally we store libraries as 'library.version' */
	std::string library_name = fmt::format("{}.{}", library, version);

	/* Check if the library + version exists */
	ScriptInfoList::iterator it = this->info_list.find(library_name);
	if (it == this->info_list.end()) return nullptr;

	return static_cast<GameLibrary *>((*it).second);
}
