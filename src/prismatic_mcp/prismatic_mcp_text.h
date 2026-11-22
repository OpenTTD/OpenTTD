/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file prismatic_mcp_text.h RESEARCH-ONLY: game text as plain text for MCP clients. */

#ifndef PRISMATIC_MCP_TEXT_H
#define PRISMATIC_MCP_TEXT_H

#include "../string_func.h"

namespace PrismaticMCP {

/**
 * A decoded game string as plain text: OpenTTD's formatting codes (colours, font sizes, the
 * private-use characters a news headline carries) are dropped, line breaks become spaces and runs
 * of spaces collapse. An MCP field is text a client prints as is; it must not carry the game's
 * own control codes, which a client renders as boxes.
 * @param decoded The string as GetDecodedString or GetString return it.
 * @return The same words, without control codes.
 */
inline std::string PlainText(std::string_view decoded)
{
	/* No AllowControlCode: StrMakeValid drops the encoded control codes instead of keeping them;
	 * ReplaceTabCrNlWithSpace keeps the words of two lines apart; no ReplaceWithQuestionMark, so
	 * nothing takes a dropped code's place. */
	std::string valid = StrMakeValid(decoded, StringValidationSetting::ReplaceTabCrNlWithSpace);
	std::string out;
	out.reserve(valid.size());
	for (char c : valid) {
		if (c == ' ' && (out.empty() || out.back() == ' ')) continue;
		out += c;
	}
	while (!out.empty() && out.back() == ' ') out.pop_back();
	return out;
}

} // namespace PrismaticMCP

#endif /* PRISMATIC_MCP_TEXT_H */
