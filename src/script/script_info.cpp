/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_info.cpp Implementation of ScriptInfo. */

#include "../stdafx.h"
#include "../settings_type.h"

#include "squirrel_helper.hpp"

#include "script_info.hpp"
#include "script_scanner.hpp"
#include "../core/string_consumer.hpp"
#include "../3rdparty/fmt/format.h"

#include "../safeguards.h"

bool ScriptInfo::CheckMethod(std::string_view name) const
{
	if (!this->engine->MethodExists(this->SQ_instance, name)) {
		this->engine->ThrowError(fmt::format("your info.nut/library.nut doesn't have the method '{}'", name));
		return false;
	}
	return true;
}

/* static */ SQInteger ScriptInfo::Constructor(HSQUIRRELVM vm, ScriptInfo &info)
{
	/* Set some basic info from the parent */
	Squirrel::GetInstance(vm, &info.SQ_instance, 2);
	/* Make sure the instance stays alive over time */
	sq_addref(vm, &info.SQ_instance);

	info.scanner = (ScriptScanner *)Squirrel::GetGlobalPointer(vm);
	info.engine = info.scanner->GetEngine();

	/* Ensure the mandatory functions exist */
	static const std::string_view required_functions[] = {
		"GetAuthor",
		"GetName",
		"GetShortName",
		"GetDescription",
		"GetVersion",
		"GetDate",
		"CreateInstance",
	};
	for (const auto &required_function : required_functions) {
		if (!info.CheckMethod(required_function)) return SQ_ERROR;
	}

	/* Get location information of the scanner */
	info.main_script = info.scanner->GetMainScript();
	info.tar_file = info.scanner->GetTarFile();

	/* Cache the data the info file gives us. */
	if (!info.engine->CallStringMethod(info.SQ_instance, "GetAuthor", &info.author, MAX_GET_OPS)) return SQ_ERROR;
	if (!info.engine->CallStringMethod(info.SQ_instance, "GetName", &info.name, MAX_GET_OPS)) return SQ_ERROR;
	if (!info.engine->CallStringMethod(info.SQ_instance, "GetShortName", &info.short_name, MAX_GET_OPS)) return SQ_ERROR;
	if (!info.engine->CallStringMethod(info.SQ_instance, "GetDescription", &info.description, MAX_GET_OPS)) return SQ_ERROR;
	if (!info.engine->CallStringMethod(info.SQ_instance, "GetDate", &info.date, MAX_GET_OPS)) return SQ_ERROR;
	if (!info.engine->CallIntegerMethod(info.SQ_instance, "GetVersion", &info.version, MAX_GET_OPS)) return SQ_ERROR;
	if (info.version < 0) return SQ_ERROR;
	if (!info.engine->CallStringMethod(info.SQ_instance, "CreateInstance", &info.instance_name, MAX_CREATEINSTANCE_OPS)) return SQ_ERROR;

	/* The GetURL function is optional. */
	if (info.engine->MethodExists(info.SQ_instance, "GetURL")) {
		if (!info.engine->CallStringMethod(info.SQ_instance, "GetURL", &info.url, MAX_GET_OPS)) return SQ_ERROR;
	}

	/* Check if we have settings */
	if (info.engine->MethodExists(info.SQ_instance, "GetSettings")) {
		if (!info.GetSettings()) return SQ_ERROR;
	}

	return 0;
}

bool ScriptInfo::GetSettings()
{
	return this->engine->CallMethod(this->SQ_instance, "GetSettings", nullptr, MAX_GET_SETTING_OPS);
}

enum class ScriptConfigItemKey : uint8_t {
	Name,
	Description,
	MinValue,
	MaxValue,
	MediumValue,
	DefaultValue,
	Flags,
};
using ScriptConfigItemKeys = EnumBitSet<ScriptConfigItemKey, uint8_t>;

