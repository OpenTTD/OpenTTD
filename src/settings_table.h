/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file settings_table.h Definition of the configuration tables of the settings.
 */

#ifndef SETTINGS_TABLE_H
#define SETTINGS_TABLE_H

#include "settings_internal.h"

extern SettingTable _company_settings;
extern SettingTable _currency_settings;
extern SettingTable _difficulty_settings;
extern SettingTable _economy_settings;
extern SettingTable _game_settings;
extern SettingTable _gui_settings;
extern SettingTable _linkgraph_settings;
extern SettingTable _locale_settings;
extern SettingTable _misc_settings;
extern SettingTable _multimedia_settings;
extern SettingTable _network_private_settings;
extern SettingTable _network_secrets_settings;
extern SettingTable _network_settings;
extern SettingTable _news_display_settings;
extern SettingTable _old_gameopt_settings;
extern SettingTable _pathfinding_settings;
extern SettingTable _script_settings;
extern SettingTable _window_settings;
extern SettingTable _world_settings;
#if defined(_WIN32) && !defined(DEDICATED)
extern SettingTable _win32_settings;
#endif /* _WIN32 */

static const uint GAME_DIFFICULTY_NUM = 18;
extern const std::array<std::string, GAME_DIFFICULTY_NUM> _old_diff_settings;
extern uint16_t _old_diff_custom[GAME_DIFFICULTY_NUM];

#endif /* SETTINGS_TABLE_H */
