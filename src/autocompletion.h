/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file autocompletion.h Generic auto-completion engine. */

#ifndef AUTOCOMPLETION_H
#define AUTOCOMPLETION_H

#include "textbuf_type.h"

class AutoCompletion {
protected:
	Textbuf *textbuf;

private:
	std::string              initial_buf; ///< Value of text buffer when we started current suggestion session.

	std::string_view         prefix;      ///< Prefix of the text before the last space.
	std::string_view         query;       ///< Last token of the text. This is used to based the suggestions on.

	std::vector<std::string> suggestions;
	size_t                   current_suggestion_index;

public:
	AutoCompletion(Textbuf *textbuf) : textbuf(textbuf)
	{
		this->Reset();
	}
	virtual ~AutoCompletion() = default;

	// Returns true the textbuf was updated.
	bool AutoComplete();
	void Reset();

private:
	void InitSuggestions(std::string_view text);

	virtual std::vector<std::string> GetSuggestions(std::string_view prefix, std::string_view query) = 0;
	virtual void ApplySuggestion(std::string_view prefix, std::string_view suggestion) = 0;
};

#endif /* AUTOCOMPLETION_H */
