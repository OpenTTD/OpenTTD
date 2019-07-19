// SFTODO: ALL THE MAGIC FORMATTING, #IFDEF, DOXYGEN FOR ALL ENTRIES ETC

#ifndef INI_HELPER_H
#define INI_HELPER_H

#include <map> // SFTODO?
#include "ini_type.h"
#include "strings_func.h" // SFTODO!?

typedef std::map<std::string, uint> EnumGroupMap;
const uint GET_ITEM_NO_DEFAULT = 0xffff;

bool GetGroup(IniLoadFile &ini_file, const char *group_name, bool optional, IniGroup **result);
bool GetStrGroupItem(IniGroup *group, const char *item_name, const char *default_value, const char **result);
bool GetEnumGroupItem(IniGroup *group, const char *item_name, uint default_value, const EnumGroupMap &lookup, uint *result);
bool GetUIntGroupItemWithValidation(IniGroup *group, const char *item_name, uint default_value, uint max_valid, uint *result);
bool GetByteGroupItemWithValidation(IniGroup *group, const char *item_name, uint default_value, uint max_valid, byte *result);

#endif
