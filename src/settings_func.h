/* $Id$ */

/** @file settings_func.h Functions related to setting/changing the settings. */

#ifndef SETTINGS_FUNC_H
#define SETTINGS_FUNC_H

bool IConsoleSetPatchSetting(const char *name, int32 value);
void IConsoleGetPatchSetting(const char *name);
void IConsoleListPatches();

void LoadFromConfig();
void SaveToConfig();
void CheckConfig();
void UpdatePatches();

#endif /* SETTINGS_FUNC_H */
