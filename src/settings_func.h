/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file settings_func.h Functions related to setting/changing the settings. */

#ifndef SETTINGS_FUNC_H
#define SETTINGS_FUNC_H

#include "company_type.h"
#include "string_type.h"

struct IniFile;

void IConsoleSetSetting(const char *name, const char *value, bool force_newgame = false);
void IConsoleSetSetting(const char *name, int32_t value);
void IConsoleGetSetting(const char *name, bool force_newgame = false);
void IConsoleListSettings(const char *prefilter);

void LoadFromConfig(bool minimal = false);
void SaveToConfig();

void IniLoadWindowSettings(IniFile &ini, const char *grpname, void *desc);
void IniSaveWindowSettings(IniFile &ini, const char *grpname, void *desc);

StringList GetGRFPresetList();
struct GRFConfig *LoadGRFPresetFromConfig(const char *config_name);
void SaveGRFPresetToConfig(const char *config_name, struct GRFConfig *config);
void DeleteGRFPresetFromConfig(const char *config_name);

void SetDefaultCompanySettings(CompanyID cid);

void SyncCompanySettings();

#endif /* SETTINGS_FUNC_H */
