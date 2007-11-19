/* $Id$ */

/** @file settings.cpp
 * All actions handling saving and loading of the settings/configuration goes on in this file.
 * The file consists of four parts:
 * <ol>
 * <li>Parsing the configuration file (openttd.cfg). This is achieved with the ini_ functions which
 *     handle various types, such as normal 'key = value' pairs, lists and value combinations of
 *     lists, strings, integers, 'bit'-masks and element selections.
 * <li>Defining the data structures that go into the configuration. These include for example
 *     the _patches struct, but also network-settings, banlists, newgrf, etc. There are a lot
 *     of helper macros available for the various types, and also saving/loading of these settings
 *     in a savegame is handled inside these structures.
 * <li>Handle reading and writing to the setting-structures from inside the game either from
 *     the console for example or through the gui with CMD_ functions.
 * <li>Handle saving/loading of the PATS chunk inside the savegame.
 * </ol>
 * @see SettingDesc
 * @see SaveLoad
 */

#include "stdafx.h"
#include "openttd.h"
#include "currency.h"
#include "functions.h"
#include "macros.h"
#include "screenshot.h"
#include "sound.h"
#include "string.h"
#include "variables.h"
#include "network/network.h"
#include "strings.h"
#include "settings.h"
#include "command.h"
#include "console.h"
#include "saveload.h"
#include "npf.h"
#include "yapf/yapf.h"
#include "newgrf.h"
#include "newgrf_config.h"
#include "genworld.h"
#include "date.h"
#include "rail.h"
#include "train.h"
#include "helpers.hpp"
#include "news.h"
#ifdef WITH_FREETYPE
#include "gfx.h"
#include "fontcache.h"
#endif
#include "spritecache.h"
#include "transparency.h"

/** The patch values that are used for new games and/or modified in config file */
Patches _patches_newgame;

struct IniFile;
struct IniItem;
struct IniGroup;
struct SettingsMemoryPool;

typedef const char *SettingListCallbackProc(const IniItem *item, uint index);
typedef void SettingDescProc(IniFile *ini, const SettingDesc *desc, const char *grpname, void *object);
typedef void SettingDescProcList(IniFile *ini, const char *grpname, char **list, uint len, SettingListCallbackProc proc);

static void pool_init(SettingsMemoryPool **pool);
static void *pool_alloc(SettingsMemoryPool **pool, uint size);
static void *pool_strdup(SettingsMemoryPool **pool, const char *mem, uint size);
static void pool_free(SettingsMemoryPool **pool);
static bool IsSignedVarMemType(VarType vt);

struct SettingsMemoryPool {
	uint pos,size;
	SettingsMemoryPool *next;
	byte mem[1];
};

static SettingsMemoryPool *pool_new(uint minsize)
{
	SettingsMemoryPool *p;
	if (minsize < 4096 - 12) minsize = 4096 - 12;

	p = (SettingsMemoryPool*)malloc(sizeof(SettingsMemoryPool) - 1 + minsize);
	p->pos = 0;
	p->size = minsize;
	p->next = NULL;
	return p;
}

static void pool_init(SettingsMemoryPool **pool)
{
	*pool = pool_new(0);
}

static void *pool_alloc(SettingsMemoryPool **pool, uint size)
{
	uint pos;
	SettingsMemoryPool *p = *pool;

	size = Align(size, sizeof(void*));

	/* first check if there's memory in the next pool */
	if (p->next && p->next->pos + size <= p->next->size) {
		p = p->next;
	/* then check if there's not memory in the cur pool */
	} else if (p->pos + size > p->size) {
		SettingsMemoryPool *n = pool_new(size);
		*pool = n;
		n->next = p;
		p = n;
	}

	pos = p->pos;
	p->pos += size;
	return p->mem + pos;
}

static void *pool_strdup(SettingsMemoryPool **pool, const char *mem, uint size)
{
	byte *p = (byte*)pool_alloc(pool, size + 1);
	p[size] = 0;
	memcpy(p, mem, size);
	return p;
}

static void pool_free(SettingsMemoryPool **pool)
{
	SettingsMemoryPool *p = *pool, *n;
	*pool = NULL;
	while (p) {
		n = p->next;
		free(p);
		p = n;
	}
}

/** structs describing the ini format. */
struct IniItem {
	char *name;
	char *value;
	char *comment;
	IniItem *next;
};

struct IniGroup {
	char *name;        ///< name of group
	char *comment;     ///<comment for group
	IniItem *item, **last_item;
	IniGroup *next;
	IniFile *ini;
	IniGroupType type; ///< type of group
};

struct IniFile {
	SettingsMemoryPool *pool;
	IniGroup *group, **last_group;
	char *comment;     ///< last comment in file
};

/** allocate an inifile object */
static IniFile *ini_alloc()
{
	IniFile *ini;
	SettingsMemoryPool *pool;
	pool_init(&pool);
	ini = (IniFile*)pool_alloc(&pool, sizeof(IniFile));
	ini->pool = pool;
	ini->group = NULL;
	ini->last_group = &ini->group;
	ini->comment = NULL;
	return ini;
}

/** allocate an ini group object */
static IniGroup *ini_group_alloc(IniFile *ini, const char *grpt, int len)
{
	IniGroup *grp = (IniGroup*)pool_alloc(&ini->pool, sizeof(IniGroup));
	grp->ini = ini;
	grp->name = (char*)pool_strdup(&ini->pool, grpt, len);
	if (!strcmp(grp->name, "newgrf") || !strcmp(grp->name, "servers") || !strcmp(grp->name, "bans")) {
		grp->type = IGT_LIST;
	} else {
		grp->type = IGT_VARIABLES;
	}
	grp->next = NULL;
	grp->item = NULL;
	grp->comment = NULL;
	grp->last_item = &grp->item;
	*ini->last_group = grp;
	ini->last_group = &grp->next;
	return grp;
}

static IniItem *ini_item_alloc(IniGroup *group, const char *name, int len)
{
	IniItem *item = (IniItem*)pool_alloc(&group->ini->pool, sizeof(IniItem));
	item->name = (char*)pool_strdup(&group->ini->pool, name, len);
	item->next = NULL;
	item->comment = NULL;
	item->value = NULL;
	*group->last_item = item;
	group->last_item = &item->next;
	return item;
}

/** load an ini file into the "abstract" format */
static IniFile *ini_load(const char *filename)
{
	char buffer[1024], c, *s, *t, *e;
	FILE *in;
	IniFile *ini;
	IniGroup *group = NULL;
	IniItem *item;

	char *comment = NULL;
	uint comment_size = 0;
	uint comment_alloc = 0;

	ini = ini_alloc();

	in = fopen(filename, "r");
	if (in == NULL) return ini;

	/* for each line in the file */
	while (fgets(buffer, sizeof(buffer), in)) {

		/* trim whitespace from the left side */
		for (s = buffer; *s == ' ' || *s == '\t'; s++);

		/* trim whitespace from right side. */
		e = s + strlen(s);
		while (e > s && ((c=e[-1]) == '\n' || c == '\r' || c == ' ' || c == '\t')) e--;
		*e = '\0';

		/* skip comments and empty lines */
		if (*s == '#' || *s == ';' || *s == '\0') {
			uint ns = comment_size + (e - s + 1);
			uint a = comment_alloc;
			uint pos;
			/* add to comment */
			if (ns > a) {
				a = max(a, 128U);
				do a*=2; while (a < ns);
				comment = ReallocT(comment, comment_alloc = a);
			}
			pos = comment_size;
			comment_size += (e - s + 1);
			comment[pos + e - s] = '\n'; // comment newline
			memcpy(comment + pos, s, e - s); // copy comment contents
			continue;
		}

		/* it's a group? */
		if (s[0] == '[') {
			if (e[-1] != ']') {
				ShowInfoF("ini: invalid group name '%s'", buffer);
			} else {
				e--;
			}
			s++; // skip [
			group = ini_group_alloc(ini, s, e - s);
			if (comment_size) {
				group->comment = (char*)pool_strdup(&ini->pool, comment, comment_size);
				comment_size = 0;
			}
		} else if (group) {
			/* find end of keyname */
			if (*s == '\"') {
				s++;
				for (t = s; *t != '\0' && *t != '\"'; t++);
				if (*t == '\"') *t = ' ';
			} else {
				for (t = s; *t != '\0' && *t != '=' && *t != '\t' && *t != ' '; t++);
			}

			/* it's an item in an existing group */
			item = ini_item_alloc(group, s, t-s);
			if (comment_size) {
				item->comment = (char*)pool_strdup(&ini->pool, comment, comment_size);
				comment_size = 0;
			}

			/* find start of parameter */
			while (*t == '=' || *t == ' ' || *t == '\t') t++;


			/* remove starting quotation marks */
			if (*t == '\"') t++;
			/* remove ending quotation marks */
			e = t + strlen(t);
			if (e > t && e[-1] == '\"') e--;
			*e = '\0';

			item->value = (char*)pool_strdup(&ini->pool, t, e - t);
		} else {
			/* it's an orphan item */
			ShowInfoF("ini: '%s' outside of group", buffer);
		}
	}

	if (comment_size > 0) {
		ini->comment = (char*)pool_strdup(&ini->pool, comment, comment_size);
		comment_size = 0;
	}

	free(comment);
	fclose(in);

	return ini;
}

/** lookup a group or make a new one */
static IniGroup *ini_getgroup(IniFile *ini, const char *name, int len)
{
	IniGroup *group;

	if (len == -1) len = strlen(name);

	/* does it exist already? */
	for (group = ini->group; group; group = group->next)
		if (!memcmp(group->name, name, len) && group->name[len] == 0)
			return group;

	/* otherwise make a new one */
	group = ini_group_alloc(ini, name, len);
	group->comment = (char*)pool_strdup(&ini->pool, "\n", 1);
	return group;
}

/** lookup an item or make a new one */
static IniItem *ini_getitem(IniGroup *group, const char *name, bool create)
{
	IniItem *item;
	uint len = strlen(name);

	for (item = group->item; item; item = item->next)
		if (strcmp(item->name, name) == 0) return item;

	if (!create) return NULL;

	/* otherwise make a new one */
	return ini_item_alloc(group, name, len);
}

/** save ini file from the "abstract" format. */
static bool ini_save(const char *filename, IniFile *ini)
{
	FILE *f;
	IniGroup *group;
	IniItem *item;

	f = fopen(filename, "w");
	if (f == NULL) return false;

	for (group = ini->group; group != NULL; group = group->next) {
		if (group->comment) fputs(group->comment, f);
		fprintf(f, "[%s]\n", group->name);
		for (item = group->item; item != NULL; item = item->next) {
			assert(item->value != NULL);
			if (item->comment != NULL) fputs(item->comment, f);

			/* protect item->name with quotes if needed */
			if (strchr(item->name, ' ') != NULL) {
				fprintf(f, "\"%s\"", item->name);
			} else {
				fprintf(f, "%s", item->name);
			}

			/* Don't give an equal sign to list items that don't have a parameter */
			if (group->type == IGT_LIST && *item->value == '\0') {
				fprintf(f, "\n");
			} else {
				fprintf(f, " = %s\n", item->value);
			}
		}
	}
	if (ini->comment) fputs(ini->comment, f);

	fclose(f);
	return true;
}

static void ini_free(IniFile *ini)
{
	pool_free(&ini->pool);
}

/** Find the index value of a ONEofMANY type in a string seperated by |
 * @param many full domain of values the ONEofMANY setting can have
 * @param one the current value of the setting for which a value needs found
 * @param onelen force calculation of the *one parameter
 * @return the integer index of the full-list, or -1 if not found */
static int lookup_oneofmany(const char *many, const char *one, int onelen)
{
	const char *s;
	int idx;

	if (onelen == -1) onelen = strlen(one);

	/* check if it's an integer */
	if (*one >= '0' && *one <= '9')
		return strtoul(one, NULL, 0);

	idx = 0;
	for (;;) {
		/* find end of item */
		s = many;
		while (*s != '|' && *s != 0) s++;
		if (s - many == onelen && !memcmp(one, many, onelen)) return idx;
		if (*s == 0) return -1;
		many = s + 1;
		idx++;
	}
}

/** Find the set-integer value MANYofMANY type in a string
 * @param many full domain of values the MANYofMANY setting can have
 * @param str the current string value of the setting, each individual
 * of seperated by a whitespace,tab or | character
 * @return the 'fully' set integer, or -1 if a set is not found */
static uint32 lookup_manyofmany(const char *many, const char *str)
{
	const char *s;
	int r;
	uint32 res = 0;

	for (;;) {
		/* skip "whitespace" */
		while (*str == ' ' || *str == '\t' || *str == '|') str++;
		if (*str == 0) break;

		s = str;
		while (*s != 0 && *s != ' ' && *s != '\t' && *s != '|') s++;

		r = lookup_oneofmany(many, str, s - str);
		if (r == -1) return (uint32)-1;

		SETBIT(res, r); // value found, set it
		if (*s == 0) break;
		str = s + 1;
	}
	return res;
}

/** Parse an integerlist string and set each found value
 * @param p the string to be parsed. Each element in the list is seperated by a
 * comma or a space character
 * @param items pointer to the integerlist-array that will be filled with values
 * @param maxitems the maximum number of elements the integerlist-array has
 * @return returns the number of items found, or -1 on an error */
static int parse_intlist(const char *p, int *items, int maxitems)
{
	int n = 0, v;
	char *end;

	for (;;) {
		v = strtol(p, &end, 0);
		if (p == end || n == maxitems) return -1;
		p = end;
		items[n++] = v;
		if (*p == '\0') break;
		if (*p != ',' && *p != ' ') return -1;
		p++;
	}

	return n;
}

/** Load parsed string-values into an integer-array (intlist)
 * @param str the string that contains the values (and will be parsed)
 * @param array pointer to the integer-arrays that will be filled
 * @param nelems the number of elements the array holds. Maximum is 64 elements
 * @param type the type of elements the array holds (eg INT8, UINT16, etc.)
 * @return return true on success and false on error */
