/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_controller.cpp Implementation of AIControler. */

#include "../../stdafx.h"
#include "../../string_func.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../../rev.h"

#include "ai_controller.hpp"
#include "../ai_instance.hpp"
#include "../ai_config.hpp"
#include "ai_log.hpp"

/* static */ void AIController::SetCommandDelay(int ticks)
{
	if (ticks <= 0) return;
	AIObject::SetDoCommandDelay(ticks);
}

/* static */ void AIController::Sleep(int ticks)
{
	if (!AIObject::CanSuspend()) {
		throw AI_FatalError("You are not allowed to call Sleep in your constructor, Save(), Load(), and any valuator.");
	}

	if (ticks <= 0) {
		AILog::Warning("Sleep() value should be > 0. Assuming value 1.");
		ticks = 1;
	}

	throw AI_VMSuspend(ticks, NULL);
}

/* static */ void AIController::Print(bool error_msg, const char *message)
{
	AILog::Log(error_msg ? AILog::LOG_SQ_ERROR : AILog::LOG_SQ_INFO, message);
}

AIController::AIController() :
	ticks(0),
	loaded_library_count(0)
{
}

AIController::~AIController()
{
	for (LoadedLibraryList::iterator iter = this->loaded_library.begin(); iter != this->loaded_library.end(); iter++) {
		free((void *)(*iter).second);
		free((void *)(*iter).first);
	}

	this->loaded_library.clear();
}

/* static */ uint AIController::GetTick()
{
	return ::Company::Get(_current_company)->ai_instance->GetController()->ticks;
}

/* static */ int AIController::GetSetting(const char *name)
{
	return AIConfig::GetConfig(_current_company)->GetSetting(name);
}

/* static */ uint AIController::GetVersion()
{
	return _openttd_newgrf_version;
}

bool AIController::LoadedLibrary(const char *library_name, int *next_number, char *fake_class_name, int fake_class_name_len)
{
	LoadedLibraryList::iterator iter = this->loaded_library.find(library_name);
	if (iter == this->loaded_library.end()) {
		*next_number = ++this->loaded_library_count;
		return false;
	}

	ttd_strlcpy(fake_class_name, (*iter).second, fake_class_name_len);
	return true;
}

void AIController::AddLoadedLibrary(const char *library_name, const char *fake_class_name)
{
	this->loaded_library[strdup(library_name)] = strdup(fake_class_name);
}
