#define STRGEN

#include "../stdafx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#if !defined(WIN32) || defined(__CYGWIN__)
#include <unistd.h>
#endif

#ifdef __MORPHOS__
#ifdef stderr
#undef stderr
#endif
#define stderr stdout
#endif // __MORPHOS__

/* Compiles a list of strings into a compiled string list */

typedef void (*ParseCmdProc)(char *buf, int value);

typedef struct {
	uint32 ident;
	uint32 version;			// 32-bits of auto generated version info which is basically a hash of strings.h
	char name[32];			// the international name of this language
	char own_name[32];	// the localized name of this language
	char isocode[16];	// the ISO code for the language (not country code)
	uint16 offsets[32];	// the offsets
	byte plural_form;		// plural form index
	byte pad[3];				// pad header to be a multiple of 4
} LanguagePackHeader;

typedef struct CmdStruct {
	const char *cmd;
	ParseCmdProc proc;
	long value;
	int8 consumes;
	bool dont_count;
} CmdStruct;

static int _cur_line;
static int _errors, _warnings;

static char *_strname[65536];     // Name of the string, NULL if not exists
static char *_master[65536];      // English text
static char *_translated[65536];  // Translated text
static uint16 _hash_next[65536];			// next hash entry

#define HASH_SIZE 32767
static uint16 _hash_head[HASH_SIZE];

static byte _put_buf[4096];
static int _put_pos;
static int _next_string_id;

static uint32 _hash;
static char _lang_name[32], _lang_ownname[32], _lang_isocode[16];
static byte _lang_pluralform;

// for each plural value, this is the number of plural forms.
static const byte _plural_form_counts[] = { 2,1,2,3,3,3,3,3,4 };

static const char *_cur_ident;

static uint HashStr(const char *s)
{
	uint hash = 0;
	for(; *s; s++)
		hash = ((hash << 3) | (hash >> 29)) ^ *s;
	return hash % HASH_SIZE;
}

static void HashAdd(const char *s, int value)
{
	uint hash = HashStr(s);
	_hash_next[value] = _hash_head[hash];
	_hash_head[hash] = value + 1;
}

static int HashFind(const char *s)
{
	int idx = _hash_head[HashStr(s)];
	while (--idx >= 0) {
		if (!strcmp(_strname[idx], s)) return idx;
		idx = _hash_next[idx];
	}
	return -1;
}


static void CDECL Warning(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vsprintf(buf, s, va);
	va_end(va);
	fprintf(stderr, "Warning:(%d): %s\n", _cur_line, buf);
	_warnings++;
}


static void CDECL Error(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vsprintf(buf, s, va);
	va_end(va);
	fprintf(stderr, "Error:(%d): %s\n", _cur_line, buf);
	_errors++;
}


static void NORETURN CDECL Fatal(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vsprintf(buf, s, va);
	va_end(va);
	fprintf(stderr, "%d: FATAL: %s\n", _cur_line, buf);
	exit(1);
}


static void ttd_strlcpy(char *dst, const char *src, size_t len)
{
	assert(len > 0);
	while (--len && *src)
		*dst++=*src++;
	*dst = 0;
}


static void PutByte(byte c)
{
	if (_put_pos == lengthof(_put_buf))
		Fatal("Put buffer too small");
	_put_buf[_put_pos++] = c;
}


static void EmitSingleByte(char *buf, int value)
{
	if (*buf != '\0')
		Warning("Ignoring trailing letters in command");
	PutByte((byte)value);
}


static void EmitEscapedByte(char *buf, int value)
{
	if (*buf != '\0')
		Warning("Ignoring trailing letters in command");
	PutByte((byte)0x85);
	PutByte((byte)value);
}


static void EmitSetX(char *buf, int value)
{
	char *err;
	int x = strtol(buf, &err, 0);
	if (*err != 0)
		Fatal("SetX param invalid");
	PutByte(1);
	PutByte((byte)x);
}


static void EmitSetXY(char *buf, int value)
{
	char *err;
	int x,y;

	x = strtol(buf, &err, 0);
	if (*err != ' ') Fatal("SetXY param invalid");
	y = strtol(err+1, &err, 0);
	if (*err != 0) Fatal("SetXY param invalid");

	PutByte(0x1F);
	PutByte((byte)x);
	PutByte((byte)y);
}

