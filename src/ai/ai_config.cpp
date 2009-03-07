/* $Id$ */

/** @file ai_config.cpp Implementation of AIConfig. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../settings_type.h"
#include "../core/random_func.hpp"
#include "ai.hpp"
#include "ai_config.hpp"

void AIConfig::ChangeAI(const char *name, int version)
{
	free((void *)this->name);
	this->name = (name == NULL) ? NULL : strdup(name);
	this->info = (name == NULL) ? NULL : AI::FindInfo(this->name, version);
	this->version = (info == NULL) ? -1 : info->GetVersion();
	if (this->config_list != NULL) delete this->config_list;
	this->config_list = (info == NULL) ? NULL : new AIConfigItemList();
	if (this->config_list != NULL) this->config_list->push_back(_start_date_config);

	/* The special casing for start_date is here to ensure that the
	 *  start_date setting won't change even if you chose another AI. */
	int start_date = this->GetSetting("start_date");

	for (SettingValueList::iterator it = this->settings.begin(); it != this->settings.end(); it++) {
		free((void*)(*it).first);
	}
	this->settings.clear();

	this->SetSetting("start_date", start_date);

	if (_game_mode == GM_NORMAL && this->info != NULL) {
		/* If we're in an existing game and the AI is changed, set all settings
		 *  for the AI that have the random flag to a random value. */
		for (AIConfigItemList::const_iterator it = this->info->GetConfigList()->begin(); it != this->info->GetConfigList()->end(); it++) {
			if ((*it).flags & AICONFIG_RANDOM) {
				this->SetSetting((*it).name, InteractiveRandomRange((*it).max_value - (*it).min_value) + (*it).min_value);
			}
		}
		this->AddRandomDeviation();
	}

}

AIConfig::AIConfig(const AIConfig *config)
{
	this->name = (config->name == NULL) ? NULL : strdup(config->name);
	this->info = config->info;
	this->version = config->version;
	this->config_list = NULL;

	for (SettingValueList::const_iterator it = config->settings.begin(); it != config->settings.end(); it++) {
		this->settings[strdup((*it).first)] = (*it).second;
	}
	this->AddRandomDeviation();
}

AIConfig::~AIConfig()
{
	free((void *)this->name);
	this->ResetSettings();
	if (this->config_list != NULL) delete this->config_list;
}

AIInfo *AIConfig::GetInfo()
{
	return this->info;
}

bool AIConfig::ResetInfo()
{
	this->info = AI::FindInfo(this->name, -1);
	return this->info != NULL;
}

const AIConfigItemList *AIConfig::GetConfigList()
{
	if (this->info != NULL) return this->info->GetConfigList();
	if (this->config_list == NULL) {
		this->config_list = new AIConfigItemList();
		this->config_list->push_back(_start_date_config);
	}
	return this->config_list;
}

AIConfig *AIConfig::GetConfig(CompanyID company, bool forceNewgameSetting)
{
	AIConfig **config;
	if (!forceNewgameSetting) {
		config = (_game_mode == GM_MENU) ? &_settings_newgame.ai_config[company] : &_settings_game.ai_config[company];
	} else {
		config = &_settings_newgame.ai_config[company];
	}
	if (*config == NULL) *config = new AIConfig();
	return *config;
}

int AIConfig::GetSetting(const char *name)
{
	SettingValueList::iterator it = this->settings.find(name);
	/* Return the default value if the setting is not set, or if we are in a not-custom difficult level */
	if (it == this->settings.end() || ((_game_mode == GM_MENU) ? _settings_newgame.difficulty.diff_level : _settings_game.difficulty.diff_level) != 3) {
		if (this->info == NULL) {
			assert(strcmp("start_date", name) == 0);
			switch ((_game_mode == GM_MENU) ? _settings_newgame.difficulty.diff_level : _settings_game.difficulty.diff_level) {
				case 0: return AI::START_NEXT_EASY;
				case 1: return AI::START_NEXT_MEDIUM;
				case 2: return AI::START_NEXT_HARD;
				case 3: return AI::START_NEXT_MEDIUM;
				default: NOT_REACHED();
			}
		}
		return this->info->GetSettingDefaultValue(name);
	}
	return (*it).second;
}

void AIConfig::SetSetting(const char *name, int value)
{
	/* You can only set ai specific settings if an AI is selected. */
	if (this->info == NULL && strcmp("start_date", name) != 0) return;

	if (this->info == NULL && strcmp("start_date", name) == 0) {
		value = Clamp(value, AI::START_NEXT_MIN, AI::START_NEXT_MAX);
	} else {
		const AIConfigItem *config_item = this->info->GetConfigItem(name);
		if (config_item == NULL) return;

		value = Clamp(value, config_item->min_value, config_item->max_value);
	}

	SettingValueList::iterator it = this->settings.find(name);
	if (it != this->settings.end()) {
		(*it).second = value;
	} else {
		this->settings[strdup(name)] = value;
	}
}

void AIConfig::ResetSettings()
{
	for (SettingValueList::iterator it = this->settings.begin(); it != this->settings.end(); it++) {
		free((void*)(*it).first);
	}
	this->settings.clear();
}

void AIConfig::AddRandomDeviation()
{
	for (AIConfigItemList::const_iterator it = this->GetConfigList()->begin(); it != this->GetConfigList()->end(); it++) {
		if ((*it).random_deviation != 0) {
			this->SetSetting((*it).name, InteractiveRandomRange((*it).random_deviation * 2) - (*it).random_deviation + this->GetSetting((*it).name));
		}
	}
}

bool AIConfig::HasAI()
{
	return this->info != NULL;
}

const char *AIConfig::GetName()
{
	return this->name;
}

int AIConfig::GetVersion()
{
	return this->version;
}

void AIConfig::StringToSettings(const char *value)
{
	char *value_copy = strdup(value);
	char *s = value_copy;

	while (s != NULL) {
		/* Analyze the string ('name=value,name=value\0') */
		char *item_name = s;
		s = strchr(s, '=');
		if (s == NULL) break;
		if (*s == '\0') break;
		*s = '\0';
		s++;

		char *item_value = s;
		s = strchr(s, ',');
		if (s != NULL) {
			*s = '\0';
			s++;
		}

		this->SetSetting(item_name, atoi(item_value));
	}
	free(value_copy);
}

void AIConfig::SettingsToString(char *string, size_t size)
{
	string[0] = '\0';
	for (SettingValueList::iterator it = this->settings.begin(); it != this->settings.end(); it++) {
		char no[10];
		snprintf(no, sizeof(no), "%d", (*it).second);

		/* Check if the string would fit in the destination */
		size_t needed_size = strlen((*it).first) + 1 + strlen(no) + 1;
		/* If it doesn't fit, skip the next settings */
		if (size <= needed_size) break;
		size -= needed_size;

		strcat(string, (*it).first);
		strcat(string, "=");
		strcat(string, no);
		strcat(string, ",");
	}
	/* Remove the last ',', but only if at least one setting was saved. */
	size_t len = strlen(string);
	if (len > 0) string[len - 1] = '\0';
}
