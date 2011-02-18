/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file getoptdata.h Library for parsing command-line options. */

#ifndef GETOPTDATA_H
#define GETOPTDATA_H

/** Data storage for parsing command line options. */
struct GetOptData {
	char *opt;           ///< Option value, if available (else \c NULL).
	int numleft;         ///< Number of arguments left in #argv.
	char **argv;         ///< Remaining command line arguments.
	const char *options; ///< Command line option descriptions.
	char *cont;          ///< Next call to #MyGetOpt should start here (in the middle of an argument).

	/**
	 * Constructor of the data store.
	 * @param argc Number of command line arguments, excluding the program name.
	 * @param argv Command line arguments, excluding the program name.
	 * @param options Command line option descriptions.
	 */
	GetOptData(int argc, char **argv, const char *options) :
			opt(NULL),
			numleft(argc),
			argv(argv),
			options(options),
			cont(NULL)
	{
	}

	int GetOpt();
};

#endif /* GETOPTDATA_H */