// The plural specifier looks like
// {NUM} {PLURAL -1 passenger passengers} then it picks either passenger/passengers depending on the count in NUM

// This is encoded like
//  CommandByte <ARG#> <NUM> {Length of each string} {each string}

bool ParseRelNum(char **buf, int *value, bool *relative)
{
	char *s = *buf, *end;
	bool rel = false;
	while (*s == ' ' || *s == '\t') s++;
	if (*s == '+') { rel = true; s++; }
	*value = strtol(s, &end, 0);
	if (end == s) return false;
	*relative = rel | (*value < 0);
	*buf = end;
	return true;
}

// Parse out the next word, or NULL
char *ParseWord(char **buf)
{
	char *s = *buf, *r;
	while (*s == ' ' || *s == '\t') s++;
	if (*s == 0)
		return NULL;

	if (*s == '"') {
		r = ++s;
		// parse until next " or NUL
		for(;;) {
			if (*s == 0)
				break;
			if (*s == '"') {
				*s++ = 0;
				break;
			}
			s++;
		}
	} else {
		// proceed until whitespace or NUL
		r = s;
		for(;;) {
			if (*s == 0)
				break;
			if (*s == ' ' || *s == '\t') {
				*s++ = 0;
				break;
			}
			s++;
		}
	}
	*buf = s;
	return r;
}

// Forward declaration
static int TranslateArgumentIdx(int arg, bool relative);

static void EmitPlural(char *buf, int value)
{
	int v,i,j;
	bool relative;
	char *words[5];
	int nw = 0;

	// Parse out the number.
	if (!ParseRelNum(&buf, &v, &relative))
		Fatal("Plural param invalid");

	// Parse each string
	for(nw=0; nw<5; nw++) {
		words[nw] = ParseWord(&buf);
		if (!words[nw])
			break;
	}

	if (nw == 0)
		Fatal("No plural words");

	if (_plural_form_counts[_lang_pluralform] != nw)
		Fatal("%s: Invalid number of plural forms. Expecting %d, found %d.", _cur_ident,
			_plural_form_counts[_lang_pluralform], nw);

	PutByte(0x7D);
	PutByte(TranslateArgumentIdx(v, relative));
	PutByte(nw);
	for(i=0; i<nw; i++)
		PutByte(strlen(words[i]));
	for(i=0; i<nw; i++) {
		for(j=0; words[i][j]; j++)
			PutByte(words[i][j]);
	}
}



