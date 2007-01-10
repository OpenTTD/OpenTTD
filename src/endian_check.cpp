/* $Id$ */

#include <stdio.h>
#include <string.h>

// This pretty simple file checks if the system is LITTLE_ENDIAN or BIG_ENDIAN
//  it does that by putting a 1 and a 0 in an array, and read it out as one
//  number. If it is 1, it is LITTLE_ENDIAN, if it is 256, it is BIG_ENDIAN
//
// After that it outputs the contents of an include files (endian.h)
//  that says or TTD_LITTLE_ENDIAN, or TTD_BIG_ENDIAN. Makefile takes
//  care of the real writing to the file.

int main (int argc, char *argv[]) {
	unsigned char EndianTest[2] = { 1, 0 };
	int force_BE = 0, force_LE = 0, force_PREPROCESSOR = 0;

	if (argc > 1 && strcmp(argv[1], "BE") == 0)
		force_BE = 1;
	if (argc > 1 && strcmp(argv[1], "LE") == 0)
		force_LE = 1;
	if (argc > 1 && strcmp(argv[1], "PREPROCESSOR") == 0)
		force_PREPROCESSOR = 1;

	printf("#ifndef ENDIAN_H\n#define ENDIAN_H\n");

	if (force_LE == 1) {
		printf("#define TTD_LITTLE_ENDIAN\n");
	} else {
		if (force_BE == 1) {
			printf("#define TTD_BIG_ENDIAN\n");
		} else {
			if (force_PREPROCESSOR == 1) {
				// adding support for universal binaries on OSX
				// Universal binaries supports both PPC and x86
				// If a compiler for OSX gets this setting, it will always pick the correct endian and no test is needed
				printf("#ifdef __BIG_ENDIAN__\n");
				printf("#define TTD_BIG_ENDIAN\n");
				printf("#else\n");
				printf("#define TTD_LITTLE_ENDIAN\n");
				printf("#endif\n");
			} else {
				if ( *(short *) EndianTest == 1 )
					printf("#define TTD_LITTLE_ENDIAN\n");
				else
					printf("#define TTD_BIG_ENDIAN\n");
			}
		}
	}
	printf("#endif\n");

	return 0;
}
