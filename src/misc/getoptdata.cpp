/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file getoptdata.cpp Library for parsing command line options. */

#include "../stdafx.h"
#include "getoptdata.h"

#include "../safeguards.h"

/**
 * Find the next option.
 * @return Function returns one
 * - An option letter if it found another option.
 * - -1 if option processing is finished. Inspect #argv and #numleft to find the command line arguments.
 * - -2 if an error was encountered.
 */
int GetOptData::GetOpt()
{
	const char *s = this->cont;
	if (s == nullptr) {
		if (this->arguments.empty()) return -1; // No arguments left -> finished.

		s = this->arguments[0];
		if (*s != '-') return -1; // No leading '-' -> not an option -> finished.

		this->arguments = this->arguments.subspan(1);

		/* Is it a long option? */
		for (auto &option : this->options) {
			if (option.longname != nullptr && !strcmp(option.longname, s)) { // Long options always use the entire argument.
				this->cont = nullptr;
				return this->GetOpt(option);
			}
		}

		s++; // Skip leading '-'.
	}

	/* Is it a short option? */
	for (auto &option : this->options) {
		if (option.shortname != '\0' && *s == option.shortname) {
			this->cont = (s[1] != '\0') ? s + 1 : nullptr;
			return this->GetOpt(option);
		}
	}

	return -2; // No other ways to interpret the text -> error.
}

int GetOptData::GetOpt(const OptionData &option)
{
	this->opt = nullptr;
	switch (option.type) {
		case ODF_NO_VALUE:
			return option.id;

		case ODF_HAS_VALUE:
		case ODF_OPTIONAL_VALUE:
			if (this->cont != nullptr) { // Remainder of the argument is the option value.
				this->opt = this->cont;
				this->cont = nullptr;
				return option.id;
			}
			/* No more arguments, either return an error or a value-less option. */
			if (this->arguments.empty()) return (option.type == ODF_HAS_VALUE) ? -2 : option.id;

			/* Next argument looks like another option, let's not return it as option value. */
			if (option.type == ODF_OPTIONAL_VALUE && this->arguments[0][0] == '-') return option.id;

			this->opt = this->arguments[0]; // Next argument is the option value.
			this->arguments = this->arguments.subspan(1);
			return option.id;

		default: NOT_REACHED();
	}
}