SQInteger ScriptInfo::AddSetting(HSQUIRRELVM vm)
{
	ScriptConfigItem config;
	ScriptConfigItemKeys present{};

	int medium_value = INT32_MIN;

	/* Read the table, and find all properties we care about */
	sq_pushnull(vm);
	while (SQ_SUCCEEDED(sq_next(vm, -2))) {
		std::string_view key_string;
		if (SQ_FAILED(sq_getstring(vm, -2, key_string))) return SQ_ERROR;
		std::string key = StrMakeValid(key_string);

		if (key == "name") {
			std::string_view sqvalue;
			if (SQ_FAILED(sq_getstring(vm, -1, sqvalue))) return SQ_ERROR;

			/* Don't allow '=' and ',' in configure setting names, as we need those
			 *  2 chars to nicely store the settings as a string. */
			auto replace_with_underscore = [](auto c) { return c == '=' || c == ','; };
			config.name = StrMakeValid(sqvalue);
			std::replace_if(config.name.begin(), config.name.end(), replace_with_underscore, '_');
			present.Set(ScriptConfigItemKey::Name);
		} else if (key == "description") {
			std::string_view sqdescription;
			if (SQ_FAILED(sq_getstring(vm, -1, sqdescription))) return SQ_ERROR;
			config.description = StrMakeValid(sqdescription);
			present.Set(ScriptConfigItemKey::Description);
		} else if (key == "min_value") {
			SQInteger res;
			if (SQ_FAILED(sq_getinteger(vm, -1, &res))) return SQ_ERROR;
			config.min_value = ClampTo<int32_t>(res);
			present.Set(ScriptConfigItemKey::MinValue);
		} else if (key == "max_value") {
			SQInteger res;
			if (SQ_FAILED(sq_getinteger(vm, -1, &res))) return SQ_ERROR;
			config.max_value = ClampTo<int32_t>(res);
			present.Set(ScriptConfigItemKey::MaxValue);
		} else if (key == "easy_value") {
			/* No longer parsed. */
		} else if (key == "medium_value") {
			SQInteger res;
			if (SQ_FAILED(sq_getinteger(vm, -1, &res))) return SQ_ERROR;
			medium_value = ClampTo<int32_t>(res);
			present.Set(ScriptConfigItemKey::MediumValue);
		} else if (key == "hard_value") {
			/* No longer parsed. */
		} else if (key == "custom_value") {
			/* No longer parsed. */
		} else if (key == "default_value") {
			SQInteger res;
			if (SQ_FAILED(sq_getinteger(vm, -1, &res))) return SQ_ERROR;
			config.default_value = ClampTo<int32_t>(res);
			present.Set(ScriptConfigItemKey::DefaultValue);
		} else if (key == "random_deviation") {
			/* No longer parsed. */
		} else if (key == "step_size") {
			SQInteger res;
			if (SQ_FAILED(sq_getinteger(vm, -1, &res))) return SQ_ERROR;
			config.step_size = ClampTo<int32_t>(res);
		} else if (key == "flags") {
			SQInteger res;
			if (SQ_FAILED(sq_getinteger(vm, -1, &res))) return SQ_ERROR;
			config.flags = static_cast<ScriptConfigFlags>(res);
			present.Set(ScriptConfigItemKey::Flags);
		} else {
			this->engine->ThrowError(fmt::format("unknown setting property '{}'", key));
			return SQ_ERROR;
		}

		sq_pop(vm, 2);
	}
	sq_pop(vm, 1);

	/* Check if default_value is set. Although required, this was changed with
	 * 14.0, and as such, older AIs don't use it yet. So we convert the older
	 * values into a default_value. */
	if (!present.Test(ScriptConfigItemKey::DefaultValue)) {
		/* Easy/medium/hard should all three be defined. */
		if (!present.Test(ScriptConfigItemKey::MediumValue)) {
			this->engine->ThrowError("please define all properties of a setting (min/max not allowed for booleans)");
			return SQ_ERROR;
		}

		config.default_value = medium_value;
		present.Set(ScriptConfigItemKey::DefaultValue);
	}

	/* Make sure all required properties are defined */
	ScriptConfigItemKeys required = {ScriptConfigItemKey::Name, ScriptConfigItemKey::Description, ScriptConfigItemKey::DefaultValue, ScriptConfigItemKey::Flags};
	if (!config.flags.Test(ScriptConfigFlag::Boolean)) required.Set({ScriptConfigItemKey::MinValue, ScriptConfigItemKey::MaxValue});

	if (!present.All(required)) {
		this->engine->ThrowError("please define all properties of a setting (min/max not allowed for booleans)");
		return SQ_ERROR;
	}

	this->config_list.emplace_back(config);
	return 0;
}

SQInteger ScriptInfo::AddLabels(HSQUIRRELVM vm)
{
	std::string_view setting_name_view;
	if (SQ_FAILED(sq_getstring(vm, -2, setting_name_view))) return SQ_ERROR;
	std::string setting_name = StrMakeValid(setting_name_view);

	ScriptConfigItem *config = nullptr;
	for (auto &item : this->config_list) {
		if (item.name == setting_name) config = &item;
	}

	if (config == nullptr) {
		this->engine->ThrowError(fmt::format("Trying to add labels for non-defined setting '{}'", setting_name));
		return SQ_ERROR;
	}
	if (!config->labels.empty()) return SQ_ERROR;

	/* Read the table and find all labels */
	sq_pushnull(vm);
	while (SQ_SUCCEEDED(sq_next(vm, -2))) {
		std::string_view key_string;
		std::string_view label;
		if (SQ_FAILED(sq_getstring(vm, -2, key_string))) return SQ_ERROR;
		if (SQ_FAILED(sq_getstring(vm, -1, label))) return SQ_ERROR;
		/* Because squirrel doesn't support identifiers starting with a digit,
		 * we skip the first character. */
		key_string.remove_prefix(1);
		int sign = 1;
		if (key_string.starts_with('_')) {
			/* When the second character is '_', it indicates the value is negative. */
			sign = -1;
			key_string.remove_prefix(1);
		}
		auto key = ParseInteger<int>(key_string);
		if (!key.has_value()) return SQ_ERROR;
		config->labels[*key * sign] = StrMakeValid(label);

		sq_pop(vm, 2);
	}
	sq_pop(vm, 1);

	/* Check labels for completeness */
	config->complete_labels = true;
	for (int value = config->min_value; value <= config->max_value; value++) {
		if (config->labels.find(value) == config->labels.end()) {
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

const ScriptConfigItem *ScriptInfo::GetConfigItem(std::string_view name) const
{
	for (const auto &item : this->config_list) {
		if (item.name == name) return &item;
	}
	return nullptr;
}

int ScriptInfo::GetSettingDefaultValue(const std::string &name) const
{
	for (const auto &item : this->config_list) {
		if (item.name != name) continue;
		return item.default_value;
	}

	/* There is no such setting */
	return -1;
}
