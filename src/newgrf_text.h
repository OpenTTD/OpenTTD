/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_text.h Header of Action 04 "universal holder" structure and functions */

#ifndef NEWGRF_TEXT_H
#define NEWGRF_TEXT_H

#include "newgrf_text_type.h"
#include "strings_type.h"
#include "table/control_codes.h"

StringID AddGRFString(uint32_t grfid, uint16_t stringid, uint8_t langid, bool new_scheme, bool allow_newlines, std::string_view text_to_add, StringID def_string);
StringID GetGRFStringID(uint32_t grfid, StringID stringid);
const char *GetGRFStringFromGRFText(const GRFTextList &text_list);
const char *GetGRFStringFromGRFText(const GRFTextWrapper &text);
const char *GetGRFStringPtr(uint32_t stringid);
void CleanUpStrings();
void SetCurrentGrfLangID(uint8_t language_id);
std::string TranslateTTDPatchCodes(uint32_t grfid, uint8_t language_id, bool allow_newlines, std::string_view str, StringControlCode byte80 = SCC_NEWGRF_PRINT_WORD_STRING_ID);
void AddGRFTextToList(GRFTextList &list, uint8_t langid, uint32_t grfid, bool allow_newlines, std::string_view text_to_add);
void AddGRFTextToList(GRFTextWrapper &list, uint8_t langid, uint32_t grfid, bool allow_newlines, std::string_view text_to_add);
void AddGRFTextToList(GRFTextWrapper &list, std::string_view text_to_add);

bool CheckGrfLangID(uint8_t lang_id, uint8_t grf_version);

void StartTextRefStackUsage(const struct GRFFile *grffile, uint8_t numEntries, const uint32_t *values = nullptr);
void StopTextRefStackUsage();
bool UsingNewGRFTextStack();
struct TextRefStack *CreateTextRefStackBackup();
void RestoreTextRefStackBackup(struct TextRefStack *backup);

#endif /* NEWGRF_TEXT_H */
