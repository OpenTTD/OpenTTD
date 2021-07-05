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

#include <array>
#include "settings_internal.h"

typedef span<const SettingVariant> SettingTable;

extern SettingTable _settings;
extern SettingTable _network_settings;
extern SettingTable _network_private_settings;
extern SettingTable _network_secrets_settings;

extern SettingTable _company_settings;
extern SettingTable _currency_settings;
extern SettingTable _gameopt_settings;
extern SettingTable _misc_settings;
extern SettingTable _window_settings;
#if defined(_WIN32) && !defined(DEDICATED)
extern SettingTable _win32_settings;
#endif /* _WIN32 */

static const uint GAME_DIFFICULTY_NUM = 18;
extern const std::array<std::string, GAME_DIFFICULTY_NUM> _old_diff_settings;
extern uint16 _old_diff_custom[GAME_DIFFICULTY_NUM];

#endif /* SETTINGS_TABLE_H */