static const CmdStruct _cmd_structs[] = {
	// Update position
	{"SETX", EmitSetX, 1, 0},
	{"SETXY", EmitSetXY, 2, 0},

	// Font size
	{"TINYFONT", EmitSingleByte, 8, 0},
	{"BIGFONT", EmitSingleByte, 9, 0},

	// New line
	{"", EmitSingleByte, 10, 0, true},

	// Colors
	{"BLUE", EmitSingleByte,    15, 0},
	{"SILVER", EmitSingleByte,  16, 0},
	{"GOLD", EmitSingleByte,    17, 0},
	{"RED", EmitSingleByte,     18, 0},
	{"PURPLE", EmitSingleByte,  19, 0},
	{"LTBROWN", EmitSingleByte, 20, 0},
	{"ORANGE", EmitSingleByte,  21, 0},
	{"GREEN", EmitSingleByte,   22, 0},
	{"YELLOW", EmitSingleByte,  23, 0},
	{"DKGREEN", EmitSingleByte, 24, 0},
	{"CREAM", EmitSingleByte,   25, 0},
	{"BROWN", EmitSingleByte,   26, 0},
	{"WHITE", EmitSingleByte,   27, 0},
	{"LTBLUE", EmitSingleByte,  28, 0},
	{"GRAY", EmitSingleByte,    29, 0},
	{"DKBLUE", EmitSingleByte,  30, 0},
	{"BLACK", EmitSingleByte,   31, 0},

	// 0x7B=123 is the LAST special character we may use.

	// Numbers
	{"COMMA", EmitSingleByte, 0x7B, 1}, // Number with comma
	{"NUM", EmitSingleByte,  0x7E, 1}, // Signed number

	{"CURRENCY", EmitSingleByte, 0x7F, 1},

	// 0x85
	{"CURRCOMPACT", EmitEscapedByte, 0, 1},		// compact currency (32 bits)
	{"REV", EmitEscapedByte, 2, 0},						// openttd revision string
	{"SHORTCARGO", EmitEscapedByte, 3, 2},		// short cargo description, only ### tons, or ### litres
	{"CURRCOMPACT64", EmitEscapedByte, 4, 2},	// compact currency 64 bits

	{"COMPANY", EmitEscapedByte, 5, 1},				// company string. This is actually a {STRING1}
																						// The first string includes the second string.

	{"PLAYERNAME", EmitEscapedByte, 5, 1},		// playername string. This is actually a {STRING1}
																						// The first string includes the second string.

	{"VEHICLE", EmitEscapedByte, 5, 1},		// playername string. This is actually a {STRING1}
																						// The first string includes the second string.


	{"STRING1", EmitEscapedByte, 5, 1},				// included string that consumes ONE argument
	{"STRING2", EmitEscapedByte, 6, 2},				// included string that consumes TWO arguments
	{"STRING3", EmitEscapedByte, 7, 3},				// included string that consumes THREE arguments
	{"STRING4", EmitEscapedByte, 8, 4},				// included string that consumes FOUR arguments
	{"STRING5", EmitEscapedByte, 9, 5},				// included string that consumes FIVE arguments

	{"STATIONFEATURES", EmitEscapedByte, 10, 1},				// station features string, icons of the features
	{"INDUSTRY", EmitEscapedByte, 11, 1},			// industry, takes an industry #

	{"PLURAL", EmitPlural, 0, 0, true},							// plural specifier

	{"DATE_LONG", EmitSingleByte, 0x82, 1},
	{"DATE_SHORT", EmitSingleByte, 0x83, 1},

	{"VELOCITY", EmitSingleByte, 0x84, 1},

	{"SKIP", EmitSingleByte, 0x86, 1},
	{"VOLUME", EmitSingleByte, 0x87, 1},

	{"STRING", EmitSingleByte, 0x88, 1},

	{"CARGO", EmitSingleByte, 0x99, 2},
	{"STATION", EmitSingleByte, 0x9A, 1},
	{"TOWN", EmitSingleByte, 0x9B, 1},
	{"CURRENCY64", EmitSingleByte, 0x9C, 2},
	{"WAYPOINT", EmitSingleByte, 0x9D, 1}, // waypoint name
	{"DATE_TINY", EmitSingleByte, 0x9E, 1},
	// 0x9E=158 is the LAST special character we may use.

	{"UPARROW", EmitSingleByte, 0xA0, 0},
	{"POUNDSIGN", EmitSingleByte, 0xA3, 0},
	{"YENSIGN", EmitSingleByte, 0xA5, 0},
	{"COPYRIGHT", EmitSingleByte, 0xA9, 0},
	{"DOWNARROW", EmitSingleByte, 0xAA, 0},
	{"CHECKMARK", EmitSingleByte, 0xAC, 0},
	{"CROSS", EmitSingleByte, 0xAD, 0},
	{"RIGHTARROW", EmitSingleByte, 0xAF, 0},

	{"TRAIN", EmitSingleByte, 0xb4, 0},
	{"LORRY", EmitSingleByte, 0xb5, 0},
	{"BUS",   EmitSingleByte, 0xb6, 0},
	{"PLANE", EmitSingleByte, 0xb7, 0},
	{"SHIP",  EmitSingleByte, 0xb8, 0},

	{"SMALLUPARROW", EmitSingleByte, 0xBC, 0},
	{"SMALLDOWNARROW", EmitSingleByte, 0xBD, 0},
	{"THREE_FOURTH", EmitSingleByte, 0xBE, 0},
};


static const CmdStruct *FindCmd(const char *s, int len)
{
	int i;
	const CmdStruct *cs = _cmd_structs;
	for(i=0; i != lengthof(_cmd_structs); i++, cs++) {
		if (!strncmp(cs->cmd, s, len) && cs->cmd[len] == '\0')
			return cs;
	}
	return NULL;
}


