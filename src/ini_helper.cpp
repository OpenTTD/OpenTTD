/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ini_helper.cpp Helper functions for reading and validating values from IniLoadFile and other related objects. */

#include "stdafx.h"
#include "ini_helper.h"
#include "error.h"

#include "table/strings.h"

/**
 * Get hold of an IniGroup pointer for the named group in an IniLoadFile. If an error occurs a message is
 * shown to the user.
 *
 * @param ini_file the IniLoadFile to operate on
 * @param group_name the group name to get the IniGroup pointer for
 * @param optional if false, it's an error for group_name not to be present; if true, a null pointer will be
 *        returned in result if group_name is not present and the call is treated as a success.
 * @param[out] result if true is returned, updated to contain the IniGroup pointer; untouched if false is returned
 * @return true if the call succeeded, false if an error occurred
 */
bool GetGroup(IniLoadFile &ini_file, const char *group_name, bool optional, IniGroup **result)
{
	IniGroup *group = ini_file.GetGroup(group_name, 0, false);
	if ((group == nullptr) && !optional) {
		SetDParamStr(0, group_name);
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_GROUP_MISSING, INVALID_STRING_ID, WL_ERROR);
		return false;
	}
	*result = group;
	return true;
}

/**
 * Get hold of a string value for the named item in an IniGroup. If an error occurs a message is
 * shown to the user.
 *
 * @param group the IniGroup to operate on
 * @param item_name the item name within the IniGroup to get the value of
 * @param default_value if this is non-null, the call will succeed if item_name isn't present and this value will
 *        be returned in result; if this is null, it's an error for item_name not to be present.
 * @param[out] result if true is returned, updated to contain the string pointer; untouched if false is returned
 * @return true if the call succeeded, false if an error occurred
 */
bool GetStrGroupItem(IniGroup *group, const char *item_name, const char *default_value, const char **result)
{
	assert(item_name != nullptr);
	assert(result != nullptr);

	IniItem *item = group->GetItem(item_name, false);
	const char *item_value;
	if (item == nullptr) {
		if (default_value == nullptr) {
			SetDParamStr(0, group->name);
			SetDParamStr(1, item_name);
			ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_GROUP_MISSING_ITEM, INVALID_STRING_ID, WL_ERROR);
			return false;
		}
		item_value = default_value;
	} else {
		item_value = item->value;
	}

	*result = item_value;
	return true;
}

/**
 * Get hold of an enumeration value for the named item in an IniGroup. If an error occurs a message is
 * shown to the user.
 *
 * @param group the IniGroup to operate on
 * @param item_name the item name within the IniGroup to get the value of
 * @param default_value if this is GET_ITEM_NO_DEFAULT, it's an error if the item name is not present;
 *        otherwise the call will succeed and this will be returned if the item name isn't present.
 * @param lookup the acceptable string values for the name item with the corresponding enumeration values
 * @param[out] result if true is returned, updated to contain the enumeration value; untouched if false is returned
 * @return true if the call succeeded, false if an error occurred
 */
bool GetEnumGroupItem(IniGroup *group, const char *item_name, uint default_value, const EnumGroupMap &lookup, uint *result)
{
	const char *item_value;
	if (!GetStrGroupItem(group, item_name, (default_value == GET_ITEM_NO_DEFAULT) ? nullptr : "", &item_value)) return false;
	if (*item_value == '\0') {
		*result = default_value;
		return true;
	}
	// EHTODO: This will be case-sensitive. Does that matter? If it does, what's the nicest way to handle it? Just iterate
	// through the map ourselves (it won't be huge) and use strnatcmp() to compare item_value with each key in turn?
	auto it = lookup.find(std::string(item_value));
	if (it == lookup.end()) {
		SetDParamStr(0, group->name);
		SetDParamStr(1, item_name);
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_GROUP_INVALID_ENUM, INVALID_STRING_ID, WL_ERROR);
		return false;
	}
	*result = it->second;
	return true;
}

/**
 * Get hold of a uint value for the named item in an IniGroup. If an error occurs a message is
 * shown to the user.
 *
 * @param group the IniGroup to operate on
 * @param item_name the item name within the IniGroup to get the value of
 * @param default_value if this is GET_ITEM_NO_DEFAULT, it's an error if the item name is not present;
 *        otherwise the call will succeed and this will be returned if the item name isn't present.
 * @param max_value if the value of the item is strictly greater than this it's treated as an error
 * @param[out] result if true is returned, updated to contain the item value; untouched if false is returned
 * @return true if the call succeeded, false if an error occurred
 */
bool GetUIntGroupItemWithValidation(IniGroup *group, const char *item_name, uint default_value, uint max_valid, uint *result)
{
	const char *item_value;
	if (!GetStrGroupItem(group, item_name, (default_value == GET_ITEM_NO_DEFAULT) ? nullptr : "", &item_value)) return false;
	if (*item_value == '\0') {
		*result = default_value;
		return true;
	}

	for (const char *p = item_value; *p != '\0'; ++p) {
		if (!isdigit(static_cast<unsigned char>(*p))) {
			SetDParamStr(0, group->name);
			SetDParamStr(1, item_name);
			ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_GROUP_NONNUMERIC_ITEM, INVALID_STRING_ID, WL_ERROR);
			return false;
		}
	}

	uint u = static_cast<uint>(strtoul(item_value, nullptr, 10));
	if (u > max_valid) {
		SetDParamStr(0, group->name);
		SetDParamStr(1, item_name);
		SetDParam(2, u);
		ShowErrorMessage(STR_MAPGEN_HEIGHTMAP_ERROR_GROUP_ITEM_TOO_LARGE, INVALID_STRING_ID, WL_ERROR);
		return false;
	}

	*result = u;
	return true;
}

/** As GetUIntGroupItemWithValidation() but returning a byte instead of a uint. */
bool GetByteGroupItemWithValidation(IniGroup *group, const char *item_name, uint default_value, uint max_valid, byte *result)
{
	assert(max_valid <= 255);
	uint result_uint;
	if (!GetUIntGroupItemWithValidation(group, item_name, default_value, max_valid, &result_uint)) return false;
	*result = static_cast<byte>(result_uint);
	return true;
}

