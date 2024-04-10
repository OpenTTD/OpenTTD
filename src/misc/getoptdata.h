/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file getoptdata.h Library for parsing command-line options. */

#ifndef GETOPTDATA_H
#define GETOPTDATA_H

/** Flags of an option. */
enum OptionDataType : uint8_t {
	ODF_NO_VALUE,       ///< A plain option (no value attached to it).
	ODF_HAS_VALUE,      ///< An option with a value.
	ODF_OPTIONAL_VALUE, ///< An option with an optional value.
};

/** Data of an option. */
struct OptionData {
	OptionDataType type; ///< The type of option.
	char id; ///< Unique identification of this option data, often the same as #shortname.
	char shortname = '\0'; ///< Short option letter if available, else use \c '\0'.
	const char *longname = nullptr; ///< Long option name including '-'/'--' prefix, use \c nullptr if not available.
};

/** Data storage for parsing command line options. */
struct GetOptData {
	using OptionSpan = std::span<const OptionData>;
	char *opt;                 ///< Option value, if available (else \c nullptr).
	int numleft;               ///< Number of arguments left in #argv.
	char **argv;               ///< Remaining command line arguments.
	OptionSpan options; ///< Command line option descriptions.
	char *cont;                ///< Next call to #GetOpt should start here (in the middle of an argument).

	/**
	 * Constructor of the data store.
	 * @param argc Number of command line arguments, excluding the program name.
	 * @param argv Command line arguments, excluding the program name.
	 * @param options Command line option descriptions.
	 */
	GetOptData(int argc, char **argv, OptionSpan options) :
			opt(nullptr),
			numleft(argc),
			argv(argv),
			options(options),
			cont(nullptr)
	{
	}

	int GetOpt();
};

#endif /* GETOPTDATA_H */
