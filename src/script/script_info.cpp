/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_info.cpp Implementation of ScriptInfo. */

#include "../stdafx.h"
#include "../settings_type.h"

#include "squirrel_helper.hpp"

#include "script_info.hpp"
#include "script_scanner.hpp"

ScriptInfo::~ScriptInfo()
{
	/* Free all allocated strings */
	for (ScriptConfigItemList::iterator it = this->config_list.begin(); it != this->config_list.end(); it++) {
		free((*it).name);
		free((*it).description);
		if (it->labels != NULL) {
			for (LabelMapping::iterator it2 = (*it).labels->Begin(); it2 != (*it).labels->End(); it2++) {
				free(it2->second);
			}
			delete it->labels;
		}
	}
	this->config_list.clear();

	free(this->author);
	free(this->name);
	free(this->short_name);
	free(this->description);
	free(this->date);
	free(this->instance_name);
	free(this->url);
	free(this->main_script);
	free(this->tar_file);
	free(this->SQ_instance);
}

bool ScriptInfo::CheckMethod(const char *name) const
{
	if (!this->engine->MethodExists(*this->SQ_instance, name)) {
		char error[1024];
		snprintf(error, sizeof(error), "your info.nut/library.nut doesn't have the method '%s'", name);
		this->engine->ThrowError(error);
		return false;
	}
	return true;
}

/* static */ SQInteger ScriptInfo::Constructor(HSQUIRRELVM vm, ScriptInfo *info)
{
	/* Set some basic info from the parent */
	info->SQ_instance = MallocT<SQObject>(1);
	Squirrel::GetInstance(vm, info->SQ_instance, 2);
	/* Make sure the instance stays alive over time */
	sq_addref(vm, info->SQ_instance);

	info->scanner = (ScriptScanner *)Squirrel::GetGlobalPointer(vm);
	info->engine = info->scanner->GetEngine();

	/* Ensure the mandatory functions exist */
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

	/* Get location information of the scanner */
	info->main_script = strdup(info->scanner->GetMainScript());
	const char *tar_name = info->scanner->GetTarFile();
	if (tar_name != NULL) info->tar_file = strdup(tar_name);

	/* Cache the data the info file gives us. */
	if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "GetAuthor", &info->author, MAX_GET_OPS)) return SQ_ERROR;
	if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "GetName", &info->name, MAX_GET_OPS)) return SQ_ERROR;
	if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "GetShortName", &info->short_name, MAX_GET_OPS)) return SQ_ERROR;
	if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "GetDescription", &info->description, MAX_GET_OPS)) return SQ_ERROR;
	if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "GetDate", &info->date, MAX_GET_OPS)) return SQ_ERROR;
	if (!info->engine->CallIntegerMethod(*info->SQ_instance, "GetVersion", &info->version, MAX_GET_OPS)) return SQ_ERROR;
	if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "CreateInstance", &info->instance_name, MAX_CREATEINSTANCE_OPS)) return SQ_ERROR;

	/* The GetURL function is optional. */
	if (info->engine->MethodExists(*info->SQ_instance, "GetURL")) {
		if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "GetURL", &info->url, MAX_GET_OPS)) return SQ_ERROR;
	}

	/* Check if we have settings */
	if (info->engine->MethodExists(*info->SQ_instance, "GetSettings")) {
		if (!info->GetSettings()) return SQ_ERROR;
	}

	return 0;
}

bool ScriptInfo::GetSettings()
{
	return this->engine->CallMethod(*this->SQ_instance, "GetSettings", NULL, MAX_GET_SETTING_OPS);
}

SQInteger ScriptInfo::AddSetting(HSQUIRRELVM vm)
{
	ScriptConfigItem config;
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
			config.flags = (ScriptConfigFlags)res;
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

	/* Don't allow both random_deviation and SCRIPTCONFIG_RANDOM to
	 * be set for the same config item. */
	if ((items & 0x200) != 0 && (config.flags & SCRIPTCONFIG_RANDOM) != 0) {
		char error[1024];
		snprintf(error, sizeof(error), "Setting both random_deviation and SCRIPTCONFIG_RANDOM is not allowed");
		this->engine->ThrowError(error);
		return SQ_ERROR;
	}
	/* Reset the bit for random_deviation as it's optional. */
	items &= ~0x200;

	/* Make sure all properties are defined */
	uint mask = (config.flags & SCRIPTCONFIG_BOOLEAN) ? 0x1F3 : 0x1FF;
	if (items != mask) {
		char error[1024];
		snprintf(error, sizeof(error), "please define all properties of a setting (min/max not allowed for booleans)");
		this->engine->ThrowError(error);
		return SQ_ERROR;
	}

	this->config_list.push_back(config);
	return 0;
}

SQInteger ScriptInfo::AddLabels(HSQUIRRELVM vm)
{
	const SQChar *sq_setting_name;
	if (SQ_FAILED(sq_getstring(vm, -2, &sq_setting_name))) return SQ_ERROR;
	const char *setting_name = SQ2OTTD(sq_setting_name);

	ScriptConfigItem *config = NULL;
	for (ScriptConfigItemList::iterator it = this->config_list.begin(); it != this->config_list.end(); it++) {
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

const ScriptConfigItemList *ScriptInfo::GetConfigList() const
{
	return &this->config_list;
}

const ScriptConfigItem *ScriptInfo::GetConfigItem(const char *name) const
{
	for (ScriptConfigItemList::const_iterator it = this->config_list.begin(); it != this->config_list.end(); it++) {
		if (strcmp((*it).name, name) == 0) return &(*it);
	}
	return NULL;
}

int ScriptInfo::GetSettingDefaultValue(const char *name) const
{
	for (ScriptConfigItemList::const_iterator it = this->config_list.begin(); it != this->config_list.end(); it++) {
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