static bool load_intlist(const char *str, void *array, int nelems, VarType type)
{
	int items[64];
	int i, nitems;

	if (str == NULL) {
		memset(items, 0, sizeof(items));
		nitems = nelems;
	} else {
		nitems = parse_intlist(str, items, lengthof(items));
		if (nitems != nelems) return false;
	}

	switch (type) {
	case SLE_VAR_BL:
	case SLE_VAR_I8:
	case SLE_VAR_U8:
		for (i = 0; i != nitems; i++) ((byte*)array)[i] = items[i];
		break;
	case SLE_VAR_I16:
	case SLE_VAR_U16:
		for (i = 0; i != nitems; i++) ((uint16*)array)[i] = items[i];
		break;
	case SLE_VAR_I32:
	case SLE_VAR_U32:
		for (i = 0; i != nitems; i++) ((uint32*)array)[i] = items[i];
		break;
	default: NOT_REACHED();
	}

	return true;
}

/** Convert an integer-array (intlist) to a string representation. Each value
 * is seperated by a comma or a space character
 * @param buf output buffer where the string-representation will be stored
 * @param array pointer to the integer-arrays that is read from
 * @param nelems the number of elements the array holds.
 * @param type the type of elements the array holds (eg INT8, UINT16, etc.) */
static void make_intlist(char *buf, const void *array, int nelems, VarType type)
{
	int i, v = 0;
	const byte *p = (const byte*)array;

	for (i = 0; i != nelems; i++) {
		switch (type) {
		case SLE_VAR_BL:
		case SLE_VAR_I8:  v = *(int8*)p;   p += 1; break;
		case SLE_VAR_U8:  v = *(byte*)p;   p += 1; break;
		case SLE_VAR_I16: v = *(int16*)p;  p += 2; break;
		case SLE_VAR_U16: v = *(uint16*)p; p += 2; break;
		case SLE_VAR_I32: v = *(int32*)p;  p += 4; break;
		case SLE_VAR_U32: v = *(uint32*)p; p += 4; break;
		default: NOT_REACHED();
		}
		buf += sprintf(buf, (i == 0) ? "%d" : ",%d", v);
	}
}

/** Convert a ONEofMANY structure to a string representation.
 * @param buf output buffer where the string-representation will be stored
 * @param many the full-domain string of possible values
 * @param id the value of the variable and whose string-representation must be found */
static void make_oneofmany(char *buf, const char *many, int id)
{
	int orig_id = id;

	/* Look for the id'th element */
	while (--id >= 0) {
		for (; *many != '|'; many++) {
			if (*many == '\0') { // not found
				sprintf(buf, "%d", orig_id);
				return;
			}
		}
		many++; // pass the |-character
	}

	/* copy string until next item (|) or the end of the list if this is the last one */
	while (*many != '\0' && *many != '|') *buf++ = *many++;
	*buf = '\0';
}

/** Convert a MANYofMANY structure to a string representation.
 * @param buf output buffer where the string-representation will be stored
 * @param many the full-domain string of possible values
 * @param x the value of the variable and whose string-representation must
 *        be found in the bitmasked many string */
static void make_manyofmany(char *buf, const char *many, uint32 x)
{
	const char *start;
	int i = 0;
	bool init = true;

	for (; x != 0; x >>= 1, i++) {
		start = many;
		while (*many != 0 && *many != '|') many++; // advance to the next element

		if (HASBIT(x, 0)) { // item found, copy it
			if (!init) *buf++ = '|';
			init = false;
			if (start == many) {
				buf += sprintf(buf, "%d", i);
			} else {
				memcpy(buf, start, many - start);
				buf += many - start;
			}
		}

		if (*many == '|') many++;
	}

	*buf = '\0';
}

/** Convert a string representation (external) of a setting to the internal rep.
 * @param desc SettingDesc struct that holds all information about the variable
 * @param str input string that will be parsed based on the type of desc
 * @return return the parsed value of the setting */
static const void *string_to_val(const SettingDescBase *desc, const char *str)
{
	switch (desc->cmd) {
	case SDT_NUMX: {
		char *end;
		unsigned long val = strtoul(str, &end, 0);
		if (*end != '\0') ShowInfoF("ini: trailing characters at end of setting '%s'", desc->name);
		return (void*)val;
	}
	case SDT_ONEOFMANY: {
		long r = lookup_oneofmany(desc->many, str, -1);
		/* if the first attempt of conversion from string to the appropriate value fails,
		 * look if we have defined a converter from old value to new value. */
		if (r == -1 && desc->proc_cnvt != NULL) r = desc->proc_cnvt(str);
		if (r != -1) return (void*)r; //and here goes converted value
		ShowInfoF("ini: invalid value '%s' for '%s'", str, desc->name); //sorry, we failed
		return 0;
	}
	case SDT_MANYOFMANY: {
		unsigned long r = lookup_manyofmany(desc->many, str);
		if (r != (unsigned long)-1) return (void*)r;
		ShowInfoF("ini: invalid value '%s' for '%s'", str, desc->name);
		return 0;
	}
	case SDT_BOOLX:
		if (strcmp(str, "true")  == 0 || strcmp(str, "on")  == 0 || strcmp(str, "1") == 0)
			return (void*)true;
		if (strcmp(str, "false") == 0 || strcmp(str, "off") == 0 || strcmp(str, "0") == 0)
			return (void*)false;
		ShowInfoF("ini: invalid setting value '%s' for '%s'", str, desc->name);
		break;

	case SDT_STRING:
	case SDT_INTLIST: return str;
	default: break;
	}

	return NULL;
}

/** Set the value of a setting and if needed clamp the value to
 * the preset minimum and maximum.
 * @param ptr the variable itself
 * @param sd pointer to the 'information'-database of the variable
 * @param val signed long version of the new value
 * @pre SettingDesc is of type SDT_BOOLX, SDT_NUMX,
 * SDT_ONEOFMANY or SDT_MANYOFMANY. Other types are not supported as of now */
static void Write_ValidateSetting(void *ptr, const SettingDesc *sd, int32 val)
{
	const SettingDescBase *sdb = &sd->desc;

	if (sdb->cmd != SDT_BOOLX &&
			sdb->cmd != SDT_NUMX &&
			sdb->cmd != SDT_ONEOFMANY &&
			sdb->cmd != SDT_MANYOFMANY) {
		return;
	}

	/* We cannot know the maximum value of a bitset variable, so just have faith */
	if (sdb->cmd != SDT_MANYOFMANY) {
		/* We need to take special care of the uint32 type as we receive from the function
		 * a signed integer. While here also bail out on 64-bit settings as those are not
		 * supported. Unsigned 8 and 16-bit variables are safe since they fit into a signed
		 * 32-bit variable
		 * TODO: Support 64-bit settings/variables */
		switch (GetVarMemType(sd->save.conv)) {
			case SLE_VAR_BL:
			case SLE_VAR_I8:
			case SLE_VAR_U8:
			case SLE_VAR_I16:
			case SLE_VAR_U16:
			case SLE_VAR_I32: {
				/* Override the minimum value. No value below sdb->min, except special value 0 */
				int32 min = ((sdb->flags & SGF_0ISDISABLED) && val <= sdb->min) ? 0 : sdb->min;
				val = Clamp(val, min, sdb->max);
			} break;
			case SLE_VAR_U32: {
				/* Override the minimum value. No value below sdb->min, except special value 0 */
				uint min = ((sdb->flags & SGF_0ISDISABLED) && (uint)val <= (uint)sdb->min) ? 0 : sdb->min;
				WriteValue(ptr, SLE_VAR_U32, (int64)ClampU(val, min, sdb->max));
				return;
			}
			case SLE_VAR_I64:
			case SLE_VAR_U64:
			default: NOT_REACHED(); break;
		}
	}

	WriteValue(ptr, sd->save.conv, (int64)val);
}

/** Load values from a group of an IniFile structure into the internal representation
 * @param ini pointer to IniFile structure that holds administrative information
 * @param sd pointer to SettingDesc structure whose internally pointed variables will
 *        be given values
 * @param grpname the group of the IniFile to search in for the new values
 * @param object pointer to the object been loaded */
static void ini_load_settings(IniFile *ini, const SettingDesc *sd, const char *grpname, void *object)
{
	IniGroup *group;
	IniGroup *group_def = ini_getgroup(ini, grpname, -1);
	IniItem *item;
	const void *p;
	void *ptr;
	const char *s;

	for (; sd->save.cmd != SL_END; sd++) {
		const SettingDescBase *sdb = &sd->desc;
		const SaveLoad        *sld = &sd->save;

		if (!SlIsObjectCurrentlyValid(sld->version_from, sld->version_to)) continue;

		/* XXX - wtf is this?? (group override?) */
		s = strchr(sdb->name, '.');
		if (s != NULL) {
			group = ini_getgroup(ini, sdb->name, s - sdb->name);
			s++;
		} else {
			s = sdb->name;
			group = group_def;
		}

		item = ini_getitem(group, s, false);
		p = (item == NULL) ? sdb->def : string_to_val(sdb, item->value);
		ptr = GetVariableAddress(object, sld);

		switch (sdb->cmd) {
		case SDT_BOOLX: /* All four are various types of (integer) numbers */
		case SDT_NUMX:
		case SDT_ONEOFMANY:
		case SDT_MANYOFMANY:
			Write_ValidateSetting(ptr, sd, (unsigned long)p); break;

		case SDT_STRING:
			switch (GetVarMemType(sld->conv)) {
				case SLE_VAR_STRB:
				case SLE_VAR_STRBQ:
					if (p != NULL) ttd_strlcpy((char*)ptr, (const char*)p, sld->length);
					break;
				case SLE_VAR_STR:
				case SLE_VAR_STRQ:
					if (p != NULL) {
						free(*(char**)ptr);
						*(char**)ptr = strdup((const char*)p);
					}
					break;
				case SLE_VAR_CHAR: *(char*)ptr = *(char*)p; break;
				default: NOT_REACHED(); break;
			}
			break;

		case SDT_INTLIST: {
			if (!load_intlist((const char*)p, ptr, sld->length, GetVarMemType(sld->conv)))
				ShowInfoF("ini: error in array '%s'", sdb->name);
			break;
		}
		default: NOT_REACHED(); break;
		}
	}
}

/** Save the values of settings to the inifile.
 * @param ini pointer to IniFile structure
 * @param sd read-only SettingDesc structure which contains the unmodified,
 *        loaded values of the configuration file and various information about it
 * @param grpname holds the name of the group (eg. [network]) where these will be saved
 * @param object pointer to the object been saved
 * The function works as follows: for each item in the SettingDesc structure we
 * have a look if the value has changed since we started the game (the original
 * values are reloaded when saving). If settings indeed have changed, we get
 * these and save them.
 */
static void ini_save_settings(IniFile *ini, const SettingDesc *sd, const char *grpname, void *object)
{
	IniGroup *group_def = NULL, *group;
	IniItem *item;
	char buf[512];
	const char *s;
	void *ptr;

	for (; sd->save.cmd != SL_END; sd++) {
		const SettingDescBase *sdb = &sd->desc;
		const SaveLoad        *sld = &sd->save;

		/* If the setting is not saved to the configuration
		 * file, just continue with the next setting */
		if (!SlIsObjectCurrentlyValid(sld->version_from, sld->version_to)) continue;
		if (sld->conv & SLF_CONFIG_NO) continue;

		/* XXX - wtf is this?? (group override?) */
		s = strchr(sdb->name, '.');
		if (s != NULL) {
			group = ini_getgroup(ini, sdb->name, s - sdb->name);
			s++;
		} else {
			if (group_def == NULL) group_def = ini_getgroup(ini, grpname, -1);
			s = sdb->name;
			group = group_def;
		}

		item = ini_getitem(group, s, true);
		ptr = GetVariableAddress(object, sld);

		if (item->value != NULL) {
			/* check if the value is the same as the old value */
			const void *p = string_to_val(sdb, item->value);

			/* The main type of a variable/setting is in bytes 8-15
			 * The subtype (what kind of numbers do we have there) is in 0-7 */
			switch (sdb->cmd) {
			case SDT_BOOLX:
			case SDT_NUMX:
			case SDT_ONEOFMANY:
			case SDT_MANYOFMANY:
				switch (GetVarMemType(sld->conv)) {
				case SLE_VAR_BL:
					if (*(bool*)ptr == (p != NULL)) continue;
					break;
				case SLE_VAR_I8:
				case SLE_VAR_U8:
					if (*(byte*)ptr == (byte)(unsigned long)p) continue;
					break;
				case SLE_VAR_I16:
				case SLE_VAR_U16:
					if (*(uint16*)ptr == (uint16)(unsigned long)p) continue;
					break;
				case SLE_VAR_I32:
				case SLE_VAR_U32:
					if (*(uint32*)ptr == (uint32)(unsigned long)p) continue;
					break;
				default: NOT_REACHED();
				}
				break;
			default: break; /* Assume the other types are always changed */
			}
		}

		/* Value has changed, get the new value and put it into a buffer */
		switch (sdb->cmd) {
		case SDT_BOOLX:
		case SDT_NUMX:
		case SDT_ONEOFMANY:
		case SDT_MANYOFMANY: {
			uint32 i = (uint32)ReadValue(ptr, sld->conv);

			switch (sdb->cmd) {
			case SDT_BOOLX:      strcpy(buf, (i != 0) ? "true" : "false"); break;
			case SDT_NUMX:       sprintf(buf, IsSignedVarMemType(sld->conv) ? "%d" : "%u", i); break;
			case SDT_ONEOFMANY:  make_oneofmany(buf, sdb->many, i); break;
			case SDT_MANYOFMANY: make_manyofmany(buf, sdb->many, i); break;
			default: NOT_REACHED();
			}
		} break;

		case SDT_STRING:
			switch (GetVarMemType(sld->conv)) {
			case SLE_VAR_STRB: strcpy(buf, (char*)ptr); break;
			case SLE_VAR_STRBQ:sprintf(buf, "\"%s\"", (char*)ptr); break;
			case SLE_VAR_STR:  strcpy(buf, *(char**)ptr); break;
			case SLE_VAR_STRQ: sprintf(buf, "\"%s\"", *(char**)ptr); break;
			case SLE_VAR_CHAR: sprintf(buf, "\"%c\"", *(char*)ptr); break;
			default: NOT_REACHED();
			}
			break;

		case SDT_INTLIST:
			make_intlist(buf, ptr, sld->length, GetVarMemType(sld->conv));
			break;
		default: NOT_REACHED();
		}

		/* The value is different, that means we have to write it to the ini */
		item->value = (char*)pool_strdup(&ini->pool, buf, strlen(buf));
	}
}