// returns NULL on eof
// else returns command struct
static const CmdStruct *ParseCommandString(char **str, char *param, int *argno, bool show_err)
{
	char *s = *str, *start;
	const CmdStruct *cmd;
	int plen = 0;
	byte c;

	*argno = -1;

	// Scan to the next command, exit if there's no next command.
	for(; *s != '{'; s++) {
		if (*s == '\0')
			return NULL;
	}
	s++; // Skip past the {

	if (*s >= '0' && *s <= '9') {
		char *end;
		*argno = strtoul(s, &end, 0);
		if (*end != ':') {
				Fatal("missing arg #");
			}
		s = end + 1;
	}

	// parse command name
	start = s;
	for(;;) {
		c = *s++;
		if (c == '}' || c == ' ')
			break;
		if (c == '\0') {
			Error("Missing } from command '%s'", start);
			return NULL;
		}
	}

	cmd = FindCmd(start, s - start - 1);
	if (cmd == NULL) {
		Error("Undefined command '%.*s'", s - start - 1, start);
		return NULL;
	}

	if (c == ' ') {
		// copy params
		start = s;
		for(;;) {
			c = *s++;
			if (c == '}') break;
			if (c == '\0') {
				Error("Missing } from command '%s'", start);
				return NULL;
			}
			if ( s - start == 250)
				Fatal("param command too long");
			*param++ = c;
		}
	}
	*param = 0;

	*str = s;

	return cmd;
}


static void HandlePragma(char *str)
{
	if (!memcmp(str, "id ", 3)) {
		_next_string_id = strtoul(str + 3, NULL, 0);
	} else if (!memcmp(str, "name ", 5)) {
		ttd_strlcpy(_lang_name, str + 5, sizeof(_lang_name));
	} else if (!memcmp(str, "ownname ", 8)) {
		ttd_strlcpy(_lang_ownname, str + 8, sizeof(_lang_ownname));
	} else if (!memcmp(str, "isocode ", 8)) {
		ttd_strlcpy(_lang_isocode, str + 8, sizeof(_lang_isocode));
	} else if (!memcmp(str, "plural ", 7)) {
		_lang_pluralform = atoi(str + 7);
		if (_lang_pluralform >= lengthof(_plural_form_counts))
			Fatal("Invalid pluralform %d", _lang_pluralform);
	} else {
		Fatal("unknown pragma '%s'", str);
	}
}

typedef struct CmdPair {
	const CmdStruct *a;
	char *v;
} CmdPair;

typedef struct ParsedCommandStruct {
	int np;
	CmdPair pairs[32];
	const CmdStruct *cmd[32]; // ordered by param #
} ParsedCommandStruct;


static void ExtractCommandString(ParsedCommandStruct *p, char *s, bool warnings)
{
	const CmdStruct *ar;
	char param[100];
	int argno;
	int argidx = 0;

	memset(p, 0, sizeof(*p));

	for(;;) {
		// read until next command from a.
		ar = ParseCommandString(&s, param, &argno, warnings);
		if (ar == NULL)
			break;

		// Sanity checking
		if (argno != -1 && !ar->consumes) Fatal("Non consumer param can't have a paramindex");

		if (ar->consumes) {
			if (argno != -1)
				argidx = argno;
			if (argidx < 0 || argidx >= lengthof(p->cmd)) Fatal("invalid param idx %d", argidx);
			if (p->cmd[argidx] != NULL && p->cmd[argidx] != ar) Fatal("duplicate param idx %d", argidx);

			p->cmd[argidx++] = ar;
		} else if (!ar->dont_count) { // Ignore some of them
			if (p->np >= lengthof(p->pairs)) Fatal("too many commands in string, max %d", lengthof(p->pairs));
			p->pairs[p->np].a = ar;
			p->pairs[p->np].v = param[0]?strdup(param):"";
			p->np++;
		}
	}
}


static const CmdStruct *TranslateCmdForCompare(const CmdStruct *a)
{
	if (!a) return NULL;

	if (!strcmp(a->cmd, "STRING1") ||
			!strcmp(a->cmd, "STRING2") ||
			!strcmp(a->cmd, "STRING3") ||
			!strcmp(a->cmd, "STRING4") ||
			!strcmp(a->cmd, "STRING5"))
		return FindCmd("STRING", 6);

	if (!strcmp(a->cmd, "SKIP"))
		return NULL;

	return a;
}


