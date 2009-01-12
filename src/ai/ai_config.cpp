/* $Id$ */

/** @file ai_config.cpp Implementation of AIConfig. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../settings_type.h"
#include "ai.hpp"
#include "ai_config.hpp"
#include "ai_info.hpp"

void AIConfig::ChangeAI(const char *name)
{
	free((void *)this->name);
	this->name = (name == NULL) ? NULL : strdup(name);
	this->info = (name == NULL) ? NULL : AI::GetCompanyInfo(this->name);

	for (SettingValueList::iterator it = this->settings.begin(); it != this->settings.end(); it++) {
		free((void*)(*it).first);
	}
	this->settings.clear();
}

AIConfig::AIConfig(const AIConfig *config) :
	name(NULL),
	info(NULL)
{
	this->name = (config->name == NULL) ? NULL : strdup(config->name);
	this->info = config->info;

	for (SettingValueList::const_iterator it = config->settings.begin(); it != config->settings.end(); it++) {
		this->settings[strdup((*it).first)] = (*it).second;
	}
}

AIConfig::~AIConfig()
{
	this->ChangeAI(NULL);
}

AIInfo *AIConfig::GetInfo()
{
	return this->info;
}

bool AIConfig::ResetInfo()
{
	 this->info = AI::GetCompanyInfo(this->name);
	 return this->info != NULL;
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
	assert(this->info != NULL);

	SettingValueList::iterator it = this->settings.find(name);
	/* Return the default value if the setting is not set, or if we are in a not-custom difficult level */
	if (it == this->settings.end() || ((_game_mode == GM_MENU) ? _settings_newgame.difficulty.diff_level : _settings_game.difficulty.diff_level) != 3) {
		return this->info->GetSettingDefaultValue(name);
	}
	return (*it).second;
}

void AIConfig::SetSetting(const char *name, int value)
{
	/* You can only set ai specific settings if an AI is selected. */
	assert(strcmp(name, "start_date") == 0 || this->info != NULL);

	SettingValueList::iterator it = this->settings.find(name);
	if (it != this->settings.end()) {
		(*it).second = value;
	} else {
		this->settings[strdup(name)] = value;
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

void AIConfig::SettingsToString(char *string, int size)
{
	string[0] = '\0';
	for (SettingValueList::iterator it = this->settings.begin(); it != this->settings.end(); it++) {
		char no[10];
		snprintf(no, sizeof(no), "%d", (*it).second);

		/* Check if the string would fit in the destination */
		size -= strlen((*it).first) - 1 - strlen(no) - 1;
		/* If it doesn't fit, skip the next settings */
		if (size <= 0) return;

		strcat(string, (*it).first);
		strcat(string, "=");
		strcat(string, no);
		strcat(string, ",");
	}
	string[strlen(string) - 1] = '\0';
}
