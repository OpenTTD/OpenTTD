#include "stdafx.h"
#include "ttd.h"
#include "sound.h"
#include "string.h"
#include "table/currency.h"
#include "network.h"
#include "settings.h"

typedef struct IniFile IniFile;
typedef struct IniItem IniItem;
typedef struct IniGroup IniGroup;
typedef struct SettingsMemoryPool SettingsMemoryPool;

static void pool_init(SettingsMemoryPool **pool);
static void *pool_alloc(SettingsMemoryPool **pool, uint size);
static void *pool_strdup(SettingsMemoryPool **pool, const char *mem, uint size);
static void pool_free(SettingsMemoryPool **pool);

struct SettingsMemoryPool {
	uint pos,size;
	SettingsMemoryPool *next;
	byte mem[1];
};

static SettingsMemoryPool *pool_new(uint minsize)
{
	SettingsMemoryPool *p;
	if (minsize < 4096 - 12) minsize = 4096 - 12;

	p = malloc(sizeof(SettingsMemoryPool) - 1 + minsize);
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

	size = (size + 3) & ~3; // align everything to a 32 bit boundary

	// first check if there's memory in the next pool
	if (p->next && p->next->pos + size <= p->next->size) {
		p = p->next;
	// then check if there's not memory in the cur pool
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
	byte *p = pool_alloc(pool, size + 1);
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

// structs describing the ini format.
struct IniItem {
	char *name;
	char *value;
	char *comment;
	IniItem *next;
};

struct IniGroup {
	char *name; // name of group
	char *comment; //comment for group
	IniItem *item, **last_item;
	IniGroup *next;
	IniFile *ini;
	IniGroupType type; // type of group
};

struct IniFile {
	SettingsMemoryPool *pool;
	IniGroup *group, **last_group;
	char *comment; // last comment in file
};

// allocate an inifile object
static IniFile *ini_alloc(void)
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

// allocate an ini group object
static IniGroup *ini_group_alloc(IniFile *ini, const char *grpt, int len)
{
	IniGroup *grp = pool_alloc(&ini->pool, sizeof(IniGroup));
	grp->ini = ini;
	grp->name = pool_strdup(&ini->pool, grpt, len);
	if(!strcmp(grp->name, "newgrf") || !strcmp(grp->name, "servers") || !strcmp(grp->name, "bans") )
		grp->type = IGT_LIST;
	else
		grp->type = IGT_VARIABLES;
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
	IniItem *item = pool_alloc(&group->ini->pool, sizeof(IniItem));
	item->name = pool_strdup(&group->ini->pool, name, len);
	item->next = NULL;
	item->comment = NULL;
	item->value = NULL;
	*group->last_item = item;
	group->last_item = &item->next;
	return item;
}

// load an ini file into the "abstract" format
static IniFile *ini_load(const char *filename)
{
	char buffer[1024], c, *s, *t, *e;
	FILE *in;
	IniFile *ini;
	IniGroup *group = NULL;
	IniItem *item;

	byte *comment = NULL;
	uint comment_size = 0;
	uint comment_alloc = 0;

	ini = ini_alloc();

	in = fopen(filename, "r");
	if (in == NULL) return ini;

	// for each line in the file
	while (fgets(buffer, sizeof(buffer), in)) {

		// trim whitespace from the left side
		for(s=buffer; s[0] == ' ' || s[0] == '\t'; s++);

		// trim whitespace from right side.
		e = s + strlen(s);
		while (e > s && ((c=e[-1]) == '\n' || c == '\r' || c == ' ' || c == '\t')) e--;
		*e = 0;

		// skip comments and empty lines
		if (*s == '#' || *s == 0) {
			uint ns = comment_size + (e - s + 1);
			uint a = comment_alloc;
			uint pos;
			// add to comment
			if (ns > a) {
				a = max(a, 128);
				do a*=2; while (a < ns);
				comment = realloc(comment, comment_alloc = a);
			}
			pos = comment_size;
			comment_size += (e - s + 1);
			comment[pos + e - s] = '\n'; // comment newline
			memcpy(comment + pos, s, e - s); // copy comment contents
			continue;
		}

		// it's a group?
		if (s[0] == '[') {
			if (e[-1] != ']')
				ShowInfoF("ini: invalid group name '%s'\n", buffer);
			else
				e--;
			s++; // skip [
			group = ini_group_alloc(ini, s, e - s);
			if (comment_size) {
				group->comment = pool_strdup(&ini->pool, comment, comment_size);
				comment_size = 0;
			}
		} else if (group) {
			// find end of keyname
			for(t=s; *t != 0 && *t != '=' && *t != '\t' && *t != ' '; t++) {}

			// it's an item in an existing group
			item = ini_item_alloc(group, s, t-s);
			if (comment_size) {
				item->comment = pool_strdup(&ini->pool, comment, comment_size);
				comment_size = 0;
			}

			// for list items, the name and value are the same:
			if( group->type == IGT_LIST ) {
				item->value = item->name;
				continue;
			}

			// find start of parameter
			while (*t == '=' || *t == ' ' || *t == '\t') t++;


			// remove starting quotation marks
			if(*t=='\"') t++;
			// remove ending quotation marks
			e = t + strlen(t);
			if(e>t && e[-1] =='\"') e--;
			*e = 0;

			item->value = pool_strdup(&ini->pool, t, e - t);
		} else {
			// it's an orphan item
			ShowInfoF("ini: '%s' outside of group\n", buffer);
		}
	}

	if (comment_size) {
		ini->comment = pool_strdup(&ini->pool, comment, comment_size);
		comment_size = 0;
	}

	free(comment);
	fclose(in);

	return ini;
}

// lookup a group or make a new one
static IniGroup *ini_getgroup(IniFile *ini, const char *name, int len)
{
	IniGroup *group;

	if (len == -1) len = strlen(name);

	// does it exist already?
	for(group = ini->group; group; group = group->next)
		if (!memcmp(group->name, name, len) && group->name[len] == 0)
			return group;

	// otherwise make a new one
	group = ini_group_alloc(ini, name, len);
	group->comment = pool_strdup(&ini->pool, "\n", 1);
	return group;
}

// lookup an item or make a new one
static IniItem *ini_getitem(IniGroup *group, const char *name, bool create)
{
	IniItem *item;
	uint len = strlen(name);

	for(item = group->item; item; item = item->next)
		if (!strcmp(item->name, name))
			return item;

	if (!create) return NULL;

	// otherwise make a new one
	item = ini_item_alloc(group, name, len);
	return item;
}

// save ini file from the "abstract" format.
static bool ini_save(const char *filename, IniFile *ini)
{
	FILE *f;
	IniGroup *group;
	IniItem *item;

	f = fopen(filename, "w");
	if (f == NULL) return false;

	for(group = ini->group; group; group = group->next) {
		if (group->comment) fputs(group->comment, f);
		fprintf(f, "[%s]\n", group->name);
		for(item = group->item; item; item = item->next) {
			if (item->comment) fputs(item->comment, f);
			if(group->type==IGT_LIST)
				fprintf(f, "%s\n", item->value ? item->value : "");
			else
				fprintf(f, "%s = %s\n", item->name, item->value ? item->value : "");
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

static int lookup_oneofmany(const char *many, const char *one, int onelen)
{
	const char *s;
	int idx;

	if (onelen == -1) onelen = strlen(one);

	// check if it's an integer
	if (*one >= '0' && *one <= '9')
		return strtoul(one, NULL, 0);

	idx = 0;
	for(;;) {
		// find end of item
		s = many;
		while (*s != '|' && *s != 0) s++;
		if (s - many == onelen && !memcmp(one, many, onelen)) return idx;
		if (*s == 0) return -1;
		many = s + 1;
		idx++;
	}
}

static uint32 lookup_manyofmany(const char *many, const char *str)
{
	const char *s;
	int r;
	uint32 res = 0;

	for(;;) {
		// skip "whitespace"
		while (*str == ' ' || *str == '\t' || *str == '|') str++;
		if (*str == 0) break;

		s = str;
		while (*s != 0 && *s != ' ' && *s != '\t' && *s != '|') s++;

		r = lookup_oneofmany(many, str, s - str);
		if (r == -1) return (uint32)-1;

		res |= (1 << r);
		if (*s == 0) break;
		str = s + 1;
	}
	return res;
}

static int parse_intlist(const char *p, int *items, int maxitems)
{
	int n = 0, v;
	char *end;

	for(;;) {
		v = strtol(p, &end, 0);
		if (p == end || n == maxitems) return -1;
		p = end;
		items[n++] = v;
		if (*p == 0) break;
		if (*p != ',') return -1;
		p++;
	}

	return n;
}

static bool load_intlist(const char *str, void *array, int nelems, int type)
{
	int items[64];
	int i,nitems;

	if (str == NULL) {
		memset(items, 0, sizeof(items));
		nitems = nelems;
	} else {
		nitems = parse_intlist(str, items, lengthof(items));
		if (nitems != nelems)
			return false;
	}

	switch(type) {
	case SDT_INT8 >> 4:
	case SDT_UINT8 >> 4:
		for(i=0; i!=nitems; i++) ((byte*)array)[i] = items[i];
		break;
	case SDT_INT16 >> 4:
	case SDT_UINT16 >> 4:
		for(i=0; i!=nitems; i++) ((uint16*)array)[i] = items[i];
		break;
	case SDT_INT32 >> 4:
	case SDT_UINT32 >> 4:
		for(i=0; i!=nitems; i++) ((uint32*)array)[i] = items[i];
		break;
	default:
		NOT_REACHED();
	}

	return true;
}

static void make_intlist(char *buf, void *array, int nelems, int type)
{
	int i, v = 0;
	byte *p = (byte*)array;
	for(i=0; i!=nelems; i++) {
		switch(type) {
		case SDT_INT8 >> 4: v = *(int8*)p; p += 1; break;
		case SDT_UINT8 >> 4:v = *(byte*)p; p += 1; break;
		case SDT_INT16 >> 4:v = *(int16*)p; p += 2; break;
		case SDT_UINT16 >> 4:v = *(uint16*)p; p += 2; break;
		case SDT_INT32 >> 4:v = *(int32*)p; p += 4; break;
		case SDT_UINT32 >> 4:v = *(uint32*)p; p += 4; break;
		default: NOT_REACHED();
		}
		buf += sprintf(buf, i ? ",%d" : "%d", v);
	}
}

static void make_oneofmany(char *buf, const char *many, int i)
{
	int orig_i = i;
	char c;

	while (--i >= 0) {
		do {
			many++;
			if (many[-1] == 0) {
				sprintf(buf, "%d", orig_i);
				return;
			}
		} while (many[-1] != '|');
	}

	// copy until | or 0
	while ((c=*many++) != 0 && c != '|')
		*buf++ = c;
	*buf = 0;
}

static void make_manyofmany(char *buf, const char *many, uint32 x)
{
	const char *start;
	int i = 0;
	bool init = true;

	do {
		start = many;
		while (*many != 0 && *many != '|') many++;
		if (x & 1) {
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
	} while (++i, x>>=1);
	*buf = 0;
}

static const void *string_to_val(const SettingDesc *desc, const char *str)
{
	unsigned long val;
	char *end;

	switch(desc->flags & 0xF) {
	case SDT_INTX:
		val = strtol(str, &end, 0);
		if (*end != 0) ShowInfoF("ini: trailing characters at end of setting '%s'", desc->name);
		return (void*)val;
	case SDT_ONEOFMANY: {
		int r = lookup_oneofmany((const char*)desc->b, str, -1);
		if (r != -1) return (void*)r;
		ShowInfoF("ini: invalid value '%s' for '%s'", str, desc->name);
		return 0;
	}
	case SDT_MANYOFMANY: {
		uint32 r = lookup_manyofmany(desc->b, str);
		if (r != (uint32)-1) return (void*)r;
		ShowInfoF("ini: invalid value '%s' for '%s'", str, desc->name);
		return 0;
	}
	case SDT_BOOLX:
		if (!strcmp(str, "true") || !strcmp(str, "on") || !strcmp(str, "1"))
			return (void*)true;
		if (!strcmp(str, "false") || !strcmp(str, "off") || !strcmp(str, "0"))
			return (void*)false;
		ShowInfoF("ini: invalid setting value '%s' for '%s'", str, desc->name);
		break;

	case SDT_STRING:
	case SDT_STRINGBUF:
	case SDT_STRINGQUOT:
	case SDT_INTLIST:
		return str;
	}

	return NULL;
}

static void load_setting_desc(IniFile *ini, const SettingDesc *desc, const void *grpname)
{
	IniGroup *group_def = ini_getgroup(ini, grpname, -1), *group;
	IniItem *item;
	const void *p;
	void *ptr;

	for (;desc->name;desc++) {
		// group override?
		const char *s = strchr(desc->name, '.');
		if (s) {
			group = ini_getgroup(ini, desc->name, s - desc->name);
			s++;
		} else {
			s = desc->name;
			group = group_def;
		}

		item = ini_getitem(group, s, false);
		if (!item) {
			p = desc->def;
		} else {
			p = string_to_val(desc, item->value);
		}

		// get ptr to array
		ptr = desc->ptr;

		switch(desc->flags & 0xF) {
		// all these are stored in the same way
		case SDT_INTX:
		case SDT_ONEOFMANY:
		case SDT_MANYOFMANY:
		case SDT_BOOLX:
			switch(desc->flags >> 4 & 7) {
			case SDT_INT8 >> 4:
			case SDT_UINT8 >> 4:
				*(byte*)ptr = (byte)(uint)p;
				break;
			case SDT_INT16 >> 4:
			case SDT_UINT16 >> 4:
				*(uint16*)ptr = (uint16)(uint)p;
				break;
			case SDT_INT32 >> 4:
			case SDT_UINT32 >> 4:
				*(uint32*)ptr = (uint32)p;
				break;
			default:
				NOT_REACHED();
			}
			break;
		case SDT_STRING:
			free(*(char**)ptr);
			*(char**)ptr = strdup((const char*)p);
			break;
		case SDT_STRINGBUF:
		case SDT_STRINGQUOT:
			if (p) ttd_strlcpy((char*)ptr, p, desc->flags >> 16);
			break;
		case SDT_INTLIST: {
			if (!load_intlist(p, ptr, desc->flags >> 16, desc->flags >> 4 & 7))
				ShowInfoF("ini: error in array '%s'", desc->name);
			break;
		}
		default:
			NOT_REACHED();
		}
	}
}

static void save_setting_desc(IniFile *ini, const SettingDesc *desc, const void *grpname)
{
	IniGroup *group_def = NULL, *group;
	IniItem *item;
	const void *p;
	void *ptr;
	int i = 0;
	char buf[512]; // setting buffer
	const char *s;

	for (;desc->name;desc++) {
		if (desc->flags & SDT_NOSAVE)
			continue;

		// group override?
		s = strchr(desc->name, '.');
		if (s) {
			group = ini_getgroup(ini, desc->name, s - desc->name);
			s++;
		} else {
			if (group_def == NULL)
				group_def = ini_getgroup(ini, grpname, -1);
			s = desc->name;
			group = group_def;
		}

		item = ini_getitem(group, s, true);

		// get ptr to array
		ptr = desc->ptr;

		if (item->value != NULL) {
			// check if the value is the same as the old value
			p = string_to_val(desc, item->value);

			switch(desc->flags & 0xF) {
			case SDT_INTX:
			case SDT_ONEOFMANY:
			case SDT_MANYOFMANY:
			case SDT_BOOLX:
				switch(desc->flags >> 4 & 7) {
				case SDT_INT8 >> 4:
				case SDT_UINT8 >> 4:
					if (*(byte*)ptr == (byte)(uint)p)
						continue;
					break;
				case SDT_INT16 >> 4:
				case SDT_UINT16 >> 4:
					if (*(uint16*)ptr == (uint16)(uint)p)
						continue;
					break;
				case SDT_INT32 >> 4:
				case SDT_UINT32 >> 4:
					if (*(uint32*)ptr == (uint32)p)
						continue;
					break;
				default:
					NOT_REACHED();
				}
				break;
			case SDT_STRING:
				assert(0);
				break;
			case SDT_INTLIST:
				// assume intlist is always changed.
				break;
			}
		}

		switch(desc->flags & 0xF) {
		case SDT_INTX:
		case SDT_ONEOFMANY:
		case SDT_MANYOFMANY:
		case SDT_BOOLX:
			switch(desc->flags >> 4 & 7) {
			case SDT_INT8 >> 4: i = *(int8*)ptr; break;
			case SDT_UINT8 >> 4:i = *(byte*)ptr; break;
			case SDT_INT16 >> 4:i = *(int16*)ptr; break;
			case SDT_UINT16 >> 4:i = *(uint16*)ptr; break;
			case SDT_INT32 >> 4:i = *(int32*)ptr; break;
			case SDT_UINT32 >> 4:i = *(uint32*)ptr; break;
			default:
				NOT_REACHED();
			}
			switch(desc->flags & 0xF) {
			case SDT_INTX:
				sprintf(buf, "%d", i);
				break;
			case SDT_ONEOFMANY:
				make_oneofmany(buf, (const char*)desc->b, i);
				break;
			case SDT_MANYOFMANY:
				make_manyofmany(buf, (const char*)desc->b, i);
				break;
			case SDT_BOOLX:
				strcpy(buf, i ? "true" : "false");
				break;
			default:
				NOT_REACHED();
			}
			break;
		case SDT_STRINGQUOT:
			sprintf(buf, "\"%s\"", (char*)ptr);
			break;
		case SDT_STRINGBUF:
			strcpy(buf, (char*)ptr);
			break;
		case SDT_STRING:
			strcpy(buf, *(char**)ptr);
			break;
		case SDT_INTLIST:
			make_intlist(buf, ptr, desc->flags >> 16, desc->flags >> 4 & 7);
			break;
		}
		// the value is different, that means we have to write it to the ini
		item->value = pool_strdup(&ini->pool, buf, strlen(buf));
	}
}

//***************************
// TTD specific INI stuff
//***************************

static const SettingDesc music_settings[] = {
	{"playlist",	SDT_UINT8,	(void*)0,			&msf.playlist, NULL},
	{"music_vol", SDT_UINT8,	(void*)128,		&msf.music_vol, NULL},
	{"effect_vol",SDT_UINT8,	(void*)128,		&msf.effect_vol, NULL},
	{"custom_1",	SDT_INTLIST | SDT_UINT8 | lengthof(msf.custom_1) << 16, NULL, &msf.custom_1, NULL},
	{"custom_2",	SDT_INTLIST | SDT_UINT8 | lengthof(msf.custom_2) << 16, NULL, &msf.custom_2, NULL},
	{"playing",		SDT_BOOL,		(void*)true,	&msf.btn_down, NULL},
	{"shuffle",		SDT_BOOL,		(void*)false, &msf.shuffle, NULL},
	{NULL,				0,					NULL,					NULL,																NULL}
};

static const SettingDesc win32_settings[] = {
	{"display_hz",				SDT_UINT, (void*)0,			&_display_hz,					NULL},
	{"force_full_redraw", SDT_BOOL, (void*)false, &_force_full_redraw,	NULL},
	{"fullscreen_bpp",		SDT_UINT, (void*)8,			&_fullscreen_bpp,			NULL},
	{"double_size",				SDT_BOOL, (void*)false, &_double_size,				NULL},
	{NULL,								0,				NULL,					NULL,									NULL}
};

static const SettingDesc misc_settings[] = {
	{"display_opt",				SDT_MANYOFMANY | SDT_UINT8, (void*)(DO_SHOW_TOWN_NAMES|DO_SHOW_STATION_NAMES|DO_SHOW_SIGNS|DO_FULL_ANIMATION|DO_FULL_DETAIL|DO_TRANS_BUILDINGS|DO_WAYPOINTS), &_display_opt, "SHOW_TOWN_NAMES|SHOW_STATION_NAMES|SHOW_SIGNS|FULL_ANIMATION|TRANS_BUILDINGS|FULL_DETAIL|WAYPOINTS"},
	{"news_display_opt",	SDT_UINT16,		(void*)-1,		&_news_display_opt,		NULL},
	{"fullscreen",				SDT_BOOL,			(void*)false, &_fullscreen,					NULL},
	{"videodriver",				SDT_STRINGBUF | (lengthof(_ini_videodriver)<<16) | SDT_NOSAVE,NULL,			_ini_videodriver,				NULL},
	{"musicdriver",				SDT_STRINGBUF | (lengthof(_ini_musicdriver)<<16) | SDT_NOSAVE,NULL,			_ini_musicdriver,				NULL},
	{"sounddriver",				SDT_STRINGBUF | (lengthof(_ini_sounddriver)<<16) | SDT_NOSAVE,NULL,			_ini_sounddriver,				NULL},
	{"language",					SDT_STRINGBUF | lengthof(_dynlang.curr_file)<<16,							NULL,			_dynlang.curr_file,			NULL},
	{"resolution",				SDT_UINT16 | SDT_INTLIST | lengthof(_cur_resolution) << 16,		"640,480",_cur_resolution,				NULL},
	{"cache_sprites",			SDT_BOOL,			(void*)false, &_cache_sprites,			NULL},
	{"screenshot_format", SDT_STRINGBUF | (lengthof(_screenshot_format_name)<<16),			NULL,			_screenshot_format_name,NULL},
	{"savegame_format",		SDT_STRINGBUF | (lengthof(_savegame_format)<<16),							NULL,			_savegame_format,				NULL},
	{"rightclick_emulate",SDT_BOOL,			(void*)false, &_rightclick_emulate, NULL},
	{NULL,								0,						NULL,					NULL,									NULL}
};

#ifdef ENABLE_NETWORK
static const SettingDesc network_settings[] = {
	{"sync_freq",				SDT_UINT16 | SDT_NOSAVE,	(void*)100,			&_network_sync_freq,		NULL},
	{"frame_freq",			SDT_UINT8 | SDT_NOSAVE,	(void*)0,			&_network_frame_freq,		NULL},
	{"server_bind_ip",	SDT_STRINGBUF | (lengthof(_network_server_bind_ip_host) << 16),	"0.0.0.0",	&_network_server_bind_ip_host,	NULL},
	{"server_port",			SDT_UINT,	(void*)NETWORK_DEFAULT_PORT,	&_network_server_port,	NULL},
	{"server_advertise",SDT_BOOL, (void*)false, &_network_advertise, NULL},
	{"lan_internet",		SDT_UINT8, (void*)0, &_network_lan_internet, NULL},
	{"player_name",			SDT_STRINGBUF | (lengthof(_network_player_name) << 16), NULL, &_network_player_name, NULL},
	{"server_password",	SDT_STRINGBUF | (lengthof(_network_server_password) << 16), NULL, &_network_server_password, NULL},
	{"rcon_password",		SDT_STRINGBUF | (lengthof(_network_rcon_password) << 16), NULL, &_network_rcon_password, NULL},
	{"server_name",			SDT_STRINGBUF | (lengthof(_network_server_name) << 16), NULL, &_network_server_name, NULL},
	{"connect_to_ip",		SDT_STRINGBUF | (lengthof(_network_default_ip) << 16), NULL, &_network_default_ip, NULL},
	{"network_id",			SDT_STRINGBUF | (lengthof(_network_unique_id) << 16), NULL, &_network_unique_id, NULL},
	{"autoclean_companies", SDT_BOOL, (void*)false, &_network_autoclean_companies, NULL},
	{"autoclean_unprotected", SDT_UINT8, (void*)12, &_network_autoclean_unprotected, NULL},
	{"autoclean_protected", SDT_UINT8, (void*)36, &_network_autoclean_protected, NULL},
	{"restart_game_date", SDT_UINT16, (void*)0, &_network_restart_game_date, NULL},
	{NULL,							0,											NULL,					NULL,										NULL}
};
#endif /* ENABLE_NETWORK */

static const SettingDesc debug_settings[] = {
	{"savedump_path",		SDT_STRINGBUF | (lengthof(_savedump_path)<<16) | SDT_NOSAVE, NULL, _savedump_path, NULL},
	{"savedump_first",	SDT_UINT | SDT_NOSAVE,	0,				&_savedump_first, NULL},
	{"savedump_freq",		SDT_UINT | SDT_NOSAVE,	(void*)1, &_savedump_freq,	NULL},
	{"savedump_last",		SDT_UINT | SDT_NOSAVE,	0,				&_savedump_last,	NULL},
	{NULL,							0,											NULL,			NULL,							NULL}
};


static const SettingDesc gameopt_settings[] = {
	{"diff_level",	SDT_UINT8,									(void*)9,		&_new_opt.diff_level, NULL},
	{"diff_custom", SDT_INTLIST | SDT_UINT32 | (sizeof(GameDifficulty)/4) << 16, NULL, &_new_opt.diff, NULL},
	{"currency",		SDT_UINT8 | SDT_ONEOFMANY,	(void*)0,	&_new_opt.currency,		"GBP|USD|EUR|YEN|ATS|BEF|CHF|CZK|DEM|DKK|ESP|FIM|FRF|GRD|HUF|ISK|ITL|NLG|NOK|PLN|ROL|RUR|SEK|custom" },
	{"distances",		SDT_UINT8 | SDT_ONEOFMANY,	(void*)1,		&_new_opt.kilometers, "imperial|metric" },
	{"town_names",	SDT_UINT8 | SDT_ONEOFMANY,	(void*)0,		&_new_opt.town_name,	"english|french|german|american|latin|silly|swedish|dutch|finnish|polish|slovakish|norwegian|hungarian|austrian|romanian|czech|swiss" },
	{"landscape",		SDT_UINT8 | SDT_ONEOFMANY,	(void*)0,		&_new_opt.landscape,	"normal|hilly|desert|candy" },
	{"autosave",		SDT_UINT8 | SDT_ONEOFMANY,	(void*)1,		&_new_opt.autosave,		"off|monthly|quarterly|half year|yearly" },
	{"road_side",		SDT_UINT8 | SDT_ONEOFMANY,	(void*)1,		&_new_opt.road_side,	"left|right" },
	{NULL,					0,													NULL,				NULL,																			NULL}
};

// The player-based settings (are not send over the network)
// Not everything can just be added to this list. For example, service_interval
//  can not be done, because every client assigns the service_interval value to the
//  v->service_interval, meaning that every client assigns his value to the interval.
//  If the setting was player-based, that would mean that vehicles could deside on
//  different moments that they are heading back to a service depot, causing desyncs
//  on a massive scale.
// Short, you can only add settings that does stuff for the screen, GUI, that kind
//  of stuff.
static const SettingDesc patch_player_settings[] = {
	{"vehicle_speed",				SDT_BOOL,		(void*)true,	&_patches.vehicle_speed,				NULL},

	{"lost_train_days",			SDT_UINT16, (void*)180,		&_patches.lost_train_days,			NULL},
	{"train_income_warn",		SDT_BOOL,		(void*)true,	&_patches.train_income_warn,		NULL},
	{"order_review_system", SDT_UINT8,	(void*)2,			&_patches.order_review_system,	NULL},

	{"status_long_date",		SDT_BOOL,		(void*)true,	&_patches.status_long_date,			NULL},
	{"show_finances",				SDT_BOOL,		(void*)true,	&_patches.show_finances,				NULL},
	{"autoscroll",					SDT_BOOL,		(void*)false,	&_patches.autoscroll,						NULL},
	{"errmsg_duration",			SDT_UINT8,	(void*)5,			&_patches.errmsg_duration,			NULL},
	{"toolbar_pos",					SDT_UINT8,	(void*)0,			&_patches.toolbar_pos,					NULL},
	{"keep_all_autosave",		SDT_BOOL,		(void*)false, &_patches.keep_all_autosave,		NULL},
	{"autosave_on_exit",		SDT_BOOL,		(void*)false, &_patches.autosave_on_exit,			NULL},

	{"bridge_pillars",			SDT_BOOL,		(void*)true,	&_patches.bridge_pillars,				NULL},
	{"invisible_trees",			SDT_BOOL,		(void*)false, &_patches.invisible_trees,			NULL},
	{"drag_signals_density",SDT_UINT8,	(void*)4,			&_patches.drag_signals_density, NULL},

	{"window_snap_radius",  SDT_UINT8,  (void*)10,    &_patches.window_snap_radius,   NULL},

	{"autorenew",						SDT_BOOL,		(void*)false,	&_patches.autorenew,						NULL},
	{"autorenew_months",		SDT_INT16,	(void*)-6,		&_patches.autorenew_months,			NULL},
	{"autorenew_money",			SDT_INT32,	(void*)100000,&_patches.autorenew_money,			NULL},

	{"population_in_label",	SDT_BOOL,		(void*)true,	&_patches.population_in_label,	NULL},

	{NULL,									0,					NULL,					NULL,																						NULL}
};

// Non-static, needed in network_server.c
const SettingDesc patch_settings[] = {
	{"build_on_slopes",			SDT_BOOL,		(void*)true,	&_patches.build_on_slopes,			NULL},
	{"mammoth_trains",			SDT_BOOL,		(void*)true,	&_patches.mammoth_trains,				NULL},
	{"join_stations",				SDT_BOOL,		(void*)true,	&_patches.join_stations,				NULL},
	{"station_spread",			SDT_UINT8,	(void*)12,		&_patches.station_spread,				NULL},
	{"full_load_any",				SDT_BOOL,		(void*)true,	&_patches.full_load_any,				NULL},
	{"modified_catchment", 	SDT_BOOL,		(void*)true,	&_patches.modified_catchment,		NULL},


	{"inflation",						SDT_BOOL,		(void*)true,	&_patches.inflation,						NULL},
	{"selectgoods",					SDT_BOOL,		(void*)true,	&_patches.selectgoods,					NULL},
	{"longbridges",					SDT_BOOL,		(void*)true, &_patches.longbridges,					NULL},
	{"gotodepot",						SDT_BOOL,		(void*)true,	&_patches.gotodepot,						NULL},

	{"build_rawmaterial_ind",	SDT_BOOL, (void*)false, &_patches.build_rawmaterial_ind,NULL},
	{"multiple_industry_per_town",SDT_BOOL, (void*)false, &_patches.multiple_industry_per_town, NULL},
	{"same_industry_close",	SDT_BOOL,		(void*)false, &_patches.same_industry_close,	NULL},

	{"signal_side",					SDT_BOOL,		(void*)true,	&_patches.signal_side,					NULL},

	{"new_nonstop",					SDT_BOOL,		(void*)false,	&_patches.new_nonstop,					NULL},
	{"roadveh_queue",				SDT_BOOL,		(void*)true,	&_patches.roadveh_queue,				NULL},

	{"snow_line_height",		SDT_UINT8,	(void*)7,			&_patches.snow_line_height,			NULL},

	{"bribe",								SDT_BOOL,		(void*)true,	&_patches.bribe,								NULL},
	{"new_depot_finding",		SDT_BOOL,		(void*)false,	&_patches.new_depot_finding,		NULL},

	{"nonuniform_stations",	SDT_BOOL,		(void*)true,	&_patches.nonuniform_stations,	NULL},
	{"always_small_airport",SDT_BOOL,		(void*)false,	&_patches.always_small_airport,	NULL},
	{"realistic_acceleration",SDT_BOOL, (void*)false,	&_patches.realistic_acceleration,	NULL},
	{"forbid_90_deg",				SDT_BOOL, 	(void*)false, &_patches.forbid_90_deg,					NULL},
	{"improved_load",				SDT_BOOL,		(void*)false,	&_patches.improved_load,				NULL},

	{"max_trains",					SDT_UINT16,	(void*)500,		&_patches.max_trains,						NULL},
	{"max_roadveh",					SDT_UINT16,	(void*)500,		&_patches.max_roadveh,					NULL},
	{"max_aircraft",				SDT_UINT16,	(void*)200,		&_patches.max_aircraft,					NULL},
	{"max_ships",						SDT_UINT16,	(void*)300,		&_patches.max_ships,						NULL},

	{"servint_ispercent",		SDT_BOOL,		(void*)false,	&_patches.servint_ispercent,		NULL},
	{"servint_trains",			SDT_UINT16, (void*)150,		&_patches.servint_trains,				NULL},
	{"servint_roadveh",			SDT_UINT16, (void*)150,		&_patches.servint_roadveh,			NULL},
	{"servint_ships",				SDT_UINT16, (void*)360,		&_patches.servint_ships,				NULL},
	{"servint_aircraft",		SDT_UINT16, (void*)100,		&_patches.servint_aircraft,			NULL},
	{"no_servicing_if_no_breakdowns", SDT_BOOL, (void*)0, &_patches.no_servicing_if_no_breakdowns, NULL},

	{"new_pathfinding",			SDT_BOOL,		(void*)true,	&_patches.new_pathfinding,			NULL},
	{"pf_maxlength",				SDT_UINT16, (void*)512,		&_patches.pf_maxlength,					NULL},
	{"pf_maxdepth",					SDT_UINT8,	(void*)16,		&_patches.pf_maxdepth,					NULL},


	{"ai_disable_veh_train",SDT_BOOL,		(void*)false, &_patches.ai_disable_veh_train,	NULL},
	{"ai_disable_veh_roadveh",SDT_BOOL,	(void*)false, &_patches.ai_disable_veh_roadveh,	NULL},
	{"ai_disable_veh_aircraft",SDT_BOOL,(void*)false, &_patches.ai_disable_veh_aircraft,NULL},
	{"ai_disable_veh_ship",	SDT_BOOL,		(void*)false, &_patches.ai_disable_veh_ship,	NULL},
	{"starting_date",				SDT_UINT32, (void*)1950,	&_patches.starting_date,				NULL},
	{"ending_date",				  SDT_UINT32, (void*)2051,	&_patches.ending_date,				  NULL},

	{"colored_news_date",		SDT_UINT32, (void*)2000,	&_patches.colored_news_date,		NULL},

	{"extra_dynamite",			SDT_BOOL,		(void*)false, &_patches.extra_dynamite,				NULL},

	{"never_expire_vehicles",SDT_BOOL,	(void*)false, &_patches.never_expire_vehicles,NULL},
	{"extend_vehicle_life",	SDT_UINT8,	(void*)0,			&_patches.extend_vehicle_life,	NULL},

	{"auto_euro",						SDT_BOOL,		(void*)true,	&_patches.auto_euro,						NULL},

	{"serviceathelipad",		SDT_BOOL,		(void*)true,	&_patches.serviceathelipad,			NULL},
	{"smooth_economy",			SDT_BOOL,		(void*)true,	&_patches.smooth_economy,				NULL},
	{"allow_shares",				SDT_BOOL,		(void*)true,	&_patches.allow_shares,					NULL},
	{"dist_local_authority",SDT_UINT8,	(void*)20,		&_patches.dist_local_authority, NULL},

	{"wait_oneway_signal",	SDT_UINT8,	(void*)15,		&_patches.wait_oneway_signal,		NULL},
	{"wait_twoway_signal",	SDT_UINT8,	(void*)41,		&_patches.wait_twoway_signal,		NULL},

	{"ainew_active",				SDT_BOOL,		(void*)false, &_patches.ainew_active,					NULL},

	{"map_x", SDT_UINT32, (void*)8, &_patches.map_x, NULL},
	{"map_y", SDT_UINT32, (void*)8, &_patches.map_y, NULL},

	/* New Path Finding */
	{"new_pathfinding_all",	SDT_BOOL,		(void*)false, &_patches.new_pathfinding_all,	NULL},

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
	* penalty will further prevent this */
	{"npf_rail_firstred_penalty",		SDT_UINT32, (void*)(10 * NPF_TILE_LENGTH),	&_patches.npf_rail_firstred_penalty,		NULL},
	/* When a train plans a route over a station tile, this penalty is
	* applied. We want that trains plan a route around a typical, 4x5
	* station, which means two tiles to the right, and two tiles back to
	* the left around it, or 5 tiles of station through it. If we assign
	* a penalty of 1 tile for every station tile passed, the route will
	* be around it.
	*/
	{"npf_rail_station_penalty",		SDT_UINT32, (void*)(1 * NPF_TILE_LENGTH),		&_patches.npf_rail_station_penalty, 		NULL},
	{"npf_rail_slope_penalty",			SDT_UINT32, (void*)(1 * NPF_TILE_LENGTH),		&_patches.npf_rail_slope_penalty,				NULL},

	{NULL,									0,					NULL,					NULL,																						NULL}
};

static const SettingDesc currency_settings[] = {
	{"rate",			SDT_UINT16,										(void*)1,		&_currency_specs[23].rate,			NULL},
	{"separator", SDT_STRINGQUOT | (2) << 16,		".", 				&_currency_specs[23].separator,	NULL},
	{"to_euro",		SDT_UINT16,										(void*)0,		&_currency_specs[23].to_euro,		NULL},
	{"prefix",		SDT_STRINGQUOT | (16) << 16,	NULL,				&_currency_specs[23].prefix,		NULL},
	{"suffix",		SDT_STRINGQUOT | (16) << 16,	" credits",	&_currency_specs[23].suffix,		NULL},
	{NULL,				0,														NULL,				NULL,														NULL}
};

typedef void SettingDescProc(IniFile *ini, const SettingDesc *desc, const void *grpname);

static void HandleSettingDescs(IniFile *ini, SettingDescProc *proc)
{
	proc(ini, misc_settings,		"misc");
	proc(ini, win32_settings,		"win32");
#ifdef ENABLE_NETWORK
	proc(ini, network_settings, "network");
#endif /* ENABLE_NETWORK */
	proc(ini, music_settings,		"music");
	proc(ini, gameopt_settings, "gameopt");
	proc(ini, patch_settings,		"patches");
	proc(ini, patch_player_settings,		"patches");
	proc(ini, currency_settings,"currency");

	proc(ini, debug_settings,		"debug");
}

// loads all items from a *grpname section into the **list
static void LoadList(IniFile *ini, const char *grpname, char **list, int len)
{
	IniGroup *group = ini_getgroup(ini, grpname, -1);
	IniItem *item;
	int i;

	if (!group)
		return;
	item = group->item;
	for ( i=0; i != len; i++) {
		if (item == NULL) break;
		list[i] = strdup(item->value);
		item = item->next;
	}
}

static void SaveList(IniFile *ini, const char *grpname, char **list, int len)
{
	IniGroup *group = ini_getgroup(ini, grpname, -1);
	IniItem *item = NULL;
	int i;
	bool first = true;

	if (!group)
		return;
	for (i = 0; i != len; i++) {
		if (list[i] == NULL || list[i][0] == '\0') continue;

		if (first) { // add first item to the head of the group
			item = ini_item_alloc(group, list[i], strlen(list[i]));
			item->value = item->name;
			group->item = item;
			first = false;
		} else { // all other items are attached to the previous one
			item->next = ini_item_alloc(group, list[i], strlen(list[i]));
			item = item->next;
			item->value = item->name;
		}
	}
}

void LoadFromConfig(void)
{
	IniFile *ini = ini_load(_config_file);
	HandleSettingDescs(ini, load_setting_desc);
	LoadList(ini, "newgrf", _newgrf_files, lengthof(_newgrf_files));
	LoadList(ini, "servers", _network_host_list, lengthof(_network_host_list));
	LoadList(ini, "bans", _network_ban_list, lengthof(_network_ban_list));
	ini_free(ini);
}

void SaveToConfig(void)
{
	IniFile *ini = ini_load(_config_file);
	HandleSettingDescs(ini, save_setting_desc);
	SaveList(ini, "servers", _network_host_list, lengthof(_network_host_list));
	SaveList(ini, "bans", _network_ban_list, lengthof(_network_ban_list));
	ini_save(_config_file, ini);
	ini_free(ini);
}
