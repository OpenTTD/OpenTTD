/* $Id$ */
#ifndef NEWGRF_TEXT_H
#define NEWGRF_TEXT_H

/** @file
 * Header of Action 04 "universal holder" structure and functions
 */

#define MAX_LANG 28

/**
 * Element of the linked list.
 * Each of those elements represent the string,
 * but according to a different lang.
 */
typedef struct GRFText {
	byte langid;
	char *text;
	struct GRFText *next;
} GRFText;


/**
 * Holder of the above structure.
 * Putting both grfid and stringid togueter allow us to avoid duplicates,
 * since it is NOT SUPPOSED to happen.
 */
typedef struct GRFTextEntry {
	uint32 grfid;
	uint16 stringid;
	GRFText *textholder;
} GRFTextEntry;


StringID AddGRFString(uint32 grfid, uint16 stringid, byte langid, const char *text_to_add);
StringID GetGRFStringID(uint32 grfid, uint16 stringid);
char *GetGRFString(char *buff, uint16 stringid);
void CleanUpStrings(void);
void SetCurrentGrfLangID(const char *iso_name);

#endif /* NEWGRF_TEXT_H */
