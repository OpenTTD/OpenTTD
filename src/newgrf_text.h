/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_text.h Header of Action 04 "universal holder" structure and functions */

#ifndef NEWGRF_TEXT_H
#define NEWGRF_TEXT_H

#include "newgrf_text_type.h"
#include "strings_type.h"
#include "table/control_codes.h"

StringID AddGRFString(uint32_t grfid, GRFStringID stringid, uint8_t langid, bool new_scheme, bool allow_newlines, std::string_view text_to_add, StringID def_string);
StringID GetGRFStringID(uint32_t grfid, GRFStringID stringid);
std::optional<std::string_view> GetGRFStringFromGRFText(const GRFTextList &text_list);
std::optional<std::string_view> GetGRFStringFromGRFText(const GRFTextWrapper &text);
std::string_view GetGRFStringPtr(StringIndexInTab stringid);
void CleanUpStrings();
void SetCurrentGrfLangID(uint8_t language_id);
std::string TranslateTTDPatchCodes(uint32_t grfid, uint8_t language_id, bool allow_newlines, std::string_view str, StringControlCode byte80 = SCC_NEWGRF_PRINT_WORD_STRING_ID);
void AddGRFTextToList(GRFTextList &list, uint8_t langid, uint32_t grfid, bool allow_newlines, std::string_view text_to_add);
void AddGRFTextToList(GRFTextWrapper &list, uint8_t langid, uint32_t grfid, bool allow_newlines, std::string_view text_to_add);
void AddGRFTextToList(GRFTextWrapper &list, std::string_view text_to_add);

bool CheckGrfLangID(uint8_t lang_id, uint8_t grf_version);

std::vector<StringParameter> GetGRFStringTextStackParameters(const struct GRFFile *grffile, StringID stringid, std::span<const int32_t> textstack);
std::string GetGRFStringWithTextStack(const struct GRFFile *grffile, GRFStringID grfstringid, std::span<const int32_t> textstack);

#endif /* NEWGRF_TEXT_H */
