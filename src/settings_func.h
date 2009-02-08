/* $Id$ */

/** @file settings_func.h Functions related to setting/changing the settings. */

#ifndef SETTINGS_FUNC_H
#define SETTINGS_FUNC_H

#include "core/smallvec_type.hpp"

void IConsoleSetSetting(const char *name, const char *value);
void IConsoleSetSetting(const char *name, int32 value);
void IConsoleGetSetting(const char *name);
void IConsoleListSettings(const char *prefilter);

void LoadFromConfig();
void SaveToConfig();
void CheckConfig();

/* Functions to load and save NewGRF settings to a separate
 * configuration file, used for presets. */
typedef AutoFreeSmallVector<char *, 4> GRFPresetList;

void GetGRFPresetList(GRFPresetList *list);
struct GRFConfig *LoadGRFPresetFromConfig(const char *config_name);
void SaveGRFPresetToConfig(const char *config_name, struct GRFConfig *config);
void DeleteGRFPresetFromConfig(const char *config_name);

#endif /* SETTINGS_FUNC_H */