/** Loads all items from a 'grpname' section into a list
 * The list parameter can be a NULL pointer, in this case nothing will be
 * saved and a callback function should be defined that will take over the
 * list-handling and store the data itself somewhere.
 * @param ini IniFile handle to the ini file with the source data
 * @param grpname character string identifying the section-header of the ini
 * file that will be parsed
 * @param list pointer to an string(pointer) array that will store the parsed
 * entries of the given section
 * @param len the maximum number of items available for the above list
 * @param proc callback function that can override how the values are stored
 * inside the list */
static void ini_load_setting_list(IniFile *ini, const char *grpname, char **list, uint len, SettingListCallbackProc proc)
{
	IniGroup *group = ini_getgroup(ini, grpname, -1);
	IniItem *item;
	const char *entry;
	uint i, j;

	if (group == NULL) return;

	for (i = j = 0, item = group->item; item != NULL; item = item->next) {
		entry = (proc != NULL) ? proc(item, i++) : item->name;

		if (entry == NULL || list == NULL) continue;

		if (j == len) break;
		list[j++] = strdup(entry);
	}
}

/** Saves all items from a list into the 'grpname' section
 * The list parameter can be a NULL pointer, in this case a callback function
 * should be defined that will provide the source data to be saved.
 * @param ini IniFile handle to the ini file where the destination data is saved
 * @param grpname character string identifying the section-header of the ini file
 * @param list pointer to an string(pointer) array that will be used as the
 *             source to be saved into the relevant ini section
 * @param len the maximum number of items available for the above list
 * @param proc callback function that can will provide the source data if defined */
static void ini_save_setting_list(IniFile *ini, const char *grpname, char **list, uint len, SettingListCallbackProc proc)
{
	IniGroup *group = ini_getgroup(ini, grpname, -1);
	IniItem *item = NULL;
	const char *entry;
	uint i;
	bool first = true;

	if (proc == NULL && list == NULL) return;
	if (group == NULL) return;
	group->item = NULL;

	for (i = 0; i != len; i++) {
		entry = (proc != NULL) ? proc(NULL, i) : list[i];

		if (entry == NULL || *entry == '\0') continue;

		if (first) { // add first item to the head of the group
			item = ini_item_alloc(group, entry, strlen(entry));
			item->value = item->name;
			group->item = item;
			first = false;
		} else { // all other items are attached to the previous one
			item->next = ini_item_alloc(group, entry, strlen(entry));
			item = item->next;
			item->value = item->name;
		}
	}
}

//***************************
// OTTD specific INI stuff
//***************************

/** Settings-macro usage:
 * The list might look daunting at first, but is in general easy to understand.
 * We have two types of list:
 * 1. SDTG_something
 * 2. SDT_something
 * The 'G' stands for global, so this is the one you will use for a
 * SettingDescGlobVarList section meaning global variables. The other uses a
 * Base/Offset and runtime variable selection mechanism, known from the saveload
 * convention (it also has global so it should not be hard).
 * Of each type there are again two versions, the normal one and one prefixed
 * with 'COND'.
 * COND means that the setting is only valid in certain savegame versions
 * (since patches are saved to the savegame, this bookkeeping is necessary.
 * Now there are a lot of types. Easy ones are:
 * - VAR:  any number type, 'type' field specifies what number. eg int8 or uint32
 * - BOOL: a boolean number type
 * - STR:  a string or character. 'type' field specifies what string. Normal, string, or quoted
 * A bit more difficult to use are MMANY (meaning ManyOfMany) and OMANY (OneOfMany)
 * These are actually normal numbers, only bitmasked. In MMANY several bits can
 * be set, in the other only one.
 * The most complex type is INTLIST. This is basically an array of numbers. If
 * the intlist is only valid in certain savegame versions because for example
 * it has grown in size its length cannot be automatically be calculated so
 * use SDT(G)_CONDLISTO() meaning Old.
 * If nothing fits you, you can use the GENERAL macros, but it exposes the
 * internal structure somewhat so it needs a little looking. There are _NULL()
 * macros as well, these fill up space so you can add more patches there (in
 * place) and you DON'T have to increase the savegame version.
 *
 * While reading values from openttd.cfg, some values may not be converted
 * properly, for any kind of reasons.  In order to allow a process of self-cleaning
 * mechanism, a callback procedure is made available.  You will have to supply the function, which
 * will work on a string, one function per patch.  And of course, enable the callback param
 * on the appropriate macro.
 */

#define NSD_GENERAL(name, def, cmd, guiflags, min, max, interval, many, str, proc, load)\
	{name, (const void*)(def), {cmd}, {guiflags}, min, max, interval, many, str, proc, load}

/* Macros for various objects to go in the configuration file.
 * This section is for global variables */
#define SDTG_GENERAL(name, sdt_cmd, sle_cmd, type, flags, guiflags, var, length, def, min, max, interval, full, str, proc, from, to)\
	{NSD_GENERAL(name, def, sdt_cmd, guiflags, min, max, interval, full, str, proc, NULL), SLEG_GENERAL(sle_cmd, var, type | flags, length, from, to)}

#define SDTG_CONDVAR(name, type, flags, guiflags, var, def, min, max, interval, str, proc, from, to)\
	SDTG_GENERAL(name, SDT_NUMX, SL_VAR, type, flags, guiflags, var, 0, def, min, max, interval, NULL, str, proc, from, to)
#define SDTG_VAR(name, type, flags, guiflags, var, def, min, max, interval, str, proc)\
	SDTG_CONDVAR(name, type, flags, guiflags, var, def, min, max, interval, str, proc, 0, SL_MAX_VERSION)

#define SDTG_CONDBOOL(name, flags, guiflags, var, def, str, proc, from, to)\
	SDTG_GENERAL(name, SDT_BOOLX, SL_VAR, SLE_BOOL, flags, guiflags, var, 0, def, 0, 1, 0, NULL, str, proc, from, to)
#define SDTG_BOOL(name, flags, guiflags, var, def, str, proc)\
	SDTG_CONDBOOL(name, flags, guiflags, var, def, str, proc, 0, SL_MAX_VERSION)

#define SDTG_CONDLIST(name, type, length, flags, guiflags, var, def, str, proc, from, to)\
	SDTG_GENERAL(name, SDT_INTLIST, SL_ARR, type, flags, guiflags, var, length, def, 0, 0, 0, NULL, str, proc, from, to)
#define SDTG_LIST(name, type, flags, guiflags, var, def, str, proc)\
	SDTG_GENERAL(name, SDT_INTLIST, SL_ARR, type, flags, guiflags, var, lengthof(var), def, 0, 0, 0, NULL, str, proc, 0, SL_MAX_VERSION)

#define SDTG_CONDSTR(name, type, length, flags, guiflags, var, def, str, proc, from, to)\
	SDTG_GENERAL(name, SDT_STRING, SL_STR, type, flags, guiflags, var, length, def, 0, 0, 0, NULL, str, proc, from, to)
#define SDTG_STR(name, type, flags, guiflags, var, def, str, proc)\
	SDTG_GENERAL(name, SDT_STRING, SL_STR, type, flags, guiflags, var, lengthof(var), def, 0, 0, 0, NULL, str, proc, 0, SL_MAX_VERSION)

#define SDTG_CONDOMANY(name, type, flags, guiflags, var, def, max, full, str, proc, from, to)\
	SDTG_GENERAL(name, SDT_ONEOFMANY, SL_VAR, type, flags, guiflags, var, 0, def, 0, max, 0, full, str, proc, from, to)
#define SDTG_OMANY(name, type, flags, guiflags, var, def, max, full, str, proc)\
	SDTG_CONDOMANY(name, type, flags, guiflags, var, def, max, full, str, proc, 0, SL_MAX_VERSION)

#define SDTG_CONDMMANY(name, type, flags, guiflags, var, def, full, str, proc, from, to)\
	SDTG_GENERAL(name, SDT_MANYOFMANY, SL_VAR, type, flags, guiflags, var, 0, def, 0, 0, 0, full, str, proc, from, to)
#define SDTG_MMANY(name, type, flags, guiflags, var, def, full, str, proc)\
	SDTG_CONDMMANY(name, type, flags, guiflags, var, def, full, str, proc, 0, SL_MAX_VERSION)

#define SDTG_CONDNULL(length, from, to)\
	{{"", NULL, {0}, {0}, 0, 0, 0, NULL, STR_NULL, NULL, NULL}, SLEG_CONDNULL(length, from, to)}

#define SDTG_END() {{NULL, NULL, {0}, {0}, 0, 0, 0, NULL, STR_NULL, NULL, NULL}, SLEG_END()}

/* Macros for various objects to go in the configuration file.
 * This section is for structures where their various members are saved */
#define SDT_GENERAL(name, sdt_cmd, sle_cmd, type, flags, guiflags, base, var, length, def, min, max, interval, full, str, proc, load, from, to)\
	{NSD_GENERAL(name, def, sdt_cmd, guiflags, min, max, interval, full, str, proc, load), SLE_GENERAL(sle_cmd, base, var, type | flags, length, from, to)}

