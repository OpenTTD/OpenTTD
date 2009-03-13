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
	/**
	 * Windows language ID:
	 * Windows cannot and will not convert isocodes to something it can use to
	 * determine whether a font can be used for the language or not. As a result
	 * of that we need to pass the language id via strgen to OpenTTD to tell
	 * what language it is in "Windows". The ID is the 'locale identifier' on:
	 *   http://msdn.microsoft.com/en-us/library/ms776294.aspx
	 */
	uint16 winlangid;   // windows language id
	uint8 newgrflangid; // newgrf language id
	byte pad[3];        // pad header to be a multiple of 4
};

assert_compile(sizeof(LanguagePackHeader) % 4 == 0);

#endif /* STRGEN_H */
