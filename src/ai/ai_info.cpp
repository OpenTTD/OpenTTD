/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_info.cpp Implementation of AIFileInfo */

#include "../stdafx.h"

#include "../script/squirrel_helper.hpp"
#include "ai_info.hpp"
#include "ai_scanner.hpp"
#include "../settings_type.h"
#include "../debug.h"
#include "../rev.h"

/** Maximum number of operations allowed for getting a particular setting. */
static const int MAX_GET_SETTING_OPS = 100000;

/** Configuration for AI start date, every AI has this setting. */
AIConfigItem _start_date_config = {
	"start_date",
	"Number of days to start this AI after the previous one (give or take)",
	AI::START_NEXT_MIN,
	AI::START_NEXT_MAX,
	AI::START_NEXT_MEDIUM,
	AI::START_NEXT_EASY,
	AI::START_NEXT_MEDIUM,
	AI::START_NEXT_HARD,
	AI::START_NEXT_DEVIATION,
	30,
	AICONFIG_NONE,
	NULL
};

AILibrary::~AILibrary()
{
	free((void *)this->category);
}

/* static */ SQInteger AIFileInfo::Constructor(HSQUIRRELVM vm, AIFileInfo *info)
{
	SQInteger res = ScriptFileInfo::Constructor(vm, info);
	if (res != 0) return res;
	info->base = ((AIScanner *)Squirrel::GetGlobalPointer(vm));

	return 0;
}

/**
 * Check if the API version provided by the AI is supported.
 * @param api_version The API version as provided by the AI.
 */
static bool CheckAPIVersion(const char *api_version)
{
	return strcmp(api_version, "0.7") == 0 || strcmp(api_version, "1.0") == 0 || strcmp(api_version, "1.1") == 0;
}

/* static */ SQInteger AIInfo::Constructor(HSQUIRRELVM vm)
{
	/* Get the AIInfo */
	SQUserPointer instance = NULL;
	if (SQ_FAILED(sq_getinstanceup(vm, 2, &instance, 0)) || instance == NULL) return sq_throwerror(vm, _SC("Pass an instance of a child class of AIInfo to RegisterAI"));
	AIInfo *info = (AIInfo *)instance;

	SQInteger res = AIFileInfo::Constructor(vm, info);
	if (res != 0) return res;

	AIConfigItem config = _start_date_config;
	config.name = strdup(config.name);
	config.description = strdup(config.description);
	info->config_list.push_back(config);

	/* Check if we have settings */
	if (info->engine->MethodExists(*info->SQ_instance, "GetSettings")) {
		if (!info->GetSettings()) return SQ_ERROR;
	}
	if (info->engine->MethodExists(*info->SQ_instance, "MinVersionToLoad")) {
		if (!info->engine->CallIntegerMethod(*info->SQ_instance, "MinVersionToLoad", &info->min_loadable_version, MAX_GET_SETTING_OPS)) return SQ_ERROR;
	} else {
		info->min_loadable_version = info->GetVersion();
	}
	/* When there is an UseAsRandomAI function, call it. */
	if (info->engine->MethodExists(*info->SQ_instance, "UseAsRandomAI")) {
		if (!info->engine->CallBoolMethod(*info->SQ_instance, "UseAsRandomAI", &info->use_as_random, MAX_GET_SETTING_OPS)) return SQ_ERROR;
	} else {
		info->use_as_random = true;
	}
	/* Try to get the API version the AI is written for. */
	if (info->engine->MethodExists(*info->SQ_instance, "GetAPIVersion")) {
		if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "GetAPIVersion", &info->api_version, MAX_GET_SETTING_OPS)) return SQ_ERROR;
		if (!CheckAPIVersion(info->api_version)) {
			DEBUG(ai, 1, "Loading info.nut from (%s.%d): GetAPIVersion returned invalid version", info->GetName(), info->GetVersion());
			return SQ_ERROR;
		}
	} else {
		info->api_version = strdup("0.7");
	}

	/* Remove the link to the real instance, else it might get deleted by RegisterAI() */
	sq_setinstanceup(vm, 2, NULL);
	/* Register the AI to the base system */
	info->base->RegisterAI(info);
	return 0;
}

/* static */ SQInteger AIInfo::DummyConstructor(HSQUIRRELVM vm)
{
	/* Get the AIInfo */
	SQUserPointer instance;
	sq_getinstanceup(vm, 2, &instance, 0);
	AIInfo *info = (AIInfo *)instance;
	info->api_version = NULL;

	SQInteger res = AIFileInfo::Constructor(vm, info);
	if (res != 0) return res;

	char buf[8];
	seprintf(buf, lastof(buf), "%d.%d", GB(_openttd_newgrf_version, 28, 4), GB(_openttd_newgrf_version, 24, 4));
	info->api_version = strdup(buf);

	/* Remove the link to the real instance, else it might get deleted by RegisterAI() */
	sq_setinstanceup(vm, 2, NULL);
	/* Register the AI to the base system */
	info->base->SetDummyAI(info);
	return 0;
}