static bool CheckCommandsMatch(char *a, char *b, const char *name)
{
	ParsedCommandStruct templ;
	ParsedCommandStruct lang;
	int i,j;
	bool result = true;

	ExtractCommandString(&templ, b, true);
	ExtractCommandString(&lang, a, true);

	// For each string in templ, see if we find it in lang
	if (templ.np != lang.np) {
		Warning("%s: template string and language string have a different # of commands", name);
		result = false;
	}

	for(i = 0; i < templ.np; i++) {
		// see if we find it in lang, and zero it out
		bool found = false;
		for(j = 0; j < lang.np; j++) {
			if (templ.pairs[i].a == lang.pairs[j].a &&
					!strcmp(templ.pairs[i].v, lang.pairs[j].v)) {
				// it was found in both. zero it out from lang so we don't find it again
				lang.pairs[j].a = NULL;
				found = true;
				break;
			}
		}

		if (!found) {
			Warning("%s: command '%s' exists in template file but not in language file", name, templ.pairs[i].a->cmd);
			result = false;
		}
	}

	// if we reach here, all non consumer commands match up.
	// Check if the non consumer commands match up also.
	for(i = 0; i < lengthof(templ.cmd); i++) {
		if (TranslateCmdForCompare(templ.cmd[i]) != TranslateCmdForCompare(lang.cmd[i])) {
			Warning("%s: Param idx #%d '%s' doesn't match with template command '%s'", name, i,
				!lang.cmd[i] ? "<empty>" : lang.cmd[i]->cmd,
				!templ.cmd[i] ? "<empty>" : templ.cmd[i]->cmd);
			result = false;
		}
	}

	return result;
}


static void HandleString(char *str, bool master)
{
	char *s,*t;
	int ent;

	if (*str == '#') {
		if (str[1] == '#' && str[2] != '#')
			HandlePragma(str + 2);
		return;
	}

	// Ignore comments & blank lines
	if (*str == ';' || *str == ' ' || *str == '\0')
		return;

	s = strchr(str, ':');
	if (s == NULL) {
		Error("Line has no ':' delimiter");
		return;
	}

	// Trim spaces.
	// After this str points to the command name, and s points to the command contents
	for(t = s; t > str && (t[-1]==' ' || t[-1]=='\t'); t--);
	*t = 0;
	s++;

	// Check if this string already exists..
	ent = HashFind(str);

	if (master) {
		if (ent != -1) {
			Error("String name '%s' is used multiple times", str);
			return;
		}

		ent = _next_string_id++;
		if (_strname[ent]) {
			Error("String ID 0x%X for '%s' already in use by '%s'", ent, str, _strname[ent]);
			return;
		}
		_strname[ent] = strdup(str);
		_master[ent] = strdup(s);

		// add to hash table
		HashAdd(str, ent);
	} else {
		if (ent == -1) {
			Warning("String name '%s' does not exist in master file", str);
			return;
		}

		if (_translated[ent]) {
			Error("String name '%s' is used multiple times", str);
			return;
		}

		if (s[0] == ':' && s[1] == '\0') {
			// Special syntax :: means we should just inherit the master string
			_translated[ent] = strdup(_master[ent]);
		} else {
			// check that the commands match
			if (!CheckCommandsMatch(s, _master[ent], str)) {
				return;
			}
			_translated[ent] = strdup(s);
		}
	}
}


static void rstrip(char *buf)
{
	int i = strlen(buf);
	while (i>0 && (buf[i-1]=='\r' || buf[i-1]=='\n' || buf[i-1] == ' ')) i--;
	buf[i] = 0;
}


static void ParseFile(const char *file, bool english)
{
	FILE *in;
	char buf[2048];

	in = fopen(file, "r");
	if (in == NULL) { Fatal("Cannot open file '%s'", file); }
	_cur_line = 1;
	while (fgets(buf, sizeof(buf),in) != NULL) {
		rstrip(buf);
		HandleString(buf, english);
		_cur_line++;
	}
	fclose(in);
}


