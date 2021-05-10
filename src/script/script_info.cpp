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

#include "../safeguards.h"

ScriptInfo::~ScriptInfo()
{
	/* Free all allocated strings */
	for (const auto &item : this->config_list) {
		free(item.name);
		free(item.description);
		if (item.labels != nullptr) {
			for (auto &lbl_map : *item.labels) {
				free(lbl_map.second);
			}
			delete item.labels;
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
	free(this->SQ_instance);
}

bool ScriptInfo::CheckMethod(const char *name) const
{
	if (!this->engine->MethodExists(*this->SQ_instance, name)) {
		char error[1024];
		seprintf(error, lastof(error), "your info.nut/library.nut doesn't have the method '%s'", name);
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
	info->main_script = info->scanner->GetMainScript();
	info->tar_file = info->scanner->GetTarFile();

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
	return this->engine->CallMethod(*this->SQ_instance, "GetSettings", nullptr, MAX_GET_SETTING_OPS);
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
		const SQChar *key;
		if (SQ_FAILED(sq_getstring(vm, -2, &key))) return SQ_ERROR;
		ValidateString(key);

		if (strcmp(key, "name") == 0) {
			const SQChar *sqvalue;
			if (SQ_FAILED(sq_getstring(vm, -1, &sqvalue))) return SQ_ERROR;
			char *name = stredup(sqvalue);
			char *s;
			ValidateString(name);

			/* Don't allow '=' and ',' in configure setting names, as we need those
			 *  2 chars to nicely store the settings as a string. */
			while ((s = strchr(name, '=')) != nullptr) *s = '_';
			while ((s = strchr(name, ',')) != nullptr) *s = '_';
			config.name = name;
			items |= 0x001;
		} else if (strcmp(key, "description") == 0) {
			const SQChar *sqdescription;
			if (SQ_FAILED(sq_getstring(vm, -1, &sqdescription))) return SQ_ERROR;
			config.description = stredup(sqdescription);
			ValidateString(config.description);
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
			seprintf(error, lastof(error), "unknown setting property '%s'", key);
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
		seprintf(error, lastof(error), "Setting both random_deviation and SCRIPTCONFIG_RANDOM is not allowed");
		this->engine->ThrowError(error);
		return SQ_ERROR;
	}
	/* Reset the bit for random_deviation as it's optional. */
	items &= ~0x200;

	/* Make sure all properties are defined */
	uint mask = (config.flags & SCRIPTCONFIG_BOOLEAN) ? 0x1F3 : 0x1FF;
	if (items != mask) {
		char error[1024];
		seprintf(error, lastof(error), "please define all properties of a setting (min/max not allowed for booleans)");
		this->engine->ThrowError(error);
		return SQ_ERROR;
	}

	this->config_list.push_back(config);
	return 0;
}

SQInteger ScriptInfo::AddLabels(HSQUIRRELVM vm)
{
	const SQChar *setting_name;
	if (SQ_FAILED(sq_getstring(vm, -2, &setting_name))) return SQ_ERROR;
	ValidateString(setting_name);

	ScriptConfigItem *config = nullptr;
	for (auto &item : this->config_list) {
		if (strcmp(item.name, setting_name) == 0) config = &item;
	}

	if (config == nullptr) {
		char error[1024];
		seprintf(error, lastof(error), "Trying to add labels for non-defined setting '%s'", setting_name);
		this->engine->ThrowError(error);
		return SQ_ERROR;
	}
	if (config->labels != nullptr) return SQ_ERROR;

	config->labels = new LabelMapping;

	/* Read the table and find all labels */
	sq_pushnull(vm);
	while (SQ_SUCCEEDED(sq_next(vm, -2))) {
		const SQChar *key_string;
		const SQChar *label;
		if (SQ_FAILED(sq_getstring(vm, -2, &key_string))) return SQ_ERROR;
		if (SQ_FAILED(sq_getstring(vm, -1, &label))) return SQ_ERROR;
		/* Because squirrel doesn't support identifiers starting with a digit,
		 * we skip the first character. */
		int key = atoi(key_string + 1);
		ValidateString(label);

		/* !Contains() prevents stredup from leaking. */
		if (!config->labels->Contains(key)) config->labels->Insert(key, stredup(label));

		sq_pop(vm, 2);
	}
	sq_pop(vm, 1);

	/* Check labels for completeness */
	config->complete_labels = true;
	for (int value = config->min_value; value <= config->max_value; value++) {
		if (!config->labels->Contains(value)) {
			config->complete_labels = false;
			break;
		}
	}

	return 0;
}

const ScriptConfigItemList *ScriptInfo::GetConfigList() const
{
	return &this->config_list;
}

const ScriptConfigItem *ScriptInfo::GetConfigItem(const char *name) const
{
	for (const auto &item : this->config_list) {
		if (strcmp(item.name, name) == 0) return &item;
	}
	return nullptr;
}

int ScriptInfo::GetSettingDefaultValue(const char *name) const
{
	for (const auto &item : this->config_list) {
		if (strcmp(item.name, name) != 0) continue;
		/* The default value depends on the difficulty level */
		switch (GetGameSettings().script.settings_profile) {
			case SP_EASY:   return item.easy_value;
			case SP_MEDIUM: return item.medium_value;
			case SP_HARD:   return item.hard_value;
			case SP_CUSTOM: return item.custom_value;
			default: NOT_REACHED();
		}
	}

	/* There is no such setting */
	return -1;
}
