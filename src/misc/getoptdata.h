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
enum OptionDataFlags {
	ODF_NO_VALUE,       ///< A plain option (no value attached to it).
	ODF_HAS_VALUE,      ///< An option with a value.
	ODF_OPTIONAL_VALUE, ///< An option with an optional value.
	ODF_END,            ///< Terminator (data is not parsed further).
};

/** Data of an option. */
struct OptionData {
	byte id;              ///< Unique identification of this option data, often the same as #shortname.
	char shortname;       ///< Short option letter if available, else use \c '\0'.
	uint16_t flags;         ///< Option data flags. @see OptionDataFlags
	const char *longname; ///< Long option name including '-'/'--' prefix, use \c nullptr if not available.
};

/** Data storage for parsing command line options. */
struct GetOptData {
	char *opt;                 ///< Option value, if available (else \c nullptr).
	int numleft;               ///< Number of arguments left in #argv.
	char **argv;               ///< Remaining command line arguments.
	const OptionData *options; ///< Command line option descriptions.
	char *cont;                ///< Next call to #GetOpt should start here (in the middle of an argument).

	/**
	 * Constructor of the data store.
	 * @param argc Number of command line arguments, excluding the program name.
	 * @param argv Command line arguments, excluding the program name.
	 * @param options Command line option descriptions.
	 */
	GetOptData(int argc, char **argv, const OptionData *options) :
			opt(nullptr),
			numleft(argc),
			argv(argv),
			options(options),
			cont(nullptr)
	{
	}

	int GetOpt();
};

/**
 * General macro for creating an option.
 * @param id        Identification of the option.
 * @param shortname Short option name. Use \c '\0' if not used.
 * @param longname  Long option name including leading '-' or '--'. Use \c nullptr if not used.
 * @param flags     Flags of the option.
 */
#define GETOPT_GENERAL(id, shortname, longname, flags) { id, shortname, flags, longname }

/**
 * Short option without value.
 * @param shortname Short option name. Use \c '\0' if not used.
 * @param longname  Long option name including leading '-' or '--'. Use \c nullptr if not used.
 */
#define GETOPT_NOVAL(shortname, longname) GETOPT_GENERAL(shortname, shortname, longname, ODF_NO_VALUE)

/**
 * Short option with value.
 * @param shortname Short option name. Use \c '\0' if not used.
 * @param longname  Long option name including leading '-' or '--'. Use \c nullptr if not used.
 */
#define GETOPT_VALUE(shortname, longname) GETOPT_GENERAL(shortname, shortname, longname, ODF_HAS_VALUE)

/**
 * Short option with optional value.
 * @param shortname Short option name. Use \c '\0' if not used.
 * @param longname  Long option name including leading '-' or '--'. Use \c nullptr if not used.
 * @note Options with optional values are hopelessly ambiguous, eg "-opt -value", avoid them.
 */
#define GETOPT_OPTVAL(shortname, longname) GETOPT_GENERAL(shortname, shortname, longname, ODF_OPTIONAL_VALUE)


/**
 * Short option without value.
 * @param shortname Short option name. Use \c '\0' if not used.
 */
#define GETOPT_SHORT_NOVAL(shortname) GETOPT_NOVAL(shortname, nullptr)

/**
 * Short option with value.
 * @param shortname Short option name. Use \c '\0' if not used.
 */
#define GETOPT_SHORT_VALUE(shortname) GETOPT_VALUE(shortname, nullptr)

/**
 * Short option with optional value.
 * @param shortname Short option name. Use \c '\0' if not used.
 * @note Options with optional values are hopelessly ambiguous, eg "-opt -value", avoid them.
 */
#define GETOPT_SHORT_OPTVAL(shortname) GETOPT_OPTVAL(shortname, nullptr)

/** Option terminator. */
#define GETOPT_END() { '\0', '\0', ODF_END, nullptr}


#endif /* GETOPTDATA_H */