static uint32 MyHashStr(uint32 hash, const char *s)
{
	for(; *s; s++) {
		hash = ((hash << 3) | (hash >> 29)) ^ *s;
		if (hash & 1) hash = (hash>>1) ^ 0xDEADBEEF; else hash >>= 1;
	}
	return hash;
}


// make a hash of the file to get a unique "version number"
static void MakeHashOfStrings()
{
	uint32 hash = 0;
	char *s;
	const CmdStruct *cs;
	char buf[256];
	int i;
	int argno;

	for(i = 0; i != 65536; i++) {
		if ((s=_strname[i]) != NULL) {
			hash ^= i * 0x717239;
			if (hash & 1) hash = (hash>>1) ^ 0xDEADBEEF; else hash >>= 1;
			hash = MyHashStr(hash, s + 1);

			s = _master[i];
			while ((cs = ParseCommandString(&s, buf, &argno, false)) != NULL) {
				hash ^= (cs - _cmd_structs) * 0x1234567;
				if (hash & 1) hash = (hash>>1) ^ 0xF00BAA4; else hash >>= 1;
			}
		}
	}
	_hash = hash;
}


static int CountInUse(int grp)
{
	int i;

	for(i = 0x800; --i >= 0;) {
		if (_strname[(grp<<11)+i] != NULL)
			break;
	}
	return i + 1;
}


static void WriteLength(FILE *f, uint length)
{
	if (length < 0xC0) {
		fputc(length, f);
	} else if (length < 0x4000) {
		fputc((length >> 8) | 0xC0, f);
		fputc(length & 0xFF, f);
	} else {
		Fatal("string too long");
	}
}


bool CompareFiles(const char *n1, const char *n2)
{
	FILE *f1, *f2;
	char b1[4096];
	char b2[4096];
	size_t l1, l2;

	f2 = fopen(n2, "rb");
	if (f2 == NULL) return false;

	f1 = fopen(n1, "rb");
	if (f1 == NULL) Fatal("can't open %s", n1);

	do {
		l1 = fread(b1, 1, sizeof(b1), f1);
		l2 = fread(b2, 1, sizeof(b2), f2);

		if (l1 != l2 || memcmp(b1, b2, l1)) {
			fclose(f2);
			fclose(f1);
			return false;
		}
	} while (l1);

	fclose(f2);
	fclose(f1);
	return true;
}


static void WriteStringsH(const char *filename)
{
	FILE *out;
	int i;
	int next = -1;
	int lastgrp;

	out = fopen("tmp.xxx", "w");
	if (out == NULL) { Fatal("can't open tmp.xxx"); }

	fprintf(out, "enum {");

	lastgrp = 0;

	for(i = 0; i != 65536; i++) {
		if (_strname[i]) {
			if (lastgrp != (i >> 11)) {
				lastgrp = (i >> 11);
				fprintf(out, "};\n\nenum {");
			}

			fprintf(out, next == i ? "%s,\n" : "\n%s = 0x%X,\n", _strname[i], i);
			next = i + 1;
		}
	}

	fprintf(out, "};\n");

	fprintf(out,
		"\nenum {\n"
		"\tLANGUAGE_PACK_IDENT = 0x474E414C, // Big Endian value for 'LANG' (LE is 0x 4C 41 4E 47)\n"
		"\tLANGUAGE_PACK_VERSION = 0x%X,\n"
		"};\n", (uint)_hash);


	fclose(out);

	if (CompareFiles("tmp.xxx", filename)) {
		// files are equal. tmp.xxx is not needed
		unlink("tmp.xxx");
	} else {
		// else rename tmp.xxx into filename
#if defined(WIN32)
		unlink(filename);
#endif
		if (rename("tmp.xxx", filename) == -1) Fatal("rename() failed");
	}
}

static ParsedCommandStruct _cur_pcs;
static int _cur_argidx;

static int TranslateArgumentIdx(int argidx, bool relative)
{
	int i, sum;

	if (relative)
		argidx += _cur_argidx;

	if (argidx < 0 || argidx >= lengthof(_cur_pcs.cmd))
		Fatal("invalid argidx %d", argidx);

	for(i = sum = 0; i < argidx; i++) {
		const CmdStruct *cs = _cur_pcs.cmd[i++];
		sum += cs ? cs->consumes : 1;
	}

	return sum;
}