bool AIInfo::GetSettings()
{
	return this->engine->CallMethod(*this->SQ_instance, "GetSettings", NULL, MAX_GET_SETTING_OPS);
}

AIInfo::AIInfo() :
	min_loadable_version(0),
	use_as_random(false),
	api_version(NULL)
{
}

AIInfo::~AIInfo()
{
	/* Free all allocated strings */
	for (AIConfigItemList::iterator it = this->config_list.begin(); it != this->config_list.end(); it++) {
		free((void*)(*it).name);
		free((void*)(*it).description);
		if (it->labels != NULL) {
			for (LabelMapping::iterator it2 = (*it).labels->Begin(); it2 != (*it).labels->End(); it2++) {
				free(it2->second);
			}
			delete it->labels;
		}
	}
	this->config_list.clear();
	free((void*)this->api_version);
}

bool AIInfo::CanLoadFromVersion(int version) const
{
	if (version == -1) return true;
	return version >= this->min_loadable_version && version <= this->GetVersion();
}

SQInteger AIInfo::AddSetting(HSQUIRRELVM vm)
{
	AIConfigItem config;
	memset(&config, 0, sizeof(config));
	config.max_value = 1;
	config.step_size = 1;
	uint items = 0;

	/* Read the table, and find all properties we care about */
	sq_pushnull(vm);
	while (SQ_SUCCEEDED(sq_next(vm, -2))) {
		const SQChar *sqkey;
		if (SQ_FAILED(sq_getstring(vm, -2, &sqkey))) return SQ_ERROR;
		const char *key = SQ2OTTD(sqkey);

		if (strcmp(key, "name") == 0) {
			const SQChar *sqvalue;
			if (SQ_FAILED(sq_getstring(vm, -1, &sqvalue))) return SQ_ERROR;
			char *name = strdup(SQ2OTTD(sqvalue));
			char *s;
			/* Don't allow '=' and ',' in configure setting names, as we need those
			 *  2 chars to nicely store the settings as a string. */
			while ((s = strchr(name, '=')) != NULL) *s = '_';
			while ((s = strchr(name, ',')) != NULL) *s = '_';
			config.name = name;
			items |= 0x001;
		} else if (strcmp(key, "description") == 0) {
			const SQChar *sqdescription;
			if (SQ_FAILED(sq_getstring(vm, -1, &sqdescription))) return SQ_ERROR;
			config.description = strdup(SQ2OTTD(sqdescription));
			items |= 0x002;
		} else if (strcmp(key, "min_value") == 0) {
			SQInteger res;
			if (SQ_FAILED(sq_getinteger(vm, -1, &res))) return SQ_ERROR;
			config.min_value = res;
			items |= 0x004;
		} else if (strcmp(key, "max_value") == 0) {
			SQInteger res;
			if (SQ_FAILED(sq_getinteger(vm, -1, &res))) return SQ_ERROR;
			config.max_value = res;
			items |= 0x008;
		} else if (strcmp(key, "easy_value") == 0) {
			SQInteger res;
			if (SQ_FAILED(sq_getinteger(vm, -1, &res))) return SQ_ERROR;
			config.easy_value = res;
			items |= 0x010;
		} else if (strcmp(key, "medium_value") == 0) {
			SQInteger res;
			if (SQ_FAILED(sq_getinteger(vm, -1, &res))) return SQ_ERROR;
			config.medium_value = res;
			items |= 0x020;
		} else if (strcmp(key, "hard_value") == 0) {
			SQInteger res;
			if (SQ_FAILED(sq_getinteger(vm, -1, &res))) return SQ_ERROR;
			config.hard_value = res;
			items |= 0x040;
		} else if (strcmp(key, "random_deviation") == 0) {
			SQInteger res;
			if (SQ_FAILED(sq_getinteger(vm, -1, &res))) return SQ_ERROR;
			config.random_deviation = res;
			items |= 0x200;
		} else if (strcmp(key, "custom_value") == 0) {
			SQInteger res;
			if (SQ_FAILED(sq_getinteger(vm, -1, &res))) return SQ_ERROR;
			config.custom_value = res;
			items |= 0x080;
		} else if (strcmp(key, "step_size") == 0) {
			SQInteger res;
			if (SQ_FAILED(sq_getinteger(vm, -1, &res))) return SQ_ERROR;
			config.step_size = res;
		} else if (strcmp(key, "flags") == 0) {
			SQInteger res;
			if (SQ_FAILED(sq_getinteger(vm, -1, &res))) return SQ_ERROR;
			config.flags = (AIConfigFlags)res;
			items |= 0x100;
		} else {
			char error[1024];
			snprintf(error, sizeof(error), "unknown setting property '%s'", key);
			this->engine->ThrowError(error);
			return SQ_ERROR;
		}

		sq_pop(vm, 2);
	}
	sq_pop(vm, 1);

	/* Don't allow both random_deviation and AICONFIG_RANDOM to
	 * be set for the same config item. */
	if ((items & 0x200) != 0 && (config.flags & AICONFIG_RANDOM) != 0) {
		char error[1024];
		snprintf(error, sizeof(error), "Setting both random_deviation and AICONFIG_RANDOM is not allowed");
		this->engine->ThrowError(error);
		return SQ_ERROR;
	}
	/* Reset the bit for random_deviation as it's optional. */
	items &= ~0x200;

	/* Make sure all properties are defined */
	uint mask = (config.flags & AICONFIG_BOOLEAN) ? 0x1F3 : 0x1FF;
	if (items != mask) {
		char error[1024];
		snprintf(error, sizeof(error), "please define all properties of a setting (min/max not allowed for booleans)");
		this->engine->ThrowError(error);
		return SQ_ERROR;
	}

	this->config_list.push_back(config);
	return 0;
}

