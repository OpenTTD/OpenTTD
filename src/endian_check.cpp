/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file endian_check.cpp
 * This pretty simple file checks if the system is LITTLE_ENDIAN or BIG_ENDIAN
 *  it does that by putting a 1 and a 0 in an array, and read it out as one
 *  number. If it is 1, it is LITTLE_ENDIAN, if it is 256, it is BIG_ENDIAN
 *
 * After that it outputs the contents of an include files (endian.h)
 *  that says or TTD_LITTLE_ENDIAN, or TTD_BIG_ENDIAN. Makefile takes
 *  care of the real writing to the file.
 */

#include <stdio.h>
#include <string.h>

/** Supported endian types */
enum Endian {
	ENDIAN_LITTLE, ///< little endian
	ENDIAN_BIG,    ///< big endian
};

/**
 * Shortcut to printf("#define TTD_ENDIAN TTD_*_ENDIAN")
 * @param endian endian type to define
 */
static inline void printf_endian(Endian endian)
{
	printf("#define TTD_ENDIAN %s\n", endian == ENDIAN_LITTLE ? "TTD_LITTLE_ENDIAN" : "TTD_BIG_ENDIAN");
}

/**
 * Main call of the endian_check program
 * @param argc argument count
 * @param argv arguments themselves
 * @return exit code
 */
int main (int argc, char *argv[])
{
	unsigned char endian_test[2] = { 1, 0 };
	int force_BE = 0, force_LE = 0, force_PREPROCESSOR = 0;

	if (argc > 1 && strcmp(argv[1], "BE") == 0) force_BE = 1;
	if (argc > 1 && strcmp(argv[1], "LE") == 0) force_LE = 1;
	if (argc > 1 && strcmp(argv[1], "PREPROCESSOR") == 0) force_PREPROCESSOR = 1;

	printf("#ifndef ENDIAN_H\n#define ENDIAN_H\n");

	if (force_LE == 1) {
		printf_endian(ENDIAN_LITTLE);
	} else if (force_BE == 1) {
		printf_endian(ENDIAN_BIG);
	} else if (force_PREPROCESSOR == 1) {
		/* Support for universal binaries on OSX
		 * Universal binaries supports both PPC and x86
		 * If a compiler for OSX gets this setting, it will always pick the correct endian and no test is needed
		 */
		printf("#ifdef __BIG_ENDIAN__\n");
		printf_endian(ENDIAN_BIG);
		printf("#else\n");
		printf_endian(ENDIAN_LITTLE);
		printf("#endif\n");
	} else if (*(short*)endian_test == 1 ) {
		printf_endian(ENDIAN_LITTLE);
	} else {
		printf_endian(ENDIAN_BIG);
	}
	printf("#endif\n");

	return 0;
}
