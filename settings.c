#include "stdafx.h"
#include "ttd.h"
#include "sound.h"

enum SettingDescType {
	SDT_INTX, // must be 0
	SDT_ONEOFMANY,
	SDT_MANYOFMANY,
	SDT_BOOLX,
	SDT_STRING,
	SDT_STRINGBUF,
	SDT_INTLIST,

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

typedef struct IniFile IniFile;
typedef struct IniItem IniItem;
typedef struct IniGroup IniGroup;
typedef struct SettingDesc SettingDesc;
typedef struct MemoryPool MemoryPool;

static void pool_init(MemoryPool **pool);
static void *pool_alloc(MemoryPool **pool, uint size);
static void *pool_strdup(MemoryPool **pool, const char *mem, uint size);
static void pool_free(MemoryPool **pool);

struct MemoryPool {
	uint pos,size;
	MemoryPool *next;
	byte mem[1];
};

static MemoryPool *pool_new(uint minsize)
{
	MemoryPool *p;
	if (minsize < 4096 - 12) minsize = 4096 - 12;
	
	p = malloc(sizeof(MemoryPool) - 1 + minsize);
	p->pos = 0;
	p->size = minsize;
	p->next = NULL;
	return p;
}

static void pool_init(MemoryPool **pool)
{
	*pool = pool_new(0);
}

static void *pool_alloc(MemoryPool **pool, uint size)
{
	uint pos;
	MemoryPool *p = *pool;

	size = (size + 3) & ~3; // align everything to a 32 bit boundary

	// first check if there's memory in the next pool
	if (p->next && p->next->pos + size <= p->next->size) {
		p = p->next;
	// then check if there's not memory in the cur pool
	} else if (p->pos + size > p->size) {
		MemoryPool *n = pool_new(size);
		*pool = n;
		n->next = p;
		p = n;		
	}

	pos = p->pos;
	p->pos += size;
	return p->mem + pos;
}

static void *pool_strdup(MemoryPool **pool, const char *mem, uint size)
{
	byte *p = pool_alloc(pool, size + 1);
	p[size] = 0;
	memcpy(p, mem, size);
	return p;
}

static void pool_free(MemoryPool **pool)
{
	MemoryPool *p = *pool, *n;
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
};

struct IniFile {
	MemoryPool *pool;
	IniGroup *group, **last_group;
	char *comment; // last comment in file
};

// allocate an inifile object
static IniFile *ini_alloc()
{
	IniFile *ini;
	MemoryPool *pool;
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
		for(s=buffer; *s == ' ' || *s == '\t'; s++);

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

			// find start of parameter
			while (*t == '=' || *t == ' ' || *t == '\t') t++;
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

struct SettingDesc {
	const char *name;
	int flags;
	void *def;
	void *ptr;
	void *b;
	
};

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

static void *string_to_val(const SettingDesc *desc, const char *str)
{
	unsigned long val;
	char *end;

	switch(desc->flags & 0xF) {
	case SDT_INTX:
		val = strtol(str, &end, 0);
		if (*end != 0) ShowInfoF("ini: trailing characters at end of setting '%s'", desc->name);
		return (void*)val;
	case SDT_ONEOFMANY: {
		int r = lookup_oneofmany((char*)desc->b, str, -1);
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
	case SDT_INTLIST:
		return (void*)str;
	}

	return NULL;
}

static void load_setting_desc(IniFile *ini, const SettingDesc *desc, void *grpname, void *base)
{
	IniGroup *group_def = ini_getgroup(ini, grpname, -1), *group;
	IniItem *item;
	void *p;
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
		if ( (uint32)ptr < 0x10000)
			ptr = (byte*)base + (uint32)ptr;

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
			if (*(char**)ptr) free(*(char**)ptr);
			*(char**)ptr = strdup((char*)p);
			break;
		case SDT_STRINGBUF:
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

static void save_setting_desc(IniFile *ini, const SettingDesc *desc, void *grpname, void *base)
{
	IniGroup *group_def = NULL, *group;
	IniItem *item;
	void *p, *ptr;
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
		if ( (uint32)ptr < 0x10000)
			ptr = (byte*)base + (uint32)ptr;
		
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
				make_oneofmany(buf, (char*)desc->b, i);
				break;
			case SDT_MANYOFMANY:
				make_manyofmany(buf, (char*)desc->b, i);
				break;
			case SDT_BOOLX:
				strcpy(buf, i ? "true" : "false");
				break;
			default:
				NOT_REACHED();
			}
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
	{"playlist", SDT_UINT8, (void*)0, (void*)offsetof(MusicFileSettings, playlist) },
	{"music_vol", SDT_UINT8, (void*)128, (void*)offsetof(MusicFileSettings, music_vol) },
	{"effect_vol", SDT_UINT8, (void*)128, (void*)offsetof(MusicFileSettings, effect_vol) },
	{"custom_1", SDT_INTLIST | SDT_UINT8 | lengthof(msf.custom_1) << 16, NULL, (void*)offsetof(MusicFileSettings, custom_1) },
	{"custom_2", SDT_INTLIST | SDT_UINT8 | lengthof(msf.custom_2) << 16, NULL, (void*)offsetof(MusicFileSettings, custom_2) },
	{"playing", SDT_BOOL, (void*)true, (void*)offsetof(MusicFileSettings, btn_down) },
	{"shuffle", SDT_BOOL, (void*)false, (void*)offsetof(MusicFileSettings, shuffle) },
	{NULL}
};

static const SettingDesc win32_settings[] = {
	{"display_hz", SDT_UINT, (void*)0, &_display_hz},
	{"force_full_redraw", SDT_BOOL, (void*)false, &_force_full_redraw},
	{"fullscreen_bpp", SDT_UINT, (void*)8, &_fullscreen_bpp},
	{"double_size", SDT_BOOL, (void*)false, &_double_size},
	{NULL}
};

static const SettingDesc misc_settings[] = {
	{"display_opt", SDT_MANYOFMANY | SDT_UINT8, (void*)(DO_SHOW_TOWN_NAMES|DO_SHOW_STATION_NAMES|DO_SHOW_SIGNS|DO_FULL_ANIMATION|DO_FULL_DETAIL|DO_TRANS_BUILDINGS|DO_CHECKPOINTS), &_display_opt, "SHOW_TOWN_NAMES|SHOW_STATION_NAMES|SHOW_SIGNS|FULL_ANIMATION|TRANS_BUILDINGS|FULL_DETAIL|CHECKPOINTS"},
	{"news_display_opt", SDT_UINT16, (void*)-1, &_news_display_opt},
	{"fullscreen", SDT_BOOL, (void*)false, &_fullscreen},
	{"videodriver", SDT_STRINGBUF | (lengthof(_ini_videodriver)<<16) | SDT_NOSAVE, NULL, _ini_videodriver},
	{"musicdriver", SDT_STRINGBUF | (lengthof(_ini_musicdriver)<<16) | SDT_NOSAVE, NULL, _ini_musicdriver},
	{"sounddriver", SDT_STRINGBUF | (lengthof(_ini_sounddriver)<<16) | SDT_NOSAVE, NULL, _ini_sounddriver},
	{"language", SDT_STRINGBUF | lengthof(_dynlang.curr_file)<<16, NULL, _dynlang.curr_file },
	{"resolution", SDT_UINT16 | SDT_INTLIST | lengthof(_cur_resolution) << 16, "640,480", _cur_resolution},
	{"cache_sprites", SDT_BOOL, (void*)false, &_cache_sprites},
	{"screenshot_format", SDT_STRINGBUF | (lengthof(_screenshot_format_name)<<16), NULL, _screenshot_format_name},
	{"savegame_format", SDT_STRINGBUF | (lengthof(_savegame_format)<<16), NULL, _savegame_format},
	{"rightclick_emulate", SDT_BOOL, (void*)false, &_rightclick_emulate},
	{NULL}
};

static const SettingDesc network_settings[] = {
	{"port", SDT_UINT | SDT_NOSAVE, (void*)3979, &_network_port},
	{"sync_freq", SDT_UINT | SDT_NOSAVE, (void*)4, &_network_sync_freq},
	{"ahead_frames", SDT_UINT | SDT_NOSAVE, (void*)5, &_network_ahead_frames},
	{NULL}
};

static const SettingDesc debug_settings[] = {
	{"savedump_path", SDT_STRINGBUF | (lengthof(_savedump_path)<<16) | SDT_NOSAVE, NULL, _savedump_path},
	{"savedump_first", SDT_UINT | SDT_NOSAVE, 0, &_savedump_first},
	{"savedump_freq", SDT_UINT | SDT_NOSAVE, (void*)1, &_savedump_freq},
	{"savedump_last", SDT_UINT | SDT_NOSAVE, 0, &_savedump_last},
	{NULL}
};


static const SettingDesc gameopt_settings[] = {
	{"diff_level", SDT_UINT8, (void*)9, (void*)offsetof(GameOptions, diff_level) },
	{"diff_custom", SDT_INTLIST | SDT_UINT32 | (sizeof(GameDifficulty)/4) << 16, NULL, (void*)offsetof(GameOptions, diff) },
	{"currency", SDT_UINT8 | SDT_ONEOFMANY, (void*)0, (void*)offsetof(GameOptions, currency), "GBP|USD|FF|DM|YEN|PT|FT|ZL|ATS|BEF|DKK|FIM|GRD|CHF|NLG|ITL|SEK|RUR|CZK|ISK|NOK|EUR" },
	{"distances", SDT_UINT8 | SDT_ONEOFMANY, (void*)0, (void*)offsetof(GameOptions, kilometers), "imperial|metric" },
	{"town_names", SDT_UINT8 | SDT_ONEOFMANY, (void*)0, (void*)offsetof(GameOptions, town_name), "english|french|german|american|latin|silly|swedish|dutch|finnish|polish|slovakish|hungarian" },
	{"landscape", SDT_UINT8 | SDT_ONEOFMANY, (void*)0, (void*)offsetof(GameOptions, landscape), "normal|hilly|desert|candy" },
	{"autosave", SDT_UINT8 | SDT_ONEOFMANY, (void*)1, (void*)offsetof(GameOptions, autosave), "off|monthly|quarterly|half year|yearly" },
	{"road_side", SDT_UINT8 | SDT_ONEOFMANY, (void*)0, (void*)offsetof(GameOptions, road_side), "left|right" },

	{NULL}
};

static const SettingDesc patch_settings[] = {
	{"vehicle_speed", SDT_BOOL, (void*)true, (void*)offsetof(Patches, vehicle_speed) },
	{"build_on_slopes", SDT_BOOL, (void*)true, (void*)offsetof(Patches, build_on_slopes) },
	{"mammoth_trains", SDT_BOOL, (void*)true, (void*)offsetof(Patches, mammoth_trains) },
	{"join_stations", SDT_BOOL, (void*)true, (void*)offsetof(Patches, join_stations) },
	{"station_spread", SDT_UINT8, (void*)12, (void*)offsetof(Patches, station_spread) },
	{"full_load_any", SDT_BOOL, (void*)true, (void*)offsetof(Patches, full_load_any)},
	{"order_review_system", SDT_UINT8, (void*)2, (void*)offsetof(Patches, order_review_system)},

	{"inflation", SDT_BOOL, (void*)true, (void*)offsetof(Patches, inflation)},
	{"selectgoods", SDT_BOOL, (void*)true, (void*)offsetof(Patches, selectgoods)},
	{"longbridges", SDT_BOOL, (void*)false, (void*)offsetof(Patches, longbridges)},
	{"gotodepot", SDT_BOOL, (void*)true, (void*)offsetof(Patches, gotodepot)},

	{"build_rawmaterial_ind", SDT_BOOL, (void*)false, (void*)offsetof(Patches, build_rawmaterial_ind)},
	{"multiple_industry_per_town", SDT_BOOL, (void*)false, (void*)offsetof(Patches, multiple_industry_per_town)},
	{"same_industry_close", SDT_BOOL, (void*)false, (void*)offsetof(Patches, same_industry_close)},

	{"lost_train_days", SDT_UINT16, (void*)180, (void*)offsetof(Patches, lost_train_days)},
	{"train_income_warn", SDT_BOOL, (void*)true, (void*)offsetof(Patches, train_income_warn)},

	{"status_long_date", SDT_BOOL, (void*)true, (void*)offsetof(Patches, status_long_date)},
	{"signal_side", SDT_BOOL, (void*)true, (void*)offsetof(Patches, signal_side)},
	{"show_finances", SDT_BOOL, (void*)true, (void*)offsetof(Patches, show_finances)},

	{"new_nonstop", SDT_BOOL, (void*)false, (void*)offsetof(Patches, new_nonstop)},
	{"roadveh_queue", SDT_BOOL, (void*)false, (void*)offsetof(Patches, roadveh_queue)},

	{"autoscroll", SDT_BOOL, (void*)false, (void*)offsetof(Patches, autoscroll)},
	{"errmsg_duration", SDT_UINT8, (void*)5, (void*)offsetof(Patches, errmsg_duration)},
	{"snow_line_height", SDT_UINT8, (void*)7, (void*)offsetof(Patches, snow_line_height)},

	{"bribe", SDT_BOOL, (void*)false, (void*)offsetof(Patches, bribe)},
	{"new_depot_finding", SDT_BOOL, (void*)false, (void*)offsetof(Patches, new_depot_finding)},

	{"nonuniform_stations", SDT_BOOL, (void*)false, (void*)offsetof(Patches, nonuniform_stations)},
	{"always_small_airport", SDT_BOOL, (void*)false, (void*)offsetof(Patches, always_small_airport)},
	{"realistic_acceleration", SDT_BOOL, (void*)false, (void*)offsetof(Patches, realistic_acceleration)},

	{"max_trains", SDT_UINT8, (void*)80,(void*)offsetof(Patches, max_trains)},
	{"max_roadveh", SDT_UINT8, (void*)80,(void*)offsetof(Patches, max_roadveh)},
	{"max_aircraft", SDT_UINT8, (void*)40,(void*)offsetof(Patches, max_aircraft)},
	{"max_ships", SDT_UINT8, (void*)50,(void*)offsetof(Patches, max_ships)},

	{"servint_trains", SDT_UINT16, (void*)150,(void*)offsetof(Patches, servint_trains)},
	{"servint_roadveh", SDT_UINT16, (void*)150,(void*)offsetof(Patches, servint_roadveh)},
	{"servint_ships", SDT_UINT16, (void*)360,(void*)offsetof(Patches, servint_ships)},
	{"servint_aircraft", SDT_UINT16, (void*)100,(void*)offsetof(Patches, servint_aircraft)},

	{"autorenew", SDT_BOOL, (void*)false,(void*)offsetof(Patches, autorenew)},
	{"autorenew_months", SDT_INT16, (void*)-6, (void*)offsetof(Patches, autorenew_months)},
	{"autorenew_money", SDT_INT32, (void*)100000, (void*)offsetof(Patches, autorenew_money)},

	{"new_pathfinding",  SDT_BOOL, (void*)false, (void*)offsetof(Patches, new_pathfinding)},
	{"pf_maxlength", SDT_UINT16, (void*)512, (void*)offsetof(Patches, pf_maxlength)},
	{"pf_maxdepth", SDT_UINT8, (void*)16, (void*)offsetof(Patches, pf_maxdepth)},

	{"build_in_pause",  SDT_BOOL, (void*)false, (void*)offsetof(Patches, build_in_pause)},

	{"ai_disable_veh_train", SDT_BOOL, (void*)false, (void*)offsetof(Patches, ai_disable_veh_train)},
	{"ai_disable_veh_roadveh", SDT_BOOL, (void*)false, (void*)offsetof(Patches, ai_disable_veh_roadveh)},
	{"ai_disable_veh_aircraft", SDT_BOOL, (void*)false, (void*)offsetof(Patches, ai_disable_veh_aircraft)},
	{"ai_disable_veh_ship", SDT_BOOL, (void*)false, (void*)offsetof(Patches, ai_disable_veh_ship)},
	{"starting_date", SDT_UINT32, (void*)1950, (void*)offsetof(Patches, starting_date)},

	{"colored_news_date", SDT_UINT32, (void*)2000, (void*)offsetof(Patches, colored_news_date)},

	{"bridge_pillars",  SDT_BOOL, (void*)true, (void*)offsetof(Patches, bridge_pillars)},

	{"keep_all_autosave",  SDT_BOOL, (void*)false, (void*)offsetof(Patches, keep_all_autosave)},

	{"extra_dynamite",  SDT_BOOL, (void*)false, (void*)offsetof(Patches, extra_dynamite)},

	{"never_expire_vehicles",  SDT_BOOL, (void*)false, (void*)offsetof(Patches, never_expire_vehicles)},
	{"extend_vehicle_life", SDT_UINT8, (void*)0, (void*)offsetof(Patches, extend_vehicle_life)},

	{"auto_euro",  SDT_BOOL, (void*)true, (void*)offsetof(Patches, auto_euro)},

	{"serviceathelipad",  SDT_BOOL, (void*)true, (void*)offsetof(Patches, serviceathelipad)},
	{"smooth_economy", SDT_BOOL, (void*)false, (void*)offsetof(Patches, smooth_economy)},
	{"dist_local_authority", SDT_UINT8, (void*)20, (void*)offsetof(Patches, dist_local_authority)},

	{"wait_oneway_signal", SDT_UINT8, (void*)15, (void*)offsetof(Patches, wait_oneway_signal)},
	{"wait_twoway_signal", SDT_UINT8, (void*)41, (void*)offsetof(Patches, wait_twoway_signal)},

	{NULL}
};

typedef void SettingDescProc(IniFile *ini, const SettingDesc *desc, void *grpname, void *base);

static void HandleSettingDescs(IniFile *ini, SettingDescProc *proc)
{
	proc(ini, misc_settings, "misc", NULL);
	proc(ini, win32_settings, "win32", NULL);
	proc(ini, network_settings, "network", NULL);
	proc(ini, music_settings, "music", &msf);
	proc(ini, gameopt_settings, "gameopt", &_new_opt);
	proc(ini, patch_settings, "patches", &_patches);

	proc(ini, debug_settings, "debug", NULL);
}

void LoadGrfSettings(IniFile *ini)
{
	IniGroup *group = ini_getgroup(ini, "newgrf", -1);
	IniItem *item;
	int i;

	if (!group)
		return;

	for(i=0; i!=lengthof(_newgrf_files); i++) {
		char buf[3];
		sprintf(buf, "%d", i);
		item = ini_getitem(group, buf, false);
		if (!item)
			break;
		_newgrf_files[i] = strdup(item->value);
	}
}

void LoadFromConfig()
{
	IniFile *ini = ini_load(_config_file);
	HandleSettingDescs(ini, load_setting_desc);
	LoadGrfSettings(ini);
	ini_free(ini);
}

void SaveToConfig()
{
	IniFile *ini = ini_load(_config_file);
	HandleSettingDescs(ini, save_setting_desc);
	ini_save(_config_file, ini);
	ini_free(ini);
}
