/* $Id$ */
#ifndef NEWGRF_TEXT_H
#define NEWGRF_TEXT_H

/** @file newgrf_text.h
 * Header of Action 04 "universal holder" structure and functions
 */

StringID AddGRFString(uint32 grfid, uint16 stringid, byte langid, bool new_scheme, const char *text_to_add, StringID def_string);
StringID GetGRFStringID(uint32 grfid, uint16 stringid);
const char *GetGRFStringPtr(uint16 stringid);
void CleanUpStrings();
void SetCurrentGrfLangID(const char *iso_name);
char *TranslateTTDPatchCodes(uint32 grfid, const char *str);

bool CheckGrfLangID(byte lang_id, byte grf_version);

void PrepareTextRefStackUsage(byte numEntries);
void StopTextRefStackUsage();
void SwitchToNormalRefStack();
void SwitchToErrorRefStack();
void RewindTextRefStack();
uint RemapNewGRFStringControlCode(uint scc, char **buff, const char **str, int64 *argv);

#endif /* NEWGRF_TEXT_H */