SQInteger AIInfo::AddLabels(HSQUIRRELVM vm)
{
	const SQChar *sq_setting_name;
	if (SQ_FAILED(sq_getstring(vm, -2, &sq_setting_name))) return SQ_ERROR;
	const char *setting_name = SQ2OTTD(sq_setting_name);

	AIConfigItem *config = NULL;
	for (AIConfigItemList::iterator it = this->config_list.begin(); it != this->config_list.end(); it++) {
		if (strcmp((*it).name, setting_name) == 0) config = &(*it);
	}

	if (config == NULL) {
		char error[1024];
		snprintf(error, sizeof(error), "Trying to add labels for non-defined setting '%s'", setting_name);
		this->engine->ThrowError(error);
		return SQ_ERROR;
	}
	if (config->labels != NULL) return SQ_ERROR;

	config->labels = new LabelMapping;

	/* Read the table and find all labels */
	sq_pushnull(vm);
	while (SQ_SUCCEEDED(sq_next(vm, -2))) {
		const SQChar *sq_key;
		const SQChar *sq_label;
		if (SQ_FAILED(sq_getstring(vm, -2, &sq_key))) return SQ_ERROR;
		if (SQ_FAILED(sq_getstring(vm, -1, &sq_label))) return SQ_ERROR;
		/* Because squirrel doesn't support identifiers starting with a digit,
		 * we skip the first character. */
		const char *key_string = SQ2OTTD(sq_key);
		int key = atoi(key_string + 1);
		const char *label = SQ2OTTD(sq_label);

		/* !Contains() prevents strdup from leaking. */
		if (!config->labels->Contains(key)) config->labels->Insert(key, strdup(label));

		sq_pop(vm, 2);
	}
	sq_pop(vm, 1);

	return 0;
}

const AIConfigItemList *AIInfo::GetConfigList() const
{
	return &this->config_list;
}

const AIConfigItem *AIInfo::GetConfigItem(const char *name) const
{
	for (AIConfigItemList::const_iterator it = this->config_list.begin(); it != this->config_list.end(); it++) {
		if (strcmp((*it).name, name) == 0) return &(*it);
	}
	return NULL;
}

int AIInfo::GetSettingDefaultValue(const char *name) const
{
	for (AIConfigItemList::const_iterator it = this->config_list.begin(); it != this->config_list.end(); it++) {
		if (strcmp((*it).name, name) != 0) continue;
		/* The default value depends on the difficulty level */
		switch (GetGameSettings().difficulty.diff_level) {
			case 0: return (*it).easy_value;
			case 1: return (*it).medium_value;
			case 2: return (*it).hard_value;
			case 3: return (*it).custom_value;
			default: NOT_REACHED();
		}
	}

	/* There is no such setting */
	return -1;
}

/* static */ SQInteger AILibrary::Constructor(HSQUIRRELVM vm)
{
	/* Create a new AIFileInfo */
	AILibrary *library = new AILibrary();

	SQInteger res = AIFileInfo::Constructor(vm, library);
	if (res != 0) {
		delete library;
		return res;
	}

	/* Cache the category */
	if (!library->CheckMethod("GetCategory") || !library->engine->CallStringMethodStrdup(*library->SQ_instance, "GetCategory", &library->category, MAX_GET_SETTING_OPS)) {
		delete library;
		return SQ_ERROR;
	}

	/* Register the Library to the base system */
	library->base->RegisterLibrary(library);

	return 0;
}

/* static */ SQInteger AILibrary::Import(HSQUIRRELVM vm)
{
	SQConvert::SQAutoFreePointers ptr;
	const char *library = GetParam(SQConvert::ForceType<const char *>(), vm, 2, &ptr);
	const char *class_name = GetParam(SQConvert::ForceType<const char *>(), vm, 3, &ptr);
	int version = GetParam(SQConvert::ForceType<int>(), vm, 4, &ptr);

	if (!AI::ImportLibrary(library, class_name, version, vm)) return -1;
	return 1;
}
