/* $Id$ */

#ifndef STRINGS_H
#define STRINGS_H

static inline char* InlineString(char* buf, uint16 string)
{
	*buf++ = '\x81';
	*buf++ = string & 0xFF;
	*buf++ = string >> 8;
	return buf;
}

char *GetString(char *buffr, uint16 string);
char *GetStringWithArgs(char *buffr, uint string, const int32 *argv);

extern char _userstring[128];

void InjectDParam(int amount);
int32 GetParamInt32(void);

#endif /* STRINGS_H */
