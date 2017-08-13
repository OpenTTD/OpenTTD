/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_scanner.cpp allows scanning AI scripts */

#include "../stdafx.h"
#include "../debug.h"
#include "../network/network.h"
#include "../core/random_func.hpp"

#include "../script/squirrel_class.hpp"
#include "ai_info.hpp"
#include "ai_scanner.hpp"

#include "../safeguards.h"


AIScannerInfo::AIScannerInfo() :
	ScriptScanner(),
	info_dummy(NULL)
{
}

void AIScannerInfo::Initialize()
{
	ScriptScanner::Initialize("AIScanner");

	/* Create the dummy AI */
	free(this->main_script);
	this->main_script = stredup("%_dummy");
	extern void Script_CreateDummyInfo(HSQUIRRELVM vm, const char *type, const char *dir);
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

void AIScannerInfo::GetScriptName(ScriptInfo *info, char *name, const char *last)
{
	seprintf(name, last, "%s", info->GetName());
}

void AIScannerInfo::RegisterAPI(class Squirrel *engine)
{
	AIInfo::RegisterAPI(engine);
}

AIInfo *AIScannerInfo::SelectRandomAI() const
{
	uint num_random_ais = 0;
	for (ScriptInfoList::const_iterator it = this->info_single_list.begin(); it != this->info_single_list.end(); it++) {
		AIInfo *i = static_cast<AIInfo *>((*it).second);
		if (i->UseAsRandomAI()) num_random_ais++;
	}

	if (num_random_ais == 0) {
		DEBUG(script, 0, "No suitable AI found, loading 'dummy' AI.");
		return this->info_dummy;
	}

	/* Find a random AI */
	uint pos;
	if (_networking) {
		pos = InteractiveRandomRange(num_random_ais);
	} else {
		pos = RandomRange(num_random_ais);
	}

	/* Find the Nth item from the array */
	ScriptInfoList::const_iterator it = this->info_single_list.begin();

#define GetAIInfo(it) static_cast<AIInfo *>((*it).second)
	while (!GetAIInfo(it)->UseAsRandomAI()) it++;
	for (; pos > 0; pos--) {
		it++;
		while (!GetAIInfo(it)->UseAsRandomAI()) it++;
	}
	return GetAIInfo(it);
#undef GetAIInfo
}

AIInfo *AIScannerInfo::FindInfo(const char *nameParam, int versionParam, bool force_exact_match)
{
	if (this->info_list.size() == 0) return NULL;
	if (nameParam == NULL) return NULL;

	char ai_name[1024];
	strecpy(ai_name, nameParam, lastof(ai_name));
	strtolower(ai_name);

	AIInfo *info = NULL;
	int version = -1;

	if (versionParam == -1) {
		/* We want to load the latest version of this AI; so find it */
		if (this->info_single_list.find(ai_name) != this->info_single_list.end()) return static_cast<AIInfo *>(this->info_single_list[ai_name]);

		/* If we didn't find a match AI, maybe the user included a version */
		char *e = strrchr(ai_name, '.');
		if (e == NULL) return NULL;
		*e = '\0';
		e++;
		versionParam = atoi(e);
		/* Continue, like we were calling this function with a version. */
	}

	if (force_exact_match) {
		/* Try to find a direct 'name.version' match */
		char ai_name_tmp[1024];
		seprintf(ai_name_tmp, lastof(ai_name_tmp), "%s.%d", ai_name, versionParam);
		strtolower(ai_name_tmp);
		if (this->info_list.find(ai_name_tmp) != this->info_list.end()) return static_cast<AIInfo *>(this->info_list[ai_name_tmp]);
	}

	/* See if there is a compatible AI which goes by that name, with the highest
	 *  version which allows loading the requested version */
	ScriptInfoList::iterator it = this->info_list.begin();
	for (; it != this->info_list.end(); it++) {
		AIInfo *i = static_cast<AIInfo *>((*it).second);
		if (strcasecmp(ai_name, i->GetName()) == 0 && i->CanLoadFromVersion(versionParam) && (version == -1 || i->GetVersion() > version)) {
			version = (*it).second->GetVersion();
			info = i;
		}
	}

	return info;
}


void AIScannerLibrary::Initialize()
{
	ScriptScanner::Initialize("AIScanner");
}

void AIScannerLibrary::GetScriptName(ScriptInfo *info, char *name, const char *last)
{
	AILibrary *library = static_cast<AILibrary *>(info);
	seprintf(name, last, "%s.%s", library->GetCategory(), library->GetInstanceName());
}

void AIScannerLibrary::RegisterAPI(class Squirrel *engine)
{
	AILibrary::RegisterAPI(engine);
}

AILibrary *AIScannerLibrary::FindLibrary(const char *library, int version)
{
	/* Internally we store libraries as 'library.version' */
	char library_name[1024];
	seprintf(library_name, lastof(library_name), "%s.%d", library, version);
	strtolower(library_name);

	/* Check if the library + version exists */
	ScriptInfoList::iterator iter = this->info_list.find(library_name);
	if (iter == this->info_list.end()) return NULL;

	return static_cast<AILibrary *>((*iter).second);
}