#define SDT_CONDVAR(base, var, type, from, to, flags, guiflags, def, min, max, interval, str, proc)\
	SDT_GENERAL(#var, SDT_NUMX, SL_VAR, type, flags, guiflags, base, var, 1, def, min, max, interval, NULL, str, proc, NULL, from, to)
#define SDT_VAR(base, var, type, flags, guiflags, def, min, max, interval, str, proc)\
	SDT_CONDVAR(base, var, type, 0, SL_MAX_VERSION, flags, guiflags, def, min, max, interval, str, proc)

#define SDT_CONDBOOL(base, var, from, to, flags, guiflags, def, str, proc)\
	SDT_GENERAL(#var, SDT_BOOLX, SL_VAR, SLE_BOOL, flags, guiflags, base, var, 1, def, 0, 1, 0, NULL, str, proc, NULL, from, to)
#define SDT_BOOL(base, var, flags, guiflags, def, str, proc)\
	SDT_CONDBOOL(base, var, 0, SL_MAX_VERSION, flags, guiflags, def, str, proc)

#define SDT_CONDLIST(base, var, type, from, to, flags, guiflags, def, str, proc)\
	SDT_GENERAL(#var, SDT_INTLIST, SL_ARR, type, flags, guiflags, base, var, lengthof(((base*)8)->var), def, 0, 0, 0, NULL, str, proc, NULL, from, to)
#define SDT_LIST(base, var, type, flags, guiflags, def, str, proc)\
	SDT_CONDLIST(base, var, type, 0, SL_MAX_VERSION, flags, guiflags, def, str, proc)
#define SDT_CONDLISTO(base, var, length, type, from, to, flags, guiflags, def, str, proc)\
	SDT_GENERAL(#var, SDT_INTLIST, SL_ARR, type, flags, guiflags, base, var, length, def, 0, 0, 0, NULL, str, proc, NULL, from, to)

#define SDT_CONDSTR(base, var, type, from, to, flags, guiflags, def, str, proc)\
	SDT_GENERAL(#var, SDT_STRING, SL_STR, type, flags, guiflags, base, var, lengthof(((base*)8)->var), def, 0, 0, 0, NULL, str, proc, NULL, from, to)
#define SDT_STR(base, var, type, flags, guiflags, def, str, proc)\
	SDT_CONDSTR(base, var, type, 0, SL_MAX_VERSION, flags, guiflags, def, str, proc)
#define SDT_CONDSTRO(base, var, length, type, from, to, flags, def, str, proc)\
	SDT_GENERAL(#var, SDT_STRING, SL_STR, type, flags, 0, base, var, length, def, 0, 0, NULL, str, proc, from, to)

#define SDT_CONDCHR(base, var, from, to, flags, guiflags, def, str, proc)\
	SDT_GENERAL(#var, SDT_STRING, SL_VAR, SLE_CHAR, flags, guiflags, base, var, 1, def, 0, 0, 0, NULL, str, proc, NULL, from, to)
#define SDT_CHR(base, var, flags, guiflags, def, str, proc)\
	SDT_CONDCHR(base, var, 0, SL_MAX_VERSION, flags, guiflags, def, str, proc)

#define SDT_CONDOMANY(base, var, type, from, to, flags, guiflags, def, max, full, str, proc, load)\
	SDT_GENERAL(#var, SDT_ONEOFMANY, SL_VAR, type, flags, guiflags, base, var, 1, def, 0, max, 0, full, str, proc, load, from, to)
#define SDT_OMANY(base, var, type, flags, guiflags, def, max, full, str, proc, load)\
	SDT_CONDOMANY(base, var, type, 0, SL_MAX_VERSION, flags, guiflags, def, max, full, str, proc, load)

#define SDT_CONDMMANY(base, var, type, from, to, flags, guiflags, def, full, str, proc)\
	SDT_GENERAL(#var, SDT_MANYOFMANY, SL_VAR, type, flags, guiflags, base, var, 1, def, 0, 0, 0, full, str, proc, NULL, from, to)
#define SDT_MMANY(base, var, type, flags, guiflags, def, full, str, proc)\
	SDT_CONDMMANY(base, var, type, 0, SL_MAX_VERSION, flags, guiflags, def, full, str, proc)

#define SDT_CONDNULL(length, from, to)\
	{{"", NULL, {0}, {0}, 0, 0, 0, NULL, STR_NULL, NULL, NULL}, SLE_CONDNULL(length, from, to)}

#define SDT_END() {{NULL, NULL, {0}, {0}, 0, 0, 0, NULL, STR_NULL, NULL, NULL}, SLE_END()}

/* Shortcuts for macros below. Logically if we don't save the value
 * we also don't sync it in a network game */
#define S SLF_SAVE_NO | SLF_NETWORK_NO
#define C SLF_CONFIG_NO
#define N SLF_NETWORK_NO

#define D0 SGF_0ISDISABLED
#define NC SGF_NOCOMMA
#define MS SGF_MULTISTRING
#define NO SGF_NETWORK_ONLY
#define CR SGF_CURRENCY
#define NN SGF_NO_NETWORK

#include "table/strings.h"

/* Begin - Callback Functions for the various settings */
#include "window.h"
#include "gui.h"
#include "town.h"
#include "gfx.h"
/* virtual PositionMainToolbar function, calls the right one.*/
static int32 v_PositionMainToolbar(int32 p1)
{
	if (_game_mode != GM_MENU) PositionMainToolbar(NULL);
	return 0;
}

static int32 AiNew_PatchActive_Warning(int32 p1)
{
	if (p1 == 1) ShowErrorMessage(INVALID_STRING_ID, TEMP_AI_ACTIVATED, 0, 0);
	return 0;
}

static int32 Ai_In_Multiplayer_Warning(int32 p1)
{
	if (p1 == 1) {
		ShowErrorMessage(INVALID_STRING_ID, TEMP_AI_MULTIPLAYER, 0, 0);
		_patches.ainew_active = true;
	}
	return 0;
}

static int32 PopulationInLabelActive(int32 p1)
{
	Town* t;

	FOR_ALL_TOWNS(t) UpdateTownVirtCoord(t);

	return 0;
}

static int32 RedrawScreen(int32 p1)
{
	MarkWholeScreenDirty();
	return 0;
}

static int32 InValidateDetailsWindow(int32 p1)
{
	InvalidateWindowClasses(WC_VEHICLE_DETAILS);
	return 0;
}

static int32 InvalidateStationBuildWindow(int32 p1)
{
	InvalidateWindow(WC_BUILD_STATION, 0);
	return 0;
}

static int32 UpdateConsists(int32 p1)
{
	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		/* Update the consist of all trains so the maximum speed is set correctly. */
		if (v->type == VEH_TRAIN && (IsFrontEngine(v) || IsFreeWagon(v))) TrainConsistChanged(v);
	}
	return 0;
}

/* Check service intervals of vehicles, p1 is value of % or day based servicing */
static int32 CheckInterval(int32 p1)
{
	bool warning;
	const Patches *ptc = (_game_mode == GM_MENU) ? &_patches_newgame : &_patches;

	if (p1) {
		warning = ( (IS_INT_INSIDE(ptc->servint_trains,   5, 90 + 1) || ptc->servint_trains   == 0) &&
								(IS_INT_INSIDE(ptc->servint_roadveh,  5, 90 + 1) || ptc->servint_roadveh  == 0) &&
								(IS_INT_INSIDE(ptc->servint_aircraft, 5, 90 + 1) || ptc->servint_aircraft == 0) &&
								(IS_INT_INSIDE(ptc->servint_ships,    5, 90 + 1) || ptc->servint_ships    == 0) );
	} else {
		warning = ( (IS_INT_INSIDE(ptc->servint_trains,   30, 800 + 1) || ptc->servint_trains   == 0) &&
								(IS_INT_INSIDE(ptc->servint_roadveh,  30, 800 + 1) || ptc->servint_roadveh  == 0) &&
								(IS_INT_INSIDE(ptc->servint_aircraft, 30, 800 + 1) || ptc->servint_aircraft == 0) &&
								(IS_INT_INSIDE(ptc->servint_ships,    30, 800 + 1) || ptc->servint_ships    == 0) );
	}

	if (!warning)
		ShowErrorMessage(INVALID_STRING_ID, STR_CONFIG_PATCHES_SERVICE_INTERVAL_INCOMPATIBLE, 0, 0);

	return InValidateDetailsWindow(0);
}

static int32 EngineRenewUpdate(int32 p1)
{
	DoCommandP(0, 0, _patches.autorenew, NULL, CMD_SET_AUTOREPLACE);
	return 0;
}

static int32 EngineRenewMonthsUpdate(int32 p1)
{
	DoCommandP(0, 1, _patches.autorenew_months, NULL, CMD_SET_AUTOREPLACE);
	return 0;
}

static int32 EngineRenewMoneyUpdate(int32 p1)
{
	DoCommandP(0, 2, _patches.autorenew_money, NULL, CMD_SET_AUTOREPLACE);
	return 0;
}

/**
 * Check for right TownLayout usage in editor mode.
 * The No Road mode is not desirable since towns have to be
 * able to grow. If a user desires to have a town with no road,
 * he can easily remove them himself. This would create less confusion
 * @param p1 unused
 * @return always 0
 */
static int32 CheckTownLayout(int32 p1)
{
	if (_patches.town_layout == TL_NO_ROADS && _game_mode == GM_EDITOR) {
		ShowErrorMessage(INVALID_STRING_ID, STR_CONFIG_PATCHES_TOWN_LAYOUT_INVALID, 0, 0);
		_patches.town_layout = TL_ORIGINAL;
	}
	return 0;
}

/** Conversion callback for _gameopt_settings.landscape
 * It converts (or try) between old values and the new ones,
 * without loosing initial setting  of the user
 * @param value that was read from config file
 * @return the "hopefully" converted value
 */
static int32 ConvertLandscape(const char *value)
{
	/* try with the old values */
	return lookup_oneofmany("normal|hilly|desert|candy", value, -1);
}

/* End - Callback Functions */

#ifndef EXTERNAL_PLAYER
#define EXTERNAL_PLAYER "timidity"
#endif

static const SettingDesc _music_settings[] = {
	 SDT_VAR(MusicFileSettings, playlist,   SLE_UINT8, S, 0,   0, 0,   5, 1,  STR_NULL, NULL),
	 SDT_VAR(MusicFileSettings, music_vol,  SLE_UINT8, S, 0, 128, 0, 100, 1,  STR_NULL, NULL),
	 SDT_VAR(MusicFileSettings, effect_vol, SLE_UINT8, S, 0, 128, 0, 100, 1,  STR_NULL, NULL),
	SDT_LIST(MusicFileSettings, custom_1,   SLE_UINT8, S, 0, NULL,            STR_NULL, NULL),
	SDT_LIST(MusicFileSettings, custom_2,   SLE_UINT8, S, 0, NULL,            STR_NULL, NULL),
	SDT_BOOL(MusicFileSettings, playing,               S, 0, true,            STR_NULL, NULL),
	SDT_BOOL(MusicFileSettings, shuffle,               S, 0, false,           STR_NULL, NULL),
	 SDT_STR(MusicFileSettings, extmidi,     SLE_STRB, S, 0, EXTERNAL_PLAYER, STR_NULL, NULL),
	 SDT_END()
};

/* win32_v.c only settings */
#ifdef WIN32
extern bool _force_full_redraw, _window_maximize;
extern uint _display_hz, _fullscreen_bpp;

static const SettingDescGlobVarList _win32_settings[] = {
	 SDTG_VAR("display_hz",     SLE_UINT, S, 0, _display_hz,       0, 0, 120, 0, STR_NULL, NULL),
	SDTG_BOOL("force_full_redraw",        S, 0, _force_full_redraw,false,        STR_NULL, NULL),
	 SDTG_VAR("fullscreen_bpp", SLE_UINT, S, 0, _fullscreen_bpp,   8, 8,  32, 0, STR_NULL, NULL),
	SDTG_BOOL("window_maximize",          S, 0, _window_maximize,  false,        STR_NULL, NULL),
	 SDTG_END()
};
#endif /* WIN32 */

static const SettingDescGlobVarList _misc_settings[] = {
	SDTG_MMANY("display_opt",     SLE_UINT8, S, 0, _display_opt,       (1 << DO_SHOW_TOWN_NAMES | 1 << DO_SHOW_STATION_NAMES | 1 << DO_SHOW_SIGNS | 1 << DO_FULL_ANIMATION | 1 << DO_FULL_DETAIL | 1 << DO_WAYPOINTS), "SHOW_TOWN_NAMES|SHOW_STATION_NAMES|SHOW_SIGNS|FULL_ANIMATION||FULL_DETAIL|WAYPOINTS", STR_NULL, NULL),
	 SDTG_BOOL("news_ticker_sound",          S, 0, _news_ticker_sound,     true,    STR_NULL, NULL),
	 SDTG_BOOL("fullscreen",                 S, 0, _fullscreen,           false,    STR_NULL, NULL),
	  SDTG_STR("videodriver",      SLE_STRB,C|S,0, _ini_videodriver,       NULL,    STR_NULL, NULL),
	  SDTG_STR("musicdriver",      SLE_STRB,C|S,0, _ini_musicdriver,       NULL,    STR_NULL, NULL),
	  SDTG_STR("sounddriver",      SLE_STRB,C|S,0, _ini_sounddriver,       NULL,    STR_NULL, NULL),
	  SDTG_STR("blitter",          SLE_STRB,C|S,0, _ini_blitter,           NULL,    STR_NULL, NULL),
	  SDTG_STR("language",         SLE_STRB, S, 0, _dynlang.curr_file,     NULL,    STR_NULL, NULL),
	 SDTG_LIST("resolution",     SLE_UINT16, S, 0, _cur_resolution,   "640,480",    STR_NULL, NULL),
	  SDTG_STR("screenshot_format",SLE_STRB, S, 0, _screenshot_format_name,NULL,    STR_NULL, NULL),
	  SDTG_STR("savegame_format",  SLE_STRB, S, 0, _savegame_format,       NULL,    STR_NULL, NULL),
	 SDTG_BOOL("rightclick_emulate",         S, 0, _rightclick_emulate,   false,    STR_NULL, NULL),
#ifdef WITH_FREETYPE
	  SDTG_STR("small_font",       SLE_STRB, S, 0, _freetype.small_font,   NULL,    STR_NULL, NULL),
	  SDTG_STR("medium_font",      SLE_STRB, S, 0, _freetype.medium_font,  NULL,    STR_NULL, NULL),
	  SDTG_STR("large_font",       SLE_STRB, S, 0, _freetype.large_font,   NULL,    STR_NULL, NULL),
	  SDTG_VAR("small_size",       SLE_UINT, S, 0, _freetype.small_size,   6, 0, 72, 0, STR_NULL, NULL),
	  SDTG_VAR("medium_size",      SLE_UINT, S, 0, _freetype.medium_size, 10, 0, 72, 0, STR_NULL, NULL),
	  SDTG_VAR("large_size",       SLE_UINT, S, 0, _freetype.large_size,  16, 0, 72, 0, STR_NULL, NULL),
	 SDTG_BOOL("small_aa",                   S, 0, _freetype.small_aa,    false,    STR_NULL, NULL),
	 SDTG_BOOL("medium_aa",                  S, 0, _freetype.medium_aa,   false,    STR_NULL, NULL),
	 SDTG_BOOL("large_aa",                   S, 0, _freetype.large_aa,    false,    STR_NULL, NULL),
#endif
	  SDTG_VAR("sprite_cache_size",SLE_UINT, S, 0, _sprite_cache_size,     4, 1, 64, 0, STR_NULL, NULL),
	  SDTG_VAR("player_face",    SLE_UINT32, S, 0, _player_face,      0,0,0xFFFFFFFF,0, STR_NULL, NULL),
	  SDTG_VAR("transparency_options", SLE_UINT8, S, 0, _transparency_opt, 0, 0,0xFF,0, STR_NULL, NULL),
	  SDTG_END()
};

#ifdef ENABLE_NETWORK
static const SettingDescGlobVarList _network_settings[] = {
	  SDTG_VAR("sync_freq",           SLE_UINT16,C|S,0, _network_sync_freq,            100, 0,   100,   0, STR_NULL, NULL),
	  SDTG_VAR("frame_freq",           SLE_UINT8,C|S,0, _network_frame_freq,             0, 0,   100,   0, STR_NULL, NULL),
	  SDTG_VAR("max_join_time",       SLE_UINT16, S, 0, _network_max_join_time,        500, 0, 32000,   0, STR_NULL, NULL),
	 SDTG_BOOL("pause_on_join",                   S, 0, _network_pause_on_join,        true,               STR_NULL, NULL),
	  SDTG_STR("server_bind_ip",        SLE_STRB, S, 0, _network_server_bind_ip_host,  "0.0.0.0",          STR_NULL, NULL),
	  SDTG_VAR("server_port",         SLE_UINT16, S, 0, _network_server_port,          NETWORK_DEFAULT_PORT, 0, 65535, 0, STR_NULL, NULL),
	 SDTG_BOOL("server_advertise",                S, 0, _network_advertise,            false,              STR_NULL, NULL),
	  SDTG_VAR("lan_internet",         SLE_UINT8, S, 0, _network_lan_internet,           0, 0,     1,   0, STR_NULL, NULL),
	  SDTG_STR("player_name",           SLE_STRB, S, 0, _network_player_name,          NULL,               STR_NULL, NULL),
	  SDTG_STR("server_password",       SLE_STRB, S, 0, _network_server_password,      NULL,               STR_NULL, NULL),
	  SDTG_STR("rcon_password",         SLE_STRB, S, 0, _network_rcon_password,        NULL,               STR_NULL, NULL),
	  SDTG_STR("server_name",           SLE_STRB, S, 0, _network_server_name,          NULL,               STR_NULL, NULL),
	  SDTG_STR("connect_to_ip",         SLE_STRB, S, 0, _network_default_ip,           NULL,               STR_NULL, NULL),
	  SDTG_STR("network_id",            SLE_STRB, S, 0, _network_unique_id,            NULL,               STR_NULL, NULL),
	 SDTG_BOOL("autoclean_companies",             S, 0, _network_autoclean_companies,  false,              STR_NULL, NULL),
	  SDTG_VAR("autoclean_unprotected",SLE_UINT8, S, 0, _network_autoclean_unprotected,12, 0,     60,   0, STR_NULL, NULL),
	  SDTG_VAR("autoclean_protected",  SLE_UINT8, S, 0, _network_autoclean_protected,  36, 0,    180,   0, STR_NULL, NULL),
	  SDTG_VAR("max_companies",        SLE_UINT8, S, 0, _network_game_info.companies_max,   8, 1, MAX_PLAYERS, 0, STR_NULL, NULL),
	  SDTG_VAR("max_clients",          SLE_UINT8, S, 0, _network_game_info.clients_max,    10, 2, MAX_CLIENTS, 0, STR_NULL, NULL),
	  SDTG_VAR("max_spectators",       SLE_UINT8, S, 0, _network_game_info.spectators_max, 10, 0, MAX_CLIENTS, 0, STR_NULL, NULL),
	  SDTG_VAR("restart_game_year",    SLE_INT32, S,D0, _network_restart_game_year,    0, MIN_YEAR, MAX_YEAR, 1, STR_NULL, NULL),
	  SDTG_VAR("min_players",          SLE_UINT8, S, 0, _network_min_players,               0, 0, 10,   0, STR_NULL, NULL),
	SDTG_OMANY("server_lang",          SLE_UINT8, S, 0, _network_game_info.server_lang,     0, 28, "ANY|ENGLISH|GERMAN|FRENCH|BRAZILIAN|BULGARIAN|CHINESE|CZECH|DANISH|DUTCH|ESPERANTO|FINNISH|HUNGARIAN|ICELANDIC|ITALIAN|JAPANESE|KOREAN|LITHUANIAN|NORWEGIAN|POLISH|PORTUGUESE|ROMANIAN|RUSSIAN|SLOVAK|SLOVENIAN|SPANISH|SWEDISH|TURKISH|UKRAINIAN", STR_NULL, NULL),
	  SDTG_END()
};
#endif /* ENABLE_NETWORK */

static const SettingDesc _gameopt_settings[] = {
	/* In version 4 a new difficulty setting has been added to the difficulty settings,
	 * town attitude towards demolishing. Needs special handling because some dimwit thought
	 * it funny to have the GameDifficulty struct be an array while it is a struct of
	 * same-sized members
	 * XXX - To save file-space and since values are never bigger than about 10? only
	 * save the first 16 bits in the savegame. Question is why the values are still int32
	 * and why not byte for example? */
	SDT_GENERAL("diff_custom", SDT_INTLIST, SL_ARR, SLE_UINT16, 0, 0, GameOptions, diff, 17, 0, 0, 0, 0, NULL, STR_NULL, NULL, NULL, 0, 3),
	SDT_GENERAL("diff_custom", SDT_INTLIST, SL_ARR, SLE_UINT16, 0, 0, GameOptions, diff, 18, 0, 0, 0, 0, NULL, STR_NULL, NULL, NULL, 4, SL_MAX_VERSION),
	    SDT_VAR(GameOptions, diff_level, SLE_UINT8, 0, 0, 0, 0,  3, 0, STR_NULL, NULL),
	  SDT_OMANY(GameOptions, currency,  SLE_UINT8, N, 0, 0, CUSTOM_CURRENCY_ID, "GBP|USD|EUR|YEN|ATS|BEF|CHF|CZK|DEM|DKK|ESP|FIM|FRF|GRD|HUF|ISK|ITL|NLG|NOK|PLN|ROL|RUR|SIT|SEK|YTL|SKK|BRR|custom", STR_NULL, NULL, NULL),
	  SDT_OMANY(GameOptions, units,     SLE_UINT8, N, 0, 1,     2, "imperial|metric|si", STR_NULL, NULL, NULL),
	/* There are only 21 predefined town_name values (0-20), but you can have more with newgrf action F so allow these bigger values (21-255). Invalid values will fallback to english on use and (undefined string) in GUI. */
	  SDT_OMANY(GameOptions, town_name, SLE_UINT8, 0, 0, 0,   255, "english|french|german|american|latin|silly|swedish|dutch|finnish|polish|slovakish|norwegian|hungarian|austrian|romanian|czech|swiss|danish|turkish|italian|catalan", STR_NULL, NULL, NULL),
	  SDT_OMANY(GameOptions, landscape, SLE_UINT8, 0, 0, 0,     3, "temperate|arctic|tropic|toyland", STR_NULL, NULL, ConvertLandscape),
	    SDT_VAR(GameOptions, snow_line, SLE_UINT8, 0, 0, 7 * TILE_HEIGHT, 2 * TILE_HEIGHT, 13 * TILE_HEIGHT, 0, STR_NULL, NULL),
	SDT_CONDOMANY(GameOptions,autosave, SLE_UINT8, 0, 22,             N, 0, 0, 0, "", STR_NULL, NULL, NULL),
	SDT_CONDOMANY(GameOptions,autosave, SLE_UINT8,23, SL_MAX_VERSION, S, 0, 1, 4, "off|monthly|quarterly|half year|yearly", STR_NULL, NULL, NULL),
	  SDT_OMANY(GameOptions, road_side, SLE_UINT8, 0, 0, 1,   1, "left|right", STR_NULL, NULL, NULL),
	    SDT_END()
};

/* Some patches do not need to be synchronised when playing in multiplayer.
 * These include for example the GUI settings and will not be saved with the
 * savegame.
 * It is also a bit tricky since you would think that service_interval
 * for example doesn't need to be synched. Every client assigns the
 * service_interval value to the v->service_interval, meaning that every client
 * assigns his value. If the setting was player-based, that would mean that
 * vehicles could decide on different moments that they are heading back to a
 * service depot, causing desyncs on a massive scale. */
const SettingDesc _patch_settings[] = {
	/***************************************************************************/
	/* User-interface section of the GUI-configure patches window */
	SDT_BOOL(Patches, vehicle_speed,                 S, 0,  true,        STR_CONFIG_PATCHES_VEHICLESPEED,          NULL),
	SDT_BOOL(Patches, status_long_date,              S, 0,  true,        STR_CONFIG_PATCHES_LONGDATE,              NULL),
	SDT_BOOL(Patches, show_finances,                 S, 0,  true,        STR_CONFIG_PATCHES_SHOWFINANCES,          NULL),
	SDT_BOOL(Patches, autoscroll,                    S, 0, false,        STR_CONFIG_PATCHES_AUTOSCROLL,            NULL),
	SDT_BOOL(Patches, reverse_scroll,                S, 0, false,        STR_CONFIG_PATCHES_REVERSE_SCROLLING,     NULL),
	SDT_BOOL(Patches, smooth_scroll,                 S, 0, false,        STR_CONFIG_PATCHES_SMOOTH_SCROLLING,      NULL),
	SDT_BOOL(Patches, measure_tooltip,               S, 0, false,        STR_CONFIG_PATCHES_MEASURE_TOOLTIP,       NULL),
	 SDT_VAR(Patches, errmsg_duration,    SLE_UINT8, S, 0,  5, 0, 20, 0, STR_CONFIG_PATCHES_ERRMSG_DURATION,       NULL),
	 SDT_VAR(Patches, toolbar_pos,        SLE_UINT8, S,MS,  0, 0,  2, 0, STR_CONFIG_PATCHES_TOOLBAR_POS,           v_PositionMainToolbar),
	 SDT_VAR(Patches, window_snap_radius, SLE_UINT8, S,D0, 10, 1, 32, 0, STR_CONFIG_PATCHES_SNAP_RADIUS,           NULL),
	SDT_BOOL(Patches, invisible_trees,               S, 0, false,        STR_CONFIG_PATCHES_INVISIBLE_TREES,       RedrawScreen),
	SDT_BOOL(Patches, population_in_label,           S, 0,  true,        STR_CONFIG_PATCHES_POPULATION_IN_LABEL,   PopulationInLabelActive),
	 SDT_VAR(Patches, map_x,              SLE_UINT8, S, 0,  8, 6, 11, 0, STR_CONFIG_PATCHES_MAP_X,                 NULL),
	 SDT_VAR(Patches, map_y,              SLE_UINT8, S, 0,  8, 6, 11, 0, STR_CONFIG_PATCHES_MAP_Y,                 NULL),
	SDT_BOOL(Patches, link_terraform_toolbar,        S, 0, false,        STR_CONFIG_PATCHES_LINK_TERRAFORM_TOOLBAR,NULL),
	 SDT_VAR(Patches, liveries,           SLE_UINT8, S,MS,  2, 0,  2, 0, STR_CONFIG_PATCHES_LIVERIES,              RedrawScreen),
	SDT_BOOL(Patches, prefer_teamchat,               S, 0, false,        STR_CONFIG_PATCHES_PREFER_TEAMCHAT,       NULL),
	SDT_VAR(Patches, scrollwheel_scrolling,SLE_UINT8,S,MS, 0,  0,  2, 0, STR_CONFIG_PATCHES_SCROLLWHEEL_SCROLLING, NULL),
	SDT_VAR(Patches,scrollwheel_multiplier,SLE_UINT8,S, 0, 5,  1, 15, 1, STR_CONFIG_PATCHES_SCROLLWHEEL_MULTIPLIER,NULL),
#ifdef __APPLE__
	/* We might need to emulate a right mouse button on mac */
	SDT_VAR(Patches,right_mouse_btn_emulation,SLE_UINT8,S,MS,0, 0, 2, 0, STR_CONFIG_PATCHES_RIGHT_MOUSE_BTN_EMU,   NULL),
#endif
	SDT_BOOL(Patches, pause_on_newgame,              S, 0, false,        STR_CONFIG_PATCHES_PAUSE_ON_NEW_GAME,     NULL),
	 SDT_VAR(Patches,advanced_vehicle_list,SLE_UINT8,S,MS, 1,  0,  2, 0, STR_CONFIG_PATCHES_ADVANCED_VEHICLE_LISTS,NULL),
	SDT_BOOL(Patches, timetable_in_ticks,            S, 0, false,        STR_CONFIG_PATCHES_TIMETABLE_IN_TICKS,    NULL),
	 SDT_VAR(Patches, loading_indicators, SLE_UINT8, S,MS,  1, 0,  2, 0, STR_CONFIG_PATCHES_LOADING_INDICATORS,    RedrawScreen),
	 SDT_VAR(Patches, default_rail_type,  SLE_UINT8, S,MS,  4, 0,  6, 0, STR_CONFIG_PATCHES_DEFAULT_RAIL_TYPE,     NULL),

	/***************************************************************************/
	/* Construction section of the GUI-configure patches window */
	SDT_BOOL(Patches, build_on_slopes,               0,NN,  true,        STR_CONFIG_PATCHES_BUILDONSLOPES,       NULL),
	SDT_CONDBOOL(Patches, autoslope,                75, SL_MAX_VERSION, 0, 0, true,  STR_CONFIG_PATCHES_AUTOSLOPE,            NULL),
	SDT_BOOL(Patches, extra_dynamite,                0, 0, false,        STR_CONFIG_PATCHES_EXTRADYNAMITE,       NULL),
	SDT_BOOL(Patches, longbridges,                   0,NN,  true,        STR_CONFIG_PATCHES_LONGBRIDGES,         NULL),
	SDT_BOOL(Patches, signal_side,                   N,NN,  true,        STR_CONFIG_PATCHES_SIGNALSIDE,          RedrawScreen),
	SDT_BOOL(Patches, always_small_airport,          0,NN, false,        STR_CONFIG_PATCHES_SMALL_AIRPORTS,      NULL),
	 SDT_VAR(Patches, drag_signals_density,SLE_UINT8,S, 0,  4, 1, 20, 0, STR_CONFIG_PATCHES_DRAG_SIGNALS_DENSITY,NULL),
	 SDT_VAR(Patches, semaphore_build_before,SLE_INT32, S, NC, 1975, MIN_YEAR, MAX_YEAR, 1, STR_CONFIG_PATCHES_SEMAPHORE_BUILD_BEFORE_DATE, NULL),
	SDT_CONDVAR(Patches, town_layout, SLE_UINT8, 59, SL_MAX_VERSION, 0, MS, TL_ORIGINAL, TL_NO_ROADS, NUM_TLS - 1, 1, STR_CONFIG_PATCHES_TOWN_LAYOUT, CheckTownLayout),

	/***************************************************************************/
	/* Vehicle section of the GUI-configure patches window */
	SDT_BOOL(Patches, realistic_acceleration,        0, 0, false,                    STR_CONFIG_PATCHES_REALISTICACCEL,       NULL),
	SDT_BOOL(Patches, forbid_90_deg,                 0, 0, false,                    STR_CONFIG_PATCHES_FORBID_90_DEG,        NULL),
	SDT_BOOL(Patches, mammoth_trains,                0,NN,  true,                    STR_CONFIG_PATCHES_MAMMOTHTRAINS,        NULL),
	SDT_BOOL(Patches, gotodepot,                     0, 0,  true,                    STR_CONFIG_PATCHES_GOTODEPOT,            NULL),
	SDT_BOOL(Patches, roadveh_queue,                 0, 0,  true,                    STR_CONFIG_PATCHES_ROADVEH_QUEUE,        NULL),
	SDT_BOOL(Patches, new_pathfinding_all,           0, 0, false,                    STR_CONFIG_PATCHES_NEW_PATHFINDING_ALL,  NULL),

	SDT_CONDBOOL(Patches, yapf.ship_use_yapf,      28, SL_MAX_VERSION, 0, 0, false,  STR_CONFIG_PATCHES_YAPF_SHIPS,      NULL),
	SDT_CONDBOOL(Patches, yapf.road_use_yapf,      28, SL_MAX_VERSION, 0, 0,  true,  STR_CONFIG_PATCHES_YAPF_ROAD,       NULL),
	SDT_CONDBOOL(Patches, yapf.rail_use_yapf,      28, SL_MAX_VERSION, 0, 0,  true,  STR_CONFIG_PATCHES_YAPF_RAIL,       NULL),

	SDT_BOOL(Patches, train_income_warn,             S, 0,  true,                    STR_CONFIG_PATCHES_WARN_INCOME_LESS,     NULL),
	 SDT_VAR(Patches, order_review_system,SLE_UINT8, S,MS,     2,     0,       2, 0, STR_CONFIG_PATCHES_ORDER_REVIEW,         NULL),
	SDT_BOOL(Patches, never_expire_vehicles,         0,NN, false,                    STR_CONFIG_PATCHES_NEVER_EXPIRE_VEHICLES,NULL),
	SDT_BOOL(Patches, lost_train_warn,               S, 0,  true,                    STR_CONFIG_PATCHES_WARN_LOST_TRAIN,      NULL),
	SDT_BOOL(Patches, autorenew,                     S, 0, false,                    STR_CONFIG_PATCHES_AUTORENEW_VEHICLE,    EngineRenewUpdate),
	 SDT_VAR(Patches, autorenew_months,   SLE_INT16, S, 0,     6,   -12,      12, 0, STR_CONFIG_PATCHES_AUTORENEW_MONTHS,     EngineRenewMonthsUpdate),
	 SDT_VAR(Patches, autorenew_money,     SLE_UINT, S,CR,100000,     0, 2000000, 0, STR_CONFIG_PATCHES_AUTORENEW_MONEY,      EngineRenewMoneyUpdate),
	SDT_BOOL(Patches, always_build_infrastructure,   S, 0, false,                    STR_CONFIG_PATCHES_ALWAYS_BUILD_INFRASTRUCTURE, RedrawScreen),
	 SDT_VAR(Patches, max_trains,        SLE_UINT16, 0, 0,   500,     0,    5000, 0, STR_CONFIG_PATCHES_MAX_TRAINS,           RedrawScreen),
	 SDT_VAR(Patches, max_roadveh,       SLE_UINT16, 0, 0,   500,     0,    5000, 0, STR_CONFIG_PATCHES_MAX_ROADVEH,          RedrawScreen),
	 SDT_VAR(Patches, max_aircraft,      SLE_UINT16, 0, 0,   200,     0,    5000, 0, STR_CONFIG_PATCHES_MAX_AIRCRAFT,         RedrawScreen),
	 SDT_VAR(Patches, max_ships,         SLE_UINT16, 0, 0,   300,     0,    5000, 0, STR_CONFIG_PATCHES_MAX_SHIPS,            RedrawScreen),
	SDT_BOOL(Patches, servint_ispercent,             0, 0, false,                    STR_CONFIG_PATCHES_SERVINT_ISPERCENT,    CheckInterval),
	 SDT_VAR(Patches, servint_trains,    SLE_UINT16, 0,D0,   150,     5,     800, 0, STR_CONFIG_PATCHES_SERVINT_TRAINS,       InValidateDetailsWindow),
	 SDT_VAR(Patches, servint_roadveh,   SLE_UINT16, 0,D0,   150,     5,     800, 0, STR_CONFIG_PATCHES_SERVINT_ROADVEH,      InValidateDetailsWindow),
	 SDT_VAR(Patches, servint_ships,     SLE_UINT16, 0,D0,   360,     5,     800, 0, STR_CONFIG_PATCHES_SERVINT_SHIPS,        InValidateDetailsWindow),
	 SDT_VAR(Patches, servint_aircraft,  SLE_UINT16, 0,D0,   100,     5,     800, 0, STR_CONFIG_PATCHES_SERVINT_AIRCRAFT,     InValidateDetailsWindow),
	SDT_BOOL(Patches, no_servicing_if_no_breakdowns, 0, 0, false,                    STR_CONFIG_PATCHES_NOSERVICE,            NULL),
	SDT_BOOL(Patches, wagon_speed_limits,            0,NN,  true,                    STR_CONFIG_PATCHES_WAGONSPEEDLIMITS,     UpdateConsists),
	SDT_CONDBOOL(Patches, disable_elrails, 38, SL_MAX_VERSION, 0, NN, false,         STR_CONFIG_PATCHES_DISABLE_ELRAILS,      SettingsDisableElrail),
	SDT_CONDVAR(Patches, freight_trains, SLE_UINT8, 39, SL_MAX_VERSION, 0,NN, 1, 1, 255, 1, STR_CONFIG_PATCHES_FREIGHT_TRAINS, NULL),
	SDT_CONDBOOL(Patches, timetabling,              67, SL_MAX_VERSION, 0, 0, true,  STR_CONFIG_PATCHES_TIMETABLE_ALLOW,      NULL),

	/***************************************************************************/
	/* Station section of the GUI-configure patches window */
	SDT_BOOL(Patches, join_stations,           0, 0,  true,        STR_CONFIG_PATCHES_JOINSTATIONS,       NULL),
	SDT_BOOL(Patches, full_load_any,           0,NN,  true,        STR_CONFIG_PATCHES_FULLLOADANY,        NULL),
	SDT_BOOL(Patches, improved_load,           0,NN, false,        STR_CONFIG_PATCHES_IMPROVEDLOAD,       NULL),
	SDT_BOOL(Patches, selectgoods,             0, 0,  true,        STR_CONFIG_PATCHES_SELECTGOODS,        NULL),
	SDT_BOOL(Patches, new_nonstop,             0, 0, false,        STR_CONFIG_PATCHES_NEW_NONSTOP,        NULL),
	SDT_BOOL(Patches, nonuniform_stations,     0,NN,  true,        STR_CONFIG_PATCHES_NONUNIFORM_STATIONS,NULL),
	 SDT_VAR(Patches, station_spread,SLE_UINT8,0, 0, 12, 4, 64, 0, STR_CONFIG_PATCHES_STATION_SPREAD,     InvalidateStationBuildWindow),
	SDT_BOOL(Patches, serviceathelipad,        0, 0,  true,        STR_CONFIG_PATCHES_SERVICEATHELIPAD,   NULL),
	SDT_BOOL(Patches, modified_catchment,      0, 0,  true,        STR_CONFIG_PATCHES_CATCHMENT,          NULL),
	SDT_CONDBOOL(Patches, gradual_loading, 40, SL_MAX_VERSION, 0, 0,  true, STR_CONFIG_PATCHES_GRADUAL_LOADING,    NULL),
	SDT_CONDBOOL(Patches, road_stop_on_town_road, 47, SL_MAX_VERSION, 0, 0, false, STR_CONFIG_PATCHES_STOP_ON_TOWN_ROAD, NULL),
	SDT_CONDBOOL(Patches, adjacent_stations,      62, SL_MAX_VERSION, 0, 0, true,  STR_CONFIG_PATCHES_ADJACENT_STATIONS, NULL),

	/***************************************************************************/
	/* Economy section of the GUI-configure patches window */
	SDT_BOOL(Patches, inflation,                  0, 0,  true,            STR_CONFIG_PATCHES_INFLATION,        NULL),
	 SDT_VAR(Patches, raw_industry_construction,SLE_UINT8,0,MS,0,0, 2, 0, STR_CONFIG_PATCHES_RAW_INDUSTRY_CONSTRUCTION_METHOD, NULL),
	SDT_BOOL(Patches, multiple_industry_per_town, 0, 0, false,            STR_CONFIG_PATCHES_MULTIPINDTOWN,    NULL),
	SDT_BOOL(Patches, same_industry_close,        0, 0, false,            STR_CONFIG_PATCHES_SAMEINDCLOSE,     NULL),
	SDT_BOOL(Patches, bribe,                      0, 0,  true,            STR_CONFIG_PATCHES_BRIBE,            NULL),
	SDT_CONDBOOL(Patches, exclusive_rights,           79, SL_MAX_VERSION, 0, 0, true,           STR_CONFIG_PATCHES_ALLOW_EXCLUSIVE, NULL),
	SDT_CONDBOOL(Patches, give_money,                 79, SL_MAX_VERSION, 0, 0, true,           STR_CONFIG_PATCHES_ALLOW_GIVE_MONEY, NULL),
	 SDT_VAR(Patches, snow_line_height,SLE_UINT8, 0, 0,     7,  2, 13, 0, STR_CONFIG_PATCHES_SNOWLINE_HEIGHT,  NULL),
	 SDT_VAR(Patches, colored_news_year,SLE_INT32, 0,NC,  2000, MIN_YEAR, MAX_YEAR, 1, STR_CONFIG_PATCHES_COLORED_NEWS_YEAR,NULL),
	 SDT_VAR(Patches, starting_year,    SLE_INT32, 0,NC,  1950, MIN_YEAR, MAX_YEAR, 1, STR_CONFIG_PATCHES_STARTING_YEAR,NULL),
	 SDT_VAR(Patches, ending_year,      SLE_INT32,0,NC|NO,2051, MIN_YEAR, MAX_YEAR, 1, STR_CONFIG_PATCHES_ENDING_YEAR,  NULL),
	SDT_BOOL(Patches, smooth_economy,             0, 0,  true,            STR_CONFIG_PATCHES_SMOOTH_ECONOMY,   NULL),
	SDT_BOOL(Patches, allow_shares,               0, 0, false,            STR_CONFIG_PATCHES_ALLOW_SHARES,     NULL),
	SDT_CONDVAR(Patches, town_growth_rate,  SLE_UINT8, 54, SL_MAX_VERSION, 0, MS, 2, 0,   4, 0, STR_CONFIG_PATCHES_TOWN_GROWTH,          NULL),
	SDT_CONDVAR(Patches, larger_towns,      SLE_UINT8, 54, SL_MAX_VERSION, 0, D0, 4, 0, 255, 1, STR_CONFIG_PATCHES_LARGER_TOWNS,         NULL),
	SDT_CONDVAR(Patches, initial_city_size, SLE_UINT8, 56, SL_MAX_VERSION, 0, 0,  2, 1,  10, 1, STR_CONFIG_PATCHES_CITY_SIZE_MULTIPLIER, NULL),
	SDT_CONDBOOL(Patches, mod_road_rebuild,            77, SL_MAX_VERSION, 0, 0, false,         STR_CONFIG_MODIFIED_ROAD_REBUILD,        NULL),

	/***************************************************************************/
	/* AI section of the GUI-configure patches window */
	SDT_BOOL(Patches, ainew_active,           0, 0, false, STR_CONFIG_PATCHES_AINEW_ACTIVE,      AiNew_PatchActive_Warning),
	SDT_BOOL(Patches, ai_in_multiplayer,      0, 0, false, STR_CONFIG_PATCHES_AI_IN_MULTIPLAYER, Ai_In_Multiplayer_Warning),
	SDT_BOOL(Patches, ai_disable_veh_train,   0, 0, false, STR_CONFIG_PATCHES_AI_BUILDS_TRAINS,  NULL),
	SDT_BOOL(Patches, ai_disable_veh_roadveh, 0, 0, false, STR_CONFIG_PATCHES_AI_BUILDS_ROADVEH, NULL),
	SDT_BOOL(Patches, ai_disable_veh_aircraft,0, 0, false, STR_CONFIG_PATCHES_AI_BUILDS_AIRCRAFT,NULL),
	SDT_BOOL(Patches, ai_disable_veh_ship,    0, 0, false, STR_CONFIG_PATCHES_AI_BUILDS_SHIPS,   NULL),

	/***************************************************************************/
	/* Patches without any GUI representation */
	SDT_BOOL(Patches, keep_all_autosave,              S, 0, false,         STR_NULL, NULL),
	SDT_BOOL(Patches, autosave_on_exit,               S, 0, false,         STR_NULL, NULL),
	 SDT_VAR(Patches, max_num_autosaves,   SLE_UINT8, S, 0, 16, 0, 255, 0, STR_NULL, NULL),
	SDT_BOOL(Patches, bridge_pillars,                 S, 0,  true,         STR_NULL, NULL),
	 SDT_VAR(Patches, extend_vehicle_life, SLE_UINT8, 0, 0,  0, 0, 100, 0, STR_NULL, NULL),
	SDT_BOOL(Patches, auto_euro,                      S, 0,  true,         STR_NULL, NULL),
	 SDT_VAR(Patches, dist_local_authority,SLE_UINT8, 0, 0, 20, 5,  60, 0, STR_NULL, NULL),
	 SDT_VAR(Patches, wait_oneway_signal,  SLE_UINT8, 0, 0, 15, 2, 100, 0, STR_NULL, NULL),
	 SDT_VAR(Patches, wait_twoway_signal,  SLE_UINT8, 0, 0, 41, 2, 100, 0, STR_NULL, NULL),

	/***************************************************************************/
	/* New Pathfinding patch settings */
	SDT_VAR(Patches, pf_maxlength,      SLE_UINT16, 0, 0,  4096,  64,  65535, 0, STR_NULL, NULL),
	SDT_VAR(Patches, pf_maxdepth,        SLE_UINT8, 0, 0,    48,   4,    255, 0, STR_NULL, NULL),
	/* The maximum number of nodes to search */
	SDT_VAR(Patches, npf_max_search_nodes,SLE_UINT, 0, 0, 10000, 500, 100000, 0, STR_NULL, NULL),

	/* When a red signal is encountered, a small detour can be made around
	 * it. This specifically occurs when a track is doubled, in which case
	 * the detour is typically 2 tiles. It is also often used at station
	 * entrances, when there is a choice of multiple platforms. If we take
	 * a typical 4 platform station, the detour is 4 tiles. To properly
	 * support larger stations we increase this value.
	 * We want to prevent that trains that want to leave at one side of a
	 * station, leave through the other side, turn around, enter the
	 * station on another platform and exit the station on the right side
	 * again, just because the sign at the right side was red. If we take
	 * a typical 5 length station, this detour is 10 or 11 tiles (not
	 * sure), so we set the default penalty at 10 (the station tile
	 * penalty will further prevent this.
	 * We give presignal exits (and combo's) a different (larger) penalty, because
	 * we really don't want trains waiting in front of a presignal exit. */
	SDT_VAR(Patches, npf_rail_firstred_penalty,     SLE_UINT, 0, 0, (10 * NPF_TILE_LENGTH), 0, 100000, 0, STR_NULL, NULL),
	SDT_VAR(Patches, npf_rail_firstred_exit_penalty,SLE_UINT, 0, 0, (100 * NPF_TILE_LENGTH),0, 100000, 0, STR_NULL, NULL),
	/* This penalty is for when the last signal before the target is red.
	 * This is useful for train stations, where there are multiple
	 * platforms to choose from, which lie in different signal blocks.
	 * Every target in a occupied signal block (ie an occupied platform)
	 * will get this penalty. */
	SDT_VAR(Patches, npf_rail_lastred_penalty, SLE_UINT, 0, 0, (10 * NPF_TILE_LENGTH), 0, 100000, 0, STR_NULL, NULL),
	/* When a train plans a route over a station tile, this penalty is
	 * applied. We want that trains plan a route around a typical, 4x5
	 * station, which means two tiles to the right, and two tiles back to
	 * the left around it, or 5 tiles of station through it. If we assign
	 * a penalty of 1 tile for every station tile passed, the route will
	 * be around it. */
	SDT_VAR(Patches, npf_rail_station_penalty, SLE_UINT, 0, 0, (1 * NPF_TILE_LENGTH), 0, 100000, 0, STR_NULL, NULL),
	SDT_VAR(Patches, npf_rail_slope_penalty,   SLE_UINT, 0, 0, (1 * NPF_TILE_LENGTH), 0, 100000, 0, STR_NULL, NULL),
	/* This penalty is applied when a train makes a turn. Its value of 1 makes
	 * sure that it has a minimal impact on the pathfinding, only when two
	 * paths have equal length it will make a difference */
	SDT_VAR(Patches, npf_rail_curve_penalty,        SLE_UINT, 0, 0, 1,                      0, 100000, 0, STR_NULL, NULL),
	/* Ths penalty is applied when a vehicle reverses inside a depot (doesn't
	 * apply to ships, as they can just come out the other end). XXX: Is this a
	 * good value? */
	SDT_VAR(Patches, npf_rail_depot_reverse_penalty,SLE_UINT, 0, 0, (NPF_TILE_LENGTH * 50), 0, 100000, 0, STR_NULL, NULL),
	SDT_VAR(Patches, npf_buoy_penalty,              SLE_UINT, 0, 0, (2 * NPF_TILE_LENGTH),  0, 100000, 0, STR_NULL, NULL),
	/* This penalty is applied when a ship makes a turn. It is bigger than the
	 * rail curve penalty, since ships (realisticly) have more trouble with
	 * making turns */
	SDT_VAR(Patches, npf_water_curve_penalty,       SLE_UINT, 0, 0, (NPF_TILE_LENGTH / 4),  0, 100000, 0, STR_NULL, NULL),
	/* This is the penalty for road, same as for rail. */
	SDT_VAR(Patches, npf_road_curve_penalty,        SLE_UINT, 0, 0, 1,                      0, 100000, 0, STR_NULL, NULL),
	/* This is the penalty for level crossings, for both road and rail vehicles */
	SDT_VAR(Patches, npf_crossing_penalty,          SLE_UINT, 0, 0, (3 * NPF_TILE_LENGTH),  0, 100000, 0, STR_NULL, NULL),
	/* This is the penalty for drive-through road, stops. */
	SDT_CONDVAR (Patches, npf_road_drive_through_penalty, SLE_UINT, 47, SL_MAX_VERSION, 0, 0,  8 * NPF_TILE_LENGTH, 0, 1000000, 0, STR_NULL, NULL),


	/* The maximum number of nodes to search */
	SDT_CONDBOOL(Patches, yapf.disable_node_optimization  ,           28, SL_MAX_VERSION, 0, 0, false                   ,                       STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.max_search_nodes           , SLE_UINT, 28, SL_MAX_VERSION, 0, 0, 10000                   ,      500, 1000000, 0, STR_NULL, NULL),
	SDT_CONDBOOL(Patches, yapf.rail_firstred_twoway_eol   ,           28, SL_MAX_VERSION, 0, 0,  true                   ,                       STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.rail_firstred_penalty      , SLE_UINT, 28, SL_MAX_VERSION, 0, 0,    10 * YAPF_TILE_LENGTH,        0, 1000000, 0, STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.rail_firstred_exit_penalty , SLE_UINT, 28, SL_MAX_VERSION, 0, 0,   100 * YAPF_TILE_LENGTH,        0, 1000000, 0, STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.rail_lastred_penalty       , SLE_UINT, 28, SL_MAX_VERSION, 0, 0,    10 * YAPF_TILE_LENGTH,        0, 1000000, 0, STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.rail_lastred_exit_penalty  , SLE_UINT, 28, SL_MAX_VERSION, 0, 0,   100 * YAPF_TILE_LENGTH,        0, 1000000, 0, STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.rail_station_penalty       , SLE_UINT, 28, SL_MAX_VERSION, 0, 0,    30 * YAPF_TILE_LENGTH,        0, 1000000, 0, STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.rail_slope_penalty         , SLE_UINT, 28, SL_MAX_VERSION, 0, 0,     2 * YAPF_TILE_LENGTH,        0, 1000000, 0, STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.rail_curve45_penalty       , SLE_UINT, 28, SL_MAX_VERSION, 0, 0,     1 * YAPF_TILE_LENGTH,        0, 1000000, 0, STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.rail_curve90_penalty       , SLE_UINT, 28, SL_MAX_VERSION, 0, 0,     6 * YAPF_TILE_LENGTH,        0, 1000000, 0, STR_NULL, NULL),
	/* This penalty is applied when a train reverses inside a depot */
	SDT_CONDVAR (Patches, yapf.rail_depot_reverse_penalty , SLE_UINT, 28, SL_MAX_VERSION, 0, 0,    50 * YAPF_TILE_LENGTH,        0, 1000000, 0, STR_NULL, NULL),
	/* This is the penalty for level crossings (for trains only) */
	SDT_CONDVAR (Patches, yapf.rail_crossing_penalty      , SLE_UINT, 28, SL_MAX_VERSION, 0, 0,     3 * YAPF_TILE_LENGTH,        0, 1000000, 0, STR_NULL, NULL),
	/* look-ahead how many signals are checked */
	SDT_CONDVAR (Patches, yapf.rail_look_ahead_max_signals, SLE_UINT, 28, SL_MAX_VERSION, 0, 0,    10                   ,        1,     100, 0, STR_NULL, NULL),
	/* look-ahead n-th red signal penalty polynomial: penalty = p2 * n^2 + p1 * n + p0 */
	SDT_CONDVAR (Patches, yapf.rail_look_ahead_signal_p0  , SLE_INT , 28, SL_MAX_VERSION, 0, 0,   500                   , -1000000, 1000000, 0, STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.rail_look_ahead_signal_p1  , SLE_INT , 28, SL_MAX_VERSION, 0, 0,  -100                   , -1000000, 1000000, 0, STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.rail_look_ahead_signal_p2  , SLE_INT , 28, SL_MAX_VERSION, 0, 0,     5                   , -1000000, 1000000, 0, STR_NULL, NULL),
	/* penalties for too long or too short station platforms */
	SDT_CONDVAR (Patches, yapf.rail_longer_platform_penalty,           SLE_UINT, 33, SL_MAX_VERSION, 0, 0,  8 * YAPF_TILE_LENGTH, 0,   20000, 0, STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.rail_longer_platform_per_tile_penalty,  SLE_UINT, 33, SL_MAX_VERSION, 0, 0,  0 * YAPF_TILE_LENGTH, 0,   20000, 0, STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.rail_shorter_platform_penalty,          SLE_UINT, 33, SL_MAX_VERSION, 0, 0, 40 * YAPF_TILE_LENGTH, 0,   20000, 0, STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.rail_shorter_platform_per_tile_penalty, SLE_UINT, 33, SL_MAX_VERSION, 0, 0,  0 * YAPF_TILE_LENGTH, 0,   20000, 0, STR_NULL, NULL),
	/* road vehicles - penalties */
	SDT_CONDVAR (Patches, yapf.road_slope_penalty                    , SLE_UINT, 33, SL_MAX_VERSION, 0, 0,  2 * YAPF_TILE_LENGTH, 0, 1000000, 0, STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.road_curve_penalty                    , SLE_UINT, 33, SL_MAX_VERSION, 0, 0,  1 * YAPF_TILE_LENGTH, 0, 1000000, 0, STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.road_crossing_penalty                 , SLE_UINT, 33, SL_MAX_VERSION, 0, 0,  3 * YAPF_TILE_LENGTH, 0, 1000000, 0, STR_NULL, NULL),
	SDT_CONDVAR (Patches, yapf.road_stop_penalty                     , SLE_UINT, 47, SL_MAX_VERSION, 0, 0,  8 * YAPF_TILE_LENGTH, 0, 1000000, 0, STR_NULL, NULL),

	/***************************************************************************/
	/* Terrain genation related patch options */
	SDT_CONDVAR(Patches,      land_generator,           SLE_UINT8,  30, SL_MAX_VERSION, 0, MS,   1,                   0,    1,               0, STR_CONFIG_PATCHES_LAND_GENERATOR,           NULL),
	SDT_CONDVAR(Patches,      oil_refinery_limit,       SLE_UINT8,  30, SL_MAX_VERSION, 0, 0,   32,                  12,   48,               0, STR_CONFIG_PATCHES_OIL_REF_EDGE_DISTANCE,    NULL),
	SDT_CONDVAR(Patches,      tgen_smoothness,          SLE_UINT8,  30, SL_MAX_VERSION, 0, MS,   1,                   0,    3,               0, STR_CONFIG_PATCHES_ROUGHNESS_OF_TERRAIN,     NULL),
	SDT_CONDVAR(Patches,      generation_seed,          SLE_UINT32, 30, SL_MAX_VERSION, 0, 0,    GENERATE_NEW_SEED,   0, MAX_UVALUE(uint32), 0, STR_NULL,                                    NULL),
	SDT_CONDVAR(Patches,      tree_placer,              SLE_UINT8,  30, SL_MAX_VERSION, 0, MS,   2,                   0,    2,               0, STR_CONFIG_PATCHES_TREE_PLACER,              NULL),
	SDT_VAR    (Patches,      heightmap_rotation,       SLE_UINT8,                      S, MS,   0,                   0,    1,               0, STR_CONFIG_PATCHES_HEIGHTMAP_ROTATION,       NULL),
	SDT_VAR    (Patches,      se_flat_world_height,     SLE_UINT8,                      S, 0,    0,                   0,   15,               0, STR_CONFIG_PATCHES_SE_FLAT_WORLD_HEIGHT,     NULL),

	SDT_END()
};

static const SettingDesc _currency_settings[] = {
	SDT_VAR(CurrencySpec, rate,    SLE_UINT16, S, 0,  1, 0, 100, 0, STR_NULL, NULL),
	SDT_CHR(CurrencySpec, separator,           S, 0,        ".",    STR_NULL, NULL),
	SDT_VAR(CurrencySpec, to_euro,  SLE_INT32, S, 0,  0, 0, 3000, 0, STR_NULL, NULL),
	SDT_STR(CurrencySpec, prefix,   SLE_STRBQ, S, 0,       NULL,    STR_NULL, NULL),
	SDT_STR(CurrencySpec, suffix,   SLE_STRBQ, S, 0, " credits",    STR_NULL, NULL),
	SDT_END()
};

/* Undefine for the shortcut macros above */
#undef S
#undef C
#undef N

#undef D0
#undef NC
#undef MS
#undef NO
#undef CR

static uint NewsDisplayLoadConfig(IniFile *ini, const char *grpname)
{
	IniGroup *group = ini_getgroup(ini, grpname, -1);
	IniItem *item;
	/* By default, set everything to full (0xAAAAAAAA = 1010101010101010) */
	uint res = 0xAAAAAAAA;

	/* If no group exists, return everything full */
	if (group == NULL) return res;

	for (item = group->item; item != NULL; item = item->next) {
		int news_item = -1;
		for (int i = 0; i < NT_END; i++) {
			if (strcasecmp(item->name, _news_display_name[i]) == 0) {
				news_item = i;
				break;
			}
		}
		if (news_item == -1) {
			DEBUG(misc, 0, "Invalid display option: %s", item->name);
			continue;
		}

		if (strcasecmp(item->value, "full") == 0) {
			SB(res, news_item * 2, 2, 2);
		} else if (strcasecmp(item->value, "off") == 0) {
			SB(res, news_item * 2, 2, 0);
		} else if (strcasecmp(item->value, "summarized") == 0) {
			SB(res, news_item * 2, 2, 1);
		} else {
			DEBUG(misc, 0, "Invalid display value: %s", item->value);
			continue;
		}
	}

	return res;
}

/* Load a GRF configuration from the given group name */
static GRFConfig *GRFLoadConfig(IniFile *ini, const char *grpname, bool is_static)
{
	IniGroup *group = ini_getgroup(ini, grpname, -1);
	IniItem *item;
	GRFConfig *first = NULL;
	GRFConfig **curr = &first;

	if (group == NULL) return NULL;

	for (item = group->item; item != NULL; item = item->next) {
		GRFConfig *c = CallocT<GRFConfig>(1);
		c->filename = strdup(item->name);

		/* Parse parameters */
		if (*item->value != '\0') {
			c->num_params = parse_intlist(item->value, (int*)c->param, lengthof(c->param));
			if (c->num_params == (byte)-1) {
				ShowInfoF("ini: error in array '%s'", item->name);
				c->num_params = 0;
			}
		}

		/* Check if item is valid */
		if (!FillGRFDetails(c, is_static)) {
			const char *msg;

			if (c->status == GCS_NOT_FOUND) {
				msg = "not found";
			} else if (HASBIT(c->flags, GCF_UNSAFE)) {
				msg = "unsafe for static use";
			} else if (HASBIT(c->flags, GCF_SYSTEM)) {
				msg = "system NewGRF";
			} else {
				msg = "unknown";
			}

			ShowInfoF("ini: ignoring invalid NewGRF '%s': %s", item->name, msg);
			ClearGRFConfig(&c);
			continue;
		}

		/* Mark file as static to avoid saving in savegame. */
		if (is_static) SETBIT(c->flags, GCF_STATIC);

		/* Add item to list */
		*curr = c;
		curr = &c->next;
	}

	return first;
}

static void NewsDisplaySaveConfig(IniFile *ini, const char *grpname, uint news_display)
{
	IniGroup *group = ini_getgroup(ini, grpname, -1);
	IniItem **item;

	if (group == NULL) return;
	group->item = NULL;
	item = &group->item;

	for (int i = 0; i < NT_END; i++) {
		const char *value;
		int v = GB(news_display, i * 2, 2);

		value = (v == 0 ? "off" : (v == 1 ? "summarized" : "full"));

		*item = ini_item_alloc(group, _news_display_name[i], strlen(_news_display_name[i]));
		(*item)->value = (char*)pool_strdup(&ini->pool, value, strlen(value));
		item = &(*item)->next;
	}
}

/* Save a GRF configuration to the given group name */
static void GRFSaveConfig(IniFile *ini, const char *grpname, const GRFConfig *list)
{
	IniGroup *group = ini_getgroup(ini, grpname, -1);
	IniItem **item;
	const GRFConfig *c;

	if (group == NULL) return;
	group->item = NULL;
	item = &group->item;

	for (c = list; c != NULL; c = c->next) {
		char params[512];
		GRFBuildParamList(params, c, lastof(params));

		*item = ini_item_alloc(group, c->filename, strlen(c->filename));
		(*item)->value = (char*)pool_strdup(&ini->pool, params, strlen(params));
		item = &(*item)->next;
	}
}

/* Common handler for saving/loading variables to the configuration file */
static void HandleSettingDescs(IniFile *ini, SettingDescProc *proc, SettingDescProcList *proc_list)
{
	proc(ini, (const SettingDesc*)_misc_settings,    "misc",  NULL);
	proc(ini, (const SettingDesc*)_music_settings,   "music", &msf);
#ifdef WIN32
	proc(ini, (const SettingDesc*)_win32_settings,   "win32", NULL);
#endif /* WIN32 */

	proc(ini, _gameopt_settings, "gameopt",  &_opt_newgame);
	proc(ini, _patch_settings,   "patches",  &_patches_newgame);
	proc(ini, _currency_settings,"currency", &_custom_currency);

#ifdef ENABLE_NETWORK
	proc(ini, (const SettingDesc*)_network_settings, "network", NULL);
	proc_list(ini, "servers", _network_host_list, lengthof(_network_host_list), NULL);
	proc_list(ini, "bans",    _network_ban_list,  lengthof(_network_ban_list), NULL);
#endif /* ENABLE_NETWORK */
}

extern void CheckDifficultyLevels();

/** Load the values from the configuration files */
void LoadFromConfig()
{
	IniFile *ini = ini_load(_config_file);
	ResetCurrencies(false); // Initialize the array of curencies, without preserving the custom one
	HandleSettingDescs(ini, ini_load_settings, ini_load_setting_list);
	_grfconfig_newgame = GRFLoadConfig(ini, "newgrf", false);
	_grfconfig_static  = GRFLoadConfig(ini, "newgrf-static", true);
	_news_display_opt  = NewsDisplayLoadConfig(ini, "news_display");
	CheckDifficultyLevels();
	ini_free(ini);
}

/** Save the values to the configuration file */
void SaveToConfig()
{
	IniFile *ini = ini_load(_config_file);
	HandleSettingDescs(ini, ini_save_settings, ini_save_setting_list);
	GRFSaveConfig(ini, "newgrf", _grfconfig_newgame);
	GRFSaveConfig(ini, "newgrf-static", _grfconfig_static);
	NewsDisplaySaveConfig(ini, "news_display", _news_display_opt);
	ini_save(_config_file, ini);
	ini_free(ini);
}

static const SettingDesc *GetSettingDescription(uint index)
{
	if (index >= lengthof(_patch_settings)) return NULL;
	return &_patch_settings[index];
}

/** Network-safe changing of patch-settings (server-only).
 * @param tile unused
 * @param flags operation to perform
 * @param p1 the index of the patch in the SettingDesc array which identifies it
 * @param p2 the new value for the patch
 * The new value is properly clamped to its minimum/maximum when setting
 * @see _patch_settings
 */
CommandCost CmdChangePatchSetting(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	const SettingDesc *sd = GetSettingDescription(p1);

	if (sd == NULL) return CMD_ERROR;
	if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) return CMD_ERROR;

	if ((sd->desc.flags & SGF_NO_NETWORK) && _networking) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Patches *patches_ptr = (_game_mode == GM_MENU) ? &_patches_newgame : &_patches;
		void *var = GetVariableAddress(patches_ptr, &sd->save);
		Write_ValidateSetting(var, sd, (int32)p2);
		if (sd->desc.proc != NULL) sd->desc.proc((int32)ReadValue(var, sd->save.conv));

		InvalidateWindow(WC_GAME_OPTIONS, 0);
	}

	return CommandCost();
}

/** Top function to save the new value of an element of the Patches struct
 * @param index offset in the SettingDesc array of the Patches struct which
 * identifies the patch member we want to change
 * @param object pointer to a valid patches struct that has its settings change.
 * This only affects patch-members that are not needed to be the same on all
 * clients in a network game.
 * @param value new value of the patch */
bool SetPatchValue(uint index, const Patches *object, int32 value)
{
	const SettingDesc *sd = &_patch_settings[index];
	/* If an item is player-based, we do not send it over the network
	 * (if any) to change. Also *hack*hack* we update the _newgame version
	 * of patches because changing a player-based setting in a game also
	 * changes its defaults. At least that is the convention we have chosen */
	if (sd->save.conv & SLF_NETWORK_NO) {
		void *var = GetVariableAddress(object, &sd->save);
		Write_ValidateSetting(var, sd, value);

		if (_game_mode != GM_MENU) {
			void *var2 = GetVariableAddress(&_patches_newgame, &sd->save);
			Write_ValidateSetting(var2, sd, value);
		}
		if (sd->desc.proc != NULL) sd->desc.proc((int32)ReadValue(var, sd->save.conv));
		InvalidateWindow(WC_GAME_OPTIONS, 0);
		return true;
	}

	/* send non-player-based settings over the network */
	if (!_networking || (_networking && _network_server)) {
		return DoCommandP(0, index, value, NULL, CMD_CHANGE_PATCH_SETTING);
	}
	return false;
}

const SettingDesc *GetPatchFromName(const char *name, uint *i)
{
	const SettingDesc *sd;

	for (*i = 0, sd = _patch_settings; sd->save.cmd != SL_END; sd++, (*i)++) {
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
		if (strcmp(sd->desc.name, name) == 0) return sd;
	}

	return NULL;
}

/* Those 2 functions need to be here, else we have to make some stuff non-static
 * and besides, it is also better to keep stuff like this at the same place */
bool IConsoleSetPatchSetting(const char *name, int32 value)
{
	bool success;
	uint index;
	const SettingDesc *sd = GetPatchFromName(name, &index);
	const Patches *patches_ptr;
	void *ptr;

	if (sd == NULL) {
		IConsolePrintF(_icolour_warn, "'%s' is an unknown patch setting.", name);
		return true;
	}

	patches_ptr = (_game_mode == GM_MENU) ? &_patches_newgame : &_patches;
	ptr = GetVariableAddress(patches_ptr, &sd->save);

	success = SetPatchValue(index, patches_ptr, value);
	return success;
}

void IConsoleGetPatchSetting(const char *name)
{
	char value[20];
	uint index;
	const SettingDesc *sd = GetPatchFromName(name, &index);
	const void *ptr;

	if (sd == NULL) {
		IConsolePrintF(_icolour_warn, "'%s' is an unknown patch setting.", name);
		return;
	}

	ptr = GetVariableAddress((_game_mode == GM_MENU) ? &_patches_newgame : &_patches, &sd->save);

	if (sd->desc.cmd == SDT_BOOLX) {
		snprintf(value, sizeof(value), (*(bool*)ptr == 1) ? "on" : "off");
	} else {
		snprintf(value, sizeof(value), "%d", (int32)ReadValue(ptr, sd->save.conv));
	}

	IConsolePrintF(_icolour_warn, "Current value for '%s' is: '%s' (min: %s%d, max: %d)",
		name, value, (sd->desc.flags & SGF_0ISDISABLED) ? "(0) " : "", sd->desc.min, sd->desc.max);
}

void IConsoleListPatches()
{
	IConsolePrintF(_icolour_warn, "All patches with their current value:");

	for (const SettingDesc *sd = _patch_settings; sd->save.cmd != SL_END; sd++) {
		char value[80];
		const void *ptr = GetVariableAddress((_game_mode == GM_MENU) ? &_patches_newgame : &_patches, &sd->save);

		if (sd->desc.cmd == SDT_BOOLX) {
			snprintf(value, lengthof(value), (*(bool*)ptr == 1) ? "on" : "off");
		} else {
			snprintf(value, lengthof(value), "%d", (uint32)ReadValue(ptr, sd->save.conv));
		}
		IConsolePrintF(_icolour_def, "%s = %s", sd->desc.name, value);
	}

	IConsolePrintF(_icolour_warn, "Use 'patch' command to change a value");
}

/** Save and load handler for patches/settings
 * @param osd SettingDesc struct containing all information
 * @param object can be either NULL in which case we load global variables or
 * a pointer to a struct which is getting saved */
static void LoadSettings(const SettingDesc *osd, void *object)
{
	for (; osd->save.cmd != SL_END; osd++) {
		const SaveLoad *sld = &osd->save;
		void *ptr = GetVariableAddress(object, sld);

		if (!SlObjectMember(ptr, sld)) continue;
	}
}

/** Loadhandler for a list of global variables
 * @param sdg pointer for the global variable list SettingDescGlobVarList
 * @note this is actually a stub for LoadSettings with the
 * object pointer set to NULL */
static inline void LoadSettingsGlobList(const SettingDescGlobVarList *sdg)
{
	LoadSettings((const SettingDesc*)sdg, NULL);
}

/** Save and load handler for patches/settings
 * @param sd SettingDesc struct containing all information
 * @param object can be either NULL in which case we load global variables or
 * a pointer to a struct which is getting saved */
static void SaveSettings(const SettingDesc *sd, void *object)
{
	/* We need to write the CH_RIFF header, but unfortunately can't call
	 * SlCalcLength() because we have a different format. So do this manually */
	const SettingDesc *i;
	size_t length = 0;
	for (i = sd; i->save.cmd != SL_END; i++) {
		const void *ptr = GetVariableAddress(object, &i->save);
		length += SlCalcObjMemberLength(ptr, &i->save);
	}
	SlSetLength(length);

	for (i = sd; i->save.cmd != SL_END; i++) {
		void *ptr = GetVariableAddress(object, &i->save);
		SlObjectMember(ptr, &i->save);
	}
}

/** Savehandler for a list of global variables
 * @note this is actually a stub for SaveSettings with the
 * object pointer set to NULL */
static inline void SaveSettingsGlobList(const SettingDescGlobVarList *sdg)
{
	SaveSettings((const SettingDesc*)sdg, NULL);
}

static void Load_OPTS()
{
	/* Copy over default setting since some might not get loaded in
	 * a networking environment. This ensures for example that the local
	 * autosave-frequency stays when joining a network-server */
	_opt = _opt_newgame;
	LoadSettings(_gameopt_settings, &_opt);
}

static void Save_OPTS()
{
	SaveSettings(_gameopt_settings, &_opt);
}

static void Load_PATS()
{
	/* Copy over default setting since some might not get loaded in
	 * a networking environment. This ensures for example that the local
	 * signal_side stays when joining a network-server */
	_patches = _patches_newgame;
	LoadSettings(_patch_settings, &_patches);
}

static void Save_PATS()
{
	SaveSettings(_patch_settings, &_patches);
}

void CheckConfig()
{
	// Increase old default values for pf_maxdepth and pf_maxlength
	// to support big networks.
	if (_patches_newgame.pf_maxdepth == 16 && _patches_newgame.pf_maxlength == 512) {
		_patches_newgame.pf_maxdepth = 48;
		_patches_newgame.pf_maxlength = 4096;
	}
}

void UpdatePatches()
{
	/* Since old(er) savegames don't have any patches saved, we initialise
	 * them with the default values just as it was in the old days.
	 * Also new games need this copying-over */
	_patches = _patches_newgame; /* backwards compatibility */
}

extern const ChunkHandler _setting_chunk_handlers[] = {
	{ 'OPTS', Save_OPTS, Load_OPTS, CH_RIFF},
	{ 'PATS', Save_PATS, Load_PATS, CH_RIFF | CH_LAST},
};

static bool IsSignedVarMemType(VarType vt)
{
	switch (GetVarMemType(vt)) {
		case SLE_VAR_I8:
		case SLE_VAR_I16:
		case SLE_VAR_I32:
		case SLE_VAR_I64:
			return true;
	}
	return false;
}
