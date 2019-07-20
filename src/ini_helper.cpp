// SFTODO: ALL THE MAGIC FORMATTING

#include <iostream> // SFTODO TEMP
#include "stdafx.h"
#include "ini_helper.h"
#include "error.h"

#include "table/strings.h"

// SFTODO DOXYGEN
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

// SFTODO DOXYGEN
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

// SFTODO: DOXYGEN
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

// SFTODO: DOXYGEN
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
	std::cout << "SFTODOU " << u << std::endl;
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

// SFTODO: DOXYGEN
bool GetByteGroupItemWithValidation(IniGroup *group, const char *item_name, uint default_value, uint max_valid, byte *result)
{
	assert(max_valid <= 255);
	uint result_uint;
	if (!GetUIntGroupItemWithValidation(group, item_name, default_value, max_valid, &result_uint)) return false;
	*result = static_cast<byte>(result_uint);
	return true;
}

