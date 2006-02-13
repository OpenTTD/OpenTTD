/* $Id$ */

#ifndef SETTINGS_H
#define SETTINGS_H

/* Convention/Type of settings. This will be merged mostly with the SaveLoad
 * SLE_ enums. So it looks a bit strange. The layout is as follows:
 * bits 0-7: the type (size) of the variable. Eg int8, uint8, bool, etc. Same as VarTypes
 * bits 8-15: the generic variable type. Eg string, oneofmany, number, intlist
 * bits 16-31: the size of a string, an intlist (which is an implicit array). */
/* XXX - the GenericType will NOT be shifted in the final implementation, just for compatility */
enum SettingDescType {
	/* 4 bytes allocated a maximum of 16 types for GenericType */
	SDT_NUMX        = 0 << 8, // value must be 0!!, refers to any number-type
	SDT_BOOLX       = 1 << 8, // a boolean number
	SDT_ONEOFMANY   = 2 << 8, // bitmasked number where only ONE bit may be set
	SDT_MANYOFMANY  = 3 << 8, // bitmasked number where MULTIPLE bits may be set
	SDT_INTLIST     = 4 << 8, // list of integers seperated by a comma ','
	SDT_STRING      = 5 << 8, // string which is only a pointer, so needs dynamic allocation
	SDT_STRINGBUF   = 6 << 8, // string with a fixed length, preset buffer
	SDT_STRINGQUOT  = 7 << 8, // string with quotation marks around it (enables spaces in string)
	SDT_CHAR        = 8 << 8, // single character
	/* 7 more possible primitives */

	/* 4 bytes allocated a maximum of 16 types for NumberType */
	SDT_INT8        = 0 << 4,
	SDT_UINT8       = 1 << 4,
	SDT_INT16       = 2 << 4,
	SDT_UINT16      = 3 << 4,
	SDT_INT32       = 4 << 4,
	SDT_UINT32      = 5 << 4,
	SDT_INT64       = 6 << 4,
	SDT_UINT64      = 7 << 4,
	/* 8 more possible primitives */

	/* Shortcut values */
	SDT_BOOL = SDT_BOOLX | SDT_UINT8,
	SDT_UINT = SDT_UINT32,
	SDT_INT  = SDT_INT32,
	SDT_STR  = SDT_STRING,
	SDT_STRB = SDT_STRINGBUF,
	SDT_STRQ = SDT_STRINGQUOT,

	/* The value is read from the configuration file but not saved */
	SDT_NOSAVE = 1 << 31,
};

typedef enum {
	IGT_VARIABLES = 0, // values of the form "landscape = hilly"
	IGT_LIST = 1,      // a list of values, seperated by \n and terminated by the next group block
} IniGroupType;

typedef struct SettingDesc {
	const char *name;
	uint32 flags;
	const void *def;
	void *ptr;
	const void *many;
} SettingDesc;

void IConsoleSetPatchSetting(const char *name, const char *value);
void IConsoleGetPatchSetting(const char *name);

#endif /* SETTINGS_H */
