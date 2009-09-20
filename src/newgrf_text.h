/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_text.h Header of Action 04 "universal holder" structure and functions */

#ifndef NEWGRF_TEXT_H
#define NEWGRF_TEXT_H

StringID AddGRFString(uint32 grfid, uint16 stringid, byte langid, bool new_scheme, const char *text_to_add, StringID def_string);
StringID GetGRFStringID(uint32 grfid, uint16 stringid);
const char *GetGRFStringPtr(uint16 stringid);
void CleanUpStrings();
void SetCurrentGrfLangID(byte language_id);
char *TranslateTTDPatchCodes(uint32 grfid, const char *str);

bool CheckGrfLangID(byte lang_id, byte grf_version);

void PrepareTextRefStackUsage(byte numEntries);
void StopTextRefStackUsage();
void SwitchToNormalRefStack();
void SwitchToErrorRefStack();
void RewindTextRefStack();
uint RemapNewGRFStringControlCode(uint scc, char **buff, const char **str, int64 *argv);

StringID TTDPStringIDToOTTDStringIDMapping(StringID string);

#endif /* NEWGRF_TEXT_H */
