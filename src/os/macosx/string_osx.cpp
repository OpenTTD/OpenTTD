/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file string_osx.cpp Functions related to localized text support on OSX. */

#include "../../stdafx.h"
#include "string_osx.h"
#include "../../string_func.h"
#include "macos.h"

#include <CoreFoundation/CoreFoundation.h>


#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
static CFLocaleRef _osx_locale = NULL;

/** Store current language locale as a CoreFounation locale. */
void MacOSSetCurrentLocaleName(const char *iso_code)
{
	if (!MacOSVersionIsAtLeast(10, 5, 0)) return;

	if (_osx_locale != NULL) CFRelease(_osx_locale);

	CFStringRef iso = CFStringCreateWithCString(kCFAllocatorNull, iso_code, kCFStringEncodingUTF8);
	_osx_locale = CFLocaleCreate(kCFAllocatorDefault, iso);
	CFRelease(iso);
}

/**
 * Compares two strings using case insensitive natural sort.
 *
 * @param s1 First string to compare.
 * @param s2 Second string to compare.
 * @return 1 if s1 < s2, 2 if s1 == s2, 3 if s1 > s2, or 0 if not supported by the OS.
 */
int MacOSStringCompare(const char *s1, const char *s2)
{
	static bool supported = MacOSVersionIsAtLeast(10, 5, 0);
	if (!supported) return 0;

	CFStringCompareFlags flags = kCFCompareCaseInsensitive | kCFCompareNumerically | kCFCompareLocalized | kCFCompareWidthInsensitive | kCFCompareForcedOrdering;

	CFStringRef cf1 = CFStringCreateWithCString(kCFAllocatorDefault, s1, kCFStringEncodingUTF8);
	CFStringRef cf2 = CFStringCreateWithCString(kCFAllocatorDefault, s2, kCFStringEncodingUTF8);

	CFComparisonResult res = CFStringCompareWithOptionsAndLocale(cf1, cf2, CFRangeMake(0, CFStringGetLength(cf1)), flags, _osx_locale);

	CFRelease(cf1);
	CFRelease(cf2);

	return (int)res + 2;
}


/* virtual */ void OSXStringIterator::SetString(const char *s)
{
	const char *string_base = s;

	this->utf16_to_utf8.clear();
	this->str_info.clear();
	this->cur_pos = 0;

	/* CoreText operates on UTF-16, thus we have to convert the input string.
	 * To be able to return proper offsets, we have to create a mapping at the same time. */
	std::vector<UniChar> utf16_str;     ///< UTF-16 copy of the string.
	while (*s != '\0') {
		size_t idx = s - string_base;

		WChar c = Utf8Consume(&s);
		if (c < 0x10000) {
			utf16_str.push_back((UniChar)c);
		} else {
			/* Make a surrogate pair. */
			utf16_str.push_back((UniChar)(0xD800 + ((c - 0x10000) >> 10)));
			utf16_str.push_back((UniChar)(0xDC00 + ((c - 0x10000) & 0x3FF)));
			this->utf16_to_utf8.push_back(idx);
		}
		this->utf16_to_utf8.push_back(idx);
	}
	this->utf16_to_utf8.push_back(s - string_base);

	/* Query CoreText for word and cluster break information. */
	this->str_info.resize(utf16_to_utf8.size());

	if (utf16_str.size() > 0) {
		CFStringRef str = CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, &utf16_str[0], utf16_str.size(), kCFAllocatorNull);

		/* Get cluster breaks. */
		for (CFIndex i = 0; i < CFStringGetLength(str); ) {
			CFRange r = CFStringGetRangeOfComposedCharactersAtIndex(str, i);
			this->str_info[r.location].char_stop = true;

			i += r.length;
		}

		/* Get word breaks. */
		CFStringTokenizerRef tokenizer = CFStringTokenizerCreate(kCFAllocatorDefault, str, CFRangeMake(0, CFStringGetLength(str)), kCFStringTokenizerUnitWordBoundary, _osx_locale);

		CFStringTokenizerTokenType tokenType = kCFStringTokenizerTokenNone;
		while ((tokenType = CFStringTokenizerAdvanceToNextToken(tokenizer)) != kCFStringTokenizerTokenNone) {
			/* Skip tokens that are white-space or punctuation tokens. */
			if ((tokenType & kCFStringTokenizerTokenHasNonLettersMask) != kCFStringTokenizerTokenHasNonLettersMask) {
				CFRange r = CFStringTokenizerGetCurrentTokenRange(tokenizer);
				this->str_info[r.location].word_stop = true;
			}
		}

		CFRelease(tokenizer);
		CFRelease(str);
	}

	/* End-of-string is always a valid stopping point. */
	this->str_info.back().char_stop = true;
	this->str_info.back().word_stop = true;
}

/* virtual */ size_t OSXStringIterator::SetCurPosition(size_t pos)
{
	/* Convert incoming position to an UTF-16 string index. */
	size_t utf16_pos = 0;
	for (size_t i = 0; i < this->utf16_to_utf8.size(); i++) {
		if (this->utf16_to_utf8[i] == pos) {
			utf16_pos = i;
			break;
		}
	}

	/* Sanitize in case we get a position inside a grapheme cluster. */
	while (utf16_pos > 0 && !this->str_info[utf16_pos].char_stop) utf16_pos--;
	this->cur_pos = utf16_pos;

	return this->utf16_to_utf8[this->cur_pos];
}

/* virtual */ size_t OSXStringIterator::Next(IterType what)
{
	assert(this->cur_pos <= this->utf16_to_utf8.size());
	assert(what == StringIterator::ITER_CHARACTER || what == StringIterator::ITER_WORD);

	if (this->cur_pos == this->utf16_to_utf8.size()) return END;

	do {
		this->cur_pos++;
	} while (this->cur_pos < this->utf16_to_utf8.size() && (what  == ITER_WORD ? !this->str_info[this->cur_pos].word_stop : !this->str_info[this->cur_pos].char_stop));

	return this->cur_pos == this->utf16_to_utf8.size() ? END : this->utf16_to_utf8[this->cur_pos];
}

/* virtual */ size_t OSXStringIterator::Prev(IterType what)
{
	assert(this->cur_pos <= this->utf16_to_utf8.size());
	assert(what == StringIterator::ITER_CHARACTER || what == StringIterator::ITER_WORD);

	if (this->cur_pos == 0) return END;

	do {
		this->cur_pos--;
	} while (this->cur_pos > 0 && (what == ITER_WORD ? !this->str_info[this->cur_pos].word_stop : !this->str_info[this->cur_pos].char_stop));

	return this->utf16_to_utf8[this->cur_pos];
}

/* static */ StringIterator *OSXStringIterator::Create()
{
	if (!MacOSVersionIsAtLeast(10, 5, 0)) return NULL;

	return new OSXStringIterator();
}

#else
void MacOSSetCurrentLocaleName(const char *iso_code) {}

int MacOSStringCompare(const char *s1, const char *s2)
{
	return 0;
}

/* static */ StringIterator *OSXStringIterator::Create()
{
	return NULL;
}
#endif /* (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5) */
