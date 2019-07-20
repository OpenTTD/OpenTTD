/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ini_helper.h Helper functions for reading and validating values from IniLoadFile and other related objects. */

#ifndef INI_HELPER_H
#define INI_HELPER_H

#include <map>
#include "ini_type.h"

typedef std::map<std::string, uint> EnumGroupMap;
const uint GET_ITEM_NO_DEFAULT = 0xffff;

bool GetGroup(IniLoadFile &ini_file, const char *group_name, bool optional, IniGroup **result);
bool GetStrGroupItem(IniGroup *group, const char *item_name, const char *default_value, const char **result);
bool GetEnumGroupItem(IniGroup *group, const char *item_name, uint default_value, const EnumGroupMap &lookup, uint *result);
bool GetUIntGroupItemWithValidation(IniGroup *group, const char *item_name, uint default_value, uint max_valid, uint *result);
bool GetByteGroupItemWithValidation(IniGroup *group, const char *item_name, uint default_value, uint max_valid, byte *result);

#endif
