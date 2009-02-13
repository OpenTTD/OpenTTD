/* $Id$ */

/** @file ai_info.cpp Implementation of AIFileInfo */

#include "../stdafx.h"

#include <squirrel.h>
#include "../script/squirrel.hpp"
#include "../script/squirrel_helper.hpp"
#include "ai.hpp"
#include "ai_info.hpp"
#include "ai_scanner.hpp"
#include "../settings_type.h"
#include "../openttd.h"

AIConfigItem _start_date_config = {
	"start_date",
	"The amount of days after the start of the last AI, this AI will start (give or take).",
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

AIFileInfo::~AIFileInfo()
{
	this->engine->ReleaseObject(this->SQ_instance);
	free((void *)this->author);
	free((void *)this->name);
	free((void *)this->short_name);
	free((void *)this->description);
	free((void *)this->date);
	free((void *)this->instance_name);
	free(this->main_script);
	free(this->SQ_instance);
}

AILibrary::~AILibrary()
{
	free((void *)this->category);
}

const char *AIFileInfo::GetAuthor()
{
	if (this->author == NULL) this->author = this->engine->CallStringMethodStrdup(*this->SQ_instance, "GetAuthor");
	return this->author;
}

const char *AIFileInfo::GetName()
{
	if (this->name == NULL) this->name = this->engine->CallStringMethodStrdup(*this->SQ_instance, "GetName");
	return this->name;
}

const char *AIFileInfo::GetShortName()
{
	if (this->short_name == NULL) this->short_name = this->engine->CallStringMethodStrdup(*this->SQ_instance, "GetShortName");
	return this->short_name;
}

const char *AIFileInfo::GetDescription()
{
	if (this->description == NULL) this->description = this->engine->CallStringMethodStrdup(*this->SQ_instance, "GetDescription");
	return this->description;
}

int AIFileInfo::GetVersion()
{
	return this->engine->CallIntegerMethod(*this->SQ_instance, "GetVersion");
}

void AIFileInfo::GetSettings()
{
	this->engine->CallMethod(*this->SQ_instance, "GetSettings", NULL, -1);
}

const char *AIFileInfo::GetDate()
{
	if (this->date == NULL) this->date = this->engine->CallStringMethodStrdup(*this->SQ_instance, "GetDate");
	return this->date;
}

const char *AIFileInfo::GetInstanceName()
{
	if (this->instance_name == NULL) this->instance_name = this->engine->CallStringMethodStrdup(*this->SQ_instance, "CreateInstance");
	return this->instance_name;
}

bool AIFileInfo::CanLoadFromVersion(int version)
{
	if (version == -1) return true;
	if (!this->engine->MethodExists(*this->SQ_instance, "MinVersionToLoad")) return (version == this->GetVersion());

	return version >= this->engine->CallIntegerMethod(*this->SQ_instance, "MinVersionToLoad") && version <= this->GetVersion();
}

const char *AIFileInfo::GetMainScript()
{
	return this->main_script;
}

bool AIFileInfo::CheckMethod(const char *name)
{
	if (!this->engine->MethodExists(*this->SQ_instance, name)) {
		char error[1024];
		snprintf(error, sizeof(error), "your info.nut/library.nut doesn't have the method '%s'", name);
		this->engine->ThrowError(error);
		return false;
	}
	return true;
}

/* static */ SQInteger AIFileInfo::Constructor(HSQUIRRELVM vm, AIFileInfo *info, bool library)
{
	/* Set some basic info from the parent */
	info->SQ_instance = MallocT<SQObject>(1);
	Squirrel::GetInstance(vm, info->SQ_instance, 2);
	/* Make sure the instance stays alive over time */
	sq_addref(vm, info->SQ_instance);
	info->base = ((AIScanner *)Squirrel::GetGlobalPointer(vm));
	info->engine = info->base->GetEngine();

	static const char * const required_functions[] = {
		"GetAuthor",
		"GetName",
		"GetShortName",
		"GetDescription",
		"GetVersion",
		"GetDate",
		"CreateInstance",
	};
	for (size_t i = 0; i < lengthof(required_functions); i++) {
		if (!info->CheckMethod(required_functions[i])) return SQ_ERROR;
	}
	if (library) {
		if (!info->CheckMethod("GetCategory")) return SQ_ERROR;
	}

	info->main_script = strdup(info->base->GetMainScript());

	return 0;
}

/* static */ SQInteger AIInfo::Constructor(HSQUIRRELVM vm)
{
	/* Get the AIInfo */
	SQUserPointer instance;
	sq_getinstanceup(vm, 2, &instance, 0);
	AIInfo *info = (AIInfo *)instance;

	SQInteger res = AIFileInfo::Constructor(vm, info, false);
	if (res != 0) return res;

	AIConfigItem config = _start_date_config;
	config.name = strdup(config.name);
	config.description = strdup(config.description);
	info->config_list.push_back(config);

	/* Check if we have settings */
	if (info->engine->MethodExists(*info->SQ_instance, "GetSettings")) {
		info->GetSettings();
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

	SQInteger res = AIFileInfo::Constructor(vm, info, false);
	if (res != 0) return res;

	/* Remove the link to the real instance, else it might get deleted by RegisterAI() */
	sq_setinstanceup(vm, 2, NULL);
	/* Register the AI to the base system */
	info->base->SetDummyAI(info);
	return 0;
}

AIInfo::~AIInfo()
{
	/* Free all allocated strings */
	for (AIConfigItemList::iterator it = this->config_list.begin(); it != this->config_list.end(); it++) {
		free((char *)(*it).name);
		free((char *)(*it).description);
		if (it->labels != NULL) {
			for (LabelMapping::iterator it2 = (*it).labels->Begin(); it2 != (*it).labels->End(); it2++) {
				free(it2->second);
			}
			delete it->labels;
		}
	}
	this->config_list.clear();
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
		sq_getstring(vm, -2, &sqkey);
		const char *key = FS2OTTD(sqkey);

		if (strcmp(key, "name") == 0) {
			const SQChar *sqvalue;
			sq_getstring(vm, -1, &sqvalue);
			config.name = strdup(FS2OTTD(sqvalue));
			char *s;
			/* Don't allow '=' and ',' in configure setting names, as we need those
			 *  2 chars to nicely store the settings as a string. */
			while ((s = (char *)strchr(config.name, '=')) != NULL) *s = '_';
			while ((s = (char *)strchr(config.name, ',')) != NULL) *s = '_';
			items |= 0x001;
		} else if (strcmp(key, "description") == 0) {
			const SQChar *sqdescription;
			sq_getstring(vm, -1, &sqdescription);
			config.description = strdup(FS2OTTD(sqdescription));
			items |= 0x002;
		} else if (strcmp(key, "min_value") == 0) {
			SQInteger res;
			sq_getinteger(vm, -1, &res);
			config.min_value = res;
			items |= 0x004;
		} else if (strcmp(key, "max_value") == 0) {
			SQInteger res;
			sq_getinteger(vm, -1, &res);
			config.max_value = res;
			items |= 0x008;
		} else if (strcmp(key, "easy_value") == 0) {
			SQInteger res;
			sq_getinteger(vm, -1, &res);
			config.easy_value = res;
			items |= 0x010;
		} else if (strcmp(key, "medium_value") == 0) {
			SQInteger res;
			sq_getinteger(vm, -1, &res);
			config.medium_value = res;
			items |= 0x020;
		} else if (strcmp(key, "hard_value") == 0) {
			SQInteger res;
			sq_getinteger(vm, -1, &res);
			config.hard_value = res;
			items |= 0x040;
		}  else if (strcmp(key, "random_deviation") == 0) {
			SQInteger res;
			sq_getinteger(vm, -1, &res);
			config.random_deviation = res;
			items |= 0x200;
		} else if (strcmp(key, "custom_value") == 0) {
			SQInteger res;
			sq_getinteger(vm, -1, &res);
			config.custom_value = res;
			items |= 0x080;
		} else if (strcmp(key, "step_size") == 0) {
			SQInteger res;
			sq_getinteger(vm, -1, &res);
			config.step_size = res;
		} else if (strcmp(key, "flags") == 0) {
			SQInteger res;
			sq_getinteger(vm, -1, &res);
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
	sq_getstring(vm, -2, &sq_setting_name);
	const char *setting_name = FS2OTTD(sq_setting_name);

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
		sq_getstring(vm, -2, &sq_key);
		sq_getstring(vm, -1, &sq_label);
		/* Because squirrel doesn't support identifiers starting with a digit,
		 * we skip the first character. */
		const char *key_string = FS2OTTD(sq_key);
		int key = atoi(key_string + 1);
		const char *label = FS2OTTD(sq_label);

		if (config->labels->Find(key) == config->labels->End()) config->labels->Insert(key, strdup(label));

		sq_pop(vm, 2);
	}
	sq_pop(vm, 1);

	return 0;
}

const AIConfigItemList *AIInfo::GetConfigList()
{
	return &this->config_list;
}

const AIConfigItem *AIInfo::GetConfigItem(const char *name)
{
	for (AIConfigItemList::iterator it = this->config_list.begin(); it != this->config_list.end(); it++) {
		if (strcmp((*it).name, name) == 0) return &(*it);
	}
	return NULL;
}

int AIInfo::GetSettingDefaultValue(const char *name)
{
	for (AIConfigItemList::iterator it = this->config_list.begin(); it != this->config_list.end(); it++) {
		if (strcmp((*it).name, name) != 0) continue;
		/* The default value depends on the difficulty level */
		switch ((_game_mode == GM_MENU) ? _settings_newgame.difficulty.diff_level : _settings_game.difficulty.diff_level) {
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

	SQInteger res = AIFileInfo::Constructor(vm, library, true);
	if (res != 0) {
		delete library;
		return res;
	}

	/* Register the Library to the base system */
	library->base->RegisterLibrary(library);

	return 0;
}

const char *AILibrary::GetCategory()
{
	if (this->category == NULL) this->category = this->engine->CallStringMethodStrdup(*this->SQ_instance, "GetCategory");
	return this->category;
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
