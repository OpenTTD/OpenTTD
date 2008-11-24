/* $Id$ */

/** @file strgen.h Language pack header for strgen (needs to match). */

#ifndef STRGEN_H
#define STRGEN_H

struct LanguagePackHeader {
	uint32 ident;       // 32-bits identifier
	uint32 version;     // 32-bits of auto generated version info which is basically a hash of strings.h
	char name[32];      // the international name of this language
	char own_name[32];  // the localized name of this language
	char isocode[16];   // the ISO code for the language (not country code)
	uint16 offsets[32]; // the offsets
	byte plural_form;   // plural form index
	byte text_dir;      // default direction of the text
	byte pad[2];        // pad header to be a multiple of 4
};

assert_compile(sizeof(LanguagePackHeader) % 4 == 0);

#endif /* STRGEN_H */
