/* $Id$ */

/** @file landscape.h */

enum {
	SNOW_LINE_MONTHS = 12,
	SNOW_LINE_DAYS   = 32,
};

struct SnowLine {
	byte table[SNOW_LINE_MONTHS][SNOW_LINE_DAYS];
	byte highest_value;
};

bool IsSnowLineSet(void);
void SetSnowLine(byte table[SNOW_LINE_MONTHS][SNOW_LINE_DAYS]);
byte GetSnowLine(void);
byte HighestSnowLine(void);
void ClearSnowLine(void);