static void PutArgidxCommand(void)
{
	PutByte(0x7C);
	PutByte(TranslateArgumentIdx(0, true));
}


static void WriteLangfile(const char *filename, int show_todo)
{
	FILE *f;
	int in_use[32];
	LanguagePackHeader hdr;
	int i,j;
	const CmdStruct *cs;
	char param[256];
	int argno;

	f = fopen(filename, "wb");
	if (f == NULL) Fatal("can't open %s", filename);

	memset(&hdr, 0, sizeof(hdr));
	for(i = 0; i != 32; i++) {
		int n = CountInUse(i);
		in_use[i] = n;
		hdr.offsets[i] = TO_LE16(n);
	}

	// see line 655: fprintf(..."\tLANGUAGE_PACK_IDENT = 0x474E414C,...)
	hdr.ident = TO_LE32(0x474E414C); // Big Endian value for 'LANG'
	hdr.version = TO_LE32(_hash);
	hdr.plural_form = _lang_pluralform;
	strcpy(hdr.name, _lang_name);
	strcpy(hdr.own_name, _lang_ownname);
	strcpy(hdr.isocode, _lang_isocode);

	fwrite(&hdr, sizeof(hdr), 1, f);

	for(i = 0; i != 32; i++) {
		for(j = 0; j != in_use[i]; j++) {
			int idx = (i<<11)+j;
			char *str;

			// For undefined strings, just set that it's an empty string
			if (_strname[idx] == NULL) {
				WriteLength(f, 0);
				continue;
			}

			_cur_ident = _strname[idx];

			// Produce a message if a string doesn't have a translation.
			if (show_todo && _translated[idx] == NULL) {
				if (show_todo == 2) {
					Warning("'%s' is untranslated", _strname[idx]);
				} else {
					const char *s = "<TODO> ";
					while(*s) PutByte(*s++);
				}
			}

			// Extract the strings and stuff from the english command string
			ExtractCommandString(&_cur_pcs, _master[idx], false);

			str = _translated[idx] ? _translated[idx] : _master[idx];
			_cur_argidx = 0;

			while (*str != '\0') {
				// Process characters as they are until we encounter a {
				if (*str != '{') {
					PutByte(*str++);
					continue;
				}
				cs = ParseCommandString(&str, param, &argno, false);
				if (cs == NULL) break;

				// For params that consume values, we need to handle the argindex properly
				if (cs->consumes) {
					// Check if we need to output a move-param command
					if (argno!=-1 && argno != _cur_argidx) {
						_cur_argidx = argno;
						PutArgidxCommand();
					}

					// Output the one from the master string... it's always accurate.
					cs = _cur_pcs.cmd[_cur_argidx++];
					if (!cs)
						Fatal("cs == NULL");
				}

				cs->proc(param, cs->value);
			}

			WriteLength(f, _put_pos);
			fwrite(_put_buf, 1, _put_pos, f);
			_put_pos = 0;
		}
	}

	fputc(0, f);

	fclose(f);
}


int CDECL main(int argc, char* argv[])
{
	char *r;
	char buf[256];
	int show_todo = 0;

	if (argc > 1 && (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version"))) {
		puts("$Revision$");
		return 0;
	}

	if (argc > 1 && !strcmp(argv[1], "-t")) {
		show_todo = 1;
		argc--, argv++;
	}

	if (argc > 1 && !strcmp(argv[1], "-w")) {
		show_todo = 2;
		argc--, argv++;
	}


	if (argc == 1) {
		// parse master file
		ParseFile("lang/english.txt", true);
		MakeHashOfStrings();
		if (_errors) return 1;

		// write english.lng and strings.h

		WriteLangfile("lang/english.lng", 0);
		WriteStringsH("table/strings.h");

	} else if (argc == 2) {
		ParseFile("lang/english.txt", true);
		MakeHashOfStrings();
		ParseFile(argv[1], false);

		if (_errors) return 1;

		strcpy(buf, argv[1]);
		r = strrchr(buf, '.');
		if (!r || strcmp(r, ".txt")) r = strchr(buf, 0);
		strcpy(r, ".lng");
		WriteLangfile(buf, show_todo);
	} else {
		fprintf(stderr, "invalid arguments\n");
	}

	return 0;
}

