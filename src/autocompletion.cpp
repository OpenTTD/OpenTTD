/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file autocompletion.cpp Generic auto-completion engine. */

#include "stdafx.h"

#include "autocompletion.h"

#include "console_internal.h"
#include "town.h"
#include "network/network_base.h"

#include "safeguards.h"

bool AutoCompletion::AutoComplete()
{
	// We are pressing TAB for the first time after reset.
	if (this->suggestions.empty()) {
		this->InitSuggestions(this->textbuf->buf);
		if (this->suggestions.empty()) {
			return false;
		}
		this->ApplySuggestion(prefix, suggestions[0]);
		return true;
	}

	// We are pressing TAB again on the same text.
	if (this->current_suggestion_index + 1 < this->suggestions.size()) {
		this->ApplySuggestion(prefix, this->suggestions[++this->current_suggestion_index]);
	} else {
		// We are out of options, restore original text.
		this->textbuf->Assign(initial_buf);
		this->Reset();
	}
	return true;
}

void AutoCompletion::Reset()
{
	this->prefix = "";
	this->query = "";
	this->initial_buf.clear();
	this->suggestions.clear();
	this->current_suggestion_index = 0;
}

void AutoCompletion::InitSuggestions(std::string_view text)
{
	this->initial_buf = text;
	size_t space_pos = this->initial_buf.find_last_of(' ');
	this->query = this->initial_buf;
	if (space_pos == std::string::npos) {
		this->prefix = "";
	} else {
		this->prefix = this->query.substr(0, space_pos + 1);
		this->query.remove_prefix(space_pos + 1);
	}

	this->suggestions = this->GetSuggestions(prefix, query);
	this->current_suggestion_index = 0;
}
