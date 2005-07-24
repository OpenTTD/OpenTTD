/* $Id$ */

#ifndef SETTINGS_H
#define SETTINGS_H

enum SettingDescType {
	SDT_INTX, // must be 0
	SDT_ONEOFMANY,
	SDT_MANYOFMANY,
	SDT_BOOLX,
	SDT_STRING,
	SDT_STRINGBUF,
	SDT_INTLIST,
	SDT_STRINGQUOT, // string with quotation marks around it

	SDT_INT8 = 0 << 4,
	SDT_UINT8 = 1 << 4,
	SDT_INT16 = 2 << 4,
	SDT_UINT16 = 3 << 4,
	SDT_INT32 = 4 << 4,
	SDT_UINT32 = 5 << 4,
	SDT_CALLBX = 6 << 4,

	SDT_UINT = SDT_UINT32,
	SDT_INT = SDT_INT32,

	SDT_NOSAVE = 1 << 8,

	SDT_CALLB = SDT_INTX | SDT_CALLBX,

	SDT_BOOL = SDT_BOOLX | SDT_UINT8,
};

typedef enum {
	IGT_VARIABLES = 0, // values of the form "landscape = hilly"
	IGT_LIST = 1,      // a list of values, seperated by \n and terminated by the next group block
} IniGroupType;

typedef struct SettingDesc {
	const char *name;
	int flags;
	const void *def;
	void *ptr;
	const void *b;
} SettingDesc;

void IConsoleSetPatchSetting(const char *name, const char *value);
void IConsoleGetPatchSetting(const char *name);

#endif /* SETTINGS_H */
