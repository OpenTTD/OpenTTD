/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_scanner.cpp allows scanning AI scripts */

#include "../stdafx.h"
#include <ranges>
#include "../debug.h"
#include "../network/network.h"
#include "../openttd.h"
#include "../core/random_func.hpp"

#include "../script/squirrel_class.hpp"
#include "../script/api/script_object.hpp"
#include "ai_info.hpp"
#include "ai_scanner.hpp"

#include "../safeguards.h"


AIScannerInfo::AIScannerInfo() :
	ScriptScanner(),
	info_dummy(nullptr)
{
}

void AIScannerInfo::Initialize()
{
	ScriptScanner::Initialize("AIScanner");

	ScriptAllocatorScope alloc_scope(this->engine);

	/* Create the dummy AI */
	this->main_script = "%_dummy";
	Script_CreateDummyInfo(this->engine->GetVM(), "AI", "ai");
}

void AIScannerInfo::SetDummyAI(class AIInfo *info)
{
	this->info_dummy = info;
}

AIScannerInfo::~AIScannerInfo()
{
	delete this->info_dummy;
}

std::string AIScannerInfo::GetScriptName(ScriptInfo *info)
{
	return info->GetName();
}

void AIScannerInfo::RegisterAPI(class Squirrel *engine)
{
	AIInfo::RegisterAPI(engine);
}

AIInfo *AIScannerInfo::SelectRandomAI() const
{
	if (_game_mode == GM_MENU) {
		Debug(script, 0, "The intro game should not use AI, loading 'dummy' AI.");
		return this->info_dummy;
	}

	/* Filter for AIs suitable as Random AI. */
	auto random_ais = info_single_list | std::views::filter([](const auto &item) { return static_cast<AIInfo *>(item.second)->UseAsRandomAI(); });

	uint num_random_ais = std::ranges::distance(random_ais);
	if (num_random_ais == 0) {
		Debug(script, 0, "No suitable AI found, loading 'dummy' AI.");
		return this->info_dummy;
	}

	/* Pick a random AI */
	uint pos = ScriptObject::GetRandomizer(OWNER_NONE).Next(num_random_ais);
	auto it = std::ranges::next(std::begin(random_ais), pos, std::end(random_ais));
	assert(it != std::end(random_ais));

	return static_cast<AIInfo *>(it->second);
}

AIInfo *AIScannerInfo::FindInfo(const std::string &name, int version, bool force_exact_match)
{
	if (this->info_list.empty()) return nullptr;
	if (name.empty()) return nullptr;

	if (version == -1) {
		/* We want to load the latest version of this AI; so find it */
		auto it = this->info_single_list.find(name);
		if (it != this->info_single_list.end()) return static_cast<AIInfo *>(it->second);
		return nullptr;
	}

	if (force_exact_match) {
		/* Try to find a direct 'name.version' match */
		std::string name_with_version = fmt::format("{}.{}", name, version);
		auto it = this->info_list.find(name_with_version);
		if (it != this->info_list.end()) return static_cast<AIInfo *>(it->second);
		return nullptr;
	}

	AIInfo *info = nullptr;
	int highest_version = -1;

	/* See if there is a compatible AI which goes by that name, with the highest
	 *  version which allows loading the requested version */
	for (const auto &item : this->info_list) {
		AIInfo *i = static_cast<AIInfo *>(item.second);
		if (StrEqualsIgnoreCase(name, i->GetName()) && i->CanLoadFromVersion(version) && (highest_version == -1 || i->GetVersion() > highest_version)) {
			highest_version = item.second->GetVersion();
			info = i;
		}
	}

	return info;
}


void AIScannerLibrary::Initialize()
{
	ScriptScanner::Initialize("AIScanner");
}

std::string AIScannerLibrary::GetScriptName(ScriptInfo *info)
{
	AILibrary *library = static_cast<AILibrary *>(info);
	return fmt::format("{}.{}", library->GetCategory(), library->GetInstanceName());
}

void AIScannerLibrary::RegisterAPI(class Squirrel *engine)
{
	AILibrary::RegisterAPI(engine);
}

AILibrary *AIScannerLibrary::FindLibrary(const std::string &library, int version)
{
	/* Internally we store libraries as 'library.version' */
	std::string library_name = fmt::format("{}.{}", library, version);

	/* Check if the library + version exists */
	ScriptInfoList::iterator it = this->info_list.find(library_name);
	if (it == this->info_list.end()) return nullptr;

	return static_cast<AILibrary *>((*it).second);
}
